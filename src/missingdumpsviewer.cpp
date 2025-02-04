#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QByteArray>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QMultiMap>
#include <QMessageBox>

#include "missingdumpsviewer.h"
#include "romalyzer.h"
#include "settings.h"
#include "options.h"
#include "macros.h"

extern Settings *qmc2Config;
extern Options *qmc2Options;

MissingDumpsViewer::MissingDumpsViewer(QString settingsKey, QWidget *parent) :
#if defined(QMC2_OS_WIN)
	QDialog(parent, Qt::Dialog)
#else
	QDialog(parent, Qt::Dialog | Qt::SubWindow)
#endif
{
	m_settingsKey = settingsKey;
	setVisible(false);
	setDefaultEmulator(false);
	setupUi(this);
	progressBar->hide();
}

void MissingDumpsViewer::on_toolButtonExportToDataFile_clicked()
{
	toolButtonExportToDataFile->setEnabled(false);
	QString storedPath(qmc2Config->value(QMC2_FRONTEND_PREFIX + m_settingsKey + "/LastDataFilePath", QString()).toString());
	QString dataFilePath(QFileDialog::getSaveFileName(this, tr("Choose data file to export to"), storedPath, tr("Data files (*.dat)") + ";;" + tr("All files (*)"), 0, qmc2Options->useNativeFileDialogs() ? (QFileDialog::Options)0 : QFileDialog::DontUseNativeDialog));
	if ( !dataFilePath.isNull() ) {
		QFile dataFile(dataFilePath);
		QFileInfo fi(dataFilePath);
		if ( dataFile.open(QIODevice::WriteOnly | QIODevice::Text) ) {
			QTextStream ts(&dataFile);
			ts << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
			ts << "<!DOCTYPE datafile PUBLIC \"-//Logiqx//DTD ROM Management Datafile//EN\" \"http://www.logiqx.com/Dats/datafile.dtd\">\n\n";
			ts << "<datafile>\n";
			ts << "\t<header>\n";
			ts << "\t\t<name>" << fi.completeBaseName() << "</name>\n";
			ts << "\t\t<description>" << fi.completeBaseName() << "</description>\n";
			ts << "\t\t<category>FIXDATFILE</category>\n";
			ts << "\t\t<version>" << QDateTime::currentDateTime().toString("MM/dd/yy hh:mm:ss") << "</version>\n";
			ts << "\t\t<date>" << QDateTime::currentDateTime().toString("yyyy-MM-dd") << "</date>\n";
			ts << "\t\t<author>auto-create</author>\n";
			ts << "\t\t<email></email>\n";
			ts << "\t\t<homepage></homepage>\n";
			ts << "\t\t<url></url>\n";
			ts << "\t\t<comment>" << tr("Created by QMC2 v%1").arg(XSTR(QMC2_VERSION)) << "</comment>\n";
			ts << "\t</header>\n";
			QString mainEntityName("machine");
			QMultiMap<QString, DumpRecord *> dumpMap;
			progressBar->setFormat(tr("Preparing"));
			progressBar->setRange(0, treeWidget->topLevelItemCount());
			progressBar->setValue(0);
			progressBar->show();
			for (int i = 0; i < treeWidget->topLevelItemCount(); i++) {
				QTreeWidgetItem *item = treeWidget->topLevelItem(i);
				if ( !checkBoxSelectedDumpsOnly->isChecked() || item->isSelected() )
					dumpMap.insertMulti(item->text(QMC2_MDV_COLUMN_ID), new DumpRecord(item->text(QMC2_MDV_COLUMN_NAME), item->text(QMC2_MDV_COLUMN_TYPE), item->text(QMC2_MDV_COLUMN_SIZE), item->text(QMC2_MDV_COLUMN_CRC), item->text(QMC2_MDV_COLUMN_SHA1)));
				progressBar->setValue(i);
			}
			QStringList dumpKeys(dumpMap.uniqueKeys());
			progressBar->setFormat(tr("Exporting"));
			progressBar->setRange(0, dumpKeys.count());
			progressBar->setValue(0);
			for (int i = 0; i < dumpKeys.count(); i++) {
				QString id(dumpKeys.at(i));
				if ( defaultEmulator() ) {
					QString sourcefile, isbios, cloneof, romof, sampleof, description, year, manufacturer, merge;
					QHash <QString, QString> mergeInfo;
					QXmlStreamReader xmlMachineEntry(ROMAlyzer::getXmlData(id, false).toUtf8());
					if ( xmlMachineEntry.readNextStartElement() ) {
						if ( xmlMachineEntry.name() == QLatin1String("machine") ) {
							ts << "\t<machine name=\"" << id << "\"";
							if ( xmlMachineEntry.attributes().hasAttribute("sourcefile") ) {
								sourcefile = xmlMachineEntry.attributes().value("sourcefile").toString();
								if ( !sourcefile.isEmpty() )
									ts << " sourcefile=\"" << sourcefile << "\"";
							}
							if ( xmlMachineEntry.attributes().hasAttribute("isbios") ) {
								isbios = xmlMachineEntry.attributes().value("isbios").toString();
								if ( !isbios.isEmpty() && isbios != "no" )
									ts << " isbios=\"" << isbios << "\"";
							}
							if ( xmlMachineEntry.attributes().hasAttribute("cloneof") ) {
								cloneof = xmlMachineEntry.attributes().value("cloneof").toString();
								if ( !cloneof.isEmpty() )
									ts << " cloneof=\"" << cloneof << "\"";
							}
							if ( xmlMachineEntry.attributes().hasAttribute("romof") ) {
								romof = xmlMachineEntry.attributes().value("romof").toString();
								if ( !romof.isEmpty() )
									ts << " romof=\"" << romof << "\"";
							}
							if ( xmlMachineEntry.attributes().hasAttribute("sampleof") ) {
								sampleof = xmlMachineEntry.attributes().value("sampleof").toString();
								if ( !sampleof.isEmpty() )
									ts << " sampleof=\"" << sampleof << "\"";
							}
							ts << ">\n";
							while ( xmlMachineEntry.readNextStartElement() ) {
								if ( xmlMachineEntry.name() == QLatin1String("description") ) {
									description = xmlMachineEntry.readElementText();
									if ( !description.isEmpty() )
										ts << "\t\t<description>" << description << "</description>\n";
								}
								else if ( xmlMachineEntry.name() == QLatin1String("year") ) {
									year = xmlMachineEntry.readElementText();
									if ( !year.isEmpty() )
										ts << "\t\t<year>" << year << "</year>\n";
								}
								else if ( xmlMachineEntry.name() == QLatin1String("manufacturer") ) {
									manufacturer = xmlMachineEntry.readElementText();
									if ( !manufacturer.isEmpty() )
										ts << "\t\t<manufacturer>" << manufacturer << "</manufacturer>\n";
								}
								else if ( ( xmlMachineEntry.name() == QLatin1String("rom") || xmlMachineEntry.name() == QLatin1String("disk") ) && xmlMachineEntry.attributes().hasAttribute("name") && xmlMachineEntry.attributes().hasAttribute("merge") ) {
									mergeInfo[xmlMachineEntry.attributes().value("name").toString()] = xmlMachineEntry.attributes().value("merge").toString();
									xmlMachineEntry.skipCurrentElement();
								}
								else
									xmlMachineEntry.skipCurrentElement();
							}
							foreach (DumpRecord *dr, dumpMap.values(id)) {
								if ( dr->type() == "ROM" ) {
									ts << "\t\t<rom name=\"" << dr->name() << "\"";
									merge = mergeInfo[dr->name()];
									if ( !merge.isEmpty() )
										ts << " merge=\"" << merge << "\"";
									if ( !dr->size().isEmpty() )
										ts << " size=\"" << dr->size() << "\"";
									if ( !dr->crc().isEmpty() )
										ts << " crc=\"" << dr->crc() << "\"";
									if ( !dr->sha1().isEmpty() )
										ts << " sha1=\"" << dr->sha1() << "\"";
									ts << "/>\n";
								} else {
									ts << "\t\t<disk name=\"" << dr->name() << "\"";
									merge = mergeInfo[dr->name()];
									if ( !merge.isEmpty() )
										ts << " merge=\"" << merge << "\"";
									if ( !dr->sha1().isEmpty() )
										ts << " sha1=\"" << dr->sha1() << "\"";;
									ts << "/>\n";
								}
								delete dr;
							}
							ts << "\t</machine>\n";
						}
					}
				} else {
					// FIXME "non-default emulator"
				}
				if ( i % QMC2_FIXDAT_EXPORT_RESPONSE == 0 ) {
					progressBar->setValue(i);
					qApp->processEvents();
				}
			}
			ts << "</datafile>\n";
			dataFile.close();
			dumpMap.clear();
			progressBar->hide();
		} else
			QMessageBox::critical(this, tr("Error"), tr("Can't open '%1' for writing!").arg(dataFilePath));
		qmc2Config->setValue(QMC2_FRONTEND_PREFIX + m_settingsKey + "/LastDataFilePath", dataFilePath);
	}
	toolButtonExportToDataFile->setEnabled(true);
}

void MissingDumpsViewer::showEvent(QShowEvent *e)
{
	restoreGeometry(qmc2Config->value(QMC2_FRONTEND_PREFIX + "Layout/" + m_settingsKey + "/Geometry", QByteArray()).toByteArray());
	treeWidget->header()->restoreState(qmc2Config->value(QMC2_FRONTEND_PREFIX + "Layout/" + m_settingsKey + "/HeaderState", QByteArray()).toByteArray());
	checkBoxSelectedDumpsOnly->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + m_settingsKey + "/SelectedDumpsOnly", false).toBool());
	if ( e )
		QDialog::showEvent(e);
}

void MissingDumpsViewer::hideEvent(QHideEvent *e)
{
	closeEvent(0);
	if ( e )
		QDialog::hideEvent(e);
}

void MissingDumpsViewer::closeEvent(QCloseEvent *e)
{
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "Layout/" + m_settingsKey + "/Geometry", saveGeometry());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "Layout/" + m_settingsKey + "/HeaderState", treeWidget->header()->saveState());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + m_settingsKey + "/SelectedDumpsOnly", checkBoxSelectedDumpsOnly->isChecked());
	if ( e )
		QDialog::closeEvent(e);
}
