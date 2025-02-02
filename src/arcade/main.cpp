#include <QTranslator>
#include <QIcon>
#include <QStyleFactory>
#include <QTimer>
#include <QString>
#include <QUrl>
#include <QGuiApplication>

#include "arcadesettings.h"
#include "tweakedqmlappviewer.h"
#include "consolewindow.h"
#include "macros.h"
#include "joystick.h"
#include "keyeventfilter.h"
#if defined(QMC2_ARCADE_OS_WIN)
#include "../windows_tools.h"
#endif
#if defined(QMC2_ARCADE_OS_MAC)
#include <mach-o/dyld.h>
#include <QFileInfo>
#include <QDir>
#endif

ArcadeSettings *globalConfig = 0;
ConsoleWindow *consoleWindow = 0;
int emulatorMode = QMC2_ARCADE_EMUMODE_MAME;
QStringList emulatorModes;
QStringList arcadeThemes;
QStringList mameThemes;
QStringList consoleModes;
QStringList argumentList;
bool runApp = true;
bool debugKeys = false;
#if defined(QMC2_ARCADE_ENABLE_JOYSTICK)
bool debugJoy = false;
#endif
bool debugQt = false;

void qtMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
	if ( !runApp )
		return;

	QString msgString;

	switch ( type ) {
	case QtDebugMsg:
		if ( !debugQt )
			return;
		msgString = "QtDebugMsg: " + QString(msg);
		break;
	case QtWarningMsg:
		msgString = "QtWarningMsg: " + QString(msg);
		break;
	case QtCriticalMsg:
		msgString = "QtCriticalMsg: " + QString(msg);
		break;
	case QtFatalMsg:
		msgString = "QtFatalMsg: " + QString(msg);
		break;
	default:
		return;
	}

	QMC2_ARCADE_LOG_STR(msgString);
}

void showHelp()
{
#if defined(QMC2_ARCADE_OS_WIN)
	if ( !consoleWindow )
		winAllocConsole();
#endif

	QString defTheme(globalConfig->defaultTheme());
	QString defConsole(globalConfig->defaultConsoleType());
	QString defLang(globalConfig->defaultLanguage());
	QString defVideo(globalConfig->defaultVideo());

	QStringList themeList;
	foreach (QString theme, arcadeThemes) {
		if ( defTheme == theme )
			themeList << "[" + theme + "]";
		else
			themeList << theme;
	}
	QString availableThemes(themeList.join(", "));

	QStringList consoleList;
	foreach (QString console, consoleModes) {
		if ( defConsole == console )
			consoleList << "[" + console + "]";
		else
			consoleList << console;
	}
	QString availableConsoles(consoleList.join(", "));

	QStringList langList;
	foreach (QString lang, globalConfig->languageMap.keys()) {
		if ( defLang == lang )
			langList << "[" + lang + "]";
		else
			langList << lang;
	}
	QString availableLanguages(langList.join(", "));

	QStringList videoList;
	foreach (QString v, QStringList() << "on" << "off") {
		if ( defVideo == v )
			videoList << "[" + v + "]";
		else
			videoList << v;
	}
	QString availableVideoSettings(videoList.join(", "));

	QString helpMessage;
#if defined(QMC2_ARCADE_ENABLE_JOYSTICK)
	helpMessage  = "Usage: qmc2-arcade [-theme <theme>] [-console <type>] [-language <lang>] [-video <vdo>] [-config_path <path>] [-fullscreen] [-windowed] [-nojoy] [-joy <index>] [-debugjoy] [-debugkeys] [-debugqt] [-h|-?|-help]\n\n";
#else
	helpMessage  = "Usage: qmc2-arcade [-theme <theme>] [-console <type>] [-language <lang>] [-video <vdo>] [-config_path <path>] [-fullscreen] [-windowed] [-debugkeys] [-debugqt] [-h|-?|-help]\n\n";
#endif
	helpMessage += "Option           Meaning                Possible values ([..] = default)\n"
		       "---------------  ---------------------  --------------------------------------------------\n";
	helpMessage += "-theme           Theme selection        " + availableThemes + "\n";
	helpMessage += "-console         Console type           " + availableConsoles + "\n";
	helpMessage += "-language        Language selection     " + availableLanguages + "\n";
	helpMessage += "-video           Video snap support     " + availableVideoSettings + "\n";
	helpMessage += QString("-config_path     Configuration path     [%1], ...\n").arg(QMC2_ARCADE_DOT_PATH);
	helpMessage += "-fullscreen      Full screen display    N/A\n";
	helpMessage += "-windowed        Windowed display       N/A\n";
#if defined(QMC2_ARCADE_ENABLE_JOYSTICK)
	helpMessage += "-nojoy           Disable joystick       N/A\n";
	helpMessage += QString("-joy             Use given joystick     SDL joystick index number [%1]\n").arg(globalConfig->joystickIndex());
	helpMessage += "-debugjoy        Debug joy-mapping      N/A\n";
#endif
	helpMessage += "-debugkeys       Debug key-mapping      N/A\n";
	helpMessage += "-debugqt         Log Qt debug messages  N/A\n";

#if defined(QMC2_ARCADE_OS_WIN)
	if ( !consoleWindow )
		helpMessage.remove(helpMessage.length() - 1, 1);
#endif

	QMC2_ARCADE_LOG_STR_NT(helpMessage);
}

