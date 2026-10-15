/*
 * SPDX-FileCopyrightText: 2016-2023 Megan Conkle <megan.conkle@kdemail.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLocale>
#include <QMessageBox>
#include <QVBoxLayout>

#include "localedialog.h"
#include "appsettings.h"
#include "../messageboxhelper.h"

namespace ghostwriter
{

class LocaleDialogPrivate
{
public:

    LocaleDialogPrivate() { }
    ~LocaleDialogPrivate() { }

    QComboBox *languageCombo;
};

LocaleDialog::LocaleDialog
(
    QWidget *parent
) : QDialog(parent), d(new LocaleDialogPrivate())
{
    setWindowTitle(tr("Set Application Language"));
    setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout();

    // mdwriter: use a standard combo box so every available language is
    // discoverable (the upstream KLanguageButton popup was easy to miss).
    d->languageCombo = new QComboBox(this);
    d->languageCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    auto currentLanguageCode = AppSettings::instance()->locale();
    auto languageCodes = AppSettings::instance()->availableTranslations();

    if (currentLanguageCode.isNull() || currentLanguageCode.isEmpty()) {
        currentLanguageCode = QLocale().name();
    }

    QString selectedLanguage;

    if (languageCodes.isEmpty()) {
        MessageBoxHelper::critical(
            this,
            tr("No translations are available!"),
            tr("Please reinstall this application for more language options.")
        );
    } else {
        const QLocale currentLocale(currentLanguageCode);

        enum {
            NoMatch,
            LanguageMatch,
            LanguageCountryMatch,
            PerfectMatch
        } matchAccuracy = NoMatch;

        for (const auto &languageCode : languageCodes) {
            const QLocale locale(languageCode);

            QString name = locale.nativeLanguageName();

            if (name.isEmpty()) {
                name = QLocale::languageToString(locale.language());
            }

            d->languageCombo->addItem(
                QStringLiteral("%1 (%2)").arg(name, languageCode),
                languageCode
            );

            if (QLatin1String("C") == currentLocale.name()) {
                continue;
            }

            if ((matchAccuracy < LanguageMatch)
                    && (locale.language() == currentLocale.language())) {
                matchAccuracy = LanguageMatch;
                selectedLanguage = languageCode;
            }

            if ((LanguageMatch == matchAccuracy)
                    && (locale.country() == currentLocale.country())) {
                matchAccuracy = LanguageCountryMatch;
                selectedLanguage = languageCode;
            }

            if ((LanguageCountryMatch == matchAccuracy)
                    && locale.script() == currentLocale.script()) {
                selectedLanguage = languageCode;
                matchAccuracy = PerfectMatch;
            }

            // This case covers local dialects, such as "ca@valencia".
            // See QTBUG-7100 for details.
            if (languageCode == currentLanguageCode) {
                selectedLanguage = languageCode;
                matchAccuracy = PerfectMatch;
            }
        }
    }

    if (selectedLanguage.isNull()) {
        selectedLanguage = "en";
    }

    if (d->languageCombo->count() <= 0) {
        // Insert fall-back language.
        d->languageCombo->addItem(
            QStringLiteral("%1 (%2)")
                .arg(QLocale(QLatin1String("en")).nativeLanguageName(),
                     QStringLiteral("en")),
            QStringLiteral("en")
        );
    }

    int selectedIndex = d->languageCombo->findData(selectedLanguage);

    if (selectedIndex < 0) {
        selectedIndex = d->languageCombo->findData(QStringLiteral("en"));
    }

    if (selectedIndex >= 0) {
        d->languageCombo->setCurrentIndex(selectedIndex);
    }

    layout->addWidget(d->languageCombo);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->addButton(QDialogButtonBox::Ok);
    buttonBox->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    connect(
        this,
        &LocaleDialog::accepted,
        [this]() {
            QString languageCode = d->languageCombo->currentData().toString();

            if (!AppSettings::instance()->setLocale(languageCode)) {
                MessageBoxHelper::critical(
                    this,
                    tr("Sorry!"),
                    tr("Could not load translation.")
                );
            } else {
                QMessageBox::information(
                    this,
                    QApplication::applicationName(),
                    tr("Please restart the application for changes to take effect.")
                );
            }
        }
    );

    this->setLayout(layout);
}

LocaleDialog::~LocaleDialog()
{
    ;
}

} // namespace ghostwriter
