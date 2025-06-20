#ifndef WIRINGSCHEMATIC_H
#define WIRINGSCHEMATIC_H

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QColor>
#include <QPixmap>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include "wiringresource.h"

// 原理图组件类型
enum class SchematicComponentType {
    RESISTOR,
    CAPACITOR,
    INDUCTOR,
    DIODE,
    TRANSISTOR,
    IC,
    CONNECTOR,
    POWER_SOURCE,
    DMM,
    TEST_POINT,
    WIRE
};

// 原理图组件信息
struct SchematicComponent {
    QString component_id;
    SchematicComponentType type;
    QString label;
    QString value;
    QPointF position;
    QSizeF size;
    double rotation;
    QList<QPointF> pin_positions;
    QStringList pin_labels;
    
    QJsonObject toJson() const;
    static SchematicComponent fromJson(const QJsonObject& json);
};

// 原理图连线信息
struct SchematicWire {
    QString wire_id;
    QPointF start_point;
    QPointF end_point;
    QString color;
    double width;
    QString label;
    bool is_highlighted;
    
    QJsonObject toJson() const;
    static SchematicWire fromJson(const QJsonObject& json);
};

// 自定义图形项基类
class SchematicGraphicsItem : public QGraphicsItem
{
public:
    SchematicGraphicsItem(const SchematicComponent& component, QGraphicsItem* parent = nullptr);
    
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    void setHighlighted(bool highlighted);
    void setSelected(bool selected);
    SchematicComponent getComponent() const { return component_; }
    
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    
private:
    void drawResistor(QPainter* painter);
    void drawCapacitor(QPainter* painter);
    void drawInductor(QPainter* painter);
    void drawDiode(QPainter* painter);
    void drawIC(QPainter* painter);
    void drawConnector(QPainter* painter);
    void drawPowerSource(QPainter* painter);
    void drawDMM(QPainter* painter);
    void drawTestPoint(QPainter* painter);
    
    SchematicComponent component_;
    bool is_highlighted_;
    bool is_selected_;
    bool is_hovered_;
};

// 自定义连线图形项
class SchematicWireItem : public QGraphicsLineItem
{
public:
    SchematicWireItem(const SchematicWire& wire, QGraphicsItem* parent = nullptr);
    
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    void setHighlighted(bool highlighted);
    SchematicWire getWire() const { return wire_; }
    
private:
    SchematicWire wire_;
    bool is_highlighted_;
};

// 原理图显示窗口
class WiringSchematic : public QWidget
{
    Q_OBJECT

public:
    explicit WiringSchematic(QWidget* parent = nullptr);
    ~WiringSchematic();
    
    // 设置接线配置
    void setWiringConfiguration(const WiringConfiguration& config);
    
    // 显示控制
    void showConnectionStep(int step_index);
    void highlightConnection(const WiringConnection& connection);
    void clearHighlights();
    
    // 缩放控制
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void resetZoom();
    
    // 导出功能
    bool exportToPNG(const QString& file_path);
    bool exportToPDF(const QString& file_path);

public slots:
    void onZoomChanged(int zoom_percent);
    void onShowGrid(bool show);
    void onShowLabels(bool show);
    void onAnimateConnections(bool animate);

signals:
    void componentClicked(const QString& component_id);
    void connectionClicked(const QString& connection_id);
    void zoomLevelChanged(double zoom_level);

private slots:
    void onAnimationTimer();

private:
    void setupUI();
    void setupToolbar();
    void setupGraphicsView();
    void connectSignals();
    
    void generateSchematic();
    void addComponentToScene(const SchematicComponent& component);
    void addWireToScene(const SchematicWire& wire);
    void updateLayout();
    void calculateComponentPositions();
    
    void createComponentLibrary();
    QPixmap createComponentSymbol(SchematicComponentType type, const QSizeF& size);
    
    // UI组件
    QVBoxLayout* main_layout_;
    QToolBar* toolbar_;
    QGraphicsView* graphics_view_;
    QGraphicsScene* graphics_scene_;
    
    QAction* zoom_in_action_;
    QAction* zoom_out_action_;
    QAction* zoom_fit_action_;
    QAction* zoom_reset_action_;
    QAction* show_grid_action_;
    QAction* show_labels_action_;
    QAction* animate_action_;
    
    QSlider* zoom_slider_;
    QLabel* zoom_label_;
    QComboBox* view_mode_combo_;
    
    // 数据成员
    WiringConfiguration current_config_;
    QList<SchematicGraphicsItem*> component_items_;
    QList<SchematicWireItem*> wire_items_;
    
    double zoom_level_;
    bool show_grid_;
    bool show_labels_;
    bool animate_connections_;
    int current_step_;
    QTimer* animation_timer_;
    int animation_frame_;

public:
    // 样式设置
    static const QColor GRID_COLOR;
    static const QColor BACKGROUND_COLOR;
    static const QColor COMPONENT_COLOR;
    static const QColor SELECTED_COLOR;
    static const QColor HIGHLIGHTED_COLOR;
    static const QColor WIRE_COLOR;
    static const QColor POWER_WIRE_COLOR;
    static const QColor GROUND_WIRE_COLOR;
    
    static const double MIN_ZOOM;
    static const double MAX_ZOOM;
    static const double ZOOM_STEP;
};

#endif // WIRINGSCHEMATIC_H
