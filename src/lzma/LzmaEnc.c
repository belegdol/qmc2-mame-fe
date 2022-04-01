/* LzmaEnc.c -- LZMA Encoder
2016-05-16 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include <string.h>

/* #define SHOW_STAT */
/* #define SHOW_STAT2 */

#if defined(SHOW_STAT) || defined(SHOW_STAT2)
#include <stdio.h>
#endif

#include "LzmaEnc.h"

#include "LzFind.h"
#ifndef _7ZIP_ST
#include "LzFindMt.h"
#endif

#ifdef SHOW_STAT
static unsigned g_STAT_OFFSET = 0;
#endif

#define kMaxHistorySize ((UInt32_7z)3 << 29)
/* #define kMaxHistorySize ((UInt32_7z)7 << 29) */

#define kBlockSizeMax ((1 << LZMA_NUM_BLOCK_SIZE_BITS) - 1)

#define kBlockSize (9 << 10)
#define kUnpackBlockSize (1 << 18)
#define kMatchArraySize (1 << 21)
#define kMatchRecordMaxSize ((LZMA_MATCH_LEN_MAX * 2 + 3) * LZMA_MATCH_LEN_MAX)

#define kNumMaxDirectBits (31)

#define kNumTopBits 24
#define kTopValue ((UInt32_7z)1 << kNumTopBits)

#define kNumBitModelTotalBits 11
#define kBitModelTotal (1 << kNumBitModelTotalBits)
#define kNumMoveBits 5
#define kProbInitValue (kBitModelTotal >> 1)

#define kNumMoveReducingBits 4
#define kNumBitPriceShiftBits 4
#define kBitPrice (1 << kNumBitPriceShiftBits)

void LzmaEncProps_Init(CLzmaEncProps *p)
{
  p->level = 5;
  p->dictSize = p->mc = 0;
  p->reduceSize = (UInt64)(Int64)-1;
  p->lc = p->lp = p->pb = p->algo = p->fb = p->btMode = p->numHashBytes = p->numThreads = -1;
  p->writeEndMark = 0;
}

void LzmaEncProps_Normalize(CLzmaEncProps *p)
{
  int level = p->level;
  if (level < 0) level = 5;
  p->level = level;
  
  if (p->dictSize == 0) p->dictSize = (level <= 5 ? (1 << (level * 2 + 14)) : (level == 6 ? (1 << 25) : (1 << 26)));
  if (p->dictSize > p->reduceSize)
  {
    unsigned i;
    for (i = 11; i <= 30; i++)
    {
      if ((UInt32_7z)p->reduceSize <= ((UInt32_7z)2 << i)) { p->dictSize = ((UInt32_7z)2 << i); break; }
      if ((UInt32_7z)p->reduceSize <= ((UInt32_7z)3 << i)) { p->dictSize = ((UInt32_7z)3 << i); break; }
    }
  }

  if (p->lc < 0) p->lc = 3;
  if (p->lp < 0) p->lp = 0;
  if (p->pb < 0) p->pb = 2;

  if (p->algo < 0) p->algo = (level < 5 ? 0 : 1);
  if (p->fb < 0) p->fb = (level < 7 ? 32 : 64);
  if (p->btMode < 0) p->btMode = (p->algo == 0 ? 0 : 1);
  if (p->numHashBytes < 0) p->numHashBytes = 4;
  if (p->mc == 0) p->mc = (16 + (p->fb >> 1)) >> (p->btMode ? 0 : 1);
  
  if (p->numThreads < 0)
    p->numThreads =
      #ifndef _7ZIP_ST
      ((p->btMode && p->algo) ? 2 : 1);
      #else
      1;
      #endif
}

UInt32_7z LzmaEncProps_GetDictSize(const CLzmaEncProps *props2)
{
  CLzmaEncProps props = *props2;
  LzmaEncProps_Normalize(&props);
  return props.dictSize;
}

#if (_MSC_VER >= 1400)
/* BSR code is fast for some new CPUs */
/* #define LZMA_LOG_BSR */
#endif

#ifdef LZMA_LOG_BSR

#define kDicLogSizeMaxCompress 32

#define BSR2_RET(pos, res) { unsigned long zz; _BitScanReverse(&zz, (pos)); res = (zz + zz) + ((pos >> (zz - 1)) & 1); }

static UInt32_7z GetPosSlot1(UInt32_7z pos)
{
  UInt32_7z res;
  BSR2_RET(pos, res);
  return res;
}
#define GetPosSlot2(pos, res) { BSR2_RET(pos, res); }
#define GetPosSlot(pos, res) { if (pos < 2) res = pos; else BSR2_RET(pos, res); }

#else

#define kNumLogBits (9 + sizeof(size_t) / 2)
/* #define kNumLogBits (11 + sizeof(size_t) / 8 * 3) */

#define kDicLogSizeMaxCompress ((kNumLogBits - 1) * 2 + 7)

static void LzmaEnc_FastPosInit(Byte *g_FastPos)
{
  unsigned slot;
  g_FastPos[0] = 0;
  g_FastPos[1] = 1;
  g_FastPos += 2;
  
  for (slot = 2; slot < kNumLogBits * 2; slot++)
  {
    size_t k = ((size_t)1 << ((slot >> 1) - 1));
    size_t j;
    for (j = 0; j < k; j++)
      g_FastPos[j] = (Byte)slot;
    g_FastPos += k;
  }
}

/* we can use ((limit - pos) >> 31) only if (pos < ((UInt32_7z)1 << 31)) */
/*
#define BSR2_RET(pos, res) { UInt32_7z zz = 6 + ((kNumLogBits - 1) & \
  (0 - (((((UInt32_7z)1 << (kNumLogBits + 6)) - 1) - pos) >> 31))); \
  res = p->g_FastPos[pos >> zz] + (zz * 2); }
*/

/*
#define BSR2_RET(pos, res) { UInt32_7z zz = 6 + ((kNumLogBits - 1) & \
  (0 - (((((UInt32_7z)1 << (kNumLogBits)) - 1) - (pos >> 6)) >> 31))); \
  res = p->g_FastPos[pos >> zz] + (zz * 2); }
*/

#define BSR2_RET(pos, res) { UInt32_7z zz = (pos < (1 << (kNumLogBits + 6))) ? 6 : 6 + kNumLogBits - 1; \
  res = p->g_FastPos[pos >> zz] + (zz * 2); }

/*
#define BSR2_RET(pos, res) { res = (pos < (1 << (kNumLogBits + 6))) ? \
  p->g_FastPos[pos >> 6] + 12 : \
  p->g_FastPos[pos >> (6 + kNumLogBits - 1)] + (6 + (kNumLogBits - 1)) * 2; }
*/

#define GetPosSlot1(pos) p->g_FastPos[pos]
#define GetPosSlot2(pos, res) { BSR2_RET(pos, res); }
#define GetPosSlot(pos, res) { if (pos < kNumFullDistances) res = p->g_FastPos[pos]; else BSR2_RET(pos, res); }

#endif


#define LZMA_NUM_REPS 4

typedef unsigned CState;

typedef struct
{
  UInt32_7z price;

  CState state;
  int prev1IsChar;
  int prev2;

  UInt32_7z posPrev2;
  UInt32_7z backPrev2;

  UInt32_7z posPrev;
  UInt32_7z backPrev;
  UInt32_7z backs[LZMA_NUM_REPS];
} COptimal;

#define kNumOpts (1 << 12)

#define kNumLenToPosStates 4
#define kNumPosSlotBits 6
#define kDicLogSizeMin 0
#define kDicLogSizeMax 32
#define kDistTableSizeMax (kDicLogSizeMax * 2)


#define kNumAlignBits 4
#define kAlignTableSize (1 << kNumAlignBits)
#define kAlignMask (kAlignTableSize - 1)

#define kStartPosModelIndex 4
#define kEndPosModelIndex 14
#define kNumPosModels (kEndPosModelIndex - kStartPosModelIndex)

#define kNumFullDistances (1 << (kEndPosModelIndex >> 1))

#ifdef _LZMA_PROB32
#define CLzmaProb UInt32_7z
#else
#define CLzmaProb UInt16
#endif

#define LZMA_PB_MAX 4
#define LZMA_LC_MAX 8
#define LZMA_LP_MAX 4

#define LZMA_NUM_PB_STATES_MAX (1 << LZMA_PB_MAX)


#define kLenNumLowBits 3
#define kLenNumLowSymbols (1 << kLenNumLowBits)
#define kLenNumMidBits 3
#define kLenNumMidSymbols (1 << kLenNumMidBits)
#define kLenNumHighBits 8
#define kLenNumHighSymbols (1 << kLenNumHighBits)

#define kLenNumSymbolsTotal (kLenNumLowSymbols + kLenNumMidSymbols + kLenNumHighSymbols)

#define LZMA_MATCH_LEN_MIN 2
#define LZMA_MATCH_LEN_MAX (LZMA_MATCH_LEN_MIN + kLenNumSymbolsTotal - 1)

#define kNumStates 12


typedef struct
{
  CLzmaProb choice;
  CLzmaProb choice2;
  CLzmaProb low[LZMA_NUM_PB_STATES_MAX << kLenNumLowBits];
  CLzmaProb mid[LZMA_NUM_PB_STATES_MAX << kLenNumMidBits];
  CLzmaProb high[kLenNumHighSymbols];
} CLenEnc;


typedef struct
{
  CLenEnc p;
  UInt32_7z tableSize;
  UInt32_7z prices[LZMA_NUM_PB_STATES_MAX][kLenNumSymbolsTotal];
  UInt32_7z counters[LZMA_NUM_PB_STATES_MAX];
} CLenPriceEnc;


typedef struct
{
  UInt32_7z range;
  Byte cache;
  UInt64 low;
  UInt64 cacheSize;
  Byte *buf;
  Byte *bufLim;
  Byte *bufBase;
  ISeqOutStream *outStream;
  UInt64 processed;
  SRes res;
} CRangeEnc;


typedef struct
{
  CLzmaProb *litProbs;

  UInt32_7z state;
  UInt32_7z reps[LZMA_NUM_REPS];

  CLzmaProb isMatch[kNumStates][LZMA_NUM_PB_STATES_MAX];
  CLzmaProb isRep[kNumStates];
  CLzmaProb isRepG0[kNumStates];
  CLzmaProb isRepG1[kNumStates];
  CLzmaProb isRepG2[kNumStates];
  CLzmaProb isRep0Long[kNumStates][LZMA_NUM_PB_STATES_MAX];

  CLzmaProb posSlotEncoder[kNumLenToPosStates][1 << kNumPosSlotBits];
  CLzmaProb posEncoders[kNumFullDistances - kEndPosModelIndex];
  CLzmaProb posAlignEncoder[1 << kNumAlignBits];
  
  CLenPriceEnc lenEnc;
  CLenPriceEnc repLenEnc;
} CSaveState;


