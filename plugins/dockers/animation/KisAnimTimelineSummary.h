/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef KIS_ANIM_TIMELINE_SUMMARY_H
#define KIS_ANIM_TIMELINE_SUMMARY_H

#include <QHeaderView>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTableView>
#include <QApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QClipboard>
#include <QTimer>
#include <functional>
#include "KisAnimTimelineFramesModel.h"
#include "KisAnimTimelineFramesView.h"
#include "KisPlaybackEngine.h"

// Shares the frame table's coordinates and selection; no second keyframe store.
class KisAnimTimelineSummary : public QWidget
{
public:
    KisAnimTimelineSummary(QTableView *view, KisAnimTimelineFramesModel *model, QWidget *parent)
        : QWidget(parent), m_view(view), m_model(model)
    {
        setFixedHeight(24);
        setToolTip(tr("Summary keys: click to select, Shift-click to select a range, Ctrl-click to toggle, drag to move. Drag empty space to select a range. G/S transform the selection."));
        auto refresh = [this]() { update(); };
        connect(model, &QAbstractItemModel::dataChanged, this, refresh);
        connect(model, &QAbstractItemModel::headerDataChanged, this, refresh);
        connect(model, &QAbstractItemModel::modelReset, this, refresh);
        connect(model, &QAbstractItemModel::rowsInserted, this, refresh);
        connect(model, &QAbstractItemModel::rowsRemoved, this, refresh);
        connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this, refresh);
        connect(view->horizontalHeader(), &QHeaderView::sectionResized, this, refresh);
        connect(view->verticalHeader(), &QHeaderView::geometriesChanged, this, refresh);
        connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, refresh);
        connect(view, &QAbstractItemView::clicked, this, [this](const QModelIndex &index) {
            m_anchorColumn = index.column();
        });
        connect(model, &QAbstractItemModel::modelAboutToBeReset, this, [this]() {
            m_anchorColumn = -1;
            m_pressed = false;
        });
    }

    std::function<void(bool, QPoint, QPoint, bool, Qt::KeyboardModifiers)> startTransform;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), palette().alternateBase());
        p.setPen(palette().text().color());
        const int left = mapFromGlobal(m_view->viewport()->mapToGlobal(QPoint())).x();
        p.drawText(QRect(6, 0, qMax(0, left - 8), height()), Qt::AlignVCenter, tr("Summary"));
        p.setClipRect(QRect(left, 0, width() - left, height()));
        const int first = qMax(0, m_view->columnAt(0));
        for (int time = first; time < m_model->columnCount(); ++time) {
            const int x = left + m_view->columnViewportPosition(time);
            if (x > width()) break;
            if (!m_model->headerData(time, Qt::Horizontal, KisTimeBasedItemModel::WithinPlaybackRange).toBool()) {
                p.fillRect(QRect(x, 0, m_view->columnWidth(time), height()), QColor(0, 0, 0, 75));
            }
        }
        for (int time : m_model->summaryKeyframeTimes()) {
            if (time >= m_model->columnCount()) continue;
            const int x = left + m_view->columnViewportPosition(time) + m_view->columnWidth(time) / 2;
            if (x < left - 5 || x > width() + 5) continue;
            bool selected = false;
            for (int row = 0; row < m_model->rowCount(); ++row) {
                const auto idx = m_model->index(row, time);
                selected |= m_view->selectionModel()->isSelected(idx)
                    && (idx.data(KisTimeBasedItemModel::FrameExistsRole).toBool()
                        || idx.data(KisTimeBasedItemModel::SpecialKeyframeExists).toBool());
            }
            p.setBrush(selected ? palette().highlight() : palette().text());
            QPolygon diamond;
            diamond << QPoint(x, 6) << QPoint(x + 5, 12) << QPoint(x, 18) << QPoint(x - 5, 12);
            p.drawPolygon(diamond);
        }
    }

    void wheelEvent(QWheelEvent *event) override
    {
        const QPoint pos = m_view->viewport()->mapFromGlobal(event->globalPos());
        QWheelEvent forwarded(pos, event->globalPos(), event->pixelDelta(), event->angleDelta(),
                              event->buttons(), event->modifiers(), event->phase(), event->inverted());
        QApplication::sendEvent(m_view->viewport(), &forwarded);
        event->accept();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        auto view = qobject_cast<KisAnimTimelineFramesView *>(m_view);
        if (view && view->beginTimelineZoom(event, this)) return;
        if (event->button() != Qt::LeftButton || !m_model->rowCount()) return;
        m_view->setProperty("summaryKeySelection", true);
        m_pressColumn = columnAt(event->pos());
        if (m_pressColumn < 0) return;
        m_pressPosition = event->globalPos();
        m_pressed = true;
        m_keyPressed = m_model->summaryKeyframeTimes().contains(m_pressColumn);
        const bool range = event->modifiers() & Qt::ShiftModifier;
        const bool toggle = event->modifiers() & Qt::ControlModifier;
        auto selection = keysInRange(m_pressColumn, m_pressColumn);
        bool alreadySelected = !selection.isEmpty();
        for (const auto &idx : selection.indexes()) alreadySelected &= m_view->selectionModel()->isSelected(idx);
        m_selectionAtPress = m_view->selectionModel()->selection();
        if (range) {
            if (m_anchorColumn < 0) m_anchorColumn = m_pressColumn;
            selection = keysInRange(qMin(m_anchorColumn, m_pressColumn), qMax(m_anchorColumn, m_pressColumn));
            m_view->selectionModel()->select(selection, toggle ? QItemSelectionModel::Select : QItemSelectionModel::ClearAndSelect);
        } else {
            m_anchorColumn = m_pressColumn;
            if (toggle) {
                m_view->selectionModel()->select(selection, alreadySelected ? QItemSelectionModel::Deselect : QItemSelectionModel::Select);
            } else if (!alreadySelected || !m_keyPressed) {
                m_view->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
            }
        }
        // Summary selection must not move the playhead: it is the scale pivot.
        // A selection click on a diamond can therefore be followed directly by S.
        m_view->setFocus();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        auto view = qobject_cast<KisAnimTimelineFramesView *>(m_view);
        if (view && view->isTimelineZoomActive()) {
            view->updateTimelineZoom(event->globalPos());
            event->accept();
            return;
        }
        if (!m_pressed || !(event->buttons() & Qt::LeftButton)) return;
        if ((event->globalPos() - m_pressPosition).manhattanLength() < QApplication::startDragDistance()) return;
        if (m_keyPressed) {
            m_pressed = false;
            if (startTransform) startTransform(false, m_pressPosition, event->globalPos(), true, event->modifiers());
        } else {
            const int column = columnAt(event->pos());
            if (column < 0) return;
            if (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) {
                m_view->selectionModel()->select(m_selectionAtPress, QItemSelectionModel::ClearAndSelect);
                m_view->selectionModel()->select(keysInRange(qMin(column, m_pressColumn), qMax(column, m_pressColumn)), QItemSelectionModel::Select);
            } else {
                m_view->selectionModel()->select(keysInRange(qMin(column, m_pressColumn), qMax(column, m_pressColumn)), QItemSelectionModel::ClearAndSelect);
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        m_pressed = false;
        auto view = qobject_cast<KisAnimTimelineFramesView *>(m_view);
        if (view && view->isTimelineZoomActive()) {
            if (event->button() == Qt::LeftButton) {
                view->updateTimelineZoom(event->globalPos());
                view->endTimelineZoom();
            }
            event->accept();
        }
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        auto view = qobject_cast<KisAnimTimelineFramesView *>(m_view);
        if (!view) return;
        m_view->setProperty("summaryKeySelection", true);
        const int column = columnAt(event->pos());
        const auto keys = keysInRange(column, column);
        bool selected = false;
        for (const auto &idx : keys.indexes()) selected |= view->selectionModel()->isSelected(idx);
        if (!keys.isEmpty() && !selected) view->selectionModel()->select(keys, QItemSelectionModel::ClearAndSelect);
        view->setFocus();
        QMenu menu(this);
        const QPoint position = event->globalPos();
        auto transformAfterMenu = [this, position](bool scale) {
            QTimer::singleShot(0, this, [this, position, scale]() {
                if (startTransform) startTransform(scale, position, position, false, Qt::NoModifier);
            });
        };
        menu.addAction(tr("Move Keys (G)"), [transformAfterMenu]() { transformAfterMenu(false); });
        menu.addAction(tr("Scale Keys (S)"), [transformAfterMenu]() { transformAfterMenu(true); });
        menu.addSeparator();
        menu.addAction(tr("Copy Keys"), view, [this, view]() {
            const auto indexes = view->selectionModel()->selectedIndexes();
            if (indexes.isEmpty()) return;
            int firstTime = indexes.first().column();
            for (const auto &idx : indexes) firstTime = qMin(firstTime, idx.column());
            // Summary paste keeps each layer in its existing row.
            auto data = m_model->mimeDataExtended(indexes, m_model->index(0, firstTime), KisAnimTimelineFramesModel::CopyFramesPolicy);
            if (data) QApplication::clipboard()->setMimeData(data);
        });
        menu.addAction(tr("Paste Keys"), view, [view, column]() {
            if (column >= 0 && view->model()->rowCount()) view->selectionModel()->setCurrentIndex(view->model()->index(0, column), QItemSelectionModel::NoUpdate);
            view->slotPasteFrames();
        });
        menu.addAction(tr("Delete Keys (X)"), view, [view]() { view->slotRemoveSelectedFrames(); });
        menu.exec(event->globalPos());
    }

private:
    int columnAt(QPoint position) const
    {
        const int left = mapFromGlobal(m_view->viewport()->mapToGlobal(QPoint())).x();
        return position.x() < left ? -1 : m_view->columnAt(position.x() - left);
    }

    QItemSelection keysInRange(int from, int to) const
    {
        QItemSelection keys;
        for (int time : m_model->summaryKeyframeTimes()) {
            if (time < from || time > to) continue;
            for (int row = 0; row < m_model->rowCount(); ++row) {
                const auto idx = m_model->index(row, time);
                if (idx.data(KisTimeBasedItemModel::FrameExistsRole).toBool()
                    || idx.data(KisTimeBasedItemModel::SpecialKeyframeExists).toBool()) keys.select(idx, idx);
            }
        }
        return keys;
    }

    QTableView *m_view;
    KisAnimTimelineFramesModel *m_model;
    bool m_pressed = false, m_keyPressed = false;
    int m_pressColumn = -1;
    int m_anchorColumn = -1;
    QPoint m_pressPosition;
    QItemSelection m_selectionAtPress;
};
#endif
