/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QVariant>

#include "documenttabbar.h"

namespace ghostwriter
{
namespace
{
// Gutter reserved at the right of every tab for the close "x" / dirty dot.
const int CloseButtonReserve = 20;

// mdwriter: unsaved-changes indicator.  An amber "warning" dot shown in the
// tab's button gutter (where VS Code shows its dirty dot and Geany its marker).
// Chosen to stay legible on both light and dark themes.
const QColor ModifiedIndicatorColor("#E8A33D");
} // namespace

DocumentTabBar::DocumentTabBar(QWidget *parent)
    : QTabBar(parent),
    m_hoveredIndex(-1),
    m_closePressedIndex(-1)
{
    // QTabBar only delivers mouse moves when tracking is enabled; the close
    // button highlight follows the cursor.
    this->setMouseTracking(true);

    // The built-in button appears on every tab at once, and QTabBar::initStyleOption()
    // is not virtual, so it cannot be limited to the active/hovered tab.  Keep
    // it off and paint this class's own glyph instead.
    this->setTabsClosable(false);
    this->setExpanding(false);
    this->setDrawBase(true);
    this->setMovable(true);
    this->setElideMode(Qt::ElideRight);
    this->setUsesScrollButtons(true);
}

DocumentTabBar::~DocumentTabBar()
{
    ;
}

QSize DocumentTabBar::tabSizeHint(int index) const
{
    QSize size = QTabBar::tabSizeHint(index);

    // Always reserve the button gutter, so the close "x" / unsaved-changes dot
    // never makes the label shift.  (QTabBar caches tabSizeHint() and does not
    // invalidate it for setTabData(), so a size that depended on the modified
    // flag would go stale anyway.)
    size.setWidth(size.width() + CloseButtonReserve);

    return size;
}

bool DocumentTabBar::shouldShowCloseButton(int index) const
{
    // The "x" yields to the unsaved-changes dot on a modified active tab, so
    // the warning stays visible; it returns as soon as the tab is hovered.
    return (this->count() > 1)
        && ((index == m_hoveredIndex)
            || ((index == this->currentIndex()) && !this->isTabModified(index)));
}

QRect DocumentTabBar::closeButtonRect(int index) const
{
    const QRect tab = this->tabRect(index);
    const int side = qBound(8, tab.height() - 8, 14);

    return QRect(tab.right() - CloseButtonReserve + (CloseButtonReserve - side) / 2 - 1,
                 tab.center().y() - side / 2,
                 side,
                 side);
}

void DocumentTabBar::setTabModified(int index, bool modified)
{
    if ((index < 0) || (index >= this->count())) {
        return;
    }

    this->setTabData(index, QVariant::fromValue(modified));
    this->update(this->tabRect(index));
}

bool DocumentTabBar::isTabModified(int index) const
{
    if ((index < 0) || (index >= this->count())) {
        return false;
    }

    return this->tabData(index).toBool();
}

void DocumentTabBar::setHoveredIndex(int index)
{
    if (index == m_hoveredIndex) {
        return;
    }

    const int previous = m_hoveredIndex;
    m_hoveredIndex = index;

    if (previous >= 0 && previous < this->count()) {
        this->update(this->tabRect(previous));
    }

    if (index >= 0 && index < this->count()) {
        this->update(this->tabRect(index));
    }
}

void DocumentTabBar::invalidateHover()
{
    m_hoveredIndex = -1;
    m_closePressedIndex = -1;
}

void DocumentTabBar::paintEvent(QPaintEvent *event)
{
    QTabBar::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < this->count(); i++) {
        const bool showClose = this->shouldShowCloseButton(i);
        const bool showDot = !showClose && this->isTabModified(i);

        if (!showClose && !showDot) {
            continue;
        }

        const QRect gutter = this->closeButtonRect(i);

        if (showDot) {
            // Unsaved-changes dot (VS Code / Geany style), in warning amber.
            const int diameter = qBound(6, gutter.height() - 2, 10);
            QRect dot(0, 0, diameter, diameter);

            dot.moveCenter(gutter.center());
            painter.setPen(Qt::NoPen);
            painter.setBrush(ModifiedIndicatorColor);
            painter.drawEllipse(dot);
            continue;
        }

        const bool hot = (i == m_hoveredIndex);
        QColor color = this->palette().color(QPalette::Text);

        color.setAlpha(hot ? 255 : 170);
        painter.setPen(QPen(color, hot ? 2.0 : 1.5, Qt::SolidLine, Qt::RoundCap));

        const QRect glyph = gutter.adjusted(3, 3, -3, -3);
        const QPoint topLeft = glyph.topLeft();
        const QPoint bottomRight = glyph.bottomRight();

        painter.drawLine(topLeft, bottomRight);
        painter.drawLine(QPoint(bottomRight.x(), topLeft.y()),
                         QPoint(topLeft.x(), bottomRight.y()));
    }
}

void DocumentTabBar::mousePressEvent(QMouseEvent *event)
{
    m_closePressedIndex = -1;

    if (Qt::LeftButton == event->button()) {
        const int index = this->tabAt(event->pos());

        if ((index >= 0)
                && this->shouldShowCloseButton(index)
                && this->closeButtonRect(index).contains(event->pos())) {
            // Swallow the press so the tab bar neither selects the tab nor
            // starts a drag; the matching release becomes a close request.
            m_closePressedIndex = index;
            return;
        }
    }

    QTabBar::mousePressEvent(event);
}

void DocumentTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QTabBar::mouseMoveEvent(event);
    this->setHoveredIndex(this->tabAt(event->pos()));
}

void DocumentTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    const int index = this->tabAt(event->pos());
    const bool closeHit = (m_closePressedIndex >= 0)
        && (Qt::LeftButton == event->button())
        && (index == m_closePressedIndex)
        && this->closeButtonRect(index).contains(event->pos());
    const bool middleHit = (Qt::MiddleButton == event->button()) && (index >= 0);

    m_closePressedIndex = -1;

    QTabBar::mouseReleaseEvent(event);

    if (closeHit || middleHit) {
        emit this->tabCloseRequested(index);
    }
}

void DocumentTabBar::leaveEvent(QEvent *event)
{
    QTabBar::leaveEvent(event);
    this->setHoveredIndex(-1);
}

void DocumentTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    emit this->contextMenuRequested(this->tabAt(event->pos()), event->globalPos());
    event->accept();
}

void DocumentTabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    this->invalidateHover();
}

void DocumentTabBar::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);
    this->invalidateHover();
}
} // namespace ghostwriter
