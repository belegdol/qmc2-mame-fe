#include <QAbstractItemView>
#include <QTreeWidgetItem>
#include <QApplication>
#include <QMultiMap>
#include <QTimer>

#include "machinelistmodel.h"
#include "machinelistviewer.h"
#include "machinelist.h"
#include "filterconfigurationdialog.h"

extern MachineList *qmc2MachineList;
extern QHash<QString, QIcon> qmc2IconHash;
extern QHash<QString, QTreeWidgetItem *> qmc2MachineListItemHash;
extern QAbstractItemView::ScrollHint qmc2CursorPositioningMode;

#define ml	qmc2MachineList
#define udDb	ml->userDataDb()

MachineListModelItem::MachineListModelItem(const QString &id, const QIcon &icon, const QString &parent, const QString &description, const QString &manufacturer, const QString &year, const QString &source_file, int players, const QString &category, const QString &version, int rank, char rom_status, bool has_roms, bool has_chds, const QString &driver_status, bool is_device, bool is_bios, bool tagged, QTreeView *treeView, MachineListModelItem *parentItem) :
	m_parentItem(parentItem),
	m_treeView(treeView)
{
	setId(id);
	setParent(parent);
	setDescription(description);
	setManufacturer(manufacturer);
	setYear(year);
	setSourceFile(source_file);
	setPlayers(players);
	setCategory(category);
	setVersion(version);
	setIcon(icon);
	setRank(rank);
	setRomStatus(rom_status);
	setHasRoms(has_roms);
	setHasChds(has_chds);
	setDriverStatus(driver_status);
	setIsDevice(is_device);
	setIsBios(is_bios);
	setTagged(tagged);
}

MachineListModelItem::MachineListModelItem(QTreeView *treeView, MachineListModelItem *parentItem) :
	m_parentItem(parentItem),
	m_treeView(treeView)
{
	setRank(0);
	setRomStatus('U');
	setDriverStatus(QObject::tr("unknown"));
	setIsDevice(false);
	setIsBios(false);
	setHasRoms(false);
	setHasChds(false);
	setTagged(false);
}

MachineListModelItem::~MachineListModelItem()
{
	qDeleteAll(childItems());
}

int MachineListModelItem::row() const
{
	if ( m_parentItem )
		return m_parentItem->childItems().indexOf(const_cast<MachineListModelItem *>(this));
        return 0;
}

MachineListModel::MachineListModel(QTreeView *treeView, QObject *parent) :
	QAbstractItemModel(parent),
	m_rootItem(0),
	m_oldRootItem(0),
	m_recordCount(0),
	m_treeView(treeView),
	m_firstQuery(true),
	m_queryActive(false)
{
	m_headers << tr("Tag")
		  << tr("Icon")
		  << tr("Name")
		  << tr("Parent")
		  << tr("Description")
		  << tr("Manufacturer")
		  << tr("Year")
		  << tr("ROM status")
		  << tr("Has ROMs?")
		  << tr("Has CHDs?")
		  << tr("Driver status")
		  << tr("Source file")
		  << tr("Players")
		  << tr("Rank")
		  << tr("Is BIOS?")
		  << tr("Is device?")
		  << tr("Category")
		  << tr("Version");
	m_pages << QMC2_FCDLG_PAGE_BOOL
		<< QMC2_FCDLG_PAGE_ICON
		<< QMC2_FCDLG_PAGE_STRING
		<< QMC2_FCDLG_PAGE_STRING
		<< QMC2_FCDLG_PAGE_STRING
		<< QMC2_FCDLG_PAGE_STRING
		<< QMC2_FCDLG_PAGE_STRING
		<< QMC2_FCDLG_PAGE_ROMSTATUS
		<< QMC2_FCDLG_PAGE_BOOL
		<< QMC2_FCDLG_PAGE_BOOL
		<< QMC2_FCDLG_PAGE_DRVSTATUS
		<< QMC2_FCDLG_PAGE_STRING
		<< QMC2_FCDLG_PAGE_INT
		<< QMC2_FCDLG_PAGE_RANK
		<< QMC2_FCDLG_PAGE_BOOL
		<< QMC2_FCDLG_PAGE_BOOL
		<< QMC2_FCDLG_PAGE_STRING
		<< QMC2_FCDLG_PAGE_STRING;
	setRootItem(new MachineListModelItem(m_treeView));
	connectToDb();
}

