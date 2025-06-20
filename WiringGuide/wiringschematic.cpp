#include "wiringschematic.h"
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QPrinter>
#include <QPainter>
// 移除QSvgGenerator，使用QPainter代替SVG导出功能

// 样式常量定义
const QColor WiringSchematic::GRID_COLOR = QColor(200, 200, 200);
const QColor WiringSchematic::BACKGROUND_COLOR = QColor(255, 255, 255);
const QColor WiringSchematic::COMPONENT_COLOR = QColor(100, 150, 200);
const QColor WiringSchematic::SELECTED_COLOR = QColor(255, 100, 100);
const QColor WiringSchematic::HIGHLIGHTED_COLOR = QColor(255, 255, 0);
const QColor WiringSchematic::WIRE_COLOR = QColor(50, 50, 50);
const QColor WiringSchematic::POWER_WIRE_COLOR = QColor(255, 0, 0);
const QColor WiringSchematic::GROUND_WIRE_COLOR = QColor(0, 0, 0);

const double WiringSchematic::MIN_ZOOM = 0.1;
const double WiringSchematic::MAX_ZOOM = 5.0;
const double WiringSchematic::ZOOM_STEP = 0.1;

//=============================================================================
// SchematicComponent JSON序列化实现
//=============================================================================

QJsonObject SchematicComponent::toJson() const {
    QJsonObject obj;
    obj["component_id"] = component_id;
    obj["type"] = static_cast<int>(type);
    obj["label"] = label;
    obj["value"] = value;
    obj["position_x"] = position.x();
    obj["position_y"] = position.y();
    obj["size_width"] = size.width();
    obj["size_height"] = size.height();
    obj["rotation"] = rotation;
    
    QJsonArray pins_array;
    for (const auto& pin : pin_positions) {
        QJsonObject pin_obj;
        pin_obj["x"] = pin.x();
        pin_obj["y"] = pin.y();
        pins_array.append(pin_obj);
    }
    obj["pin_positions"] = pins_array;
    
    QJsonArray labels_array;
    for (const auto& label : pin_labels) {
        labels_array.append(label);
    }
    obj["pin_labels"] = labels_array;
    
    return obj;
}

SchematicComponent SchematicComponent::fromJson(const QJsonObject& json) {
    SchematicComponent comp;
    comp.component_id = json["component_id"].toString();
    comp.type = static_cast<SchematicComponentType>(json["type"].toInt());
    comp.label = json["label"].toString();
    comp.value = json["value"].toString();
    comp.position = QPointF(json["position_x"].toDouble(), json["position_y"].toDouble());
    comp.size = QSizeF(json["size_width"].toDouble(), json["size_height"].toDouble());
    comp.rotation = json["rotation"].toDouble();
    
    QJsonArray pins_array = json["pin_positions"].toArray();
    for (const auto& pin_value : pins_array) {
        QJsonObject pin_obj = pin_value.toObject();
        comp.pin_positions.append(QPointF(pin_obj["x"].toDouble(), pin_obj["y"].toDouble()));
    }
    
    QJsonArray labels_array = json["pin_labels"].toArray();
    for (const auto& label_value : labels_array) {
        comp.pin_labels.append(label_value.toString());
    }
    
    return comp;
}

//=============================================================================
// SchematicWire JSON序列化实现
//=============================================================================

QJsonObject SchematicWire::toJson() const {
    QJsonObject obj;
    obj["wire_id"] = wire_id;
    obj["start_x"] = start_point.x();
    obj["start_y"] = start_point.y();
    obj["end_x"] = end_point.x();
    obj["end_y"] = end_point.y();
    obj["color"] = color;
    obj["width"] = width;
    obj["label"] = label;
    obj["is_highlighted"] = is_highlighted;
    return obj;
}

