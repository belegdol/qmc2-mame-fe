VERSION = 0.244
MAIN_UI_VERSION = 0.244

# Add more folders to ship with the application, here
folder_01.source = qml/ToxicWaste
folder_01.target = qml
folder_02.source = qml/darkone
folder_02.target = qml
DEPLOYMENTFOLDERS = folder_01 folder_02

QML_IMPORT_PATH =

SOURCES += main.cpp \
    tweakedqmlappviewer.cpp \
    imageprovider.cpp \
    infoprovider.cpp \
    arcadesettings.cpp \
    machineobject.cpp \
    consolewindow.cpp \
    processmanager.cpp \
    joystick.cpp \
    pointer.cpp \
    keyeventfilter.cpp \
    keysequencemap.cpp \
    joyfunctionmap.cpp \
    joystickmanager.cpp \
    datinfodbmgr.cpp \
    ../settings.cpp \
    ../sevenzipfile.cpp \
    ../bigbytearray.cpp \
    ../lzma/7zAlloc.c \
    ../lzma/7zBuf2.c \
    ../lzma/7zBuf.c \
    ../lzma/7zCrc.c \
    ../lzma/7zCrcOpt.c \
    ../lzma/7zDec.c \
    ../lzma/7zFile.c \
    ../lzma/7zArcIn.c \
    ../lzma/7zStream.c \
    ../lzma/Alloc.c \
    ../lzma/Bcj2.c \
    ../lzma/Bra86.c \
    ../lzma/Bra.c \
    ../lzma/BraIA64.c \
    ../lzma/CpuArch.c \
    ../lzma/Delta.c \
    ../lzma/LzFind.c \
    ../lzma/Lzma2Dec.c \
    ../lzma/Lzma2Enc.c \
    ../lzma/Lzma86Dec.c \
    ../lzma/Lzma86Enc.c \
    ../lzma/LzmaDec.c \
    ../lzma/LzmaEnc.c \
    ../lzma/LzmaLib.c \
    ../lzma/Ppmd7.c \
    ../lzma/Ppmd7Dec.c \
    ../lzma/Ppmd7Enc.c \
    ../lzma/Sha256.c \
    ../iconcachedbmgr.cpp

HEADERS += \
    tweakedqmlappviewer.h \
    imageprovider.h \
    infoprovider.h \
    arcadesettings.h \
    macros.h \
    machineobject.h \
    consolewindow.h \
    processmanager.h \
    emulatoroption.h \
    joystick.h \
    pointer.h \
    keyeventfilter.h \
    keysequences.h \
    keysequencemap.h \
    joyfunctionmap.h \
    joystickmanager.h \
    datinfodbmgr.h \
    ../settings.h \
    ../sevenzipfile.h \
    ../bigbytearray.h \
    ../iconcachedbmgr.h

INCLUDEPATH += ../lzma

DEFINES += QMC2_ARCADE

contains(DEFINES, QMC2_ARCADE_LIBARCHIVE_ENABLED) {
    SOURCES += ../archivefile.cpp
    HEADERS += ../archivefile.h
    LIBS += -larchive
}

contains(DEFINES, QMC2_ARCADE_BUNDLED_MINIZIP) {
	INCLUDEPATH += ../minizip
	SOURCES += ../minizip/mz_compat.c \
		../minizip/mz_crypt.c \
		../minizip/mz_os.c \
		../minizip/mz_strm.c \
		../minizip/mz_strm_mem.c \
		../minizip/mz_strm_zlib.c \
		../minizip/mz_zip.c
	!win32 {
		SOURCES += ../minizip/mz_os_posix.c \
			../minizip/mz_strm_os_posix.c
	} else {
		SOURCES += ../minizip/mz_os_win32.c \
			../minizip/mz_strm_os_win32.c
	}
	DEFINES += HAVE_ZLIB ZLIB_COMPAT
} else {
	CONFIG += link_pkgconfig
	PKGCONFIG += minizip
}

contains(DEFINES, QMC2_ARCADE_BUNDLED_ZLIB) {
    INCLUDEPATH += ../zlib
    SOURCES += ../zlib/zutil.c \
	       ../zlib/uncompr.c \
	       ../zlib/trees.c \
	       ../zlib/inftrees.c \
	       ../zlib/inflate.c \
	       ../zlib/inffast.c \
	       ../zlib/infback.c \
	       ../zlib/gzwrite.c \
	       ../zlib/gzread.c \
	       ../zlib/gzlib.c \
	       ../zlib/gzclose.c \
	       ../zlib/deflate.c \
	       ../zlib/crc32.c \
	       ../zlib/compress.c \
	       ../zlib/adler32.c
} else {
    CONFIG += link_pkgconfig
    PKGCONFIG += zlib
}

