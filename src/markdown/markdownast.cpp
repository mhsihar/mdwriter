/*
 * SPDX-FileCopyrightText: 2020-2023 Megan Conkle <megan.conkle@kdemail.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QStack>
#include <QTextStream>
#include <QVector>
#include <QtGlobal>

#include <3rdparty/cmark-gfm/src/cmark-gfm.h>
#include "markdownast.h"

namespace ghostwriter
{
class MarkdownASTPrivate
{
public:
    MarkdownASTPrivate()
        : root(nullptr),
          indexValid(false)
    {
        ;
    }

    ~MarkdownASTPrivate()
    {
        ;
    }

    // mdwriter: findBlockAtLine() used to walk the root's child list from the
    // start on every call.  The syntax highlighter calls it once per block, so
    // loading an n-line document cost O(n^2) and large files appeared to hang.
    // Build a line -> deepest-block-node index once per AST instead, then every
    // lookup is O(1).  The members are mutable so the (const) lookup can build
    // the index lazily.
    void invalidateIndex() const
    {
        indexValid = false;
    }

    int maxLineNumber() const
    {
        int maxLine = 0;
        int maxStart = 0;

        measureLines(root, maxLine, maxStart);

        return (maxLine > 0) ? maxLine : maxStart;
    }

    void buildIndex() const
    {
        lineIndex.clear();
        indexValid = true;

        const int maxLine = maxLineNumber();

        if (maxLine <= 0) {
            return;
        }

        lineIndex.resize(maxLine + 1);

        for (MarkdownNode *child = root->firstChild();
                nullptr != child;
                child = child->next()) {
            if (child->isBlockType() && (MarkdownNode::TableCell != child->type())) {
                indexNode(child, maxLine);
            }
        }
    }

    MemoryArena<MarkdownNode> arena;
    MarkdownNode *root;
    mutable QVector<MarkdownNode *> lineIndex;
    mutable bool indexValid;

private:
    void measureLines(MarkdownNode *node, int &maxLine, int &maxStart) const
    {
        if (node->startLine() > maxStart) {
            maxStart = node->startLine();
        }

        if (node->endLine() > maxLine) {
            maxLine = node->endLine();
        }

        for (MarkdownNode *child = node->firstChild();
                nullptr != child;
                child = child->next()) {
            measureLines(child, maxLine, maxStart);
        }
    }

    // Fills lineIndex[line] with the same node findBlockAtLine() used to
    // return: the deepest block containing the line, stopping at list items,
    // and keeping a multi-line heading's final line on the heading itself.
    void indexNode(MarkdownNode *node, int maxLine) const
    {
        int start = node->startLine();
        int end = node->endLine();

        if (start < 1) {
            start = 1;
        }

        if ((0 == end) || (end > maxLine)) {
            end = maxLine;
        }

        for (int line = start; (line <= end) && (line < lineIndex.size()); ++line) {
            lineIndex[line] = node;
        }

        const MarkdownNode::NodeType nodeType = node->type();

        if ((MarkdownNode::ListItem == nodeType)
                || (MarkdownNode::TaskListItem == nodeType)) {
            return;
        }

        for (MarkdownNode *child = node->firstChild();
                nullptr != child;
                child = child->next()) {
            if (child->isBlockType() && (MarkdownNode::TableCell != child->type())) {
                indexNode(child, maxLine);
            }
        }

        if ((MarkdownNode::Heading == nodeType)
                && ((node->endLine() - node->startLine() + 1) > 2)
                && (end >= 1)
                && (end < lineIndex.size())) {
            lineIndex[end] = node;
        }
    }
};

MarkdownAST::MarkdownAST()
    : d_ptr(new MarkdownASTPrivate())
{
    Q_D(MarkdownAST);
    
    d->root = nullptr;
}

MarkdownAST::MarkdownAST(cmark_node *root)
    : d_ptr(new MarkdownASTPrivate())
{    
    setRoot(root);
}

MarkdownAST::~MarkdownAST()
{
    Q_D(MarkdownAST);
    
    d->arena.freeAll();
    d->root = nullptr;
}

MarkdownNode *MarkdownAST::root()
{
    Q_D(MarkdownAST);
    
    return d->root;
}

void MarkdownAST::setRoot(cmark_node *root)
{
    Q_D(MarkdownAST);
    
    d->arena.freeAll();
    d->invalidateIndex();

    if (nullptr == root) {
        d->root = nullptr;
        return;
    }

    d->root = d->arena.allocate();

    // Clone the node into memory that isn't allocated to
    // cmark-gfm's arena memory.
    QStack<cmark_node *> fromNodes;
    QStack<MarkdownNode *> toNodes;

    d->root->setDataFrom(root);
    fromNodes.push(root);
    toNodes.push(d->root);

    while (!fromNodes.isEmpty()) {
        cmark_node *source = fromNodes.pop();
        MarkdownNode *dest = toNodes.pop();

        // Prep children nodes for cloning.
        MarkdownNode *destParent = dest;
        source = cmark_node_first_child(source);

        while (NULL != source) {
            fromNodes.push(source);
            dest = d->arena.allocate();
            dest->setDataFrom(source);
            destParent->appendChild(dest);
            toNodes.push(dest);
            source = cmark_node_next(source);
        }
    }
}

MarkdownNode *MarkdownAST::findBlockAtLine(int lineNumber) const
{
    Q_D(const MarkdownAST);
    
    if ((nullptr == d->root) || (MarkdownNode::Invalid == d->root->type())) {
        return nullptr;
    }

    // mdwriter: O(1) lookup into the lazily built line index (see
    // MarkdownASTPrivate).  Falls back gracefully for out-of-range lines.
    if (!d->indexValid) {
        d->buildIndex();
    }

    if ((lineNumber >= 1) && (lineNumber < d->lineIndex.size())) {
        return d->lineIndex[lineNumber];
    }

    return nullptr;
}

QVector<MarkdownNode *> MarkdownAST::headings() const
{
    Q_D(const MarkdownAST);
    
    QVector<MarkdownNode *> headings;

    if ((nullptr == d->root) || (MarkdownNode::Invalid == d->root->type())) {
        return headings;
    }

    MarkdownNode *node = d->root->firstChild();

    while (nullptr != node) {
        if (MarkdownNode::Heading == node->type()) {
            headings.append(node);
        }

        node = node->next();
    }

    return headings;
}

void MarkdownAST::clear()
{
    Q_D(MarkdownAST);
    
    d->arena.freeAll();
    d->root = nullptr;
    d->invalidateIndex();
}

QString MarkdownAST::toString() const
{
    Q_D(const MarkdownAST);
    
    if (nullptr == d->root) {
        return "AST is empty";
    }

    QString text;
    QTextStream stream(&text);
    QStack<MarkdownNode *> nodes;
    QStack<QString> indentation;

    nodes.push(d->root);
    indentation.push("");

    while (!nodes.empty()) {
        MarkdownNode *node = nodes.pop();
        QString indent = indentation.pop();


#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        stream << indent << "->" << node->toString() << Qt::endl;
#else
        stream << indent << "->" << node->toString() << endl;
#endif

        MarkdownNode *child = node->lastChild();
        indent += "   ";

        while (nullptr != child) {
            nodes.push(child);
            indentation.push(indent);
            child = child->previous();
        }
    }

    return text;
}
} // namespace ghostwriter