SchematicWire SchematicWire::fromJson(const QJsonObject& json) {
    SchematicWire wire;
    wire.wire_id = json["wire_id"].toString();
    wire.start_point = QPointF(json["start_x"].toDouble(), json["start_y"].toDouble());
    wire.end_point = QPointF(json["end_x"].toDouble(), json["end_y"].toDouble());
    wire.color = json["color"].toString();
    wire.width = json["width"].toDouble();
    wire.label = json["label"].toString();
    wire.is_highlighted = json["is_highlighted"].toBool();
    return wire;
}

//=============================================================================
// SchematicGraphicsItem 实现
//=============================================================================

SchematicGraphicsItem::SchematicGraphicsItem(const SchematicComponent& component, QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , component_(component)
    , is_highlighted_(false)
    , is_selected_(false)
    , is_hovered_(false)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setAcceptHoverEvents(true);
}

QRectF SchematicGraphicsItem::boundingRect() const
{
    return QRectF(component_.position, component_.size);
}

void SchematicGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    painter->save();
      // 设置画笔和画刷
    QPen pen(WiringSchematic::COMPONENT_COLOR, 2);
    QBrush brush(WiringSchematic::COMPONENT_COLOR.lighter(150));
    
    if (is_highlighted_) {
        pen.setColor(WiringSchematic::HIGHLIGHTED_COLOR);
        brush.setColor(WiringSchematic::HIGHLIGHTED_COLOR.lighter(150));
    } else if (is_selected_) {
        pen.setColor(WiringSchematic::SELECTED_COLOR);
        brush.setColor(WiringSchematic::SELECTED_COLOR.lighter(150));
    } else if (is_hovered_) {
        pen.setWidth(3);
        brush.setColor(WiringSchematic::COMPONENT_COLOR.lighter(120));
    }
    
    painter->setPen(pen);
    painter->setBrush(brush);
    
    // 根据组件类型绘制不同形状
    switch (component_.type) {
    case SchematicComponentType::RESISTOR:
        drawResistor(painter);
        break;
    case SchematicComponentType::CAPACITOR:
        drawCapacitor(painter);
        break;
    case SchematicComponentType::INDUCTOR:
        drawInductor(painter);
        break;
    case SchematicComponentType::DIODE:
        drawDiode(painter);
        break;
    case SchematicComponentType::IC:
        drawIC(painter);
        break;
    case SchematicComponentType::CONNECTOR:
        drawConnector(painter);
        break;
    case SchematicComponentType::POWER_SOURCE:
        drawPowerSource(painter);
        break;
    case SchematicComponentType::DMM:
        drawDMM(painter);
        break;
    case SchematicComponentType::TEST_POINT:
        drawTestPoint(painter);
        break;
    default:
        painter->drawRect(boundingRect());
        break;
    }
    
    // 绘制标签
    painter->setPen(QPen(Qt::black, 1));
    QFont font = painter->font();
    font.setPointSize(8);
    painter->setFont(font);
    
    QRectF text_rect = boundingRect();
    text_rect.moveTop(text_rect.bottom() + 5);
    painter->drawText(text_rect, Qt::AlignCenter, component_.label);
    
    // 绘制数值
    if (!component_.value.isEmpty()) {
        text_rect.moveTop(text_rect.bottom() + 2);
        QFont value_font = font;
        value_font.setPointSize(7);
        painter->setFont(value_font);
        painter->drawText(text_rect, Qt::AlignCenter, component_.value);
    }
    
    painter->restore();
}

void SchematicGraphicsItem::drawResistor(QPainter* painter)
{
    QRectF rect = boundingRect();
    
    // 绘制电阻的锯齿形状
    QPainterPath path;
    double width = rect.width();
    double height = rect.height();
    double step = width / 8;
    
    path.moveTo(rect.left(), rect.center().y());
    for (int i = 0; i < 4; ++i) {
        path.lineTo(rect.left() + step * (2 * i + 1), rect.top());
        path.lineTo(rect.left() + step * (2 * i + 2), rect.bottom());
    }
    path.lineTo(rect.right(), rect.center().y());
    
    painter->drawPath(path);
}

