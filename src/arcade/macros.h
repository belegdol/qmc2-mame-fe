#ifndef MACROS_QMC2_ARCADE_H
#define MACROS_QMC2_ARCADE_H

#include <Qt>

// make a string out of a non-string constant
#define STR(s)					#s
#define XSTR(s)					STR(s)

// global OS macros for supported target operating systems
#if (defined(Q_OS_UNIX) || defined(Q_OS_LINUX)) && !defined(Q_OS_MAC)
#define QMC2_ARCADE_OS_UNIX
#if defined(Q_OS_LINUX)
#define QMC2_ARCADE_OS_LINUX
#define QMC2_ARCADE_OS_NAME			QString("Linux")
#else
#define QMC2_ARCADE_OS_NAME			QString("UNIX")
#endif
#elif defined(Q_OS_MAC)
#define QMC2_ARCADE_OS_MAC
#define QMC2_ARCADE_OS_NAME			QString("Darwin")
#elif defined(Q_OS_WIN32) || defined(Q_OS_WIN64)
#define QMC2_ARCADE_OS_WIN
#define QMC2_ARCADE_OS_NAME			QString("Windows")
#else
#error "Target OS is not supported -- QMC2 Arcade currently supports Linux/UNIX, Windows and Mac OS X!"
#endif

#include <QStringList>
#include <QString>
#include <QRegExp>
#include <QTime>
#include <QDir>
#include <stdio.h>

// current format version of the DAT-info database schema
#define QMC2_ARCADE_DATINFO_VERSION		2

// number of rows added to the DAT-info database in *one* transaction
#define QMC2_ARCADE_DATINFO_COMMIT		5000

// min/max of two constants
#define QMC2_ARCADE_MAX(a, b)			(((a) > (b)) ? (a) : (b))
#define QMC2_ARCADE_MIN(a, b)			(((a) < (b)) ? (a) : (b))

// application and ini related
#define QMC2_ARCADE_ORG_DOMAIN			QString("qmc2.batcom-it.net")
#define QMC2_ARCADE_ORG_NAME			QString("qmc2")
#define QMC2_ARCADE_APP_VERSION			QString(XSTR(QMC2_ARCADE_VERSION))
#define QMC2_ARCADE_APP_TITLE			QObject::tr("QMC2 Arcade")
#define QMC2_ARCADE_APP_NAME			QMC2_ARCADE_ORG_NAME

// relevant game list cache data columns
#define QMC2_ARCADE_MLC_ID			0
#define QMC2_ARCADE_MLC_DESCRIPTION		1
#define QMC2_ARCADE_MLC_PARENT			4
#define QMC2_ARCADE_MLC_BIOS			5
#define QMC2_ARCADE_MLC_DEVICE			10

// dot-path related
#if defined(Q_OS_MAC)
#define QMC2_ARCADE_DOT_PATH			(QDir::homePath() + "/Library/Application Support/qmc2")
#else
#define QMC2_ARCADE_DOT_PATH			(QDir::homePath() + "/.qmc2")
#endif
#define QMC2_ARCADE_DYN_DOT_PATH		(argumentList.indexOf("-config_path") >= 0 && argumentList.indexOf("-config_path") + 1 <= argumentList.count() ? argumentList.at(argumentList.indexOf("-config_path") + 1) : QMC2_ARCADE_DOT_PATH)

// ROM states
#define QMC2_ARCADE_ROMSTATE_C			0
#define QMC2_ARCADE_ROMSTATE_M			1
#define QMC2_ARCADE_ROMSTATE_I			2
#define QMC2_ARCADE_ROMSTATE_N			3
#define QMC2_ARCADE_ROMSTATE_U			4

// emulator modes
#define QMC2_ARCADE_EMUMODE_UNK			-1
#define QMC2_ARCADE_EMUMODE_MAME		0

// console modes
#define QMC2_ARCADE_CONSOLE_UNK			-1
#define QMC2_ARCADE_CONSOLE_TERM		0
#define QMC2_ARCADE_CONSOLE_WIN			1
#define QMC2_ARCADE_CONSOLE_WINMIN		2
#define QMC2_ARCADE_CONSOLE_NONE		3

// emulator option types
#define QMC2_ARCADE_EMUOPT_UNKNOWN		0
#define QMC2_ARCADE_EMUOPT_BOOL			1
#define QMC2_ARCADE_EMUOPT_INT			2
#define QMC2_ARCADE_EMUOPT_FLOAT		3
#define QMC2_ARCADE_EMUOPT_FLOAT1		QMC2_ARCADE_EMUOPT_FLOAT
#define QMC2_ARCADE_EMUOPT_STRING		4
#define QMC2_ARCADE_EMUOPT_FILE			5
#define QMC2_ARCADE_EMUOPT_FOLDER		6
#define QMC2_ARCADE_EMUOPT_COMBO		7
#define QMC2_ARCADE_EMUOPT_FLOAT2		8
#define QMC2_ARCADE_EMUOPT_FLOAT3		9

