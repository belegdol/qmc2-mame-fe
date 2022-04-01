#ifndef CUSTOMARTWORK_H
#define CUSTOMARTWORK_H

#include <QString>

#include "imagewidget.h"

class CustomArtwork : public ImageWidget
{
	Q_OBJECT 

	public:
		CustomArtwork(QWidget *parent, QString name, int num);
		~CustomArtwork();

		virtual QString cachePrefix() { return m_cachePrefix; }
		virtual QString imageZip();
		virtual QString imageDir();
		virtual QString imageType() { return m_name; }
		virtual int imageTypeNumeric() { return m_num; }
		virtual bool useZip();
		virtual bool useSevenZip();
		virtual bool useArchive();
		virtual bool scaledImage();
		virtual bool customArtwork() { return true; }
		virtual QString fallbackSettingsKey() { return QString("Artwork/%1/Fallback").arg(name()); }

		QString name() { return m_name; }
		void setName(QString name) { m_name = name; }
		int num() { return m_num; }
		void setNum(int num);

	private:
		QString m_name;
		QString m_cachePrefix;
		int m_num;
};

#endif