void SchematicGraphicsItem::drawCapacitor(QPainter* painter)
{
    QRectF rect = boundingRect();
    double center_x = rect.center().x();
    
    // 绘制电容的两个平行板
    painter->drawLine(center_x - 5, rect.top(), center_x - 5, rect.bottom());
    painter->drawLine(center_x + 5, rect.top(), center_x + 5, rect.bottom());
    
    // 绘制连接线
    painter->drawLine(rect.left(), rect.center().y(), center_x - 5, rect.center().y());
    painter->drawLine(center_x + 5, rect.center().y(), rect.right(), rect.center().y());
}

void SchematicGraphicsItem::drawInductor(QPainter* painter)
{
    QRectF rect = boundingRect();
    
    // 绘制电感的线圈形状
    QPainterPath path;
    double width = rect.width();
    double coil_width = width / 4;
    
    path.moveTo(rect.left(), rect.center().y());
    for (int i = 0; i < 4; ++i) {
        QRectF coil_rect(rect.left() + i * coil_width, rect.top(), 
                        coil_width, rect.height());
        path.arcTo(coil_rect, 0, 180);
    }
    
    painter->drawPath(path);
}

void SchematicGraphicsItem::drawDiode(QPainter* painter)
{
    QRectF rect = boundingRect();
    
    // 绘制二极管的三角形和直线
    QPolygonF triangle;
    triangle << QPointF(rect.center().x() - 10, rect.top())
             << QPointF(rect.center().x() - 10, rect.bottom())
             << QPointF(rect.center().x() + 5, rect.center().y());
    
    painter->drawPolygon(triangle);
    painter->drawLine(rect.center().x() + 5, rect.top(), 
                     rect.center().x() + 5, rect.bottom());
    
    // 连接线
    painter->drawLine(rect.left(), rect.center().y(), 
                     rect.center().x() - 10, rect.center().y());
    painter->drawLine(rect.center().x() + 5, rect.center().y(), 
                     rect.right(), rect.center().y());
}

void SchematicGraphicsItem::drawIC(QPainter* painter)
{
    QRectF rect = boundingRect();
    
    // 绘制IC的矩形外壳
    painter->drawRect(rect);
    
    // 绘制引脚
    int pin_count = component_.pin_positions.size();
    if (pin_count > 0) {
        double pin_spacing = rect.height() / (pin_count / 2 + 1);
        
        for (int i = 0; i < pin_count / 2; ++i) {
            // 左侧引脚
            double y = rect.top() + pin_spacing * (i + 1);
            painter->drawLine(rect.left() - 5, y, rect.left(), y);
            
            // 右侧引脚
            painter->drawLine(rect.right(), y, rect.right() + 5, y);
        }
    }
}

void SchematicGraphicsItem::drawConnector(QPainter* painter)
{
    QRectF rect = boundingRect();
    
    // 绘制连接器的圆形
    painter->drawEllipse(rect);
    
    // 绘制中心点
    painter->setBrush(QBrush(Qt::black));
    painter->drawEllipse(rect.center(), 2, 2);
}

void SchematicGraphicsItem::drawPowerSource(QPainter* painter)
{
    QRectF rect = boundingRect();
    
    // 绘制电源的圆形符号
    painter->drawEllipse(rect);
    
    // 绘制正负号
    painter->setPen(QPen(Qt::black, 2));
    double center_x = rect.center().x();
    double center_y = rect.center().y();
    
    // 正号
    painter->drawLine(center_x - 8, center_y - 5, center_x - 8, center_y + 5);
    painter->drawLine(center_x - 13, center_y, center_x - 3, center_y);
    
    // 负号
    painter->drawLine(center_x + 3, center_y, center_x + 13, center_y);
}

void SchematicGraphicsItem::drawDMM(QPainter* painter)
{
    QRectF rect = boundingRect();
    
    // 绘制万用表的矩形外形
    painter->drawRect(rect);
    
    // 绘制显示屏
    QRectF screen_rect = rect.adjusted(5, 5, -5, -rect.height()/2);
    painter->fillRect(screen_rect, QBrush(Qt::black));
    
    // 绘制探头连接点
    painter->setBrush(QBrush(Qt::red));
    painter->drawEllipse(rect.bottomLeft() + QPointF(10, -10), 4, 4);
    painter->setBrush(QBrush(Qt::black));
    painter->drawEllipse(rect.bottomRight() + QPointF(-10, -10), 4, 4);
}

