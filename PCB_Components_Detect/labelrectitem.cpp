#include "labelrectitem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

const qreal LabelRectItem::HandleSize = 8.0;

LabelRectItem::LabelRectItem(QGraphicsItem *parent, Label label)
    : QGraphicsRectItem(parent)
    , currentHandle(None)
    , isSelected(false)
    , isHovered(false)
    , label_info(label)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsFocusable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);

    updateZValue();  // 初始化时设置Z值
}

void LabelRectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->save();

    // 使用设置的画笔绘制矩形
    painter->setPen(rectPen);
    painter->setBrush(Qt::transparent);
    painter->drawRect(rect());

    // 只在选中状态下绘制控制点和标签
    if (isSelected) {
        // 绘制控制点
        painter->setPen(QPen(rectPen.color(), 1));
        painter->setBrush(Qt::white);

        // 绘制四个角的控制点
        QRectF r = rect();
        QRectF handle(0, 0, HandleSize, HandleSize);

        handle.moveCenter(r.topLeft());
        painter->drawRect(handle);

        handle.moveCenter(r.topRight());
        painter->drawRect(handle);

        handle.moveCenter(r.bottomLeft());
        painter->drawRect(handle);

        handle.moveCenter(r.bottomRight());
        painter->drawRect(handle);

        // 绘制标签和ID
        if (!label_info.label.isEmpty()) {
            QRectF labelRect = getLabelRect();
            QRectF idRect = getIdRect();  // 使用相同的尺寸和位置来初始化ID矩形
            idRect.moveLeft(rect().left() - idRect.width());  // 将ID矩形移动到框的左侧

            // 设置透明度
            painter->setOpacity(1);

            // 绘制标签背景
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255));
            painter->drawRect(labelRect);

            // 绘制ID背景
            painter->drawRect(idRect);

            // 绘制标签文本，使用与矩形框相同的颜色
            painter->setPen(rectPen.color());
            painter->setBrush(Qt::NoBrush);
            painter->drawText(labelRect, Qt::AlignCenter, label_info.label);

            // 绘制ID文本
            painter->drawText(idRect, Qt::AlignCenter, QString::number(label_info.id));
        }
    }

    painter->restore();
}

QRectF LabelRectItem::getIdRect() const
{
    QRectF r = rect();
    int idWidth = QString::number(label_info.id).length() * 10; // 假设每个数字宽度为10像素
    return QRectF(r.left() - idWidth, r.top(), idWidth, r.height());
}

QRectF LabelRectItem::getLabelRect() const
{
    QRectF r = rect();
    QFont font;
    QFontMetrics fm(font);
    QString displayText = label_info.label.isEmpty() ? "Label" : label_info.label;
    int charCount = displayText.length();
    int widthPerChar = 9; // 每个字符平均宽度为9像素
    int padding = 15; // 文本周围的填充

    // 根据字符数量动态调整标签宽度
    int labelWidth = charCount * widthPerChar + padding * 2;
    int labelHeight = fm.height() + padding;

    // 在右上角创建标签矩形
    return QRectF(r.right() - labelWidth,
                  r.top() - labelHeight,
                  labelWidth,
                  labelHeight);
}

bool LabelRectItem::isLabelHit(const QPointF &pos) const
{
    return getLabelRect().contains(pos);
}

void LabelRectItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 先检查是否点击在 handle 上
        HandleType clickedHandle = getHandle(event->pos());
        if (!isSelected) {
            // 未选中状态下，只处理选中操作
            isSelected = true;
            currentHandle = None;
            scene()->update();
            return;
        }

        // 已选中状态下的操作
        if (clickedHandle != None) {
            // 如果点击在 handle 上，准备调整大小
            currentHandle = clickedHandle;
            dragStart = event->pos();
            scene()->update();
            return;  // 直接返回，不调用基类事件
        } else {
            // 点击在矩形内部，准备移动
            currentHandle = None;
            setCursor(Qt::ClosedHandCursor);
            dragStart = event->pos();
        }
    } else if (event->button() == Qt::RightButton && isSelected) {
        // 右键点击且在选中状态下，取消选中
        setCursor(Qt::ArrowCursor);
        isSelected = false;
        currentHandle = None;
        scene()->update();
        return;
    }

    if (isSelected) {
        QGraphicsRectItem::mousePressEvent(event);
    }
}

void LabelRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!isSelected) return;  // 未选中状态下不处理移动

    if (event->buttons() & Qt::LeftButton) {
        QPointF delta = event->pos() - dragStart;
        QRectF r = rect();

        switch (currentHandle) {
        case TopLeft:
            r.setTopLeft(r.topLeft() + delta);
            break;
        case TopRight:
            r.setTopRight(r.topRight() + delta);
            break;
        case BottomLeft:
            r.setBottomLeft(r.bottomLeft() + delta);
            break;
        case BottomRight:
            r.setBottomRight(r.bottomRight() + delta);
            break;
        case Top:
            r.setTop(r.top() + delta.y());
            break;
        case Bottom:
            r.setBottom(r.bottom() + delta.y());
            break;
        case Left:
            r.setLeft(r.left() + delta.x());
            break;
        case Right:
            r.setRight(r.right() + delta.x());
            break;
        case None:
            QGraphicsRectItem::mouseMoveEvent(event);
            scene()->update();
            return;
        }

        if (r.width() >= HandleSize && r.height() >= HandleSize) {
            setRect(r);
            dragStart = event->pos();
            updateZValue();  // 大小改变时更新Z值
        }
        scene()->update();
    }
}

void LabelRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsRectItem::mouseReleaseEvent(event);
}

void LabelRectItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!isSelected) {
        setCursor(Qt::ArrowCursor);
        currentHandle = None;
        return;
    }

    // 更新当前 handle
    currentHandle = getHandle(event->pos());
    updateCursor(currentHandle);
    QGraphicsRectItem::hoverMoveEvent(event);
}

LabelRectItem::HandleType LabelRectItem::getHandle(const QPointF &pos)
{
    QRectF r = rect();
    QRectF handle(0, 0, HandleSize, HandleSize);

    // 检查四个角
    handle.moveCenter(r.topLeft());
    if (handle.contains(pos)) return TopLeft;

    handle.moveCenter(r.topRight());
    if (handle.contains(pos)) return TopRight;

    handle.moveCenter(r.bottomLeft());
    if (handle.contains(pos)) return BottomLeft;

    handle.moveCenter(r.bottomRight());
    if (handle.contains(pos)) return BottomRight;

    // 检查四条边
    const qreal margin = HandleSize / 2;
    if (qAbs(pos.y() - r.top()) <= margin && pos.x() > r.left() + margin && pos.x() < r.right() - margin)
        return Top;
    if (qAbs(pos.y() - r.bottom()) <= margin && pos.x() > r.left() + margin && pos.x() < r.right() - margin)
        return Bottom;
    if (qAbs(pos.x() - r.left()) <= margin && pos.y() > r.top() + margin && pos.y() < r.bottom() - margin)
        return Left;
    if (qAbs(pos.x() - r.right()) <= margin && pos.y() > r.top() + margin && pos.y() < r.bottom() - margin)
        return Right;
    return None;
}

void LabelRectItem::updateCursor(HandleType handle)
{
    switch (handle) {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Top:
    case Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case Left:
    case Right:
        setCursor(Qt::SizeHorCursor);
        break;
    default:
        setCursor(Qt::OpenHandCursor);
        break;
    }
}

QRectF LabelRectItem::sceneRect() const
{
    // 将本地坐标系的矩转换为场景坐标系
    return mapToScene(rect()).boundingRect();
}

