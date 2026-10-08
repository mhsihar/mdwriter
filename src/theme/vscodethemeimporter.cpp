/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include "vscodethemeimporter.h"

namespace ghostwriter
{
namespace
{
/**
 * Reads a JSON file into a JSON object.  On failure, err is populated and an
 * empty object is returned.
 */
QJsonObject readJsonObject(const QString &path, QString &err)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        err = QStringLiteral("Could not open '%1'.").arg(path);
        return QJsonObject();
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        err = QStringLiteral("Invalid JSON in '%1': %2")
              .arg(path, parseError.errorString());
        return QJsonObject();
    }

    return doc.object();
}

/**
 * VS Code allows #RGB, #RRGGBB and #RRGGBBAA.  QColor does not understand the
 * 4- or 8-digit alpha forms, so strip any alpha channel.
 */
QString normalizeHexColor(const QString &value)
{
    QString color = value.trimmed();

    if (!color.startsWith('#')) {
        return color;
    }

    if (color.size() == 9) {
        return color.left(7);
    }

    if (color.size() == 5) {
        return QStringLiteral("#") + color.mid(1, 3);
    }

    return color;
}

QColor parseColor(const QString &value)
{
    if (value.isEmpty()) {
        return QColor();
    }

    QColor color(normalizeHexColor(value));

    return color.isValid() ? color : QColor();
}

QColor blend(const QColor &a, const QColor &b, qreal t)
{
    return QColor(
        qRound(a.red()   + (b.red()   - a.red())   * t),
        qRound(a.green() + (b.green() - a.green()) * t),
        qRound(a.blue()  + (b.blue()  - a.blue())  * t)
    );
}

struct TokenRule
{
    QStringList scopes;
    QColor foreground;
};

/**
 * Parses the "tokenColors" (or legacy "settings") array into a flat list of
 * scope -> foreground rules.
 */
QList<TokenRule> parseTokenRules(const QJsonObject &theme)
{
    QList<TokenRule> rules;

    QJsonValue value = theme.contains("tokenColors")
                       ? theme.value("tokenColors")
                       : theme.value("settings");

    if (!value.isArray()) {
        return rules;
    }

    const QJsonArray array = value.toArray();

    for (const QJsonValue &entry : array) {
        if (!entry.isObject()) {
            continue;
        }

        const QJsonObject rule = entry.toObject();
        const QJsonValue scopeValue = rule.value("scope");

        QStringList scopes;

        if (scopeValue.isString()) {
            scopes << scopeValue.toString();
        } else if (scopeValue.isArray()) {
            for (const QJsonValue &s : scopeValue.toArray()) {
                if (s.isString()) {
                    scopes << s.toString();
                }
            }
        }

        if (scopes.isEmpty()) {
            continue;
        }

        const QJsonObject settings = rule.value("settings").toObject();
        QColor foreground = parseColor(settings.value("foreground").toString());

        if (!foreground.isValid()) {
            continue;
        }

        TokenRule tokenRule;
        tokenRule.scopes = scopes;
        tokenRule.foreground = foreground;
        rules.append(tokenRule);
    }

    return rules;
}

/**
 * Returns the foreground of the *last* token rule whose scope matches any of
 * the target scopes (VS Code applies later rules with higher precedence).
 * Returns fallback when nothing matches.
 */
QColor tokenColor
(
    const QList<TokenRule> &rules,
    const QStringList &targets,
    const QColor &fallback
)
{
    QColor result = fallback;

    for (const TokenRule &rule : rules) {
        bool ruleMatches = false;

        for (const QString &scope : rule.scopes) {
            for (const QString &target : targets) {
                if (scope == target
                        || scope.startsWith(target + '.')
                        || target.startsWith(scope + '.')) {
                    ruleMatches = true;
                    break;
                }
            }

            if (ruleMatches) {
                break;
            }
        }

        if (ruleMatches) {
            result = rule.foreground;
        }
    }

    return result;
}

QColor workbenchColor
(
    const QJsonObject &colors,
    const QString &key,
    const QColor &fallback
)
{
    QColor color = parseColor(colors.value(key).toString());

    return color.isValid() ? color : fallback;
}

/**
 * Resolves an extension theme label.  VS Code package.json labels may be
 * "%token%" placeholders that resolve via package.nls.json.
 */
QString resolveLabel
(
    const QString &extensionDir,
    const QString &label,
    const QString &themePath
)
{
    if (!label.isEmpty() && !(label.startsWith('%') && label.endsWith('%'))) {
        return label;
    }

    if (label.startsWith('%') && label.endsWith('%')) {
        QString err;
        QJsonObject nls =
            readJsonObject(extensionDir + QStringLiteral("/package.nls.json"), err);
        QString resolved = nls.value(label.mid(1, label.size() - 2)).toString();

        if (!resolved.isEmpty()) {
            return resolved;
        }
    }

    QString err;
    QJsonObject theme = readJsonObject(themePath, err);
    QString name = theme.value("name").toString();

    if (!name.isEmpty()) {
        return name;
    }

    return QFileInfo(themePath).completeBaseName();
}
} // namespace