void SchematicGraphicsItem::drawTestPoint(QPainter* painter)
{
    QRectF rect = boundingRect();
    
    // 绘制测试点的小圆圈
    painter->setBrush(QBrush(Qt::yellow));
    painter->drawEllipse(rect);
    
    // 绘制中心十字线
    painter->setPen(QPen(Qt::black, 1));
    painter->drawLine(rect.center().x(), rect.top(), 
                     rect.center().x(), rect.bottom());
    painter->drawLine(rect.left(), rect.center().y(), 
                     rect.right(), rect.center().y());
}

void SchematicGraphicsItem::setHighlighted(bool highlighted)
{
    is_highlighted_ = highlighted;
    update();
}

void SchematicGraphicsItem::setSelected(bool selected)
{
    is_selected_ = selected;
    update();
}

void SchematicGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    // 这里可以添加鼠标点击处理逻辑
    QGraphicsItem::mousePressEvent(event);
}

void SchematicGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    is_hovered_ = true;
    update();
    QGraphicsItem::hoverEnterEvent(event);
}

void SchematicGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    is_hovered_ = false;
    update();
    QGraphicsItem::hoverLeaveEvent(event);
}

//=============================================================================
// SchematicWireItem 实现
//=============================================================================

SchematicWireItem::SchematicWireItem(const SchematicWire& wire, QGraphicsItem* parent)
    : QGraphicsLineItem(wire.start_point.x(), wire.start_point.y(),
                       wire.end_point.x(), wire.end_point.y(), parent)
    , wire_(wire)
    , is_highlighted_(false)
{
    QPen pen(QColor(wire.color), wire.width);
    setPen(pen);
}

void SchematicWireItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    QPen pen = this->pen();
    
    if (is_highlighted_) {
        pen.setColor(WiringSchematic::HIGHLIGHTED_COLOR);
        pen.setWidth(pen.width() + 2);
    }
    
    setPen(pen);
    QGraphicsLineItem::paint(painter, option, widget);
    
    // 绘制标签
    if (!wire_.label.isEmpty()) {
        painter->save();
        QPointF mid_point = (wire_.start_point + wire_.end_point) / 2;
        painter->setPen(QPen(Qt::black, 1));
        QFont font = painter->font();
        font.setPointSize(7);
        painter->setFont(font);
        painter->drawText(mid_point, wire_.label);
        painter->restore();
    }
}

void SchematicWireItem::setHighlighted(bool highlighted)
{
    is_highlighted_ = highlighted;
    update();
}

//=============================================================================
// WiringSchematic 实现
//=============================================================================

WiringSchematic::WiringSchematic(QWidget* parent)
    : QWidget(parent)
    , main_layout_(nullptr)
    , toolbar_(nullptr)
    , graphics_view_(nullptr)
    , graphics_scene_(nullptr)
    , zoom_in_action_(nullptr)
    , zoom_out_action_(nullptr)
    , zoom_fit_action_(nullptr)
    , zoom_reset_action_(nullptr)
    , show_grid_action_(nullptr)
    , show_labels_action_(nullptr)
    , animate_action_(nullptr)
    , zoom_slider_(nullptr)
    , zoom_label_(nullptr)
    , view_mode_combo_(nullptr)
    , animation_timer_(nullptr)
    , zoom_level_(1.0)
    , show_grid_(true)
    , show_labels_(true)
    , animate_connections_(false)
    , current_step_(0)
    , animation_frame_(0)
{
    try {
        setupUI();          
        setupToolbar();     
        setupGraphicsView();
        connectSignals();   
        
        // 初始化动画定时器
        animation_timer_ = new QTimer(this);
        animation_timer_->setInterval(500); // 500ms间隔
        connect(animation_timer_, &QTimer::timeout, this, &WiringSchematic::onAnimationTimer);
        
        qDebug() << "WiringSchematic 初始化完成";
    } catch (const std::exception& e) {
        qDebug() << "WiringSchematic 初始化失败:" << e.what();
    } catch (...) {
        qDebug() << "WiringSchematic 初始化失败: 未知错误";
    }
}

