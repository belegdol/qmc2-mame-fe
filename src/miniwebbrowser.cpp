#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QApplication>
#include <QScreen>
#include <QDir>
#include <QWebEngineHistory>
#include <QWebEngineSettings>
#include <QFontMetrics>

#include "settings.h"
#include "miniwebbrowser.h"
#include "macros.h"
#include "qmc2main.h"
#include "options.h"
#include "networkaccessmanager.h"

extern MainWindow *qmc2MainWindow;
extern NetworkAccessManager *qmc2NetworkAccessManager;
extern Settings *qmc2Config;

QCache<QString, QIcon> MiniWebBrowser::iconCache;
QStringList MiniWebBrowser::supportedSchemes;

MiniWebBrowser::MiniWebBrowser(QWidget *parent, bool useAsPdfViewer)
	: QWidget(parent)
{
	setObjectName("MiniWebBrowser");
	m_isPdfViewer = useAsPdfViewer;

	if ( MiniWebBrowser::supportedSchemes.isEmpty() )
		MiniWebBrowser::supportedSchemes << "http" << "ftp" << "file";

	setupUi(this);

	currentTitle = "QMC2_NO_TITLE";
	changeTitle(currentTitle);

	webViewBrowser = new BrowserWidget(frameBrowser, this);
	webViewBrowser->setObjectName("webViewBrowser");
	verticalLayout->addWidget(webViewBrowser);

	labelStatus->hide();
	progressBar->hide();

	horizontalLayoutFrameSearch->removeWidget(lineEditSearch);
	delete lineEditSearch;
	iconLineEditSearch = new IconLineEdit(QIcon(QString::fromUtf8(":/data/img/find.png")), QMC2_ALIGN_LEFT, this);
	connect(iconLineEditSearch, SIGNAL(returnPressed()), toolButtonNext, SLOT(animateClick()));
	connect(iconLineEditSearch, SIGNAL(textEdited(const QString &)), this, SLOT(startSearchTimer()));
	connect(&searchTimer, SIGNAL(timeout()), this, SLOT(on_toolButtonNext_clicked()));
	horizontalLayoutFrameSearch->insertWidget(0, iconLineEditSearch);
	iconLineEditSearch->setToolTip(tr("Enter search string"));
	iconLineEditSearch->setPlaceholderText(tr("Enter search string"));
	frameSearch->hide();

	comboBoxURL->lineEdit()->setPlaceholderText(tr("Enter URL"));

	firstTimeLoadStarted = firstTimeLoadProgress = firstTimeLoadFinished = true;

	iconCache.setMaxCost(QMC2_BROWSER_ICONCACHE_SIZE);

	// we want the same global network access manager for all browsers
	//webViewBrowser->page()->setNetworkAccessManager(qmc2NetworkAccessManager);

	// we want to manipulate the link activation
	//webViewBrowser->page()->setLinkDelegationPolicy(QWebEnginePage::DelegateAllLinks);

	// connect page actions we provide
	connect(webViewBrowser->page(), SIGNAL(downloadRequested(const QNetworkRequest &)), this, SLOT(processPageActionDownloadRequested(const QNetworkRequest &)));
	connect(webViewBrowser->page(), SIGNAL(unsupportedContent(QNetworkReply *)), this, SLOT(processPageActionHandleUnsupportedContent(QNetworkReply *)));
	connect(webViewBrowser->page(), SIGNAL(linkHovered(const QString &, const QString &, const QString &)), this, SLOT(webViewBrowser_linkHovered(const QString &, const QString &, const QString &)));
	connect(webViewBrowser->page(), SIGNAL(statusBarVisibilityChangeRequested(bool)), this, SLOT(webViewBrowser_statusBarVisibilityChangeRequested(bool)));

	connect(webViewBrowser, SIGNAL(linkClicked(const QUrl)), this, SLOT(webViewBrowser_linkClicked(const QUrl)));
	connect(webViewBrowser, SIGNAL(urlChanged(const QUrl)), this, SLOT(webViewBrowser_urlChanged(const QUrl)));
	connect(webViewBrowser, SIGNAL(loadStarted()), this, SLOT(webViewBrowser_loadStarted()));
	connect(webViewBrowser, SIGNAL(loadFinished(bool)), this, SLOT(webViewBrowser_loadFinished(bool)));
	connect(webViewBrowser, SIGNAL(loadProgress(int)), this, SLOT(webViewBrowser_loadProgress(int)));
	connect(webViewBrowser, SIGNAL(statusBarMessage(const QString &)), this, SLOT(webViewBrowser_statusBarMessage(const QString &)));
	connect(webViewBrowser, SIGNAL(iconChanged()), this, SLOT(webViewBrowser_iconChanged()));
	connect(toolButtonReload, SIGNAL(clicked()), webViewBrowser, SLOT(reload()));
	connect(toolButtonStop, SIGNAL(clicked()), webViewBrowser, SLOT(stop()));

	if ( isPdfViewer() ) {
		frameUrl->hide();
		// hide all page actions
		/*
		for (QWebEnginePage::WebAction pa = QWebEnginePage::Back; pa != QWebEnginePage::InsertUnorderedList; pa = static_cast<QWebEnginePage::WebAction>(static_cast<int>(pa) + 1)) {
			QAction *a= webViewBrowser->pageAction(pa);
			if ( a )
				a->setVisible(false);
		}
		*/
	} else {
		/* 
		// hide page actions we don't provide
		webViewBrowser->pageAction(QWebEnginePage::OpenFrameInNewWindow)->setVisible(false);

		// change provided page actions to better fit our usage / integrate into QMC2's look
		webViewBrowser->pageAction(QWebEnginePage::OpenLink)->setText(tr("Open link"));
		webViewBrowser->pageAction(QWebEnginePage::OpenLink)->setIcon(QIcon(QString::fromUtf8(":/data/img/fileopen.png")));
		webViewBrowser->pageAction(QWebEnginePage::OpenLinkInNewWindow)->setText(tr("Open link in new window"));
		webViewBrowser->pageAction(QWebEnginePage::OpenLinkInNewWindow)->setIcon(QIcon(QString::fromUtf8(":/data/img/browser.png")));
		webViewBrowser->pageAction(QWebEnginePage::OpenImageInNewWindow)->setText(tr("Open image in new window"));
		webViewBrowser->pageAction(QWebEnginePage::OpenImageInNewWindow)->setIcon(QIcon(QString::fromUtf8(":/data/img/thumbnail.png")));
		webViewBrowser->pageAction(QWebEnginePage::DownloadLinkToDisk)->setText(tr("Save link as..."));
		webViewBrowser->pageAction(QWebEnginePage::DownloadLinkToDisk)->setIcon(QIcon(QString::fromUtf8(":/data/img/filesaveas.png")));
		webViewBrowser->pageAction(QWebEnginePage::CopyLinkToClipboard)->setText(tr("Copy link"));
		webViewBrowser->pageAction(QWebEnginePage::CopyLinkToClipboard)->setIcon(QIcon(QString::fromUtf8(":/data/img/editcopy.png")));
		webViewBrowser->pageAction(QWebEnginePage::DownloadImageToDisk)->setText(tr("Save image as..."));
		webViewBrowser->pageAction(QWebEnginePage::DownloadImageToDisk)->setIcon(QIcon(QString::fromUtf8(":/data/img/filesaveas.png")));
		webViewBrowser->pageAction(QWebEnginePage::CopyImageToClipboard)->setText(tr("Copy image"));
		webViewBrowser->pageAction(QWebEnginePage::CopyImageToClipboard)->setIcon(QIcon(QString::fromUtf8(":/data/img/editcopy.png")));
		webViewBrowser->pageAction(QWebEnginePage::CopyImageUrlToClipboard)->setText(tr("Copy image address"));
		webViewBrowser->pageAction(QWebEnginePage::Back)->setText(tr("Go back"));
		webViewBrowser->pageAction(QWebEnginePage::Back)->setIcon(QIcon(QString::fromUtf8(":/data/img/back.png")));
		webViewBrowser->pageAction(QWebEnginePage::Forward)->setText(tr("Go forward"));
		webViewBrowser->pageAction(QWebEnginePage::Forward)->setIcon(QIcon(QString::fromUtf8(":/data/img/forward.png")));
		webViewBrowser->pageAction(QWebEnginePage::Reload)->setText(tr("Reload"));
		webViewBrowser->pageAction(QWebEnginePage::Reload)->setIcon(QIcon(QString::fromUtf8(":/data/img/reload.png")));
		webViewBrowser->pageAction(QWebEnginePage::Stop)->setText(tr("Stop"));
		webViewBrowser->pageAction(QWebEnginePage::Stop)->setIcon(QIcon(QString::fromUtf8(":/data/img/stop_browser.png")));
		webViewBrowser->pageAction(QWebEnginePage::Copy)->setText(tr("Copy"));
		webViewBrowser->pageAction(QWebEnginePage::Copy)->setIcon(QIcon(QString::fromUtf8(":/data/img/editcopy.png")));
#if defined(QMC2_BROWSER_EXTRAS_ENABLED)
		webViewBrowser->pageAction(QWebEnginePage::InspectElement)->setText(tr("Inspect"));
		webViewBrowser->pageAction(QWebEnginePage::InspectElement)->setIcon(QIcon(QString::fromUtf8(":/data/img/inspect.png")));
#endif
		*/

		// connect page actions to own routines
		connect(webViewBrowser->pageAction(QWebEnginePage::Back), SIGNAL(triggered()), this, SLOT(checkBackAndForward()));
		connect(webViewBrowser->pageAction(QWebEnginePage::Forward), SIGNAL(triggered()), this, SLOT(checkBackAndForward()));
	}

	// setup browser settings
	/*
	webViewBrowser->page()->settings()->setIconDatabasePath(Options::configPath());
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::AutoLoadImages, true);
#if defined(QMC2_BROWSER_JAVASCRIPT_ENABLED)
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
#else
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
#endif
#if defined(QMC2_BROWSER_JAVA_ENABLED)
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::JavaEnabled, true);
#else
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::JavaEnabled, false);
#endif
#if defined(QMC2_BROWSER_PLUGINS_ENABLED)
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
#else
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, false);
#endif
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::PrivateBrowsingEnabled, false);
#if defined(QMC2_BROWSER_EXTRAS_ENABLED)
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::DeveloperExtrasEnabled, true);
#else
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::DeveloperExtrasEnabled, false);
#endif
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::LinksIncludedInFocusChain, false);
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::ZoomTextOnly, false);
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::PrintElementBackgrounds, false);
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::OfflineStorageDatabaseEnabled, false);
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::OfflineWebApplicationCacheEnabled, false);
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
#if defined(QMC2_BROWSER_PREFETCH_DNS_ENABLED)
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);
#else
	webViewBrowser->page()->settings()->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, false);
#endif
	*/

	connect(this, SIGNAL(titleChanged(QString &)), this, SLOT(changeTitle(QString &)));

#if defined(QMC2_BROWSER_EXTRAS_ENABLED)
	connect(webViewBrowser->pageAction(QWebEnginePage::InspectElement), SIGNAL(triggered()), this, SLOT(postProcessPageActionInspect()), Qt::QueuedConnection);
#endif

	// we want to detect/handle unsupported content
	//webViewBrowser->page()->setForwardUnsupportedContent(true);

	// status bar timeout connection
	connect(&statusTimer, SIGNAL(timeout()), this, SLOT(statusTimeout()));

	// "activate" the combo box on pressing return
	connect(comboBoxURL->lineEdit(), SIGNAL(returnPressed()), this, SLOT(comboBoxURL_activated()));

	adjustIconSizes();
}

