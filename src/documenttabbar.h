/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DOCUMENTTABBAR_H
#define DOCUMENTTABBAR_H

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QTabBar>

class QContextMenuEvent;
class QEvent;
class QMouseEvent;
class QPaintEvent;

namespace ghostwriter
{
/**
 * The tab strip shown above the editor stack.
 *
 * QTabBar puts a close button on *every* tab as soon as tabsClosable() is
 * true, and its initStyleOption() is not virtual, so the button cannot be
 * restricted per tab.  This bar therefore disables the built-in button and
 * paints (and hit-tests) its own instead:
 *
 * - the "x" is shown only on the active tab or on the tab under the cursor;
 * - 20 px are reserved on every tab while more than one tab is open, so the
 *   label never shifts when the button appears;
 * - middle click closes the tab;
 * - right click opens a tab context menu.
 */
class DocumentTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit DocumentTabBar(QWidget *parent = nullptr);
    ~DocumentTabBar();

    /**
     * Whether the close button is painted for @p index: true only for the
     * active tab or the tab under the cursor, and only while more than one
     * tab is open.
     */
    bool shouldShowCloseButton(int index) const;

    /**
     * The area occupied by the close button of @p index, in bar coordinates
     * (mirrors QTabBar::tabRect()).
     */
    QRect closeButtonRect(int index) const;

    /**
     * Marks the tab at @p index as having unsaved changes.  Modified tabs show
     * a warning-coloured dot in the button gutter (instead of the close "x",
     * which appears on hover), echoing VS Code / Geany.
     */
    void setTabModified(int index, bool modified);

    /**
     * Whether the tab at @p index is currently marked modified.
     */
    bool isTabModified(int index) const;

signals:
    /**
     * Emitted on a right click.  @p index is -1 when the click landed on the
     * empty part of the strip rather than on a tab.
     */
    void contextMenuRequested(int index, const QPoint &globalPos);

protected:
    QSize tabSizeHint(int index) const override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void tabInserted(int index) override;
    void tabRemoved(int index) override;

private:
    int m_hoveredIndex;
    int m_closePressedIndex;

    void setHoveredIndex(int index);
    void invalidateHover();
};
} // namespace ghostwriter

#endif // DOCUMENTTABBAR_H
