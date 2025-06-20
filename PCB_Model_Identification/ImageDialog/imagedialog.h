#ifndef IMAGEDIALOG_H
#define IMAGEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QScrollBar>
#include <QResizeEvent>

class FlowImageViewer : public QGraphicsView {
    Q_OBJECT
public:
    explicit FlowImageViewer(QGraphicsScene* scene, QWidget* parent = nullptr)
        : QGraphicsView(scene, parent)
        , isDragging(false)
    {
        setDragMode(QGraphicsView::ScrollHandDrag);
        setAlignment(Qt::AlignCenter);  // 设置居中对齐
    }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        // 计算缩放因子
        double factor = 1.15;
        if (event->angleDelta().y() < 0) {
            factor = 1.0 / factor;
        }
        
        // 获取鼠标位置
        QPointF oldPos = mapToScene(event->position().toPoint());
        
        // 执行缩放
        scale(factor, factor);
        
        // 调整视图中心，使鼠标位置保持不变
        QPointF newPos = mapToScene(event->position().toPoint());
        QPointF delta = newPos - oldPos;
        translate(delta.x(), delta.y());
        
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        QGraphicsView::mouseReleaseEvent(event);
        
        if (event->button() == Qt::LeftButton) {
            QPointF scenePos = mapToScene(event->pos());
            QGraphicsItem* item = scene()->itemAt(scenePos, transform());
            
            // 如果点击的不是图片项或背景项，发出信号
            if (!item || (item == scene()->items().last())) {  // 背景项是最后添加的
                emit clickedOutside();
            }
        }
    }

signals:
    void clickedOutside();

private:
    bool isDragging;
};

class ImageDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImageDialog(const QPixmap& pixmap, const QString& originalPath = QString(), QWidget *parent = nullptr) 
        : QDialog(parent)
        , originalPixmap(pixmap)
    {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        
        // 创建半透明黑色背景
        setStyleSheet("background-color: rgba(0, 0, 0, 180);");
        
        // 创建场景和视图
        scene = new QGraphicsScene(this);
        view = new FlowImageViewer(scene);
        
        // 设置视图属性
        view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setBackgroundBrush(Qt::transparent);
        view->setFrameShape(QFrame::NoFrame);
        view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        
        // 添加背景层用于检测点击
        backgroundItem = new QGraphicsRectItem;
        backgroundItem->setPen(Qt::NoPen);
        backgroundItem->setBrush(Qt::transparent);
        scene->addItem(backgroundItem);
        
        // 尝试加载原始图片
        if (!originalPath.isEmpty()) {
            QPixmap fullPixmap(originalPath);
            if (!fullPixmap.isNull()) {
                pixmapItem = scene->addPixmap(fullPixmap);
            } else {
                pixmapItem = scene->addPixmap(pixmap);  // 如果无法加载原图，使用传入的图片
            }
        } else {
            pixmapItem = scene->addPixmap(pixmap);  // 如果没有原图路径，使用传入的图片
        }
        
        pixmapItem->setTransformationMode(Qt::SmoothTransformation);
        
        // 设置布局
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(view);
        
        // 设置对话框大小为屏幕大小的80%
        QSize screenSize = QApplication::primaryScreen()->size();
        resize(screenSize * 0.8);
        
        // 调整图片初始大小以适应视图
        fitInView();
        
        // 连接点击信号
        connect(view, &FlowImageViewer::clickedOutside, this, &ImageDialog::close);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QDialog::resizeEvent(event);
        
        // 更新背景项大小
        QRectF viewRect = view->viewport()->rect();
        QRectF sceneRect = scene->sceneRect();
        
        // 确保场景矩形至少和视图一样大
        sceneRect.setWidth(qMax(sceneRect.width(), viewRect.width()));
        sceneRect.setHeight(qMax(sceneRect.height(), viewRect.height()));
        
        scene->setSceneRect(sceneRect);
        backgroundItem->setRect(sceneRect);
        
        if (!event->oldSize().isValid()) {
            fitInView();
        }
    }
    
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Escape) {
            close();
        } else if (event->key() == Qt::Key_Space) {
            fitInView();
        }
    }

private:
    void fitInView()
    {
        if (pixmapItem) {
            // 计算图片和视图的尺寸
            QRectF pixmapRect = pixmapItem->boundingRect();
            QRectF viewRect = view->viewport()->rect();
            
            // 计算缩放比例
            qreal scaleW = viewRect.width() / pixmapRect.width();
            qreal scaleH = viewRect.height() / pixmapRect.height();
            qreal scale = qMin(scaleW, scaleH);
            
            // 重置变换
            view->resetTransform();
            view->scale(scale, scale);
            
            // 确保场景矩形足够大
            QRectF sceneRect = pixmapRect;
            sceneRect.setWidth(qMax(pixmapRect.width(), viewRect.width() / scale));
            sceneRect.setHeight(qMax(pixmapRect.height(), viewRect.height() / scale));
            
            // 设置场景矩形并居中图片
            scene->setSceneRect(sceneRect);
            pixmapItem->setPos((sceneRect.width() - pixmapRect.width()) / 2,
                              (sceneRect.height() - pixmapRect.height()) / 2);
            
            // 居中视图
            view->centerOn(pixmapItem);
        }
    }

    QGraphicsScene* scene;
    FlowImageViewer* view;
    QGraphicsPixmapItem* pixmapItem;
    QGraphicsRectItem* backgroundItem;
    QPixmap originalPixmap;
};

#endif // IMAGEDIALOG_H 