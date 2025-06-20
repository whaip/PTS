#include "imageflowdialog.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QtMath>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsScene>
#include <QResizeEvent>
#include <QTimer>
#include <QShowEvent>
#include <QGraphicsDropShadowEffect>
#include <QPainterPath>
#include <QPen>
#include <QLabel>
#include <QFileInfo>
#include <QPushButton>
#include <QHBoxLayout>

CoverFlowView::CoverFlowView(ImageFlowDialog* parent)
    : QGraphicsView(parent)
    , imageFlowDialog(parent)
{
}

void CoverFlowView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        lastPos = event->pos();
        isDragging = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void CoverFlowView::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging) {
        QPointF delta = event->pos() - lastPos;
        if (qAbs(delta.x()) > 5) {
            // 根据spacing调整拖动灵敏度
            qreal sensitivity = imageFlowDialog->getDragSensitivity() * 
                              (imageFlowDialog->getSpacing() / 200.0);  // 200是原始spacing参考值
            int indexDelta = -delta.x() / sensitivity;
            if (indexDelta != 0) {
                int newIndex = qBound(0, 
                    imageFlowDialog->getCurrentIndex() + indexDelta,
                    imageFlowDialog->getItemCount() - 1);
                if (newIndex != imageFlowDialog->getCurrentIndex()) {
                    imageFlowDialog->animateToIndex(newIndex);
                }
                lastPos = event->pos();
            }
        }
    }
}

void CoverFlowView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void CoverFlowView::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    if (delta > 0) {
        imageFlowDialog->showPreviousImage();
    } else if (delta < 0) {
        imageFlowDialog->showNextImage();
    }
    event->accept();
}

ImageFlowDialog::ImageFlowDialog(QWidget *parent)
    : QDialog(parent)
    , currentImageIndex(0)
    , spacing(0)
    , dragSensitivity(70)
    , centerScale(1.0)
    , sideScale(0.8)
    , lateralDeviation(400)
{
    setupImageViewer();
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

ImageFlowDialog::~ImageFlowDialog()
{
}

void ImageFlowDialog::setupImageViewer()
{
    // 设置固定大小
    setFixedSize(800, 400);  // 增加高度以容纳按钮
    
    scene = new QGraphicsScene(this);
    view = new CoverFlowView(this);
    view->setScene(scene);
    
    // 创建路径标签
    pathLabel = new QLabel(this);
    pathLabel->setAlignment(Qt::AlignCenter);
    pathLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   background-color: rgba(0, 0, 0, 100);"
        "   padding: 5px 10px;"
        "   border-radius: 5px;"
        "   font-size: 12px;"
        "   min-width: 200px;"
        "   max-height: 30px;"
        "}"
    );
    pathLabel->hide();
    
    // 创建按钮
    confirmButton = new QPushButton("确认", this);
    cancelButton = new QPushButton("取消", this);
    
    // 设置按钮样式
    QString buttonStyle = 
        "QPushButton {"
        "   background-color: #2196F3;"
        "   color: white;"
        "   border: none;"
        "   padding: 5px 15px;"
        "   border-radius: 3px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #1976D2;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #1565C0;"
        "}";
    
    confirmButton->setStyleSheet(buttonStyle);
    cancelButton->setStyleSheet(buttonStyle);
    
    // 创建按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(confirmButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch();
    
    // 主布局
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 10, 0, 10);
    layout->setSpacing(10);
    
    layout->addWidget(pathLabel, 0, Qt::AlignHCenter);
    layout->addWidget(view, 1);
    layout->addLayout(buttonLayout);
    
    setLayout(layout);
    
    // 设置窗口背景
    setStyleSheet("background-color: #1e1e1e;");
    
    // 连接按钮信号
    connect(confirmButton, &QPushButton::clicked, this, &ImageFlowDialog::onConfirmClicked);
    connect(cancelButton, &QPushButton::clicked, this, &ImageFlowDialog::onCancelClicked);
    
    // 设置视图属性
    view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    view->setAlignment(Qt::AlignCenter);
    view->setFocusPolicy(Qt::NoFocus);
    
    // 设置背景样式
    view->setBackgroundBrush(QColor(30, 30, 30));
    view->setStyleSheet("border: none;");
}

