#include "wiringresource.h"
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>
#include <QDebug>
#include <algorithm>

// ResourcePort JSON序列化实现
QJsonObject ResourcePort::toJson() const {
    QJsonObject obj;
    obj["port_id"] = port_id;
    obj["type"] = static_cast<int>(type);
    obj["name"] = name;
    obj["description"] = description;
    obj["is_available"] = is_available;
    obj["max_voltage"] = max_voltage;
    obj["max_current"] = max_current;
    obj["device_name"] = device_name;
    obj["device_channel"] = device_channel;
    return obj;
}

ResourcePort ResourcePort::fromJson(const QJsonObject& json) {
    ResourcePort port;
    port.port_id = json["port_id"].toInt();
    port.type = static_cast<ResourceType>(json["type"].toInt());
    port.name = json["name"].toString();
    port.description = json["description"].toString();
    port.is_available = json["is_available"].toBool();
    port.max_voltage = json["max_voltage"].toDouble();
    port.max_current = json["max_current"].toDouble();
    port.device_name = json["device_name"].toString();
    port.device_channel = json["device_channel"].toInt();
    return port;
}

// ConnectionPoint JSON序列化实现
QJsonObject ConnectionPoint::toJson() const {
    QJsonObject obj;
    obj["point_id"] = point_id;
    obj["type"] = static_cast<int>(type);
    obj["label"] = label;
    obj["description"] = description;
    obj["polarity"] = static_cast<int>(polarity);
    obj["x_position"] = x_position;
    obj["y_position"] = y_position;
    return obj;
}

ConnectionPoint ConnectionPoint::fromJson(const QJsonObject& json) {
    ConnectionPoint point;
    point.point_id = json["point_id"].toString();
    point.type = static_cast<ConnectionType>(json["type"].toInt());
    point.label = json["label"].toString();
    point.description = json["description"].toString();
    point.polarity = static_cast<Polarity>(json["polarity"].toInt());
    point.x_position = json["x_position"].toDouble();
    point.y_position = json["y_position"].toDouble();
    return point;
}

// WiringConnection JSON序列化实现
QJsonObject WiringConnection::toJson() const {
    QJsonObject obj;
    obj["connection_id"] = connection_id;
    obj["resource_port"] = resource_port.toJson();
    obj["target_point"] = target_point.toJson();
    obj["wire_color"] = wire_color;
    obj["priority"] = priority;
    obj["instruction"] = instruction;
    obj["is_completed"] = is_completed;
    return obj;
}

WiringConnection WiringConnection::fromJson(const QJsonObject& json) {
    WiringConnection connection;
    connection.connection_id = json["connection_id"].toString();
    connection.resource_port = ResourcePort::fromJson(json["resource_port"].toObject());
    connection.target_point = ConnectionPoint::fromJson(json["target_point"].toObject());
    connection.wire_color = json["wire_color"].toString();
    connection.priority = json["priority"].toInt();
    connection.instruction = json["instruction"].toString();
    connection.is_completed = json["is_completed"].toBool();
    return connection;
}

// WiringStep JSON序列化实现
QJsonObject WiringStep::toJson() const {
    QJsonObject obj;
    obj["step_number"] = step_number;
    obj["title"] = title;
    obj["description"] = description;
    obj["warning"] = warning;
    obj["image_path"] = image_path;
    obj["estimated_time_seconds"] = estimated_time_seconds;
    obj["is_critical"] = is_critical;
    
    QJsonArray connections_array;
    for (const auto& connection : connections) {
        connections_array.append(connection.toJson());
    }
    obj["connections"] = connections_array;
    
    return obj;
}

WiringStep WiringStep::fromJson(const QJsonObject& json) {
    WiringStep step;
    step.step_number = json["step_number"].toInt();
    step.title = json["title"].toString();
    step.description = json["description"].toString();
    step.warning = json["warning"].toString();
    step.image_path = json["image_path"].toString();
    step.estimated_time_seconds = json["estimated_time_seconds"].toInt();
    step.is_critical = json["is_critical"].toBool();
    
    QJsonArray connections_array = json["connections"].toArray();
    for (const auto& value : connections_array) {
        step.connections.append(WiringConnection::fromJson(value.toObject()));
    }
    
    return step;
}

