#include <QMap>
#include <QHash>
#include <QTreeWidgetItem>
#include <QStringList>

#include <algorithm> // std::sort()

#include "settings.h"
#include "demomode.h"
#include "qmc2main.h"
#include "machinelist.h"
#include "macros.h"

extern MainWindow *qmc2MainWindow;
extern QHash<QString, QTreeWidgetItem *> qmc2MachineListItemHash;
extern QString qmc2DemoMachine;
extern QStringList qmc2DemoArgs;
extern bool qmc2ReloadActive;
extern bool qmc2VerifyActive;
extern bool qmc2VerifyTaggedActive;
extern Settings *qmc2Config;
extern MachineList *qmc2MachineList;
extern QHash<QString, QString> qmc2ParentHash;

DemoModeDialog::DemoModeDialog(QWidget *parent)
	: QDialog(parent)
{
	setupUi(this);
	demoModeRunning = false;
	emuProcess = 0;
#if !defined(QMC2_OS_UNIX) && !defined(QMC2_OS_WIN)
	checkBoxEmbedded->setVisible(false);
#endif

	adjustIconSizes();
	clearStatus();

	restoreGeometry(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/Geometry").toByteArray());
	toolButtonSelectC->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/SelectC", true).toBool());
	toolButtonSelectM->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/SelectM", true).toBool());
	toolButtonSelectI->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/SelectI", false).toBool());
	toolButtonSelectN->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/SelectN", false).toBool());
	toolButtonSelectU->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/SelectU", false).toBool());
	checkBoxFullScreen->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/FullScreen", true).toBool());
	checkBoxMaximized->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/Maximized", false).toBool());
#if (defined(QMC2_OS_UNIX) && QT_VERSION < 0x050000) || defined(QMC2_OS_WIN)
	checkBoxEmbedded->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/Embedded", false).toBool());
#endif
	checkBoxTagged->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/Tagged", false).toBool());
	checkBoxFavorites->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/Favorites", false).toBool());
	checkBoxParents->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/Parents", false).toBool());
	checkBoxSequential->setChecked(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/Sequential", false).toBool());
	spinBoxSecondsToRun->setValue(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/SecondsToRun", 60).toInt());
	spinBoxPauseSeconds->setValue(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/PauseSeconds", 2).toInt());
	comboBoxDriverStatus->setCurrentIndex(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/DriverStatus", QMC2_DEMO_MODE_DRV_STATUS_GOOD).toInt());
	lineEditNameFilter->setText(qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/NameFilter", QString()).toString());

	QTimer::singleShot(0, this, SLOT(updateCategoryFilter()));
}

void DemoModeDialog::showEvent(QShowEvent *e)
{
	// try to "grab" the input focus...
	activateWindow();
	setFocus();
}

void DemoModeDialog::closeEvent(QCloseEvent *e)
{
	if ( demoModeRunning )
		pushButtonRunDemo->animateClick();

	saveCategoryFilter();

	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/SelectC", toolButtonSelectC->isChecked());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/SelectM", toolButtonSelectM->isChecked());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/SelectI", toolButtonSelectI->isChecked());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/SelectN", toolButtonSelectN->isChecked());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/SelectU", toolButtonSelectU->isChecked());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/FullScreen", checkBoxFullScreen->isChecked());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/Maximized", checkBoxMaximized->isChecked());
#if (defined(QMC2_OS_UNIX) && QT_VERSION < 0x050000) || defined(QMC2_OS_WIN)
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/Embedded", checkBoxEmbedded->isChecked());
#endif
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/Tagged", checkBoxTagged->isChecked());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/Favorites", checkBoxFavorites->isChecked());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/Parents", checkBoxParents->isChecked());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/Sequential", checkBoxSequential->isChecked());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/SecondsToRun", spinBoxSecondsToRun->value());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/PauseSeconds", spinBoxPauseSeconds->value());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/DriverStatus", comboBoxDriverStatus->currentIndex());
	if ( lineEditNameFilter->text().isEmpty() )
		qmc2Config->remove(QMC2_FRONTEND_PREFIX + "DemoMode/NameFilter");
	else
		qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/NameFilter", lineEditNameFilter->text());
	qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/Geometry", saveGeometry());
}

