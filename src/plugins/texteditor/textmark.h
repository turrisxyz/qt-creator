/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "texteditor_global.h"

#include <utils/fileutils.h>
#include <utils/id.h>
#include <utils/optional.h>
#include <utils/theme/theme.h>

#include <QCoreApplication>
#include <QIcon>
#include <QVector>

QT_BEGIN_NAMESPACE
class QAction;
class QGridLayout;
class QLayout;
class QPainter;
class QRect;
class QTextBlock;
QT_END_NAMESPACE

namespace TextEditor {

class TextDocument;

class TEXTEDITOR_EXPORT TextMark
{
    Q_DECLARE_TR_FUNCTIONS(TextEditor::TextMark)
public:
    TextMark(const Utils::FilePath &fileName,
             int lineNumber,
             Utils::Id category,
             double widthFactor = 1.0);
    TextMark() = delete;
    virtual ~TextMark();

    // determine order on markers on the same line.
    enum Priority
    {
        LowPriority,
        NormalPriority,
        HighPriority // shown on top.
    };

    Utils::FilePath fileName() const;
    int lineNumber() const;

    virtual void paintIcon(QPainter *painter, const QRect &rect) const;
    virtual void paintAnnotation(QPainter &painter,
                                 const QRect &eventRect,
                                 QRectF *annotationRect,
                                 const qreal fadeInOffset,
                                 const qreal fadeOutOffset,
                                 const QPointF &contentOffset) const;
    struct AnnotationRects
    {
        QRectF fadeInRect;
        QRectF annotationRect;
        QRectF iconRect;
        QRectF textRect;
        QRectF fadeOutRect;
        QString text;
    };
    AnnotationRects annotationRects(const QRectF &boundingRect, const QFontMetrics &fm,
                                    const qreal fadeInOffset, const qreal fadeOutOffset) const;
    /// called if the filename of the document changed
    virtual void updateFileName(const Utils::FilePath &fileName);
    virtual void updateLineNumber(int lineNumber);
    virtual void updateBlock(const QTextBlock &block);
    virtual void move(int line);
    virtual void removedFromEditor();
    virtual bool isClickable() const;
    virtual void clicked();
    virtual bool isDraggable() const;
    virtual void dragToLine(int lineNumber);
    void addToToolTipLayout(QGridLayout *target) const;
    virtual bool addToolTipContent(QLayout *target) const;
    virtual QColor annotationColor() const;

    void setIcon(const QIcon &icon);
    void setIconProvider(const std::function<QIcon()> &iconProvider);
    const QIcon icon() const;
    void updateMarker();
    Priority priority() const { return m_priority;}
    void setPriority(Priority prioriy);
    bool isVisible() const;
    void setVisible(bool isVisible);
    Utils::Id category() const { return m_category; }
    double widthFactor() const;
    void setWidthFactor(double factor);

    Utils::optional<Utils::Theme::Color> color() const;
    void setColor(const Utils::Theme::Color &color);

    QString defaultToolTip() const { return m_defaultToolTip; }
    void setDefaultToolTip(const QString &toolTip) { m_defaultToolTip = toolTip; }

    TextDocument *baseTextDocument() const { return m_baseTextDocument; }
    void setBaseTextDocument(TextDocument *baseTextDocument) { m_baseTextDocument = baseTextDocument; }

    QString lineAnnotation() const { return m_lineAnnotation; }
    void setLineAnnotation(const QString &lineAnnotation);

    QString toolTip() const;
    void setToolTip(const QString &toolTip);
    void setToolTipProvider(const std::function<QString ()> &toolTipProvider);

    QVector<QAction *> actions() const;
    void setActions(const QVector<QAction *> &actions); // Takes ownership

protected:
    void setSettingsPage(Utils::Id settingsPage);

private:
    Q_DISABLE_COPY(TextMark)

    TextDocument *m_baseTextDocument = nullptr;
    Utils::FilePath m_fileName;
    int m_lineNumber = 0;
    Priority m_priority = LowPriority;
    QIcon m_icon;
    std::function<QIcon()> m_iconProvider;
    Utils::optional<Utils::Theme::Color> m_color;
    bool m_visible = false;
    Utils::Id m_category;
    double m_widthFactor = 1.0;
    QString m_lineAnnotation;
    QString m_toolTip;
    std::function<QString()> m_toolTipProvider;
    QString m_defaultToolTip;
    QVector<QAction *> m_actions;
    QAction *m_settingsAction = nullptr;
};

} // namespace TextEditor
