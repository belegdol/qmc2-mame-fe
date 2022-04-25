VERSION = 0.243

QT += core gui script scripttools
TARGET = qchdman
TEMPLATE = app

greaterThan(DEBUG, 0) | contains(DEFINES, "QCHDMAN_DEBUG") {
    !contains(DEFINES, "QCHDMAN_DEBUG"): DEFINES += QCHDMAN_DEBUG
    !contains(CONFIG, "warn_on debug"): CONFIG += warn_on debug
} else {
    !contains(DEFINES, "QCHDMAN_RELEASE"): DEFINES += QCHDMAN_RELEASE
    !contains(CONFIG, "warn_off release"): CONFIG += warn_off release
}

!equals(GIT_REV, ) {
    DEFINES += QCHDMAN_GIT_REV=$$GIT_REV
}

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

macx {
    QMAKE_INFO_PLIST = Info.plist
    contains(DEFINES, QCHDMAN_MAC_UNIVERSAL): CONFIG += x86 ppc
}

win32 {
    RC_FILE = qchdman.rc
}

DEFINES += QCHDMAN_VERSION=$$VERSION

SOURCES += main.cpp\
    mainwindow.cpp \
    projectwindow.cpp \
    projectwidget.cpp \
    preferencesdialog.cpp \
    aboutdialog.cpp \
    scriptwidget.cpp \
    scriptengine.cpp \
    ecmascripthighlighter.cpp \
    scripteditor.cpp \
    ../../settings.cpp

HEADERS  += mainwindow.h \
    macros.h \
    projectwindow.h \
    projectwidget.h \
    preferencesdialog.h \
    aboutdialog.h \
    scriptwidget.h \
    scriptengine.h \
    ecmascripthighlighter.h \
    scripteditor.h \
    qchdmansettings.h \
    ../../settings.h

FORMS += mainwindow.ui \
    projectwidget.ui \
    preferencesdialog.ui \
    aboutdialog.ui \
    scriptwidget.ui

RESOURCES += qchdman.qrc

TRANSLATIONS += translations/qchdman_de.ts \
    translations/qchdman_es.ts \
    translations/qchdman_el.ts \
    translations/qchdman_it.ts \
    translations/qchdman_fr.ts \
    translations/qchdman_pl.ts \
    translations/qchdman_pt.ts \
    translations/qchdman_ro.ts \
    translations/qchdman_sv.ts \
    translations/qchdman_us.ts