void DemoModeDialog::saveCategoryFilter()
{
	QStringList excludedCategories;

	if ( listWidgetCategoryFilter->count() == 1 ) {
		excludedCategories = qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/ExcludedCategories", QStringList()).toStringList();
		if ( listWidgetCategoryFilter->item(0)->checkState() == Qt::Checked )
			excludedCategories.removeAll(MachineList::trQuestionMark);
	} else {
		for (int i = 0; i < listWidgetCategoryFilter->count(); i++) {
			QListWidgetItem *item = listWidgetCategoryFilter->item(i);
			if ( item->checkState() == Qt::Unchecked )
				excludedCategories << item->text();
		}
	}

	if ( !excludedCategories.isEmpty() )
		qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/ExcludedCategories", excludedCategories);
	else
		qmc2Config->remove(QMC2_FRONTEND_PREFIX + "DemoMode/ExcludedCategories");
}

void DemoModeDialog::updateCategoryFilter()
{
	QStringList categoryNames;
	foreach (QString *category, qmc2MachineList->categoryHash.values())
		if ( category )
			categoryNames << *category;
	categoryNames.removeDuplicates();
	std::sort(categoryNames.begin(), categoryNames.end(), MainWindow::qStringListLessThan);
	QStringList excludedCategories = qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/ExcludedCategories", QStringList()).toStringList();
	listWidgetCategoryFilter->setUpdatesEnabled(false);
	listWidgetCategoryFilter->clear();
	QListWidgetItem *item = new QListWidgetItem(MachineList::trQuestionMark, listWidgetCategoryFilter);
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
	item->setCheckState(excludedCategories.contains(MachineList::trQuestionMark) ? Qt::Unchecked : Qt::Checked);
	foreach (QString category, categoryNames) {
		if ( !category.isEmpty() ) {
			item = new QListWidgetItem(category, listWidgetCategoryFilter);
			item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
			item->setCheckState(excludedCategories.contains(category) ? Qt::Unchecked : Qt::Checked);
		}
	}
	listWidgetCategoryFilter->setUpdatesEnabled(true);
}

