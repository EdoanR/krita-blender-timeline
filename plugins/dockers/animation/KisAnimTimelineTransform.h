/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef KIS_ANIM_TIMELINE_TRANSFORM_H
#define KIS_ANIM_TIMELINE_TRANSFORM_H

#include <QApplication>
#include <QCursor>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QTableView>
#include <cmath>
#include "KisAnimTimelineFramesModel.h"

// A modal timeline tool. Only the ghost positions change until confirmation;
// cancel never touches the document or its undo stack. No cursor warping.
class KisAnimTimelineTransform : public QWidget
{
public:
    KisAnimTimelineTransform(QTableView *view, KisAnimTimelineFramesModel *model, QWidget *body)
        : QWidget(body), m_view(view), m_model(model), m_body(body)
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_NoSystemBackground);
        hide();
        qApp->installEventFilter(this);
        auto cancel = [this]() { finish(false); };
        connect(model, &QAbstractItemModel::modelAboutToBeReset, this, cancel);
        connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, cancel);
        connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, cancel);
    }

    bool isTransforming() const { return m_active; }
    double factor() const { return m_factor; }
    int offset() const { return m_offset; }
    bool isValidTransform() const { return m_valid; }

    bool begin(bool scale, QPoint globalPosition, bool drag = false, bool allowOverwrite = false)
    {
        if (m_active || !m_model->hasConnectionToCanvas()) return false;
        m_sources.clear();
        for (const auto &idx : m_view->selectionModel()->selectedIndexes()) {
            if (idx.data(KisTimeBasedItemModel::FrameEditableRole).toBool()
                && (idx.data(KisTimeBasedItemModel::FrameExistsRole).toBool()
                    || idx.data(KisTimeBasedItemModel::SpecialKeyframeExists).toBool())) m_sources.append(idx);
        }
        if (m_sources.isEmpty()) return false;
        if (m_model->isPlaybackActive() || m_model->isPlaybackPaused()) m_model->stopPlayback();
        m_scale = scale;
        m_drag = drag;
        m_allowOverwrite = allowOverwrite;
        m_pivot = m_model->currentTime();
        m_lastPosition = globalPosition;
        m_pixels = 0;
        m_factor = 1;
        m_offset = 0;
        m_number.clear();
        const int pivotX = m_view->viewport()->mapToGlobal(QPoint(frameX(m_pivot), 0)).x();
        m_scaleDistance = globalPosition.x() - pivotX;
        if (qAbs(m_scaleDistance) < 40) m_scaleDistance = 100;
        m_active = true;
        setGeometry(m_body->rect());
        show();
        raise();
        setFocus(Qt::OtherFocusReason);
        refresh(Qt::NoModifier);
        return true;
    }

    // Used both by mouse events and the summary's thresholded drag start.
    void moveTo(QPoint globalPosition, Qt::KeyboardModifiers modifiers)
    {
        if (!m_active) return;
        const double precision = modifiers & Qt::ShiftModifier ? 0.1 : 1.0;
        m_pixels += (globalPosition.x() - m_lastPosition.x()) * precision;
        m_lastPosition = globalPosition;
        refresh(modifiers);
    }

    void finish(bool commit)
    {
        if (!m_active) return;
        // Clear modal state before emitting model notifications from commit.
        m_active = false;
        hide();
        if (commit && m_valid) {
            QModelIndexList destinations;
            if (m_model->retimeFrames(m_sources, m_factor, m_offset, &destinations, m_pivot, false, m_allowOverwrite)) {
                QItemSelection selection;
                for (const auto &idx : destinations) selection.select(idx, idx);
                m_view->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
            }
        }
        m_sources.clear();
        m_view->setFocus(Qt::OtherFocusReason);
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (!m_active) return false;
        if (object == m_body && event->type() == QEvent::Resize) setGeometry(m_body->rect());
        if ((object == m_body || object == window()) && event->type() == QEvent::Hide) {
            finish(false);
            return false;
        }
        if (event->type() == QEvent::ApplicationDeactivate
            || (event->type() == QEvent::WindowDeactivate && object == window())) {
            finish(false);
            return false;
        }
        auto widget = qobject_cast<QWidget *>(object);
        if (!widget || widget->window() != window()) return false;
        if (event->type() == QEvent::ShortcutOverride) {
            event->accept();
            return true;
        }
        if (event->type() == QEvent::KeyPress) {
            auto key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) finish(false);
            else if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                if (m_valid) finish(true);
            } else if (key->key() == Qt::Key_Backspace) {
                m_number.chop(1);
                refresh(key->modifiers());
            } else if (key->key() == Qt::Key_G || key->key() == Qt::Key_S) {
                m_scale = key->key() == Qt::Key_S;
                m_number.clear();
                m_pixels = 0;
                refresh(key->modifiers());
            } else {
                const QString text = key->text();
                if (text == "-") {
                    if (m_number.startsWith('-')) m_number.remove(0, 1);
                    else m_number.prepend('-');
                } else if (text == "+") {
                    if (m_number.startsWith('-')) m_number.remove(0, 1);
                } else if (text.size() == 1 && m_number.size() < 20
                    && (text[0].isDigit() || ((text == "." || text == ",") && !m_number.contains('.')))) {
                    m_number += text == "," ? "." : text;
                }
                refresh(key->modifiers());
            }
            return true;
        }
        if (event->type() == QEvent::KeyRelease) {
            refresh(static_cast<QKeyEvent *>(event)->modifiers());
            return true;
        }
        if (event->type() == QEvent::MouseMove) {
            auto mouse = static_cast<QMouseEvent *>(event);
            moveTo(mouse->globalPos(), mouse->modifiers());
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            auto mouse = static_cast<QMouseEvent *>(event);
            moveTo(mouse->globalPos(), mouse->modifiers());
            if (mouse->button() == Qt::RightButton) finish(false);
            else if (!m_drag && mouse->button() == Qt::LeftButton && m_valid) finish(true);
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            auto mouse = static_cast<QMouseEvent *>(event);
            moveTo(mouse->globalPos(), mouse->modifiers());
            if (m_drag && mouse->button() == Qt::LeftButton) finish(m_valid);
            return true;
        }
        // Don't allow edits, playback, or zoom to change the transform basis.
        return event->type() == QEvent::Wheel || event->type() == QEvent::ContextMenu
            || event->type() == QEvent::MouseButtonDblClick;
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        const QColor color = m_valid ? palette().highlight().color() : QColor(230, 70, 70);
        const QPoint origin = mapFromGlobal(m_view->viewport()->mapToGlobal(QPoint()));
        const int cellWidth = m_view->columnWidth(0);
        painter.setPen(QPen(color, 2, Qt::DashLine));
        const int pivotX = origin.x() + frameX(m_pivot);
        painter.drawLine(pivotX, 0, pivotX, height());
        QSet<int> summaryTimes;
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 75));
        painter.setClipRect(QRect(origin.x(), 0, width() - origin.x(), height()));
        for (const auto &idx : m_sources) {
            const int time = qBound(-10000, qRound(m_pivot + (idx.column() - m_pivot) * m_factor + m_offset), 20000);
            const int x = origin.x() + frameX(time) - cellWidth / 2;
            const int y = origin.y() + m_view->rowViewportPosition(idx.row());
            painter.drawRect(QRect(x, y, cellWidth, m_view->rowHeight(idx.row())));
            if (!summaryTimes.contains(time)) {
                summaryTimes.insert(time);
                const int cx = x + cellWidth / 2;
                painter.drawPolygon(QPolygon({QPoint(cx, 6), QPoint(cx + 5, 12), QPoint(cx, 18), QPoint(cx - 5, 12)}));
            }
        }
        painter.setClipping(false);
        const QString amount = m_number.isEmpty() ? QString::number(m_scale ? m_factor : m_offset, 'g', 5) : m_number;
        const QString label = tr("%1: %2 | Pivot: %3 | Shift: precision, Ctrl: snap | Enter/click: confirm, Esc/right-click: cancel%4")
            .arg(m_scale ? tr("Scale") : tr("Move"), amount).arg(m_pivot)
            .arg(m_valid ? QString() : tr(" | Invalid destination"));
        QRect box(4, qMax(24, height() - 30), qMax(0, width() - 8), 26);
        painter.fillRect(box, palette().window());
        painter.setPen(color);
        painter.drawText(box.adjusted(5, 0, -5, 0), Qt::AlignVCenter, label);
    }