WiringSchematic::~WiringSchematic()
{
    if (graphics_scene_) {
        graphics_scene_->clear();
    }
}

void WiringSchematic::setupUI()
{
    try {
        main_layout_ = new QVBoxLayout(this);
        main_layout_->setContentsMargins(0, 0, 0, 0);
        main_layout_->setSpacing(0);
        qDebug() << "WiringSchematic::setupUI 完成";
    } catch (const std::exception& e) {
        qDebug() << "WiringSchematic::setupUI 失败:" << e.what();
    }
}

void WiringSchematic::setupToolbar()
{
    try {
        if (!main_layout_) {
            qDebug() << "WiringSchematic::setupToolbar - main_layout_ 为空";
            return;
        }
        
        toolbar_ = new QToolBar("原理图工具栏", this);
        main_layout_->addWidget(toolbar_);
        
        // 缩放控制
        zoom_in_action_ = toolbar_->addAction("放大");
        zoom_out_action_ = toolbar_->addAction("缩小");
        zoom_fit_action_ = toolbar_->addAction("适应窗口");
        zoom_reset_action_ = toolbar_->addAction("重置缩放");
        
        toolbar_->addSeparator();
        
        // 显示控制
        show_grid_action_ = toolbar_->addAction("显示网格");
        show_grid_action_->setCheckable(true);
        show_grid_action_->setChecked(true);
        
        show_labels_action_ = toolbar_->addAction("显示标签");
        show_labels_action_->setCheckable(true);
        show_labels_action_->setChecked(true);
        
        animate_action_ = toolbar_->addAction("连接动画");
        animate_action_->setCheckable(true);
        animate_action_->setChecked(false);
        
        toolbar_->addSeparator();
        
        // 缩放滑块
        QLabel* zoom_label_text = new QLabel("缩放:");
        toolbar_->addWidget(zoom_label_text);
        
        zoom_slider_ = new QSlider(Qt::Horizontal);
        zoom_slider_->setRange(static_cast<int>(MIN_ZOOM * 100), 
                              static_cast<int>(MAX_ZOOM * 100));
        zoom_slider_->setValue(100);
        zoom_slider_->setMaximumWidth(100);
        toolbar_->addWidget(zoom_slider_);
        
        zoom_label_ = new QLabel("100%");
        toolbar_->addWidget(zoom_label_);
        
        toolbar_->addSeparator();
        
        // 视图模式
        QLabel* mode_label = new QLabel("视图:");
        toolbar_->addWidget(mode_label);
        
        view_mode_combo_ = new QComboBox();
        view_mode_combo_->addItems({"标准视图", "连接视图", "分层视图"});
        toolbar_->addWidget(view_mode_combo_);
        
        qDebug() << "WiringSchematic::setupToolbar 完成";
    } catch (const std::exception& e) {
        qDebug() << "WiringSchematic::setupToolbar 失败:" << e.what();
    }
}

void WiringSchematic::setupGraphicsView()
{
    try {
        if (!main_layout_) {
            qDebug() << "WiringSchematic::setupGraphicsView - main_layout_ 为空";
            return;
        }
        
        graphics_scene_ = new QGraphicsScene(this);
        graphics_scene_->setBackgroundBrush(QBrush(BACKGROUND_COLOR));
        
        graphics_view_ = new QGraphicsView(graphics_scene_);
        graphics_view_->setRenderHint(QPainter::Antialiasing);
        graphics_view_->setDragMode(QGraphicsView::RubberBandDrag);
        
        main_layout_->addWidget(graphics_view_);
        
        qDebug() << "WiringSchematic::setupGraphicsView 完成";
    } catch (const std::exception& e) {
        qDebug() << "WiringSchematic::setupGraphicsView 失败:" << e.what();
    }
}

