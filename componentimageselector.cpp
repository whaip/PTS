#include "componentimageselector.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QGraphicsProxyWidget>
#include <QCursor>
#include <QTimer>

// ComponentRectItem 实现
const qreal ComponentRectItem::HandleSize = 8.0;

ComponentRectItem::ComponentRectItem(QGraphicsItem *parent)
    : QGraphicsRectItem(parent)
    , currentHandle_(None)
    , isSelected_(false)
    , isHovered_(false)
    , rectPen_(Qt::red, 2)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsFocusable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
}

void ComponentRectItem::setSelected(bool selected)
{
    isSelected_ = selected;
    update();
}

QRectF ComponentRectItem::getComponentRect() const
{
    return rect();
}

void ComponentRectItem::setComponentRect(const QRectF& rect)
{
    setRect(rect);
    update();
}

void ComponentRectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    painter->save();
    
    // 绘制矩形边框
    painter->setPen(rectPen_);
    painter->setBrush(QBrush(Qt::transparent));
    painter->drawRect(rect());
    
    // 如果选中，绘制控制点
    if (isSelected_) {
        drawHandles(painter);
    }
    
    painter->restore();
}

void ComponentRectItem::drawHandles(QPainter *painter)
{
    QRectF r = rect();
    painter->setPen(QPen(Qt::red, 1));
    painter->setBrush(QBrush(Qt::white, Qt::SolidPattern));
    
    QRectF handle(0, 0, HandleSize, HandleSize);
    
    // 绘制四个角的控制点
    handle.moveCenter(r.topLeft());
    painter->drawRect(handle);
    
    handle.moveCenter(r.topRight());
    painter->drawRect(handle);
    
    handle.moveCenter(r.bottomLeft());
    painter->drawRect(handle);
    
    handle.moveCenter(r.bottomRight());
    painter->drawRect(handle);
    
    // 绘制四条边的中点控制点
    handle.moveCenter(QPointF(r.center().x(), r.top()));
    painter->drawRect(handle);
    
    handle.moveCenter(QPointF(r.center().x(), r.bottom()));
    painter->drawRect(handle);
    
    handle.moveCenter(QPointF(r.left(), r.center().y()));
    painter->drawRect(handle);
    
    handle.moveCenter(QPointF(r.right(), r.center().y()));
    painter->drawRect(handle);
}

ComponentRectItem::HandleType ComponentRectItem::getHandle(const QPointF &pos)
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

void ComponentRectItem::updateCursor(HandleType handle)
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
        setCursor(Qt::ArrowCursor);
        break;
    }
}

void ComponentRectItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragStart_ = event->pos();
        currentHandle_ = getHandle(event->pos());
        setSelected(true);
    }
    QGraphicsRectItem::mousePressEvent(event);
}

void ComponentRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!isSelected_) return;
    
    if (event->buttons() & Qt::LeftButton) {
        QPointF delta = event->pos() - dragStart_;
        QRectF newRect = rect();
        
        switch (currentHandle_) {
        case TopLeft:
            newRect.setTopLeft(newRect.topLeft() + delta);
            break;
        case TopRight:
            newRect.setTopRight(newRect.topRight() + delta);
            break;
        case BottomLeft:
            newRect.setBottomLeft(newRect.bottomLeft() + delta);
            break;
        case BottomRight:
            newRect.setBottomRight(newRect.bottomRight() + delta);
            break;
        case Top:
            newRect.setTop(newRect.top() + delta.y());
            break;
        case Bottom:
            newRect.setBottom(newRect.bottom() + delta.y());
            break;
        case Left:
            newRect.setLeft(newRect.left() + delta.x());
            break;
        case Right:
            newRect.setRight(newRect.right() + delta.x());
            break;
        case None:
            // 移动整个矩形
            newRect.translate(delta);
            break;
        }
        
        // 确保矩形有最小尺寸
        if (newRect.width() < 10) {
            if (currentHandle_ == Left || currentHandle_ == TopLeft || currentHandle_ == BottomLeft) {
                newRect.setLeft(newRect.right() - 10);
            } else {
                newRect.setRight(newRect.left() + 10);
            }
        }
        
        if (newRect.height() < 10) {
            if (currentHandle_ == Top || currentHandle_ == TopLeft || currentHandle_ == TopRight) {
                newRect.setTop(newRect.bottom() - 10);
            } else {
                newRect.setBottom(newRect.top() + 10);
            }
        }
        
        setRect(newRect);
        dragStart_ = event->pos();
    }
}

void ComponentRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsRectItem::mouseReleaseEvent(event);
}

void ComponentRectItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!isSelected_) {
        QGraphicsRectItem::hoverMoveEvent(event);
        return;
    }
    
    currentHandle_ = getHandle(event->pos());
    updateCursor(currentHandle_);
    QGraphicsRectItem::hoverMoveEvent(event);
}

void ComponentRectItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    isHovered_ = true;
    QGraphicsRectItem::hoverEnterEvent(event);
}

void ComponentRectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    isHovered_ = false;
    setCursor(Qt::ArrowCursor);
    QGraphicsRectItem::hoverLeaveEvent(event);
}

// ComponentImageEditor 实现
ComponentImageEditor::ComponentImageEditor(const cv::Mat& image, QWidget *parent)
    : QGraphicsView(parent)
    , original_image_(image.clone())
    , scene_(nullptr)
    , image_item_(nullptr)
    , rect_item_(nullptr)
    , is_creating_(false)
    , display_scale_(1.0)
{
    setupEditor();
    loadImage(original_image_);
}

ComponentImageEditor::ComponentImageEditor(const cv::Mat& image, const QRect& initialRect, QWidget *parent)
    : QGraphicsView(parent)
    , original_image_(image.clone())
    , scene_(nullptr)
    , image_item_(nullptr)
    , rect_item_(nullptr)
    , is_creating_(false)
    , display_scale_(1.0)
{
    setupEditor();
    loadImage(original_image_);
    setSelection(initialRect);
}

ComponentImageEditor::~ComponentImageEditor()
{
    if (scene_) {
        scene_->clear();
        delete scene_;
    }
}