DEFINES += _7ZIP_PPMD_SUPPORT _7ZIP_ST QMC2_ARCADE_VERSION=$$VERSION QMC2_ARCADE_MAIN_UI_VERSION=$$MAIN_UI_VERSION

RESOURCES += qmc2-arcade-common.qrc
greaterThan(QT_MAJOR_VERSION, 4) {
    RESOURCES += qmc2-arcade-2-0.qrc
} else {
    RESOURCES += qmc2-arcade-1-1.qrc
}

evil_hack_to_fool_lupdate {
    SOURCES += qml/ToxicWaste/1.1/ToxicWaste.qml \
	       qml/ToxicWaste/1.1/ToxicWaste-video.qml \
	       qml/ToxicWaste/1.1/ToxicWaste.js \
	       qml/ToxicWaste/1.1/animations/BackgroundAnimation.qml \
	       qml/ToxicWaste/2.0/ToxicWaste.qml \
	       qml/ToxicWaste/2.0/ToxicWaste-video.qml \
	       qml/ToxicWaste/2.0/ToxicWaste.js \
	       qml/ToxicWaste/2.0/animations/BackgroundAnimation.qml \
	       qml/darkone/1.1/darkone.qml \
	       qml/darkone/1.1/darkone-video.qml \
	       qml/darkone/1.1/darkone.js \
	       qml/darkone/2.0/darkone.qml \
	       qml/darkone/2.0/darkone-video.qml \
	       qml/darkone/2.0/darkone.js
}

TRANSLATIONS += translations/qmc2-arcade_de.ts \
    translations/qmc2-arcade_es.ts \
    translations/qmc2-arcade_el.ts \
    translations/qmc2-arcade_it.ts \
    translations/qmc2-arcade_fr.ts \
    translations/qmc2-arcade_pl.ts \
    translations/qmc2-arcade_pt.ts \
    translations/qmc2-arcade_ro.ts \
    translations/qmc2-arcade_sv.ts \
    translations/qmc2-arcade_us.ts

!equals(GIT_REV, ) {
    DEFINES += QMC2_ARCADE_GIT_REV=$$GIT_REV
}

isEmpty(QMC2_ARCADE_JOYSTICK): QMC2_ARCADE_JOYSTICK = 1
greaterThan(QMC2_ARCADE_JOYSTICK, 0) {
    DEFINES += QMC2_ARCADE_ENABLE_JOYSTICK
}

isEmpty(QMC2_ARCADE_QML_IMPORT_PATH): QMC2_ARCADE_QML_IMPORT_PATH = imports
DEFINES += QMC2_ARCADE_QML_IMPORT_PATH=$$QMC2_ARCADE_QML_IMPORT_PATH

QT += core gui svg sql opengl

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += quick qml testlib
} else {
    QT += declarative
    CONFIG += qtestlib
}

macx {
    greaterThan(SDL, 1) {
	LIBS += -framework SDL2 -framework Cocoa -F/Library/Frameworks
	INCLUDEPATH += /Library/Frameworks/SDL2.framework/Headers
    } else {
	OBJECTIVE_SOURCES += ../SDLMain_tmpl.m
	HEADERS += ../SDLMain_tmpl.h
	LIBS += -framework SDL -framework Cocoa -F/Library/Frameworks
	INCLUDEPATH += /Library/Frameworks/SDL.framework/Headers
    }
    ICON = images/qmc2-arcade.icns
    contains(DEFINES, QMC2_ARCADE_MAC_UNIVERSAL): CONFIG += x86_64 arm64
    QMAKE_INFO_PLIST = Info.plist
} else {
    !win32 {
	greaterThan(SDL, 0) {
	    LIBS += $$system("../../scripts/sdl-libs.sh $$SDL")
	    INCLUDEPATH += $$system("../../scripts/sdl-includepath.sh $$SDL")
	} else {
	    LIBS += $$system("../../scripts/sdl-libs.sh")
	    INCLUDEPATH += $$system("../../scripts/sdl-includepath.sh")
	}
    } else {
	DEFINES += PSAPI_VERSION=1
	SOURCES += ../windows_tools.cpp
	contains(DEFINES, QMC2_ARCADE_MINGW) {
	    CONFIG += windows
	    greaterThan(SDL, 1) {
		LIBS += -lSDL2.dll -lSDL2 -lole32 -lpsapi
	    } else {
		LIBS += -lSDLmain -lSDL.dll -lSDL -lole32 -lpsapi
	    }
	    INCLUDEPATH += $$QMC2_ARCADE_INCLUDEPATH
	    QMAKE_CXXFLAGS += -Wl,-subsystem,windows
	    QMAKE_CFLAGS += -Wl,-subsystem,windows
	    QMAKE_LFLAGS += -Wl,-subsystem,windows
	} else {
	    CONFIG += embed_manifest_exe windows
	    LIBS += psapi.lib ole32.lib
	}
	RC_FILE = qmc2-arcade.rc
    }
}
