#include <qglobal.h>

#if QT_VERSION < 0x050000
#include <QApplication>
#include <QGraphicsObject>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#else
#include <QMetaType>
#include <QGuiApplication>
#include <QtQml>
#endif
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QPaintEngine>
#include <QDesktopServices>
#include <QUrl>
#include <QHash>
#include <QMap>

#include <algorithm> // std::sort()

#include "tweakedqmlappviewer.h"
#include "arcadesettings.h"
#include "machineobject.h"
#include "consolewindow.h"
#include "macros.h"
#if QT_VERSION < 0x050000
#include "wheel.h"
#endif
#include "pointer.h"
#include "keysequences.h"

extern ArcadeSettings *globalConfig;
extern ConsoleWindow *consoleWindow;
extern int emulatorMode;
extern QStringList emulatorModeNames;
extern QStringList mameThemes;
extern QStringList arcadeThemes;
extern QStringList consoleModes;
#if QT_VERSION < 0x050000
extern QStringList graphicsSystems;
#endif

int TweakedQmlApplicationViewer::consoleMode = QMC2_ARCADE_CONSOLE_TERM;

#if QT_VERSION < 0x050000
TweakedQmlApplicationViewer::TweakedQmlApplicationViewer(QWidget *parent)
	: QmlApplicationViewer(parent)
	#else
TweakedQmlApplicationViewer::TweakedQmlApplicationViewer(QWindow *parent)
	: QQuickView(parent)
	#endif
{
	m_initialized = m_initialFullScreen = m_videoEnabled = windowModeSwitching = false;
	m_currentSystemArtworkIndex = m_currentSoftwareArtworkIndex = -2;
	numFrames = 0;

	QStringList keySequences;
	QMC2_ARCADE_ADD_COMMON_KEYSEQUENCES(keySequences);
	switch ( themeIndex() ) {
	case QMC2_ARCADE_THEME_TOXICWASTE:
		QMC2_ARCADE_ADD_TOXIXCWASTE_KEYSEQUENCES(keySequences);
		break;
	case QMC2_ARCADE_THEME_DARKONE:
		QMC2_ARCADE_ADD_DARKONE_KEYSEQUENCES(keySequences);
		break;
	}
	keySequenceMap = new KeySequenceMap(keySequences);
#if defined(QMC2_ARCADE_ENABLE_JOYSTICK)
	joyFunctionMap = new JoyFunctionMap(keySequences);
	joystickManager = new JoystickManager(joyFunctionMap);
#endif

	infoClasses << "sysinfo" << "emuinfo" << "softinfo";
	videoSnapAllowedFormatExtensions << ".mp4" << ".avi";

#if QT_VERSION < 0x050000
	cliParams << "theme" << "graphicssystem" << "console" << "language" << "video";
#else
	cliParams << "theme" << "console" << "language" << "video";
#endif
	switch ( emulatorMode ) {
	case QMC2_ARCADE_EMUMODE_MAME:
	default:
		cliAllowedParameterValues["theme"] = mameThemes;
		break;
	}
#if QT_VERSION < 0x050000
	cliAllowedParameterValues["graphicssystem"] = graphicsSystems;
#endif
	cliAllowedParameterValues["console"] = consoleModes;
	cliAllowedParameterValues["language"] = globalConfig->languageMap.keys();
	cliAllowedParameterValues["video"] = QStringList() << "on" << "off";
	cliParameterDescriptions["theme"] = tr("Theme");
#if QT_VERSION < 0x050000
	cliParameterDescriptions["graphicssystem"] = tr("Graphics system");
#endif
	cliParameterDescriptions["console"] = tr("Console mode");
	cliParameterDescriptions["language"] = tr("Language");
	cliParameterDescriptions["video"] = tr("Video snaps");

#if QT_VERSION < 0x050000
	qmlRegisterType<WheelArea>("Wheel", 1, 0, "WheelArea");
#endif
	qmlRegisterType<CursorShapeArea>("Pointer", 1, 0, "CursorShapeArea");

	processManager = new ProcessManager(this);
	processManager->createTemplateList();
	connect(processManager, SIGNAL(emulatorStarted(int)), this, SIGNAL(emulatorStarted(int)));
	connect(processManager, SIGNAL(emulatorFinished(int)), this, SIGNAL(emulatorFinished(int)));

#if QT_VERSION < 0x050000
	setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	setResizeMode(QDeclarativeView::SizeRootObjectToView);
	imageProvider = new ImageProvider(QDeclarativeImageProvider::Image);
#else
	setResizeMode(QQuickView::SizeRootObjectToView);
	imageProvider = new ImageProvider(QQuickImageProvider::Image);
#endif

	connect(imageProvider, SIGNAL(imageDataUpdated(const QString &)), this, SLOT(imageDataUpdate(const QString &)), Qt::DirectConnection);
	engine()->addImageProvider(QString("qmc2"), imageProvider);

	infoProvider = new InfoProvider();

	engine()->addImportPath(QDir::fromNativeSeparators(XSTR(QMC2_ARCADE_QML_IMPORT_PATH)));
	rootContext()->setContextProperty("viewer", this);

	// theme-specific initialization
	switch ( themeIndex() ) {
	case QMC2_ARCADE_THEME_TOXICWASTE:
		loadMachineList();
		break;
	case QMC2_ARCADE_THEME_DARKONE:
		// propagate empty gameList to QML
		rootContext()->setContextProperty("machineListModel", QVariant::fromValue(gameList));
		rootContext()->setContextProperty("machineListModelCount", gameList.count());
		break;
	}

#if QT_VERSION >= 0x050000
	connect(this, SIGNAL(frameSwapped()), this, SLOT(frameBufferSwapped()));
	connect(engine(), SIGNAL(quit()), this, SLOT(handleQuit()));
#endif

	connect(&frameCheckTimer, SIGNAL(timeout()), this, SLOT(fpsReady()));
}