void upgradeSettings()
{
	/*
	QStringList verList = globalConfig->value("Version").toString().split(".", Qt::SkipEmptyParts);
	if ( verList.count() > 1 ) {
		int omv = verList[1].toInt();
		int osr = globalConfig->value("GIT_Revision").toInt();
		if ( QMC2_ARCADE_TEST_VERSION(omv, 57, osr, 6989) ) {
			QStringList oldKeys = QStringList() << "/MAME/DatInfoDatabase/GameInfoImportFiles"
							    << "/MAME/DatInfoDatabase/GameInfoImportDates";
			QStringList newKeys = QStringList() << "/MAME/DatInfoDatabase/MachineInfoImportFiles"
							    << "/MAME/DatInfoDatabase/MachineInfoImportDates";
			for (int i = 0; i < oldKeys.count(); i++) {
				QString oldKey = oldKeys[i];
				QString newKey = newKeys[i];
				if ( globalConfig->contains(oldKey) ) {
					globalConfig->setValue(newKey, globalConfig->value(oldKey));
					globalConfig->remove(oldKey);
				}
			}
		}
	}
	*/
}

#if defined(QMC2_ARCADE_OS_WIN)
#if defined(TCOD_VISUAL_STUDIO)
int SDL_main(int argc, char *argv[])
{
	return main(argc, argv);
}
#endif
#if defined(QMC2_ARCADE_MINGW)
#undef main
#endif
#endif

int main(int argc, char *argv[])
{
#if defined(QMC2_ARCADE_OS_MAC)
	// this hack ensures that we're using the bundled plugins rather than the ones from a Qt SDK installation
	char exec_path[4096];
	uint32_t exec_path_size = sizeof(exec_path);
	if ( _NSGetExecutablePath(exec_path, &exec_path_size) == 0 ) {
		QFileInfo fi(exec_path);
		QCoreApplication::addLibraryPath(fi.absoluteDir().absolutePath() + "/../PlugIns");
	}
#endif

	qsrand(QDateTime::currentDateTime().toTime_t());
	qInstallMessageHandler(qtMessageHandler);

	// available emulator-modes, themes, console-modes and graphics-systems
	emulatorModes << "mame";
	arcadeThemes << "ToxicWaste" << "darkone";
	mameThemes << "ToxicWaste" << "darkone";
	consoleModes << "terminal" << "window" << "window-minimized" << "none";

	// we have to make a copy of the command line arguments since QApplication's constructor "eats"
	// -graphicssystem and its value (and we *really* need to know if it has been set or not!)
	for (int i = 0; i < argc; i++)
		argumentList << argv[i];

	QCoreApplication::setOrganizationName(QMC2_ARCADE_ORG_NAME);
	QCoreApplication::setOrganizationDomain(QMC2_ARCADE_ORG_DOMAIN);
	QCoreApplication::setApplicationName(QMC2_ARCADE_APP_NAME);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, ArcadeSettings::configPath());

	// create the actual application instance
	QGuiApplication *app = new QGuiApplication(argc, argv);

	if ( !QMC2_ARCADE_CLI_EMU_UNK ) {
		emulatorMode = QMC2_ARCADE_EMUMODE_MAME;
	} else if ( !emulatorModes.contains(QMC2_ARCADE_CLI_EMU) ) {
		QMC2_ARCADE_LOG_STR_NT(QObject::tr("%1 is not a valid emulator-mode - available emulator-modes: %2").arg(QMC2_ARCADE_CLI_EMU).arg(emulatorModes.join(", ")));
		return 1;
	}

	globalConfig = new ArcadeSettings;

	QString console(globalConfig->defaultConsoleType());
	if ( QMC2_ARCADE_CLI_CONS_VAL )
		console = QMC2_ARCADE_CLI_CONS;

	if ( console == "window" || console == "window-minimized" ) {
		TweakedQmlApplicationViewer::consoleMode = console == "window" ? QMC2_ARCADE_CONSOLE_WIN : QMC2_ARCADE_CONSOLE_WINMIN;
		consoleWindow = new ConsoleWindow(0);
		if ( TweakedQmlApplicationViewer::consoleMode == QMC2_ARCADE_CONSOLE_WINMIN )
			consoleWindow->showMinimized();
		else
			consoleWindow->show();
	} else if ( !consoleModes.contains(console) ) {
		QMC2_ARCADE_LOG_STR_NT(QObject::tr("%1 is not a valid console-mode - available console-modes: %2").arg(console).arg(consoleModes.join(", ")));
		return 1;
	} else
		TweakedQmlApplicationViewer::consoleMode = console == "terminal" ? QMC2_ARCADE_CONSOLE_TERM : QMC2_ARCADE_CONSOLE_NONE;

	if ( QMC2_ARCADE_CLI_HELP || QMC2_ARCADE_CLI_INVALID ) {
		showHelp();
		if ( !consoleWindow ) {
			delete globalConfig;
			return 1;
		} else
			runApp = false;
	}

