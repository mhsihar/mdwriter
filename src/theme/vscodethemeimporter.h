/*
 * SPDX-FileCopyrightText: 2026 mhsihar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef VSCODETHEMEIMPORTER_H
#define VSCODETHEMEIMPORTER_H

#include <QList>
#include <QString>

#include <editor/colorscheme.h>

namespace ghostwriter
{
/**
 * Describes a VS Code color theme discovered on disk.
 */
struct VscodeThemeInfo
{
    QString name;   ///< Human-readable label.
    QString path;   ///< Absolute path to the VS Code theme JSON file.
};

/**
 * Converts VS Code color themes (workbench colors + TextMate token colors)
 * into a ghostwriter ColorScheme.
 *
 * Not a QObject; all methods are static helpers.
 */
class VscodeThemeImporter
{
public:
    /**
     * Scans the VS Code built-in extension directory and the user extension
     * directories for color themes.  Duplicate paths are removed.
     */
    static QList<VscodeThemeInfo> discoverThemes();

    /**
     * Loads a VS Code theme JSON file (resolving "include" chains) and maps
     * its colors onto the given ghostwriter ColorScheme.  Returns false and
     * populates err on failure.
     */
    static bool loadColorScheme
    (
        const QString &path,
        ColorScheme &scheme,
        QString &err
    );
};
} // namespace ghostwriter

#endif // VSCODETHEMEIMPORTER_H