MiniWebBrowser::~MiniWebBrowser()
{
	hideEvent(0);
}

void MiniWebBrowser::adjustIconSizes()
{
	QFont f;
	f.fromString(qmc2Config->value(QMC2_FRONTEND_PREFIX + "GUI/Font").toString());
	QFontMetrics fm(f);
	QSize iconSize = QSize(fm.height() - 2, fm.height() - 2);

	toolButtonBack->setIconSize(iconSize);
	toolButtonForward->setIconSize(iconSize);
	toolButtonReload->setIconSize(iconSize);
	toolButtonStop->setIconSize(iconSize);
	toolButtonHome->setIconSize(iconSize);
	toolButtonLoad->setIconSize(iconSize);
	toolButtonNext->setIconSize(iconSize);
	toolButtonPrevious->setIconSize(iconSize);
	toolButtonToggleSearchBar->setIconSize(iconSize);
	iconLineEditSearch->setIconSize(iconSize);
	toolButtonCaseSensitive->setIconSize(iconSize);
	toolButtonHighlight->setIconSize(iconSize);
}

void MiniWebBrowser::on_toolButtonCaseSensitive_clicked()
{
	on_toolButtonPrevious_clicked();
	on_toolButtonNext_clicked();
}

void MiniWebBrowser::on_toolButtonHighlight_clicked()
{
	on_toolButtonPrevious_clicked();
	on_toolButtonNext_clicked();
}

