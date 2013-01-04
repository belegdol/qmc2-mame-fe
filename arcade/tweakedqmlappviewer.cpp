#include <QGraphicsObject>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QApplication>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QPaintEngine>

#include "tweakedqmlappviewer.h"
#include "imageprovider.h"
#include "arcadesettings.h"
#include "gameobject.h"
#include "consolewindow.h"
#include "macros.h"

extern ArcadeSettings *globalConfig;
extern ConsoleWindow *consoleWindow;
extern int emulatorMode;
extern int consoleMode;
extern QStringList emulatorModeNames;

TweakedQmlApplicationViewer::TweakedQmlApplicationViewer(QWidget *parent)
	: QmlApplicationViewer(parent)
{
    numFrames = 0;
    windowModeSwitching = false;

    processManager = new ProcessManager(this);
    processManager->createTemplateList();

    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    engine()->addImageProvider(QLatin1String("qmc2"), new ImageProvider(QDeclarativeImageProvider::Image));
    engine()->addImportPath(QDir::fromNativeSeparators(XSTR(QMC2_ARCADE_QML_IMPORT_PATH)));
    rootContext()->setContextProperty("viewer", this);

    loadGamelist();

    connect(&frameCheckTimer, SIGNAL(timeout()), this, SLOT(fpsReady()));
    frameCheckTimer.start(1000);
}

TweakedQmlApplicationViewer::~TweakedQmlApplicationViewer()
{
    saveSettings();
}

void TweakedQmlApplicationViewer::fpsReady()
{
    if ( rootObject() )
        rootObject()->setProperty("fps", numFrames);
    numFrames = 0;
}

void TweakedQmlApplicationViewer::loadSettings()
{
    QMC2_ARCADE_LOG_STR(tr("Loading global and theme-specific settings"));

    // load global arcade settings
    rootObject()->setProperty("version", globalConfig->applicationVersion());

    // load theme-specific arcade settings
    if ( globalConfig->arcadeTheme == "ToxicWaste" ) {
        rootObject()->setProperty("fpsVisible", globalConfig->fpsVisible());
        rootObject()->setProperty("showBackgroundAnimation", globalConfig->showBackgroundAnimation());
        rootObject()->setProperty("showShaderEffect", globalConfig->showShaderEffect());
        rootObject()->setProperty("animateInForeground", globalConfig->animateInForeground());
        rootObject()->setProperty("fullScreen", globalConfig->fullScreen());
        rootObject()->setProperty("secondaryImageType", globalConfig->secondaryImageType());
        rootObject()->setProperty("cabinetFlipped", globalConfig->cabinetFlipped());
        rootObject()->setProperty("lastIndex", globalConfig->lastIndex() < gameList.count() ? globalConfig->lastIndex() : 0);
        rootObject()->setProperty("menuHidden", globalConfig->menuHidden());
    }

    QMC2_ARCADE_LOG_STR(tr("Ready to launch %1").arg(emulatorMode != QMC2_ARCADE_EMUMODE_MESS ? tr("games") : tr("machines")));
}

void TweakedQmlApplicationViewer::saveSettings()
{
    QMC2_ARCADE_LOG_STR(tr("Saving global and theme-specific settings"));

    // save global arcade settings
    if ( isFullScreen() ) {
        globalConfig->setViewerGeometry(savedGeometry);
        globalConfig->setViewerMaximized(savedMaximized);
    } else {
        globalConfig->setViewerGeometry(saveGeometry());
        globalConfig->setViewerMaximized(isMaximized());
    }

    // save theme-specific arcade settings
    if ( globalConfig->arcadeTheme == "ToxicWaste" ) {
        globalConfig->setFpsVisible(rootObject()->property("fpsVisible").toBool());
        globalConfig->setShowBackgroundAnimation(rootObject()->property("showBackgroundAnimation").toBool());
        globalConfig->setShowShaderEffect(rootObject()->property("showShaderEffect").toBool());
        globalConfig->setAnimateInForeground(rootObject()->property("animateInForeground").toBool());
        globalConfig->setFullScreen(rootObject()->property("fullScreen").toBool());
        globalConfig->setSecondaryImageType(rootObject()->property("secondaryImageType").toString());
        globalConfig->setCabinetFlipped(rootObject()->property("cabinetFlipped").toBool());
        globalConfig->setLastIndex(rootObject()->property("lastIndex").toInt());
        globalConfig->setMenuHidden(rootObject()->property("menuHidden").toBool());
    }
}

void TweakedQmlApplicationViewer::goFullScreen()
{
    showFullScreen();
    raise();
    qApp->processEvents();
    windowModeSwitching = false;
}