// ZIP read buffer size
#define QMC2_ARCADE_ZIP_BUFSIZE			65536

// number of image- & pixmap-cache slots
#define QMC2_ARCADE_IMGCACHE_SIZE		1000

// FPS counter update interval
#define QMC2_ARCADE_FPS_UPDATE_INTERVAL		1000

// indexed QML themes
#define QMC2_ARCADE_THEME_TOXICWASTE		0
#define QMC2_ARCADE_THEME_DARKONE		1

// indexed info classes
#define QMC2_ARCADE_INFO_CLASS_MACHINE		0
#define QMC2_ARCADE_INFO_CLASS_EMU		1
#define QMC2_ARCADE_INFO_CLASS_SOFT		2

// additional command line arguments
// -emu <emu> ([mame])
#define QMC2_ARCADE_CLI_EMU			(argumentList.indexOf("-emu") >= 0 && argumentList.count() > argumentList.indexOf("-emu") + 1 ? argumentList[argumentList.indexOf("-emu") + 1].toLower() : "mame")
#define QMC2_ARCADE_CLI_EMU_MAME		(argumentList.indexOf("-emu") >= 0 && argumentList.count() > argumentList.indexOf("-emu") + 1 ? argumentList[argumentList.indexOf("-emu") + 1].toLower() == "mame" : false)
#define QMC2_ARCADE_CLI_EMU_UNK			!QMC2_ARCADE_CLI_EMU_MAME
#define QMC2_ARCADE_CLI_EMU_INV			(argumentList.indexOf("-emu") >= 0 && argumentList.count() == argumentList.indexOf("-emu") + 1)
// -theme <theme> ([ToxicWaste], darkone)
#define QMC2_ARCADE_CLI_THEME			(argumentList.indexOf("-theme") >= 0 && argumentList.count() > argumentList.indexOf("-theme") + 1 ? argumentList[argumentList.indexOf("-theme") + 1] : "ToxicWaste")
#define QMC2_ARCADE_CLI_THEME_INV		(argumentList.indexOf("-theme") >= 0 && argumentList.count() == argumentList.indexOf("-theme") + 1)
#define QMC2_ARCADE_CLI_THEME_VAL		(argumentList.indexOf("-theme") >= 0 && argumentList.count() > argumentList.indexOf("-theme") + 1)
// -console <mode> ([terminal], window, window-minimized)
#define QMC2_ARCADE_CLI_CONS			(argumentList.indexOf("-console") >= 0 && argumentList.count() > argumentList.indexOf("-console") + 1 ? argumentList[argumentList.indexOf("-console") + 1] : "terminal")
#define QMC2_ARCADE_CLI_CONS_INV		(argumentList.indexOf("-console") >= 0 && argumentList.count() == argumentList.indexOf("-console") + 1)
#define QMC2_ARCADE_CLI_CONS_VAL		(argumentList.indexOf("-console") >= 0 && argumentList.count() > argumentList.indexOf("-console") + 1)
// -graphicssystem <gsys> ([raster], native, opengl)
#if QT_VERSION < 0x050000
#define QMC2_ARCADE_CLI_GSYS			(argumentList.indexOf("-graphicssystem") >= 0 && argumentList.count() > argumentList.indexOf("-graphicssystem") + 1 ? argumentList[argumentList.indexOf("-graphicssystem") + 1] : "raster")
#define QMC2_ARCADE_CLI_GSYS_INV		(argumentList.indexOf("-graphicssystem") >= 0 && argumentList.count() == argumentList.indexOf("-graphicssystem") + 1)
#define QMC2_ARCADE_CLI_GSYS_VAL		(argumentList.indexOf("-graphicssystem") >= 0 && argumentList.count() > argumentList.indexOf("-graphicssystem") + 1)
#else
#define QMC2_ARCADE_CLI_GSYS_INV		false
#endif
// -language <lang>
#define QMC2_ARCADE_CLI_LANG			(argumentList.indexOf("-language") >= 0 && argumentList.count() > argumentList.indexOf("-language") + 1 ? argumentList[argumentList.indexOf("-language") + 1] : "us")
#define QMC2_ARCADE_CLI_LANG_INV		(argumentList.indexOf("-language") >= 0 && argumentList.count() == argumentList.indexOf("-language") + 1)
#define QMC2_ARCADE_CLI_LANG_VAL		(argumentList.indexOf("-language") >= 0 && argumentList.count() > argumentList.indexOf("-language") + 1)
// -video
#define QMC2_ARCADE_CLI_VIDEO			(argumentList.indexOf("-video") >= 0 && argumentList.count() > argumentList.indexOf("-video") + 1 ? argumentList[argumentList.indexOf("-video") + 1] : "off")
#define QMC2_ARCADE_CLI_VIDEO_INV		(argumentList.indexOf("-video") >= 0 && argumentList.count() == argumentList.indexOf("-video") + 1)
#define QMC2_ARCADE_CLI_VIDEO_VAL		(argumentList.indexOf("-video") >= 0 && argumentList.count() > argumentList.indexOf("-video") + 1)
// -fullscreen
#define QMC2_ARCADE_CLI_FULLSCREEN		(argumentList.indexOf("-fullscreen") >= 0)
// -windowed
#define QMC2_ARCADE_CLI_WINDOWED		(argumentList.indexOf("-windowed") >= 0)
// -debugkeys
#define QMC2_ARCADE_CLI_DEBUG_KEYS		(argumentList.indexOf("-debugkeys") >= 0)
// -joy <index>
#define QMC2_ARCADE_CLI_JOY			(argumentList.indexOf("-joy") >= 0 && argumentList.count() > argumentList.indexOf("-joy") + 1 ? argumentList[argumentList.indexOf("-joy") + 1].toInt() : 0)
#define QMC2_ARCADE_CLI_JOY_INV			(argumentList.indexOf("-joy") >= 0 && argumentList.count() == argumentList.indexOf("-joy") + 1)
#define QMC2_ARCADE_CLI_JOY_VAL			(argumentList.indexOf("-joy") >= 0 && argumentList.count() > argumentList.indexOf("-joy") + 1)
// -nojoy
#define QMC2_ARCADE_CLI_NO_JOY			(argumentList.indexOf("-nojoy") >= 0)
// -debugjoy
#define QMC2_ARCADE_CLI_DEBUG_JOY		(argumentList.indexOf("-debugjoy") >= 0)
// -debugqt
#define QMC2_ARCADE_CLI_DEBUG_QT		(argumentList.indexOf("-debugqt") >= 0)
// -h|-?|-help
#define QMC2_ARCADE_CLI_HELP			(argumentList.indexOf(QRegExp("(-h|-\\?|-help)")) >= 0)
// argument validation
#define QMC2_ARCADE_CLI_INVALID			(QMC2_ARCADE_CLI_EMU_INV || QMC2_ARCADE_CLI_THEME_INV || QMC2_ARCADE_CLI_CONS_INV || QMC2_ARCADE_CLI_GSYS_INV || QMC2_ARCADE_CLI_LANG_INV || QMC2_ARCADE_CLI_VIDEO_INV || QMC2_ARCADE_CLI_JOY_INV)

