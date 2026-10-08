/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

#include "documentarea.h"
#include "documenttab.h"

namespace ghostwriter
{
DocumentArea::DocumentArea(QWidget *parent)
    : QWidget(parent)
{
    m_tabBar = new QTabBar(this);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);

    m_stack = new QStackedWidget(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
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

    m_tabBar->removeTab(index);
    m_stack->removeWidget(tab);
    tab->deleteLater();
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

void DocumentArea::setTabsClosable(bool closable)
{
    m_tabBar->setTabsClosable(closable);
}

void DocumentArea::setEditorVisible(bool visible)
{
    m_stack->setVisible(visible);
}
} // namespace ghostwriter