// WiringConfiguration JSON序列化实现
QJsonObject WiringConfiguration::toJson() const {
    QJsonObject obj;
    obj["config_id"] = config_id;
    obj["component_reference"] = component_reference;
    obj["test_name"] = test_name;
    obj["total_voltage"] = total_voltage;
    obj["total_current"] = total_current;
    obj["created_time"] = created_time.toString(Qt::ISODate);
    obj["notes"] = notes;
    
    QJsonArray steps_array;
    for (const auto& step : steps) {
        steps_array.append(step.toJson());
    }
    obj["steps"] = steps_array;
    
    QJsonArray resources_array;
    for (const auto& resource : used_resources) {
        resources_array.append(resource.toJson());
    }
    obj["used_resources"] = resources_array;
    
    return obj;
}

WiringConfiguration WiringConfiguration::fromJson(const QJsonObject& json) {
    WiringConfiguration config;
    config.config_id = json["config_id"].toString();
    config.component_reference = json["component_reference"].toString();
    config.test_name = json["test_name"].toString();
    config.total_voltage = json["total_voltage"].toDouble();
    config.total_current = json["total_current"].toDouble();
    config.created_time = QDateTime::fromString(json["created_time"].toString(), Qt::ISODate);
    config.notes = json["notes"].toString();
    
    QJsonArray steps_array = json["steps"].toArray();
    for (const auto& value : steps_array) {
        config.steps.append(WiringStep::fromJson(value.toObject()));
    }
    
    QJsonArray resources_array = json["used_resources"].toArray();
    for (const auto& value : resources_array) {
        config.used_resources.append(ResourcePort::fromJson(value.toObject()));
    }
    
    return config;
}

//=============================================================================
// WiringResourceManager 实现
//=============================================================================

WiringResourceManager::WiringResourceManager(QObject *parent)
    : QObject(parent)
{
    resource_config_path_ = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) 
                           + "/FaultDetect/WiringGuide/";
    QDir().mkpath(resource_config_path_);
}

WiringResourceManager::~WiringResourceManager()
{
    saveResourceState(resource_config_path_ + "resource_state.json");
}

bool WiringResourceManager::initializeResources()
{
    try {
        setupDefaultResources();
        
        // 尝试加载之前保存的资源状态
        QString state_file = resource_config_path_ + "resource_state.json";
        if (QFile::exists(state_file)) {
            loadResourceState(state_file);
        }
        
        qDebug() << "资源管理器初始化完成，共" << resources_.size() << "个资源";
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "资源管理器初始化失败:" << e.what();
        emit errorOccurred(QString("资源初始化失败: %1").arg(e.what()));
        return false;
    }
}