typedef struct
{
  void *matchFinderObj;
  IMatchFinder matchFinder;

  UInt32_7z optimumEndIndex;
  UInt32_7z optimumCurrentIndex;

  UInt32_7z longestMatchLength;
  UInt32_7z numPairs;
  UInt32_7z numAvail;

  UInt32_7z numFastBytes;
  UInt32_7z additionalOffset;
  UInt32_7z reps[LZMA_NUM_REPS];
  UInt32_7z state;

  unsigned lc, lp, pb;
  unsigned lpMask, pbMask;
  unsigned lclp;

  CLzmaProb *litProbs;

  Bool_7z fastMode;
  Bool_7z writeEndMark;
  Bool_7z finished;
  Bool_7z multiThread;
  Bool_7z needInit;

  UInt64 nowPos64;
  
  UInt32_7z matchPriceCount;
  UInt32_7z alignPriceCount;

  UInt32_7z distTableSize;

  UInt32_7z dictSize;
  SRes result;

  CRangeEnc rc;

  #ifndef _7ZIP_ST
  Bool_7z mtMode;
  CMatchFinderMt matchFinderMt;
  #endif

  CMatchFinder matchFinderBase;

  #ifndef _7ZIP_ST
  Byte pad[128];
  #endif
  
  COptimal opt[kNumOpts];
  
  #ifndef LZMA_LOG_BSR
  Byte g_FastPos[1 << kNumLogBits];
  #endif

  UInt32_7z ProbPrices[kBitModelTotal >> kNumMoveReducingBits];
  UInt32_7z matches[LZMA_MATCH_LEN_MAX * 2 + 2 + 1];

  UInt32_7z posSlotPrices[kNumLenToPosStates][kDistTableSizeMax];
  UInt32_7z distancesPrices[kNumLenToPosStates][kNumFullDistances];
  UInt32_7z alignPrices[kAlignTableSize];

  CLzmaProb isMatch[kNumStates][LZMA_NUM_PB_STATES_MAX];
  CLzmaProb isRep[kNumStates];
  CLzmaProb isRepG0[kNumStates];
  CLzmaProb isRepG1[kNumStates];
  CLzmaProb isRepG2[kNumStates];
  CLzmaProb isRep0Long[kNumStates][LZMA_NUM_PB_STATES_MAX];

  CLzmaProb posSlotEncoder[kNumLenToPosStates][1 << kNumPosSlotBits];
  CLzmaProb posEncoders[kNumFullDistances - kEndPosModelIndex];
  CLzmaProb posAlignEncoder[1 << kNumAlignBits];
  
  CLenPriceEnc lenEnc;
  CLenPriceEnc repLenEnc;

  CSaveState saveState;

  #ifndef _7ZIP_ST
  Byte pad2[128];
  #endif
} CLzmaEnc;


void LzmaEnc_SaveState(CLzmaEncHandle pp)
{
  CLzmaEnc *p = (CLzmaEnc *)pp;
  CSaveState *dest = &p->saveState;
  int i;
  dest->lenEnc = p->lenEnc;
  dest->repLenEnc = p->repLenEnc;
  dest->state = p->state;

  for (i = 0; i < kNumStates; i++)
  {
    memcpy(dest->isMatch[i], p->isMatch[i], sizeof(p->isMatch[i]));
    memcpy(dest->isRep0Long[i], p->isRep0Long[i], sizeof(p->isRep0Long[i]));
  }
  for (i = 0; i < kNumLenToPosStates; i++)
    memcpy(dest->posSlotEncoder[i], p->posSlotEncoder[i], sizeof(p->posSlotEncoder[i]));
  memcpy(dest->isRep, p->isRep, sizeof(p->isRep));
  memcpy(dest->isRepG0, p->isRepG0, sizeof(p->isRepG0));
  memcpy(dest->isRepG1, p->isRepG1, sizeof(p->isRepG1));
  memcpy(dest->isRepG2, p->isRepG2, sizeof(p->isRepG2));
  memcpy(dest->posEncoders, p->posEncoders, sizeof(p->posEncoders));
  memcpy(dest->posAlignEncoder, p->posAlignEncoder, sizeof(p->posAlignEncoder));
  memcpy(dest->reps, p->reps, sizeof(p->reps));
  memcpy(dest->litProbs, p->litProbs, ((UInt32_7z)0x300 << p->lclp) * sizeof(CLzmaProb));
}

void LzmaEnc_RestoreState(CLzmaEncHandle pp)
{
  CLzmaEnc *dest = (CLzmaEnc *)pp;
  const CSaveState *p = &dest->saveState;
  int i;
  dest->lenEnc = p->lenEnc;
  dest->repLenEnc = p->repLenEnc;
  dest->state = p->state;

  for (i = 0; i < kNumStates; i++)
  {
    memcpy(dest->isMatch[i], p->isMatch[i], sizeof(p->isMatch[i]));
    memcpy(dest->isRep0Long[i], p->isRep0Long[i], sizeof(p->isRep0Long[i]));
  }
  for (i = 0; i < kNumLenToPosStates; i++)
    memcpy(dest->posSlotEncoder[i], p->posSlotEncoder[i], sizeof(p->posSlotEncoder[i]));
  memcpy(dest->isRep, p->isRep, sizeof(p->isRep));
  memcpy(dest->isRepG0, p->isRepG0, sizeof(p->isRepG0));
  memcpy(dest->isRepG1, p->isRepG1, sizeof(p->isRepG1));
  memcpy(dest->isRepG2, p->isRepG2, sizeof(p->isRepG2));
  memcpy(dest->posEncoders, p->posEncoders, sizeof(p->posEncoders));
  memcpy(dest->posAlignEncoder, p->posAlignEncoder, sizeof(p->posAlignEncoder));
  memcpy(dest->reps, p->reps, sizeof(p->reps));
  memcpy(dest->litProbs, p->litProbs, ((UInt32_7z)0x300 << dest->lclp) * sizeof(CLzmaProb));
}

SRes LzmaEnc_SetProps(CLzmaEncHandle pp, const CLzmaEncProps *props2)
{
  CLzmaEnc *p = (CLzmaEnc *)pp;
  CLzmaEncProps props = *props2;
  LzmaEncProps_Normalize(&props);

  if (props.lc > LZMA_LC_MAX
      || props.lp > LZMA_LP_MAX
      || props.pb > LZMA_PB_MAX
      || props.dictSize > ((UInt64)1 << kDicLogSizeMaxCompress)
      || props.dictSize > kMaxHistorySize)
    return SZ_ERROR_PARAM;

  p->dictSize = props.dictSize;
  {
    unsigned fb = props.fb;
    if (fb < 5)
      fb = 5;
    if (fb > LZMA_MATCH_LEN_MAX)
      fb = LZMA_MATCH_LEN_MAX;
    p->numFastBytes = fb;
  }
  p->lc = props.lc;
  p->lp = props.lp;
  p->pb = props.pb;
  p->fastMode = (props.algo == 0);
  p->matchFinderBase.btMode = (Byte)(props.btMode ? 1 : 0);
  {
    UInt32_7z numHashBytes = 4;
    if (props.btMode)
    {
      if (props.numHashBytes < 2)
        numHashBytes = 2;
      else if (props.numHashBytes < 4)
        numHashBytes = props.numHashBytes;
    }
    p->matchFinderBase.numHashBytes = numHashBytes;
  }

  p->matchFinderBase.cutValue = props.mc;

  p->writeEndMark = props.writeEndMark;

  #ifndef _7ZIP_ST
  /*
  if (newMultiThread != _multiThread)
  {
    ReleaseMatchFinder();
    _multiThread = newMultiThread;
  }
  */
  p->multiThread = (props.numThreads > 1);
  #endif

  return SZ_OK;
}

static const int kLiteralNextStates[kNumStates] = {0, 0, 0, 0, 1, 2, 3, 4,  5,  6,   4, 5};
static const int kMatchNextStates[kNumStates]   = {7, 7, 7, 7, 7, 7, 7, 10, 10, 10, 10, 10};
static const int kRepNextStates[kNumStates]     = {8, 8, 8, 8, 8, 8, 8, 11, 11, 11, 11, 11};
static const int kShortRepNextStates[kNumStates]= {9, 9, 9, 9, 9, 9, 9, 11, 11, 11, 11, 11};

#define IsCharState(s) ((s) < 7)

#define GetLenToPosState(len) (((len) < kNumLenToPosStates + 1) ? (len) - 2 : kNumLenToPosStates - 1)

#define kInfinityPrice (1 << 30)

static void RangeEnc_Construct(CRangeEnc *p)
{
  p->outStream = NULL;
  p->bufBase = NULL;
}

#define RangeEnc_GetProcessed(p) ((p)->processed + ((p)->buf - (p)->bufBase) + (p)->cacheSize)

#define RC_BUF_SIZE (1 << 16)
static int RangeEnc_Alloc(CRangeEnc *p, ISzAlloc *alloc)
{
  if (!p->bufBase)
  {
    p->bufBase = (Byte *)alloc->Alloc(alloc, RC_BUF_SIZE);
    if (!p->bufBase)
      return 0;
    p->bufLim = p->bufBase + RC_BUF_SIZE;
  }
  return 1;
}

static void RangeEnc_Free(CRangeEnc *p, ISzAlloc *alloc)
{
  alloc->Free(alloc, p->bufBase);
  p->bufBase = 0;
}

static void RangeEnc_Init(CRangeEnc *p)
{
  /* Stream.Init(); */
  p->low = 0;
  p->range = 0xFFFFFFFF;
  p->cacheSize = 1;
  p->cache = 0;

  p->buf = p->bufBase;

  p->processed = 0;
  p->res = SZ_OK;
}

static void RangeEnc_FlushStream(CRangeEnc *p)
{
  size_t num;
  if (p->res != SZ_OK)
    return;
  num = p->buf - p->bufBase;
  if (num != p->outStream->Write(p->outStream, p->bufBase, num))
    p->res = SZ_ERROR_WRITE;
  p->processed += num;
  p->buf = p->bufBase;
}

static void MY_FAST_CALL RangeEnc_ShiftLow(CRangeEnc *p)
{
  if ((UInt32_7z)p->low < (UInt32_7z)0xFF000000 || (unsigned)(p->low >> 32) != 0)
  {
    Byte temp = p->cache;
    do
    {
      Byte *buf = p->buf;
      *buf++ = (Byte)(temp + (Byte)(p->low >> 32));
      p->buf = buf;
      if (buf == p->bufLim)
        RangeEnc_FlushStream(p);
      temp = 0xFF;
    }
    while (--p->cacheSize != 0);
    p->cache = (Byte)((UInt32_7z)p->low >> 24);
  }
  p->cacheSize++;
  p->low = (UInt32_7z)p->low << 8;
}

static void RangeEnc_FlushData(CRangeEnc *p)
{
  int i;
  for (i = 0; i < 5; i++)
    RangeEnc_ShiftLow(p);
}

static void RangeEnc_EncodeDirectBits(CRangeEnc *p, UInt32_7z value, unsigned numBits)
{
  do
  {
    p->range >>= 1;
    p->low += p->range & (0 - ((value >> --numBits) & 1));
    if (p->range < kTopValue)
    {
      p->range <<= 8;
      RangeEnc_ShiftLow(p);
    }
  }
  while (numBits != 0);
}