TweakedQmlApplicationViewer::~TweakedQmlApplicationViewer()
{
	if ( m_initialized )
		saveSettings();
#if defined(QMC2_ARCADE_ENABLE_JOYSTICK)
	delete joystickManager;
	delete joyFunctionMap;
#endif
	delete keySequenceMap;
}

int TweakedQmlApplicationViewer::themeIndex()
{
	return arcadeThemes.indexOf(globalConfig->arcadeTheme);
}

void TweakedQmlApplicationViewer::logString(const QString &s)
{
	if ( consoleMode != QMC2_ARCADE_CONSOLE_NONE ) {
		if ( !consoleWindow ) {
			printf("%s: %s\n", QTime::currentTime().toString("hh:mm:ss.zzz").toUtf8().constData(), s.toUtf8().constData());
			fflush(stdout);
		} else
			consoleWindow->appendPlainText(QTime::currentTime().toString("hh:mm:ss.zzz") + ": " + s);
	}
}

void TweakedQmlApplicationViewer::logStringNoTime(const QString &s)
{
	if ( consoleMode != QMC2_ARCADE_CONSOLE_NONE ) {
		if ( !consoleWindow ) {
			printf("%s\n", s.toUtf8().constData());
			fflush(stdout);
		} else
			consoleWindow->appendPlainText(s);
	}
}

void TweakedQmlApplicationViewer::logCString(const char *s)
{
	if ( consoleMode != QMC2_ARCADE_CONSOLE_NONE ) {
		if ( !consoleWindow ) {
			printf("%s: %s\n", (const char *)QTime::currentTime().toString("hh:mm:ss.zzz").toUtf8(), (const char *)s);
			fflush(stdout);
		} else
			consoleWindow->appendPlainText(QTime::currentTime().toString("hh:mm:ss.zzz") + ": " + QString(s));
	}
}

void TweakedQmlApplicationViewer::logCStringNoTime(const char *s)
{
	if ( consoleMode != QMC2_ARCADE_CONSOLE_NONE ) {
		if ( !consoleWindow ) {
			printf("%s\n", (const char *)s);
			fflush(stdout);
		} else
			consoleWindow->appendPlainText(QString(s));
	}
}

void TweakedQmlApplicationViewer::displayInit()
{
	if ( initialFullScreen() )
		switchToFullScreen(true);
	else
		switchToWindowed(true);
	if ( rootObject() )
		frameCheckTimer.start(QMC2_ARCADE_FPS_UPDATE_INTERVAL);
}

void TweakedQmlApplicationViewer::fpsReady()
{
	rootObject()->setProperty("fps", numFrames);
	numFrames = 0;
}