void MiniWebBrowser::on_toolButtonNext_clicked()
{
	// FIXME
	/*
	searchTimer.stop();
	webViewBrowser->page()->findText("", QWebEnginePage::HighlightAllOccurrences);
	QWebEnginePage::FindFlags flags = QWebEnginePage::FindWrapsAroundDocument;
	if ( toolButtonCaseSensitive->isChecked() )
		flags |= QWebEnginePage::FindCaseSensitively;
	webViewBrowser->page()->findText(iconLineEditSearch->text(), flags);
	if ( toolButtonHighlight->isChecked() )
		webViewBrowser->page()->findText(iconLineEditSearch->text(), flags | QWebEnginePage::HighlightAllOccurrences);
	*/
}

void MiniWebBrowser::on_toolButtonPrevious_clicked()
{
	// FIXME
	/*
	searchTimer.stop();
	webViewBrowser->page()->findText("", QWebEnginePage::HighlightAllOccurrences);
	QWebEnginePage::FindFlags flags = QWebEnginePage::FindWrapsAroundDocument | QWebEnginePage::FindBackward;
	if ( toolButtonCaseSensitive->isChecked() )
		flags |= QWebEnginePage::FindCaseSensitively;
	webViewBrowser->page()->findText(iconLineEditSearch->text(), flags);
	if ( toolButtonHighlight->isChecked() )
		webViewBrowser->page()->findText(iconLineEditSearch->text(), flags | QWebEnginePage::HighlightAllOccurrences);
	*/
}