void WiringResourceManager::setupDefaultResources()
{
    resources_.clear();
    allocated_resources_.clear();
    
    // 创建12个数字输出端口
    for (int i = 0; i < 12; ++i) {
        ResourcePort port = createResourcePort(
            i + 1000, ResourceType::DIGITAL_OUTPUT, 
            QString("DO_%1").arg(i), "JY5710", i, 5.0, 0.1);
        resources_[port.port_id] = port;
    }
    
    // 创建4个电源输出端口
    QStringList power_names = {"PWR_3V3", "PWR_5V", "PWR_12V", "PWR_VAR"};
    QList<double> power_voltages = {3.3, 5.0, 12.0, 30.0};
    QList<double> power_currents = {3.0, 5.0, 2.0, 10.0};
    
    for (int i = 0; i < 4; ++i) {
        ResourcePort port = createResourcePort(
            i + 2000, ResourceType::POWER_OUTPUT,
            power_names[i], "JY5710", i + 100, 
            power_voltages[i], power_currents[i]);
        resources_[port.port_id] = port;
    }
    
    // 创建16个模拟输出端口
    for (int i = 0; i < 16; ++i) {
        ResourcePort port = createResourcePort(
            i + 3000, ResourceType::ANALOG_OUTPUT,
            QString("AO_%1").arg(i), "JY5320", i, 10.0, 0.02);
        resources_[port.port_id] = port;
    }
    
    // 创建16个数字输入端口
    for (int i = 0; i < 16; ++i) {
        ResourcePort port = createResourcePort(
            i + 4000, ResourceType::DIGITAL_INPUT,
            QString("DI_%1").arg(i), "JY5320", i + 16, 5.0, 0.0);
        resources_[port.port_id] = port;
    }
    
    // 创建32个模拟输入端口
    for (int i = 0; i < 32; ++i) {
        ResourcePort port = createResourcePort(
            i + 5000, ResourceType::ANALOG_INPUT,
            QString("AI_%1").arg(i), "JY5320", i + 32, 10.0, 0.0);
        resources_[port.port_id] = port;
    }
    
    // 创建1个数字万用表
    ResourcePort dmm_port = createResourcePort(
        6000, ResourceType::DMM, "DMM_CH1", "JY8902", 0, 1000.0, 10.0);
    resources_[dmm_port.port_id] = dmm_port;
}

ResourcePort WiringResourceManager::createResourcePort(int id, ResourceType type, 
                                                      const QString& name, const QString& device, 
                                                      int channel, double max_v, double max_i)
{
    ResourcePort port;
    port.port_id = id;
    port.type = type;
    port.name = name;
    port.device_name = device;
    port.device_channel = channel;
    port.max_voltage = max_v;
    port.max_current = max_i;
    port.is_available = true;
    
    switch (type) {
    case ResourceType::DIGITAL_OUTPUT:
        port.description = QString("数字输出端口 %1，最大电压 %2V").arg(name).arg(max_v);
        break;
    case ResourceType::POWER_OUTPUT:
        port.description = QString("电源输出端口 %1，%2V/%3A").arg(name).arg(max_v).arg(max_i);
        break;
    case ResourceType::ANALOG_OUTPUT:
        port.description = QString("模拟输出端口 %1，±%2V").arg(name).arg(max_v);
        break;
    case ResourceType::DIGITAL_INPUT:
        port.description = QString("数字输入端口 %1").arg(name);
        break;
    case ResourceType::ANALOG_INPUT:
        port.description = QString("模拟输入端口 %1，±%2V").arg(name).arg(max_v);
        break;
    case ResourceType::DMM:
        port.description = QString("数字万用表通道 %1").arg(name);
        break;
    }
    
    return port;
}

QList<ResourcePort> WiringResourceManager::getAvailableResources(ResourceType type) const
{
    QList<ResourcePort> available;
    for (const auto& port : resources_.values()) {
        if (port.type == type && port.is_available) {
            available.append(port);
        }
    }
    return available;
}

QList<ResourcePort> WiringResourceManager::getAllResources() const
{
    return resources_.values();
}

bool WiringResourceManager::allocateResource(const ResourcePort& port)
{
    if (!resources_.contains(port.port_id)) {
        emit errorOccurred(QString("资源端口 %1 不存在").arg(port.port_id));
        return false;
    }
    
    if (!resources_[port.port_id].is_available) {
        emit errorOccurred(QString("资源端口 %1 已被占用").arg(port.name));
        return false;
    }
    
    resources_[port.port_id].is_available = false;
    allocated_resources_.append(port.port_id);
    
    emit resourceAllocated(port);
    qDebug() << "已分配资源:" << port.name;
    return true;
}

bool WiringResourceManager::releaseResource(int port_id)
{
    if (!resources_.contains(port_id)) {
        return false;
    }
    
    resources_[port_id].is_available = true;
    allocated_resources_.removeAll(port_id);
    
    emit resourceReleased(port_id);
    qDebug() << "已释放资源:" << resources_[port_id].name;
    return true;
}