static void RangeEnc_EncodeBit(CRangeEnc *p, CLzmaProb *prob, UInt32_7z symbol)
{
  UInt32_7z ttt = *prob;
  UInt32_7z newBound = (p->range >> kNumBitModelTotalBits) * ttt;
  if (symbol == 0)
  {
    p->range = newBound;
    ttt += (kBitModelTotal - ttt) >> kNumMoveBits;
  }
  else
  {
    p->low += newBound;
    p->range -= newBound;
    ttt -= ttt >> kNumMoveBits;
  }
  *prob = (CLzmaProb)ttt;
  if (p->range < kTopValue)
  {
    p->range <<= 8;
    RangeEnc_ShiftLow(p);
  }
}

static void LitEnc_Encode(CRangeEnc *p, CLzmaProb *probs, UInt32_7z symbol)
{
  symbol |= 0x100;
  do
  {
    RangeEnc_EncodeBit(p, probs + (symbol >> 8), (symbol >> 7) & 1);
    symbol <<= 1;
  }
  while (symbol < 0x10000);
}

static void LitEnc_EncodeMatched(CRangeEnc *p, CLzmaProb *probs, UInt32_7z symbol, UInt32_7z matchByte)
{
  UInt32_7z offs = 0x100;
  symbol |= 0x100;
  do
  {
    matchByte <<= 1;
    RangeEnc_EncodeBit(p, probs + (offs + (matchByte & offs) + (symbol >> 8)), (symbol >> 7) & 1);
    symbol <<= 1;
    offs &= ~(matchByte ^ symbol);
  }
  while (symbol < 0x10000);
}

static void LzmaEnc_InitPriceTables(UInt32_7z *ProbPrices)
{
  UInt32_7z i;
  for (i = (1 << kNumMoveReducingBits) / 2; i < kBitModelTotal; i += (1 << kNumMoveReducingBits))
  {
    const int kCyclesBits = kNumBitPriceShiftBits;
    UInt32_7z w = i;
    UInt32_7z bitCount = 0;
    int j;
    for (j = 0; j < kCyclesBits; j++)
    {
      w = w * w;
      bitCount <<= 1;
      while (w >= ((UInt32_7z)1 << 16))
      {
        w >>= 1;
        bitCount++;
      }
    }
    ProbPrices[i >> kNumMoveReducingBits] = ((kNumBitModelTotalBits << kCyclesBits) - 15 - bitCount);
  }
}


#define GET_PRICE(prob, symbol) \
  p->ProbPrices[((prob) ^ (((-(int)(symbol))) & (kBitModelTotal - 1))) >> kNumMoveReducingBits];

#define GET_PRICEa(prob, symbol) \
  ProbPrices[((prob) ^ ((-((int)(symbol))) & (kBitModelTotal - 1))) >> kNumMoveReducingBits];

#define GET_PRICE_0(prob) p->ProbPrices[(prob) >> kNumMoveReducingBits]
#define GET_PRICE_1(prob) p->ProbPrices[((prob) ^ (kBitModelTotal - 1)) >> kNumMoveReducingBits]

#define GET_PRICE_0a(prob) ProbPrices[(prob) >> kNumMoveReducingBits]
#define GET_PRICE_1a(prob) ProbPrices[((prob) ^ (kBitModelTotal - 1)) >> kNumMoveReducingBits]

static UInt32_7z LitEnc_GetPrice(const CLzmaProb *probs, UInt32_7z symbol, const UInt32_7z *ProbPrices)
{
  UInt32_7z price = 0;
  symbol |= 0x100;
  do
  {
    price += GET_PRICEa(probs[symbol >> 8], (symbol >> 7) & 1);
    symbol <<= 1;
  }
  while (symbol < 0x10000);
  return price;
}

static UInt32_7z LitEnc_GetPriceMatched(const CLzmaProb *probs, UInt32_7z symbol, UInt32_7z matchByte, const UInt32_7z *ProbPrices)
{
  UInt32_7z price = 0;
  UInt32_7z offs = 0x100;
  symbol |= 0x100;
  do
  {
    matchByte <<= 1;
    price += GET_PRICEa(probs[offs + (matchByte & offs) + (symbol >> 8)], (symbol >> 7) & 1);
    symbol <<= 1;
    offs &= ~(matchByte ^ symbol);
  }
  while (symbol < 0x10000);
  return price;
}


static void RcTree_Encode(CRangeEnc *rc, CLzmaProb *probs, int numBitLevels, UInt32_7z symbol)
{
  UInt32_7z m = 1;
  int i;
  for (i = numBitLevels; i != 0;)
  {
    UInt32_7z bit;
    i--;
    bit = (symbol >> i) & 1;
    RangeEnc_EncodeBit(rc, probs + m, bit);
    m = (m << 1) | bit;
  }
}

static void RcTree_ReverseEncode(CRangeEnc *rc, CLzmaProb *probs, int numBitLevels, UInt32_7z symbol)
{
  UInt32_7z m = 1;
  int i;
  for (i = 0; i < numBitLevels; i++)
  {
    UInt32_7z bit = symbol & 1;
    RangeEnc_EncodeBit(rc, probs + m, bit);
    m = (m << 1) | bit;
    symbol >>= 1;
  }
}

static UInt32_7z RcTree_GetPrice(const CLzmaProb *probs, int numBitLevels, UInt32_7z symbol, const UInt32_7z *ProbPrices)
{
  UInt32_7z price = 0;
  symbol |= (1 << numBitLevels);
  while (symbol != 1)
  {
    price += GET_PRICEa(probs[symbol >> 1], symbol & 1);
    symbol >>= 1;
  }
  return price;
}

static UInt32_7z RcTree_ReverseGetPrice(const CLzmaProb *probs, int numBitLevels, UInt32_7z symbol, const UInt32_7z *ProbPrices)
{
  UInt32_7z price = 0;
  UInt32_7z m = 1;
  int i;
  for (i = numBitLevels; i != 0; i--)
  {
    UInt32_7z bit = symbol & 1;
    symbol >>= 1;
    price += GET_PRICEa(probs[m], bit);
    m = (m << 1) | bit;
  }
  return price;
}


static void LenEnc_Init(CLenEnc *p)
{
  unsigned i;
  p->choice = p->choice2 = kProbInitValue;
  for (i = 0; i < (LZMA_NUM_PB_STATES_MAX << kLenNumLowBits); i++)
    p->low[i] = kProbInitValue;
  for (i = 0; i < (LZMA_NUM_PB_STATES_MAX << kLenNumMidBits); i++)
    p->mid[i] = kProbInitValue;
  for (i = 0; i < kLenNumHighSymbols; i++)
    p->high[i] = kProbInitValue;
}

static void LenEnc_Encode(CLenEnc *p, CRangeEnc *rc, UInt32_7z symbol, UInt32_7z posState)
{
  if (symbol < kLenNumLowSymbols)
  {
    RangeEnc_EncodeBit(rc, &p->choice, 0);
    RcTree_Encode(rc, p->low + (posState << kLenNumLowBits), kLenNumLowBits, symbol);
  }
  else
  {
    RangeEnc_EncodeBit(rc, &p->choice, 1);
    if (symbol < kLenNumLowSymbols + kLenNumMidSymbols)
    {
      RangeEnc_EncodeBit(rc, &p->choice2, 0);
      RcTree_Encode(rc, p->mid + (posState << kLenNumMidBits), kLenNumMidBits, symbol - kLenNumLowSymbols);
    }
    else
    {
      RangeEnc_EncodeBit(rc, &p->choice2, 1);
      RcTree_Encode(rc, p->high, kLenNumHighBits, symbol - kLenNumLowSymbols - kLenNumMidSymbols);
    }
  }
}

static void LenEnc_SetPrices(CLenEnc *p, UInt32_7z posState, UInt32_7z numSymbols, UInt32_7z *prices, const UInt32_7z *ProbPrices)
{
  UInt32_7z a0 = GET_PRICE_0a(p->choice);
  UInt32_7z a1 = GET_PRICE_1a(p->choice);
  UInt32_7z b0 = a1 + GET_PRICE_0a(p->choice2);
  UInt32_7z b1 = a1 + GET_PRICE_1a(p->choice2);
  UInt32_7z i = 0;
  for (i = 0; i < kLenNumLowSymbols; i++)
  {
    if (i >= numSymbols)
      return;
    prices[i] = a0 + RcTree_GetPrice(p->low + (posState << kLenNumLowBits), kLenNumLowBits, i, ProbPrices);
  }
  for (; i < kLenNumLowSymbols + kLenNumMidSymbols; i++)
  {
    if (i >= numSymbols)
      return;
    prices[i] = b0 + RcTree_GetPrice(p->mid + (posState << kLenNumMidBits), kLenNumMidBits, i - kLenNumLowSymbols, ProbPrices);
  }
  for (; i < numSymbols; i++)
    prices[i] = b1 + RcTree_GetPrice(p->high, kLenNumHighBits, i - kLenNumLowSymbols - kLenNumMidSymbols, ProbPrices);
}

static void MY_FAST_CALL LenPriceEnc_UpdateTable(CLenPriceEnc *p, UInt32_7z posState, const UInt32_7z *ProbPrices)
{
  LenEnc_SetPrices(&p->p, posState, p->tableSize, p->prices[posState], ProbPrices);
  p->counters[posState] = p->tableSize;
}

static void LenPriceEnc_UpdateTables(CLenPriceEnc *p, UInt32_7z numPosStates, const UInt32_7z *ProbPrices)
{
  UInt32_7z posState;
  for (posState = 0; posState < numPosStates; posState++)
    LenPriceEnc_UpdateTable(p, posState, ProbPrices);
}

static void LenEnc_Encode2(CLenPriceEnc *p, CRangeEnc *rc, UInt32_7z symbol, UInt32_7z posState, Bool_7z updatePrice, const UInt32_7z *ProbPrices)
{
  LenEnc_Encode(&p->p, rc, symbol, posState);
  if (updatePrice)
    if (--p->counters[posState] == 0)
      LenPriceEnc_UpdateTable(p, posState, ProbPrices);
}




static void MovePos(CLzmaEnc *p, UInt32_7z num)
{
  #ifdef SHOW_STAT
  g_STAT_OFFSET += num;
  printf("\n MovePos %u", num);
  #endif
  
  if (num != 0)
  {
    p->additionalOffset += num;
    p->matchFinder.Skip(p->matchFinderObj, num);
  }
}

