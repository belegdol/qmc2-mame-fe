#ifndef CHECKSUMDBMGR_H
#define CHECKSUMDBMGR_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QString>
#include <QStringList>

class CheckSumDatabaseManager : public QObject
{
	Q_OBJECT

	public:
		explicit CheckSumDatabaseManager(QObject *parent, QString settingsKey);
		~CheckSumDatabaseManager();

		QString qmc2Version();
		void setQmc2Version(QString qmc2_version);
		int checkSumDbVersion();
		void setCheckSumDbVersion(int checksum_db_version);
		uint scanTime();
		void setScanTime(uint scan_time);

		qint64 checkSumRowCount();
		qint64 nextRowId(bool refreshRowIds = false);

		QString connectionName() { return m_connectionName; }
		QString databasePath() { return m_db.databaseName(); }
		quint64 databaseSize();
		void setCacheSize(quint64 kiloBytes);
		void setSyncMode(uint syncMode);
		void setJournalMode(uint journalMode);

		bool exists(QString sha1, QString crc, quint64 size = 0);
		void setData(QString sha1, QString crc, quint64 size, QString path, QString member, QString type);
		bool getData(QString sha1, QString crc, quint64 *size, QString *path, QString *member, QString *type);
		QString getCrc(QString sha1);
		QString getSha1(QString crc);

		void pathRemove(QString path);
		QString pathOfRow(qint64 row, QString *key = 0, bool simpleKey = false);
		QString keyOfRow(qint64 row);
		void invalidateRow(qint64 row);
		void removeInvalidatedRows();

		int nameToType(QString name);
		QString typeToName(int type);
		void emitlog(QString);

	signals:
		void log(const QString &);

	public slots:
		void recreateDatabase();
		void vacuum();
		void beginTransaction() { m_db.driver()->beginTransaction(); }
		void commitTransaction() { m_db.driver()->commitTransaction(); }

	private:
		mutable QSqlDatabase m_db;
		QString m_tableBasename;
		QString m_connectionName;
		QStringList m_fileTypes;
		QList<qint64> m_rowIdList;
		qint64 m_lastRowId;
		QString m_settingsKey;
};

#endif