void WiringResourceManager::releaseAllResources()
{
    for (int port_id : allocated_resources_) {
        resources_[port_id].is_available = true;
        emit resourceReleased(port_id);
    }
    allocated_resources_.clear();
    qDebug() << "已释放所有资源";
}

bool WiringResourceManager::isResourceAvailable(int port_id) const
{
    return resources_.contains(port_id) && resources_[port_id].is_available;
}

ResourcePort WiringResourceManager::getResourcePort(int port_id) const
{
    return resources_.value(port_id, ResourcePort());
}

QList<ResourcePort> WiringResourceManager::getResourcesByType(ResourceType type) const
{
    QList<ResourcePort> result;
    for (const auto& port : resources_.values()) {
        if (port.type == type) {
            result.append(port);
        }
    }
    return result;
}

int WiringResourceManager::getAvailableResourceCount(ResourceType type) const
{
    int count = 0;
    for (const auto& port : resources_.values()) {
        if (port.type == type && port.is_available) {
            count++;
        }
    }
    return count;
}

WiringConfiguration WiringResourceManager::generateWiringConfig(const QString& component_ref, 
                                                               double voltage, double current, 
                                                               bool use_dmm)
{
    WiringConfiguration config;
    config.config_id = QUuid::createUuid().toString();
    config.component_reference = component_ref;
    config.test_name = QString("%1 故障检测测试").arg(component_ref);
    config.total_voltage = voltage;
    config.total_current = current;
    config.created_time = QDateTime::currentDateTime();
    
    // 生成连接点
    QList<ConnectionPoint> connection_points = generateConnectionPoints(component_ref);
    
    // 为测试配置选择合适的资源
    QList<ResourcePort> required_resources;
    
    // 选择电源端口
    auto power_ports = getAvailableResources(ResourceType::POWER_OUTPUT);
    for (const auto& port : power_ports) {
        if (port.max_voltage >= voltage && port.max_current >= current) {
            required_resources.append(port);
            break;
        }
    }
    
    // 选择万用表（如果需要）
    if (use_dmm) {
        auto dmm_ports = getAvailableResources(ResourceType::DMM);
        if (!dmm_ports.isEmpty()) {
            required_resources.append(dmm_ports.first());
        }
    }
    
    // 根据元件类型选择额外的I/O端口
    // 这里可以根据具体的测试需求来选择
    
    config.used_resources = required_resources;
    
    emit configurationGenerated(config);
    return config;
}