void LabelRectItem::keyPressEvent(QKeyEvent *event)
{
    if (!isSelected || !isHovered || !(event->modifiers() & Qt::ShiftModifier)) {
        QGraphicsRectItem::keyPressEvent(event);
        return;
    }

    const qreal step = 1.0;  // 每次移动1个像素
    QRectF r = rect();
    QPointF pos = this->pos();

    switch (event->key()) {
    case Qt::Key_Left:
        if (currentHandle == None) {
            setPos(pos.x() - step, pos.y());
        } else {
            switch (currentHandle) {
            case Left:
            case TopLeft:
            case BottomLeft:
                r.setLeft(r.left() - step);
                break;
            case Right:
            case TopRight:
            case BottomRight:
                r.setRight(r.right() - step);
                break;
            default:
                break;
            }
        }
        break;

    case Qt::Key_Right:
        if (currentHandle == None) {
            setPos(pos.x() + step, pos.y());
        } else {
            switch (currentHandle) {
            case Left:
            case TopLeft:
            case BottomLeft:
                r.setLeft(r.left() + step);
                break;
            case Right:
            case TopRight:
            case BottomRight:
                r.setRight(r.right() + step);
                break;
            default:
                break;
            }
        }
        break;

    case Qt::Key_Up:
        if (currentHandle == None) {
            setPos(pos.x(), pos.y() - step);
        } else {
            switch (currentHandle) {
            case Top:
            case TopLeft:
            case TopRight:
                r.setTop(r.top() - step);
                break;
            case Bottom:
            case BottomLeft:
            case BottomRight:
                r.setBottom(r.bottom() - step);
                break;
            default:
                break;
            }
        }
        break;

    case Qt::Key_Down:
        if (currentHandle == None) {
            setPos(pos.x(), pos.y() + step);
        } else {
            switch (currentHandle) {
            case Top:
            case TopLeft:
            case TopRight:
                r.setTop(r.top() + step);
                break;
            case Bottom:
            case BottomLeft:
            case BottomRight:
                r.setBottom(r.bottom() + step);
                break;
            default:
                break;
            }
        }
        break;

    default:
        QGraphicsRectItem::keyPressEvent(event);
        return;
    }

    if (r.width() >= HandleSize && r.height() >= HandleSize) {
        setRect(r);
    }
    scene()->update();
    event->accept();
}

void LabelRectItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    isHovered = true;
    setFocus();  // 获���键盘焦点
    QGraphicsRectItem::hoverEnterEvent(event);
}

void LabelRectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    isHovered = false;
    currentHandle = None;  // 离开时清除 handle
    clearFocus();
    setCursor(Qt::ArrowCursor);
    QGraphicsRectItem::hoverLeaveEvent(event);
}

void LabelRectItem::updateZValue()
{
    // 计算矩形面积的倒数作为Z值
    // 面积越小，Z值越大，就会显示在上层
    QRectF r = rect();
    qreal area = r.width() * r.height();
    if (area > 0) {
        // 使用一个较大的基数确保Z有足够的精度
        setZValue(100000.0 / area);
    }
}

void LabelRectItem::setRect(const QRectF &rect)
{
    QGraphicsRectItem::setRect(rect);
    updateZValue();  // 当矩形大小改变时更新Z值

    // 更新 label_info 以反映新的矩形尺寸和位置
    label_info.point_x = rect.x();
    label_info.point_y = rect.y();
    label_info.width = rect.width();
    label_info.height = rect.height();

    update();  // 确保重新绘制以反映更改
}

QVariant LabelRectItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemPositionChange || change == QGraphicsItem::ItemScaleChange) {
        // 处理位置或缩放变化
        updateZValue();
        QRectF rect = sceneRect();
        label_info.point_x = rect.x();
        label_info.point_y = rect.y();
        label_info.width = rect.width();
        label_info.height = rect.height();
    }
    return QGraphicsRectItem::itemChange(change, value);
}