MachineListModel::~MachineListModel()
{
	delete rootItem();
	disconnectFromDb();
}

void MachineListModel::setRootItem(MachineListModelItem *item)
{
	m_rootItem = item;
}

void MachineListModel::startQuery()
{
	m_queryActive = true;
	machineListDb()->queryRecords(m_query);
	while ( canFetchMore(QModelIndex()) )
		fetchMore(QModelIndex());
	if ( m_firstQuery )
		QTimer::singleShot(0, (MachineListViewer *)QObject::parent(), SLOT(updateCurrentView()));
	m_firstQuery = m_queryActive = false;
}

void MachineListModel::resetModel()
{
	m_oldRootItem = rootItem();
	beginResetModel();
#if QT_VERSION < 0x050000
	reset();
#endif
	m_recordCount = 0;
	setRootItem(new MachineListModelItem(treeView()));
	endResetModel();
	startQuery();
	deleteOldRoot();
}

void MachineListModel::deleteOldRoot()
{
	if ( m_oldRootItem )
		delete m_oldRootItem;
	m_oldRootItem = 0;
}

QModelIndex MachineListModel::index(int row, int column, const QModelIndex &parent) const
{
	if ( parent.isValid() && parent.column() != int(ID) )
		return QModelIndex();
	MachineListModelItem *childItem = itemFromIndex(parent)->childItems().at(row);
	if ( childItem )
		return createIndex(row, column, childItem);
	else
		return QModelIndex();
}

QVariant MachineListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ( orientation == Qt::Horizontal ) {
		switch ( role ) {
			case Qt::DisplayRole:
				return m_headers.at(section);
			case Qt::TextAlignmentRole:
				return Qt::AlignLeft;
		}
	}
	return QVariant();
}

bool MachineListModel::canFetchMore(const QModelIndex &parent) const
{
	return m_recordCount < ml->numMachines;
}

void MachineListModel::fetchMore(const QModelIndex &parent)
{
	MachineListModelItem *parentItem = itemFromIndex(parent);
	if ( !parentItem )
		return;
	int itemsToFetch = QMC2_MIN(QMC2_MLM_FETCH_ONCE, ml->numMachines - m_recordCount);
	QString id, description, manufacturer, year, cloneof, drvstat, srcfile;
	bool is_bios, is_device, has_roms, has_chds;
	int players, i = 0;
	beginInsertRows(QModelIndex(), m_recordCount, m_recordCount + itemsToFetch - 1);
	while ( i < itemsToFetch && machineListDb()->nextRecord(m_query, &id, &description, &manufacturer, &year, &cloneof, &is_bios, &is_device, &has_roms, &has_chds, &players, &drvstat, &srcfile) ) {
		QString *category = ml->categoryHash.value(id);
		QString *version = ml->versionHash.value(id);
		MachineListItem *mlItem = (MachineListItem *)qmc2MachineListItemHash.value(id);
		MachineListModelItem *mlmItem = new MachineListModelItem(id,
									 qmc2IconHash.value(id),
									 cloneof,
									 description,
									 manufacturer,
									 year,
									 srcfile,
									 players,
									 category ? *category : MachineList::trQuestionMark,
									 version ? *version : MachineList::trQuestionMark,
									 udDb->rankCache().value(id),
									 ml->romState(id),
									 has_roms,
									 has_chds,
									 drvstat,
									 is_device,
									 is_bios,
									 mlItem->tagged(),
									 treeView(),
									 rootItem());
		parentItem->childItems().append(mlmItem);
		itemHash().insert(id, mlmItem);
		i++;
	}
	m_recordCount += itemsToFetch;
        endInsertRows();
}

Qt::ItemFlags MachineListModel::flags(const QModelIndex &index) const
{
	return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
}

int MachineListModel::columnCount(const QModelIndex &) const
{
	return int(LAST_COLUMN);
}

