#ifndef IMAGEFLOWDIALOG_H
#define IMAGEFLOWDIALOG_H

#include <QDialog>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPropertyAnimation>
#include <QGraphicsPixmapItem>
#include <QParallelAnimationGroup>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QStyleOption>
#include <QGraphicsSceneMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QLabel>
#include <QFileInfo>
#include <QPushButton>
#include <QHBoxLayout>
#include "imagedialog.h"

class ImageFlowDialog;  // 前向声明

class CoverFlowView : public QGraphicsView {
    Q_OBJECT
public:
    explicit CoverFlowView(ImageFlowDialog* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override {
        event->ignore(); // 忽略视图的键盘事件，让事件传递给父窗口
    }
    void wheelEvent(QWheelEvent *event) override;

private:
    QPointF lastPos;
    bool isDragging = false;
    ImageFlowDialog* imageFlowDialog;
};

// 自定义可动画的图片项
class AnimatedPixmapItem : public QObject, public QGraphicsPixmapItem {
    Q_OBJECT
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation)
    Q_PROPERTY(qreal scale READ scale WRITE setScale)

public:
    AnimatedPixmapItem(const QPixmap& pixmap, const QString& originalPath = QString(), QGraphicsItem* parent = nullptr)
        : QGraphicsPixmapItem(pixmap, parent)
        , m_originalPath(originalPath) {
        borderRect = new QGraphicsEllipseItem(this);
        updateBorder();
    }
    
    qreal rotation() const { return m_rotation; }
    void setRotation(qreal angle) {
        m_rotation = angle;
        QGraphicsPixmapItem::setRotation(angle);
    }

    void setBorderVisible(bool visible) {
        if (borderRect) {
            borderRect->setVisible(visible);
        }
    }

    void setBorderPen(const QPen& pen) {  // 添加设置边框样式的方法
        if (borderRect) {
            borderRect->setPen(pen);
        }
    }

    void updateBorder() {
        if (borderRect) {
            QRectF rect = boundingRect();
            borderRect->setRect(rect);
            borderRect->setPen(QPen(QColor(255, 255, 255, 50), 2));
            borderRect->setBrush(Qt::NoBrush);
        }
    }
    
    QString originalPath() const { return m_originalPath; }

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override {
        QGraphicsPixmapItem::mouseDoubleClickEvent(event);
        
        // 创建并显示对话框，传递原始路径
        ImageDialog* dialog = new ImageDialog(pixmap(), m_originalPath);
        dialog->exec();
        dialog->deleteLater();
    }

private:
    qreal m_rotation = 0;
    QGraphicsEllipseItem* borderRect = nullptr;  // 使用 EllipseItem
    QString m_originalPath;
};

class ImageFlowDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImageFlowDialog(QWidget *parent = nullptr);
    ~ImageFlowDialog();

    void loadImages(const QStringList& imagePaths);  // 加载图片列表
    void showPreviousImage();
    void showNextImage();
    void animateToIndex(int index);
    qreal getSpacing() const { return spacing; }
    qreal getDragSensitivity() const { return dragSensitivity; }
    int getCurrentIndex() const { return currentImageIndex; }
    int getItemCount() const { return imageItems.size(); }

signals:
    void imageSelected(const QString& imagePath);  // 新增信号：当确认选择时发送

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onConfirmClicked();  // 新增：确认按钮点击处理
    void onCancelClicked();   // 新增：取消按钮点击处理

private:
    void setupImageViewer();
    void updateImagePositions();
    
    QGraphicsScene *scene;
    CoverFlowView *view;
    QList<AnimatedPixmapItem*> imageItems;
    int currentImageIndex;
    qreal spacing;      // 图片之间的视觉间距
    qreal dragSensitivity;  // 添加拖动灵敏度参数
    qreal centerScale;  // 中心图片的缩放比例
    qreal sideScale;    // 两侧图片的缩放比例
    QLabel* pathLabel;  // 添加标签成员
    int lateralDeviation;
    
    // 新增按钮成员
    QPushButton* confirmButton;
    QPushButton* cancelButton;
};

#endif // IMAGEFLOWDIALOG_H 