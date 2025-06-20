#ifndef LABELRECTITEM_H
#define LABELRECTITEM_H

#include <QtWidgets>
#include <QGraphicsRectItem>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QBrush>
#include <QPen>
#include <QWidget>
#include <QInputDialog>
#include <QLineEdit>
#include <QFontMetrics>
#include "ClassList.h"

class LabelRectItem : public QGraphicsRectItem
{
public:
    LabelRectItem(QGraphicsItem *parent, Label label);

    enum HandleType {
        None,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Top,
        Bottom,
        Left,
        Right
    };

    void setSelected(bool selected) { isSelected = selected; }
    bool getSelected() const { return isSelected; }

    void setLabel(const QString &text) { label_info.label = text; update(); }
    void setPositionNumber(const QString &text) { label_info.position_number = text; update(); }
    void setNotes(const QByteArray &text) { label_info.notes = text; update(); }
    QString getLabel() const { return label_info.label; }
    Label getLabelInfo() const { return label_info; }
    QString getPositionNumber() const { return label_info.position_number; }
    QByteArray getNotes() const { return label_info.notes; }
    QRectF sceneRect() const;

    void setRect(const QRectF &rect);
    void setPen(const QPen &pen) { rectPen = pen; update(); }

    QColor getPenColor() const { return rectPen.color(); }

    std::tuple<int, QRectF, QString, QByteArray> getItemInfo(){
        return std::make_tuple(label_info.id, sceneRect(), label_info.position_number, label_info.notes);
    };
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    HandleType getHandle(const QPointF &pos);
    void updateCursor(HandleType handle);

    static const qreal HandleSize;
    HandleType currentHandle;
    QPointF dragStart;
    bool isSelected;
    QRectF getLabelRect() const;
    QRectF getIdRect() const;
    bool isLabelHit(const QPointF &pos) const;
    bool isHovered;
    Label label_info;
    void updateZValue();
    QPen rectPen = QPen(Qt::blue, 2);  // 默认画笔
};

#endif // LABELRECTITEM_H