void WiringSchematic::connectSignals()
{
    try {
        // 工具栏动作信号
        if (zoom_in_action_) 
            connect(zoom_in_action_, &QAction::triggered, this, &WiringSchematic::zoomIn);
        if (zoom_out_action_) 
            connect(zoom_out_action_, &QAction::triggered, this, &WiringSchematic::zoomOut);
        if (zoom_fit_action_) 
            connect(zoom_fit_action_, &QAction::triggered, this, &WiringSchematic::zoomToFit);
        if (zoom_reset_action_) 
            connect(zoom_reset_action_, &QAction::triggered, this, &WiringSchematic::resetZoom);
        
        if (show_grid_action_) 
            connect(show_grid_action_, &QAction::toggled, this, &WiringSchematic::onShowGrid);
        if (show_labels_action_) 
            connect(show_labels_action_, &QAction::toggled, this, &WiringSchematic::onShowLabels);
        if (animate_action_) 
            connect(animate_action_, &QAction::toggled, this, &WiringSchematic::onAnimateConnections);
        
        // 缩放滑块信号
        if (zoom_slider_) 
            connect(zoom_slider_, &QSlider::valueChanged, this, &WiringSchematic::onZoomChanged);
            
        qDebug() << "WiringSchematic::connectSignals 完成";
    } catch (const std::exception& e) {
        qDebug() << "WiringSchematic::connectSignals 失败:" << e.what();
    }
}

void WiringSchematic::setWiringConfiguration(const WiringConfiguration& config)
{
    current_config_ = config;
    generateSchematic();
}

void WiringSchematic::generateSchematic()
{
    // 清空当前场景
    graphics_scene_->clear();
    component_items_.clear();
    wire_items_.clear();
    
    if (current_config_.steps.isEmpty()) {
        return;
    }
    
    // 为简化实现，这里创建一些基本的原理图组件
    
    // 创建被测元件
    SchematicComponent component;
    component.component_id = current_config_.component_reference;
    component.type = SchematicComponentType::RESISTOR; // 简化为电阻
    component.label = current_config_.component_reference;
    component.value = "测试元件";
    component.position = QPointF(200, 150);
    component.size = QSizeF(80, 40);
    addComponentToScene(component);
    
    // 创建电源
    for (const auto& resource : current_config_.used_resources) {
        if (resource.type == ResourceType::POWER_OUTPUT) {
            SchematicComponent power;
            power.component_id = resource.name;
            power.type = SchematicComponentType::POWER_SOURCE;
            power.label = resource.name;
            power.value = QString("%1V/%2A").arg(current_config_.total_voltage)
                                            .arg(current_config_.total_current);
            power.position = QPointF(50, 100);
            power.size = QSizeF(60, 60);
            addComponentToScene(power);
            
            // 创建连接线
            SchematicWire wire1, wire2;
            wire1.wire_id = "power_positive";
            wire1.start_point = QPointF(110, 130);
            wire1.end_point = QPointF(200, 170);
            wire1.color = "red";
            wire1.width = 2;
            wire1.label = "+";
            addWireToScene(wire1);
            
            wire2.wire_id = "power_negative";
            wire2.start_point = QPointF(110, 130);
            wire2.end_point = QPointF(200, 170);
            wire2.color = "black";
            wire2.width = 2;
            wire2.label = "-";
            addWireToScene(wire2);
            break;
        }
    }
    
    // 创建万用表
    for (const auto& resource : current_config_.used_resources) {
        if (resource.type == ResourceType::DMM) {
            SchematicComponent dmm;
            dmm.component_id = resource.name;
            dmm.type = SchematicComponentType::DMM;
            dmm.label = resource.name;
            dmm.value = "数字万用表";
            dmm.position = QPointF(350, 100);
            dmm.size = QSizeF(80, 100);
            addComponentToScene(dmm);
            
            // 创建测量连接线
            SchematicWire measure1, measure2;
            measure1.wire_id = "dmm_positive";
            measure1.start_point = QPointF(350, 150);
            measure1.end_point = QPointF(280, 170);
            measure1.color = "red";
            measure1.width = 1;
            measure1.label = "V+";
            addWireToScene(measure1);
            
            measure2.wire_id = "dmm_negative";
            measure2.start_point = QPointF(350, 180);
            measure2.end_point = QPointF(280, 170);
            measure2.color = "black";
            measure2.width = 1;
            measure2.label = "V-";
            addWireToScene(measure2);
            break;
        }
    }
    
    // 创建测试点
    SchematicComponent test_point1, test_point2;
    test_point1.component_id = "TP1";
    test_point1.type = SchematicComponentType::TEST_POINT;
    test_point1.label = "TP1";
    test_point1.position = QPointF(180, 120);
    test_point1.size = QSizeF(15, 15);
    addComponentToScene(test_point1);
    
    test_point2.component_id = "TP2";
    test_point2.type = SchematicComponentType::TEST_POINT;
    test_point2.label = "TP2";
    test_point2.position = QPointF(300, 120);
    test_point2.size = QSizeF(15, 15);
    addComponentToScene(test_point2);
    
    // 自动调整视图
    zoomToFit();
}