private:
    int frameX(int time) const
    {
        return m_view->horizontalHeader()->sectionViewportPosition(0) + time * m_view->columnWidth(0) + m_view->columnWidth(0) / 2;
    }

    void refresh(Qt::KeyboardModifiers modifiers)
    {
        bool numberValid = true;
        double amount = m_scale ? 1 + m_pixels / m_scaleDistance : m_pixels / qMax(1, m_view->columnWidth(0));
        if (!m_number.isEmpty()) {
            amount = m_number.toDouble(&numberValid);
        } else if (modifiers & Qt::ControlModifier) {
            // Integer animation frames are the fine move increment; Ctrl alone
            // snaps to seconds. Scale uses 0.1, or 0.01 with Ctrl+Shift.
            const double step = m_scale ? (modifiers & Qt::ShiftModifier ? 0.01 : 0.1)
                : (modifiers & Qt::ShiftModifier ? 1 : qMax(1, m_model->headerData(0, Qt::Horizontal, KisTimeBasedItemModel::FramesPerSecondRole).toInt()));
            amount = std::round(amount / step) * step;
        }
        // Bound before rounding/painting, including hostile or overflowing text.
        numberValid &= std::isfinite(amount) && qAbs(amount) <= 10000;
        m_factor = m_scale && numberValid ? amount : 1;
        m_offset = !m_scale && numberValid ? qRound(amount) : 0;
        m_valid = numberValid && m_model->retimeFrames(m_sources, m_factor, m_offset, nullptr, m_pivot, true, m_allowOverwrite);
        update();
    }

    QTableView *m_view;
    KisAnimTimelineFramesModel *m_model;
    QWidget *m_body;
    QModelIndexList m_sources;
    bool m_allowOverwrite = false;
    bool m_active = false, m_scale = false, m_drag = false, m_valid = false;
    int m_pivot = 0, m_offset = 0;
    double m_pixels = 0, m_factor = 1, m_scaleDistance = 100;
    QPoint m_lastPosition;
    QString m_number;
};
#endif
