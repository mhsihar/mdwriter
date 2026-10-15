/*
 * SPDX-FileCopyrightText: 2014-2023 Megan Conkle <megan.conkle@kdemail.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QFutureWatcher>
#include <QLabel>
#include <QMenu>
#include <QVariant>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QApplication>
#include <QStack>
#include <QDir>
#include <QDesktopServices>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QtConcurrentRun>
#include <QFuture>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#endif

#include <export/exporter.h>
#include "../settings/appsettings.h"
#include "htmlpreview.h"
#include "previewproxy.h"
#include "sandboxedwebpage.h"

namespace ghostwriter
{
namespace
{
/**
 * mdwriter: small, non-interactive spinner drawn over the live HTML
 * preview while a large document is being converted to HTML.
 *
 * It is a plain QLabel subclass painted with QPainter -- no moc and no
 * image assets.  Showing is delayed by 125 ms so that the usual
 * sub-frame preview update never flashes it, and it is centred on every
 * resize so it stays in the middle of the preview pane.
 */
class PreviewSpinner : public QLabel
{
public:
    explicit PreviewSpinner(QWidget *parent) : QLabel(parent)
    {
        this->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        this->setObjectName("previewSpinner");

        // Declaration order is deliberate: members are destroyed in
        // reverse order, so showTimer (declared last) goes first and can
        // never fire into a half-destroyed widget, while the spin timer
        // still stops before the angle it paints is released.
        this->angle = 0;

        this->spinTimer.setInterval(70);

        QObject::connect(&this->spinTimer, &QTimer::timeout, [this]() {
            this->angle = (this->angle + 24) % 360;
            this->update();
        });

        this->showTimer.setSingleShot(true);
        this->showTimer.setInterval(125);

        QObject::connect(&this->showTimer, &QTimer::timeout, [this]() {
            this->center();
            this->show();
            this->raise();
            this->spinTimer.start();
        });

        this->hide();
    }

    /** Arm the spinner (starts the delayed show; keeps it running). */
    void begin()
    {
        if (!this->isVisible()) {
            this->angle = 0;
            this->showTimer.start();
        } else if (!this->spinTimer.isActive()) {
            this->spinTimer.start();
        }
    }

    /** Disarm the spinner immediately. */
    void end()
    {
        this->showTimer.stop();
        this->spinTimer.stop();
        this->hide();
    }

    /** Keep the spinner centred in its parent (the preview pane). */
    void center()
    {
        QWidget *parent = this->parentWidget();

        if (nullptr == parent) {
            return;
        }

        const int size = 48;
        this->setGeometry((parent->width() - size) / 2,
            (parent->height() - size) / 2,
            size,
            size);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 165));
        painter.drawEllipse(this->rect().adjusted(1, 1, -1, -1));

        painter.translate(this->width() / 2.0, this->height() / 2.0);
        painter.rotate(this->angle);

        painter.setPen(QPen(QColor(255, 255, 255, 235),
            3.0,
            Qt::SolidLine,
            Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(QRectF(-14.0, -14.0, 28.0, 28.0), 0, 270 * 16);
    }

private:
    int angle;
    QTimer spinTimer;
    QTimer showTimer;
};
} // namespace

class HtmlPreviewPrivate
{
    Q_DECLARE_PUBLIC(HtmlPreview)

public:
    HtmlPreviewPrivate(HtmlPreview *q_ptr)
        : q_ptr(q_ptr)
    {
        ;
    }

    ~HtmlPreviewPrivate()
    {
        ;
    }

    HtmlPreview *q_ptr;

    MarkdownDocument *document;
    bool updateInProgress;
    bool updateAgain;
    PreviewProxy proxy;
    QString baseUrl;
    QRegularExpression headingTagExp;
    Exporter *exporter;
    QString wrapperHtml;
    QFutureWatcher<QString> *futureWatcher;
    PreviewSpinner *spinner;

    /**
     * mdwriter: true while a freshly converted HTML snapshot has been handed
     * to the renderer but has not yet been reported as painted back.  The
     * spinner stays up across the WebChannel transfer and the renderer's
     * HTML parse/paint -- the visibly slow part of a large preview update --
     * instead of being hidden the moment the C++-side conversion finishes.
     */
    bool renderPending;

    /**
     * mdwriter: safety net -- if the preview page never reports the render
     * back (e.g. its script failed), never leave the spinner stuck forever.
     */
    QTimer *renderSafetyTimer;

    void onHtmlReady();
    void onLoadFinished(bool ok);

    /**
     * Sets the base directory path for determining resource
     * paths relative to the web page being previewed.
     * This method is called whenever the file path changes.
     */
    void updateBaseDir();
    /*
    * Sets the HTML contents to display, and creates a backup of the old
    * HTML for diffing to scroll to the first difference whenever
    * updatePreview() is called.
    */
    void setHtmlContent(const QString &html);

    static QString exportToHtml(const QString &text, Exporter *exporter);
};