void MiniWebBrowser::on_toolButtonToggleSearchBar_clicked()
{
	if ( toolButtonToggleSearchBar->isChecked() )
		frameSearch->show();
	else {
		frameSearch->hide();
		toolButtonHighlight->setChecked(false);
		toolButtonCaseSensitive->setChecked(false);
		iconLineEditSearch->clear();
		on_toolButtonPrevious_clicked();
		on_toolButtonNext_clicked();
	}
}

void MiniWebBrowser::checkBackAndForward()
{
	toolButtonBack->setEnabled(webViewBrowser->history()->canGoBack());
	toolButtonForward->setEnabled(webViewBrowser->history()->canGoForward());
}

void MiniWebBrowser::on_toolButtonBack_clicked()
{
	webViewBrowser->back();
	QTimer::singleShot(0, this, SLOT(checkBackAndForward()));
}

void MiniWebBrowser::on_toolButtonForward_clicked()
{
	webViewBrowser->forward();
	QTimer::singleShot(0, this, SLOT(checkBackAndForward()));
}

void MiniWebBrowser::hideEvent(QHideEvent *e)
{
#if defined(QMC2_BROWSER_EXTRAS_ENABLED)
	// FIXME
	/*
  	foreach (QWidget *widget, QApplication::topLevelWidgets()) {
		if ( widget->inherits("QWebInspector") ) {
			QWebInspector *inspector = (QWebInspector *)widget;
			if ( inspector->page() == webViewBrowser->page() ) {
				qmc2Config->setValue(QMC2_FRONTEND_PREFIX + "WebInspector/Geometry", inspector->saveGeometry());
				inspector->close();
				inspector->update();
				qApp->processEvents();
				break;
			}
		}
	}
	*/
#endif
	if ( !e )
		return;

	e->accept();
}

