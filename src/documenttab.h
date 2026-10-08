/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DOCUMENTTAB_H
#define DOCUMENTTAB_H

#include <QWidget>

namespace ghostwriter
{
class DocumentManager;
class FindReplace;
class MarkdownDocument;
class MarkdownEditor;
class SpellCheckDecorator;
class Theme;

/**
 * One document's editing UI: a Markdown editor plus its document, document
 * manager (open/save/close lifecycle), spell checker and find/replace.
 *
 * The live HTML preview is shared by the window and is repointed to the
 * active tab's document (see MainWindow).
 */
class DocumentTab : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentTab(const Theme &theme, QWidget *parent = nullptr);
    ~DocumentTab();

    MarkdownEditor *editor() const;
    MarkdownDocument *document() const;
    DocumentManager *documentManager() const;
    SpellCheckDecorator *spelling() const;
    FindReplace *findReplace() const;

signals:
    void fontSizeChanged(int size);

private:
    MarkdownEditor *m_editor;
    DocumentManager *m_documentManager;
    SpellCheckDecorator *m_spelling;
    FindReplace *m_findReplace;
};
} // namespace ghostwriter

#endif // DOCUMENTTAB_H