HtmlPreview::HtmlPreview
(
    MarkdownDocument *document,
    Exporter *exporter,
    QWidget *parent
) : QWebEngineView(parent),
    d_ptr(new HtmlPreviewPrivate(this))
{
    Q_D(HtmlPreview);
    
    d->document = document;
    d->updateInProgress = false;
    d->updateAgain = false;
    d->renderPending = false;
    d->exporter = exporter;
    d->spinner = new PreviewSpinner(this);
    d->proxy.setMathEnabled(d->exporter->supportsMath());

    d->baseUrl = "";

    this->setPage(new SandboxedWebPage(this));
    this->settings()->setDefaultTextEncoding("utf-8");
    this->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessFileUrls,
        true);
    this->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessRemoteUrls,
        true);
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    this->page()->action(QWebEnginePage::Reload)->setVisible(false);
    this->page()->action(QWebEnginePage::ReloadAndBypassCache)
        ->setVisible(false);
    this->page()->action(QWebEnginePage::OpenLinkInThisWindow)
        ->setVisible(false);
    this->page()->action(QWebEnginePage::OpenLinkInNewWindow)
        ->setVisible(false);
    this->page()->action(QWebEnginePage::ViewSource)->setVisible(false);
    this->page()->action(QWebEnginePage::SavePage)->setVisible(false);
    QWebEngineProfile::defaultProfile()
        ->setHttpCacheType(QWebEngineProfile::NoCache);
    QWebEngineProfile::defaultProfile()->clearHttpCache();
    QWebEngineProfile::defaultProfile()->clearAllVisitedLinks();

    this->connect(
        this,
        &QWebEngineView::loadFinished,
        [d](bool ok) {
            d->onLoadFinished(ok);
        }
    );

    d->headingTagExp.setPattern("^[Hh][1-6]$");

    d->futureWatcher = new QFutureWatcher<QString>(this);
    this->connect(
        d->futureWatcher,
        &QFutureWatcher<QString>::finished,
        [d]() {
            d->onHtmlReady();
        }
    );

    this->connect(
        document,
        &MarkdownDocument::filePathChanged,
        [d]() {
            d->updateBaseDir();
        }
    );

    // Set zoom factor for Chromium browser to account for system DPI settings,
    // since Chromium assumes 96 DPI as a fixed resolution.
    //
    qreal horizontalDpi =
        QGuiApplication::primaryScreen()->logicalDotsPerInchX();
    this->setZoomFactor((horizontalDpi / 96.0));

    QWebChannel *channel = new QWebChannel(this);
    channel->registerObject(QStringLiteral("previewProxy"), &d->proxy);
    this->page()->setWebChannel(channel);

    // mdwriter: the preview page pings back through the channel once it has
    // painted a delivered snapshot -- only then take the spinner down.
    QObject::connect(&d->proxy, &PreviewProxy::rendered, [d]() {
        if (d->renderPending && !d->updateInProgress) {
            d->renderPending = false;
            d->renderSafetyTimer->stop();
            d->spinner->end();
        }
    });

    d->renderSafetyTimer = new QTimer(this);
    d->renderSafetyTimer->setSingleShot(true);
    d->renderSafetyTimer->setInterval(8000);
    QObject::connect(d->renderSafetyTimer, &QTimer::timeout, [d]() {
        d->renderPending = false;
        d->spinner->end();
    });

    QFile wrapperHtmlFile(":/resources/preview.html");

    if (!wrapperHtmlFile.open(QFile::ReadOnly | QFile::Text)) {
        d->wrapperHtml = tr("Error loading resources/preview.html");
    } else {
        QTextStream stream(&wrapperHtmlFile);
        d->wrapperHtml = stream.readAll();
        wrapperHtmlFile.close();
    }

    // Set the base URL and load the preview using wrapperHtml above.
    d->updateBaseDir();
}

HtmlPreview::~HtmlPreview()
{
    Q_D(HtmlPreview);
    
    // Wait for thread to finish if in the middle of updating the preview.
    d->futureWatcher->waitForFinished();
}

void HtmlPreview::contextMenuEvent(QContextMenuEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QMenu *menu = page()->createStandardContextMenu();
#else
    QMenu *menu = createStandardContextMenu();
#endif

    menu->popup(event->globalPos());
}

void HtmlPreview::updatePreview()
{
    Q_D(HtmlPreview);
    
    if (d->updateInProgress) {
        d->updateAgain = true;
        return;
    }

    if (this->isVisible()) {
        // Some markdown processors don't handle empty text very well
        // and will err.  Thus, only pass in text from the document
        // into the markdown processor if the text isn't empty or null.
        //
        if (d->document->isEmpty()) {
            d->setHtmlContent("");
        } else if (nullptr != d->exporter) {
            QString text = d->document->toPlainText();

            if (!text.isNull() && !text.isEmpty()) {
                d->updateInProgress = true;
                QFuture<QString> future =
                    QtConcurrent::run
                    (
                        &HtmlPreviewPrivate::exportToHtml,
                        d->document->toPlainText(),
                        d->exporter
                    );
                d->futureWatcher->setFuture(future);
                // mdwriter: conversion of a large document can take
                // noticeably long -- show the spinner while it runs.  It
                // only becomes visible after a ~125 ms grace period (and
                // stays up until the renderer paints the result), so
                // ordinary quick updates never flash it.
                d->spinner->begin();
            }
        }
    }
}