void MiniWebBrowser::postProcessPageActionInspect()
{
#if defined(QMC2_BROWSER_EXTRAS_ENABLED)
	// FIXME
	/*
  	foreach (QWidget *widget, QApplication::topLevelWidgets()) {
		if ( widget->inherits("QWebInspector") ) {
			QWebInspector *inspector = (QWebInspector *)widget;
			if ( inspector->page() == webViewBrowser->page() ) {
				inspector->restoreGeometry(qmc2Config->value(QMC2_FRONTEND_PREFIX + "WebInspector/Geometry", QByteArray()).toByteArray());
				break;
			}
		}
	}
	*/
#endif
}

void MiniWebBrowser::on_comboBoxURL_activated(int /*index*/)
{
	if ( !comboBoxURL->lineEdit()->text().isEmpty() ) {
		QString text = comboBoxURL->lineEdit()->text();
		QUrl url(text, QUrl::TolerantMode);
		if ( url.scheme().isEmpty() )
			if ( !text.toLower().startsWith("http://") )
				text.prepend("http://");
		comboBoxURL->setEditText(text);
		url = QUrl(text, QUrl::TolerantMode);
		webViewBrowser->load(url);
		int i = comboBoxURL->findText(text);
		if ( i >= 0 )
			comboBoxURL->setCurrentIndex(i);
		comboBoxURL->lineEdit()->setCursorPosition(0);
		webViewBrowser->setFocus();
	}

	QTimer::singleShot(0, toolButtonLoad, SLOT(animateClick()));
}

void MiniWebBrowser::on_toolButtonHome_clicked()
{
	if ( homeUrl.isValid() )
		webViewBrowser->load(homeUrl);
}

void MiniWebBrowser::on_toolButtonLoad_clicked()
{
	if ( !comboBoxURL->lineEdit()->text().isEmpty() ) {
		QString text = comboBoxURL->lineEdit()->text();
		QUrl url(text, QUrl::TolerantMode);
		if ( url.scheme().isEmpty() )
			if ( !text.toLower().startsWith("http://") )
				text.prepend("http://");
		comboBoxURL->setEditText(text);
		comboBoxURL->lineEdit()->setCursorPosition(0);
		webViewBrowser->load(QUrl::fromUserInput(text));
	}
}

void MiniWebBrowser::on_spinBoxZoom_valueChanged(int zoom)
{
	webViewBrowser->setZoomFactor((double)zoom/100.0);
}

void MiniWebBrowser::webViewBrowser_linkClicked(const QUrl url)
{
	if ( url.isValid() ) {
		if ( isPdfViewer() ) {
			MiniWebBrowser *webBrowser = new MiniWebBrowser(0);
			webBrowser->setAttribute(Qt::WA_DeleteOnClose);
			if ( qmc2Config->contains(QMC2_FRONTEND_PREFIX + "WebBrowser/Geometry") )
				webBrowser->restoreGeometry(qmc2Config->value(QMC2_FRONTEND_PREFIX + "WebBrowser/Geometry").toByteArray());
			else {
				webBrowser->adjustSize();
				webBrowser->move(QApplication::primaryScreen()->availableGeometry().center() - webBrowser->rect().center());
			}
			connect(webBrowser->webViewBrowser->page(), SIGNAL(windowCloseRequested()), webBrowser, SLOT(close()));
			webBrowser->webViewBrowser->load(url);
			webBrowser->show();
		} else {
			/*
			QWebHitTestResult hitTest = webViewBrowser->page()->mainFrame()->hitTestContent(webViewBrowser->lastMouseClickPosition);
			if ( hitTest.linkTargetFrame() )
				hitTest.linkTargetFrame()->load(url);
			else {
				webViewBrowser->load(url);
				webViewBrowser_urlChanged(url);
			}
			*/
			webViewBrowser->load(url);
			webViewBrowser_urlChanged(url);
		}
	}
	QTimer::singleShot(0, this, SLOT(checkBackAndForward()));
}