static UInt32_7z ReadMatchDistances(CLzmaEnc *p, UInt32_7z *numDistancePairsRes)
{
  UInt32_7z lenRes = 0, numPairs;
  p->numAvail = p->matchFinder.GetNumAvailableBytes(p->matchFinderObj);
  numPairs = p->matchFinder.GetMatches(p->matchFinderObj, p->matches);
  
  #ifdef SHOW_STAT
  printf("\n i = %u numPairs = %u    ", g_STAT_OFFSET, numPairs / 2);
  g_STAT_OFFSET++;
  {
    UInt32_7z i;
    for (i = 0; i < numPairs; i += 2)
      printf("%2u %6u   | ", p->matches[i], p->matches[i + 1]);
  }
  #endif
  
  if (numPairs > 0)
  {
    lenRes = p->matches[numPairs - 2];
    if (lenRes == p->numFastBytes)
    {
      UInt32_7z numAvail = p->numAvail;
      if (numAvail > LZMA_MATCH_LEN_MAX)
        numAvail = LZMA_MATCH_LEN_MAX;
      {
        const Byte *pbyCur = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - 1;
        const Byte *pby = pbyCur + lenRes;
        ptrdiff_t dif = (ptrdiff_t)-1 - p->matches[numPairs - 1];
        const Byte *pbyLim = pbyCur + numAvail;
        for (; pby != pbyLim && *pby == pby[dif]; pby++);
        lenRes = (UInt32_7z)(pby - pbyCur);
      }
    }
  }
  p->additionalOffset++;
  *numDistancePairsRes = numPairs;
  return lenRes;
}


#define MakeAsChar(p) (p)->backPrev = (UInt32_7z)(-1); (p)->prev1IsChar = false;
#define MakeAsShortRep(p) (p)->backPrev = 0; (p)->prev1IsChar = false;
#define IsShortRep(p) ((p)->backPrev == 0)

static UInt32_7z GetRepLen1Price(CLzmaEnc *p, UInt32_7z state, UInt32_7z posState)
{
  return
    GET_PRICE_0(p->isRepG0[state]) +
    GET_PRICE_0(p->isRep0Long[state][posState]);
}

static UInt32_7z GetPureRepPrice(CLzmaEnc *p, UInt32_7z repIndex, UInt32_7z state, UInt32_7z posState)
{
  UInt32_7z price;
  if (repIndex == 0)
  {
    price = GET_PRICE_0(p->isRepG0[state]);
    price += GET_PRICE_1(p->isRep0Long[state][posState]);
  }
  else
  {
    price = GET_PRICE_1(p->isRepG0[state]);
    if (repIndex == 1)
      price += GET_PRICE_0(p->isRepG1[state]);
    else
    {
      price += GET_PRICE_1(p->isRepG1[state]);
      price += GET_PRICE(p->isRepG2[state], repIndex - 2);
    }
  }
  return price;
}

static UInt32_7z GetRepPrice(CLzmaEnc *p, UInt32_7z repIndex, UInt32_7z len, UInt32_7z state, UInt32_7z posState)
{
  return p->repLenEnc.prices[posState][len - LZMA_MATCH_LEN_MIN] +
    GetPureRepPrice(p, repIndex, state, posState);
}

static UInt32_7z Backward(CLzmaEnc *p, UInt32_7z *backRes, UInt32_7z cur)
{
  UInt32_7z posMem = p->opt[cur].posPrev;
  UInt32_7z backMem = p->opt[cur].backPrev;
  p->optimumEndIndex = cur;
  do
  {
    if (p->opt[cur].prev1IsChar)
    {
      MakeAsChar(&p->opt[posMem])
      p->opt[posMem].posPrev = posMem - 1;
      if (p->opt[cur].prev2)
      {
        p->opt[posMem - 1].prev1IsChar = false;
        p->opt[posMem - 1].posPrev = p->opt[cur].posPrev2;
        p->opt[posMem - 1].backPrev = p->opt[cur].backPrev2;
      }
    }
    {
      UInt32_7z posPrev = posMem;
      UInt32_7z backCur = backMem;
      
      backMem = p->opt[posPrev].backPrev;
      posMem = p->opt[posPrev].posPrev;
      
      p->opt[posPrev].backPrev = backCur;
      p->opt[posPrev].posPrev = cur;
      cur = posPrev;
    }
  }
  while (cur != 0);
  *backRes = p->opt[0].backPrev;
  p->optimumCurrentIndex  = p->opt[0].posPrev;
  return p->optimumCurrentIndex;
}

#define LIT_PROBS(pos, prevByte) (p->litProbs + ((((pos) & p->lpMask) << p->lc) + ((prevByte) >> (8 - p->lc))) * (UInt32_7z)0x300)