void WiringSchematic::addComponentToScene(const SchematicComponent& component)
{
    SchematicGraphicsItem* item = new SchematicGraphicsItem(component);
    graphics_scene_->addItem(item);
    component_items_.append(item);
    
    // 连接信号
    // 注意：QGraphicsItem不是QObject，所以不能直接连接信号
    // 这里需要通过scene的信号来处理
}

void WiringSchematic::addWireToScene(const SchematicWire& wire)
{
    SchematicWireItem* item = new SchematicWireItem(wire);
    graphics_scene_->addItem(item);
    wire_items_.append(item);
}

void WiringSchematic::showConnectionStep(int step_index)
{
    current_step_ = step_index;
    
    if (step_index < 0 || step_index >= current_config_.steps.size()) {
        clearHighlights();
        return;
    }
    
    // 清除之前的高亮
    clearHighlights();
    
    // 高亮当前步骤的连接
    const auto& step = current_config_.steps[step_index];
    for (const auto& connection : step.connections) {
        highlightConnection(connection);
    }
    
    // 如果启用了动画
    if (animate_connections_) {
        animation_frame_ = 0;
        animation_timer_->start();
    }
}

void WiringSchematic::highlightConnection(const WiringConnection& connection)
{
    // 高亮相关的连线
    for (auto* wire_item : wire_items_) {
        SchematicWire wire = wire_item->getWire();
        if (wire.color == connection.wire_color ||
            wire.label.contains(connection.resource_port.name) ||
            wire.label.contains(connection.target_point.label)) {
            wire_item->setHighlighted(true);
        }
    }
    
    // 高亮相关的组件
    for (auto* comp_item : component_items_) {
        SchematicComponent comp = comp_item->getComponent();
        if (comp.label.contains(connection.resource_port.name) ||
            comp.label.contains(connection.target_point.label) ||
            comp.component_id == connection.target_point.point_id) {
            comp_item->setHighlighted(true);
        }
    }
}

void WiringSchematic::clearHighlights()
{
    for (auto* comp_item : component_items_) {
        comp_item->setHighlighted(false);
    }
    
    for (auto* wire_item : wire_items_) {
        wire_item->setHighlighted(false);
    }
    
    if (animation_timer_->isActive()) {
        animation_timer_->stop();
    }
}

void WiringSchematic::zoomIn()
{
    zoom_level_ = qMin(zoom_level_ + ZOOM_STEP, MAX_ZOOM);
    graphics_view_->setTransform(QTransform::fromScale(zoom_level_, zoom_level_));
    emit zoomLevelChanged(zoom_level_);
    
    zoom_slider_->setValue(static_cast<int>(zoom_level_ * 100));
    zoom_label_->setText(QString("%1%").arg(static_cast<int>(zoom_level_ * 100)));
}