void ComponentImageEditor::setupEditor()
{
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    
    // 不使用 OpenGL，使用标准渲染
    setCacheMode(QGraphicsView::CacheBackground);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    
    setDragMode(QGraphicsView::NoDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ComponentImageEditor::loadImage(const cv::Mat& image)
{
    if (image.empty()) {
        qWarning() << "ComponentImageEditor::loadImage - 图像为空";
        return;
    }
    
    scene_->clear();
    
    QPixmap pixmap = matToQPixmap(image);
    if (pixmap.isNull()) {
        qWarning() << "ComponentImageEditor::loadImage - 图像转换失败";
        return;
    }
    
    image_item_ = scene_->addPixmap(pixmap);
    scene_->setSceneRect(pixmap.rect());
    
    // 适配视图
    fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}

QPixmap ComponentImageEditor::matToQPixmap(const cv::Mat& mat)
{
    if (mat.empty()) return QPixmap();
    
    try {
        cv::Mat processedMat = mat;
        const int MAX_SIZE = 1024;
        
        if (mat.cols > MAX_SIZE || mat.rows > MAX_SIZE) {
            display_scale_ = std::min(static_cast<double>(MAX_SIZE) / mat.cols,
                                     static_cast<double>(MAX_SIZE) / mat.rows);
            
            int newWidth = static_cast<int>(mat.cols * display_scale_);
            int newHeight = static_cast<int>(mat.rows * display_scale_);
            
            cv::resize(mat, processedMat, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LINEAR);
        } else {
            display_scale_ = 1.0;
        }
        
        if (!processedMat.isContinuous()) {
            processedMat = processedMat.clone();
        }
        
        QImage qimage;
        if (processedMat.channels() == 3) {
            cv::Mat rgbMat;
            cv::cvtColor(processedMat, rgbMat, cv::COLOR_BGR2RGB);
            qimage = QImage(rgbMat.data, rgbMat.cols, rgbMat.rows, rgbMat.step, QImage::Format_RGB888).copy();
        } else if (processedMat.channels() == 1) {
            qimage = QImage(processedMat.data, processedMat.cols, processedMat.rows, 
                           processedMat.step, QImage::Format_Grayscale8).copy();
        } else {
            qWarning() << "ComponentImageEditor::matToQPixmap - 不支持的通道数:" << processedMat.channels();
            return QPixmap();
        }
        
        return QPixmap::fromImage(qimage);
        
    } catch (const std::exception& e) {
        qCritical() << "ComponentImageEditor::matToQPixmap - 异常:" << e.what();
        return QPixmap();
    }
}

QRect ComponentImageEditor::getSelectedRect() const
{
    if (!rect_item_) return QRect();
    
    QRectF sceneRect = rect_item_->getComponentRect();
    
    // 转换回原始图像坐标
    int x = static_cast<int>(sceneRect.x() / display_scale_);
    int y = static_cast<int>(sceneRect.y() / display_scale_);
    int w = static_cast<int>(sceneRect.width() / display_scale_);
    int h = static_cast<int>(sceneRect.height() / display_scale_);
    
    return QRect(x, y, w, h);
}

bool ComponentImageEditor::hasSelection() const
{
    return rect_item_ != nullptr && rect_item_->getSelected();
}

void ComponentImageEditor::setSelection(const QRect& rect)
{
    if (rect.isEmpty()) {
        clearSelection();
        return;
    }
    
    // 转换为场景坐标
    QRectF sceneRect(rect.x() * display_scale_, rect.y() * display_scale_,
                     rect.width() * display_scale_, rect.height() * display_scale_);
    
    if (!rect_item_) {
        rect_item_ = new ComponentRectItem();
        scene_->addItem(rect_item_);
    }
    
    rect_item_->setComponentRect(sceneRect);
    rect_item_->setSelected(true);
    
    emit selectionChanged(rect);
}

void ComponentImageEditor::clearSelection()
{
    if (rect_item_) {
        scene_->removeItem(rect_item_);
        delete rect_item_;
        rect_item_ = nullptr;
        emit selectionChanged(QRect());
    }
}

void ComponentImageEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        
        // 检查是否点击在已有的矩形上
        if (rect_item_ && rect_item_->contains(scenePos)) {
            // 让矩形项处理事件
            QGraphicsView::mousePressEvent(event);
            return;
        }
        
        // 开始创建新的选择区域
        is_creating_ = true;
        creation_start_ = scenePos;
        
        // 清除旧的选择
        clearSelection();
        
        // 创建新的矩形项
        rect_item_ = new ComponentRectItem();
        rect_item_->setComponentRect(QRectF(scenePos, scenePos));
        rect_item_->setSelected(true);
        scene_->addItem(rect_item_);
    }
    
    QGraphicsView::mousePressEvent(event);
}

void ComponentImageEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && is_creating_) {
        is_creating_ = false;
        
        if (rect_item_) {
            QRectF currentRect = rect_item_->getComponentRect();
            
            // 检查选择区域是否有效
            if (currentRect.width() < 5 || currentRect.height() < 5) {
                clearSelection();
            } else {
                // 规范化矩形
                QRectF normalizedRect = currentRect.normalized();
                rect_item_->setComponentRect(normalizedRect);
                
                // 发出选择变化信号
                emit selectionChanged(getSelectedRect());
            }
        }
    }
    
    QGraphicsView::mouseReleaseEvent(event);
}

void ComponentImageEditor::wheelEvent(QWheelEvent *event)
{
    // 缩放功能
    const double scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
        scale(scaleFactor, scaleFactor);
    } else {
        scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }
}

void ComponentImageEditor::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (scene_) {
        fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    }
}

void ComponentImageEditor::onSelectionChanged()
{
    // 处理选择变化
}

QPointF ComponentImageEditor::mapToImageCoordinates(const QPointF& scenePos)
{
    return QPointF(scenePos.x() / display_scale_, scenePos.y() / display_scale_);
}

QPointF ComponentImageEditor::mapToSceneCoordinates(const QPointF& imagePos)
{
    return QPointF(imagePos.x() * display_scale_, imagePos.y() * display_scale_);
}

