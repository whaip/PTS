#include "wiringtaskgenerator.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QUuid>
#include <QDebug>
#include <QtMath>

// TestTask JSON序列化实现
QJsonObject TestTask::toJson() const {
    QJsonObject obj;
    obj["task_id"] = task_id;
    obj["component_reference"] = component_reference;
    obj["wiring_config_id"] = wiring_config_id;
    obj["created_time"] = created_time.toString(Qt::ISODate);
    obj["description"] = description;
    obj["is_executed"] = is_executed;
    obj["execution_result"] = execution_result;
    
    QJsonArray operations_array;
    for (const auto& operation : operations) {
        QJsonObject op_obj;
        op_obj["command"] = static_cast<int>(operation.command);
        op_obj["channel"] = operation.channel;
        op_obj["value"] = operation.value;
        op_obj["sample_rate"] = operation.sampleRate;
        op_obj["timeout"] = operation.timeout;
        op_obj["blocking"] = operation.blocking;
        op_obj["device_name"] = operation.deviceName;
        op_obj["sync_group"] = operation.syncGroup;
        op_obj["sync_delay"] = operation.syncDelay;
        operations_array.append(op_obj);
    }
    obj["operations"] = operations_array;
    
    return obj;
}

TestTask TestTask::fromJson(const QJsonObject& json) {
    TestTask task;
    task.task_id = json["task_id"].toString();
    task.component_reference = json["component_reference"].toString();
    task.wiring_config_id = json["wiring_config_id"].toString();
    task.created_time = QDateTime::fromString(json["created_time"].toString(), Qt::ISODate);
    task.description = json["description"].toString();
    task.is_executed = json["is_executed"].toBool();
    task.execution_result = json["execution_result"].toString();
    
    QJsonArray operations_array = json["operations"].toArray();
    for (const auto& op_value : operations_array) {
        QJsonObject op_obj = op_value.toObject();
        DeviceOperation operation;
        operation.command = static_cast<DeviceCommand>(op_obj["command"].toInt());
        operation.channel = op_obj["channel"].toInt();
        operation.value = op_obj["value"].toDouble();
        operation.sampleRate = op_obj["sample_rate"].toDouble();
        operation.timeout = op_obj["timeout"].toInt();
        operation.blocking = op_obj["blocking"].toBool();
        operation.deviceName = op_obj["device_name"].toString();
        operation.syncGroup = op_obj["sync_group"].toString();
        operation.syncDelay = op_obj["sync_delay"].toInt();
        task.operations.append(operation);
    }
    
    return task;
}

//=============================================================================
// WiringTaskGenerator 实现
//=============================================================================

WiringTaskGenerator::WiringTaskGenerator(QObject *parent)
    : QObject(parent)
{
    task_storage_path_ = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) 
                        + "/FaultDetect/TestTasks/";
    QDir().mkpath(task_storage_path_);
}

WiringTaskGenerator::~WiringTaskGenerator()
{
    // 保存所有未完成的任务状态
    for (auto it = active_tasks_.begin(); it != active_tasks_.end(); ++it) {
        if (!it.value().is_executed) {
            QString file_path = task_storage_path_ + it.key() + ".json";
            saveTestTask(it.value(), file_path);
        }
    }
}

TestTask WiringTaskGenerator::generateTestTask(const WiringConfiguration& wiringConfig, 
                                              const ComponentSpec& component)
{
    TestTask task;
    task.task_id = QUuid::createUuid().toString();
    task.component_reference = component.reference;
    task.wiring_config_id = wiringConfig.config_id;
    task.created_time = QDateTime::currentDateTime();
    task.description = QString("为元件 %1 生成的测试任务").arg(component.reference);
    task.is_executed = false;
    
    try {
        // 生成初始化操作序列
        QList<DeviceOperation> init_ops = createInitializationSequence(wiringConfig);
        task.operations.append(init_ops);
        
        // 根据元件类型生成特定的测试操作
        QList<DeviceOperation> test_ops;
        switch (component.type) {
        case ComponentType::RESISTOR:
            test_ops = generateResistorTestOperations(component, wiringConfig);
            break;
        case ComponentType::CAPACITOR:
            test_ops = generateCapacitorTestOperations(component, wiringConfig);
            break;
        case ComponentType::INDUCTOR:
            test_ops = generateInductorTestOperations(component, wiringConfig);
            break;
        case ComponentType::DIODE:
            test_ops = generateDiodeTestOperations(component, wiringConfig);
            break;
        default:
            qWarning() << "不支持的元件类型:" << static_cast<int>(component.type);
            break;
        }
        task.operations.append(test_ops);
        
        // 生成测量操作序列
        QList<DeviceOperation> measure_ops = createMeasurementSequence(component, wiringConfig);
        task.operations.append(measure_ops);
        
        // 生成关闭操作序列
        QList<DeviceOperation> shutdown_ops = createShutdownSequence(wiringConfig);
        task.operations.append(shutdown_ops);
        
        // 将任务加入活动任务列表
        active_tasks_[task.task_id] = task;
        task_execution_status_[task.task_id] = "已生成";
        
        emit taskGenerated(task);
        qDebug() << "测试任务生成成功:" << task.task_id;
        
    } catch (const std::exception& e) {
        task.execution_result = QString("任务生成失败: %1").arg(e.what());
        emit errorOccurred(task.execution_result);
    }
    
    return task;
}

