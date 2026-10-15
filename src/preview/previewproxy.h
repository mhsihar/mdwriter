/*
 * SPDX-FileCopyrightText: 2018-2023 Megan Conkle <megan.conkle@kdemail.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PREVIEWPROXY_H
#define PREVIEWPROXY_H

#include <QObject>
#include <QString>

namespace ghostwriter
{
/**
 * Web Channel Proxy to the HTML preview data. Object is shared between C++ and
 * JavaScript to pass the live preview's settings/data updates.
 */
class PreviewProxy : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructor.
     */
    explicit PreviewProxy(QObject *parent = nullptr);

    /**
     * Destructor.
     */
    virtual ~PreviewProxy();

    /**
     * Sets the HTML contents of the live preview browser.
     */
    void setHtmlContent(const QString &html);

    /**
     * Returns the HTML contents of the live preview browser.
     */
    Q_INVOKABLE QString htmlContent() const;
    Q_PROPERTY(QString htmlContent READ htmlContent NOTIFY htmlChanged)

    /**
     * Sets the CSS style sheet of the live preview browser.
     */
    void setStyleSheet(const QString &css);

    /**
     * Returns the CSS style sheet used in the live preview browser.
     */
    Q_INVOKABLE QString styleSheet() const;
    Q_PROPERTY(QString styleSheet READ styleSheet NOTIFY styleSheetChanged)

    /**
     * Sets whether math rendering is enabled in the live preview.
     */
    void setMathEnabled(bool enabled);

    /**
     * Returns true if math rendering is enabled in the live preview,
     * false otherwise.
     */
    Q_INVOKABLE bool mathEnabled() const;
    Q_PROPERTY(bool mathEnabled READ mathEnabled NOTIFY mathToggled)

    /**
     * mdwriter: invoked from the preview page's JavaScript once a newly
     * delivered HTML snapshot has been applied to the DOM and painted, so
     * that the host can hide the loading spinner only when the preview is
     * actually ready (the WebChannel transfer and the renderer's HTML
     * parse/paint are the visibly slow part of a large update).
     */
    Q_INVOKABLE void contentRendered();

signals:
    /**
     * Emitted when the HTML content changes.
     */
    void htmlChanged(const QString &html);

    /**
     * Emitted when the style sheet changes.
     */
    void styleSheetChanged(const QString &css);

    /**
     * Emitted when the math rendering is toggled.
     */
    void mathToggled(bool enabled);

    /**
     * mdwriter: emitted when the preview page reports that a delivered HTML
     * snapshot has been rendered (see contentRendered()).
     */
    void rendered();

private:
    QString m_htmlContent;
    QString m_styleSheet;
    bool m_mathEnabled;
};

} // namespace ghostwriter

#endif // PREVIEWPROXY_H
