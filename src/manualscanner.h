#ifndef MANUALSCANNER_H
#define MANUALSCANNER_H

#include <QString>
#include <QTimer>

#include "ui_manualscanner.h"

#define QMC2_MANUALSCANNER_MODE_SYSTEMS		0
#define QMC2_MANUALSCANNER_MODE_SOFTWARE	1

#define QMC2_MANUALSCANNER_SCAN_RESPONSE	10
#define QMC2_MANUALSCANNER_DB_COMMIT 		100

class ManualScanner : public QDialog, public Ui::ManualScanner
{
	Q_OBJECT

       	public:
		explicit ManualScanner(int mode, QWidget *parent = 0);
		~ManualScanner();

	public slots:
		void on_pushButtonScanNow_clicked();
		void log(const QString &);
		void scrollLog();

	protected:
		void showEvent(QShowEvent *);
		void hideEvent(QHideEvent *);
		void closeEvent(QCloseEvent *);

	private:
		void scan();
		void recursiveFileList(const QString &, QStringList *);

		int m_mode;
		QString m_settingsKey;
		QTimer m_logScrollTimer;
};

#endif