bool WiringTaskGenerator::executeTestTask(const TestTask& task, DeviceManager* deviceManager)
{
    if (!deviceManager) {
        emit errorOccurred("设备管理器为空");
        return false;
    }
    
    if (!deviceManager->isSystemReady()) {
        emit errorOccurred("测试系统未就绪");
        return false;
    }
    
    task_execution_status_[task.task_id] = "执行中";
    emit taskExecutionStarted(task.task_id);
    
    try {
        // 逐个执行设备操作
        for (const auto& operation : task.operations) {
            if (!deviceManager->submitOperation(operation.deviceName, operation)) {
                QString error = QString("设备操作失败: %1").arg(operation.deviceName);
                emit errorOccurred(error);
                task_execution_status_[task.task_id] = "执行失败";
                return false;
            }
            
            // 如果是阻塞操作，等待结果
            if (operation.blocking) {
                DeviceResult result = deviceManager->waitForResult(operation.deviceName, operation.timeout);
                if (!result.success) {
                    QString error = QString("设备操作执行失败: %1 - %2")
                                   .arg(operation.deviceName).arg(result.error);
                    emit errorOccurred(error);
                    task_execution_status_[task.task_id] = "执行失败";
                    return false;
                }
            }
        }
        
        // 标记任务为已执行
        if (active_tasks_.contains(task.task_id)) {
            active_tasks_[task.task_id].is_executed = true;
            active_tasks_[task.task_id].execution_result = "执行成功";
        }
        
        task_execution_status_[task.task_id] = "执行完成";
        emit taskExecutionCompleted(task.task_id, true);
        
        qDebug() << "测试任务执行成功:" << task.task_id;
        return true;
        
    } catch (const std::exception& e) {
        QString error = QString("任务执行异常: %1").arg(e.what());
        emit errorOccurred(error);
        task_execution_status_[task.task_id] = "执行异常";
        emit taskExecutionCompleted(task.task_id, false);
        return false;
    }
}

bool WiringTaskGenerator::validateTestTask(const TestTask& task, QStringList& errors)
{
    errors.clear();
    bool is_valid = true;
    
    // 检查任务基本信息
    if (task.task_id.isEmpty()) {
        errors.append("任务ID为空");
        is_valid = false;
    }
    
    if (task.component_reference.isEmpty()) {
        errors.append("元件标识为空");
        is_valid = false;
    }
    
    if (task.operations.isEmpty()) {
        errors.append("操作序列为空");
        is_valid = false;
    }
    
    // 检查操作序列的有效性
    QSet<QString> used_devices;
    for (const auto& operation : task.operations) {
        if (operation.deviceName.isEmpty()) {
            errors.append("设备名称为空");
            is_valid = false;
        }
        
        used_devices.insert(operation.deviceName);
        
        // 检查操作参数的合理性
        switch (operation.command) {
        case DeviceCommand::CONFIGURE_CHANNEL:
            if (operation.channel < 0) {
                errors.append(QString("无效的通道号: %1").arg(operation.channel));
                is_valid = false;
            }
            break;
        case DeviceCommand::START_MEASUREMENT:
        case DeviceCommand::STOP_MEASUREMENT:
            if (operation.sampleRate <= 0) {
                errors.append("采样率必须大于0");
                is_valid = false;
            }
            break;
        case DeviceCommand::WRITE_DATA:
            // 检查输出值的范围
            break;
        default:
            break;
        }
        
        if (operation.timeout <= 0) {
            errors.append("超时时间必须大于0");
            is_valid = false;
        }
    }
    
    return is_valid;
}

