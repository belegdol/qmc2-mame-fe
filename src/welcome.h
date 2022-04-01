#ifndef WELCOME_H
#define WELCOME_H

#include <QStringList>

#include "ui_welcome.h"
#include "settings.h"
#include "macros.h"

class Welcome : public QDialog, public Ui::Welcome
{
	Q_OBJECT

	public:
		Welcome(QWidget *parent = 0);
		~Welcome();

		bool checkOkay;
		QSettings *startupConfig;

		bool checkConfig();
#if defined(QMC2_OS_MAC)
		bool useNativeFileDialogs() { return startupConfig->value(QMC2_FRONTEND_PREFIX + "GUI/NativeFileDialogs", true).toBool(); }
#else
		bool useNativeFileDialogs() { return startupConfig->value(QMC2_FRONTEND_PREFIX + "GUI/NativeFileDialogs", false).toBool(); }
#endif

	public slots:
		void on_pushButtonOkay_clicked();
		void on_pushButtonRunSetupWizard_clicked();
		void on_toolButtonBrowseExecutableFile_clicked();
		void on_toolButtonBrowseWorkingDirectory_clicked();
		void on_toolButtonBrowseROMPath_clicked();
		void on_toolButtonBrowseSamplePath_clicked();
		void on_toolButtonBrowseHashPath_clicked();
		void on_comboBoxLanguage_currentIndexChanged(int index);
		void setupLanguage();
		void reject();

	private:
		QStringList availableLanguages;
		QString originalLanguage;
};

#endif
