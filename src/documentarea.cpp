/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QStackedWidget>
#include <QVBoxLayout>

#include "documentarea.h"
#include "documenttab.h"

namespace ghostwriter
{
DocumentArea::DocumentArea(QWidget *parent)
    : QWidget(parent)
{
    // mdwriter: custom bar -- hover-only close button, middle-click close and
    // a right-click context menu (see documenttabbar.h).
    m_tabBar = new DocumentTabBar(this);

    m_stack = new QStackedWidget(this);

    // The tab bar is NOT added to this layout; MainWindow adds it above the
    // splitter so it stays visible when the editor stack is hidden.
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_stack, 1);

    this->connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        m_stack->setCurrentIndex(index);
        emit currentChanged(index);
    });
    this->connect(m_tabBar, &QTabBar::tabCloseRequested, this, &DocumentArea::tabCloseRequested);
}

DocumentArea::~DocumentArea()
{
    ;
}

DocumentTabBar *DocumentArea::tabBar() const
{
    return m_tabBar;
}

int DocumentArea::addTab(DocumentTab *tab, const QString &text)
{
    m_stack->addWidget(tab);
    int index = m_tabBar->addTab(text);
    m_tabBar->setTabToolTip(index, text);
    return index;
}

void DocumentArea::removeTab(int index)
{
    DocumentTab *tab = tabAt(index);

    if (nullptr == tab) {
        return;
    }

    // mdwriter: the tab bar and the stacked widget are two index spaces that
    // must move together.  Removing from the bar FIRST fires its
    // currentChanged while the stack still holds the dying tab, so the
    // currentChanged handler above pushes a *stack* index that still points
    // at the widget being removed - and MainWindow would repoint its shared
    // editor / document manager / spelling / find-replace at a DocumentTab
    // that is deleted below.  Block the bar's signals, drop the widget from
    // the stack, then resynchronize and notify once with both containers
    // consistent.
    m_tabBar->blockSignals(true);
    m_tabBar->removeTab(index);
    m_tabBar->blockSignals(false);

    m_stack->removeWidget(tab);
    tab->deleteLater();

    const int current = m_tabBar->currentIndex();

    if (current >= 0 && current < m_stack->count()) {
        m_stack->setCurrentIndex(current);
    }

    emit currentChanged(current);
}

int DocumentArea::count() const
{
    return m_tabBar->count();
}

int DocumentArea::currentIndex() const
{
    return m_tabBar->currentIndex();
}

void DocumentArea::setCurrentIndex(int index)
{
    m_tabBar->setCurrentIndex(index);
}

DocumentTab *DocumentArea::currentTab() const
{
    return qobject_cast<DocumentTab *>(m_stack->currentWidget());
}

DocumentTab *DocumentArea::tabAt(int index) const
{
    return qobject_cast<DocumentTab *>(m_stack->widget(index));
}

int DocumentArea::indexOf(DocumentTab *tab) const
{
    return m_stack->indexOf(tab);
}

void DocumentArea::setTabText(int index, const QString &text)
{
    if (index >= 0 && index < m_tabBar->count()) {
        m_tabBar->setTabText(index, text);
    }
}

void DocumentArea::setTabToolTip(int index, const QString &tip)
{
    if (index >= 0 && index < m_tabBar->count()) {
        m_tabBar->setTabToolTip(index, tip);
    }
}

void DocumentArea::setEditorVisible(bool visible)
{
    m_stack->setVisible(visible);
}
} // namespace ghostwriter