void MiniWebBrowser::webViewBrowser_urlChanged(const QUrl url)
{
	comboBoxURL->setEditText(QString::fromUtf8(webViewBrowser->url().toEncoded()));
	QString newTitle = webViewBrowser->title();
	emit titleChanged(newTitle);
	int i = comboBoxURL->findText(comboBoxURL->lineEdit()->text());
	if ( i < 0 )
		comboBoxURL->insertItem(0, comboBoxURL->lineEdit()->text());
	else {
		QString itemText = comboBoxURL->itemText(i);
		QIcon itemIcon = comboBoxURL->itemIcon(i);
		comboBoxURL->removeItem(i);
		comboBoxURL->insertItem(0, itemIcon, itemText);
	}
	comboBoxURL->setCurrentIndex(0);
	comboBoxURL->lineEdit()->setCursorPosition(0);
	QTimer::singleShot(0, this, SLOT(webViewBrowser_iconChanged()));
}

void MiniWebBrowser::webViewBrowser_loadStarted()
{
	QString newTitle = webViewBrowser->title();
	emit titleChanged(newTitle);

	progressBar->reset();
	progressBar->setRange(0, 100);
	progressBar->setValue(0);
	QFont f(font());
	f.setPointSize(f.pointSize() - 2);
	progressBar->setFont(f);
	progressBar->setMaximumHeight(f.pointSize() + 4);

	QString oldStatusMessage = m_statusMessage;
	if ( labelStatus->isVisible() )
		clearStatus();
	progressBar->show();
	if ( !oldStatusMessage.isEmpty() )
		setStatus(oldStatusMessage);

	if ( firstTimeLoadStarted ) {
		firstTimeLoadStarted = false;
		homeUrl = webViewBrowser->url();
		webViewBrowser->history()->clear();
		toolButtonStop->setEnabled(true);
		toolButtonReload->setEnabled(false);
		toolButtonBack->setEnabled(false);
		toolButtonForward->setEnabled(false);
		toolButtonHome->setEnabled(true);
	} else {
		toolButtonStop->setEnabled(true);
		toolButtonReload->setEnabled(false);
		toolButtonHome->setEnabled(true);
		QTimer::singleShot(0, this, SLOT(checkBackAndForward()));
	}

	QTimer::singleShot(0, this, SLOT(webViewBrowser_iconChanged()));
}

void MiniWebBrowser::webViewBrowser_loadProgress(int progress)
{
	QString newTitle = webViewBrowser->title();
	emit titleChanged(newTitle);

	progressBar->setValue(progress);

	if ( firstTimeLoadProgress ) {
		firstTimeLoadProgress = false;
		homeUrl = webViewBrowser->url();
		webViewBrowser->history()->clear();
		toolButtonBack->setEnabled(false);
		toolButtonForward->setEnabled(false);
		toolButtonHome->setEnabled(true);
	} else {
		QTimer::singleShot(0, this, SLOT(webViewBrowser_iconChanged()));
		QTimer::singleShot(0, this, SLOT(checkBackAndForward()));
	}
}

void MiniWebBrowser::webViewBrowser_loadFinished(bool ok)
{
	QString newTitle = webViewBrowser->title();
	if ( newTitle.isEmpty() )
		newTitle = "QMC2_NO_TITLE";
	emit titleChanged(newTitle);
	progressBar->reset();
	progressBar->hide();
	if ( firstTimeLoadFinished ) {
		firstTimeLoadFinished = false;
		homeUrl = webViewBrowser->url();
		webViewBrowser->history()->clear();
	}
	toolButtonStop->setEnabled(false);
	toolButtonReload->setEnabled(true);
	toolButtonHome->setEnabled(true);
	QTimer::singleShot(0, this, SLOT(checkBackAndForward()));
	QTimer::singleShot(250, this, SLOT(webViewBrowser_iconChanged()));
}