QList<DeviceOperation> WiringTaskGenerator::generateResistorTestOperations(const ComponentSpec& component, 
                                                                          const WiringConfiguration& config)
{
    QList<DeviceOperation> operations;
    
    // 找到电源端口
    ResourcePort power_port;
    bool found_power = false;
    for (const auto& resource : config.used_resources) {
        if (resource.type == ResourceType::POWER_OUTPUT) {
            power_port = resource;
            found_power = true;
            break;
        }
    }
    
    if (found_power) {
        // 配置电源输出
        DeviceOperation power_config = createPowerOutputOperation(power_port, 
                                                                 component.test_voltage, 
                                                                 component.test_current);
        operations.append(power_config);
        
        // 启动电源输出
        DeviceOperation power_start;
        power_start.command = DeviceCommand::START_MEASUREMENT;
        power_start.deviceName = power_port.device_name;
        power_start.channel = power_port.device_channel;
        power_start.timeout = 2000;
        power_start.blocking = true;
        operations.append(power_start);
        
        // 等待稳定
        DeviceOperation wait_stable;
        wait_stable.command = DeviceCommand::READ_DATA;
        wait_stable.deviceName = "SYSTEM";
        wait_stable.value = 100; // 等待100ms
        wait_stable.timeout = 1000;
        wait_stable.blocking = true;
        operations.append(wait_stable);
    }
    
    return operations;
}

QList<DeviceOperation> WiringTaskGenerator::generateCapacitorTestOperations(const ComponentSpec& component, 
                                                                           const WiringConfiguration& config)
{
    QList<DeviceOperation> operations;
    
    // 电容测试需要特殊的充放电序列
    // 这里实现简化的测试流程
    
    return operations;
}

QList<DeviceOperation> WiringTaskGenerator::generateInductorTestOperations(const ComponentSpec& component, 
                                                                          const WiringConfiguration& config)
{
    QList<DeviceOperation> operations;
    
    // 电感测试通常需要交流信号
    // 这里实现简化的测试流程
    
    return operations;
}

QList<DeviceOperation> WiringTaskGenerator::generateDiodeTestOperations(const ComponentSpec& component, 
                                                                       const WiringConfiguration& config)
{
    QList<DeviceOperation> operations;
    
    // 二极管测试需要正向和反向偏置
    // 这里实现简化的测试流程
    
    return operations;
}

DeviceOperation WiringTaskGenerator::createPowerOutputOperation(const ResourcePort& powerPort, 
                                                               double voltage, double current)
{
    DeviceOperation operation;
    operation.command = DeviceCommand::CONFIGURE_CHANNEL;
    operation.deviceName = powerPort.device_name;
    operation.channel = powerPort.device_channel;
    operation.value = voltage;
    operation.timeout = 5000;
    operation.blocking = true;
    
    // 在参数中传递电流限制
    operation.parameters["current_limit"] = current;
    operation.parameters["voltage_output"] = voltage;
    
    return operation;
}

DeviceOperation WiringTaskGenerator::createDMMOperation(const ResourcePort& dmmPort, 
                                                       const QString& measurement_type)
{
    DeviceOperation operation;
    operation.command = DeviceCommand::START_MEASUREMENT;
    operation.deviceName = dmmPort.device_name;
    operation.channel = dmmPort.device_channel;
    operation.timeout = 3000;
    operation.blocking = true;
    
    operation.parameters["measurement_type"] = measurement_type;
    
    return operation;
}

DeviceOperation WiringTaskGenerator::createDigitalIOOperation(const ResourcePort& ioPort, 
                                                             bool output_value)
{
    DeviceOperation operation;
    operation.command = DeviceCommand::WRITE_DATA;
    operation.deviceName = ioPort.device_name;
    operation.channel = ioPort.device_channel;
    operation.value = output_value ? 1.0 : 0.0;
    operation.timeout = 1000;
    operation.blocking = false;
    
    return operation;
}