void DemoModeDialog::on_pushButtonRunDemo_clicked()
{
	if ( demoModeRunning ) {
		qmc2MainWindow->log(QMC2_LOG_FRONTEND, tr("demo mode stopped"));
		demoModeRunning = false;
		pushButtonRunDemo->setText(tr("Run &demo"));
		pushButtonRunDemo->setToolTip(tr("Run demo now"));
		qmc2DemoMachine.clear();
		qmc2DemoArgs.clear();
		seqNum = -1;
		if ( emuProcess ) {
			emuProcess->kill(); // terminate() doesn't work with SDL2-MAME/MESS, so we have to use kill() :(
			emuProcess = 0;
		}
		qmc2MainWindow->actionCheckROMs->setEnabled(true);
		qmc2MainWindow->actionPlay->setEnabled(true);
#if (defined(QMC2_OS_UNIX) && QT_VERSION < 0x050000) || defined(QMC2_OS_WIN)
		qmc2MainWindow->actionPlayEmbedded->setEnabled(true);
#endif
		qmc2MainWindow->enableContextMenuPlayActions(true);
		clearStatus();
	} else {
		if ( qmc2ReloadActive ) {
			qmc2MainWindow->log(QMC2_LOG_FRONTEND, tr("please wait for reload to finish and try again"));
			return;
		}
		if ( qmc2VerifyActive || qmc2VerifyTaggedActive ) {
			qmc2MainWindow->log(QMC2_LOG_FRONTEND, tr("please wait for ROM verification to finish and try again"));
			return;
		}
		saveCategoryFilter();
		qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "DemoMode/Sequential", checkBoxSequential->isChecked());
		selectedMachines.clear();
		if ( checkBoxTagged->isChecked() ) {
			foreach (QString game, qmc2MachineListItemHash.keys()) {
				if ( qmc2MachineList->isDevice(game) )
					continue;
				QTreeWidgetItem *gameItem = qmc2MachineListItemHash.value(game);
				if ( !gameItem )
					continue;
				if ( gameItem->checkState(QMC2_MACHINELIST_COLUMN_TAG) == Qt::Checked )
					selectedMachines << game;
			}
		} else if ( checkBoxFavorites->isChecked() ) {
			foreach (QString game, qmc2MachineListItemHash.keys()) {
				if ( qmc2MachineList->isDevice(game) )
					continue;
				QTreeWidgetItem *gameItem = qmc2MachineListItemHash.value(game);
				if ( gameItem ) {
					QList<QListWidgetItem *> favoritesMatches = qmc2MainWindow->listWidgetFavorites->findItems(gameItem->text(QMC2_MACHINELIST_COLUMN_MACHINE), Qt::MatchExactly);
					if ( !favoritesMatches.isEmpty() )
						selectedMachines << game;
				}
			}
		} else {
			QStringList excludedCategories = qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/ExcludedCategories", QStringList()).toStringList();
			int minDrvStatus = comboBoxDriverStatus->currentIndex();
			QString nameFilter = lineEditNameFilter->text();
			QRegExp nameFilterRegExp(nameFilter);
			if ( !nameFilter.isEmpty() && !nameFilterRegExp.isValid() )
				qmc2MainWindow->log(QMC2_LOG_FRONTEND, tr("WARNING: demo mode: the name filter regular expression is invalid"));
			foreach (QString game, qmc2MachineListItemHash.keys()) {
				if ( checkBoxParents->isChecked() )
					if ( !qmc2ParentHash.value(game).isEmpty() )
						continue;
				if ( !nameFilter.isEmpty() )
					if ( game.indexOf(nameFilterRegExp) < 0 )
						continue;
				QString *categoryPtr = qmc2MachineList->categoryHash.value(game);
				QString category;
				if ( categoryPtr )
					category = *categoryPtr;
				else
					category = MachineList::trQuestionMark;
				if ( qmc2MachineList->isDevice(game) || (!qmc2MachineList->categoryHash.isEmpty() && excludedCategories.contains(category)) )
					continue;
				QTreeWidgetItem *gameItem = qmc2MachineListItemHash.value(game);
				if ( !gameItem )
					continue;
				if ( minDrvStatus < QMC2_DEMO_MODE_DRV_STATUS_PRELIMINARY ) {
					QString drvStatus = gameItem->text(QMC2_MACHINELIST_COLUMN_DRVSTAT);
					if ( minDrvStatus == QMC2_DEMO_MODE_DRV_STATUS_IMPERFECT ) {
						if ( drvStatus != tr("good") && drvStatus != tr("imperfect") )
							continue;
					} else {
						if ( drvStatus != tr("good") )
							continue;
					}
				}
				switch ( qmc2MachineList->romState(game) ) {
					case 'C':
						if ( toolButtonSelectC->isChecked() )
							selectedMachines << game;
						break;
					case 'M':
						if ( toolButtonSelectM->isChecked() )
							selectedMachines << game;
						break;
					case 'I':
						if ( toolButtonSelectI->isChecked() )
							selectedMachines << game;
						break;
					case 'N':
						if ( toolButtonSelectN->isChecked() )
							selectedMachines << game;
						break;
					case 'U':
					default:
						if ( toolButtonSelectU->isChecked() )
							selectedMachines << game;
						break;
				}
			}
		}
		if ( selectedMachines.count() > 0 )
			qmc2MainWindow->log(QMC2_LOG_FRONTEND, tr("demo mode started -- %n machine(s) selected by filter", "", selectedMachines.count()));
		else {
			qmc2MainWindow->log(QMC2_LOG_FRONTEND, tr("demo mode cannot start -- no machine selected by filter"));
			return;
		}
		std::sort(selectedMachines.begin(), selectedMachines.end(), MainWindow::qStringListLessThan);
		demoModeRunning = true;
		seqNum = -1;
		pushButtonRunDemo->setText(tr("Stop &demo"));
		pushButtonRunDemo->setToolTip(tr("Stop demo now"));
		qmc2MainWindow->actionCheckROMs->setEnabled(false);
		qmc2MainWindow->actionPlay->setEnabled(false);
#if (defined(QMC2_OS_UNIX) && QT_VERSION < 0x050000) || defined(QMC2_OS_WIN)
		qmc2MainWindow->actionPlayEmbedded->setEnabled(false);
#endif
		qmc2MainWindow->enableContextMenuPlayActions(false);
		QTimer::singleShot(0, this, SLOT(startNextEmu()));
	}
}

void DemoModeDialog::emuStarted()
{
	emuProcess = (QProcess *)sender();
}

void DemoModeDialog::emuFinished(int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/)
{
	// try to "grab" the input focus...
	activateWindow();
	setFocus();

	qmc2DemoArgs.clear();
	qmc2DemoMachine.clear();
	emuProcess = 0;

	if ( demoModeRunning ) {
		clearStatus();
		QTimer::singleShot(spinBoxPauseSeconds->value() * 1000, this, SLOT(startNextEmu()));
	}
}

