#ifndef COMPONENTIMAGESELECTOR_H
#define COMPONENTIMAGESELECTOR_H

#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QPoint>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <opencv2/opencv.hpp>

// 可拖拽调整的组件矩形项
class ComponentRectItem : public QGraphicsRectItem
{
public:
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

    explicit ComponentRectItem(QGraphicsItem *parent = nullptr);
    
    void setSelected(bool selected);
    bool getSelected() const { return isSelected_; }
    
    QRectF getComponentRect() const;
    void setComponentRect(const QRectF& rect);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    HandleType getHandle(const QPointF &pos);
    void updateCursor(HandleType handle);
    void drawHandles(QPainter *painter);

private:
    static const qreal HandleSize;
    HandleType currentHandle_;
    QPointF dragStart_;
    bool isSelected_;
    bool isHovered_;
    QPen rectPen_;
};

// 基于 QGraphicsView 的组件位置编辑器
class ComponentImageEditor : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ComponentImageEditor(const cv::Mat& image, QWidget *parent = nullptr);
    explicit ComponentImageEditor(const cv::Mat& image, const QRect& initialRect, QWidget *parent = nullptr);
    ~ComponentImageEditor();

    QRect getSelectedRect() const;
    bool hasSelection() const;
    void setSelection(const QRect& rect);
    void clearSelection();

signals:
    void selectionChanged(const QRect& rect);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSelectionChanged();

private:
    void setupEditor();
    void loadImage(const cv::Mat& image);
    QPixmap matToQPixmap(const cv::Mat& mat);
    QPointF mapToImageCoordinates(const QPointF& scenePos);
    QPointF mapToSceneCoordinates(const QPointF& imagePos);

private:
    cv::Mat original_image_;
    QGraphicsScene* scene_;
    QGraphicsPixmapItem* image_item_;
    ComponentRectItem* rect_item_;
    
    bool is_creating_;
    QPointF creation_start_;
    double display_scale_;
};

// 组件位置选择对话框
class ComponentImageSelector : public QDialog
{
    Q_OBJECT

public:
    explicit ComponentImageSelector(const cv::Mat& image, QWidget *parent = nullptr);
    explicit ComponentImageSelector(const cv::Mat& image, const QRect& initialRect, QWidget *parent = nullptr);
    
    QRect getSelectedRect() const;
    bool hasSelection() const;

signals:
    void selectionConfirmed(const QRect& rect);
    void selectionCanceled();

private slots:
    void onConfirm();
    void onCancel();
    void onReset();
    void onSelectionChanged(const QRect& rect);

private:
    void setupUI();
    void updateButtonStates();

private:
    ComponentImageEditor* editor_;
    QPushButton* confirm_button_;
    QPushButton* cancel_button_;
    QPushButton* reset_button_;
    QLabel* info_label_;
    
    QRect current_selection_;
    bool has_selection_;
};

#endif // COMPONENTIMAGESELECTOR_H