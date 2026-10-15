/*
 * SPDX-FileCopyrightText: 2020-2023 Megan Conkle <megan.conkle@kdemail.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QMutex>
#include <QRegularExpression>

#include <3rdparty/cmark-gfm/src/cmark-gfm-extension_api.h>
#include <3rdparty/cmark-gfm/extensions/cmark-gfm-core-extensions.h>
#include "cmarkgfmapi.h"

namespace ghostwriter
{
class CmarkGfmAPIPrivate
{
public:
    CmarkGfmAPIPrivate()
    {
        ;
    };

    ~CmarkGfmAPIPrivate()
    {
        ;
    };

    // cmark-gfm extensions to enable.
    cmark_syntax_extension *tableExt;
    cmark_syntax_extension *strikethroughExt;
    cmark_syntax_extension *autolinkExt;
    cmark_syntax_extension *tagfilterExt;
    cmark_syntax_extension *tasklistExt;

    QMutex apiMutex;
};

class CmarkGfmAPIWithPublicConstructor : public CmarkGfmAPI
{
public:
    CmarkGfmAPIWithPublicConstructor() {}
};
Q_GLOBAL_STATIC(CmarkGfmAPIWithPublicConstructor, CmarkGfmAPI_instance);

CmarkGfmAPI *CmarkGfmAPI::instance()
{
    return CmarkGfmAPI_instance();
}

CmarkGfmAPI::~CmarkGfmAPI()
{
    cmark_arena_reset();
}

MarkdownAST *CmarkGfmAPI::parse(const QString &text, const bool smartTypographyEnabled)
{
    Q_D(CmarkGfmAPI);

    int opts = CMARK_OPT_DEFAULT | CMARK_OPT_FOOTNOTES | CMARK_OPT_UNSAFE;

    if (smartTypographyEnabled) {
        opts |= CMARK_OPT_SMART;
    }

    d->apiMutex.lock();

    cmark_mem *mem = cmark_get_arena_mem_allocator();
    cmark_parser *parser = cmark_parser_new_with_mem(opts, mem);

    cmark_parser_attach_syntax_extension(parser, d->tableExt);
    cmark_parser_attach_syntax_extension(parser, d->strikethroughExt);
    cmark_parser_attach_syntax_extension(parser, d->autolinkExt);
    cmark_parser_attach_syntax_extension(parser, d->tagfilterExt);
    cmark_parser_attach_syntax_extension(parser, d->tasklistExt);

    // Use Latin1 instead of UTF-8 since the column numbers and even
    // the nodes returned in the AST are shifted or missing when
    // UTF-8 characters longer than 1 byte are encountered.
    //
    cmark_parser_feed(parser, text.toLatin1().data(), text.toLatin1().length());

    cmark_node *root = cmark_parser_finish(parser);
    MarkdownAST *ast = new MarkdownAST(root);
    cmark_parser_free(parser);
    cmark_node_free(root);
    cmark_arena_reset();

    d->apiMutex.unlock();

    return ast;
}

// cmark-gfm's CMARK_OPT_SMART pass also runs over the literal text found
// between raw inline HTML tags.  That corrupts fixed-width content inside
// elements such as <code>a...b</code>: the ASCII ellipsis, quotes and
// dashes are swapped for typographic glyphs and ASCII art loses its
// alignment.  Escape the trigger sequences inside raw HTML code elements
// using numeric character references.  cmark consumes these with
// handle_entity and emits the decoded character directly as text, so the
// SMART handlers (which only fire on literal '.', '-', '\'' and '"' in the
// input) never see them, while the original characters still render.
//
static QString protectRawHtmlCodeFromSmartTypography(const QString &text)
{
    static const QRegularExpression htmlCodeRegex
    (
        QStringLiteral("<(code|pre|kbd|samp|tt)(\\s[^>]*)?>(.*?)</\\1>"),
        QRegularExpression::DotMatchesEverythingOption
            | QRegularExpression::CaseInsensitiveOption
    );

    QString result;
    int lastEnd = 0;

    QRegularExpressionMatchIterator matches = htmlCodeRegex.globalMatch(text);

    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();

        // Copy everything up to and including the opening HTML tag.
        result += text.mid(lastEnd, match.capturedStart(3) - lastEnd);

        QString content = match.captured(3);

        // Numeric character references are treated as entities by cmark and
        // therefore skipped by its smart typography pass.
        content.replace(QStringLiteral("..."), QStringLiteral("&#46;&#46;&#46;"));
        content.replace(QStringLiteral("--"), QStringLiteral("&#45;&#45;"));
        content.replace(QStringLiteral("\""), QStringLiteral("&quot;"));
        content.replace(QStringLiteral("'"), QStringLiteral("&#39;"));

        result += content;

        lastEnd = match.capturedEnd(3);
    }

    result += text.mid(lastEnd);

    return result;
}

QString CmarkGfmAPI::renderToHtml(const QString &text, const bool smartTypographyEnabled)
{
    Q_D(CmarkGfmAPI);
    
    int opts = CMARK_OPT_DEFAULT | CMARK_OPT_FOOTNOTES | CMARK_OPT_UNSAFE;

    if (smartTypographyEnabled) {
        opts |= CMARK_OPT_SMART;
    }

    d->apiMutex.lock();

    cmark_mem *mem = cmark_get_arena_mem_allocator();
    cmark_parser *parser = cmark_parser_new_with_mem(opts, mem);

    cmark_parser_attach_syntax_extension(parser, d->tableExt);
    cmark_parser_attach_syntax_extension(parser, d->strikethroughExt);
    cmark_parser_attach_syntax_extension(parser, d->autolinkExt);
    cmark_parser_attach_syntax_extension(parser, d->tagfilterExt);
    cmark_parser_attach_syntax_extension(parser, d->tasklistExt);

    // Protect literal text inside raw HTML code elements from the smart
    // typography pass, which would otherwise corrupt fixed-width content.
    const QString source =
        smartTypographyEnabled
            ? protectRawHtmlCodeFromSmartTypography(text)
            : text;
    const QByteArray buffer = source.toUtf8();

    cmark_parser_feed(parser, buffer.data(), buffer.length());

    cmark_node *root = cmark_parser_finish(parser);
    char *output = cmark_render_html(root, opts, cmark_parser_get_syntax_extensions(parser));
    QString html = QString::fromUtf8(output);

    cmark_parser_free(parser);
    cmark_arena_reset();

    d->apiMutex.unlock();

    return html;
}

CmarkGfmAPI::CmarkGfmAPI()
    : d_ptr(new CmarkGfmAPIPrivate())
{
    Q_D(CmarkGfmAPI);
    
    cmark_gfm_core_extensions_ensure_registered();
    d->tableExt = cmark_find_syntax_extension("table");
    d->strikethroughExt = cmark_find_syntax_extension("strikethrough");
    d->autolinkExt = cmark_find_syntax_extension("autolink");
    d->tagfilterExt = cmark_find_syntax_extension("tagfilter");
    d->tasklistExt = cmark_find_syntax_extension("tasklist");
}
}