QList<WiringStep> WiringResourceManager::generateWiringSteps(const WiringConfiguration& config)
{
    QList<WiringStep> steps;
    
    if (config.used_resources.isEmpty()) {
        return steps;
    }
    
    // 生成连接点
    QList<ConnectionPoint> connection_points = generateConnectionPoints(config.component_reference);
    
    int step_number = 1;
    
    // 步骤1: 安全检查
    WiringStep safety_step;
    safety_step.step_number = step_number++;
    safety_step.title = "安全检查";
    safety_step.description = "在开始接线前，请确保：\n"
                             "1. 所有设备已断电\n"
                             "2. 测试夹具已就位\n"
                             "3. PCB板已正确放置\n"
                             "4. 工作区域整洁无干扰";
    safety_step.warning = "接线前必须确保所有设备断电，避免短路或人身伤害";
    safety_step.estimated_time_seconds = 60;
    safety_step.is_critical = true;
    steps.append(safety_step);
    
    // 步骤2: 电源连接
    for (const auto& resource : config.used_resources) {
        if (resource.type == ResourceType::POWER_OUTPUT) {
            WiringStep power_step;
            power_step.step_number = step_number++;
            power_step.title = QString("连接电源 - %1").arg(resource.name);
            power_step.description = QString("将 %1 连接到测试点：\n"
                                           "输出电压: %2V\n"
                                           "最大电流: %3A")
                                   .arg(resource.name)
                                   .arg(config.total_voltage)
                                   .arg(config.total_current);
            
            // 生成电源连接
            WiringConnection power_positive, power_negative;
            
            // 正极连接
            power_positive.connection_id = QUuid::createUuid().toString();
            power_positive.resource_port = resource;
            if (!connection_points.isEmpty()) {
                power_positive.target_point = connection_points.first();
                power_positive.target_point.polarity = Polarity::POSITIVE;
                power_positive.target_point.label = QString("%1_正极").arg(config.component_reference);
            }
            power_positive.wire_color = getRecommendedWireColor(resource.type, Polarity::POSITIVE);
            power_positive.priority = 1;
            power_positive.instruction = QString("将 %1 正极连接到 %2")
                                       .arg(resource.name)
                                       .arg(power_positive.target_point.label);
            
            // 负极连接
            power_negative.connection_id = QUuid::createUuid().toString();
            power_negative.resource_port = resource;
            if (connection_points.size() > 1) {
                power_negative.target_point = connection_points[1];
                power_negative.target_point.polarity = Polarity::NEGATIVE;
                power_negative.target_point.label = QString("%1_负极").arg(config.component_reference);
            }
            power_negative.wire_color = getRecommendedWireColor(resource.type, Polarity::NEGATIVE);
            power_negative.priority = 2;
            power_negative.instruction = QString("将 %1 负极连接到 %2")
                                       .arg(resource.name)
                                       .arg(power_negative.target_point.label);
            
            power_step.connections.append(power_positive);
            power_step.connections.append(power_negative);
            power_step.estimated_time_seconds = 120;
            power_step.is_critical = true;
            
            steps.append(power_step);
            break;
        }
    }
    
    // 步骤3: 万用表连接
    for (const auto& resource : config.used_resources) {
        if (resource.type == ResourceType::DMM) {
            WiringStep dmm_step;
            dmm_step.step_number = step_number++;
            dmm_step.title = QString("连接万用表 - %1").arg(resource.name);
            dmm_step.description = "连接数字万用表进行测量：\n"
                                  "1. 将万用表探头连接到测试点\n"
                                  "2. 选择合适的测量量程\n"
                                  "3. 确保连接稳定可靠";
            
            // 生成万用表连接
            WiringConnection dmm_positive, dmm_negative;
            
            dmm_positive.connection_id = QUuid::createUuid().toString();
            dmm_positive.resource_port = resource;
            if (connection_points.size() > 2) {
                dmm_positive.target_point = connection_points[2];
                dmm_positive.target_point.polarity = Polarity::POSITIVE;
                dmm_positive.target_point.label = QString("%1_测量点+").arg(config.component_reference);
            }
            dmm_positive.wire_color = "红色";
            dmm_positive.priority = 3;
            dmm_positive.instruction = "将万用表红色探头连接到正测量点";
            
            dmm_negative.connection_id = QUuid::createUuid().toString();
            dmm_negative.resource_port = resource;
            if (connection_points.size() > 3) {
                dmm_negative.target_point = connection_points[3];
                dmm_negative.target_point.polarity = Polarity::NEGATIVE;
                dmm_negative.target_point.label = QString("%1_测量点-").arg(config.component_reference);
            }
            dmm_negative.wire_color = "黑色";
            dmm_negative.priority = 4;
            dmm_negative.instruction = "将万用表黑色探头连接到负测量点";
            
            dmm_step.connections.append(dmm_positive);
            dmm_step.connections.append(dmm_negative);
            dmm_step.estimated_time_seconds = 90;
            dmm_step.is_critical = false;
            
            steps.append(dmm_step);
            break;
        }
    }
    
    // 步骤4: 最终检查
    WiringStep final_check;
    final_check.step_number = step_number++;
    final_check.title = "最终检查";
    final_check.description = "接线完成后的最终检查：\n"
                             "1. 检查所有连接是否牢固\n"
                             "2. 确认极性正确\n"
                             "3. 检查是否有短路风险\n"
                             "4. 准备开始测试";
    final_check.warning = "开始测试前务必再次确认所有连接正确";
    final_check.estimated_time_seconds = 60;
    final_check.is_critical = true;
    
    steps.append(final_check);
    
    return steps;
}