DeviceOperation WiringTaskGenerator::createAnalogIOOperation(const ResourcePort& ioPort, 
                                                            double value)
{
    DeviceOperation operation;
    operation.command = DeviceCommand::WRITE_DATA;
    operation.deviceName = ioPort.device_name;
    operation.channel = ioPort.device_channel;
    operation.value = value;
    operation.timeout = 1000;
    operation.blocking = false;
    
    return operation;
}

QList<DeviceOperation> WiringTaskGenerator::createInitializationSequence(const WiringConfiguration& config)
{
    QList<DeviceOperation> operations;
    
    // 为每个使用的资源创建初始化操作
    for (const auto& resource : config.used_resources) {
        DeviceOperation init_op;
        init_op.command = DeviceCommand::INITIALIZE;
        init_op.deviceName = resource.device_name;
        init_op.channel = resource.device_channel;
        init_op.timeout = 3000;
        init_op.blocking = true;
        operations.append(init_op);
    }
    
    return operations;
}

QList<DeviceOperation> WiringTaskGenerator::createMeasurementSequence(const ComponentSpec& component, 
                                                                     const WiringConfiguration& config)
{
    QList<DeviceOperation> operations;
    
    // 找到万用表端口
    for (const auto& resource : config.used_resources) {
        if (resource.type == ResourceType::DMM) {
            // 根据元件类型选择测量模式
            QString measurement_type;
            switch (component.type) {
            case ComponentType::RESISTOR:
                measurement_type = "RESISTANCE";
                break;
            case ComponentType::CAPACITOR:
                measurement_type = "CAPACITANCE";
                break;
            case ComponentType::INDUCTOR:
                measurement_type = "INDUCTANCE";
                break;
            case ComponentType::DIODE:
                measurement_type = "DIODE_TEST";
                break;
            default:
                measurement_type = "DC_VOLTAGE";
                break;
            }
            
            DeviceOperation measure_op = createDMMOperation(resource, measurement_type);
            operations.append(measure_op);
            break;
        }
    }
    
    return operations;
}

QList<DeviceOperation> WiringTaskGenerator::createShutdownSequence(const WiringConfiguration& config)
{
    QList<DeviceOperation> operations;
    
    // 关闭所有输出
    for (const auto& resource : config.used_resources) {
        if (resource.type == ResourceType::POWER_OUTPUT || 
            resource.type == ResourceType::ANALOG_OUTPUT ||
            resource.type == ResourceType::DIGITAL_OUTPUT) {
            
            DeviceOperation shutdown_op;
            shutdown_op.command = DeviceCommand::STOP_MEASUREMENT;
            shutdown_op.deviceName = resource.device_name;
            shutdown_op.channel = resource.device_channel;
            shutdown_op.value = 0.0;
            shutdown_op.timeout = 2000;
            shutdown_op.blocking = true;
            operations.append(shutdown_op);
        }
    }
    
    return operations;
}

bool WiringTaskGenerator::saveTestTask(const TestTask& task, const QString& filePath)
{
    try {
        QJsonDocument doc(task.toJson());
        
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit errorOccurred(QString("无法保存任务文件: %1").arg(filePath));
            return false;
        }
        
        file.write(doc.toJson());
        file.close();
        
        qDebug() << "测试任务已保存:" << filePath;
        return true;
        
    } catch (const std::exception& e) {
        emit errorOccurred(QString("保存任务失败: %1").arg(e.what()));
        return false;
    }
}

bool WiringTaskGenerator::loadTestTask(const QString& filePath, TestTask& task)
{
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit errorOccurred(QString("无法打开任务文件: %1").arg(filePath));
            return false;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) {
            emit errorOccurred("任务文件格式错误");
            return false;
        }
        
        task = TestTask::fromJson(doc.object());
        active_tasks_[task.task_id] = task;
        
        qDebug() << "测试任务已加载:" << filePath;
        return true;
        
    } catch (const std::exception& e) {
        emit errorOccurred(QString("加载任务失败: %1").arg(e.what()));
        return false;
    }
}

QString WiringTaskGenerator::getTaskExecutionStatus(const QString& taskId) const
{
    return task_execution_status_.value(taskId, "未知状态");
}

void WiringTaskGenerator::onDeviceOperationCompleted(const QString& deviceName, const DeviceResult& result)
{
    // 处理设备操作完成的回调
    qDebug() << "设备操作完成:" << deviceName << "结果:" << result.success;
}
