/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef KIS_ANIM_TIMELINE_RANGE_FIELD_H
#define KIS_ANIM_TIMELINE_RANGE_FIELD_H
#include <QObject>
#include <QAbstractSpinBox>
#include <QMouseEvent>
#include <functional>

class KisAnimTimelineRangeField : public QObject
{
public:
    KisAnimTimelineRangeField(QAbstractSpinBox *field, std::function<void()> useCurrentFrame)
        : QObject(field), m_useCurrentFrame(useCurrentFrame)
    {
        field->installEventFilter(this);
        for (auto child : field->findChildren<QWidget *>()) child->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonPress) {
            auto mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && (mouse->modifiers() & Qt::ShiftModifier)) {
                m_useCurrentFrame();
                event->accept();
                return true;
            }
        }
        return false;
    }

private:
    std::function<void()> m_useCurrentFrame;
};
#endif
