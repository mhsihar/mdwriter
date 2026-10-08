/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QModelIndex>
#include <QPoint>
#include <QResizeEvent>
#include <QShortcut>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "fileexplorer.h"

namespace ghostwriter
{
FileExplorer::FileExplorer(QWidget *parent)
    : QWidget(parent)
{
    model = new QFileSystemModel(this);
    model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    model->setReadOnly(true);

    // --- header bar: up / home buttons + current folder label ---
    upButton = new QToolButton(this);
    upButton->setAutoRaise(true);
    upButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    upButton->setToolTip(tr("Go to Parent Folder"));

    homeButton = new QToolButton(this);
    homeButton->setAutoRaise(true);
    homeButton->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
    homeButton->setToolTip(tr("Go to Home Folder"));

    pathLabel = new QLabel(this);
    pathLabel->setObjectName("fileExplorerPathLabel");
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    QHBoxLayout *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(2);
    header->addWidget(upButton);
    header->addWidget(homeButton);
    header->addWidget(pathLabel, 1);

    // --- tree ---
    tree = new QTreeView(this);
    tree->setModel(model);
    tree->setHeaderHidden(true);
    tree->setUniformRowHeights(true);
    tree->setSortingEnabled(false);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree->setExpandsOnDoubleClick(false);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);

    for (int column = 1; column < model->columnCount(); ++column) {
        tree->setColumnHidden(column, true);
    }

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addLayout(header);
    layout->addWidget(tree, 1);

    this->connect(upButton, &QToolButton::clicked, this, &FileExplorer::goToParentFolder);
    this->connect(homeButton, &QToolButton::clicked, this, &FileExplorer::goToHomeFolder);
    this->connect(tree, &QTreeView::doubleClicked, this, &FileExplorer::onDoubleClicked);
    this->connect(
        tree,
        &QTreeView::customContextMenuRequested,
        this,
        &FileExplorer::showContextMenu
    );

    // Widget-scoped shortcuts so they fire while the tree has focus.
    QShortcut *backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    backspaceShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    this->connect(backspaceShortcut, &QShortcut::activated, this, &FileExplorer::goToParentFolder);

    QShortcut *altUpShortcut = new QShortcut(QKeySequence("Alt+Up"), this);
    altUpShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    this->connect(altUpShortcut, &QShortcut::activated, this, &FileExplorer::goToParentFolder);

    this->navigateTo(QDir::homePath());
}

FileExplorer::~FileExplorer()
{
    ;
}

void FileExplorer::navigateTo(const QString &directory)
{
    QFileInfo info(directory);

    if (!info.exists() || !info.isDir()) {
        return;
    }

    currentPath = info.absoluteFilePath();
    tree->setRootIndex(model->setRootPath(currentPath));

    this->updatePathLabel();
    pathLabel->setToolTip(currentPath);
    upButton->setEnabled(!QDir(currentPath).isRoot());
}

void FileExplorer::updatePathLabel()
{
    const int available = pathLabel->width();

    if (available <= 0) {
        pathLabel->setText(currentPath);
        return;
    }

    pathLabel->setText(
        pathLabel->fontMetrics().elidedText(currentPath, Qt::ElideMiddle, available)
    );
}

void FileExplorer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    this->updatePathLabel();
}

void FileExplorer::goToParentFolder()
{
    const QString current = model->rootPath();

    if (current.isEmpty()) {
        return;
    }

    QDir dir(current);

    if (dir.isRoot() || !dir.cdUp()) {
        return;
    }

    navigateTo(dir.absolutePath());
}

void FileExplorer::goToHomeFolder()
{
    navigateTo(QDir::homePath());
}

void FileExplorer::reveal(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return;
    }

    const QFileInfo info(filePath);

    if (!info.exists()) {
        return;
    }

    const QString directory =
        info.isDir() ? info.absoluteFilePath() : info.absolutePath();

    // Re-root at the document's folder so the explorer "follows" the doc.
    navigateTo(directory);

    const QModelIndex index = model->index(filePath);

    if (index.isValid()) {
        tree->expand(index);
        tree->setCurrentIndex(index);
        tree->selectionModel()->select(
            index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
        );
        tree->scrollTo(index, QAbstractItemView::PositionAtCenter);
    }
}

void FileExplorer::onDoubleClicked(const QModelIndex &index)
{
    const QString path = model->filePath(index);

    if (QFileInfo(path).isFile()) {
        emit fileActivated(path);
    }
}

void FileExplorer::showContextMenu(const QPoint &pos)
{
    QMenu menu(this);

    QAction *upAction = menu.addAction(tr("Go to Parent Folder"));
    upAction->setEnabled(upButton->isEnabled());

    QAction *homeAction = menu.addAction(tr("Go to Home Folder"));

    QAction *chosen = menu.exec(tree->viewport()->mapToGlobal(pos));

    if (chosen == upAction) {
        goToParentFolder();
    } else if (chosen == homeAction) {
        goToHomeFolder();
    }
}
} // namespace ghostwriter