void DemoModeDialog::startNextEmu()
{
	if ( !demoModeRunning )
		return;

	qmc2DemoArgs.clear();
	emuProcess = 0;
	if ( checkBoxFullScreen->isChecked() )
		qmc2DemoArgs << "disable-window";
	else {
		qmc2DemoArgs << "enable-window";
		if ( checkBoxMaximized->isChecked() )
			qmc2DemoArgs << "enable-maximize";
		else
			qmc2DemoArgs << "disable-maximize";
	}
	if ( qmc2Config->value(QMC2_FRONTEND_PREFIX + "DemoMode/Sequential", false).toBool() ) {
		seqNum++;
		if ( seqNum > selectedMachines.count() - 1 )
			seqNum = 0;
		qmc2DemoMachine = selectedMachines[seqNum];
	} else
		qmc2DemoMachine = selectedMachines[qrand() % selectedMachines.count()];
	QString gameDescription = qmc2MachineListItemHash.value(qmc2DemoMachine)->text(QMC2_MACHINELIST_COLUMN_MACHINE);
	qmc2MainWindow->log(QMC2_LOG_FRONTEND, tr("starting emulation in demo mode for '%1'").arg(gameDescription));
	setStatus(gameDescription);
#if (defined(QMC2_OS_UNIX) && QT_VERSION < 0x050000) || defined(QMC2_OS_WIN)
	if ( checkBoxEmbedded->isChecked() && !checkBoxFullScreen->isChecked() )
		QTimer::singleShot(0, qmc2MainWindow, SLOT(on_actionPlayEmbedded_triggered(bool)));
	else
		QTimer::singleShot(0, qmc2MainWindow, SLOT(on_actionPlay_triggered(bool)));
#else
	QTimer::singleShot(0, qmc2MainWindow, SLOT(on_actionPlay_triggered(bool)));
#endif
}

void DemoModeDialog::adjustIconSizes()
{
	QFontMetrics fm(qApp->font());
	QSize iconSize(fm.height() - 2, fm.height() - 2);

	toolButtonSelectC->setIconSize(iconSize);
	toolButtonSelectM->setIconSize(iconSize);
	toolButtonSelectI->setIconSize(iconSize);
	toolButtonSelectN->setIconSize(iconSize);
	toolButtonSelectU->setIconSize(iconSize);
	toolButtonSelectAll->setIconSize(iconSize);
	toolButtonDeselectAll->setIconSize(iconSize);
	toolButtonClearNameFilter->setIconSize(iconSize);

	pushButtonRunDemo->setMinimumHeight(fm.height() * 3);

	adjustSize();
}

void DemoModeDialog::setStatus(QString statusString)
{
	if ( statusString.isEmpty() ) {
		labelDemoStatus->clear();
		labelDemoStatus->hide();
	} else {
		labelDemoStatus->setText("<font size=\"+1\"><b>" + statusString + "</b></font>");
		labelDemoStatus->show();
	}
	adjustSize();
}

void DemoModeDialog::on_toolButtonSelectAll_clicked()
{
	for (int i = 0; i < listWidgetCategoryFilter->count(); i++) {
		QListWidgetItem *item = listWidgetCategoryFilter->item(i);
		item->setCheckState(Qt::Checked);
	}
}

void DemoModeDialog::on_toolButtonDeselectAll_clicked()
{
	for (int i = 0; i < listWidgetCategoryFilter->count(); i++) {
		QListWidgetItem *item = listWidgetCategoryFilter->item(i);
		item->setCheckState(Qt::Unchecked);
	}
}

void DemoModeDialog::enableFilters(bool enable)
{
	checkBoxParents->setEnabled(enable);
	toolButtonSelectC->setEnabled(enable);
	toolButtonSelectM->setEnabled(enable);
	toolButtonSelectI->setEnabled(enable);
	toolButtonSelectN->setEnabled(enable);
	toolButtonSelectU->setEnabled(enable);
	comboBoxDriverStatus->setEnabled(enable);
	lineEditNameFilter->setEnabled(enable);
	toolButtonClearNameFilter->setEnabled(enable);
	toolButtonSelectAll->setEnabled(enable);
	toolButtonDeselectAll->setEnabled(enable);
	listWidgetCategoryFilter->setEnabled(enable);
}

void DemoModeDialog::on_checkBoxTagged_toggled(bool enable)
{
	if ( enable )
		checkBoxFavorites->setChecked(false);
	enableFilters(!enable);
}

void DemoModeDialog::on_checkBoxFavorites_toggled(bool enable)
{
	if ( enable )
		checkBoxTagged->setChecked(false);
	enableFilters(!enable);
}