QList<ConnectionPoint> WiringResourceManager::generateConnectionPoints(const QString& component_ref)
{
    QList<ConnectionPoint> points;
    
    // 根据元件参考标识生成标准连接点
    // 这里是简化实现，实际应用中可能需要根据PCB设计文件来生成
    
    ConnectionPoint point1;
    point1.point_id = component_ref + "_PIN1";
    point1.type = ConnectionType::COMPONENT_PIN;
    point1.label = component_ref + " 引脚1";
    point1.description = "元件正极或第一引脚";
    point1.polarity = Polarity::POSITIVE;
    point1.x_position = 0.0;
    point1.y_position = 0.0;
    points.append(point1);
    
    ConnectionPoint point2;
    point2.point_id = component_ref + "_PIN2";
    point2.type = ConnectionType::COMPONENT_PIN;
    point2.label = component_ref + " 引脚2";
    point2.description = "元件负极或第二引脚";
    point2.polarity = Polarity::NEGATIVE;
    point2.x_position = 10.0;
    point2.y_position = 0.0;
    points.append(point2);
    
    // 添加测试点
    ConnectionPoint test_point_pos;
    test_point_pos.point_id = component_ref + "_TEST_POS";
    test_point_pos.type = ConnectionType::TEST_POINT;
    test_point_pos.label = component_ref + " 测试点+";
    test_point_pos.description = "正极测试点";
    test_point_pos.polarity = Polarity::POSITIVE;
    test_point_pos.x_position = 5.0;
    test_point_pos.y_position = -5.0;
    points.append(test_point_pos);
    
    ConnectionPoint test_point_neg;
    test_point_neg.point_id = component_ref + "_TEST_NEG";
    test_point_neg.type = ConnectionType::TEST_POINT;
    test_point_neg.label = component_ref + " 测试点-";
    test_point_neg.description = "负极测试点";
    test_point_neg.polarity = Polarity::NEGATIVE;
    test_point_neg.x_position = 5.0;
    test_point_neg.y_position = 5.0;
    points.append(test_point_neg);
    
    return points;
}

QString WiringResourceManager::getRecommendedWireColor(ResourceType type, Polarity polarity)
{
    switch (type) {
    case ResourceType::POWER_OUTPUT:
        return (polarity == Polarity::POSITIVE) ? "红色" : "黑色";
    case ResourceType::DIGITAL_OUTPUT:
        return (polarity == Polarity::POSITIVE) ? "蓝色" : "白色";
    case ResourceType::ANALOG_OUTPUT:
        return (polarity == Polarity::POSITIVE) ? "绿色" : "黄色";
    case ResourceType::DMM:
        return (polarity == Polarity::POSITIVE) ? "红色" : "黑色";
    default:
        return "灰色";
    }
}

bool WiringResourceManager::validateWiringConfiguration(const WiringConfiguration& config, QStringList& errors)
{
    errors.clear();
    bool is_valid = true;
    
    // 检查配置完整性
    if (config.config_id.isEmpty()) {
        errors.append("配置ID为空");
        is_valid = false;
    }
    
    if (config.component_reference.isEmpty()) {
        errors.append("元件标识为空");
        is_valid = false;
    }
    
    if (config.used_resources.isEmpty()) {
        errors.append("未分配任何资源");
        is_valid = false;
    }
    
    // 检查资源分配
    QStringList resource_errors;
    if (!validateResourceAllocation(config.used_resources, resource_errors)) {
        errors.append(resource_errors);
        is_valid = false;
    }
    
    // 检查电压电流限制
    for (const auto& resource : config.used_resources) {
        if (resource.type == ResourceType::POWER_OUTPUT) {
            if (config.total_voltage > resource.max_voltage) {
                errors.append(QString("测试电压 %1V 超过端口 %2 最大电压 %3V")
                             .arg(config.total_voltage)
                             .arg(resource.name)
                             .arg(resource.max_voltage));
                is_valid = false;
            }
            
            if (config.total_current > resource.max_current) {
                errors.append(QString("测试电流 %1A 超过端口 %2 最大电流 %3A")
                             .arg(config.total_current)
                             .arg(resource.name)
                             .arg(resource.max_current));
                is_valid = false;
            }
        }
    }
    
    // 检查接线步骤
    if (config.steps.isEmpty()) {
        errors.append("未定义接线步骤");
        is_valid = false;
    }
    
    return is_valid;
}