int MachineListModel::rowCount(const QModelIndex &parent) const
{
	if ( !parent.isValid() )
		return m_rootItem->childItems().count();
        MachineListModelItem *parentItem = itemFromIndex(parent);
	if ( !parentItem )
		return 0;
	return parentItem->childItems().count();
}

QVariant MachineListModel::data(const QModelIndex &index, int role) const
{
	if ( !index.isValid() )
		return QVariant();
	MachineListModelItem *item = itemFromIndex(index);
	if ( !item )
		return QVariant();
	int machineType = int(item->isBios()) + int(item->isDevice()) * 2; // 0: normal, 1: BIOS, 2: device
	switch ( role ) {
		case Qt::TextAlignmentRole:
			return Qt::AlignLeft;
		case Qt::CheckStateRole:
			switch ( Column(index.column()) ) {
				case TAG:
					return item->tagged() ? Qt::Checked : Qt::Unchecked;
				default:
					return QVariant();
			}
		case Qt::DisplayRole:
			switch ( Column(index.column()) ) {
				case ID:
					return item->id();
				case PARENT:
					return item->parent();
				case DESCRIPTION:
					return item->description();
				case MANUFACTURER:
					return item->manufacturer();
				case YEAR:
					return item->year();
				case ROM_STATUS:
					return ml->romStatus(item->id(), true);
				case HAS_ROMS:
					return item->hasRoms() ? tr("true") : tr("false");
				case HAS_CHDS:
					return item->hasChds() ? tr("true") : tr("false");
				case DRIVER_STATUS:
					return item->driverStatus();
				case SOURCE_FILE:
					return item->sourceFile();
				case PLAYERS:
					return item->players();
				case IS_BIOS:
					return item->isBios() ? tr("true") : tr("false");
				case IS_DEVICE:
					return item->isDevice() ? tr("true") : tr("false");
				case RANK:
					return item->rank();
				case CATEGORY:
					return item->category();
				case VERSION:
					return item->version();
				default:
					return QVariant();
			}
		case Qt::DecorationRole:
			switch ( Column(index.column()) ) {
				// FIXME
				case ICON:
					if ( !item->icon().isNull() )
						return item->icon();
					else if ( !item->parent().isEmpty() )
						return qmc2IconHash.value(item->parent());
					else
						return QVariant();
				case ROM_STATUS:
					switch ( item->romStatus() ) {
						case 'C':
							switch ( machineType ) {
								case QMC2_MACHINETYPE_NORMAL:
									return ml->qmc2CorrectImageIcon;
								case QMC2_MACHINETYPE_BIOS:
									return ml->qmc2CorrectBIOSImageIcon;
								case QMC2_MACHINETYPE_DEVICE:
									return ml->qmc2CorrectDeviceImageIcon;
							}
						case 'M':
							switch ( machineType ) {
								case QMC2_MACHINETYPE_NORMAL:
									return ml->qmc2MostlyCorrectImageIcon;
								case QMC2_MACHINETYPE_BIOS:
									return ml->qmc2MostlyCorrectBIOSImageIcon;
								case QMC2_MACHINETYPE_DEVICE:
									return ml->qmc2MostlyCorrectDeviceImageIcon;
							}
						case 'I':
							switch ( machineType ) {
								case QMC2_MACHINETYPE_NORMAL:
									return ml->qmc2IncorrectImageIcon;
								case QMC2_MACHINETYPE_BIOS:
									return ml->qmc2IncorrectBIOSImageIcon;
								case QMC2_MACHINETYPE_DEVICE:
									return ml->qmc2IncorrectDeviceImageIcon;
							}
						case 'N':
							switch ( machineType ) {
								case QMC2_MACHINETYPE_NORMAL:
									return ml->qmc2NotFoundImageIcon;
								case QMC2_MACHINETYPE_BIOS:
									return ml->qmc2NotFoundBIOSImageIcon;
								case QMC2_MACHINETYPE_DEVICE:
									return ml->qmc2NotFoundDeviceImageIcon;
							}
						case 'U':
						default:
							switch ( machineType ) {
								case QMC2_MACHINETYPE_NORMAL:
									return ml->qmc2UnknownImageIcon;
								case QMC2_MACHINETYPE_BIOS:
									return ml->qmc2UnknownBIOSImageIcon;
								case QMC2_MACHINETYPE_DEVICE:
									return ml->qmc2UnknownDeviceImageIcon;
							}
					}
				default:
					return QVariant();
			}
	}
	return QVariant();
}