void ImageFlowDialog::loadImages(const QStringList& imagePaths)
{
    // 清除现有图片
    for (auto item : imageItems) {
        scene->removeItem(item);
        delete item;
    }
    imageItems.clear();
    currentImageIndex = 0;

    // 设置圆形区域的直径
    int circleSize = height() * 0.7;  // 使用窗口高度的70%作为圆的直径
    
    // 加载新图片
    for (const QString &path : imagePaths) {
        QPixmap originalPixmap(path);
        
        // 计算缩放比例，使图片刚好内切于圆形
        qreal scaleRatio;
        qreal imageRatio = (qreal)originalPixmap.width() / originalPixmap.height();
        
        // 使用勾股定理计算内切矩形的尺寸
        // 对于圆内最大矩形，如果矩形宽高比为r，则：
        // width = diameter * r / sqrt(1 + r*r)
        // height = diameter / sqrt(1 + r*r)
        qreal width = circleSize * imageRatio / sqrt(1 + imageRatio * imageRatio);
        qreal height = circleSize / sqrt(1 + imageRatio * imageRatio);
        
        // 按计算出的尺寸缩放图片
        QPixmap scaledPixmap = originalPixmap.scaled(
            width,
            height,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        
        // 创建一个正方形的透明背景
        QPixmap squarePixmap(circleSize, circleSize);
        squarePixmap.fill(Qt::transparent);
        
        // 在正方形中央绘制缩放后的图片
        QPainter squarePainter(&squarePixmap);
        squarePainter.setRenderHint(QPainter::Antialiasing);
        int x = (circleSize - scaledPixmap.width()) / 2;
        int y = (circleSize - scaledPixmap.height()) / 2;
        squarePainter.drawPixmap(x, y, scaledPixmap);
        
        // 创建圆形遮罩
        QPixmap roundPixmap(circleSize, circleSize);
        roundPixmap.fill(Qt::transparent);
        
        QPainter painter(&roundPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // 创建圆形路径
        QPainterPath clipPath;  // 重命名为 clipPath 以避免混淆
        clipPath.addEllipse(0, 0, circleSize, circleSize);
        
        painter.setClipPath(clipPath);
        painter.drawPixmap(0, 0, squarePixmap);
        
        // 添加圆形边框
        painter.setPen(QPen(QColor(255, 255, 255, 50), 2));
        painter.drawEllipse(1, 1, circleSize-2, circleSize-2);
        
        // 创建 AnimatedPixmapItem 时传入原始路径
        AnimatedPixmapItem *item = new AnimatedPixmapItem(roundPixmap, path);  // 使用原始的文件路径
        scene->addItem(item);
        item->setTransformOriginPoint(item->boundingRect().center());
        imageItems.append(item);
    }

    if (!imageItems.isEmpty()) {
        // 根据圆的直径调整间距
        spacing = circleSize * 1.2;  // 间距设为圆直径的120%
        
        if (isVisible()) {
            updateImagePositions();
        }
    }
}

void ImageFlowDialog::updateImagePositions()
{
    if (imageItems.isEmpty()) return;

    QRectF viewRect = view->viewport()->rect();
    qreal centerX = viewRect.width() / 2 - lateralDeviation;  // 使用lateralDeviation
    qreal centerY = viewRect.height() * 0.05;
    qreal verticalOffset = -20;
    
    // 更新所有图片的位置和变换
    for (int i = 0; i < imageItems.size(); ++i) {
        AnimatedPixmapItem *item = imageItems[i];
        qreal itemX = centerX + (i - currentImageIndex) * spacing;
        
        // 计算距离中心的偏移量
        qreal distance = qAbs(i - currentImageIndex);
        
        // 修改缩放计算，使右侧图片比左侧图片大一些
        qreal scale;
        if (i > currentImageIndex) {
            // 右侧图片
            scale = qMax(sideScale, centerScale - (distance * 0.12));  // 减小缩放衰减
        } else if (i < currentImageIndex) {
            // 左侧图片
            scale = qMax(sideScale, centerScale - (distance * 0.18));  // 增加缩放衰减
        } else {
            // 中心图片
            scale = centerScale;
        }
        
        qreal yOffset = distance * 3;
        
        // 计算图片实际高度（考虑缩放）
        qreal scaledHeight = item->boundingRect().height() * scale;
        
        // 设置位置，右侧图片位置稍微调整
        qreal xOffset = 0;
        if (i > currentImageIndex) {
            xOffset = distance * 20;  // 右侧图片间距稍微增加
        }
        
        qreal yPos = centerY - (scaledHeight/2) + yOffset + verticalOffset;
        item->setPos(itemX - item->boundingRect().width()/2 + xOffset, yPos);
        item->setScale(scale);
        
        // 设置Z值
        item->setZValue(imageItems.size() - qAbs(i - currentImageIndex));
        
        // 设置倾斜角度，调整右侧图片的角度
        qreal rotation;
        if (i > currentImageIndex) {
            rotation = (i - currentImageIndex) * 8;  // 右侧图片倾斜角度稍小
        } else if (i < currentImageIndex) {
            rotation = (i - currentImageIndex) * 12;  // 左侧图片倾斜角度稍大
        } else {
            rotation = 0;
        }
        item->setRotation(rotation);
        
        // 为中心图片添加高亮和阴影效果
        if (i == currentImageIndex) {
            item->setOpacity(1.0);
            item->setBorderVisible(true);
            
            // 更新并显示路径标签
            QString path = item->originalPath();
            QFileInfo fileInfo(path);
            QString displayText = fileInfo.fileName();  // 只显示文件名
            pathLabel->setText(displayText);
            pathLabel->setVisible(true);
            
            // 创建发光效果
            QGraphicsDropShadowEffect *glowEffect = new QGraphicsDropShadowEffect(this);
            glowEffect->setColor(QColor(64, 185, 255, 180));
            glowEffect->setBlurRadius(30);
            glowEffect->setOffset(0);
            item->setGraphicsEffect(glowEffect);
            
            // 设置更粗的边框
            QPen highlightPen(QColor(150, 220, 255, 200));
            highlightPen.setWidth(4);
            item->setBorderPen(highlightPen);
        } else {
            item->setOpacity(0.6);
            item->setBorderVisible(false);
            item->setGraphicsEffect(nullptr);
            item->setBorderPen(QPen(QColor(255, 255, 255, 50), 2));  // 恢复默认边框
        }
    }
}

void ImageFlowDialog::animateToIndex(int newIndex)
{
    if (newIndex == currentImageIndex) return;
    
    QParallelAnimationGroup *group = new QParallelAnimationGroup(this);
    
    QRectF viewRect = view->viewport()->rect();
    qreal centerX = viewRect.width() / 2 - lateralDeviation;  // 使用lateralDeviation
    qreal centerY = viewRect.height() * 0.05;
    qreal verticalOffset = -20;
    
    for (int i = 0; i < imageItems.size(); ++i) {
        AnimatedPixmapItem *item = imageItems[i];
        QPropertyAnimation *animation = new QPropertyAnimation(item, "pos", this);
        animation->setDuration(300);
        
        // 计算新位置
        qreal newDistance = qAbs(i - newIndex);
        qreal newX = centerX + (i - newIndex) * spacing;
        qreal newYOffset = newDistance * 3;
        
        // 计算缩放，与updateImagePositions保持一致
        qreal scale;
        if (i > newIndex) {
            scale = qMax(sideScale, centerScale - (newDistance * 0.12));
        } else if (i < newIndex) {
            scale = qMax(sideScale, centerScale - (newDistance * 0.18));
        } else {
            scale = centerScale;
        }
        
        // 添加右侧图片的额外偏移
        qreal xOffset = 0;
        if (i > newIndex) {
            xOffset = newDistance * 20;
        }
        
        qreal scaledHeight = item->boundingRect().height() * scale;
        qreal yPos = centerY - (scaledHeight/2) + newYOffset + verticalOffset;
        
        animation->setEndValue(QPointF(newX - item->boundingRect().width()/2 + xOffset, yPos));
        
        // 添加旋转动画，调整角度
        QPropertyAnimation *rotationAnim = new QPropertyAnimation(item, "rotation", this);
        rotationAnim->setDuration(300);
        qreal endRotation;
        if (i > newIndex) {
            endRotation = (i - newIndex) * 8;
        } else if (i < newIndex) {
            endRotation = (i - newIndex) * 12;
        } else {
            endRotation = 0;
        }
        rotationAnim->setEndValue(endRotation);
        group->addAnimation(rotationAnim);
        
        // 添加缩放动画
        QPropertyAnimation *scaleAnim = new QPropertyAnimation(item, "scale", this);
        scaleAnim->setDuration(300);
        scaleAnim->setEndValue(scale);
        group->addAnimation(scaleAnim);
        
        group->addAnimation(animation);
    }
    
    currentImageIndex = newIndex;
    connect(group, &QParallelAnimationGroup::finished, [this]() {
        updateImagePositions();
    });
    
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void ImageFlowDialog::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Left:
            showPreviousImage();
            event->accept();
            break;
        case Qt::Key_Right:
            showNextImage();
            event->accept();
            break;
        default:
            QWidget::keyPressEvent(event);
    }
}

void ImageFlowDialog::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateImagePositions();
}

void ImageFlowDialog::showPreviousImage()
{
    if (currentImageIndex > 0) {
        animateToIndex(currentImageIndex - 1);
    }
}

void ImageFlowDialog::showNextImage()
{
    if (currentImageIndex < imageItems.size() - 1) {
        animateToIndex(currentImageIndex + 1);
    }
}

void ImageFlowDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 延迟一下更新位置，确保窗口大小已经准备好
    QTimer::singleShot(100, this, [this]() {
        updateImagePositions();
    });
}

void ImageFlowDialog::onConfirmClicked()
{
    if (!imageItems.isEmpty() && currentImageIndex >= 0 && currentImageIndex < imageItems.size()) {
        QString selectedPath = imageItems[currentImageIndex]->originalPath();
        emit imageSelected(selectedPath);
    }
    accept();
}

void ImageFlowDialog::onCancelClicked()
{
    reject();
} 