// ComponentImageSelector 实现
ComponentImageSelector::ComponentImageSelector(const cv::Mat& image, QWidget *parent)
    : QDialog(parent)
    , has_selection_(false)
{
    setWindowTitle("选择元器件位置");
    setModal(true);
    resize(800, 600);
    
    editor_ = new ComponentImageEditor(image, this);
    setupUI();
    
    connect(editor_, &ComponentImageEditor::selectionChanged, 
            this, &ComponentImageSelector::onSelectionChanged);
}

ComponentImageSelector::ComponentImageSelector(const cv::Mat& image, const QRect& initialRect, QWidget *parent)
    : QDialog(parent)
    , current_selection_(initialRect)
    , has_selection_(!initialRect.isEmpty())
{
    setWindowTitle("选择元器件位置");
    setModal(true);
    resize(800, 600);
    
    editor_ = new ComponentImageEditor(image, initialRect, this);
    setupUI();
    
    connect(editor_, &ComponentImageEditor::selectionChanged, 
            this, &ComponentImageSelector::onSelectionChanged);
    
    updateButtonStates();
}

void ComponentImageSelector::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 工具栏
    QHBoxLayout* toolLayout = new QHBoxLayout();
    
    QLabel* instructionLabel = new QLabel("请在图像上拖拽选择元器件区域，可以拖拽控制点调整大小");
    instructionLabel->setStyleSheet("font-weight: bold; color: blue;");
    
    toolLayout->addWidget(instructionLabel);
    toolLayout->addStretch();
    
    // 按钮
    reset_button_ = new QPushButton("重新选择");
    confirm_button_ = new QPushButton("确认选择");
    cancel_button_ = new QPushButton("取消");
    
    toolLayout->addWidget(reset_button_);
    toolLayout->addWidget(confirm_button_);
    toolLayout->addWidget(cancel_button_);
    
    mainLayout->addLayout(toolLayout);
    
    // 编辑器
    mainLayout->addWidget(editor_);
    
    // 信息标签
    info_label_ = new QLabel();
    mainLayout->addWidget(info_label_);
    
    // 连接信号
    connect(confirm_button_, &QPushButton::clicked, this, &ComponentImageSelector::onConfirm);
    connect(cancel_button_, &QPushButton::clicked, this, &ComponentImageSelector::onCancel);
    connect(reset_button_, &QPushButton::clicked, this, &ComponentImageSelector::onReset);
    
    updateButtonStates();
}

void ComponentImageSelector::updateButtonStates()
{
    confirm_button_->setEnabled(has_selection_);
    if (has_selection_) {
        info_label_->setText(QString("选择区域: x=%1, y=%2, w=%3, h=%4")
                            .arg(current_selection_.x())
                            .arg(current_selection_.y())
                            .arg(current_selection_.width())
                            .arg(current_selection_.height()));
    } else {
        info_label_->setText("请选择一个区域");
    }
}

QRect ComponentImageSelector::getSelectedRect() const
{
    return current_selection_;
}

bool ComponentImageSelector::hasSelection() const
{
    return has_selection_;
}

void ComponentImageSelector::onConfirm()
{
    if (has_selection_) {
        emit selectionConfirmed(current_selection_);
        
        // 使用定时器延迟关闭，确保信号处理完成
        QTimer::singleShot(100, this, [this]() {
            accept();
        });
    } else {
        QMessageBox::warning(this, "警告", "请先选择一个区域！");
    }
}

void ComponentImageSelector::onCancel()
{
    emit selectionCanceled();
    
    // 使用定时器延迟关闭
    QTimer::singleShot(100, this, [this]() {
        reject();
    });
}

void ComponentImageSelector::onReset()
{
    editor_->clearSelection();
    has_selection_ = false;
    current_selection_ = QRect();
    updateButtonStates();
}

void ComponentImageSelector::onSelectionChanged(const QRect& rect)
{
    current_selection_ = rect;
    has_selection_ = !rect.isEmpty();
    updateButtonStates();
}