void TweakedQmlApplicationViewer::switchToFullScreen(bool initially)
{
    if ( windowModeSwitching )
        return;
    windowModeSwitching = true;
    QMC2_ARCADE_LOG_STR(tr("Activating full-screen display"));
    if ( initially ) {
        savedGeometry = globalConfig->viewerGeometry();
        savedMaximized = globalConfig->viewerMaximized();
    } else {
        savedGeometry = saveGeometry();
        savedMaximized = isMaximized();
    }
#if defined(QMC2_ARCADE_OS_UNIX)
    hide();
    qApp->processEvents();
    QTimer::singleShot(100, this, SLOT(goFullScreen()));
#else
    showFullScreen();
    windowModeSwitching = false;
#endif
}

void TweakedQmlApplicationViewer::switchToWindowed(bool initially)
{
    if ( windowModeSwitching )
        return;
    windowModeSwitching = true;
    QMC2_ARCADE_LOG_STR(tr("Activating windowed display"));
    if ( initially ) {
        savedGeometry = globalConfig->viewerGeometry();
        savedMaximized = globalConfig->viewerMaximized();
    }
#if defined(QMC2_ARCADE_OS_UNIX)
    hide();
#endif
    restoreGeometry(savedGeometry);
    if ( savedMaximized )
        showMaximized();
    else
        showNormal();
    raise();
    windowModeSwitching = false;
}

QString TweakedQmlApplicationViewer::romStateText(int status)
{
    switch ( status ) {
    case QMC2_ARCADE_ROMSTATE_C:
        return tr("correct");
    case QMC2_ARCADE_ROMSTATE_M:
        return tr("mostly correct");
    case QMC2_ARCADE_ROMSTATE_I:
        return tr("incorrect");
    case QMC2_ARCADE_ROMSTATE_N:
        return tr("not found");
    case QMC2_ARCADE_ROMSTATE_U:
    default:
        return tr("unknown");
    }
}

int TweakedQmlApplicationViewer::romStateCharToInt(char status)
{
    switch ( status ) {
    case 'C':
        return QMC2_ARCADE_ROMSTATE_C;
    case 'M':
        return QMC2_ARCADE_ROMSTATE_M;
    case 'I':
        return QMC2_ARCADE_ROMSTATE_I;
    case 'N':
        return QMC2_ARCADE_ROMSTATE_N;
    case 'U':
    default:
        return QMC2_ARCADE_ROMSTATE_U;
    }
}

void TweakedQmlApplicationViewer::loadGamelist()
{
    QString gameListCachePath;
    bool listAlreadySorted = false;

    if ( globalConfig->useFilteredList() ) {
        gameListCachePath = QFileInfo(globalConfig->filteredListFile()).absoluteFilePath();
        if ( !QFileInfo(gameListCachePath).exists() || !QFileInfo(gameListCachePath).isReadable() ) {
            QMC2_ARCADE_LOG_STR(tr("WARNING: filtered list file '%1' doesn't exist or isn't accessible, falling back to the full %2").
                                arg(gameListCachePath).
                                arg(emulatorMode != QMC2_ARCADE_EMUMODE_MESS ? tr("game list") : tr("machine list")));
            gameListCachePath = QFileInfo(globalConfig->gameListCacheFile()).absoluteFilePath();
        } else
            listAlreadySorted = true;
    } else
        gameListCachePath = QFileInfo(globalConfig->gameListCacheFile()).absoluteFilePath();

    QMap<QString, char> rscMap;

    QMC2_ARCADE_LOG_STR(tr("Loading %1 from '%2'").
                        arg(emulatorMode != QMC2_ARCADE_EMUMODE_MESS ? tr("game list") : tr("machine list")).
                        arg(QDir::toNativeSeparators(gameListCachePath)));

    QString romStateCachePath = QFileInfo(globalConfig->romStateCacheFile()).absoluteFilePath();
    QFile romStateCache(romStateCachePath);
    if ( romStateCache.exists() ) {
        if ( romStateCache.open(QIODevice::ReadOnly | QIODevice::Text) ) {
            QTextStream tsRomCache(&romStateCache);
            while ( !tsRomCache.atEnd() ) {
                QString line = tsRomCache.readLine();
                if ( !line.isEmpty() && !line.startsWith("#") ) {
                    QStringList words = line.split(" ");
                    rscMap[words[0]] = words[1].at(0).toLatin1();
                }
            }
        } else
            QMC2_ARCADE_LOG_STR(tr("WARNING: Can't open ROM state cache file '%1', please check permissions").
                         arg(QDir::toNativeSeparators(romStateCachePath)));
    } else
        QMC2_ARCADE_LOG_STR(tr("WARNING: The ROM state cache file '%1' doesn't exist, please run main front-end executable to create it").
                     arg(QDir::toNativeSeparators(romStateCachePath)));

    QFile gameListCache(gameListCachePath);
    if ( gameListCache.exists() ) {
        if ( gameListCache.open(QIODevice::ReadOnly | QIODevice::Text) ) {
            QTextStream tsGameListCache(&gameListCache);
            tsGameListCache.readLine();
            tsGameListCache.readLine();
            while ( !tsGameListCache.atEnd() ) {
                QStringList words = tsGameListCache.readLine().split("\t");
                if ( words[QMC2_ARCADE_GLC_DEVICE] != "1" ) {
                    QString gameId = words[QMC2_ARCADE_GLC_ID];
                    gameList.append(new GameObject(gameId, words[QMC2_ARCADE_GLC_DESCRIPTION], romStateCharToInt(rscMap[gameId])));
                }
            }
        } else
            QMC2_ARCADE_LOG_STR(tr("FATAL: Can't open %1 cache file '%2', please check permissions").
                         arg(emulatorMode != QMC2_ARCADE_EMUMODE_MESS ? tr("game list") : tr("machine list")).
                         arg(QDir::toNativeSeparators(gameListCachePath)));
    } else
        QMC2_ARCADE_LOG_STR(tr("FATAL: The %1 cache file '%2' doesn't exist, please run main front-end executable to create it").
                     arg(emulatorMode != QMC2_ARCADE_EMUMODE_MESS ? tr("game list") : tr("machine list")).
                     arg(QDir::toNativeSeparators(gameListCachePath)));

    if ( !listAlreadySorted )
        qSort(gameList.begin(), gameList.end(), GameObject::lessThan);

    // propagate gameList to QML
    rootContext()->setContextProperty("gameListModel", QVariant::fromValue(gameList));
    rootContext()->setContextProperty("gameListModelCount", gameList.count());

    QMC2_ARCADE_LOG_STR(QString(tr("Done (loading %1 from '%2')").
                         arg(emulatorMode != QMC2_ARCADE_EMUMODE_MESS ? tr("game list") : tr("machine list")) + " - " + tr("%n non-device set(s) loaded", "", gameList.count())).
                         arg(QDir::toNativeSeparators(gameListCachePath)));
}