void TweakedQmlApplicationViewer::loadSettings()
{
	QMC2_ARCADE_LOG_STR(tr("Loading global and theme-specific settings"));

	// load global arcade settings
	rootObject()->setProperty("version", globalConfig->applicationVersion());
	rootObject()->setProperty("qtVersion", qVersion());
	QStringList systemArtworkList = customSystemArtwork();
	int systemArtworkIndex = -1;

	// load theme-specific arcade settings
	switch ( themeIndex() ) {
	case QMC2_ARCADE_THEME_TOXICWASTE:
		rootObject()->setProperty("fpsVisible", globalConfig->fpsVisible());
		rootObject()->setProperty("showBackgroundAnimation", globalConfig->showBackgroundAnimation());
		rootObject()->setProperty("showShaderEffect", globalConfig->showShaderEffect());
		rootObject()->setProperty("animateInForeground", globalConfig->animateInForeground());
		rootObject()->setProperty("fullScreen", globalConfig->fullScreen());
		rootObject()->setProperty("secondaryImageType", globalConfig->secondaryImageType());
		systemArtworkIndex = systemArtworkList.indexOf(globalConfig->secondaryImageType());
		if ( systemArtworkIndex >= 0 )
			m_currentSystemArtworkIndex = systemArtworkIndex;
		rootObject()->setProperty("cabinetFlipped", globalConfig->cabinetFlipped());
		rootObject()->setProperty("lastIndex", globalConfig->lastIndex() < gameList.count() ? globalConfig->lastIndex() : 0);
		rootObject()->setProperty("menuHidden", globalConfig->menuHidden());
		rootObject()->setProperty("confirmQuit", globalConfig->confirmQuit());
		rootObject()->setProperty("machineCardPage", globalConfig->machineCardPage());
		rootObject()->setProperty("preferencesTab", globalConfig->preferencesTab());
		rootObject()->setProperty("autoPositionOverlay", globalConfig->autoPositionOverlay());
		rootObject()->setProperty("overlayScale", QMC2_ARCADE_MAX(0.0, QMC2_ARCADE_MIN(10.0, globalConfig->overlayScale())));
		rootObject()->setProperty("overlayOffsetX", globalConfig->overlayOffsetX());
		rootObject()->setProperty("overlayOffsetY", globalConfig->overlayOffsetY());
		rootObject()->setProperty("overlayOpacity", globalConfig->overlayOpacity());
		rootObject()->setProperty("backgroundOpacity", globalConfig->backgroundOpacity());
		rootObject()->setProperty("machineListOpacity", globalConfig->machineListOpacity());
		rootObject()->setProperty("cabinetImageType", globalConfig->cabinetImageType());
		rootObject()->setProperty("autoStopAnimations", globalConfig->autoStopAnimations());
		if ( videoEnabled() ) {
			rootObject()->setProperty("videoPlayerVolume", QMC2_ARCADE_MAX(0.0, QMC2_ARCADE_MIN(1.0, globalConfig->videoPlayerVolume())));
			rootObject()->setProperty("videoAutoPlayTimeout", QMC2_ARCADE_MAX(-1, QMC2_ARCADE_MIN(60, globalConfig->videoAutoPlayTimeout())) * 1000);
		}
		break;
	case QMC2_ARCADE_THEME_DARKONE:
		rootObject()->setProperty("lastIndex", globalConfig->lastIndex());
		rootObject()->setProperty("dataTypePrimary", globalConfig->dataTypePrimary());
		rootObject()->setProperty("dataTypeSecondary", globalConfig->dataTypeSecondary());
		rootObject()->setProperty("fullScreen", globalConfig->fullScreen());
		rootObject()->setProperty("listHidden", globalConfig->listHidden());
		rootObject()->setProperty("toolbarHidden", globalConfig->toolbarHidden());
		rootObject()->setProperty("fpsVisible", globalConfig->fpsVisible());
		rootObject()->setProperty("sortByName", globalConfig->sortByName());
		rootObject()->setProperty("screenLight", globalConfig->screenLight());
		rootObject()->setProperty("screenLightOpacity", globalConfig->screenLightOpacity());
		rootObject()->setProperty("backLight", globalConfig->backLight());
		rootObject()->setProperty("backLightOpacity", globalConfig->backLightOpacity());
		rootObject()->setProperty("toolbarAutoHide", globalConfig->toolbarAutoHide());
		rootObject()->setProperty("launchFlash", globalConfig->launchFlash());
		rootObject()->setProperty("launchZoom", globalConfig->launchZoom());
		rootObject()->setProperty("overlayScale", QMC2_ARCADE_MAX(0.33, globalConfig->overlayScale()));
		rootObject()->setProperty("lightTimeout", QMC2_ARCADE_MAX(5.0, globalConfig->lightTimeout()));
		rootObject()->setProperty("colourScheme", globalConfig->colourScheme());
		if ( videoEnabled() ) {
			rootObject()->setProperty("videoPlayerVolume", QMC2_ARCADE_MAX(0.0, QMC2_ARCADE_MIN(1.0, globalConfig->videoPlayerVolume())));
			rootObject()->setProperty("videoAutoPlayTimeout", QMC2_ARCADE_MAX(-1, QMC2_ARCADE_MIN(60, globalConfig->videoAutoPlayTimeout())) * 1000);
		}
		break;
	}
	m_initialized = true;
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
	switch ( themeIndex() ) {
	case QMC2_ARCADE_THEME_TOXICWASTE:
		globalConfig->setFpsVisible(rootObject()->property("fpsVisible").toBool());
		globalConfig->setShowBackgroundAnimation(rootObject()->property("showBackgroundAnimation").toBool());
		globalConfig->setShowShaderEffect(rootObject()->property("showShaderEffect").toBool());
		globalConfig->setAnimateInForeground(rootObject()->property("animateInForeground").toBool());
		globalConfig->setFullScreen(rootObject()->property("fullScreen").toBool());
		globalConfig->setSecondaryImageType(rootObject()->property("secondaryImageType").toString());
		globalConfig->setCabinetFlipped(rootObject()->property("cabinetFlipped").toBool());
		globalConfig->setLastIndex(rootObject()->property("lastIndex").toInt());
		globalConfig->setMenuHidden(rootObject()->property("menuHidden").toBool());
		globalConfig->setConfirmQuit(rootObject()->property("confirmQuit").toBool());
		globalConfig->setMachineCardPage(rootObject()->property("machineCardPage").toInt());
		globalConfig->setPreferencesTab(rootObject()->property("preferencesTab").toInt());
		globalConfig->setAutoPositionOverlay(rootObject()->property("autoPositionOverlay").toBool());
		globalConfig->setOverlayScale(rootObject()->property("overlayScale").toDouble());
		globalConfig->setOverlayOffsetX(rootObject()->property("overlayOffsetX").toDouble());
		globalConfig->setOverlayOffsetY(rootObject()->property("overlayOffsetY").toDouble());
		globalConfig->setOverlayOpacity(rootObject()->property("overlayOpacity").toDouble());
		globalConfig->setBackgroundOpacity(rootObject()->property("backgroundOpacity").toDouble());
		globalConfig->setMachineListOpacity(rootObject()->property("machineListOpacity").toDouble());
		globalConfig->setCabinetImageType(rootObject()->property("cabinetImageType").toString());
		globalConfig->setAutoStopAnimations(rootObject()->property("autoStopAnimations").toBool());
		if ( videoEnabled() ) {
			globalConfig->setVideoPlayerVolume(rootObject()->property("videoPlayerVolume").toDouble());
			globalConfig->setVideoAutoPlayTimeout(rootObject()->property("videoAutoPlayTimeout").toInt() / 1000);
		}
		break;
	case QMC2_ARCADE_THEME_DARKONE:
		globalConfig->setLastIndex(rootObject()->property("lastIndex").toInt());
		globalConfig->setDataTypePrimary(rootObject()->property("dataTypePrimary").toString());
		globalConfig->setDataTypeSecondary(rootObject()->property("dataTypeSecondary").toString());
		globalConfig->setToolbarHidden(rootObject()->property("toolbarHidden").toBool());
		globalConfig->setListHidden(rootObject()->property("listHidden").toBool());
		globalConfig->setFullScreen(rootObject()->property("fullScreen").toBool());
		globalConfig->setFpsVisible(rootObject()->property("fpsVisible").toBool());
		globalConfig->setSortByName(rootObject()->property("sortByName").toBool());
		globalConfig->setScreenLight(rootObject()->property("screenLight").toBool());
		globalConfig->setScreenLightOpacity(rootObject()->property("screenLightOpacity").toDouble());
		globalConfig->setBackLight(rootObject()->property("backLight").toBool());
		globalConfig->setBackLightOpacity(rootObject()->property("backLightOpacity").toDouble());
		globalConfig->setToolbarAutoHide(rootObject()->property("toolbarAutoHide").toBool());
		globalConfig->setLaunchFlash(rootObject()->property("launchFlash").toBool());
		globalConfig->setLaunchZoom(rootObject()->property("launchZoom").toBool());
		globalConfig->setOverlayScale(rootObject()->property("overlayScale").toDouble());
		globalConfig->setLightTimeout(rootObject()->property("lightTimeout").toDouble());
		globalConfig->setColourScheme(rootObject()->property("colourScheme").toString());
		if ( videoEnabled() ) {
			globalConfig->setVideoPlayerVolume(rootObject()->property("videoPlayerVolume").toDouble());
			globalConfig->setVideoAutoPlayTimeout(rootObject()->property("videoAutoPlayTimeout").toInt() / 1000);
		}
		break;
	}
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
	showFullScreen();
	windowModeSwitching = false;
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
	restoreGeometry(savedGeometry);
	if ( savedMaximized )
		showMaximized();
	else
		showNormal();
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

void TweakedQmlApplicationViewer::loadMachineList()
{
	QString gameListCachePath;
	gameList.clear();
	m_parentHash.clear();

	if ( globalConfig->useFilteredList() ) {
		gameListCachePath = QFileInfo(globalConfig->filteredListFile()).absoluteFilePath();
		if ( !QFileInfo(gameListCachePath).exists() || !QFileInfo(gameListCachePath).isReadable() ) {
			QMC2_ARCADE_LOG_STR(tr("WARNING: filtered list file '%1' doesn't exist or isn't accessible, falling back to the full %2").arg(gameListCachePath).arg(tr("machine list")));
			gameListCachePath = QFileInfo(globalConfig->gameListCacheFile()).absoluteFilePath();
		}
	} else
		gameListCachePath = QFileInfo(globalConfig->gameListCacheFile()).absoluteFilePath();

	QHash<QString, char> rscHash;

	QMC2_ARCADE_LOG_STR(tr("Loading %1 from '%2'").arg(tr("machine list")).arg(QDir::toNativeSeparators(gameListCachePath)));

	QString romStateCachePath = QFileInfo(globalConfig->romStateCacheFile()).absoluteFilePath();
	QFile romStateCache(romStateCachePath);
	if ( romStateCache.exists() ) {
		if ( romStateCache.open(QIODevice::ReadOnly | QIODevice::Text) ) {
			QTextStream tsRomCache(&romStateCache);
			int lineCounter = 0;
			while ( !tsRomCache.atEnd() ) {
				QString line = tsRomCache.readLine();
				if ( !line.isEmpty() && !line.startsWith("#") ) {
					QStringList words = line.split(" ");
					rscHash[words[0]] = words[1].at(0).toLatin1();
				}
				if ( lineCounter++ % QMC2_ARCADE_LOAD_RESPONSE == 0 )
					qApp->processEvents();
			}
		} else
			QMC2_ARCADE_LOG_STR(tr("WARNING: Can't open ROM state cache file '%1', please check permissions").arg(QDir::toNativeSeparators(romStateCachePath)));
	} else
		QMC2_ARCADE_LOG_STR(tr("WARNING: The ROM state cache file '%1' doesn't exist, please run main front-end executable to create it").arg(QDir::toNativeSeparators(romStateCachePath)));

	QFile gameListCache(gameListCachePath);
	if ( gameListCache.exists() ) {
		if ( gameListCache.open(QIODevice::ReadOnly | QIODevice::Text) ) {
			QTextStream tsGameListCache(&gameListCache);
			tsGameListCache.readLine();
			tsGameListCache.readLine();
			int lineCounter = 0;
			while ( !tsGameListCache.atEnd() ) {
				QStringList words = tsGameListCache.readLine().split("\t");
				if ( words[QMC2_ARCADE_MLC_DEVICE] != "1" ) {
					QString gameId = words[QMC2_ARCADE_MLC_ID];
					QString parentId = words[QMC2_ARCADE_MLC_PARENT];
					gameList.append(new MachineObject(gameId, parentId, words[QMC2_ARCADE_MLC_DESCRIPTION], romStateCharToInt(rscHash[gameId])));
					m_parentHash.insert(gameId, parentId);
				}
				if ( lineCounter++ % QMC2_ARCADE_LOAD_RESPONSE == 0 )
					qApp->processEvents();
			}
		} else
			QMC2_ARCADE_LOG_STR(tr("FATAL: Can't open %1 cache file '%2', please check permissions").arg(tr("machine list")).arg(QDir::toNativeSeparators(gameListCachePath)));
	} else
		QMC2_ARCADE_LOG_STR(tr("FATAL: The %1 cache file '%2' doesn't exist, please run main front-end executable to create it").arg(tr("machine list")).arg(QDir::toNativeSeparators(gameListCachePath)));

	if ( globalConfig->sortByName() )
		std::sort(gameList.begin(), gameList.end(), MachineObject::lessThan);

	// propagate gameList to QML
	rootContext()->setContextProperty("machineListModel", QVariant::fromValue(gameList));
	rootContext()->setContextProperty("machineListModelCount", gameList.count());

	QMC2_ARCADE_LOG_STR(QString(tr("Done (loading %1 from '%2')").arg(tr("machine list")) + " - " + tr("%n non-device set(s) loaded", "", gameList.count())).arg(QDir::toNativeSeparators(gameListCachePath)));
}

void TweakedQmlApplicationViewer::launchEmulator(QString id)
{
	QMC2_ARCADE_LOG_STR(tr("Starting emulator #%1 for %2 ID '%3'").arg(processManager->highestProcessID()).arg(tr("machine")).arg(id));
	processManager->startEmulator(id);
}

QString TweakedQmlApplicationViewer::loadImage(const QString &id)
{
	return imageProvider->loadImage(id);
}

QString TweakedQmlApplicationViewer::requestInfo(const QString &id, const QString &infoClass)
{
	QString infoText;

	switch ( infoClasses.indexOf(infoClass) ) {
	case QMC2_ARCADE_INFO_CLASS_MACHINE:
		infoText = infoProvider->requestInfo(id, InfoProvider::InfoClassMachine);
		break;
	case QMC2_ARCADE_INFO_CLASS_EMU:
		infoText = infoProvider->requestInfo(id, InfoProvider::InfoClassEmu);
		break;
	case QMC2_ARCADE_INFO_CLASS_SOFT:
		infoText = infoProvider->requestInfo(id, InfoProvider::InfoClassSoft);
		break;
	default:
		QMC2_ARCADE_LOG_STR(tr("WARNING: TweakedQmlApplicationViewer::requestInfo(): unsupported info class '%1'").arg(infoClass));
		return QString("<p>" + tr("no info available") + "</p>");
	}

	if ( infoText.isEmpty() ) {
		QString pI = parentId(id);
		if ( !pI.isEmpty() ) {
			switch ( infoClasses.indexOf(infoClass) ) {
			case QMC2_ARCADE_INFO_CLASS_MACHINE:
				infoText = infoProvider->requestInfo(pI, InfoProvider::InfoClassMachine);
				break;
			case QMC2_ARCADE_INFO_CLASS_EMU:
				infoText = infoProvider->requestInfo(pI, InfoProvider::InfoClassEmu);
				break;
			case QMC2_ARCADE_INFO_CLASS_SOFT:
				infoText = infoProvider->requestInfo(pI, InfoProvider::InfoClassSoft);
				break;
			}
		}
	}

	if ( infoText.isEmpty() )
		infoText = "<p>" + tr("no info available") + "</p>";

	return infoText;
}

QString TweakedQmlApplicationViewer::videoSnapUrl(const QString &id)
{
	if ( m_videoSnapUrlCache.contains(id) )
		return m_videoSnapUrlCache[id];
	foreach (QString videoSnapFolder, globalConfig->videoSnapFolder().split(";", QString::SkipEmptyParts)) {
		foreach (QString formatExtension, videoSnapAllowedFormatExtensions) {
			QFileInfo fi(QDir::cleanPath(videoSnapFolder + "/" + id + formatExtension));
			if ( fi.exists() && fi.isReadable() ) {
				QString videoSnapUrl = fi.absoluteFilePath();
#if defined(QMC2_ARCADE_OS_WIN)
				videoSnapUrl.prepend("file:///");
#else
				videoSnapUrl.prepend("file://");
#endif
				m_videoSnapUrlCache[id] = videoSnapUrl;
				return videoSnapUrl;
			}
		}
		// parent fallback
		if ( globalConfig->parentFallback("vdo") ) {
			QString pI = parentId(id);
			if ( !pI.isEmpty() ) {
				foreach (QString formatExtension, videoSnapAllowedFormatExtensions) {
					QFileInfo fi(QDir::cleanPath(videoSnapFolder + "/" + pI + formatExtension));
					if ( fi.exists() && fi.isReadable() ) {
						QString videoSnapUrl = fi.absoluteFilePath();
#if defined(QMC2_ARCADE_OS_WIN)
						videoSnapUrl.prepend("file:///");
#else
						videoSnapUrl.prepend("file://");
#endif
						m_videoSnapUrlCache[id] = videoSnapUrl;
						return videoSnapUrl;
					}
				}
			}
		}
	}
	return QString();
}

int TweakedQmlApplicationViewer::findIndex(QString pattern, int startIndex)
{
	if ( pattern.isEmpty() )
		return startIndex;

	int foundIndex = startIndex;
	bool indexFound = false;

	QRegExp wildcard(pattern, Qt::CaseInsensitive, QRegExp::Wildcard);
	QRegExp regexp(pattern, Qt::CaseInsensitive, QRegExp::RegExp);

	for (int i = startIndex + 1; i < gameList.count() && !indexFound; i++) {
		QString description = ((MachineObject *)gameList[i])->description();
		QString id = ((MachineObject *)gameList[i])->id();
		if ( description.indexOf(wildcard, 0) >= 0 || id.indexOf(wildcard, 0) >= 0 ) {
			foundIndex = i;
			indexFound = true;
		} else if ( regexp.indexIn(description, 0) >= 0 || regexp.indexIn(id, 0) >= 0 ) {
			foundIndex = i;
			indexFound = true;
		}
	}

	for (int i = 0; i < startIndex && !indexFound; i++) {
		QString description = ((MachineObject *)gameList[i])->description();
		QString id = ((MachineObject *)gameList[i])->id();
		if ( description.indexOf(wildcard, 0) >= 0 || id.indexOf(wildcard, 0) >= 0 ) {
			foundIndex = i;
			indexFound = true;
		} else if ( regexp.indexIn(description, 0) >= 0 || regexp.indexIn(id, 0) >= 0 ) {
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

QStringList TweakedQmlApplicationViewer::cliParamNames()
{
	return cliAllowedParameterValues.keys();
}

QString TweakedQmlApplicationViewer::cliParamDescription(QString param)
{
	return cliParameterDescriptions[param];
}

QString TweakedQmlApplicationViewer::cliParamValue(QString param)
{
	switch ( cliParams.indexOf(param) ) {
	case QMC2_ARCADE_PARAM_THEME:
		return globalConfig->defaultTheme();
#if QT_VERSION < 0x050000
	case QMC2_ARCADE_PARAM_GRASYS:
		return globalConfig->defaultGraphicsSystem();
#endif
	case QMC2_ARCADE_PARAM_CONSOLE:
		return globalConfig->defaultConsoleType();
	case QMC2_ARCADE_PARAM_LANGUAGE:
		return globalConfig->defaultLanguage();
	case QMC2_ARCADE_PARAM_VIDEO:
		return globalConfig->defaultVideo();
	default:
		return QString();
	}
}

QStringList TweakedQmlApplicationViewer::cliParamAllowedValues(QString param)
{
	return cliAllowedParameterValues[param];
}

void TweakedQmlApplicationViewer::setCliParamValue(QString param, QString value)
{
	switch ( cliParams.indexOf(param) ) {
	case QMC2_ARCADE_PARAM_THEME:
		globalConfig->setDefaultTheme(value);
		break;
#if QT_VERSION < 0x050000
	case QMC2_ARCADE_PARAM_GRASYS:
		globalConfig->setDefaultGraphicsSystem(value);
		break;
#endif
	case QMC2_ARCADE_PARAM_CONSOLE:
		globalConfig->setDefaultConsoleType(value);
		break;
	case QMC2_ARCADE_PARAM_LANGUAGE:
		globalConfig->setDefaultLanguage(value);
		break;
	case QMC2_ARCADE_PARAM_VIDEO:
		globalConfig->setDefaultVideo(value);
		break;
	}
}

void TweakedQmlApplicationViewer::linkActivated(QString link)
{
	QDesktopServices::openUrl(QUrl::fromUserInput(link));
}

QString TweakedQmlApplicationViewer::emuMode()
{
	switch ( emulatorMode ) {
	case QMC2_ARCADE_EMUMODE_MAME:
	default:
		return "mame";
	}
}

bool TweakedQmlApplicationViewer::iconCacheDatabaseEnabled()
{
	return globalConfig->iconCacheDatabaseEnabled();
}

QString TweakedQmlApplicationViewer::parentId(QString id)
{
	if ( m_parentHash.contains(id) )
		return m_parentHash[id];
	else
		return QString();
}

QStringList TweakedQmlApplicationViewer::customSystemArtwork()
{
	return globalConfig->customSystemArtworkNames();
}

QStringList TweakedQmlApplicationViewer::customSoftwareArtwork()
{
	return globalConfig->customSoftwareArtworkNames();
}

QString TweakedQmlApplicationViewer::nextCustomSytemArtwork()
{
	QString artwork;
	QStringList artworkList = customSystemArtwork();
	if ( m_currentSystemArtworkIndex == -2 )
		m_currentSystemArtworkIndex = 0;
	else
		m_currentSystemArtworkIndex++;
	if ( m_currentSystemArtworkIndex >= 0 && m_currentSystemArtworkIndex < artworkList.count() )
		artwork = artworkList[m_currentSystemArtworkIndex];
	else {
		m_currentSystemArtworkIndex = -2;
		return QString();
	}
	return artwork;
}

QString TweakedQmlApplicationViewer::previousCustomSytemArtwork()
{
	QString artwork;
	QStringList artworkList = customSystemArtwork();
	if ( m_currentSystemArtworkIndex == -2 )
		m_currentSystemArtworkIndex = artworkList.count() - 1;
	else
		m_currentSystemArtworkIndex--;
	if ( m_currentSystemArtworkIndex >= 0 && m_currentSystemArtworkIndex < artworkList.count() )
		artwork = artworkList[m_currentSystemArtworkIndex];
	else {
		m_currentSystemArtworkIndex = -2;
		return QString();
	}
	return artwork;
}

QString TweakedQmlApplicationViewer::nextCustomSoftwareArtwork()
{
	QStringList artworkList = customSoftwareArtwork();
	if ( m_currentSoftwareArtworkIndex == -2 )
		m_currentSoftwareArtworkIndex = 0;
	else
		m_currentSoftwareArtworkIndex++;
	if ( m_currentSoftwareArtworkIndex >= 0 && m_currentSoftwareArtworkIndex < artworkList.count() )
		return artworkList[m_currentSoftwareArtworkIndex];
	else {
		m_currentSoftwareArtworkIndex = -2;
		return QString();
	}
}

QString TweakedQmlApplicationViewer::previousCustomSoftwareArtwork()
{
	QStringList artworkList = customSoftwareArtwork();
	if ( m_currentSoftwareArtworkIndex == -2 )
		m_currentSoftwareArtworkIndex = artworkList.count() - 1;
	else
		m_currentSoftwareArtworkIndex--;
	if ( m_currentSoftwareArtworkIndex >= 0 && m_currentSoftwareArtworkIndex < artworkList.count() )
		return artworkList[m_currentSoftwareArtworkIndex];
	else {
		m_currentSoftwareArtworkIndex = -2;
		return QString();
	}
}

#if QT_VERSION >= 0x050000
void TweakedQmlApplicationViewer::handleQuit()
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

	close();
}
#else
void TweakedQmlApplicationViewer::paintEvent(QPaintEvent *e)
{
	QmlApplicationViewer::paintEvent(e);
	numFrames++;
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
#endif
