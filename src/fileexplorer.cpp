/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMenu>
#include <QModelIndex>
#include <QPoint>
#include <QShortcut>
#include <QSize>
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
    // mdwriter: the header buttons must sit in the same columns as the tree
    // below them -- the up button over the branch/expand column and the home
    // button over the row-icon column.  Both are pinned to the tree's 16px
    // icon square so they line up vertically with the folder icons.
    upButton = new QToolButton(this);
    upButton->setAutoRaise(true);
    upButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    upButton->setIconSize(QSize(16, 16));
    upButton->setFixedSize(16, 16);
    upButton->setToolTip(tr("Go to Parent Folder"));

    homeButton = new QToolButton(this);
    homeButton->setAutoRaise(true);
    homeButton->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
    homeButton->setIconSize(QSize(16, 16));
    homeButton->setFixedSize(16, 16);
    homeButton->setToolTip(tr("Go to Home Folder"));

    pathEdit = new QLineEdit(this);
    pathEdit->setObjectName("fileExplorerPathLabel");
    pathEdit->setPlaceholderText(tr("Type or paste a folder path…"));
    pathEdit->setToolTip(tr("Folder path — edit and press Enter to navigate"));
    pathEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    pathEdit->setClearButtonEnabled(false);

    QHBoxLayout *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(0);
    header->addWidget(upButton);
    // mdwriter: the tree draws its row icon a few px right of the bare
    // indentation, so nudge the home button across to sit exactly under the
    // folder icons.
    header->addSpacing(4);
    header->addWidget(homeButton);
    header->addWidget(pathEdit, 1);

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
    // mdwriter: pin every row's icon to the same square so folder and file
    // icons share one baseline, and give the branch column a fixed width so
    // the expand indicators line up in a straight column.
    tree->setIconSize(QSize(16, 16));
    tree->setIndentation(16);

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
    this->connect(pathEdit, &QLineEdit::returnPressed, this, &FileExplorer::onPathEntered);
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

    pathEdit->setText(currentPath);
    pathEdit->setToolTip(currentPath);
    upButton->setEnabled(!QDir(currentPath).isRoot());
}

void FileExplorer::onPathEntered()
{
    const QString typed = pathEdit->text().trimmed();

    if (typed.isEmpty()) {
        navigateTo(currentPath);
        return;
    }

    navigateTo(typed);
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

    if (!index.isValid() || path.isEmpty()) {
        return;
    }

    // mdwriter: double-clicking a directory expands/collapses it in place,
    // while double-clicking a file still opens it.
    const QFileInfo info(path);

    if (info.isDir()) {
        tree->setExpanded(index, !tree->isExpanded(index));
    } else if (info.isFile()) {
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