static UInt32_7z GetOptimum(CLzmaEnc *p, UInt32_7z position, UInt32_7z *backRes)
{
  UInt32_7z lenEnd, cur;
  UInt32_7z reps[LZMA_NUM_REPS], repLens[LZMA_NUM_REPS];
  UInt32_7z *matches;

  {

  UInt32_7z numAvail, mainLen, numPairs, repMaxIndex, i, posState, len;
  UInt32_7z matchPrice, repMatchPrice, normalMatchPrice;
  const Byte *data;
  Byte curByte, matchByte;

  if (p->optimumEndIndex != p->optimumCurrentIndex)
  {
    const COptimal *opt = &p->opt[p->optimumCurrentIndex];
    UInt32_7z lenRes = opt->posPrev - p->optimumCurrentIndex;
    *backRes = opt->backPrev;
    p->optimumCurrentIndex = opt->posPrev;
    return lenRes;
  }
  p->optimumCurrentIndex = p->optimumEndIndex = 0;
  
  if (p->additionalOffset == 0)
    mainLen = ReadMatchDistances(p, &numPairs);
  else
  {
    mainLen = p->longestMatchLength;
    numPairs = p->numPairs;
  }

  numAvail = p->numAvail;
  if (numAvail < 2)
  {
    *backRes = (UInt32_7z)(-1);
    return 1;
  }
  if (numAvail > LZMA_MATCH_LEN_MAX)
    numAvail = LZMA_MATCH_LEN_MAX;

  data = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - 1;
  repMaxIndex = 0;
  for (i = 0; i < LZMA_NUM_REPS; i++)
  {
    UInt32_7z lenTest;
    const Byte *data2;
    reps[i] = p->reps[i];
    data2 = data - reps[i] - 1;
    if (data[0] != data2[0] || data[1] != data2[1])
    {
      repLens[i] = 0;
      continue;
    }
    for (lenTest = 2; lenTest < numAvail && data[lenTest] == data2[lenTest]; lenTest++);
    repLens[i] = lenTest;
    if (lenTest > repLens[repMaxIndex])
      repMaxIndex = i;
  }
  if (repLens[repMaxIndex] >= p->numFastBytes)
  {
    UInt32_7z lenRes;
    *backRes = repMaxIndex;
    lenRes = repLens[repMaxIndex];
    MovePos(p, lenRes - 1);
    return lenRes;
  }

  matches = p->matches;
  if (mainLen >= p->numFastBytes)
  {
    *backRes = matches[numPairs - 1] + LZMA_NUM_REPS;
    MovePos(p, mainLen - 1);
    return mainLen;
  }
  curByte = *data;
  matchByte = *(data - (reps[0] + 1));

  if (mainLen < 2 && curByte != matchByte && repLens[repMaxIndex] < 2)
  {
    *backRes = (UInt32_7z)-1;
    return 1;
  }

  p->opt[0].state = (CState)p->state;

  posState = (position & p->pbMask);

  {
    const CLzmaProb *probs = LIT_PROBS(position, *(data - 1));
    p->opt[1].price = GET_PRICE_0(p->isMatch[p->state][posState]) +
        (!IsCharState(p->state) ?
          LitEnc_GetPriceMatched(probs, curByte, matchByte, p->ProbPrices) :
          LitEnc_GetPrice(probs, curByte, p->ProbPrices));
  }

  MakeAsChar(&p->opt[1]);

  matchPrice = GET_PRICE_1(p->isMatch[p->state][posState]);
  repMatchPrice = matchPrice + GET_PRICE_1(p->isRep[p->state]);

  if (matchByte == curByte)
  {
    UInt32_7z shortRepPrice = repMatchPrice + GetRepLen1Price(p, p->state, posState);
    if (shortRepPrice < p->opt[1].price)
    {
      p->opt[1].price = shortRepPrice;
      MakeAsShortRep(&p->opt[1]);
    }
  }
  lenEnd = ((mainLen >= repLens[repMaxIndex]) ? mainLen : repLens[repMaxIndex]);

  if (lenEnd < 2)
  {
    *backRes = p->opt[1].backPrev;
    return 1;
  }

  p->opt[1].posPrev = 0;
  for (i = 0; i < LZMA_NUM_REPS; i++)
    p->opt[0].backs[i] = reps[i];

  len = lenEnd;
  do
    p->opt[len--].price = kInfinityPrice;
  while (len >= 2);

  for (i = 0; i < LZMA_NUM_REPS; i++)
  {
    UInt32_7z repLen = repLens[i];
    UInt32_7z price;
    if (repLen < 2)
      continue;
    price = repMatchPrice + GetPureRepPrice(p, i, p->state, posState);
    do
    {
      UInt32_7z curAndLenPrice = price + p->repLenEnc.prices[posState][repLen - 2];
      COptimal *opt = &p->opt[repLen];
      if (curAndLenPrice < opt->price)
      {
        opt->price = curAndLenPrice;
        opt->posPrev = 0;
        opt->backPrev = i;
        opt->prev1IsChar = false;
      }
    }
    while (--repLen >= 2);
  }

  normalMatchPrice = matchPrice + GET_PRICE_0(p->isRep[p->state]);

  len = ((repLens[0] >= 2) ? repLens[0] + 1 : 2);
  if (len <= mainLen)
  {
    UInt32_7z offs = 0;
    while (len > matches[offs])
      offs += 2;
    for (; ; len++)
    {
      COptimal *opt;
      UInt32_7z distance = matches[offs + 1];

      UInt32_7z curAndLenPrice = normalMatchPrice + p->lenEnc.prices[posState][len - LZMA_MATCH_LEN_MIN];
      UInt32_7z lenToPosState = GetLenToPosState(len);
      if (distance < kNumFullDistances)
        curAndLenPrice += p->distancesPrices[lenToPosState][distance];
      else
      {
        UInt32_7z slot;
        GetPosSlot2(distance, slot);
        curAndLenPrice += p->alignPrices[distance & kAlignMask] + p->posSlotPrices[lenToPosState][slot];
      }
      opt = &p->opt[len];
      if (curAndLenPrice < opt->price)
      {
        opt->price = curAndLenPrice;
        opt->posPrev = 0;
        opt->backPrev = distance + LZMA_NUM_REPS;
        opt->prev1IsChar = false;
      }
      if (len == matches[offs])
      {
        offs += 2;
        if (offs == numPairs)
          break;
      }
    }
  }

  cur = 0;

    #ifdef SHOW_STAT2
    /* if (position >= 0) */
    {
      unsigned i;
      printf("\n pos = %4X", position);
      for (i = cur; i <= lenEnd; i++)
      printf("\nprice[%4X] = %u", position - cur + i, p->opt[i].price);
    }
    #endif

  }

  for (;;)
  {
    UInt32_7z numAvail;
    UInt32_7z numAvailFull, newLen, numPairs, posPrev, state, posState, startLen;
    UInt32_7z curPrice, curAnd1Price, matchPrice, repMatchPrice;
    Bool_7z nextIsChar;
    Byte curByte, matchByte;
    const Byte *data;
    COptimal *curOpt;
    COptimal *nextOpt;

    cur++;
    if (cur == lenEnd)
      return Backward(p, backRes, cur);

    newLen = ReadMatchDistances(p, &numPairs);
    if (newLen >= p->numFastBytes)
    {
      p->numPairs = numPairs;
      p->longestMatchLength = newLen;
      return Backward(p, backRes, cur);
    }
    position++;
    curOpt = &p->opt[cur];
    posPrev = curOpt->posPrev;
    if (curOpt->prev1IsChar)
    {
      posPrev--;
      if (curOpt->prev2)
      {
        state = p->opt[curOpt->posPrev2].state;
        if (curOpt->backPrev2 < LZMA_NUM_REPS)
          state = kRepNextStates[state];
        else
          state = kMatchNextStates[state];
      }
      else
        state = p->opt[posPrev].state;
      state = kLiteralNextStates[state];
    }
    else
      state = p->opt[posPrev].state;
    if (posPrev == cur - 1)
    {
      if (IsShortRep(curOpt))
        state = kShortRepNextStates[state];
      else
        state = kLiteralNextStates[state];
    }
    else
    {
      UInt32_7z pos;
      const COptimal *prevOpt;
      if (curOpt->prev1IsChar && curOpt->prev2)
      {
        posPrev = curOpt->posPrev2;
        pos = curOpt->backPrev2;
        state = kRepNextStates[state];
      }
      else
      {
        pos = curOpt->backPrev;
        if (pos < LZMA_NUM_REPS)
          state = kRepNextStates[state];
        else
          state = kMatchNextStates[state];
      }
      prevOpt = &p->opt[posPrev];
      if (pos < LZMA_NUM_REPS)
      {
        UInt32_7z i;
        reps[0] = prevOpt->backs[pos];
        for (i = 1; i <= pos; i++)
          reps[i] = prevOpt->backs[i - 1];
        for (; i < LZMA_NUM_REPS; i++)
          reps[i] = prevOpt->backs[i];
      }
      else
      {
        UInt32_7z i;
        reps[0] = (pos - LZMA_NUM_REPS);
        for (i = 1; i < LZMA_NUM_REPS; i++)
          reps[i] = prevOpt->backs[i - 1];
      }
    }
    curOpt->state = (CState)state;

    curOpt->backs[0] = reps[0];
    curOpt->backs[1] = reps[1];
    curOpt->backs[2] = reps[2];
    curOpt->backs[3] = reps[3];

    curPrice = curOpt->price;
    nextIsChar = false;
    data = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - 1;
    curByte = *data;
    matchByte = *(data - (reps[0] + 1));

    posState = (position & p->pbMask);

    curAnd1Price = curPrice + GET_PRICE_0(p->isMatch[state][posState]);
    {
      const CLzmaProb *probs = LIT_PROBS(position, *(data - 1));
      curAnd1Price +=
        (!IsCharState(state) ?
          LitEnc_GetPriceMatched(probs, curByte, matchByte, p->ProbPrices) :
          LitEnc_GetPrice(probs, curByte, p->ProbPrices));
    }

    nextOpt = &p->opt[cur + 1];

    if (curAnd1Price < nextOpt->price)
    {
      nextOpt->price = curAnd1Price;
      nextOpt->posPrev = cur;
      MakeAsChar(nextOpt);
      nextIsChar = true;
    }

    matchPrice = curPrice + GET_PRICE_1(p->isMatch[state][posState]);
    repMatchPrice = matchPrice + GET_PRICE_1(p->isRep[state]);
    
    if (matchByte == curByte && !(nextOpt->posPrev < cur && nextOpt->backPrev == 0))
    {
      UInt32_7z shortRepPrice = repMatchPrice + GetRepLen1Price(p, state, posState);
      if (shortRepPrice <= nextOpt->price)
      {
        nextOpt->price = shortRepPrice;
        nextOpt->posPrev = cur;
        MakeAsShortRep(nextOpt);
        nextIsChar = true;
      }
    }
    numAvailFull = p->numAvail;
    {
      UInt32_7z temp = kNumOpts - 1 - cur;
      if (temp < numAvailFull)
        numAvailFull = temp;
    }

    if (numAvailFull < 2)
      continue;
    numAvail = (numAvailFull <= p->numFastBytes ? numAvailFull : p->numFastBytes);

    if (!nextIsChar && matchByte != curByte) /* speed optimization */
    {
      /* try Literal + rep0 */
      UInt32_7z temp;
      UInt32_7z lenTest2;
      const Byte *data2 = data - reps[0] - 1;
      UInt32_7z limit = p->numFastBytes + 1;
      if (limit > numAvailFull)
        limit = numAvailFull;

      for (temp = 1; temp < limit && data[temp] == data2[temp]; temp++);
      lenTest2 = temp - 1;
      if (lenTest2 >= 2)
      {
        UInt32_7z state2 = kLiteralNextStates[state];
        UInt32_7z posStateNext = (position + 1) & p->pbMask;
        UInt32_7z nextRepMatchPrice = curAnd1Price +
            GET_PRICE_1(p->isMatch[state2][posStateNext]) +
            GET_PRICE_1(p->isRep[state2]);
        /* for (; lenTest2 >= 2; lenTest2--) */
        {
          UInt32_7z curAndLenPrice;
          COptimal *opt;
          UInt32_7z offset = cur + 1 + lenTest2;
          while (lenEnd < offset)
            p->opt[++lenEnd].price = kInfinityPrice;
          curAndLenPrice = nextRepMatchPrice + GetRepPrice(p, 0, lenTest2, state2, posStateNext);
          opt = &p->opt[offset];
          if (curAndLenPrice < opt->price)
          {
            opt->price = curAndLenPrice;
            opt->posPrev = cur + 1;
            opt->backPrev = 0;
            opt->prev1IsChar = true;
            opt->prev2 = false;
          }
        }
      }
    }
    
    startLen = 2; /* speed optimization */
    {
    UInt32_7z repIndex;
    for (repIndex = 0; repIndex < LZMA_NUM_REPS; repIndex++)
    {
      UInt32_7z lenTest;
      UInt32_7z lenTestTemp;
      UInt32_7z price;
      const Byte *data2 = data - reps[repIndex] - 1;
      if (data[0] != data2[0] || data[1] != data2[1])
        continue;
      for (lenTest = 2; lenTest < numAvail && data[lenTest] == data2[lenTest]; lenTest++);
      while (lenEnd < cur + lenTest)
        p->opt[++lenEnd].price = kInfinityPrice;
      lenTestTemp = lenTest;
      price = repMatchPrice + GetPureRepPrice(p, repIndex, state, posState);
      do
      {
        UInt32_7z curAndLenPrice = price + p->repLenEnc.prices[posState][lenTest - 2];
        COptimal *opt = &p->opt[cur + lenTest];
        if (curAndLenPrice < opt->price)
        {
          opt->price = curAndLenPrice;
          opt->posPrev = cur;
          opt->backPrev = repIndex;
          opt->prev1IsChar = false;
        }
      }
      while (--lenTest >= 2);
      lenTest = lenTestTemp;
      
      if (repIndex == 0)
        startLen = lenTest + 1;
        
      /* if (_maxMode) */
        {
          UInt32_7z lenTest2 = lenTest + 1;
          UInt32_7z limit = lenTest2 + p->numFastBytes;
          if (limit > numAvailFull)
            limit = numAvailFull;
          for (; lenTest2 < limit && data[lenTest2] == data2[lenTest2]; lenTest2++);
          lenTest2 -= lenTest + 1;
          if (lenTest2 >= 2)
          {
            UInt32_7z nextRepMatchPrice;
            UInt32_7z state2 = kRepNextStates[state];
            UInt32_7z posStateNext = (position + lenTest) & p->pbMask;
            UInt32_7z curAndLenCharPrice =
                price + p->repLenEnc.prices[posState][lenTest - 2] +
                GET_PRICE_0(p->isMatch[state2][posStateNext]) +
                LitEnc_GetPriceMatched(LIT_PROBS(position + lenTest, data[lenTest - 1]),
                    data[lenTest], data2[lenTest], p->ProbPrices);
            state2 = kLiteralNextStates[state2];
            posStateNext = (position + lenTest + 1) & p->pbMask;
            nextRepMatchPrice = curAndLenCharPrice +
                GET_PRICE_1(p->isMatch[state2][posStateNext]) +
                GET_PRICE_1(p->isRep[state2]);
            
            /* for (; lenTest2 >= 2; lenTest2--) */
            {
              UInt32_7z curAndLenPrice;
              COptimal *opt;
              UInt32_7z offset = cur + lenTest + 1 + lenTest2;
              while (lenEnd < offset)
                p->opt[++lenEnd].price = kInfinityPrice;
              curAndLenPrice = nextRepMatchPrice + GetRepPrice(p, 0, lenTest2, state2, posStateNext);
              opt = &p->opt[offset];
              if (curAndLenPrice < opt->price)
              {
                opt->price = curAndLenPrice;
                opt->posPrev = cur + lenTest + 1;
                opt->backPrev = 0;
                opt->prev1IsChar = true;
                opt->prev2 = true;
                opt->posPrev2 = cur;
                opt->backPrev2 = repIndex;
              }
            }
          }
        }
    }
    }
    /* for (UInt32_7z lenTest = 2; lenTest <= newLen; lenTest++) */
    if (newLen > numAvail)
    {
      newLen = numAvail;
      for (numPairs = 0; newLen > matches[numPairs]; numPairs += 2);
      matches[numPairs] = newLen;
      numPairs += 2;
    }
    if (newLen >= startLen)
    {
      UInt32_7z normalMatchPrice = matchPrice + GET_PRICE_0(p->isRep[state]);
      UInt32_7z offs, curBack, posSlot;
      UInt32_7z lenTest;
      while (lenEnd < cur + newLen)
        p->opt[++lenEnd].price = kInfinityPrice;

      offs = 0;
      while (startLen > matches[offs])
        offs += 2;
      curBack = matches[offs + 1];
      GetPosSlot2(curBack, posSlot);
      for (lenTest = /*2*/ startLen; ; lenTest++)
      {
        UInt32_7z curAndLenPrice = normalMatchPrice + p->lenEnc.prices[posState][lenTest - LZMA_MATCH_LEN_MIN];
        {
        UInt32_7z lenToPosState = GetLenToPosState(lenTest);
        COptimal *opt;
        if (curBack < kNumFullDistances)
          curAndLenPrice += p->distancesPrices[lenToPosState][curBack];
        else
          curAndLenPrice += p->posSlotPrices[lenToPosState][posSlot] + p->alignPrices[curBack & kAlignMask];
        
        opt = &p->opt[cur + lenTest];
        if (curAndLenPrice < opt->price)
        {
          opt->price = curAndLenPrice;
          opt->posPrev = cur;
          opt->backPrev = curBack + LZMA_NUM_REPS;
          opt->prev1IsChar = false;
        }
        }

        if (/*_maxMode && */lenTest == matches[offs])
        {
          /* Try Match + Literal + Rep0 */
          const Byte *data2 = data - curBack - 1;
          UInt32_7z lenTest2 = lenTest + 1;
          UInt32_7z limit = lenTest2 + p->numFastBytes;
          if (limit > numAvailFull)
            limit = numAvailFull;
          for (; lenTest2 < limit && data[lenTest2] == data2[lenTest2]; lenTest2++);
          lenTest2 -= lenTest + 1;
          if (lenTest2 >= 2)
          {
            UInt32_7z nextRepMatchPrice;
            UInt32_7z state2 = kMatchNextStates[state];
            UInt32_7z posStateNext = (position + lenTest) & p->pbMask;
            UInt32_7z curAndLenCharPrice = curAndLenPrice +
                GET_PRICE_0(p->isMatch[state2][posStateNext]) +
                LitEnc_GetPriceMatched(LIT_PROBS(position + lenTest, data[lenTest - 1]),
                    data[lenTest], data2[lenTest], p->ProbPrices);
            state2 = kLiteralNextStates[state2];
            posStateNext = (posStateNext + 1) & p->pbMask;
            nextRepMatchPrice = curAndLenCharPrice +
                GET_PRICE_1(p->isMatch[state2][posStateNext]) +
                GET_PRICE_1(p->isRep[state2]);
            
            /* for (; lenTest2 >= 2; lenTest2--) */
            {
              UInt32_7z offset = cur + lenTest + 1 + lenTest2;
              UInt32_7z curAndLenPrice2;
              COptimal *opt;
              while (lenEnd < offset)
                p->opt[++lenEnd].price = kInfinityPrice;
              curAndLenPrice2 = nextRepMatchPrice + GetRepPrice(p, 0, lenTest2, state2, posStateNext);
              opt = &p->opt[offset];
              if (curAndLenPrice2 < opt->price)
              {
                opt->price = curAndLenPrice2;
                opt->posPrev = cur + lenTest + 1;
                opt->backPrev = 0;
                opt->prev1IsChar = true;
                opt->prev2 = true;
                opt->posPrev2 = cur;
                opt->backPrev2 = curBack + LZMA_NUM_REPS;
              }
            }
          }
          offs += 2;
          if (offs == numPairs)
            break;
          curBack = matches[offs + 1];
          if (curBack >= kNumFullDistances)
            GetPosSlot2(curBack, posSlot);
        }
      }
    }
  }
}

