/*
 * SPDX-FileCopyrightText: 2014-2023 Megan Conkle <megan.conkle@kdemail.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

#include <3rdparty/QtAwesome/QtAwesome.h>

#include "../messageboxhelper.h"

#include "settings/appsettings.h"
#include "colorschemepreviewer.h"
#include "themeeditordialog.h"
#include "themerepository.h"
#include "themeselectiondialog.h"
#include "vscodethemeimporter.h"

#define GW_LIST_WIDGET_ICON_WIDTH 300
#define GW_LIST_WIDGET_ICON_HEIGHT 175

namespace ghostwriter
{
class ThemeSelectionDialogPrivate
{
    Q_DECLARE_PUBLIC(ThemeSelectionDialog)

public:
    ThemeSelectionDialogPrivate(ThemeSelectionDialog *q_ptr)
        : q_ptr(q_ptr),
          repo(AppSettings::instance()->themeDirectoryPath())
    {
        currentTheme = repo.defaultTheme();
        awesome = new QtAwesome(q_ptr);
        awesome->initFontAwesome();
    }

    ~ThemeSelectionDialogPrivate()
    {
        ;
    }

    ThemeSelectionDialog *q_ptr;
    QtAwesome *awesome;

    ThemeRepository repo;
    bool darkModeEnabled;
    QListWidget *themeListWidget;
    Theme currentTheme;
    bool currentThemeIsValid;
    bool currentThemeIsNew;
    int iconWidth;
    int iconHeight;

    // Use this list to count where the custom
    // themes start in the theme selection list.
    //
    QStringList builtInThemes;

    void buildThemeList(const QString &currentThemeName = nullptr);
    void loadSelectedTheme();
    void createNewTheme();
    void deleteTheme();
    void editTheme();
    void importFromVscode();
    void updateTheme(const Theme &theme);
};

ThemeSelectionDialog::ThemeSelectionDialog
(
    const QString &currentThemeName,
    const bool darkModeEnabled,
    QWidget *parent
)
    : QDialog(parent),
      d_ptr(new ThemeSelectionDialogPrivate(this))
{
    Q_D(ThemeSelectionDialog);

    d->iconWidth = GW_LIST_WIDGET_ICON_WIDTH;
    d->iconHeight = GW_LIST_WIDGET_ICON_HEIGHT;

    d->darkModeEnabled = darkModeEnabled;

    this->setWindowTitle(tr("Themes"));
    this->setAttribute(Qt::WA_DeleteOnClose);

    QGridLayout *layout = new QGridLayout();

    // Find view sizes
    int focush = style()->pixelMetric(QStyle::PM_FocusFrameHMargin);
    int focusv = style()->pixelMetric(QStyle::PM_FocusFrameVMargin);
    int frame = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    int scrollbar = style()->pixelMetric(QStyle::PM_SliderThickness) +
                    style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    const int spacing = 10;
    const int rowCount = 3;
    const int columnCount = 3;
    QSize viewSize
    (
        ((d->iconWidth + frame + focush + (spacing * 2)) * columnCount) + scrollbar,
        (d->iconHeight + fontMetrics().height() + frame + focusv + (spacing * 2)) * rowCount
    );

    // Set up theme list
    d->themeListWidget = new QListWidget(this);
    d->themeListWidget->setSortingEnabled(true);
    d->themeListWidget->setFlow(QListView::LeftToRight);
    d->themeListWidget->setViewMode(QListView::IconMode);
    d->themeListWidget->setIconSize(QSize(d->iconWidth, d->iconHeight));
    d->themeListWidget->setSpacing(spacing);
    d->themeListWidget->setMovement(QListView::Static);
    d->themeListWidget->setResizeMode(QListView::Adjust);
    d->themeListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    d->themeListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->themeListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    d->themeListWidget->setWordWrap(true);
    d->themeListWidget->setUniformItemSizes(true);
    d->themeListWidget->setMinimumSize(viewSize);

    d->currentThemeIsValid = true;
    d->currentThemeIsNew = false;

    d->buildThemeList(currentThemeName);

    QCheckBox *darkModeCheckbox = new QCheckBox("Preview in dark mode", this);
    darkModeCheckbox->setChecked(darkModeEnabled);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    QPushButton *newThemeButton = new QPushButton(QChar(fa::plus));
    newThemeButton->setFont(d->awesome->font(style::stfas, newThemeButton->font().pointSize()));
    QPushButton *deleteThemeButton = new QPushButton(QChar(fa::minus));
    deleteThemeButton->setFont(d->awesome->font(style::stfas, deleteThemeButton->font().pointSize()));
    QPushButton *editThemeButton = new QPushButton(tr("Edit..."));
    QPushButton *importVscodeButton = new QPushButton(tr("Import from VS Code..."));

    buttonBox->addButton(newThemeButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(deleteThemeButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(editThemeButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(importVscodeButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(QDialogButtonBox::Close);

    layout->addWidget(d->themeListWidget, 0, 0, 1, 2);
    layout->addWidget(darkModeCheckbox, 1, 0, 1, 1, Qt::AlignLeft);
    layout->addWidget(buttonBox, 1, 1, 1, 1, Qt::AlignRight);

    this->setLayout(layout);

    connect(buttonBox->button(QDialogButtonBox::Close), SIGNAL(clicked()), this, SLOT(close()));
    this->connect(
        newThemeButton,
        &QPushButton::clicked,
        [d]() {
            d->createNewTheme();
        }
    );
    this->connect(
        deleteThemeButton,
        &QPushButton::clicked,
        [d]() {
            d->deleteTheme();
        }
    );
    this->connect(
        darkModeCheckbox,
        &QCheckBox::toggled,
        [d](bool enabled) {
            d->darkModeEnabled = enabled;
            d->buildThemeList();
        }
    );
    this->connect(
        editThemeButton,
        &QPushButton::clicked,
        [d]() {
            d->editTheme();
        }
    );
    this->connect(
        importVscodeButton,
        &QPushButton::clicked,
        [d]() {
            d->importFromVscode();
        }
    );
    this->connect(
        d->themeListWidget,
        &QListWidget::itemSelectionChanged,
        [d]() {
            d->loadSelectedTheme();
        }
    );
}

ThemeSelectionDialog::~ThemeSelectionDialog()
{
    ;
}

const Theme &ThemeSelectionDialog::theme() const
{
    Q_D(const ThemeSelectionDialog);
    
    return d->currentTheme;
}

void ThemeSelectionDialogPrivate::buildThemeList(const QString &currentThemeName)
{
    qreal dpr = 1.0;

#if QT_VERSION >= 0x050600
    dpr = this->themeListWidget->devicePixelRatioF();
#endif

    QStringList availableThemes = repo.availableThemes();
    QList<QListWidgetItem *> selected = this->themeListWidget->selectedItems();
    QString currentName = currentThemeName;

    if (!selected.isEmpty()) {
        currentName = selected.first()->text();
    }

    this->themeListWidget->clear();

    for (int i = 0; i < availableThemes.size(); i++) {
        QString themeName = availableThemes[i];
        QString err;
        QIcon themeIcon;

        Theme theme = repo.loadTheme(themeName, err);

        ColorScheme colors;

        if (this->darkModeEnabled && theme.hasDarkColorScheme()) {
            colors = theme.darkColorScheme();
        } else {
            colors = theme.lightColorScheme();
        }

        ColorSchemePreviewer previewer(
            colors,
            theme.isReadOnly(),
            (err.isNull() || err.isEmpty()),
            GW_LIST_WIDGET_ICON_WIDTH,
            GW_LIST_WIDGET_ICON_HEIGHT,
            dpr
        );

        themeIcon = previewer.icon();

        QListWidgetItem *item = new QListWidgetItem(
            themeIcon,
            themeName,
            this->themeListWidget
        );

        if (!err.isNull() && !err.isEmpty()) {
            item->setToolTip(err);
        }

        this->themeListWidget->insertItem(this->themeListWidget->count(), item);

        if (themeName == currentName) {
            this->themeListWidget->setCurrentItem(item);
            this->currentTheme = theme;
            this->currentThemeIsValid = (err.isNull() || err.isEmpty());
        }
    }
}

void ThemeSelectionDialogPrivate::loadSelectedTheme()
{
    QList<QListWidgetItem *> selectedThemes = themeListWidget->selectedItems();
    QString themeName;

    if (selectedThemes.isEmpty()
            || selectedThemes[0]->text().isNull()
            || selectedThemes[0]->text().isEmpty()) {
        return;
    }

    currentThemeIsValid = true;
    themeName = selectedThemes[0]->text();

    QString err;

    if (!currentThemeIsNew) {
        this->currentTheme = repo.loadTheme(themeName, err);
    }
}

void ThemeSelectionDialogPrivate::createNewTheme()
{
    Q_Q(ThemeSelectionDialog);
    
    QString err = QString();

    QList<QListWidgetItem *> selectedThemes = themeListWidget->selectedItems();

    Theme newTheme = repo.defaultTheme();

    if (selectedThemes.size() > 0) {
        newTheme = repo.loadTheme(
            selectedThemes[0]->text(),
            err
        );
    }

    // It doesn't matter if there was an error, since loadTheme will
    // return a default, built-in theme if it fails.
    //
    err = QString();

    QIcon themeIcon;

    ColorScheme colors = newTheme.lightColorScheme();

    if (darkModeEnabled && newTheme.hasDarkColorScheme()) {
        colors = newTheme.darkColorScheme();
    }

    ColorSchemePreviewer previewer
    (
        colors,
        false,
        false,
        iconWidth,
        iconHeight
    );

    themeIcon = previewer.icon();

    newTheme.setName(repo.generateUntitledThemeName());
    newTheme.setReadOnly(false);

    repo.saveTheme(newTheme.name(), newTheme, err);

    QListWidgetItem *item =
        new QListWidgetItem(themeIcon, newTheme.name(), themeListWidget);
    themeListWidget->insertItem(themeListWidget->count(), item);

    currentThemeIsNew = true;
    currentTheme = newTheme;
    currentThemeIsValid = true;
    this->themeListWidget->setCurrentItem(item);

    ThemeEditorDialog *themeEditorDialog = new ThemeEditorDialog(newTheme, q);
    themeEditorDialog->setAttribute(Qt::WA_DeleteOnClose);

    q->connect(
        themeEditorDialog,
        &ThemeEditorDialog::finished,
        themeEditorDialog,
        [this, themeEditorDialog](int result) {
            Q_UNUSED(result)
            updateTheme(themeEditorDialog->theme());
        }
    );

    themeEditorDialog->open();
}


void ThemeSelectionDialogPrivate::deleteTheme()
{
    Q_Q(ThemeSelectionDialog);
    
    QList<QListWidgetItem *> selectedThemes = themeListWidget->selectedItems();

    if (selectedThemes.isEmpty()) {
        // Ignore if no theme is selected.
        return;
    }

    // If the theme is one of the built in themes, display an error.
    //
    if (currentTheme.isReadOnly()) {
        MessageBoxHelper::critical(
            q,
            ThemeSelectionDialog::tr("Cannot delete theme."),
            ThemeSelectionDialog::tr("Sorry, this is a built-in theme that cannot be deleted.")
        );
        return;
    }

    QString themeName = selectedThemes[0]->text();

    int result =
        MessageBoxHelper::question(
            q,
            ThemeSelectionDialog::tr("Are you sure you want to permanently delete the '%1' theme?")
            .arg(themeName),
            QString(),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

    if (QMessageBox::Yes == result) {
        QString err;

        repo.deleteTheme(themeName, err);

        if (!err.isNull()) {
            MessageBoxHelper::critical(
                q,
                ThemeSelectionDialog::tr("Failed to delete theme."),
                err
            );
        } else {
            int row = themeListWidget->row(selectedThemes[0]);
            QListWidgetItem *item = themeListWidget->takeItem(row);

            if (nullptr != item) {
                delete item;
                item = nullptr;
            }
        }
    }
}

void ThemeSelectionDialogPrivate::editTheme()
{
    Q_Q(ThemeSelectionDialog);
    
    QList<QListWidgetItem *> selectedThemes = themeListWidget->selectedItems();

    if (!selectedThemes.isEmpty()) {
        // If the theme is one of the built in themes, display an error.
        //
        if (currentTheme.isReadOnly()) {
            MessageBoxHelper::critical(
                q,
                ThemeSelectionDialog::tr("Cannot edit theme."),
                ThemeSelectionDialog::tr("Sorry, this is a built-in theme that cannot be edited.")
            );
            return;
        }

        Theme themeToEdit = currentTheme;

        if (!currentThemeIsValid) {
            QString err;
            themeToEdit = repo.loadTheme("", err);
            themeToEdit.setName(currentTheme.name());
        }

        ThemeEditorDialog *themeEditorDialog = new ThemeEditorDialog(themeToEdit, q);
        themeEditorDialog->setAttribute(Qt::WA_DeleteOnClose);

        q->connect(
            themeEditorDialog,
            &ThemeEditorDialog::finished,
            [this, themeEditorDialog](int result) {
                if (QDialog::Accepted == result) {
                    updateTheme(themeEditorDialog->theme());
                }
            }
        );

        themeEditorDialog->open();
    }
}

void ThemeSelectionDialogPrivate::importFromVscode()
{
    Q_Q(ThemeSelectionDialog);

    const QList<VscodeThemeInfo> discovered = VscodeThemeImporter::discoverThemes();

    QDialog dialog(q);
    dialog.setWindowTitle(ThemeSelectionDialog::tr("Import VS Code Themes"));
    dialog.setMinimumSize(520, 420);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        ThemeSelectionDialog::tr(
            "Select one or more VS Code themes to import (built-in and installed extensions):"),
        &dialog
    ));

    QListWidget *listWidget = new QListWidget(&dialog);
    listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    for (const VscodeThemeInfo &info : discovered) {
        QListWidgetItem *item = new QListWidgetItem(info.name, listWidget);
        item->setData(Qt::UserRole, info.path);
        item->setToolTip(info.path);
    }

    layout->addWidget(listWidget);

    QPushButton *browseButton =
        new QPushButton(ThemeSelectionDialog::tr("Browse for theme file..."), &dialog);
    layout->addWidget(browseButton);

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QObject::connect(
        browseButton,
        &QPushButton::clicked,
        &dialog,
        [&dialog, listWidget, q]() {
            const QStringList files = QFileDialog::getOpenFileNames(
                q,
                ThemeSelectionDialog::tr("Select VS Code theme JSON file(s)"),
                QDir::homePath(),
                ThemeSelectionDialog::tr("VS Code theme JSON (*.json)")
            );

            for (const QString &file : files) {
                QListWidgetItem *item =
                    new QListWidgetItem(QFileInfo(file).completeBaseName(), listWidget);
                item->setData(Qt::UserRole, file);
                item->setToolTip(file);
                item->setSelected(true);
            }
        }
    );

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QList<QListWidgetItem *> selected = listWidget->selectedItems();

    if (selected.isEmpty()) {
        return;
    }

    QString lastImported;
    QStringList errors;
    int imported = 0;

    for (QListWidgetItem *item : selected) {
        const QString path = item->data(Qt::UserRole).toString();
        ColorScheme scheme;
        QString err;

        if (!VscodeThemeImporter::loadColorScheme(path, scheme, err)) {
            errors << QStringLiteral("%1: %2").arg(item->text(), err);
            continue;
        }

        const QString baseName = QStringLiteral("VS Code · ") + item->text();
        QString themeName = baseName;
        int suffix = 2;

        while (QFileInfo::exists(repo.themeFilePath(themeName))) {
            themeName = QStringLiteral("%1 (%2)").arg(baseName).arg(suffix++);
        }

        Theme theme(themeName, scheme);
        theme.setReadOnly(false);

        repo.saveTheme(themeName, theme, err);

        if (err.isNull() || err.isEmpty()) {
            imported++;
            lastImported = themeName;
        } else {
            errors << QStringLiteral("%1: %2").arg(themeName, err);
        }
    }

    if (!errors.isEmpty()) {
        MessageBoxHelper::warning(
            q,
            ThemeSelectionDialog::tr("Some themes could not be imported."),
            errors.join(QStringLiteral("\n"))
        );
    }

    if (imported > 0) {
        currentThemeIsNew = false;
        buildThemeList(lastImported);
    }
}

void ThemeSelectionDialogPrivate::updateTheme(const Theme &theme)
{
    // Update theme name and preview icon, as applicable.
    //
    QList<QListWidgetItem *> selectedThemes = themeListWidget->selectedItems();

    if (!selectedThemes.isEmpty()) {
        QListWidgetItem *item = selectedThemes[0];

        if (nullptr != item) {
            if (theme.name() != currentTheme.name()) {
                item->setText(theme.name());
            }

            currentTheme = theme;
            currentThemeIsValid = true;
            ColorScheme colors;

            if (darkModeEnabled && theme.hasDarkColorScheme()) {
                colors = theme.darkColorScheme();
            } else {
                colors = theme.lightColorScheme();
            }

            ColorSchemePreviewer previewer(
                colors,
                theme.isReadOnly(),
                true,
                iconWidth,
                iconHeight
            );

            QIcon themeIcon = previewer.icon();
            item->setIcon(themeIcon);
        }
    }

    currentThemeIsNew = false;
}
} // namespace ghostwriter
