#ifndef EMUOPT_H
#define EMUOPT_H

#include <QStyledItemDelegate>
#include <QModelIndex>
#include <QCheckBox>
#include <QTreeWidget>
#include <QMap>
#include <QHash>
#include <QTimer>
#include <QTime>
#include <QKeyEvent>
#include <QXmlStreamReader>

#include "settings.h"
#include "macros.h"
#include "iconlineedit.h"

class EmulatorOptionDelegate : public QStyledItemDelegate
{
	Q_OBJECT

	public:
		EmulatorOptionDelegate(QTreeWidget *treeWidget = 0, QObject *parent = 0);

		virtual QWidget *createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const;
		virtual void setEditorData(QWidget *, const QModelIndex &) const;
		virtual void setModelData(QWidget *, QAbstractItemModel *, const QModelIndex &) const;
		virtual void updateEditorGeometry(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const;
		virtual void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const;

		static QString boolToString(bool b) { return (b ? QString("true") : QString("false")); }
		static bool stringToBool(QString s) { return (s == "true" ? true : false); }

	public slots:
		void dataChanged();

	signals:
		void editorDataChanged(QWidget *, QTreeWidgetItem *) const;

	private:
		QTreeWidget *mTreeWidget;
};

class EmulatorOption
{
	public:
		QString name, dvalue, description, value, part, relativeTo;
		QStringList choices;
		bool valid, visible, wip;
		int type;
		int decimals;
		QTreeWidgetItem *item;

		EmulatorOption() { valid = false; }
		EmulatorOption(const QString &n, const QString &t, const QString &dv, const QString &d, const QString &v, const QString &p, QTreeWidgetItem *i, bool va, int dec = QMC2_EMUOPT_DFLT_DECIMALS, QStringList c = QStringList(), bool vis = true, QString relTo = QString(), bool isWip = false) {
			name = n;
			type = QMC2_EMUOPT_TYPE_UNKNOWN;
			switch ( t.at(0).toLatin1() ) {
				case 'b':
					type = QMC2_EMUOPT_TYPE_BOOL;
					break;
				case 'i':
					type = QMC2_EMUOPT_TYPE_INT;
					break;
				case 'f':
					if ( t == "float" || t == "float1" )
						type = QMC2_EMUOPT_TYPE_FLOAT;
					else if ( t == "file" )
						type = QMC2_EMUOPT_TYPE_FILE;
					else if ( t == "float2" )
						type = QMC2_EMUOPT_TYPE_FLOAT2;
					else if ( t == "float3" )
						type = QMC2_EMUOPT_TYPE_FLOAT3;
					break;
				case 's':
					type = QMC2_EMUOPT_TYPE_STRING;
					break;
				case 'd':
					type = QMC2_EMUOPT_TYPE_DIRECTORY;
					break;
				case 'c':
					if ( t == "color" )
						type = QMC2_EMUOPT_TYPE_COLOR;
					else
						type = QMC2_EMUOPT_TYPE_COMBO;
					break;
				default:
					type = QMC2_EMUOPT_TYPE_UNKNOWN;
					break;
			}
			dvalue = dv;
			description = d;
			value = v;
			part = p;
			item = i;
			valid = va;
			choices = c;
			visible = vis;
			decimals = dec;
			relativeTo = relTo;
			wip = isWip;
		}
		static bool lessThan(const EmulatorOption &a, const EmulatorOption &b) {
			return a.name.compare(b.name) < 0;
		}
};

class EmulatorOptions : public QTreeWidget
{
	Q_OBJECT

	public:
		QTimer searchTimer;
		QTime miscTimer;
		IconLineEdit *lineEditSearch;
		EmulatorOptionDelegate *delegate;
		QString settingsGroup;
		QString templateEmulator;
		QString templateVersion;
		QString templateFormat;
		QString translatedDescription;
		QMap<QString, QList<EmulatorOption> > optionsMap;
		QStringList ignoredOptions;
		QStringList validChoices;
		static QMap<QString, QList<EmulatorOption> > templateMap;
		static QMap<QString, bool> sectionExpansionMap;
		static QMap<QString, QTreeWidgetItem *> sectionItemMap;
		static QMap<QString, bool> optionExpansionMap;
		static QHash<QString, int> typeNameToIndexHash;
		static int horizontalScrollPosition;
		static int verticalScrollPosition;
		bool loadActive;
		bool changed;
		bool isGlobal;

		EmulatorOptions(const QString &, QWidget *parent = 0);
		~EmulatorOptions();

		QString &readDescription(QXmlStreamReader *, QString &, bool *);
		QStringList &readChoices(QXmlStreamReader *);

		QTreeWidgetItem *index2item(const QModelIndex &index) const { return itemFromIndex(index); }
		QModelIndex item2index(QTreeWidgetItem *item, int column) const { return indexFromItem(item, column); }

	public slots:
		void load(bool overwrite = false, QString optName = QString());
		void save(QString optName = QString());
		void restoreHeaderState();
		void saveHeaderState();
		void addChoices(QString, QStringList, QStringList, QString defaultChoice = QString(), bool sort = true);
		void createTemplateMap();
		void checkTemplateMap();
		void createMap();
		void searchTimeout();
		void exportToIni(bool global, QString useFileName = QString());
		void importFromIni(bool global, QString useFileName = QString());
		void adjustIconSizes();
		void updateEmuOptActions(QWidget *, QTreeWidgetItem *);
		void updateAllEmuOptActions();

	protected:
		virtual void keyPressEvent(QKeyEvent *);
};

#endif