#define ChangePair(smallDist, bigDist) (((bigDist) >> 7) > (smallDist))

static UInt32_7z GetOptimumFast(CLzmaEnc *p, UInt32_7z *backRes)
{
  UInt32_7z numAvail, mainLen, mainDist, numPairs, repIndex, repLen, i;
  const Byte *data;
  const UInt32_7z *matches;

  if (p->additionalOffset == 0)
    mainLen = ReadMatchDistances(p, &numPairs);
  else
  {
    mainLen = p->longestMatchLength;
    numPairs = p->numPairs;
  }

  numAvail = p->numAvail;
  *backRes = (UInt32_7z)-1;
  if (numAvail < 2)
    return 1;
  if (numAvail > LZMA_MATCH_LEN_MAX)
    numAvail = LZMA_MATCH_LEN_MAX;
  data = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - 1;

  repLen = repIndex = 0;
  for (i = 0; i < LZMA_NUM_REPS; i++)
  {
    UInt32_7z len;
    const Byte *data2 = data - p->reps[i] - 1;
    if (data[0] != data2[0] || data[1] != data2[1])
      continue;
    for (len = 2; len < numAvail && data[len] == data2[len]; len++);
    if (len >= p->numFastBytes)
    {
      *backRes = i;
      MovePos(p, len - 1);
      return len;
    }
    if (len > repLen)
    {
      repIndex = i;
      repLen = len;
    }
  }

  matches = p->matches;
  if (mainLen >= p->numFastBytes)
  {
    *backRes = matches[numPairs - 1] + LZMA_NUM_REPS;
    MovePos(p, mainLen - 1);
    return mainLen;
  }

  mainDist = 0; /* for GCC */
  if (mainLen >= 2)
  {
    mainDist = matches[numPairs - 1];
    while (numPairs > 2 && mainLen == matches[numPairs - 4] + 1)
    {
      if (!ChangePair(matches[numPairs - 3], mainDist))
        break;
      numPairs -= 2;
      mainLen = matches[numPairs - 2];
      mainDist = matches[numPairs - 1];
    }
    if (mainLen == 2 && mainDist >= 0x80)
      mainLen = 1;
  }

  if (repLen >= 2 && (
        (repLen + 1 >= mainLen) ||
        (repLen + 2 >= mainLen && mainDist >= (1 << 9)) ||
        (repLen + 3 >= mainLen && mainDist >= (1 << 15))))
  {
    *backRes = repIndex;
    MovePos(p, repLen - 1);
    return repLen;
  }
  
  if (mainLen < 2 || numAvail <= 2)
    return 1;

  p->longestMatchLength = ReadMatchDistances(p, &p->numPairs);
  if (p->longestMatchLength >= 2)
  {
    UInt32_7z newDistance = matches[p->numPairs - 1];
    if ((p->longestMatchLength >= mainLen && newDistance < mainDist) ||
        (p->longestMatchLength == mainLen + 1 && !ChangePair(mainDist, newDistance)) ||
        (p->longestMatchLength > mainLen + 1) ||
        (p->longestMatchLength + 1 >= mainLen && mainLen >= 3 && ChangePair(newDistance, mainDist)))
      return 1;
  }
  
  data = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - 1;
  for (i = 0; i < LZMA_NUM_REPS; i++)
  {
    UInt32_7z len, limit;
    const Byte *data2 = data - p->reps[i] - 1;
    if (data[0] != data2[0] || data[1] != data2[1])
      continue;
    limit = mainLen - 1;
    for (len = 2; len < limit && data[len] == data2[len]; len++);
    if (len >= limit)
      return 1;
  }
  *backRes = mainDist + LZMA_NUM_REPS;
  MovePos(p, mainLen - 2);
  return mainLen;
}

static void WriteEndMarker(CLzmaEnc *p, UInt32_7z posState)
{
  UInt32_7z len;
  RangeEnc_EncodeBit(&p->rc, &p->isMatch[p->state][posState], 1);
  RangeEnc_EncodeBit(&p->rc, &p->isRep[p->state], 0);
  p->state = kMatchNextStates[p->state];
  len = LZMA_MATCH_LEN_MIN;
  LenEnc_Encode2(&p->lenEnc, &p->rc, len - LZMA_MATCH_LEN_MIN, posState, !p->fastMode, p->ProbPrices);
  RcTree_Encode(&p->rc, p->posSlotEncoder[GetLenToPosState(len)], kNumPosSlotBits, (1 << kNumPosSlotBits) - 1);
  RangeEnc_EncodeDirectBits(&p->rc, (((UInt32_7z)1 << 30) - 1) >> kNumAlignBits, 30 - kNumAlignBits);
  RcTree_ReverseEncode(&p->rc, p->posAlignEncoder, kNumAlignBits, kAlignMask);
}

static SRes CheckErrors(CLzmaEnc *p)
{
  if (p->result != SZ_OK)
    return p->result;
  if (p->rc.res != SZ_OK)
    p->result = SZ_ERROR_WRITE;
  if (p->matchFinderBase.result != SZ_OK)
    p->result = SZ_ERROR_READ;
  if (p->result != SZ_OK)
    p->finished = true;
  return p->result;
}

static SRes Flush(CLzmaEnc *p, UInt32_7z nowPos)
{
  /* ReleaseMFStream(); */
  p->finished = true;
  if (p->writeEndMark)
    WriteEndMarker(p, nowPos & p->pbMask);
  RangeEnc_FlushData(&p->rc);
  RangeEnc_FlushStream(&p->rc);
  return CheckErrors(p);
}

static void FillAlignPrices(CLzmaEnc *p)
{
  UInt32_7z i;
  for (i = 0; i < kAlignTableSize; i++)
    p->alignPrices[i] = RcTree_ReverseGetPrice(p->posAlignEncoder, kNumAlignBits, i, p->ProbPrices);
  p->alignPriceCount = 0;
}

static void FillDistancesPrices(CLzmaEnc *p)
{
  UInt32_7z tempPrices[kNumFullDistances];
  UInt32_7z i, lenToPosState;
  for (i = kStartPosModelIndex; i < kNumFullDistances; i++)
  {
    UInt32_7z posSlot = GetPosSlot1(i);
    UInt32_7z footerBits = ((posSlot >> 1) - 1);
    UInt32_7z base = ((2 | (posSlot & 1)) << footerBits);
    tempPrices[i] = RcTree_ReverseGetPrice(p->posEncoders + base - posSlot - 1, footerBits, i - base, p->ProbPrices);
  }

  for (lenToPosState = 0; lenToPosState < kNumLenToPosStates; lenToPosState++)
  {
    UInt32_7z posSlot;
    const CLzmaProb *encoder = p->posSlotEncoder[lenToPosState];
    UInt32_7z *posSlotPrices = p->posSlotPrices[lenToPosState];
    for (posSlot = 0; posSlot < p->distTableSize; posSlot++)
      posSlotPrices[posSlot] = RcTree_GetPrice(encoder, kNumPosSlotBits, posSlot, p->ProbPrices);
    for (posSlot = kEndPosModelIndex; posSlot < p->distTableSize; posSlot++)
      posSlotPrices[posSlot] += ((((posSlot >> 1) - 1) - kNumAlignBits) << kNumBitPriceShiftBits);

    {
      UInt32_7z *distancesPrices = p->distancesPrices[lenToPosState];
      for (i = 0; i < kStartPosModelIndex; i++)
        distancesPrices[i] = posSlotPrices[i];
      for (; i < kNumFullDistances; i++)
        distancesPrices[i] = posSlotPrices[GetPosSlot1(i)] + tempPrices[i];
    }
  }
  p->matchPriceCount = 0;
}

void LzmaEnc_Construct(CLzmaEnc *p)
{
  RangeEnc_Construct(&p->rc);
  MatchFinder_Construct(&p->matchFinderBase);
  
  #ifndef _7ZIP_ST
  MatchFinderMt_Construct(&p->matchFinderMt);
  p->matchFinderMt.MatchFinder = &p->matchFinderBase;
  #endif

  {
    CLzmaEncProps props;
    LzmaEncProps_Init(&props);
    LzmaEnc_SetProps(p, &props);
  }

  #ifndef LZMA_LOG_BSR
  LzmaEnc_FastPosInit(p->g_FastPos);
  #endif

  LzmaEnc_InitPriceTables(p->ProbPrices);
  p->litProbs = NULL;
  p->saveState.litProbs = NULL;
}

