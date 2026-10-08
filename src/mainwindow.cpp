/*
 * SPDX-FileCopyrightText: 2014-2023 Megan Conkle <megan.conkle@kdemail.net>
 * SPDX-FileCopyrightText: 2009-2014 Graeme Gott <graeme@gottcode.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDialog>
#include <QGridLayout>
#include <QIcon>
#include <QImageReader>
#include <QIODevice>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMimeDatabase>
#include <QMimeType>
#include <QScrollBar>
#include <QSettings>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTextDocumentFragment>

#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KHelpMenu>

#include <3rdparty/QtAwesome/QtAwesome.h>

#include "export/exporter.h"
#include "export/exporterfactory.h"
#include "settings/localedialog.h"
#include "settings/preferencesdialog.h"
#include "settings/previewoptionsdialog.h"
#include "settings/simplefontdialog.h"
#include "theme/stylesheetbuilder.h"
#include "theme/themeselectiondialog.h"
#include "spelling/spellcheckdecorator.h"
#include "spelling/spellcheckdialog.h"

#include "findreplace.h"
#include "library.h"
#include "mainwindow.h"
#include "messageboxhelper.h"

namespace ghostwriter
{
enum SidebarTabIndex {
    FirstSidebarTab,
    FilesSidebarTab = FirstSidebarTab,
    OutlineSidebarTab,
    SessionStatsSidebarTab,
    DocumentStatsSidebarTab,
    CheatSheetSidebarTab,
    LastSidebarTab = CheatSheetSidebarTab
};

#define GW_MAIN_WINDOW_GEOMETRY_KEY "Window/mainWindowGeometry"
#define GW_MAIN_WINDOW_STATE_KEY "Window/mainWindowState"
#define GW_SPLITTER_GEOMETRY_KEY "Window/splitterGeometry"

MainWindow::MainWindow(const QString &filePath, QWidget *parent)
    : QMainWindow(parent)
{
    QString fileToOpen;
    this->focusModeEnabled = false;
    this->awesome = new QtAwesome(qApp);
    this->awesome->initFontAwesome();
    this->setObjectName("mainWindow");
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    appSettings = AppSettings::instance();

    QString themeName = appSettings->themeName();

    QString err;
    ThemeRepository themeRepo(appSettings->themeDirectoryPath());
    theme = themeRepo.loadTheme(themeName, err);

    // mdwriter: documents live in tabs.  A DocumentTab owns the document,
    // editor, document manager, spell checker and find/replace for one file.
    documentArea = new DocumentArea(this);

    DocumentTab *firstTab = createDocumentTab();

    editor = firstTab->editor();
    spelling = firstTab->spelling();
    documentManager = firstTab->documentManager();
    findReplace = firstTab->findReplace();
    lastEditor = editor;

    this->setFocusProxy(editor);
    setWindowTitle(documentManager->document()->displayName() + "[*] - " + qAppName());

    buildSidebar();

    Library library;
    Bookmarks recentFiles;
    Bookmark fileBookmark(filePath);

    if (appSettings->fileHistoryEnabled()) {
        recentFiles = library.recentFiles(MAX_RECENT_FILES + 2);

        if (fileBookmark.isValid()) {
            recentFiles.removeAll(fileBookmark);
        }
    }

    if (!filePath.isNull() && !filePath.isEmpty()) {
        fileToOpen = filePath;

        // If the file passed as a command line argument does not exist, then
        // create it.
        if (!QFileInfo(fileToOpen).exists()) {
            QFile file(fileToOpen);
            file.open(QIODevice::WriteOnly);
            file.close();
        }
    }

    if (fileToOpen.isNull()
            && appSettings->fileHistoryEnabled()
            && appSettings->restoreSessionEnabled()) {
        QString lastFile;

        if (!recentFiles.isEmpty()) {
            lastFile = recentFiles.first().filePath();
        }

        if (!lastFile.isNull()) {
            fileToOpen = lastFile;
            recentFiles.removeAll(lastFile);
        }
    }

    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        recentFilesActions[i] = new QAction(this);

        this->connect
        (
            recentFilesActions[i],
            &QAction::triggered,
            this,
            [this, i]() {

                if (nullptr != recentFilesActions[i]) {
                    // Use the action's data for access to the actual file path, since
                    // KDE Plasma will add a keyboard accelerator to the action's text
                    // by inserting an ampersand (&) into it.
                    //
                    DocumentTab *tab = createDocumentTab(recentFilesActions[i]->data().toString());
                    documentArea->setCurrentIndex(documentArea->indexOf(tab));
                    refreshRecentFiles();
                }
            }
        );

        if (i < recentFiles.size()) {
            recentFilesActions[i]->setText(recentFiles.at(i).filePath());

            // Use the action's data for access to the actual file path, since
            // KDE Plasma will add a keyboard accelerator to the action's text
            // by inserting an ampersand (&) into it.
            //
            recentFilesActions[i]->setData(recentFiles.at(i).filePath());

            recentFilesActions[i]->setVisible(true);
        } else {
            recentFilesActions[i]->setVisible(false);
        }
    }

    // Set dimensions for the main window.  This is best done before
    // building the status bar, so that we can determine whether the full
    // screen button should be checked.
    //
    QSettings windowSettings;

    if (windowSettings.contains(GW_MAIN_WINDOW_GEOMETRY_KEY)) {
        restoreGeometry(windowSettings.value(GW_MAIN_WINDOW_GEOMETRY_KEY).toByteArray());
        restoreState(windowSettings.value(GW_MAIN_WINDOW_STATE_KEY).toByteArray());
    } else {
        this->adjustSize();
    }

    connect(appSettings, SIGNAL(autoSaveChanged(bool)), documentManager, SLOT(setAutoSaveEnabled(bool)));
    connect(appSettings, SIGNAL(backupFileChanged(bool)), documentManager, SLOT(setFileBackupEnabled(bool)));
    connect(appSettings, SIGNAL(backupLocationChanged(QString)), documentManager, SLOT(setBackupLocation(QString)));
    connect(appSettings, SIGNAL(tabWidthChanged(int)), editor, SLOT(setTabulationWidth(int)));
    connect(appSettings, SIGNAL(insertSpacesForTabsChanged(bool)), editor, SLOT(setInsertSpacesForTabs(bool)));
    connect(appSettings, &AppSettings::showUnbreakableSpaceEnabledChanged, editor, &MarkdownEditor::setShowUnbreakableSpaces);
    connect(appSettings, SIGNAL(useUnderlineForEmphasisChanged(bool)), editor, SLOT(setUseUnderlineForEmphasis(bool)));
    connect(appSettings, SIGNAL(italicizeBlockquotesChanged(bool)), editor, SLOT(setItalicizeBlockquotes(bool)));
    connect(appSettings, SIGNAL(largeHeadingSizesChanged(bool)), editor, SLOT(setEnableLargeHeadingSizes(bool)));
    connect(appSettings, SIGNAL(autoMatchChanged(bool)), editor, SLOT(setAutoMatchEnabled(bool)));
    connect(appSettings, SIGNAL(autoMatchCharChanged(QChar, bool)), editor, SLOT(setAutoMatchEnabled(QChar, bool)));
    connect(appSettings, SIGNAL(bulletPointCyclingChanged(bool)), editor, SLOT(setBulletPointCyclingEnabled(bool)));
    connect(appSettings, SIGNAL(autoMatchChanged(bool)), editor, SLOT(setAutoMatchEnabled(bool)));
    connect(appSettings, SIGNAL(focusModeChanged(FocusMode)), this, SLOT(changeFocusMode(FocusMode)));
    connect(appSettings, SIGNAL(hideMenuBarInFullScreenChanged(bool)), this, SLOT(toggleHideMenuBarInFullScreen(bool)));
    connect(appSettings, SIGNAL(fileHistoryChanged(bool)), this, SLOT(toggleFileHistoryEnabled(bool)));
    connect(appSettings, SIGNAL(displayTimeInFullScreenChanged(bool)), this, SLOT(toggleDisplayTimeInFullScreen(bool)));
    connect(appSettings, SIGNAL(editorWidthChanged(EditorWidth)), this, SLOT(changeEditorWidth(EditorWidth)));
    connect(appSettings, SIGNAL(interfaceStyleChanged(InterfaceStyle)), this, SLOT(changeInterfaceStyle(InterfaceStyle)));
    connect(appSettings, SIGNAL(previewTextFontChanged(QFont)), this, SLOT(applyTheme()));
    connect(appSettings, SIGNAL(previewCodeFontChanged(QFont)), this, SLOT(applyTheme()));

    editor->setShowUnbreakableSpaces(appSettings->showUnbreakableSpaceEnabled());

    if (this->isFullScreen() && appSettings->hideMenuBarInFullScreenEnabled()) {
        this->menuBar()->hide();
    }

    // Note that the parent widget for this new window must be NULL, so that
    // it will hide beneath other windows when it is deactivated.
    //
    htmlPreview = new HtmlPreview
    (
        documentManager->document(),
        appSettings->currentHtmlExporter(),
        this
    );

    connect(editor,
        &MarkdownEditor::typingPausedScaled,
        htmlPreview,
        &HtmlPreview::updatePreview);

    connect(documentManager,
        &DocumentManager::documentLoaded,
        htmlPreview,
        &HtmlPreview::updatePreview);

    connect(documentManager,
        &DocumentManager::documentClosed,
        htmlPreview,
        &HtmlPreview::updatePreview);

    connect(outlineWidget, SIGNAL(headingNumberNavigated(int)), htmlPreview, SLOT(navigateToHeading(int)));
    connect(appSettings, SIGNAL(currentHtmlExporterChanged(Exporter *)), htmlPreview, SLOT(setHtmlExporter(Exporter *)));

    htmlPreview->setMinimumWidth(0.1 * qApp->primaryScreen()->size().width());
    htmlPreview->setObjectName("htmlpreview");
    htmlPreview->setVisible(appSettings->htmlPreviewVisible());

    // mdwriter: tab switching / closing.
    this->connect(documentArea, &DocumentArea::currentChanged, this, &MainWindow::onCurrentTabChanged);
    this->connect(documentArea, &DocumentArea::tabCloseRequested, this, &MainWindow::closeTab);

    statusBarWidgets.append(this->findReplace);
    this->findReplace->setVisible(false);

    buildMenuBar();
    buildStatusBar();

    sidebar->setMinimumWidth(0.1 * qApp->primaryScreen()->size().width());

    splitter = new QSplitter(this);
    splitter->addWidget(sidebar);
    splitter->addWidget(documentArea);
    splitter->addWidget(htmlPreview);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 1);

    // Set default sizes for splitter.
    QList<int> sizes;
    int sidebarWidth = this->width() * 0.2;
    int otherWidth = (this->width() - sidebarWidth) / 2;
    sizes.append(sidebarWidth);
    sizes.append(otherWidth);
    sizes.append(otherWidth);

    splitter->setSizes(sizes);

    // If previous splitter geometry was stored, load it.
    if (windowSettings.contains(GW_SPLITTER_GEOMETRY_KEY)) {
        this->splitter->restoreState(windowSettings.value(GW_SPLITTER_GEOMETRY_KEY).toByteArray());
    }

    this->connect(splitter,
        &QSplitter::splitterMoved,
        [this](int pos, int index) {
            Q_UNUSED(pos)
            Q_UNUSED(index)
            adjustEditor();
        });

    this->setCentralWidget(splitter);

    qApp->installEventFilter(this);

    toggleHideMenuBarInFullScreen(appSettings->hideMenuBarInFullScreenEnabled());
    menuBarMenuActivated = false;

    // Need this call for GTK / Gnome 42 segmentation fault workaround.
    qApp->processEvents();

    show();

    // Apply the theme only after show() is called on all the widgets,
    // since the Outline scrollbars can end up transparent in Windows if
    // the theme is applied before show().  We cannot call show() and
    // then apply the theme in the constructor due to a bug with
    // Wayland + GTK that causes a segmentation fault.
    //
    applyTheme();

    // mdwriter: preview-first layout — hide the source editor pages unless
    // enabled (the tab bar stays visible).  Set BEFORE adjustEditor() so the
    // preview width cap is computed for the actual layout.
    documentArea->setVisible(appSettings->sourceEditorVisible());

    adjustEditor();

    // Show the theme right away before loading any files.
    qApp->processEvents();

    // Load file from command line or last session.
    if (!fileToOpen.isNull() && !fileToOpen.isEmpty()) {
        documentManager->open(fileToOpen);
    }
}

MainWindow::~MainWindow()
{
    ;
}

QSize MainWindow::sizeHint() const
{
    return QSize(800, 500);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    int width = event->size().width();

    if (width < (0.5 * qApp->primaryScreen()->size().width())) {
        this->sidebar->setVisible(false);
        this->sidebar->setAutoHideEnabled(true);
        this->sidebarHiddenForResize = true;
    }
    else {
        this->sidebarHiddenForResize = false;

        if (!this->focusModeEnabled && this->appSettings->sidebarVisible()) {
            this->sidebar->setAutoHideEnabled(false);
            this->sidebar->setVisible(true);
        }
        else {
            this->sidebar->setAutoHideEnabled(true);
            this->sidebar->setVisible(false);
        }
    }

    adjustEditor();
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    int key = e->key();

    switch (key) {
    case Qt::Key_Escape:
    case Qt::Key_F11:
        if (this->isFullScreen()) {
            toggleFullScreen(false);
        }
        break;
    case Qt::Key_Alt:
        if (this->isFullScreen() && appSettings->hideMenuBarInFullScreenEnabled()) {
            if (!this->menuBar()->isVisible()) {
                this->menuBar()->show();
            } else {
                this->menuBar()->hide();
            }
        }
        break;
    case Qt::Key_Tab:
        if (findReplace->isVisible() && findReplace->hasFocus()) {
            findReplace->keyPressEvent(e);
            return;
        }
        else if (!this->editor->hasFocus()) {
            QMainWindow::keyPressEvent(e);
        }
        break;
    default:
        break;
    }

    QMainWindow::keyPressEvent(e);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (this->isFullScreen() && appSettings->hideMenuBarInFullScreenEnabled()) {
        if ((this->menuBar() == obj) 
                && (QEvent::Leave == event->type()) 
                && !menuBarMenuActivated) {
            this->menuBar()->hide();
        } else if (QEvent::MouseMove == event->type()) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            if ((mouseEvent->globalPos().y()) <= 0 && !this->menuBar()->isVisible()) {
#else
            if ((mouseEvent->globalPosition().y()) <= 0 && !this->menuBar()->isVisible()) {
#endif
                this->menuBar()->show();
            }
        } else if ((this == obj) 
                && (((QEvent::Leave == event->type()) && !menuBarMenuActivated) 
                    || (QEvent::WindowDeactivate == event->type()))) {
            this->menuBar()->hide();
        }
    }

    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (documentManager->close()) {
        this->quitApplication();
    } else {
        event->ignore();
    }
}

void MainWindow::quitApplication()
{
    if (documentManager->close()) {
        appSettings->store();

        QSettings windowSettings;

        windowSettings.setValue(GW_MAIN_WINDOW_GEOMETRY_KEY, saveGeometry());
        windowSettings.setValue(GW_MAIN_WINDOW_STATE_KEY, saveState());
        windowSettings.setValue(GW_SPLITTER_GEOMETRY_KEY, splitter->saveState());
        windowSettings.sync();

        this->editor->document()->disconnect();
        this->editor->disconnect();
        this->htmlPreview->disconnect();
        StyleSheetBuilder::clearCache();

        qApp->quit();
    }
}

void MainWindow::changeTheme()
{
    ThemeSelectionDialog *themeDialog = new ThemeSelectionDialog(theme.name(), appSettings->darkModeEnabled(), this);
    themeDialog->setAttribute(Qt::WA_DeleteOnClose);

    this->connect
    (
        themeDialog,
        &ThemeSelectionDialog::finished,
        [this, themeDialog](int result) {
            Q_UNUSED(result)
            this->theme = themeDialog->theme();
            applyTheme();
        }
    );

    themeDialog->open();
}

void MainWindow::openPreferencesDialog()
{
    PreferencesDialog *preferencesDialog = new PreferencesDialog(this);
    preferencesDialog->show();
}

void MainWindow::toggleHtmlPreview(bool checked)
{
    htmlPreviewMenuAction->blockSignals(true);

    htmlPreviewMenuAction->setChecked(checked);
    htmlPreview->setVisible(checked);
    htmlPreview->updatePreview();

    htmlPreviewMenuAction->blockSignals(false);
    appSettings->setHtmlPreviewVisible(checked);
    this->update();
    adjustEditor();
}

void MainWindow::toggleSourceEditor(bool checked)
{
    sourceEditorMenuAction->blockSignals(true);

    sourceEditorMenuAction->setChecked(checked);
    documentArea->setVisible(checked);

    sourceEditorMenuAction->blockSignals(false);
    appSettings->setSourceEditorVisible(checked);
    this->update();
    adjustEditor();
}

void MainWindow::toggleHemingwayMode(bool checked)
{
    if (checked) {
        editor->setHemingWayModeEnabled(true);
    } else {
        editor->setHemingWayModeEnabled(false);
    }
}

void MainWindow::toggleFocusMode(bool checked)
{
    this->focusModeEnabled = checked;

    if (checked) {
        editor->setFocusMode(appSettings->focusMode());
        sidebar->setVisible(false);
        sidebar->setAutoHideEnabled(true);
    } else {
        editor->setFocusMode(FocusModeDisabled);

        if (!this->sidebarHiddenForResize && this->appSettings->sidebarVisible()) {
            sidebar->setAutoHideEnabled(false);
            sidebar->setVisible(true);
        }
    }
}

void MainWindow::toggleFullScreen(bool checked)
{
    static bool lastStateWasMaximized = false;

    this->fullScreenMenuAction->blockSignals(true);
    this->fullScreenMenuAction->setChecked(checked);
    this->fullScreenMenuAction->blockSignals(false);

    this->fullScreenButton->blockSignals(true);
    this->fullScreenButton->setChecked(checked);
    this->fullScreenButton->blockSignals(false);

    if (this->isFullScreen() || !checked) {
        if (appSettings->displayTimeInFullScreenEnabled()) {
            timeIndicator->hide();
        }

        // If the window had been maximized prior to entering
        // full screen mode, then put the window back to
        // to maximized.  Don't call showNormal(), as that
        // doesn't restore the window to maximized.
        //
        if (lastStateWasMaximized) {
            showMaximized();
        }
        // Put the window back to normal (not maximized).
        else {
            showNormal();
        }

        if (appSettings->hideMenuBarInFullScreenEnabled()) {
            this->menuBar()->show();
        }
    } else {
        if (appSettings->displayTimeInFullScreenEnabled()) {
            timeIndicator->show();
        }

        if (this->isMaximized()) {
            lastStateWasMaximized = true;
        } else {
            lastStateWasMaximized = false;
        }

        showFullScreen();

        if (appSettings->hideMenuBarInFullScreenEnabled()) {
            this->menuBar()->hide();
        }
    }
}

void MainWindow::toggleHideMenuBarInFullScreen(bool checked)
{
    if (this->isFullScreen()) {
        if (checked) {
            this->menuBar()->hide();
        } else {
            this->menuBar()->show();
        }
    }
}

void MainWindow::toggleFileHistoryEnabled(bool checked)
{
    if (!checked) {
        this->clearRecentFileHistory();
    }

    documentManager->setFileHistoryEnabled(checked);
}

void MainWindow::toggleDisplayTimeInFullScreen(bool checked)
{
    if (this->isFullScreen()) {
        if (checked) {
            this->timeIndicator->show();
        } else {
            this->timeIndicator->hide();
        }
    }
}

void MainWindow::changeEditorWidth(EditorWidth editorWidth)
{
    editor->setEditorWidth(editorWidth);
    adjustEditor();
}

void MainWindow::changeInterfaceStyle(InterfaceStyle style)
{
    Q_UNUSED(style);

    applyTheme();
}

void MainWindow::showQuickReferenceGuide()
{
    QDesktopServices::openUrl(QUrl("https://github.com/mhsihar/mdwriter#readme"));
}

void MainWindow::showWikiPage()
{
    QDesktopServices::openUrl(QUrl("https://github.com/mhsihar/mdwriter"));
}

void MainWindow::changeFocusMode(FocusMode focusMode)
{
    if (FocusModeDisabled != editor->focusMode()) {
        editor->setFocusMode(focusMode);
    }
}

void MainWindow::refreshRecentFiles()
{
    if (appSettings->fileHistoryEnabled()) {
        Library library;
        Bookmarks recentFiles = library.recentFiles(MAX_RECENT_FILES + 1);
        MarkdownDocument *document = documentManager->document();

        if (!document->isNew()) {
            recentFiles.removeAll(document->filePath());
        }

        for (int i = 0; (i < MAX_RECENT_FILES) && (i < recentFiles.size()); i++) {
            recentFilesActions[i]->setText(recentFiles.at(i).filePath());
            recentFilesActions[i]->setData(recentFiles.at(i).filePath());
            recentFilesActions[i]->setVisible(true);
        }

        for (int i = recentFiles.size(); i < MAX_RECENT_FILES; i++) {
            recentFilesActions[i]->setVisible(false);
        }
    }
}

void MainWindow::clearRecentFileHistory()
{
    Library library;
    library.clear();

    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        recentFilesActions[i]->setVisible(false);
    }
}

void MainWindow::changeDocumentDisplayName(const QString &displayName)
{
    setWindowTitle(displayName + QString("[*] - ") + qAppName());

    // mdwriter: keep the current tab's label in sync with the document.
    if (nullptr != documentArea) {
        const int index = documentArea->currentIndex();

        if (index >= 0) {
            documentArea->setTabText(index, displayName);

            const QString path = documentManager->document()->filePath();
            documentArea->setTabToolTip(index, path.isEmpty() ? displayName : path);
        }
    }

    if (documentManager->document()->isModified()) {
        setWindowModified(!appSettings->autoSaveEnabled());
    } else {
        setWindowModified(false);
    }
}

void MainWindow::onOperationStarted(const QString &description)
{
    if (!description.isNull()) {
        statusIndicator->setText(description);
    }

    statisticsIndicator->hide();
    statusIndicator->show();
    this->update();
    qApp->processEvents();
}

void MainWindow::onOperationFinished()
{
    statusIndicator->setText(QString());
    statisticsIndicator->show();
    statusIndicator->hide();
    this->update();
    qApp->processEvents();
}

void MainWindow::changeFont()
{
    bool success;

    QFont font =
        SimpleFontDialog::font(&success, editor->font(), this);

    if (success) {
        editor->setFont(font.family(), font.pointSize());
        appSettings->setEditorFont(font);
    }
}

void MainWindow::onFontSizeChanged(int size)
{
    QFont font = editor->font();
    font.setPointSize(size);
    appSettings->setEditorFont(font);
}

void MainWindow::onSetLocale()
{
    LocaleDialog *dialog = new LocaleDialog(this);
    dialog->show();
}

void MainWindow::copyHtml()
{
    Exporter *htmlExporter = appSettings->currentHtmlExporter();

    if (nullptr != htmlExporter) {
        QTextCursor c = editor->textCursor();
        QString markdownText;
        QString html;

        if (c.hasSelection()) {
            // Get only selected text from the document.
            markdownText = c.selection().toPlainText();
        } else {
            // Get all text from the document.
            markdownText = editor->toPlainText();
        }

        // Convert Markdown to HTML.
        htmlExporter->exportToHtml(markdownText, html);

        // Insert HTML into clipboard.
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(html);
    }
}

void MainWindow::showPreviewOptions()
{
    static PreviewOptionsDialog *dialog = new PreviewOptionsDialog(this);
    
    dialog->setModal(false);
    dialog->show();
}

void MainWindow::onAboutToHideMenuBarMenu()
{
    menuBarMenuActivated = false;

    if (!this->menuBar()->underMouse()
            && this->isFullScreen()
            && appSettings->hideMenuBarInFullScreenEnabled()
            && this->menuBar()->isVisible()) {
        this->menuBar()->hide();
    }
}

void MainWindow::onAboutToShowMenuBarMenu()
{
    menuBarMenuActivated = true;

    if (this->isFullScreen()
            && appSettings->hideMenuBarInFullScreenEnabled()
            && !this->menuBar()->isVisible()) {
        this->menuBar()->show();
    }
}

void MainWindow::onSidebarVisibilityChanged(bool visible)
{
    if (visible) {
        toggleSidebarButton->setText(QChar(fa::chevronleft));
    } else {
        toggleSidebarButton->setText(QChar(fa::chevronright));
    }

    if (!visible) {
        editor->setFocus();
    }

    this->showSidebarAction->blockSignals(true);
    this->showSidebarAction->setChecked(visible);
    this->showSidebarAction->blockSignals(false);

    this->adjustEditor();
}

void MainWindow::toggleSidebarVisible(bool visible)
{
    this->appSettings->setSidebarVisible(visible);

    if (!this->sidebarHiddenForResize
            && !this->focusModeEnabled
            && this->appSettings->sidebarVisible()) {
        sidebar->setAutoHideEnabled(false);
    }
    else {
        sidebar->setAutoHideEnabled(true);
    }

    this->sidebar->setVisible(visible);
    this->sidebar->setFocus();
    adjustEditor();
}

QAction* MainWindow::createWindowAction
(
    const QString &text,
    QObject *receiver,
    const char *member,
    const QKeySequence &shortcut
)
{
    // mdwriter: document actions (New/Open/Save/Reload/…) must act on the
    // *current* tab's document manager.
    if (nullptr != qobject_cast<DocumentManager *>(receiver)) {
        QAction *action = new QAction(text, this);
        action->setShortcut(shortcut);
        action->setShortcutContext(Qt::WindowShortcut);

        const QByteArray memberName(member);
        const QByteArray slot = memberName.left(memberName.indexOf('('));

        this->connect(action, &QAction::triggered, this, [this, slot]() {
            if (nullptr != documentManager) {
                QMetaObject::invokeMethod(documentManager, slot.constData());
            }
        });

        this->addAction(action);

        return action;
    }

    QAction* action = new QAction(text, this);
    action->setShortcut(shortcut);
    action->setShortcutContext(Qt::WindowShortcut);
    
    connect(action, SIGNAL(triggered(bool)), receiver, member);
    this->addAction(action);

    return action;
}

QAction* MainWindow::createWidgetAction
(
    const QString &text,
    QWidget *receiver,
    const char *member,
    const QKeySequence &shortcut
)
{
    // mdwriter: actions that target the Markdown editor must act on the
    // *current* tab's editor (the receiver is only the tab active when the
    // action was created).  Re-homed to the active editor on tab switch.
    if (nullptr != qobject_cast<MarkdownEditor *>(receiver)) {
        QAction *action = new QAction(text, this);
        action->setShortcut(shortcut);
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);

        const QByteArray memberName(member);
        const QByteArray slot = memberName.left(memberName.indexOf('('));

        this->connect(action, &QAction::triggered, this, [this, slot]() {
            MarkdownEditor *current = currentEditor();

            if (nullptr != current) {
                QMetaObject::invokeMethod(current, slot.constData());
            }
        });

        receiver->addAction(action);
        editorActions.append(action);

        return action;
    }

    QAction* action = new QAction(text, receiver);
    action->setShortcut(shortcut);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    
    connect(action, SIGNAL(triggered(bool)), receiver, member);
    receiver->addAction(action);

    return action;
}

void MainWindow::buildMenuBar()
{

    QMenu *fileMenu = this->menuBar()->addMenu(tr("&File"));

    fileMenu->addAction(createWindowAction(tr("&New"), documentManager, SLOT(close()), QKeySequence::New));

    // mdwriter: tab management.
    QAction *newTabAction = new QAction(tr("New &Tab"), this);
    newTabAction->setShortcut(QKeySequence("CTRL+T"));
    newTabAction->setShortcutContext(Qt::WindowShortcut);
    this->connect(newTabAction, &QAction::triggered, this, &MainWindow::newTab);
    fileMenu->addAction(newTabAction);
    this->addAction(newTabAction);

    QAction *closeTabAction = new QAction(tr("&Close Tab"), this);
    closeTabAction->setShortcut(QKeySequence("CTRL+W"));
    closeTabAction->setShortcutContext(Qt::WindowShortcut);
    this->connect(closeTabAction, &QAction::triggered, this, [this]() {
        closeTab(documentArea->currentIndex());
    });
    fileMenu->addAction(closeTabAction);
    this->addAction(closeTabAction);

    fileMenu->addAction(createWindowAction(tr("&Open"), this, SLOT(openFileInNewTab()), QKeySequence::Open));

    QMenu *recentFilesMenu = new QMenu(tr("Open &Recent..."), fileMenu);
    recentFilesMenu->addAction(createWindowAction(tr("Reopen Closed Tab"), this, SLOT(reopenClosedTab()), QKeySequence("SHIFT+CTRL+T")));
    recentFilesMenu->addSeparator();

    for (int i = 0; i < MAX_RECENT_FILES; i++) {
        recentFilesMenu->addAction(recentFilesActions[i]);
    }

    recentFilesMenu->addSeparator();
    recentFilesMenu->addAction(createWindowAction(tr("Clear Menu"), this, SLOT(clearRecentFileHistory())));

    fileMenu->addMenu(recentFilesMenu);

    fileMenu->addSeparator();
    fileMenu->addAction(createWindowAction(tr("&Save"), documentManager, SLOT(saveFile()), QKeySequence::Save));
    fileMenu->addAction(createWindowAction(tr("Save &As..."), documentManager, SLOT(saveAs()), QKeySequence::SaveAs));
    fileMenu->addAction(createWindowAction(tr("R&ename..."), documentManager, SLOT(rename())));
    fileMenu->addAction(createWindowAction(tr("Re&load from Disk..."), documentManager, SLOT(reload()), QKeySequence("CTRL+R")));
    fileMenu->addSeparator();
    fileMenu->addAction(createWindowAction(tr("&Export"), documentManager, SLOT(exportFile()), QKeySequence("CTRL+E")));
    fileMenu->addSeparator();
    QAction *quitAction = createWindowAction(tr("&Quit"), this, SLOT(quitApplication()), QKeySequence::Quit);
    quitAction->setMenuRole(QAction::QuitRole);
    fileMenu->addAction(quitAction);

    QMenu *editMenu = this->menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(createWidgetAction(tr("&Undo"), editor, SLOT(undo()), QKeySequence::Undo));
    editMenu->addAction(createWidgetAction(tr("&Redo"), editor, SLOT(redo()), QKeySequence::Redo));
    editMenu->addSeparator();
    editMenu->addAction(createWidgetAction(tr("Cu&t"), editor, SLOT(cut()), QKeySequence::Cut));
    editMenu->addAction(createWidgetAction(tr("&Copy"), editor, SLOT(copy()), QKeySequence::Copy));
    editMenu->addAction(createWidgetAction(tr("&Paste"), editor, SLOT(paste()), QKeySequence::Paste));
    editMenu->addAction(createWidgetAction(tr("Copy &HTML"), this, SLOT(copyHtml()), QKeySequence("SHIFT+CTRL+C")));
    editMenu->addSeparator();
    editMenu->addAction(createWidgetAction(tr("&Insert Image..."), editor, SLOT(insertImage())));
    editMenu->addSeparator();

    editMenu->addAction(createWindowAction(tr("&Find"), findReplace, SLOT(showFindView()), QKeySequence::Find));
    editMenu->addAction(createWindowAction(tr("Rep&lace"), findReplace, SLOT(showReplaceView()), QKeySequence::Replace));
    editMenu->addAction(createWindowAction(tr("Find &Next"), findReplace, SLOT(findNext()), QKeySequence::FindNext));
    editMenu->addAction(createWindowAction(tr("Find &Previous"), findReplace, SLOT(findPrevious()), QKeySequence::FindPrevious));
    editMenu->addSeparator();
    editMenu->addAction(createWindowAction(tr("&Spell check"), this, SLOT(runSpellCheck())));

    QMenu *formatMenu = this->menuBar()->addMenu(tr("For&mat"));
    formatMenu->addAction(createWidgetAction(tr("&Bold"), editor, SLOT(bold()), QKeySequence::Bold));
    formatMenu->addAction(createWidgetAction(tr("&Italic"), editor, SLOT(italic()), QKeySequence::Italic));
    formatMenu->addAction(createWidgetAction(tr("Stri&kethrough"), editor, SLOT(strikethrough()), QKeySequence("Ctrl+K")));
    formatMenu->addAction(createWidgetAction(tr("&Code Fences"), editor, SLOT(insertCodeFences()), QKeySequence("Ctrl+G")));
    formatMenu->addAction(createWidgetAction(tr("&HTML Comment"), editor, SLOT(insertComment()), QKeySequence("Ctrl+/")));
    formatMenu->addSeparator();

    formatMenu->addAction(createWidgetAction(tr("I&ndent"), editor, SLOT(indentText()), QKeySequence("Tab")));
    formatMenu->addAction(createWidgetAction(tr("&Unindent"), editor, SLOT(unindentText()), QKeySequence("Shift+Tab")));
    formatMenu->addSeparator();
    formatMenu->addAction(createWidgetAction(tr("Block &Quote"), editor, SLOT(createBlockquote()), QKeySequence("Ctrl+.")));
    formatMenu->addAction(createWidgetAction(tr("&Strip Block Quote"), editor, SLOT(removeBlockquote()), QKeySequence("Ctrl+,")));
    formatMenu->addSeparator();
    formatMenu->addAction(createWidgetAction(tr("&* Bullet List"), editor, SLOT(createBulletListWithAsteriskMarker()), QKeySequence("Ctrl+8")));
    formatMenu->addAction(createWidgetAction(tr("&- Bullet List"), editor, SLOT(createBulletListWithMinusMarker()), QKeySequence("Ctrl+Shift+-")));
    formatMenu->addAction(createWidgetAction(tr("&+ Bullet List"), editor, SLOT(createBulletListWithPlusMarker()), QKeySequence("Ctrl+Shift+=")));
    formatMenu->addSeparator();
    formatMenu->addAction(createWidgetAction(tr("1&. Numbered List"), editor, SLOT(createNumberedListWithPeriodMarker()), QKeySequence("Ctrl+1")));
    formatMenu->addAction(createWidgetAction(tr("1&) Numbered List"), editor, SLOT(createNumberedListWithParenthesisMarker()), QKeySequence("Ctrl+0")));
    formatMenu->addSeparator();
    formatMenu->addAction(createWidgetAction(tr("&Task List"), editor, SLOT(createTaskList()), QKeySequence("Ctrl+Shift+L")));
    formatMenu->addAction(createWidgetAction(tr("Toggle Task(s) &Complete"), editor, SLOT(toggleTaskComplete()), QKeySequence("Ctrl+D")));


    QMenu *viewMenu = this->menuBar()->addMenu(tr("&View"));

    fullScreenMenuAction = new QAction(tr("&Full Screen"), this);
    fullScreenMenuAction->setCheckable(true);
    fullScreenMenuAction->setChecked(this->isFullScreen());
    fullScreenMenuAction->setShortcut(QKeySequence("F11"));
    fullScreenMenuAction->setShortcutContext(Qt::WindowShortcut);
    connect(fullScreenMenuAction, SIGNAL(toggled(bool)), this, SLOT(toggleFullScreen(bool)));
    viewMenu->addAction(fullScreenMenuAction);
    
    htmlPreviewMenuAction = createWindowAction(tr("&Preview in HTML"), this, SLOT(toggleHtmlPreview(bool)), QKeySequence("CTRL+P"));
    htmlPreviewMenuAction->setCheckable(true);
    htmlPreviewMenuAction->setChecked(appSettings->htmlPreviewVisible());
    viewMenu->addAction(htmlPreviewMenuAction);

    sourceEditorMenuAction = createWindowAction(tr("Show &Editor"), this, SLOT(toggleSourceEditor(bool)), QKeySequence("CTRL+SHIFT+E"));
    sourceEditorMenuAction->setCheckable(true);
    sourceEditorMenuAction->setChecked(appSettings->sourceEditorVisible());
    viewMenu->addAction(sourceEditorMenuAction);

    this->addAction(showSidebarAction);
    viewMenu->addAction(showSidebarAction);

    QAction *showFilesTabAction = viewMenu->addAction(tr("&Files"),
        this,
        [this]() {
            sidebar->setVisible(true);
            sidebar->setCurrentTabIndex(FilesSidebarTab);
        },
        QKeySequence("CTRL+SHIFT+F"));
    showFilesTabAction->setShortcutContext(Qt::WindowShortcut);
    this->addAction(showFilesTabAction);

    QAction *showSidebarTabAction = viewMenu->addAction(tr("&Outline"),
        this,
        [this]() {
            sidebar->setVisible(true);
            sidebar->setCurrentTabIndex(OutlineSidebarTab);
        },
        QKeySequence("CTRL+J"));
    showSidebarTabAction->setShortcutContext(Qt::WindowShortcut);
    this->addAction(showSidebarTabAction);
    
    showSidebarTabAction = viewMenu->addAction(tr("&Session Statistics"),
        this,
        [this]() {
            sidebar->setVisible(true);
            sidebar->setCurrentTabIndex(SessionStatsSidebarTab);
        });
    showSidebarTabAction->setShortcutContext(Qt::WindowShortcut);
    this->addAction(showSidebarTabAction);

    showSidebarTabAction = viewMenu->addAction(tr("&Document Statistics"),
        this,
        [this]() {
            sidebar->setVisible(true);
            sidebar->setCurrentTabIndex(DocumentStatsSidebarTab);
        });
    showSidebarTabAction->setShortcutContext(Qt::WindowShortcut);
    this->addAction(showSidebarTabAction);

    showSidebarTabAction = viewMenu->addAction(tr("&Cheat Sheet"),
        this,
        [this]() {
            sidebar->setVisible(true);
            sidebar->setCurrentTabIndex(CheatSheetSidebarTab);
        },
        QKeySequence::HelpContents);
    showSidebarTabAction->setShortcutContext(Qt::WindowShortcut);
    this->addAction(showSidebarTabAction);
    
    viewMenu->addSeparator();
    viewMenu->addAction(createWidgetAction(tr("Increase Font Size"), editor, SLOT(increaseFontSize()), QKeySequence("CTRL+=")));
    viewMenu->addAction(createWidgetAction(tr("Decrease Font Size"), editor, SLOT(decreaseFontSize()), QKeySequence("CTRL+-")));

    QMenu *settingsMenu = this->menuBar()->addMenu(tr("&Settings"));
    settingsMenu->addAction(createWindowAction(tr("Themes..."), this, SLOT(changeTheme())));
    settingsMenu->addAction(createWindowAction(tr("Font..."), this, SLOT(changeFont())));
    settingsMenu->addAction(createWindowAction(tr("Application Language..."), this, SLOT(onSetLocale())));
    settingsMenu->addAction(createWindowAction(tr("Preview Options..."), this, SLOT(showPreviewOptions())));
    QAction *preferencesAction = createWindowAction(tr("Preferences..."), this, SLOT(openPreferencesDialog()));
    preferencesAction->setMenuRole(QAction::PreferencesRole);
    settingsMenu->addAction(preferencesAction);

    KHelpMenu *kHelpMenu = new KHelpMenu(this, KAboutData::applicationData(), false);

    QMenu *helpMenu = this->menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(createWindowAction(
        kHelpMenu->action(KHelpMenu::menuAboutApp)->text(), kHelpMenu, SLOT(aboutApplication())));
    helpMenu->addAction(createWindowAction(
        kHelpMenu->action(KHelpMenu::menuAboutKDE)->text(), kHelpMenu, SLOT(aboutKDE())));
    helpMenu->addAction(createWindowAction(
        kHelpMenu->action(KHelpMenu::menuReportBug)->text(), kHelpMenu, SLOT(reportBug())));
    helpMenu->addAction(createWindowAction(
        kHelpMenu->action(KHelpMenu::menuDonate)->text(), kHelpMenu, SLOT(donate())));
    helpMenu->addSeparator();
    helpMenu->addAction(createWindowAction(tr("Quick &Reference Guide"), this, SLOT(showQuickReferenceGuide())));
    helpMenu->addAction(createWindowAction(tr("Wiki"), this, SLOT(showWikiPage())));

    this->menuBar()->addMenu(helpMenu);

    connect(fileMenu, SIGNAL(aboutToShow()), this, SLOT(onAboutToShowMenuBarMenu()));
    connect(fileMenu, SIGNAL(aboutToHide()), this, SLOT(onAboutToHideMenuBarMenu()));
    connect(editMenu, SIGNAL(aboutToShow()), this, SLOT(onAboutToShowMenuBarMenu()));
    connect(editMenu, SIGNAL(aboutToHide()), this, SLOT(onAboutToHideMenuBarMenu()));
    connect(formatMenu, SIGNAL(aboutToShow()), this, SLOT(onAboutToShowMenuBarMenu()));
    connect(formatMenu, SIGNAL(aboutToHide()), this, SLOT(onAboutToHideMenuBarMenu()));
    connect(viewMenu, SIGNAL(aboutToShow()), this, SLOT(onAboutToShowMenuBarMenu()));
    connect(viewMenu, SIGNAL(aboutToHide()), this, SLOT(onAboutToHideMenuBarMenu()));
    connect(settingsMenu, SIGNAL(aboutToShow()), this, SLOT(onAboutToShowMenuBarMenu()));
    connect(settingsMenu, SIGNAL(aboutToHide()), this, SLOT(onAboutToHideMenuBarMenu()));
    connect(helpMenu, SIGNAL(aboutToShow()), this, SLOT(onAboutToShowMenuBarMenu()));
    connect(helpMenu, SIGNAL(aboutToHide()), this, SLOT(onAboutToHideMenuBarMenu()));
}

void MainWindow::buildStatusBar()
{
    QGridLayout *statusBarLayout = new QGridLayout();
    statusBarLayout->setSpacing(0);
    statusBarLayout->setContentsMargins(0, 0, 0, 0);

    statusBarLayout->addWidget(this->findReplace, 0, 0, 1, 3);

    // Divide the status bar into thirds for placing widgets.
    QFrame *leftWidget = new QFrame(this->statusBar());
    leftWidget->setObjectName("leftStatusBarWidget");
    QFrame *midWidget = new QFrame(this->statusBar());
    midWidget->setObjectName("midStatusBarWidget");
    QFrame *rightWidget = new QFrame(this->statusBar());
    rightWidget->setObjectName("rightStatusBarWidget");

    QHBoxLayout *leftLayout = new QHBoxLayout(leftWidget);
    leftWidget->setLayout(leftLayout);
    leftLayout->setContentsMargins(0,0,0,0);
    QHBoxLayout *midLayout = new QHBoxLayout(midWidget);
    midWidget->setLayout(midLayout);
    midLayout->setContentsMargins(0,0,0,0);
    QHBoxLayout *rightLayout = new QHBoxLayout(rightWidget);
    rightWidget->setLayout(rightLayout);
    rightLayout->setContentsMargins(0,0,0,0);

    // Add left-most widgets to status bar.
    toggleSidebarButton = new QPushButton(QChar(fa::chevronright));
    toggleSidebarButton->setObjectName("showSidebarButton");
    toggleSidebarButton->setFocusPolicy(Qt::NoFocus);
    toggleSidebarButton->setToolTip(tr("Toggle sidebar"));
    toggleSidebarButton->setCheckable(false);
    toggleSidebarButton->setChecked(false);

    leftLayout->addWidget(toggleSidebarButton, 0, Qt::AlignLeft);
    statusBarWidgets.append(toggleSidebarButton);

    if (appSettings->sidebarVisible()) {
        toggleSidebarButton->setText(QChar(fa::chevronleft));
    }

    this->connect(toggleSidebarButton,
        &QPushButton::clicked,
        [this]() {
            toggleSidebarVisible(!sidebar->isVisible());
        }
    );
    
    timeIndicator = new TimeLabel(this);
    leftLayout->addWidget(timeIndicator, 0, Qt::AlignLeft);
    leftWidget->setContentsMargins(0, 0, 0, 0);
    statusBarWidgets.append(timeIndicator);

    if (!this->isFullScreen() || appSettings->displayTimeInFullScreenEnabled()) {
        timeIndicator->hide();
    }

    statusBarLayout->addWidget(leftWidget, 1, 0, 1, 1, Qt::AlignLeft);

    // Add middle widgets to status bar.
    statusIndicator = new QLabel();
    midLayout->addWidget(statusIndicator, 0, Qt::AlignCenter);
    statusIndicator->hide();

    statisticsIndicator = new StatisticsIndicator(this->documentStats, this->sessionStats, this);

    if ((appSettings->favoriteStatistic() >= 0)
            && (appSettings->favoriteStatistic() < statisticsIndicator->count())) {
        statisticsIndicator->setCurrentIndex(appSettings->favoriteStatistic());
    }
    else {
        statisticsIndicator->setCurrentIndex(0);
    }

    this->connect(statisticsIndicator,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        appSettings,
        &AppSettings::setFavoriteStatistic);

    midLayout->addWidget(statisticsIndicator, 0, Qt::AlignCenter);
    midWidget->setContentsMargins(0, 0, 0, 0);
    statusBarLayout->addWidget(midWidget, 1, 1, 1, 1, Qt::AlignCenter);
    statusBarWidgets.append(statisticsIndicator);

    // Add right-most widgets to status bar.
    QPushButton *button = new QPushButton(QChar(fa::moon));
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolTip(tr("Toggle dark mode"));
    button->setCheckable(true);
    button->setChecked(appSettings->darkModeEnabled());

    this->connect
    (
        button,
        &QPushButton::toggled,
        [this](bool enabled) {
            appSettings->setDarkModeEnabled(enabled);
            applyTheme();
        }
    );

    rightLayout->addWidget(button, 0, Qt::AlignRight);
    statusBarWidgets.append(button);

    button = new QPushButton(QChar(fa::code));
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolTip(tr("Toggle Live HTML Preview"));
    button->setCheckable(true);
    button->setChecked(appSettings->htmlPreviewVisible());
    connect(button, SIGNAL(toggled(bool)), this, SLOT(toggleHtmlPreview(bool)));
    rightLayout->addWidget(button, 0, Qt::AlignRight);
    statusBarWidgets.append(button);

    this->connect
    (
        this->htmlPreviewMenuAction,
        &QAction::toggled,
        [button](bool checked) {
            button->blockSignals(true);
            button->setChecked(checked);
            button->blockSignals(false);
        }
    );

    button = new QPushButton(QChar(fa::edit));
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolTip(tr("Toggle Source Editor"));
    button->setCheckable(true);
    button->setChecked(appSettings->sourceEditorVisible());
    connect(button, SIGNAL(toggled(bool)), this, SLOT(toggleSourceEditor(bool)));
    rightLayout->addWidget(button, 0, Qt::AlignRight);
    statusBarWidgets.append(button);
    sourceEditorButton = button;

    this->connect
    (
        this->sourceEditorMenuAction,
        &QAction::toggled,
        [button](bool checked) {
            button->blockSignals(true);
            button->setChecked(checked);
            button->blockSignals(false);
        }
    );

    button = new QPushButton(QChar(fa::backspace));
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolTip(tr("Toggle Hemingway mode"));
    button->setCheckable(true);
    connect(button, SIGNAL(toggled(bool)), this, SLOT(toggleHemingwayMode(bool)));
    rightLayout->addWidget(button, 0, Qt::AlignRight);
    statusBarWidgets.append(button);

    button = new QPushButton(QChar(fa::headphonesalt));
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolTip(tr("Toggle distraction free mode"));
    button->setCheckable(true);
    connect(button, SIGNAL(toggled(bool)), this, SLOT(toggleFocusMode(bool)));
    rightLayout->addWidget(button, 0, Qt::AlignRight);
    statusBarWidgets.append(button);

    button = new QPushButton(QChar(fa::expand));
    button->setFocusPolicy(Qt::NoFocus);
    button->setObjectName("fullscreenButton");
    button->setToolTip(tr("Toggle full screen mode"));
    button->setCheckable(true);
    button->setChecked(this->isFullScreen());
    connect(button, SIGNAL(toggled(bool)), this, SLOT(toggleFullScreen(bool)));
    rightLayout->addWidget(button, 0, Qt::AlignRight);
    statusBarWidgets.append(button);
    this->fullScreenButton = button;

    rightWidget->setContentsMargins(0, 0, 0, 0);
    statusBarLayout->addWidget(rightWidget, 1, 2, 1, 1, Qt::AlignRight);
    
    QWidget *container = new QWidget(this);
    container->setObjectName("statusBarWidgetContainer");
    container->setLayout(statusBarLayout);
    container->setContentsMargins(0, 0, 2, 0);
    container->setStyleSheet("#statusBarWidgetContainer { border: 0; margin: 0; padding: 0 }");

    this->statusBar()->addWidget(container, 1);
    this->statusBar()->setSizeGripEnabled(false);
}

void MainWindow::buildSidebar()
{
    this->showSidebarAction = new QAction(tr("Show Sidebar"), this);
    showSidebarAction->setCheckable(true);
    showSidebarAction->setChecked(appSettings->sidebarVisible());
    showSidebarAction->setShortcut(QKeySequence("CTRL+SPACE"));
    showSidebarAction->setShortcutContext(Qt::WindowShortcut);
    
    connect(this->showSidebarAction,
        &QAction::toggled,
        [this](bool visible) {
            this->toggleSidebarVisible(visible);
        });

    // mdwriter: file explorer that follows the current document.
    fileExplorer = new FileExplorer(this);
    this->connect(
        fileExplorer,
        &FileExplorer::fileActivated,
        [this](const QString &path) {
            DocumentTab *tab = createDocumentTab(path);
            documentArea->setCurrentIndex(documentArea->indexOf(tab));
        }
    );

    cheatSheetWidget = new QListWidget(this);

    cheatSheetWidget->setSelectionMode(QAbstractItemView::NoSelection);
    cheatSheetWidget->setAlternatingRowColors(false);

    cheatSheetWidget->addItem(tr("# Heading 1"));
    cheatSheetWidget->addItem(tr("## Heading 2"));
    cheatSheetWidget->addItem(tr("### Heading 3"));
    cheatSheetWidget->addItem(tr("#### Heading 4"));
    cheatSheetWidget->addItem(tr("##### Heading 5"));
    cheatSheetWidget->addItem(tr("###### Heading 6"));
    cheatSheetWidget->addItem(tr("*Emphasis* _Emphasis_"));
    cheatSheetWidget->addItem(tr("**Strong** __Strong__"));
    cheatSheetWidget->addItem(tr("1. Numbered List"));
    cheatSheetWidget->addItem(tr("* Bullet List"));
    cheatSheetWidget->addItem(tr("+ Bullet List"));
    cheatSheetWidget->addItem(tr("- Bullet List"));
    cheatSheetWidget->addItem(tr("> Block Quote"));
    cheatSheetWidget->addItem(tr("`Code Span`"));
    cheatSheetWidget->addItem(tr("``` Code Block"));
    cheatSheetWidget->addItem(tr("[Link](http://url.com \"Title\")"));
    cheatSheetWidget->addItem(tr("[Reference Link][ID]"));
    cheatSheetWidget->addItem(tr("[ID]: http://url.com \"Reference Definition\""));
    cheatSheetWidget->addItem(tr("![Image](./image.jpg \"Title\")"));
    cheatSheetWidget->addItem(tr("--- *** ___ Horizontal Rule"));

    documentStatsWidget = new DocumentStatisticsWidget(this);
    documentStatsWidget->setSelectionMode(QAbstractItemView::NoSelection);
    documentStatsWidget->setAlternatingRowColors(false);

    sessionStatsWidget = new SessionStatisticsWidget(this);
    sessionStatsWidget->setSelectionMode(QAbstractItemView::NoSelection);
    sessionStatsWidget->setAlternatingRowColors(false);

    outlineWidget = new OutlineWidget(editor, this);
    outlineWidget->setAlternatingRowColors(false);

    documentStats = new DocumentStatistics((MarkdownDocument *) editor->document(), this);
    connect(documentStats, &DocumentStatistics::wordCountChanged,
            documentStatsWidget, &DocumentStatisticsWidget::setWordCount);
    connect(documentStats, &DocumentStatistics::characterCountChanged,
            documentStatsWidget, &DocumentStatisticsWidget::setCharacterCount);
    connect(documentStats, &DocumentStatistics::sentenceCountChanged,
            documentStatsWidget, &DocumentStatisticsWidget::setSentenceCount);
    connect(documentStats, &DocumentStatistics::paragraphCountChanged,
            documentStatsWidget, &DocumentStatisticsWidget::setParagraphCount);
    connect(documentStats, &DocumentStatistics::pageCountChanged,
            documentStatsWidget, &DocumentStatisticsWidget::setPageCount);
    connect(documentStats, &DocumentStatistics::complexWordsChanged,
            documentStatsWidget, &DocumentStatisticsWidget::setComplexWords);
    connect(documentStats, &DocumentStatistics::readingTimeChanged,
            documentStatsWidget, &DocumentStatisticsWidget::setReadingTime);
    connect(documentStats, &DocumentStatistics::lixReadingEaseChanged,
            documentStatsWidget, &DocumentStatisticsWidget::setLixReadingEase);
    connect(documentStats, &DocumentStatistics::readabilityIndexChanged,
            documentStatsWidget, &DocumentStatisticsWidget::setReadabilityIndex);
    connect(editor, SIGNAL(textSelected(QString, int, int)), documentStats, SLOT(onTextSelected(QString, int, int)));
    connect(editor, SIGNAL(textDeselected()), documentStats, SLOT(onTextDeselected()));

    sessionStats = new SessionStatistics(this);
    connect(documentStats, SIGNAL(totalWordCountChanged(int)), sessionStats, SLOT(onDocumentWordCountChanged(int)));
    connect(sessionStats, SIGNAL(wordCountChanged(int)), sessionStatsWidget, SLOT(setWordCount(int)));
    connect(sessionStats, SIGNAL(pageCountChanged(int)), sessionStatsWidget, SLOT(setPageCount(int)));
    connect(sessionStats, SIGNAL(wordsPerMinuteChanged(int)), sessionStatsWidget, SLOT(setWordsPerMinute(int)));
    connect(sessionStats, SIGNAL(writingTimeChanged(int)), sessionStatsWidget, SLOT(setWritingTime(int)));
    connect(sessionStats, SIGNAL(idleTimePercentageChanged(int)), sessionStatsWidget, SLOT(setIdleTime(int)));
    connect(editor, SIGNAL(typingPaused()), sessionStats, SLOT(onTypingPaused()));
    connect(editor, SIGNAL(typingResumed()), sessionStats, SLOT(onTypingResumed()));

    sidebar = new Sidebar(this);
    sidebar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sidebar->setMinimumWidth(0.1 * QGuiApplication::primaryScreen()->availableSize().width());
    sidebar->setMaximumWidth(0.5 * QGuiApplication::primaryScreen()->availableSize().width());

    sidebar->addTab(QChar(fa::folder), fileExplorer, tr("Files"));
    sidebar->addTab(QChar(fa::hashtag), outlineWidget, tr("Outline"));
    sidebar->addTab(QChar(fa::tachometeralt), sessionStatsWidget, tr("Session Statistics"));
    sidebar->addTab(QChar(fa::chartbar), documentStatsWidget, tr("Document Statistics"));
    sidebar->addTab(QChar(fa::markdown), cheatSheetWidget, tr("Cheat Sheet"), "cheatSheetTab");

    int tabIndex = QSettings().value("sidebarCurrentTab", (int)FirstSidebarTab).toInt();

    if (tabIndex < 0 || tabIndex >= sidebar->tabCount()) {
        tabIndex = (int) FirstSidebarTab;
    }

    sidebar->setCurrentTabIndex(tabIndex);

    QPushButton *button = sidebar->addButton(QChar(fa::cog), tr("Settings"));
    this->connect
    (
        button,
        &QPushButton::clicked,
        [this, button]() {
            QMenu *popupMenu = new QMenu(button);
            popupMenu->addAction(tr("Themes..."), this, SLOT(changeTheme()));
            popupMenu->addAction(tr("Font..."), this, SLOT(changeFont()));
            popupMenu->addAction(tr("Application Language..."), this, SLOT(onSetLocale()));
            popupMenu->addAction(tr("Preview Options..."), this, SLOT(showPreviewOptions()));
            popupMenu->addAction(tr("Preferences..."),
                this,
                SLOT(openPreferencesDialog()))->setMenuRole(QAction::PreferencesRole);
            popupMenu->popup(button->mapToGlobal(QPoint(button->width() / 2,
                -(button->height() / 2) - 10)));
        }
    );

    this->connect(this->sidebar,
        &Sidebar::visibilityChanged,
        this,
        &MainWindow::onSidebarVisibilityChanged);
    
    if (!this->sidebarHiddenForResize
            && !this->focusModeEnabled
            && appSettings->sidebarVisible()) {
        this->sidebar->setAutoHideEnabled(false);
        this->sidebar->setVisible(true);
    }
    else {
        this->sidebar->setAutoHideEnabled(true);
        this->sidebar->setVisible(false);
    }
}

DocumentTab *MainWindow::currentTab() const
{
    return (nullptr != documentArea) ? documentArea->currentTab() : nullptr;
}

MarkdownEditor *MainWindow::currentEditor() const
{
    DocumentTab *tab = currentTab();

    return (nullptr != tab) ? tab->editor() : editor;
}

void MainWindow::rehomeEditorActions()
{
    if (lastEditor == editor) {
        return;
    }

    for (QAction *action : editorActions) {
        if (nullptr != lastEditor) {
            lastEditor->removeAction(action);
        }

        if (nullptr != editor) {
            editor->addAction(action);
        }
    }

    lastEditor = editor;
}

DocumentTab *MainWindow::createDocumentTab(const QString &filePath)
{
    DocumentTab *tab = new DocumentTab(theme, documentArea);
    documentArea->addTab(tab, tr("Untitled 1"));

    this->connect(tab, &DocumentTab::fontSizeChanged, this, &MainWindow::onFontSizeChanged);

    DocumentManager *dm = tab->documentManager();

    this->connect(dm, &DocumentManager::documentDisplayNameChanged, this,
        [this, tab](const QString &name) {
            const int i = documentArea->indexOf(tab);

            if (i >= 0) {
                documentArea->setTabText(i, name);

                const QString path = tab->document()->filePath();
                documentArea->setTabToolTip(i, path.isEmpty() ? name : path);
            }

            if (tab == currentTab()) {
                setWindowTitle(name + QString("[*] - ") + qAppName());
            }
        });

    this->connect(dm, &DocumentManager::documentModifiedChanged, this,
        [this, tab](bool modified) {
            const int i = documentArea->indexOf(tab);

            if (i >= 0) {
                const QString name = tab->document()->displayName();
                documentArea->setTabText(i, modified ? name + QStringLiteral("*") : name);
            }

            if (tab == currentTab()) {
                setWindowModified(modified && !appSettings->autoSaveEnabled());
            }
        });

    this->connect(dm, &DocumentManager::operationStarted, this, &MainWindow::onOperationStarted);
    this->connect(dm, &DocumentManager::operationUpdate, this,
        [this](const QString &description) { onOperationStarted(description); });
    this->connect(dm, &DocumentManager::operationFinished, this, &MainWindow::onOperationFinished);
    this->connect(dm, &DocumentManager::documentClosed, this,
        [this]() { refreshRecentFiles(); });
    this->connect(dm, &DocumentManager::documentLoaded, this,
        [this, tab]() {
            refreshRecentFiles();

            if (tab == currentTab() && nullptr != sessionStats && nullptr != documentStats) {
                sessionStats->startNewSession(documentStats->wordCount());
            }

            if (tab == currentTab() && nullptr != fileExplorer) {
                fileExplorer->reveal(tab->document()->filePath());
            }
        });

    if (!filePath.isEmpty()) {
        dm->open(filePath);
    }

    return tab;
}

void MainWindow::newTab()
{
    DocumentTab *tab = createDocumentTab();
    documentArea->setCurrentIndex(documentArea->indexOf(tab));
}

void MainWindow::openFileInNewTab()
{
    DocumentTab *tab = createDocumentTab();
    const int index = documentArea->indexOf(tab);

    documentArea->setCurrentIndex(index);
    tab->documentManager()->open();

    // If the user cancelled the dialog, drop the empty tab.
    if (tab->document()->isNew() && tab->document()->isEmpty()
            && documentArea->count() > 1) {
        documentArea->removeTab(index);
    }
}

void MainWindow::closeTab(int index)
{
    DocumentTab *tab = documentArea->tabAt(index);

    if (nullptr == tab) {
        return;
    }

    const QString path = tab->document()->filePath();

    // Prompts to save if modified; resets the document on success.
    if (!tab->documentManager()->close()) {
        return;
    }

    if (documentArea->count() <= 1) {
        // Keep at least one tab (it was just reset to a new document).
        return;
    }

    if (!path.isEmpty()) {
        closedTabPaths.append(path);
    }

    documentArea->removeTab(index);
}

void MainWindow::reopenClosedTab()
{
    while (!closedTabPaths.isEmpty()) {
        const QString path = closedTabPaths.takeLast();

        if (QFileInfo::exists(path)) {
            DocumentTab *tab = createDocumentTab(path);
            documentArea->setCurrentIndex(documentArea->indexOf(tab));
            return;
        }
    }
}

void MainWindow::onCurrentTabChanged(int index)
{
    Q_UNUSED(index)

    DocumentTab *tab = currentTab();

    if (nullptr == tab) {
        return;
    }

    // Detach the shared preview from the previous document.
    if (nullptr != editor && nullptr != htmlPreview) {
        disconnect(editor, &MarkdownEditor::typingPausedScaled, htmlPreview, &HtmlPreview::updatePreview);
    }

    if (nullptr != documentManager && nullptr != htmlPreview) {
        disconnect(documentManager, &DocumentManager::documentLoaded, htmlPreview, &HtmlPreview::updatePreview);
        disconnect(documentManager, &DocumentManager::documentClosed, htmlPreview, &HtmlPreview::updatePreview);
    }

    editor = tab->editor();
    spelling = tab->spelling();
    documentManager = tab->documentManager();
    findReplace = tab->findReplace();

    rehomeEditorActions();

    // Repoint the shared sidebar widgets to the new document.
    if (nullptr != outlineWidget) {
        outlineWidget->setEditor(editor);
    }

    if (nullptr != documentStats) {
        documentStats->setDocument(documentManager->document());
    }

    if (nullptr != htmlPreview) {
        this->connect(editor, &MarkdownEditor::typingPausedScaled, htmlPreview, &HtmlPreview::updatePreview);
        this->connect(documentManager, &DocumentManager::documentLoaded, htmlPreview, &HtmlPreview::updatePreview);
        this->connect(documentManager, &DocumentManager::documentClosed, htmlPreview, &HtmlPreview::updatePreview);
        htmlPreview->setDocument(tab->document());
    }

    setWindowTitle(documentManager->document()->displayName() + QString("[*] - ") + qAppName());
    setWindowModified(documentManager->document()->isModified() && !appSettings->autoSaveEnabled());

    if (nullptr != sourceEditorMenuAction) {
        sourceEditorMenuAction->blockSignals(true);
        sourceEditorMenuAction->setChecked(documentArea->isVisible());
        sourceEditorMenuAction->blockSignals(false);
    }

    editor->setFocus();
    adjustEditor();
}

void MainWindow::adjustEditor()
{
    // Make sure editor size is updated.
    qApp->processEvents();

    int width = this->width();
    int sidebarWidth = 0;

    if (this->sidebar->isVisible()) {
        sidebarWidth = this->sidebar->width();
    }

    // mdwriter: when the source editor is hidden (preview-first layout), let the
    // live preview maximize across the full width.  Otherwise keep the upstream
    // 50% cap so the preview does not crowd out the editor.
    //
    if (this->editor->isVisible()) {
        this->htmlPreview->setMaximumWidth((width - sidebarWidth) / 2);
    } else {
        this->htmlPreview->setMaximumWidth(QWIDGETSIZE_MAX);
    }

    // Resize the editor's margins.
    this->editor->setupPaperMargins();

    // Scroll to cursor position.
    this->editor->centerCursor();
}

void MainWindow::applyTheme()
{
    if (!theme.name().isNull() && !theme.name().isEmpty()) {
        appSettings->setThemeName(theme.name());
    }

    ColorScheme colorScheme = theme.lightColorScheme();

    if (appSettings->darkModeEnabled()) {
        colorScheme = theme.darkColorScheme();
    }

    StyleSheetBuilder styler(colorScheme,
        (InterfaceStyleRounded == appSettings->interfaceStyle()),
        appSettings->editorFont(),
        appSettings->previewTextFont(),
        appSettings->previewCodeFont());

    editor->setColorScheme(colorScheme);
    spelling->setErrorColor(colorScheme.error);

    // Do not call this->setStyleSheet().  Calling it more than once in a run
    // (i.e., when changing a theme) causes a crash in Qt 5.11.  Instead,
    // change the main window's style sheet via qApp.
    //
    QString styleSheet = styler.widgetStyleSheet();

    if (styleSheet.isNull()) {
        qCritical() << "Invalid widget style sheet provided.";
    } else {
        qApp->style()->unpolish(qApp);
        qApp->style()->unpolish(this);
        qApp->setStyleSheet(styleSheet);
        qApp->style()->polish(qApp);
        qApp->style()->polish(this);
    }

    styleSheet = styler.htmlPreviewStyleSheet();

    if (styleSheet.isNull()) {
        qCritical() << "Invalid HTML preview style sheet provided.";
    } else {
        htmlPreview->setStyleSheet(styler.htmlPreviewStyleSheet());
    }

    adjustEditor();
}

void MainWindow::runSpellCheck()
{
    SpellCheckDialog *dialog = new SpellCheckDialog(this->editor);
    connect(
        dialog,
        &SpellCheckDialog::finished,
        this->spelling,
        &SpellCheckDecorator::rehighlight
    );

    dialog->show();
}

} // namespace ghostwriter