void TweakedQmlApplicationViewer::launchEmulator(QString id)
{
    QMC2_ARCADE_LOG_STR(tr("Starting emulator #%1 for %2 ID '%3'").arg(processManager->highestProcessID()).arg(emulatorMode != QMC2_ARCADE_EMUMODE_MESS ? tr("game") : tr("machine")).arg(id));
    processManager->startEmulator(id);
}

int TweakedQmlApplicationViewer::findIndex(QString pattern, int startIndex)
{
    if ( pattern.isEmpty() )
        return startIndex;

    int foundIndex = startIndex;
    bool indexFound = false;

    QRegExp patternRegExp(pattern, Qt::CaseInsensitive, QRegExp::Wildcard);

    for (int i = startIndex + 1; i < gameList.count() && !indexFound; i++) {
        QString description = ((GameObject *)gameList[i])->description();
        QString id = ((GameObject *)gameList[i])->id();
        if ( description.indexOf(patternRegExp, 0) >= 0 || id.indexOf(patternRegExp, 0) >= 0 ) {
            foundIndex = i;
            indexFound = true;
        }
    }

    for (int i = 0; i < startIndex && !indexFound; i++) {
        QString description = ((GameObject *)gameList[i])->description();
        QString id = ((GameObject *)gameList[i])->id();
        if ( description.indexOf(patternRegExp, 0) >= 0 || id.indexOf(patternRegExp, 0) >= 0 ) {
            foundIndex = i;
            indexFound = true;
        }
    }

    return foundIndex;
}

void TweakedQmlApplicationViewer::log(QString message)
{
    QMC2_ARCADE_LOG_STR(message);
}

void TweakedQmlApplicationViewer::paintEvent(QPaintEvent *e)
{
    numFrames++;
    QmlApplicationViewer::paintEvent(e);
}

void TweakedQmlApplicationViewer::closeEvent(QCloseEvent *e)
{
    QMC2_ARCADE_LOG_STR(tr("Stopping QML viewer"));

    if ( consoleWindow ) {
        QString consoleMessage(tr("QML viewer stopped - please close the console window to exit"));
        QMC2_ARCADE_LOG_STR(QString("-").repeated(consoleMessage.length()));
        QMC2_ARCADE_LOG_STR(consoleMessage);
        QMC2_ARCADE_LOG_STR(QString("-").repeated(consoleMessage.length()));
        consoleWindow->showNormal();
        consoleWindow->raise();
    }
    e->accept();
}