void WiringSchematic::zoomOut()
{
    zoom_level_ = qMax(zoom_level_ - ZOOM_STEP, MIN_ZOOM);
    graphics_view_->setTransform(QTransform::fromScale(zoom_level_, zoom_level_));
    emit zoomLevelChanged(zoom_level_);
    
    zoom_slider_->setValue(static_cast<int>(zoom_level_ * 100));
    zoom_label_->setText(QString("%1%").arg(static_cast<int>(zoom_level_ * 100)));
}

void WiringSchematic::zoomToFit()
{
    if (graphics_scene_->items().isEmpty()) {
        return;
    }
    
    QRectF scene_rect = graphics_scene_->itemsBoundingRect();
    graphics_view_->fitInView(scene_rect, Qt::KeepAspectRatio);
    
    // 计算缩放级别
    QTransform transform = graphics_view_->transform();
    zoom_level_ = qBound(MIN_ZOOM, transform.m11(), MAX_ZOOM);
    
    emit zoomLevelChanged(zoom_level_);
    zoom_slider_->setValue(static_cast<int>(zoom_level_ * 100));
    zoom_label_->setText(QString("%1%").arg(static_cast<int>(zoom_level_ * 100)));
}

void WiringSchematic::resetZoom()
{
    zoom_level_ = 1.0;
    graphics_view_->setTransform(QTransform::fromScale(zoom_level_, zoom_level_));
    emit zoomLevelChanged(zoom_level_);
    
    zoom_slider_->setValue(100);
    zoom_label_->setText("100%");
}

void WiringSchematic::onZoomChanged(int zoom_percent)
{
    zoom_level_ = zoom_percent / 100.0;
    zoom_level_ = qBound(MIN_ZOOM, zoom_level_, MAX_ZOOM);
    
    graphics_view_->setTransform(QTransform::fromScale(zoom_level_, zoom_level_));
    emit zoomLevelChanged(zoom_level_);
    
    zoom_label_->setText(QString("%1%").arg(zoom_percent));
}

void WiringSchematic::onShowGrid(bool show)
{
    show_grid_ = show;
    // 这里可以添加网格显示的实现
    graphics_view_->update();
}

void WiringSchematic::onShowLabels(bool show)
{
    show_labels_ = show;
    
    // 更新所有组件的标签显示
    for (auto* item : component_items_) {
        item->update();
    }
    
    for (auto* item : wire_items_) {
        item->update();
    }
}

void WiringSchematic::onAnimateConnections(bool animate)
{
    animate_connections_ = animate;
    
    if (!animate && animation_timer_->isActive()) {
        animation_timer_->stop();
    }
}

void WiringSchematic::onAnimationTimer()
{
    animation_frame_++;
    
    // 简单的闪烁动画
    bool highlight = (animation_frame_ % 2) == 0;
    
    if (current_step_ >= 0 && current_step_ < current_config_.steps.size()) {
        const auto& step = current_config_.steps[current_step_];
        for (const auto& connection : step.connections) {
            // 这里可以实现更复杂的动画效果
            // 当前只是简单的闪烁
        }
    }
    
    // 动画持续一定时间后停止
    if (animation_frame_ > 10) {
        animation_timer_->stop();
        animation_frame_ = 0;
    }
}

bool WiringSchematic::exportToPNG(const QString& file_path)
{
    try {
        QPixmap pixmap(graphics_scene_->sceneRect().size().toSize());
        pixmap.fill(Qt::white);
        
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        graphics_scene_->render(&painter);
        
        return pixmap.save(file_path, "PNG");
        
    } catch (const std::exception& e) {
        qDebug() << "导出PNG失败:" << e.what();
        return false;
    }
}

bool WiringSchematic::exportToPDF(const QString& file_path)
{
    try {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(file_path);
        
        // Set page orientation to Portrait (default is usually Portrait anyway)
        printer.setPageOrientation(QPageLayout::Portrait);
        
        QPainter painter(&printer);
        graphics_scene_->render(&painter);
        
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "导出PDF失败:" << e.what();
        return false;
    }
}
