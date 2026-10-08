/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DOCUMENTAREA_H
#define DOCUMENTAREA_H

#include <QString>
#include <QWidget>

class QStackedWidget;
class QTabBar;

namespace ghostwriter
{
class DocumentTab;

/**
 * The middle pane: a tab bar over a stack of DocumentTab pages.
 *
 * Unlike a plain QTabWidget, the tab bar stays visible when the editor pages
 * are hidden, so tabs remain switchable in the preview-first layout.
 */
class DocumentArea : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentArea(QWidget *parent = nullptr);
    ~DocumentArea();

    int addTab(DocumentTab *tab, const QString &text);
    void removeTab(int index);
    int count() const;
    int currentIndex() const;
    void setCurrentIndex(int index);
    DocumentTab *currentTab() const;
    DocumentTab *tabAt(int index) const;
    int indexOf(DocumentTab *tab) const;
    void setTabText(int index, const QString &text);
    void setTabToolTip(int index, const QString &tip);
    void setTabsClosable(bool closable);

    /**
     * Shows/hides the editor pages but keeps the tab bar visible.
     */
    void setEditorVisible(bool visible);

signals:
    void currentChanged(int index);
    void tabCloseRequested(int index);

private:
    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
};
} // namespace ghostwriter

#endif // DOCUMENTAREA_H