void MiniWebBrowser::webViewBrowser_statusBarMessage(const QString &message)
{
	if ( !message.isEmpty() ) {
		statusTimer.stop();
		setStatus(message);
	} else {
		clearStatus();
		statusTimer.start(QMC2_BROWSER_STATUS_TIMEOUT);
	}
}

void MiniWebBrowser::webViewBrowser_iconChanged()
{
	int i = comboBoxURL->findText(comboBoxURL->lineEdit()->text());

	if ( i >= 0 ) {
		QFontMetrics fm(qApp->font());
		QSize iconSize(fm.height() - 2, fm.height() - 2);
		comboBoxURL->setIconSize(iconSize);
		QIcon pageIcon;
		QString urlStr = webViewBrowser->url().toString();
		if ( iconCache.contains(urlStr) )
			pageIcon = *iconCache[urlStr];
		if ( pageIcon.isNull() ) {
			//pageIcon = QWebEngineSettings::iconForUrl(webViewBrowser->url());
			if ( pageIcon.isNull() )
				pageIcon = QIcon(QString::fromUtf8(":/data/img/browser.png"));
			else
				// for the "cache cost" we simply assume that icons take up 64x64 = 4096 bytes
				iconCache.insert(urlStr, new QIcon(pageIcon), 4096);
		}
		comboBoxURL->setItemIcon(i, pageIcon);
		comboBoxURL->setCurrentIndex(i);
		comboBoxURL->lineEdit()->setCursorPosition(0);
	}
}

void MiniWebBrowser::webViewBrowser_linkHovered(const QString &link, const QString &title, const QString &textContent)
{
	if ( !link.isEmpty() ) {
		statusTimer.stop();
		setStatus(link);
	} else {
		clearStatus();
		statusTimer.start(QMC2_BROWSER_STATUS_TIMEOUT);
	}
}

void MiniWebBrowser::webViewBrowser_statusBarVisibilityChangeRequested(bool visible)
{
	progressBar->setVisible(visible);
}

void MiniWebBrowser::statusTimeout()
{
	statusTimer.stop();
	labelStatus->hide();
	updateGeometry();
}

void MiniWebBrowser::processPageActionDownloadRequested(const QNetworkRequest &request)
{
	qmc2MainWindow->startDownload(this, qmc2NetworkAccessManager->get(request));
}

void MiniWebBrowser::processPageActionHandleUnsupportedContent(QNetworkReply *reply)
{
	QMap <QNetworkAccessManager::Operation, QString> opsShort;
	opsShort[QNetworkAccessManager::HeadOperation] = "HEAD";
	opsShort[QNetworkAccessManager::GetOperation] = "GET";
	opsShort[QNetworkAccessManager::PutOperation] = "PUT";
	opsShort[QNetworkAccessManager::PostOperation] = "POST";

	if ( !reply || reply->url().isEmpty() || reply->url() == homeUrl ) {
		qmc2MainWindow->log(QMC2_LOG_FRONTEND, tr("WARNING: invalid network reply and/or network error"));
		return;
	}

	QVariant header = reply->header(QNetworkRequest::ContentLengthHeader);
	bool ok;
	int size = header.toInt(&ok);
	if ( ok && size == 0 )
		return;

	if ( MiniWebBrowser::supportedSchemes.contains(reply->url().scheme().toLower()) ) {
		switch ( reply->operation() ) {
			case QNetworkAccessManager::GetOperation:
				qmc2MainWindow->startDownload(this, reply);
				break;
			default:
				qmc2MainWindow->log(QMC2_LOG_FRONTEND, QString("FIXME: MiniWebBrowser::processPageActionHandleUnsupportedContent(): OP = %1, URL = %2").arg(opsShort[reply->operation()]).arg(reply->url().toString()));
				break;
		}
	} else
		qmc2MainWindow->log(QMC2_LOG_FRONTEND, QString("FIXME: MiniWebBrowser::processPageActionHandleUnsupportedContent(): OP = %1, URL = %2").arg(opsShort[reply->operation()]).arg(reply->url().toString()));
}