bool WiringResourceManager::validateResourceAllocation(const QList<ResourcePort>& ports, QStringList& errors)
{
    errors.clear();
    bool is_valid = true;
    
    QSet<int> used_port_ids;
    
    for (const auto& port : ports) {
        // 检查端口是否存在
        if (!resources_.contains(port.port_id)) {
            errors.append(QString("端口 %1 不存在").arg(port.port_id));
            is_valid = false;
            continue;
        }
        
        // 检查端口是否重复分配
        if (used_port_ids.contains(port.port_id)) {
            errors.append(QString("端口 %1 重复分配").arg(port.name));
            is_valid = false;
            continue;
        }
        used_port_ids.insert(port.port_id);
        
        // 检查端口是否可用
        if (!resources_[port.port_id].is_available) {
            errors.append(QString("端口 %1 不可用").arg(port.name));
            is_valid = false;
        }
    }
    
    return is_valid;
}

bool WiringResourceManager::saveConfiguration(const WiringConfiguration& config, const QString& file_path)
{
    try {
        QJsonDocument doc(config.toJson());
        
        QFile file(file_path);
        if (!file.open(QIODevice::WriteOnly)) {
            emit errorOccurred(QString("无法打开文件: %1").arg(file_path));
            return false;
        }
        
        file.write(doc.toJson());
        file.close();
        
        qDebug() << "配置已保存至:" << file_path;
        return true;
        
    } catch (const std::exception& e) {
        emit errorOccurred(QString("保存配置失败: %1").arg(e.what()));
        return false;
    }
}

bool WiringResourceManager::loadConfiguration(const QString& file_path, WiringConfiguration& config)
{
    try {
        QFile file(file_path);
        if (!file.open(QIODevice::ReadOnly)) {
            emit errorOccurred(QString("无法打开文件: %1").arg(file_path));
            return false;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) {
            emit errorOccurred("JSON格式错误");
            return false;
        }
        
        config = WiringConfiguration::fromJson(doc.object());
        qDebug() << "配置已从文件加载:" << file_path;
        return true;
        
    } catch (const std::exception& e) {
        emit errorOccurred(QString("加载配置失败: %1").arg(e.what()));
        return false;
    }
}

bool WiringResourceManager::saveResourceState(const QString& file_path)
{
    try {
        QJsonObject state_obj;
        
        QJsonArray resources_array;
        for (const auto& resource : resources_.values()) {
            resources_array.append(resource.toJson());
        }
        state_obj["resources"] = resources_array;
        
        QJsonArray allocated_array;
        for (int port_id : allocated_resources_) {
            allocated_array.append(port_id);
        }
        state_obj["allocated_resources"] = allocated_array;
        
        QJsonDocument doc(state_obj);
        
        QFile file(file_path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        
        file.write(doc.toJson());
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "保存资源状态失败:" << e.what();
        return false;
    }
}

bool WiringResourceManager::loadResourceState(const QString& file_path)
{
    try {
        QFile file(file_path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) {
            return false;
        }
        
        QJsonObject state_obj = doc.object();
        
        // 加载资源状态
        QJsonArray resources_array = state_obj["resources"].toArray();
        for (const auto& value : resources_array) {
            ResourcePort port = ResourcePort::fromJson(value.toObject());
            resources_[port.port_id] = port;
        }
        
        // 加载已分配资源列表
        allocated_resources_.clear();
        QJsonArray allocated_array = state_obj["allocated_resources"].toArray();
        for (const auto& value : allocated_array) {
            allocated_resources_.append(value.toInt());
        }
        
        qDebug() << "资源状态已加载";
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "加载资源状态失败:" << e.what();
        return false;
    }
}