#if defined(QMC2_ARCADE_OS_WIN)
	if ( console == "terminal" )
		winAllocConsole();
#endif

	QString theme(globalConfig->defaultTheme());
	if ( QMC2_ARCADE_CLI_THEME_VAL )
		theme = QMC2_ARCADE_CLI_THEME;

	if ( !arcadeThemes.contains(theme) && runApp ) {
		QMC2_ARCADE_LOG_STR_NT(QObject::tr("%1 is not valid theme - available themes: %2").arg(theme).arg(arcadeThemes.join(", ")));
		if ( !consoleWindow ) {
			delete globalConfig;
			return 1;
		} else
			runApp = false;
	}

	delete globalConfig;
	globalConfig = 0;

	switch ( emulatorMode ) {
	case QMC2_ARCADE_EMUMODE_MAME:
	default:
		if ( !mameThemes.contains(theme) && runApp ) {
			QMC2_ARCADE_LOG_STR_NT(QObject::tr("%1 is not a valid %2 theme - available %2 themes: %3").arg(theme).arg(emulatorModes[QMC2_ARCADE_EMUMODE_MAME]).arg(mameThemes.isEmpty() ? QObject::tr("(none)") : mameThemes.join(", ")));
			if ( !consoleWindow )
				return 1;
			else
				runApp = false;
		}
		break;
	}

	// create final instance of the global settings object
	globalConfig = new ArcadeSettings(theme);
	upgradeSettings();
	globalConfig->setApplicationVersion(QMC2_ARCADE_APP_VERSION);

	// set default font
	QString font(globalConfig->defaultFont());
	if ( !font.isEmpty() ) {
		QFont f;
		f.fromString(font);
		app->setFont(f);
	}

	// set language
	QString language(globalConfig->defaultLanguage());
	if ( QMC2_ARCADE_CLI_LANG_VAL )
		language = QMC2_ARCADE_CLI_LANG;
	if ( !globalConfig->languageMap.contains(language) && !globalConfig->countryMap.contains(language) ) {
		if ( QMC2_ARCADE_CLI_LANG_VAL ) {
			QStringList languages = globalConfig->languageMap.keys() + globalConfig->countryMap.keys();
			languages.sort();
			QMC2_ARCADE_LOG_STR_NT(QString("%1 is not a valid language - available languages: %2").arg(language).arg(languages.join(", ")));
			delete globalConfig;
			return 1;
		} else
			language = "us";
	}

	// load translator
	QTranslator qmc2ArcadeTranslator;
	if ( qmc2ArcadeTranslator.load(QString("qmc2-arcade_%1").arg(language), ":/translations") )
		app->installTranslator(&qmc2ArcadeTranslator);

	int returnCode;

	if ( runApp ) {
		// log banner message
		QString bannerMessage = QString("%1 %2 (%3)").
				arg(QMC2_ARCADE_APP_TITLE).
#if defined(QMC2_ARCADE_GIT_REV)
				arg(QMC2_ARCADE_APP_VERSION + QString(", GIT %1").arg(XSTR(QMC2_ARCADE_GIT_REV))).
#else
				arg(QMC2_ARCADE_APP_VERSION).
#endif
				arg(QString("Qt") + " " + qVersion() + ", " +
				    QObject::tr("emulator-mode: %1").arg(emulatorModes[emulatorMode]) + ", " +
				    QObject::tr("console-mode: %1").arg(consoleModes[TweakedQmlApplicationViewer::consoleMode]) + ", " +
				    QObject::tr("language: %1").arg(language) + ", " +
				    QObject::tr("theme: %1").arg(theme));

		QMC2_ARCADE_LOG_STR(bannerMessage);

		if ( consoleWindow )
			consoleWindow->loadSettings();

		// debug options
#if defined(QMC2_ARCADE_ENABLE_JOYSTICK)
		debugJoy = QMC2_ARCADE_CLI_DEBUG_JOY;
#endif
		debugKeys = QMC2_ARCADE_CLI_DEBUG_KEYS;
		debugQt = QMC2_ARCADE_CLI_DEBUG_QT;

		// set up the main QML app viewer window
		TweakedQmlApplicationViewer *viewer = new TweakedQmlApplicationViewer();

		// install our key-event filter to remap key-sequences, if applicable
		KeyEventFilter keyEventFilter(viewer->keySequenceMap);
		app->installEventFilter(&keyEventFilter);

		viewer->setTitle(QMC2_ARCADE_APP_TITLE + " " + QMC2_ARCADE_APP_VERSION + " [Qt " + qVersion() + "]");
		viewer->winId(); // see QTBUG-33370 QQuickView does not set icon correctly
		viewer->setIcon(QIcon(QLatin1String(":/images/qmc2-arcade.png")));
		viewer->setColor(QColor(0, 0, 0, 255));

		bool initialFullScreen = globalConfig->fullScreen();
		if ( QMC2_ARCADE_CLI_FULLSCREEN )
			initialFullScreen = true;
		else if ( QMC2_ARCADE_CLI_WINDOWED )
			initialFullScreen = false;

		// setup viewer params
		viewer->setInitialFullScreen(initialFullScreen);
		viewer->setVideoEnabled(globalConfig->defaultVideo() == "on");
		if ( QMC2_ARCADE_CLI_VIDEO_VAL )
			viewer->setVideoEnabled(QMC2_ARCADE_CLI_VIDEO == "on");

		QMC2_ARCADE_LOG_STR(QObject::tr("Starting QML viewer using theme '%1'").arg(theme) + " (" + QObject::tr("video snaps %1").arg(viewer->videoEnabled() ? QObject::tr("enabled") : QObject::tr("disabled")) + ")");

		// load theme
		QString themeUrl;
		if ( viewer->videoEnabled() )
			themeUrl = QString("qrc:/qml/%1/2.0/%1-video.qml").arg(theme);
		else
			themeUrl = QString("qrc:/qml/%1/2.0/%1.qml").arg(theme);
		viewer->setSource(QUrl(themeUrl));

		// delayed setup of the initial display mode
		QTimer::singleShot(100, viewer, SLOT(displayInit()));

		// run the event loop
		returnCode = app->exec();

		// remove the key-event filter before destroying the viewer, otherwise there's a small possibility
		// for an exit-crash because the event-filter uses the key-sequence-map instance from the viewer
		app->removeEventFilter(&keyEventFilter);
		delete viewer;
	} else {
		if ( consoleWindow ) {
			consoleWindow->loadSettings();
			QString consoleMessage(QObject::tr("QML viewer not started - please close the console window to exit"));
			QMC2_ARCADE_LOG_STR_NT(QString("-").repeated(consoleMessage.length()));
			QMC2_ARCADE_LOG_STR_NT(consoleMessage);
			QMC2_ARCADE_LOG_STR_NT(QString("-").repeated(consoleMessage.length()));
			consoleWindow->showNormal();
			consoleWindow->raise();
			app->exec();
		}
		returnCode = 1;
	}

	if ( consoleWindow ) {
		consoleWindow->saveSettings();
		delete consoleWindow;
	}

	delete globalConfig;

	return returnCode;
}