QModelIndex MachineListModel::parent(const QModelIndex &child) const
{
	if ( !child.isValid() )
		return QModelIndex();
	MachineListModelItem *childItem = itemFromIndex(child);
	if ( !childItem )
		return QModelIndex();
	if ( childItem->parentItem() == m_rootItem )
		return QModelIndex();
	return createIndex(childItem->row(), 0, childItem);
}

void MachineListModel::sort(int column, Qt::SortOrder order)
{
	QModelIndexList selectedIndexes(treeView()->selectionModel()->selectedIndexes());
	MachineListModelItem *selectedItem = 0;
	if ( !selectedIndexes.isEmpty() )
		selectedItem = itemFromIndex(selectedIndexes.at(0));
	emit layoutAboutToBeChanged();
	switch ( Column(column) ) {
		case TAG:
			{
				QMultiMap<bool, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->tagged(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case ICON:
			{
				QMultiMap<qint64, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->icon().cacheKey(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case ID:
			{
				QMultiMap<QString, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->id(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case PARENT:
			{
				QMultiMap<QString, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->parent(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case DESCRIPTION:
			{
				QMultiMap<QString, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->description(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case MANUFACTURER:
			{
				QMultiMap<QString, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->manufacturer(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case YEAR:
			{
				QMultiMap<QString, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->year(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case ROM_STATUS:
			{
				QMultiMap<char, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->romStatus(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case HAS_ROMS:
			{
				QMultiMap<bool, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->hasRoms(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case HAS_CHDS:
			{
				QMultiMap<bool, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->hasChds(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case DRIVER_STATUS:
			{
				QMultiMap<QString, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->driverStatus(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case SOURCE_FILE:
			{
				QMultiMap<QString, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->sourceFile(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case PLAYERS:
			{
				QMultiMap<int, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->players(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case RANK:
			{
				QMultiMap<int, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->rank(), item);
				rootItem()->childItems() = map.values();
				toggleSortOrder(order);
			}
			break;
		case IS_BIOS:
			{
				QMultiMap<bool, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->isBios(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case IS_DEVICE:
			{
				QMultiMap<bool, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->isDevice(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case CATEGORY:
			{
				QMultiMap<QString, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->category(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		case VERSION:
			{
				QMultiMap<QString, MachineListModelItem *> map;
				foreach (MachineListModelItem *item, rootItem()->childItems())
					map.insert(item->version(), item);
				rootItem()->childItems() = map.values();
			}
			break;
		default:
			break;
	}
	if ( order == Qt::DescendingOrder )
		for (int k = 0; k < rootItem()->childItems().size() / 2; k++)
			rootItem()->childItems().swap(k, rootItem()->childItems().size() - (1 + k));
	emit layoutChanged();
	if ( selectedItem ) {
		int row = rootItem()->childItems().indexOf(selectedItem);
		if ( row >= 0 ) {
			QModelIndex idx(index(row, 0, QModelIndex()));
			treeView()->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
			treeView()->scrollTo(idx, qmc2CursorPositioningMode);
		}
	}
}

MachineListModelItem *MachineListModel::itemFromIndex(const QModelIndex &index) const
{
	if ( index.isValid() ) {
		MachineListModelItem *item = static_cast<MachineListModelItem *>(index.internalPointer());
		if ( item )
			return item;
	}
	return m_rootItem;
}

void MachineListModel::connectToDb()
{
	m_machineListDb = new MachineListDatabaseManager(this);
	machineListDb()->setSyncMode(QMC2_DB_SYNC_MODE_OFF);
	machineListDb()->setJournalMode(QMC2_DB_JOURNAL_MODE_MEMORY);
	m_query = new QSqlQuery(machineListDb()->db());
	QTimer::singleShot(0, this, SLOT(startQuery()));
}

void MachineListModel::disconnectFromDb()
{
	while ( m_queryActive )
		qApp->processEvents(QEventLoop::AllEvents, 10);
	m_query->clear();
	delete m_query;
	delete machineListDb();
}