void HtmlPreview::navigateToHeading(int headingSequenceNumber)
{
    this->page()->runJavaScript
    (
        QString
        (
            "scrollToHeading(%1);"
        ).arg(headingSequenceNumber)
    );
}

void HtmlPreview::setHtmlExporter(Exporter *exporter)
{
    Q_D(HtmlPreview);
    
    d->exporter = exporter;
    d->setHtmlContent("");
    d->proxy.setMathEnabled(d->exporter->supportsMath());
    updatePreview();
}

void HtmlPreview::setStyleSheet(const QString &css)
{
    Q_D(HtmlPreview);
    
    d->proxy.setStyleSheet(css);
}

void HtmlPreview::setDocument(MarkdownDocument *document)
{
    Q_D(HtmlPreview);

    d->document = document;
    updatePreview();
}

void HtmlPreview::setMathEnabled(bool enabled)
{
    Q_D(HtmlPreview);
    
    d->proxy.setMathEnabled(enabled);
}

void HtmlPreviewPrivate::onHtmlReady()
{
    Q_Q(HtmlPreview);
    
    setHtmlContent(futureWatcher->result());
    updateInProgress = false;

    // mdwriter: keep the spinner up -- the snapshot has only just been
    // handed to the renderer.  It comes down when the preview page reports
    // the DOM actually rendered and painted (see the proxy::rendered
    // handler), or after the safety timeout in the worst case.  This must
    // happen before the updateAgain branch below, otherwise a queued
    // follow-up render would be disarmed right after being armed.
    renderPending = true;
    renderSafetyTimer->start();

    if (updateAgain) {
        updateAgain = false;
        q->updatePreview();
    }

}

void HtmlPreviewPrivate::onLoadFinished(bool ok)
{
    Q_Q(HtmlPreview);
    
    if (ok) {
        q->page()->runJavaScript(
            "document.documentElement.contentEditable = false;");
    }

    // mdwriter: the wrapper HTML reload triggered by updateBaseDir() is
    // not a conversion, so it must not leave the spinner stuck on screen.
    // While a conversion is still running, or a rendered snapshot is still
    // awaiting its paint confirmation, we leave it alone.
    if (!updateInProgress && !renderPending) {
        spinner->end();
    }
}

void HtmlPreviewPrivate::updateBaseDir()
{
    Q_Q(HtmlPreview);
    
    if (!document->filePath().isNull() && !document->filePath().isEmpty()) {
        // Note that a forward slash ("/") is appended to the path to
        // ensure it works.  If the slash isn't there, then it won't
        // recognize the base URL for some reason.
        //
        baseUrl = QUrl::fromLocalFile(
            QFileInfo(document->filePath()).dir().absolutePath() 
                      + "/").toString();
    } else {
        this->baseUrl = "";
    }

    q->setHtml(wrapperHtml, QUrl(baseUrl));
    q->updatePreview();
}

void HtmlPreview::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    Q_D(HtmlPreview);
    
    d->renderPending = false;
    if (nullptr != d->renderSafetyTimer) {
        d->renderSafetyTimer->stop();
    }
    d->spinner->end();
    d->setHtmlContent("");
}

void HtmlPreview::resizeEvent(QResizeEvent *event)
{
    QWebEngineView::resizeEvent(event);

    // mdwriter: keep the loading spinner centred in the preview pane.
    Q_D(HtmlPreview);

    if (d->spinner->isVisible()) {
        d->spinner->center();
    }
}

void HtmlPreviewPrivate::setHtmlContent(const QString &html)
{
    this->proxy.setHtmlContent(html);
}

QString HtmlPreviewPrivate::exportToHtml
(
    const QString &text,
    Exporter *exporter
)
{
    QString html;

    // Use the smart typography preference for the preview, independent of
    // the last setting used during document export.  Literal text inside raw
    // HTML code elements is protected regardless (see CmarkGfmAPI).
    bool smartTypographyEnabled = exporter->smartTypographyEnabled();
    exporter->setSmartTypographyEnabled
    (
        AppSettings::instance()->previewSmartTypographyEnabled()
    );

    // Export to HTML.
    exporter->exportToHtml(text, html);

    // Put smart typography setting back to the way it was before
    // so that the last setting used during document export is remembered.
    //
    exporter->setSmartTypographyEnabled(smartTypographyEnabled);

    return html;
}
} // namespace ghostwriter
