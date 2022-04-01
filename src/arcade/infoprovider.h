#ifndef INFOPROVIDER_H
#define INFOPROVIDER_H

#include <QString>

#include "datinfodbmgr.h"

class InfoProvider
{
public:
	explicit InfoProvider();
	virtual ~InfoProvider();

	enum InfoClass { InfoClassMachine, InfoClassEmu, InfoClassSoft };
	QString requestInfo(const QString &, InfoClass);

	bool isMessGameInfo(const QString & id) { return datInfoDb()->machineInfoEmulator(id) == "MESS"; }
	bool isMameGameInfo(const QString & id) { return datInfoDb()->machineInfoEmulator(id) == "MAME"; }

	QString &messWikiToHtml(QString &);
	DatInfoDatabaseManager *datInfoDb() { return m_datInfoDb; }

private:
	void loadGameInfo();
	void loadEmuInfo();
	void loadSoftwareInfo();
	QString urlSectionRegExp;
	DatInfoDatabaseManager *m_datInfoDb;
};

#endif
