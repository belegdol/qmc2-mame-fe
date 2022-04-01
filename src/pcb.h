#ifndef PCB_H
#define PCB_H

#include "imagewidget.h"

class PCB : public ImageWidget
{
	Q_OBJECT 

	public:
		PCB(QWidget *parent);
		~PCB();

		virtual QString cachePrefix() { return "pcb"; }
		virtual QString imageZip();
		virtual QString imageDir();
		virtual QString imageType() { return tr("PCB"); }
		virtual int imageTypeNumeric() { return QMC2_IMGTYPE_PCB; }
		virtual bool useZip();
		virtual bool useSevenZip();
		virtual bool useArchive();
		virtual bool scaledImage();
		virtual QString fallbackSettingsKey() { return QMC2_EMULATOR_PREFIX + "FilesAndDirectories/PCBFallback"; }
};

#endif
