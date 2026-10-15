/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FILEEXPLORER_H
#define FILEEXPLORER_H

#include <QString>
#include <QWidget>

class QFileSystemModel;
class QLabel;
class QLineEdit;
class QModelIndex;
class QToolButton;
class QTreeView;

namespace ghostwriter
{
/**
 * A directory tree (file explorer) with an "up"/"home" header bar that can
 * follow the currently open document by re-rooting itself at the document's
 * folder and revealing (selecting + scrolling to) the document's file.
 */
class FileExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit FileExplorer(QWidget *parent = nullptr);
    ~FileExplorer();

    /**
     * Re-roots the tree at the folder containing filePath and selects,
     * expands and scrolls to that file.  Does nothing for an empty or
     * non-existent path.
     */
    void reveal(const QString &filePath);

signals:
    /**
     * Emitted when a file (not a directory) is double-clicked.
     */
    void fileActivated(const QString &filePath);

protected:
    /**
     * Re-roots the tree at the given directory and updates the header bar.
     */
    void navigateTo(const QString &directory);

private:
    /**
     * Accepts a typed/pasted path from the header bar (Geany-style).
     */
    void onPathEntered();

    void goToParentFolder();
    void goToHomeFolder();
    void onDoubleClicked(const QModelIndex &index);
    void showContextMenu(const QPoint &pos);

    QFileSystemModel *model;
    QTreeView *tree;
    QToolButton *upButton;
    QToolButton *homeButton;
    QLineEdit *pathEdit;
    QString currentPath;
};
} // namespace ghostwriter

#endif // FILEEXPLORER_H
