/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QScreen>

#include "documenttab.h"

#include "documentmanager.h"
#include "findreplace.h"
#include "editor/markdowndocument.h"
#include "editor/markdowneditor.h"
#include "settings/appsettings.h"
#include "spelling/spellcheckdecorator.h"
#include "theme/theme.h"

namespace ghostwriter
{
DocumentTab::DocumentTab(const Theme &theme, QWidget *parent)
    : QWidget(parent)
{
    AppSettings *appSettings = AppSettings::instance();

    MarkdownDocument *doc = new MarkdownDocument();

    m_editor = new MarkdownEditor(doc, theme.lightColorScheme(), this);
    m_editor->setMinimumWidth(0.1 * qApp->primaryScreen()->size().width());
    m_editor->setFont(appSettings->editorFont().family(), appSettings->editorFont().pointSize());
    m_editor->setUseUnderlineForEmphasis(appSettings->useUnderlineForEmphasis());
    m_editor->setEnableLargeHeadingSizes(appSettings->largeHeadingSizesEnabled());
    m_editor->setAutoMatchEnabled(appSettings->autoMatchEnabled());
    m_editor->setBulletPointCyclingEnabled(appSettings->bulletPointCyclingEnabled());
    m_editor->setPlainText("");
    m_editor->setEditorWidth((EditorWidth) appSettings->editorWidth());
    m_editor->setEditorCorners((InterfaceStyle) appSettings->interfaceStyle());
    m_editor->setItalicizeBlockquotes(appSettings->italicizeBlockquotes());
    m_editor->setTabulationWidth(appSettings->tabWidth());
    m_editor->setInsertSpacesForTabs(appSettings->insertSpacesForTabsEnabled());
    m_editor->setShowUnbreakableSpaces(appSettings->showUnbreakableSpaceEnabled());

    m_editor->setAutoMatchEnabled('\"', appSettings->autoMatchCharEnabled('\"'));
    m_editor->setAutoMatchEnabled('\'', appSettings->autoMatchCharEnabled('\''));
    m_editor->setAutoMatchEnabled('(', appSettings->autoMatchCharEnabled('('));
    m_editor->setAutoMatchEnabled('[', appSettings->autoMatchCharEnabled('['));
    m_editor->setAutoMatchEnabled('{', appSettings->autoMatchCharEnabled('{'));
    m_editor->setAutoMatchEnabled('*', appSettings->autoMatchCharEnabled('*'));
    m_editor->setAutoMatchEnabled('_', appSettings->autoMatchCharEnabled('_'));
    m_editor->setAutoMatchEnabled('`', appSettings->autoMatchCharEnabled('`'));
    m_editor->setAutoMatchEnabled('<', appSettings->autoMatchCharEnabled('<'));

    this->connect(m_editor, &MarkdownEditor::fontSizeChanged, this, &DocumentTab::fontSizeChanged);

    m_spelling = new SpellCheckDecorator(m_editor);
    this->connect(
        appSettings,
        &AppSettings::spellCheckSettingsChanged,
        m_spelling,
        &SpellCheckDecorator::settingsChanged
    );

    m_documentManager = new DocumentManager(m_editor, this);
    m_documentManager->setAutoSaveEnabled(appSettings->autoSaveEnabled());
    m_documentManager->setFileBackupEnabled(appSettings->backupFileEnabled());
    m_documentManager->setDraftLocation(appSettings->draftLocation());
    m_documentManager->setBackupLocation(appSettings->backupLocation());
    m_documentManager->setFileHistoryEnabled(appSettings->fileHistoryEnabled());

    m_findReplace = new FindReplace(m_editor, this);

    // Editor settings that can change at runtime apply to every tab's editor.
    this->connect(appSettings, SIGNAL(tabWidthChanged(int)), m_editor, SLOT(setTabulationWidth(int)));
    this->connect(appSettings, SIGNAL(insertSpacesForTabsChanged(bool)), m_editor, SLOT(setInsertSpacesForTabs(bool)));
    this->connect(appSettings, &AppSettings::showUnbreakableSpaceEnabledChanged, m_editor, &MarkdownEditor::setShowUnbreakableSpaces);
    this->connect(appSettings, SIGNAL(useUnderlineForEmphasisChanged(bool)), m_editor, SLOT(setUseUnderlineForEmphasis(bool)));
    this->connect(appSettings, SIGNAL(italicizeBlockquotesChanged(bool)), m_editor, SLOT(setItalicizeBlockquotes(bool)));
    this->connect(appSettings, SIGNAL(largeHeadingSizesChanged(bool)), m_editor, SLOT(setEnableLargeHeadingSizes(bool)));
    this->connect(appSettings, SIGNAL(autoMatchChanged(bool)), m_editor, SLOT(setAutoMatchEnabled(bool)));
    this->connect(appSettings, SIGNAL(autoMatchCharChanged(QChar, bool)), m_editor, SLOT(setAutoMatchEnabled(QChar, bool)));
    this->connect(appSettings, SIGNAL(bulletPointCyclingChanged(bool)), m_editor, SLOT(setBulletPointCyclingEnabled(bool)));
}

DocumentTab::~DocumentTab()
{
    ;
}

MarkdownEditor *DocumentTab::editor() const
{
    return m_editor;
}

MarkdownDocument *DocumentTab::document() const
{
    return m_documentManager->document();
}

DocumentManager *DocumentTab::documentManager() const
{
    return m_documentManager;
}

SpellCheckDecorator *DocumentTab::spelling() const
{
    return m_spelling;
}

FindReplace *DocumentTab::findReplace() const
{
    return m_findReplace;
}
} // namespace ghostwriter