CLzmaEncHandle LzmaEnc_Create(ISzAlloc *alloc)
{
  void *p;
  p = alloc->Alloc(alloc, sizeof(CLzmaEnc));
  if (p)
    LzmaEnc_Construct((CLzmaEnc *)p);
  return p;
}

void LzmaEnc_FreeLits(CLzmaEnc *p, ISzAlloc *alloc)
{
  alloc->Free(alloc, p->litProbs);
  alloc->Free(alloc, p->saveState.litProbs);
  p->litProbs = NULL;
  p->saveState.litProbs = NULL;
}

void LzmaEnc_Destruct(CLzmaEnc *p, ISzAlloc *alloc, ISzAlloc *allocBig)
{
  #ifndef _7ZIP_ST
  MatchFinderMt_Destruct(&p->matchFinderMt, allocBig);
  #endif
  
  MatchFinder_Free(&p->matchFinderBase, allocBig);
  LzmaEnc_FreeLits(p, alloc);
  RangeEnc_Free(&p->rc, alloc);
}

void LzmaEnc_Destroy(CLzmaEncHandle p, ISzAlloc *alloc, ISzAlloc *allocBig)
{
  LzmaEnc_Destruct((CLzmaEnc *)p, alloc, allocBig);
  alloc->Free(alloc, p);
}

static SRes LzmaEnc_CodeOneBlock(CLzmaEnc *p, Bool_7z useLimits, UInt32_7z maxPackSize, UInt32_7z maxUnpackSize)
{
  UInt32_7z nowPos32, startPos32;
  if (p->needInit)
  {
    p->matchFinder.Init(p->matchFinderObj);
    p->needInit = 0;
  }

  if (p->finished)
    return p->result;
  RINOK(CheckErrors(p));

  nowPos32 = (UInt32_7z)p->nowPos64;
  startPos32 = nowPos32;

  if (p->nowPos64 == 0)
  {
    UInt32_7z numPairs;
    Byte curByte;
    if (p->matchFinder.GetNumAvailableBytes(p->matchFinderObj) == 0)
      return Flush(p, nowPos32);
    ReadMatchDistances(p, &numPairs);
    RangeEnc_EncodeBit(&p->rc, &p->isMatch[p->state][0], 0);
    p->state = kLiteralNextStates[p->state];
    curByte = *(p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - p->additionalOffset);
    LitEnc_Encode(&p->rc, p->litProbs, curByte);
    p->additionalOffset--;
    nowPos32++;
  }

  if (p->matchFinder.GetNumAvailableBytes(p->matchFinderObj) != 0)
  for (;;)
  {
    UInt32_7z pos, len, posState;

    if (p->fastMode)
      len = GetOptimumFast(p, &pos);
    else
      len = GetOptimum(p, nowPos32, &pos);

    #ifdef SHOW_STAT2
    printf("\n pos = %4X,   len = %u   pos = %u", nowPos32, len, pos);
    #endif

    posState = nowPos32 & p->pbMask;
    if (len == 1 && pos == (UInt32_7z)-1)
    {
      Byte curByte;
      CLzmaProb *probs;
      const Byte *data;

      RangeEnc_EncodeBit(&p->rc, &p->isMatch[p->state][posState], 0);
      data = p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - p->additionalOffset;
      curByte = *data;
      probs = LIT_PROBS(nowPos32, *(data - 1));
      if (IsCharState(p->state))
        LitEnc_Encode(&p->rc, probs, curByte);
      else
        LitEnc_EncodeMatched(&p->rc, probs, curByte, *(data - p->reps[0] - 1));
      p->state = kLiteralNextStates[p->state];
    }
    else
    {
      RangeEnc_EncodeBit(&p->rc, &p->isMatch[p->state][posState], 1);
      if (pos < LZMA_NUM_REPS)
      {
        RangeEnc_EncodeBit(&p->rc, &p->isRep[p->state], 1);
        if (pos == 0)
        {
          RangeEnc_EncodeBit(&p->rc, &p->isRepG0[p->state], 0);
          RangeEnc_EncodeBit(&p->rc, &p->isRep0Long[p->state][posState], ((len == 1) ? 0 : 1));
        }
        else
        {
          UInt32_7z distance = p->reps[pos];
          RangeEnc_EncodeBit(&p->rc, &p->isRepG0[p->state], 1);
          if (pos == 1)
            RangeEnc_EncodeBit(&p->rc, &p->isRepG1[p->state], 0);
          else
          {
            RangeEnc_EncodeBit(&p->rc, &p->isRepG1[p->state], 1);
            RangeEnc_EncodeBit(&p->rc, &p->isRepG2[p->state], pos - 2);
            if (pos == 3)
              p->reps[3] = p->reps[2];
            p->reps[2] = p->reps[1];
          }
          p->reps[1] = p->reps[0];
          p->reps[0] = distance;
        }
        if (len == 1)
          p->state = kShortRepNextStates[p->state];
        else
        {
          LenEnc_Encode2(&p->repLenEnc, &p->rc, len - LZMA_MATCH_LEN_MIN, posState, !p->fastMode, p->ProbPrices);
          p->state = kRepNextStates[p->state];
        }
      }
      else
      {
        UInt32_7z posSlot;
        RangeEnc_EncodeBit(&p->rc, &p->isRep[p->state], 0);
        p->state = kMatchNextStates[p->state];
        LenEnc_Encode2(&p->lenEnc, &p->rc, len - LZMA_MATCH_LEN_MIN, posState, !p->fastMode, p->ProbPrices);
        pos -= LZMA_NUM_REPS;
        GetPosSlot(pos, posSlot);
        RcTree_Encode(&p->rc, p->posSlotEncoder[GetLenToPosState(len)], kNumPosSlotBits, posSlot);
        
        if (posSlot >= kStartPosModelIndex)
        {
          UInt32_7z footerBits = ((posSlot >> 1) - 1);
          UInt32_7z base = ((2 | (posSlot & 1)) << footerBits);
          UInt32_7z posReduced = pos - base;

          if (posSlot < kEndPosModelIndex)
            RcTree_ReverseEncode(&p->rc, p->posEncoders + base - posSlot - 1, footerBits, posReduced);
          else
          {
            RangeEnc_EncodeDirectBits(&p->rc, posReduced >> kNumAlignBits, footerBits - kNumAlignBits);
            RcTree_ReverseEncode(&p->rc, p->posAlignEncoder, kNumAlignBits, posReduced & kAlignMask);
            p->alignPriceCount++;
          }
        }
        p->reps[3] = p->reps[2];
        p->reps[2] = p->reps[1];
        p->reps[1] = p->reps[0];
        p->reps[0] = pos;
        p->matchPriceCount++;
      }
    }
    p->additionalOffset -= len;
    nowPos32 += len;
    if (p->additionalOffset == 0)
    {
      UInt32_7z processed;
      if (!p->fastMode)
      {
        if (p->matchPriceCount >= (1 << 7))
          FillDistancesPrices(p);
        if (p->alignPriceCount >= kAlignTableSize)
          FillAlignPrices(p);
      }
      if (p->matchFinder.GetNumAvailableBytes(p->matchFinderObj) == 0)
        break;
      processed = nowPos32 - startPos32;
      if (useLimits)
      {
        if (processed + kNumOpts + 300 >= maxUnpackSize ||
            RangeEnc_GetProcessed(&p->rc) + kNumOpts * 2 >= maxPackSize)
          break;
      }
      else if (processed >= (1 << 17))
      {
        p->nowPos64 += nowPos32 - startPos32;
        return CheckErrors(p);
      }
    }
  }
  p->nowPos64 += nowPos32 - startPos32;
  return Flush(p, nowPos32);
}

#define kBigHashDicLimit ((UInt32_7z)1 << 24)

static SRes LzmaEnc_Alloc(CLzmaEnc *p, UInt32_7z keepWindowSize, ISzAlloc *alloc, ISzAlloc *allocBig)
{
  UInt32_7z beforeSize = kNumOpts;
  if (!RangeEnc_Alloc(&p->rc, alloc))
    return SZ_ERROR_MEM;

  #ifndef _7ZIP_ST
  p->mtMode = (p->multiThread && !p->fastMode && (p->matchFinderBase.btMode != 0));
  #endif

  {
    unsigned lclp = p->lc + p->lp;
    if (!p->litProbs || !p->saveState.litProbs || p->lclp != lclp)
    {
      LzmaEnc_FreeLits(p, alloc);
      p->litProbs = (CLzmaProb *)alloc->Alloc(alloc, ((UInt32_7z)0x300 << lclp) * sizeof(CLzmaProb));
      p->saveState.litProbs = (CLzmaProb *)alloc->Alloc(alloc, ((UInt32_7z)0x300 << lclp) * sizeof(CLzmaProb));
      if (!p->litProbs || !p->saveState.litProbs)
      {
        LzmaEnc_FreeLits(p, alloc);
        return SZ_ERROR_MEM;
      }
      p->lclp = lclp;
    }
  }

  p->matchFinderBase.bigHash = (Byte)(p->dictSize > kBigHashDicLimit ? 1 : 0);

  if (beforeSize + p->dictSize < keepWindowSize)
    beforeSize = keepWindowSize - p->dictSize;

  #ifndef _7ZIP_ST
  if (p->mtMode)
  {
    RINOK(MatchFinderMt_Create(&p->matchFinderMt, p->dictSize, beforeSize, p->numFastBytes, LZMA_MATCH_LEN_MAX, allocBig));
    p->matchFinderObj = &p->matchFinderMt;
    MatchFinderMt_CreateVTable(&p->matchFinderMt, &p->matchFinder);
  }
  else
  #endif
  {
    if (!MatchFinder_Create(&p->matchFinderBase, p->dictSize, beforeSize, p->numFastBytes, LZMA_MATCH_LEN_MAX, allocBig))
      return SZ_ERROR_MEM;
    p->matchFinderObj = &p->matchFinderBase;
    MatchFinder_CreateVTable(&p->matchFinderBase, &p->matchFinder);
  }
  
  return SZ_OK;
}

void LzmaEnc_Init(CLzmaEnc *p)
{
  UInt32_7z i;
  p->state = 0;
  for (i = 0 ; i < LZMA_NUM_REPS; i++)
    p->reps[i] = 0;

  RangeEnc_Init(&p->rc);


  for (i = 0; i < kNumStates; i++)
  {
    UInt32_7z j;
    for (j = 0; j < LZMA_NUM_PB_STATES_MAX; j++)
    {
      p->isMatch[i][j] = kProbInitValue;
      p->isRep0Long[i][j] = kProbInitValue;
    }
    p->isRep[i] = kProbInitValue;
    p->isRepG0[i] = kProbInitValue;
    p->isRepG1[i] = kProbInitValue;
    p->isRepG2[i] = kProbInitValue;
  }

  {
    UInt32_7z num = (UInt32_7z)0x300 << (p->lp + p->lc);
    CLzmaProb *probs = p->litProbs;
    for (i = 0; i < num; i++)
      probs[i] = kProbInitValue;
  }

  {
    for (i = 0; i < kNumLenToPosStates; i++)
    {
      CLzmaProb *probs = p->posSlotEncoder[i];
      UInt32_7z j;
      for (j = 0; j < (1 << kNumPosSlotBits); j++)
        probs[j] = kProbInitValue;
    }
  }
  {
    for (i = 0; i < kNumFullDistances - kEndPosModelIndex; i++)
      p->posEncoders[i] = kProbInitValue;
  }

  LenEnc_Init(&p->lenEnc.p);
  LenEnc_Init(&p->repLenEnc.p);

  for (i = 0; i < (1 << kNumAlignBits); i++)
    p->posAlignEncoder[i] = kProbInitValue;

  p->optimumEndIndex = 0;
  p->optimumCurrentIndex = 0;
  p->additionalOffset = 0;

  p->pbMask = (1 << p->pb) - 1;
  p->lpMask = (1 << p->lp) - 1;
}