QList<VscodeThemeInfo> VscodeThemeImporter::discoverThemes()
{
    QStringList roots;
    roots << QStringLiteral("/usr/share/code/resources/app/extensions")
          << QStringLiteral("/usr/lib/code/extensions");

    const QString home = QDir::homePath();
    roots << home + QStringLiteral("/.vscode/extensions")
          << home + QStringLiteral("/.vscode-server/extensions")
          << home + QStringLiteral("/.vscode-oss/extensions")
          << home + QStringLiteral("/.vscode-insiders/extensions")
          << home + QStringLiteral("/.vscode-shared/extensions");

    QList<VscodeThemeInfo> result;
    QSet<QString> seenPaths;

    for (const QString &root : roots) {
        QDir rootDir(root);

        if (!rootDir.exists()) {
            continue;
        }

        const QStringList extensionDirs =
            rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QString &extension : extensionDirs) {
            const QString extensionDir = rootDir.filePath(extension);
            const QString packagePath = extensionDir + QStringLiteral("/package.json");

            if (!QFileInfo::exists(packagePath)) {
                continue;
            }

            QString err;
            QJsonObject package = readJsonObject(packagePath, err);
            QJsonArray themes =
                package.value("contributes").toObject().value("themes").toArray();

            for (const QJsonValue &themeValue : themes) {
                const QJsonObject theme = themeValue.toObject();
                const QString relativePath = theme.value("path").toString();

                if (relativePath.isEmpty()) {
                    continue;
                }

                const QString absolutePath =
                    QDir(extensionDir).filePath(relativePath);

                if (!QFileInfo::exists(absolutePath) || seenPaths.contains(absolutePath)) {
                    continue;
                }

                VscodeThemeInfo info;
                info.name = resolveLabel(
                    extensionDir,
                    theme.value("label").toString(),
                    absolutePath
                );
                info.path = absolutePath;

                seenPaths.insert(absolutePath);
                result.append(info);
            }
        }
    }

    return result;
}

bool VscodeThemeImporter::loadColorScheme
(
    const QString &path,
    ColorScheme &scheme,
    QString &err
)
{
    err.clear();

    QJsonObject theme = readJsonObject(path, err);

    if (theme.isEmpty()) {
        return false;
    }

    // Resolve a single level of "include" (the VS Code base theme).
    QJsonObject base;

    QJsonValue includeValue = theme.value("include");

    if (includeValue.isString()) {
        const QString includePath =
            QFileInfo(path).dir().filePath(includeValue.toString());

        if (QFileInfo::exists(includePath)) {
            QString includeErr;
            base = readJsonObject(includePath, includeErr);
        }
    }

    // Workbench colors: base first, then the theme's own colors override.
    QJsonObject colors = base.value("colors").toObject();
    const QJsonObject ownColors = theme.value("colors").toObject();

    for (auto it = ownColors.begin(); it != ownColors.end(); ++it) {
        colors.insert(it.key(), it.value());
    }

    // Token colors live either in the theme or, if absent, in the base.
    QJsonObject tokenHost = theme.contains("tokenColors") ? theme : base;

    QString type = theme.value("type").toString();

    if (type.isEmpty()) {
        type = base.value("type").toString();
    }

    const bool darkType = (type.compare("dark", Qt::CaseInsensitive) == 0)
                          || (type.compare("hc-black", Qt::CaseInsensitive) == 0);

    QColor background = workbenchColor(colors, "editor.background", QColor());
    QColor foreground = workbenchColor(colors, "editor.foreground", QColor());

    bool dark = darkType;

    if (!background.isValid()) {
        // No explicit type and no background: assume dark if type says so,
        // otherwise light.
        background = dark ? QColor("#1e1e1e") : QColor("#ffffff");
    }

    if (type.isEmpty()) {
        dark = (background.lightness() < 128);
    }

    if (!foreground.isValid()) {
        foreground = dark ? QColor("#d4d4d4") : QColor("#000000");
    }

    const QList<TokenRule> rules = parseTokenRules(tokenHost);

    const QColor comment = tokenColor(rules, {"comment"}, foreground);

    scheme.background = background;
    scheme.foreground = foreground;
    scheme.selection = workbenchColor(
        colors,
        "editor.selectionBackground",
        blend(background, foreground, 0.25)
    );
    scheme.cursor = workbenchColor(colors, "editorCursor.foreground", foreground);

    QColor link = workbenchColor(colors, "textLink.foreground", QColor());

    if (!link.isValid()) {
        link = tokenColor(
            rules,
            {"markup.underline.link", "string.other.link"},
            QColor(dark ? "#3794ff" : "#0066bf")
        );
    }

    scheme.link = link;

    scheme.headingText = tokenColor(
        rules,
        {"markup.heading", "entity.name.section", "punctuation.definition.heading"},
        foreground
    );
    scheme.emphasisText = tokenColor(
        rules,
        {"markup.bold", "markup.italic", "markup.underline"},
        foreground
    );
    scheme.blockquoteText = tokenColor(rules, {"markup.quote"}, comment);
    scheme.listMarkup = tokenColor(
        rules,
        {"markup.list", "punctuation.definition.list_item", "punctuation.definition.list"},
        foreground
    );

    // Syntax/markup punctuation (used for heading/emphasis/code/divider markup).
    scheme.emphasisMarkup = tokenColor(
        rules,
        {"punctuation.definition", "punctuation.separator", "punctuation"},
        comment
    );

    QColor error = workbenchColor(colors, "editorError.foreground", QColor());

    if (!error.isValid()) {
        error = tokenColor(
            rules,
            {"invalid.illegal", "invalid", "message.error"},
            QColor("#f44747")
        );
    }

    scheme.error = error;

    // Fields not directly imported are derived by ThemeRepository on load.
    return true;
}
} // namespace ghostwriter