void MiniWebBrowser::changeTitle(QString &title)
{
	if ( title == "QMC2_NO_TITLE" ) {
		setWindowTitle(isPdfViewer() ? tr("PDF viewer") : tr("MiniWebBrowser"));
		currentTitle = "QMC2_NO_TITLE";
	} else {
		if ( title.isEmpty() ) {
			if ( currentTitle == "QMC2_NO_TITLE" )
				setWindowTitle(isPdfViewer() ? tr("PDF viewer") : tr("MiniWebBrowser"));
			else
				setWindowTitle((isPdfViewer() ? tr("PDF viewer") : tr("MiniWebBrowser")) + " :: " + currentTitle);
		} else {
			currentTitle = title;
			setWindowTitle((isPdfViewer() ? tr("PDF viewer") : tr("MiniWebBrowser")) + " :: " + currentTitle);
		}
	}
}

void MiniWebBrowser::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	if ( parentWidget() == 0 )
		qmc2Config->setValue(QMC2_FRONTEND_PREFIX + (isPdfViewer() ? "PdfViewer/Geometry" : "WebBrowser/Geometry"), saveGeometry());
}

void MiniWebBrowser::moveEvent(QMoveEvent *e)
{
	QWidget::moveEvent(e);
	if ( parentWidget() == 0 )
		qmc2Config->setValue(QMC2_FRONTEND_PREFIX + (isPdfViewer() ? "PdfViewer/Geometry" : "WebBrowser/Geometry"), saveGeometry());
}

void MiniWebBrowser::setStatus(QString statusMessage)
{
	if ( isPdfViewer() )
		return;

	m_statusMessage = statusMessage;

	if ( m_statusMessage.isEmpty() )
		labelStatus->clear();
	else {
		QFont f(font());
		f.setPointSize(f.pointSize() - 2);
		labelStatus->setFont(f);
		labelStatus->setMaximumHeight(f.pointSize() + 4);
		QFontMetrics fm(f);
		if ( progressBar->isVisible() )
			labelStatus->setText(fm.elidedText(m_statusMessage, Qt::ElideRight, webViewBrowser->width() / 2 - fm.horizontalAdvance("W")));
		else
			labelStatus->setText(fm.elidedText(m_statusMessage, Qt::ElideRight, webViewBrowser->width() - fm.horizontalAdvance("W")));
		labelStatus->show();
	}

	updateGeometry();
}

QWebEngineView *BrowserWidget::createWindow(QWebEnginePage::WebWindowType type)
{
	MiniWebBrowser *webBrowser = new MiniWebBrowser(0);
	//if ( type == QWebEnginePage::WebModalDialog )
	//	webBrowser->setWindowModality(Qt::ApplicationModal);
	webBrowser->setAttribute(Qt::WA_DeleteOnClose);
	if ( parentBrowser )
		webBrowser->spinBoxZoom->setValue(parentBrowser->spinBoxZoom->value());
	if ( qmc2Config->contains(QMC2_FRONTEND_PREFIX + "WebBrowser/Geometry") )
		webBrowser->restoreGeometry(qmc2Config->value(QMC2_FRONTEND_PREFIX + "WebBrowser/Geometry").toByteArray());
	else {
		webBrowser->adjustSize();
		webBrowser->move(QApplication::primaryScreen()->availableGeometry().center() - webBrowser->rect().center());
	}
	connect(webBrowser->webViewBrowser->page(), SIGNAL(windowCloseRequested()), webBrowser, SLOT(close()));
	webBrowser->show();
	return webBrowser->webViewBrowser;
}

void BrowserWidget::wheelEvent(QWheelEvent *e)
{
	if ( e->modifiers() & Qt::ControlModifier ) {
		if ( parentBrowser )
			parentBrowser->spinBoxZoom->setValue(parentBrowser->spinBoxZoom->value() + parentBrowser->spinBoxZoom->singleStep() * (e->delta() > 0 ? 1 : e->delta() < 0 ? -1 : 0));
		e->accept();
	} else {
		e->ignore();
		QWebEngineView::wheelEvent(e);
	}
}