// console logging macros
#define QMC2_ARCADE_LOG_STR(s)			TweakedQmlApplicationViewer::logString(s)
#define QMC2_ARCADE_LOG_STR_NT(s)		TweakedQmlApplicationViewer::logStringNoTime(s)
#define QMC2_ARCADE_LOG_CSTR(s)			TweakedQmlApplicationViewer::logCString(s)
#define QMC2_ARCADE_LOG_CSTR_NT(s)		TweakedQmlApplicationViewer::logCStringNoTime(s)

// exchangable (de)compression routines
#define QMC2_ARCADE_COMPRESS(data)		qCompress((data))
#define QMC2_ARCADE_UNCOMPRESS(data)		(data).isEmpty() ? QByteArray() : qUncompress(data)

// numeric image types
#define QMC2_ARCADE_IMGTYPE_NONE		-1
#define QMC2_ARCADE_IMGTYPE_PREVIEW		0
#define QMC2_ARCADE_IMGTYPE_FLYER		1
#define QMC2_ARCADE_IMGTYPE_CABINET		2
#define QMC2_ARCADE_IMGTYPE_CONTROLLER		3
#define QMC2_ARCADE_IMGTYPE_MARQUEE		4
#define QMC2_ARCADE_IMGTYPE_LOGO		QMC2_ARCADE_IMGTYPE_MARQUEE
#define QMC2_ARCADE_IMGTYPE_TITLE		5
#define QMC2_ARCADE_IMGTYPE_PCB			6
#define QMC2_ARCADE_IMGTYPE_SWSNAP		7
#define QMC2_ARCADE_IMGTYPE_ICON		8

// indexes in compressed image file type selectors
#define QMC2_ARCADE_IMG_FILETYPE_ZIP		0
#define QMC2_ARCADE_IMG_FILETYPE_7Z		1
#define QMC2_ARCADE_IMG_FILETYPE_ARCHIVE	2

// custom artwork related
#define QMC2_ARCADE_AW_TARGET_SYSTEM		0
#define QMC2_ARCADE_AW_TARGET_SOFTWARE		1
#define QMC2_ARCADE_AW_FORMAT_ZIP		0
#define QMC2_ARCADE_AW_FORMAT_7Z		1
#define QMC2_ARCADE_AW_FORMAT_ARCHIVE		2

// responsiveness when loading data from files (game/machine info DBs, ROM state cache and game/machine list caches)
#define QMC2_ARCADE_LOAD_RESPONSE		500

#define QMC2_ARCADE_TEST_VERSION(v, mv, r, mr)	((v) < mv || ((r) > 0 && (r) < (mr)))

#endif