void LzmaEnc_InitPrices(CLzmaEnc *p)
{
  if (!p->fastMode)
  {
    FillDistancesPrices(p);
    FillAlignPrices(p);
  }

  p->lenEnc.tableSize =
  p->repLenEnc.tableSize =
      p->numFastBytes + 1 - LZMA_MATCH_LEN_MIN;
  LenPriceEnc_UpdateTables(&p->lenEnc, 1 << p->pb, p->ProbPrices);
  LenPriceEnc_UpdateTables(&p->repLenEnc, 1 << p->pb, p->ProbPrices);
}

static SRes LzmaEnc_AllocAndInit(CLzmaEnc *p, UInt32_7z keepWindowSize, ISzAlloc *alloc, ISzAlloc *allocBig)
{
  UInt32_7z i;
  for (i = 0; i < (UInt32_7z)kDicLogSizeMaxCompress; i++)
    if (p->dictSize <= ((UInt32_7z)1 << i))
      break;
  p->distTableSize = i * 2;

  p->finished = false;
  p->result = SZ_OK;
  RINOK(LzmaEnc_Alloc(p, keepWindowSize, alloc, allocBig));
  LzmaEnc_Init(p);
  LzmaEnc_InitPrices(p);
  p->nowPos64 = 0;
  return SZ_OK;
}

static SRes LzmaEnc_Prepare(CLzmaEncHandle pp, ISeqOutStream *outStream, ISeqInStream *inStream,
    ISzAlloc *alloc, ISzAlloc *allocBig)
{
  CLzmaEnc *p = (CLzmaEnc *)pp;
  p->matchFinderBase.stream = inStream;
  p->needInit = 1;
  p->rc.outStream = outStream;
  return LzmaEnc_AllocAndInit(p, 0, alloc, allocBig);
}

SRes LzmaEnc_PrepareForLzma2(CLzmaEncHandle pp,
    ISeqInStream *inStream, UInt32_7z keepWindowSize,
    ISzAlloc *alloc, ISzAlloc *allocBig)
{
  CLzmaEnc *p = (CLzmaEnc *)pp;
  p->matchFinderBase.stream = inStream;
  p->needInit = 1;
  return LzmaEnc_AllocAndInit(p, keepWindowSize, alloc, allocBig);
}

static void LzmaEnc_SetInputBuf(CLzmaEnc *p, const Byte *src, SizeT srcLen)
{
  p->matchFinderBase.directInput = 1;
  p->matchFinderBase.bufferBase = (Byte *)src;
  p->matchFinderBase.directInputRem = srcLen;
}

SRes LzmaEnc_MemPrepare(CLzmaEncHandle pp, const Byte *src, SizeT srcLen,
    UInt32_7z keepWindowSize, ISzAlloc *alloc, ISzAlloc *allocBig)
{
  CLzmaEnc *p = (CLzmaEnc *)pp;
  LzmaEnc_SetInputBuf(p, src, srcLen);
  p->needInit = 1;

  return LzmaEnc_AllocAndInit(p, keepWindowSize, alloc, allocBig);
}

void LzmaEnc_Finish(CLzmaEncHandle pp)
{
  #ifndef _7ZIP_ST
  CLzmaEnc *p = (CLzmaEnc *)pp;
  if (p->mtMode)
    MatchFinderMt_ReleaseStream(&p->matchFinderMt);
  #else
  UNUSED_VAR(pp);
  #endif
}


typedef struct
{
  ISeqOutStream funcTable;
  Byte *data;
  SizeT rem;
  Bool_7z overflow;
} CSeqOutStreamBuf;

static size_t MyWrite(void *pp, const void *data, size_t size)
{
  CSeqOutStreamBuf *p = (CSeqOutStreamBuf *)pp;
  if (p->rem < size)
  {
    size = p->rem;
    p->overflow = true;
  }
  memcpy(p->data, data, size);
  p->rem -= size;
  p->data += size;
  return size;
}


UInt32_7z LzmaEnc_GetNumAvailableBytes(CLzmaEncHandle pp)
{
  const CLzmaEnc *p = (CLzmaEnc *)pp;
  return p->matchFinder.GetNumAvailableBytes(p->matchFinderObj);
}


const Byte *LzmaEnc_GetCurBuf(CLzmaEncHandle pp)
{
  const CLzmaEnc *p = (CLzmaEnc *)pp;
  return p->matchFinder.GetPointerToCurrentPos(p->matchFinderObj) - p->additionalOffset;
}


SRes LzmaEnc_CodeOneMemBlock(CLzmaEncHandle pp, Bool_7z reInit,
    Byte *dest, size_t *destLen, UInt32_7z desiredPackSize, UInt32_7z *unpackSize)
{
  CLzmaEnc *p = (CLzmaEnc *)pp;
  UInt64 nowPos64;
  SRes res;
  CSeqOutStreamBuf outStream;

  outStream.funcTable.Write = MyWrite;
  outStream.data = dest;
  outStream.rem = *destLen;
  outStream.overflow = false;

  p->writeEndMark = false;
  p->finished = false;
  p->result = SZ_OK;

  if (reInit)
    LzmaEnc_Init(p);
  LzmaEnc_InitPrices(p);
  nowPos64 = p->nowPos64;
  RangeEnc_Init(&p->rc);
  p->rc.outStream = &outStream.funcTable;

  res = LzmaEnc_CodeOneBlock(p, true, desiredPackSize, *unpackSize);
  
  *unpackSize = (UInt32_7z)(p->nowPos64 - nowPos64);
  *destLen -= outStream.rem;
  if (outStream.overflow)
    return SZ_ERROR_OUTPUT_EOF;

  return res;
}


static SRes LzmaEnc_Encode2(CLzmaEnc *p, ICompressProgress *progress)
{
  SRes res = SZ_OK;

  #ifndef _7ZIP_ST
  Byte allocaDummy[0x300];
  allocaDummy[0] = 0;
  allocaDummy[1] = allocaDummy[0];
  #endif

  for (;;)
  {
    res = LzmaEnc_CodeOneBlock(p, false, 0, 0);
    if (res != SZ_OK || p->finished)
      break;
    if (progress)
    {
      res = progress->Progress(progress, p->nowPos64, RangeEnc_GetProcessed(&p->rc));
      if (res != SZ_OK)
      {
        res = SZ_ERROR_PROGRESS;
        break;
      }
    }
  }
  
  LzmaEnc_Finish(p);

  /*
  if (res == S_OK && !Inline_MatchFinder_IsFinishedOK(&p->matchFinderBase))
    res = SZ_ERROR_FAIL;
  }
  */

  return res;
}


SRes LzmaEnc_Encode(CLzmaEncHandle pp, ISeqOutStream *outStream, ISeqInStream *inStream, ICompressProgress *progress,
    ISzAlloc *alloc, ISzAlloc *allocBig)
{
  RINOK(LzmaEnc_Prepare(pp, outStream, inStream, alloc, allocBig));
  return LzmaEnc_Encode2((CLzmaEnc *)pp, progress);
}


SRes LzmaEnc_WriteProperties(CLzmaEncHandle pp, Byte *props, SizeT *size)
{
  CLzmaEnc *p = (CLzmaEnc *)pp;
  unsigned i;
  UInt32_7z dictSize = p->dictSize;
  if (*size < LZMA_PROPS_SIZE)
    return SZ_ERROR_PARAM;
  *size = LZMA_PROPS_SIZE;
  props[0] = (Byte)((p->pb * 5 + p->lp) * 9 + p->lc);

  if (dictSize >= ((UInt32_7z)1 << 22))
  {
    UInt32_7z kDictMask = ((UInt32_7z)1 << 20) - 1;
    if (dictSize < (UInt32_7z)0xFFFFFFFF - kDictMask)
      dictSize = (dictSize + kDictMask) & ~kDictMask;
  }
  else for (i = 11; i <= 30; i++)
  {
    if (dictSize <= ((UInt32_7z)2 << i)) { dictSize = (2 << i); break; }
    if (dictSize <= ((UInt32_7z)3 << i)) { dictSize = (3 << i); break; }
  }

  for (i = 0; i < 4; i++)
    props[1 + i] = (Byte)(dictSize >> (8 * i));
  return SZ_OK;
}


SRes LzmaEnc_MemEncode(CLzmaEncHandle pp, Byte *dest, SizeT *destLen, const Byte *src, SizeT srcLen,
    int writeEndMark, ICompressProgress *progress, ISzAlloc *alloc, ISzAlloc *allocBig)
{
  SRes res;
  CLzmaEnc *p = (CLzmaEnc *)pp;

  CSeqOutStreamBuf outStream;

  outStream.funcTable.Write = MyWrite;
  outStream.data = dest;
  outStream.rem = *destLen;
  outStream.overflow = false;

  p->writeEndMark = writeEndMark;
  p->rc.outStream = &outStream.funcTable;

  res = LzmaEnc_MemPrepare(pp, src, srcLen, 0, alloc, allocBig);
  
  if (res == SZ_OK)
  {
    res = LzmaEnc_Encode2(p, progress);
    if (res == SZ_OK && p->nowPos64 != srcLen)
      res = SZ_ERROR_FAIL;
  }

  *destLen -= outStream.rem;
  if (outStream.overflow)
    return SZ_ERROR_OUTPUT_EOF;
  return res;
}


SRes LzmaEncode(Byte *dest, SizeT *destLen, const Byte *src, SizeT srcLen,
    const CLzmaEncProps *props, Byte *propsEncoded, SizeT *propsSize, int writeEndMark,
    ICompressProgress *progress, ISzAlloc *alloc, ISzAlloc *allocBig)
{
  CLzmaEnc *p = (CLzmaEnc *)LzmaEnc_Create(alloc);
  SRes res;
  if (!p)
    return SZ_ERROR_MEM;

  res = LzmaEnc_SetProps(p, props);
  if (res == SZ_OK)
  {
    res = LzmaEnc_WriteProperties(p, propsEncoded, propsSize);
    if (res == SZ_OK)
      res = LzmaEnc_MemEncode(p, dest, destLen, src, srcLen,
          writeEndMark, progress, alloc, allocBig);
  }

  LzmaEnc_Destroy(p, alloc, allocBig);
  return res;
}
