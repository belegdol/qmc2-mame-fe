#ifndef SOFTWARESNAPSHOT_H
#define SOFTWARESNAPSHOT_H

#include "softwareimagewidget.h"

class SoftwareSnapshot : public SoftwareImageWidget
{
	Q_OBJECT

	public:
		SoftwareSnapshot(QWidget *parent = 0);
		~SoftwareSnapshot();

		virtual QString cachePrefix() { return "sws"; }
		virtual QString imageZip();
		virtual QString imageDir();
		virtual QString imageType() { return tr("software snapshot"); }
		virtual int imageTypeNumeric() { return QMC2_IMGTYPE_SWSNAP; }
		virtual bool useZip();
		virtual bool useSevenZip();
		virtual bool useArchive();
		virtual bool scaledImage();
		virtual QString fallbackSettingsKey() { return QMC2_EMULATOR_PREFIX + "FilesAndDirectories/SoftwareSnapFallback"; }
};

#endif
