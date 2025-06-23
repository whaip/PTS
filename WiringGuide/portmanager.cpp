#include "portmanager.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QMutexLocker>

PortManager::PortManager(DeviceManager* deviceManager, QObject *parent)
    : QObject(parent)
    , deviceManager_(deviceManager)
    , statusUpdateTimer_(new QTimer(this))
{
    // 连接设备管理器信号
    if (deviceManager_) {
        connect(deviceManager_, &DeviceManager::deviceStatusChanged,
                this, &PortManager::onDeviceStatusChanged);
    }
    
    // 状态更新定时器
    connect(statusUpdateTimer_, &QTimer::timeout, this, &PortManager::updateDeviceStatus);
    statusUpdateTimer_->start(5000); // 每5秒更新一次状态
}

PortManager::~PortManager()
{
    qDebug() << "PortManager: 开始析构...";
    
    // 停止定时器
    if (statusUpdateTimer_) {
        statusUpdateTimer_->stop();
    }
    
    // 先断开所有信号连接，避免析构过程中触发信号
    if (deviceManager_) {
        disconnect(deviceManager_, nullptr, this, nullptr);
    }
    disconnect(this, nullptr, nullptr, nullptr);
    
    // 静默释放所有端口（不发送信号）
    {
        QMutexLocker locker(&portMutex_);
        for (auto it = devicePorts_.begin(); it != devicePorts_.end(); ++it) {
            for (PortInfo& port : it.value()) {
                if (!port.isAvailable) {
                    port.isAvailable = true;
                    port.allocatedTo.clear();
                    // 不发送信号，避免析构过程中的问题
                }
            }
        }
        qDebug() << "PortManager: 所有端口已静默释放";
    }
    
    qDebug() << "PortManager: 析构完成";
}

bool PortManager::initializePorts()
{
    QMutexLocker locker(&portMutex_);
    
    qDebug() << "初始化端口系统...";
    
    // 清空现有端口配置
    devicePorts_.clear();
      // 初始化各设备端口
    initializeJY5711Ports();
    initializeJY5323Ports();
    initializeJY5322Ports();
    
    // 清理任何无效的端口分配（使用内部版本，因为已经持有锁）
    cleanupInvalidAllocationsInternal();
    
    qDebug() << "端口系统初始化完成";
    return true;
}

void PortManager::initializeJY5711Ports()
{
    const QString deviceName = "JY5711";
    QVector<PortInfo> ports;
    
    qDebug() << "初始化" << deviceName << "端口...";
    
    // 模拟输出端口 (0-15)
    for (int i = JY5711Ports::ANALOG_OUTPUT_START; i <= JY5711Ports::ANALOG_OUTPUT_END; ++i) {
        PortInfo port(deviceName, i, PortType::ANALOG_OUTPUT,
                     QString("模拟输出端口 AO%1").arg(i), 10.0, 0.02);
        ports.append(port);
    }
    
    // 数字输出端口 (16-27)
    for (int i = JY5711Ports::DIGITAL_OUTPUT_START; i <= JY5711Ports::DIGITAL_OUTPUT_END; ++i) {
        PortInfo port(deviceName, i, PortType::DIGITAL_OUTPUT,
                     QString("数字输出端口 DO%1").arg(i), 5.0, 0.01);
        ports.append(port);
    }
    
    // 电源输出端口 (28-31)
    for (int i = JY5711Ports::POWER_OUTPUT_START; i <= JY5711Ports::POWER_OUTPUT_END; ++i) {
        PortInfo port(deviceName, i, PortType::POWER_OUTPUT,
                     QString("电源输出端口 PWR%1").arg(i), 12.0, 1.0);
        ports.append(port);
    }
    
    devicePorts_[deviceName] = ports;
    qDebug() << deviceName << "端口初始化完成，共" << ports.size() << "个端口";
}

void PortManager::initializeJY5323Ports()
{
    const QString deviceName = "JY5323";
    QVector<PortInfo> ports;
    
    // 模拟输入端口 (0-31)
    for (int i = JY5323Ports::ANALOG_INPUT_START; i <= JY5323Ports::ANALOG_INPUT_END; ++i) {
        PortInfo port(deviceName, i, PortType::ANALOG_INPUT,
                     QString("模拟输入端口 AI%1").arg(i), 10.0, 0.0);
        ports.append(port);
    }
    
    devicePorts_[deviceName] = ports;
    qDebug() << "JY5323 端口初始化完成:" << ports.size() << "个端口";
}

void PortManager::initializeJY5322Ports()
{
    const QString deviceName = "JY5322";
    QVector<PortInfo> ports;
    
    // 数字输入端口 (0-15)
    for (int i = JY5322Ports::DIGITAL_INPUT_START; i <= JY5322Ports::DIGITAL_INPUT_END; ++i) {
        PortInfo port(deviceName, i, PortType::DIGITAL_INPUT,
                     QString("数字输入端口 DI%1").arg(i), 5.0, 0.0);
        ports.append(port);
    }
    
    devicePorts_[deviceName] = ports;
    qDebug() << "JY5322 端口初始化完成:" << ports.size() << "个端口";
}

QVector<PortInfo> PortManager::getAvailablePorts(PortType type) const
{
    QMutexLocker locker(&portMutex_);
    QVector<PortInfo> availablePorts;
    
    for (auto it = devicePorts_.begin(); it != devicePorts_.end(); ++it) {
        for (const PortInfo& port : it.value()) {
            if (port.portType == type && port.isAvailable) {
                availablePorts.append(port);
            }
        }
    }
    
    return availablePorts;
}

QVector<PortInfo> PortManager::getAllPorts() const
{
    QMutexLocker locker(&portMutex_);
    QVector<PortInfo> allPorts;
    
    for (auto it = devicePorts_.begin(); it != devicePorts_.end(); ++it) {
        allPorts.append(it.value());
    }
    
    return allPorts;
}

PortInfo PortManager::getPortInfo(const QString& deviceName, int portNumber) const
{
    QMutexLocker locker(&portMutex_);
    const PortInfo* port = findPort(deviceName, portNumber);
    return port ? *port : PortInfo();
}

bool PortManager::allocatePort(const QString& deviceName, int portNumber, const QString& allocatedTo)
{
    QMutexLocker locker(&portMutex_);
    
    // 添加详细的调试信息
    qDebug() << "尝试分配端口:" << deviceName << portNumber << "给用户:" << allocatedTo;
    qDebug() << "用户字符串长度:" << allocatedTo.length() << "是否为空:" << allocatedTo.isEmpty();
    
    PortInfo* port = findPort(deviceName, portNumber);
    
    if (!port) {
        qWarning() << "端口不存在:" << deviceName << portNumber;
        return false;
    }
    
    qDebug() << "端口状态 - 可用:" << port->isAvailable << "当前分配给:" << port->allocatedTo;

    if (!port->isAvailable) {
        qWarning() << "端口已被分配:" << deviceName << portNumber << "分配给:" << port->allocatedTo;
        return false;
    }
    
    // 检查allocatedTo是否为空或无效
    if (allocatedTo.isEmpty()) {
        qWarning() << "警告：尝试将端口分配给空用户名！";
        // 可以选择拒绝分配或使用默认名称
        // return false; // 如果要拒绝空用户名分配
    }
    
    port->isAvailable = false;
    port->allocatedTo = allocatedTo;
    
    qDebug() << "端口分配成功:" << deviceName << portNumber << "分配给:" << allocatedTo;
    emit portAllocated(deviceName, portNumber, allocatedTo);
    
    return true;
}

bool PortManager::releasePort(const QString& deviceName, int portNumber)
{
    QMutexLocker locker(&portMutex_);
    PortInfo* port = findPort(deviceName, portNumber);
    
    if (!port) {
        qWarning() << "端口不存在:" << deviceName << portNumber;
        return false;
    }
    
    if (port->isAvailable) {
        qDebug() << "端口已经可用:" << deviceName << portNumber;
        return true;
    }
    
    QString previousAllocatedTo = port->allocatedTo;
    port->isAvailable = true;
    port->allocatedTo.clear();
    
    qDebug() << "端口释放成功:" << deviceName << portNumber << "之前分配给:" << previousAllocatedTo;
    emit portReleased(deviceName, portNumber);
    
    return true;
}

void PortManager::releaseAllPorts()
{
    QMutexLocker locker(&portMutex_);
    
    for (auto it = devicePorts_.begin(); it != devicePorts_.end(); ++it) {
        for (PortInfo& port : it.value()) {
            if (!port.isAvailable) {
                port.isAvailable = true;
                port.allocatedTo.clear();
                emit portReleased(port.deviceName, port.portNumber);
            }
        }
    }
    
    qDebug() << "所有端口已释放";
}

void PortManager::releasePortsForUser(const QString& allocatedTo)
{
    releasePortsForUser(allocatedTo, true);  // 默认发送信号
}

void PortManager::releasePortsForUser(const QString& allocatedTo, bool emitSignals)
{
    QMutexLocker locker(&portMutex_);
    
    for (auto it = devicePorts_.begin(); it != devicePorts_.end(); ++it) {
        for (PortInfo& port : it.value()) {
            if (!port.isAvailable && port.allocatedTo == allocatedTo) {
                port.isAvailable = true;
                port.allocatedTo.clear();
                if (emitSignals) {
                    emit portReleased(port.deviceName, port.portNumber);
                }
            }
        }
    }
    
    qDebug() << "释放用户端口:" << allocatedTo;
}

void PortManager::cleanupInvalidAllocations()
{
    QMutexLocker locker(&portMutex_);
    cleanupInvalidAllocationsInternal();
}

void PortManager::cleanupInvalidAllocationsInternal()
{
    // 注意：此函数假设调用者已经持有portMutex_锁
    int cleanedCount = 0;
    for (auto it = devicePorts_.begin(); it != devicePorts_.end(); ++it) {
        for (PortInfo& port : it.value()) {
            // 清理分配给空字符串或无效用户的端口
            if (!port.isAvailable && (port.allocatedTo.isEmpty() || port.allocatedTo.trimmed().isEmpty())) {
                qDebug() << "清理无效分配的端口:" << port.deviceName << port.portNumber 
                         << "之前分配给:" << port.allocatedTo;
                port.isAvailable = true;
                port.allocatedTo.clear();
                cleanedCount++;
            }
        }
    }
    
    if (cleanedCount > 0) {
        qDebug() << "已清理" << cleanedCount << "个无效分配的端口";
    }
}

QVector<PortInfo> PortManager::getRecommendedPorts(ComponentType componentType) const
{
    switch (componentType) {
    case ComponentType::RESISTOR:
        return getResistorPorts();
    case ComponentType::CAPACITOR:
        return getCapacitorPorts();
    case ComponentType::INDUCTOR:
        return getInductorPorts();
    case ComponentType::DIODE:
        return getDiodePorts();
    case ComponentType::IC:
        return getICPorts();
    default:
        return getResistorPorts(); // 默认使用电阻配置
    }
}

QVector<PortInfo> PortManager::getResistorPorts() const
{
    QVector<PortInfo> ports;
    QVector<PortInfo> availableAO = getAvailablePorts(PortType::ANALOG_OUTPUT);
    QVector<PortInfo> availableAI = getAvailablePorts(PortType::ANALOG_INPUT);
    QVector<PortInfo> availablePWR = getAvailablePorts(PortType::POWER_OUTPUT);
    
    // 电阻测试需要：1个模拟输出，2个模拟输入，1个电源
    if (!availableAO.isEmpty()) ports.append(availableAO.first());
    if (availableAI.size() >= 2) {
        ports.append(availableAI[0]);
        ports.append(availableAI[1]);
    }
    if (!availablePWR.isEmpty()) ports.append(availablePWR.first());
    
    return ports;
}

QVector<PortInfo> PortManager::getCapacitorPorts() const
{
    QVector<PortInfo> ports;
    QVector<PortInfo> availableAO = getAvailablePorts(PortType::ANALOG_OUTPUT);
    QVector<PortInfo> availableAI = getAvailablePorts(PortType::ANALOG_INPUT);
    
    // 电容测试需要：1个模拟输出，2个模拟输入
    if (!availableAO.isEmpty()) ports.append(availableAO.first());
    if (availableAI.size() >= 2) {
        ports.append(availableAI[0]);
        ports.append(availableAI[1]);
    }
    
    return ports;
}

QVector<PortInfo> PortManager::getInductorPorts() const
{
    QVector<PortInfo> ports;
    QVector<PortInfo> availableAO = getAvailablePorts(PortType::ANALOG_OUTPUT);
    QVector<PortInfo> availableAI = getAvailablePorts(PortType::ANALOG_INPUT);
    
    // 电感测试需要：1个模拟输出，2个模拟输入
    if (!availableAO.isEmpty()) ports.append(availableAO.first());
    if (availableAI.size() >= 2) {
        ports.append(availableAI[0]);
        ports.append(availableAI[1]);
    }
    
    return ports;
}

QVector<PortInfo> PortManager::getDiodePorts() const
{
    QVector<PortInfo> ports;
    QVector<PortInfo> availableAO = getAvailablePorts(PortType::ANALOG_OUTPUT);
    QVector<PortInfo> availableAI = getAvailablePorts(PortType::ANALOG_INPUT);
    QVector<PortInfo> availablePWR = getAvailablePorts(PortType::POWER_OUTPUT);
    
    // 二极管测试需要：1个模拟输出，2个模拟输入，1个电源
    if (!availableAO.isEmpty()) ports.append(availableAO.first());
    if (availableAI.size() >= 2) {
        ports.append(availableAI[0]);
        ports.append(availableAI[1]);
    }
    if (!availablePWR.isEmpty()) ports.append(availablePWR.first());
    
    return ports;
}

QVector<PortInfo> PortManager::getICPorts() const
{
    QVector<PortInfo> ports;
    QVector<PortInfo> availableAO = getAvailablePorts(PortType::ANALOG_OUTPUT);
    QVector<PortInfo> availableAI = getAvailablePorts(PortType::ANALOG_INPUT);
    QVector<PortInfo> availableDO = getAvailablePorts(PortType::DIGITAL_OUTPUT);
    QVector<PortInfo> availableDI = getAvailablePorts(PortType::DIGITAL_INPUT);
    QVector<PortInfo> availablePWR = getAvailablePorts(PortType::POWER_OUTPUT);
    
    // IC测试需要更多端口
    if (availableAO.size() >= 2) {
        ports.append(availableAO[0]);
        ports.append(availableAO[1]);
    }
    if (availableAI.size() >= 4) {
        for (int i = 0; i < 4; ++i) {
            ports.append(availableAI[i]);
        }
    }
    if (availableDO.size() >= 2) {
        ports.append(availableDO[0]);
        ports.append(availableDO[1]);
    }
    if (availableDI.size() >= 2) {
        ports.append(availableDI[0]);
        ports.append(availableDI[1]);
    }
    if (!availablePWR.isEmpty()) ports.append(availablePWR.first());
    
    return ports;
}

bool PortManager::validatePortConfiguration(const QVector<ConnectionInfo>& connections, QStringList& errors) const
{
    errors.clear();
    bool isValid = true;
    
    QSet<QString> usedPorts;
    
    for (const ConnectionInfo& connection : connections) {
        const PortInfo& source = connection.sourcePort;
        const PortInfo& target = connection.targetPort;
        
        // 检查源端口
        QString sourceKey = generatePortKey(source.deviceName, source.portNumber);
        if (usedPorts.contains(sourceKey)) {
            errors.append(QString("端口 %1:%2 重复使用").arg(source.deviceName).arg(source.portNumber));
            isValid = false;
        } else {
            usedPorts.insert(sourceKey);
        }
        
        // 检查目标端口
        QString targetKey = generatePortKey(target.deviceName, target.portNumber);
        if (usedPorts.contains(targetKey)) {
            errors.append(QString("端口 %1:%2 重复使用").arg(target.deviceName).arg(target.portNumber));
            isValid = false;
        } else {
            usedPorts.insert(targetKey);
        }
        
        // 检查端口是否可用
        if (!isPortAvailable(source.deviceName, source.portNumber)) {
            errors.append(QString("源端口 %1:%2 不可用").arg(source.deviceName).arg(source.portNumber));
            isValid = false;
        }
        
        if (!isPortAvailable(target.deviceName, target.portNumber)) {
            errors.append(QString("目标端口 %1:%2 不可用").arg(target.deviceName).arg(target.portNumber));
            isValid = false;
        }
    }
    
    return isValid;
}

void PortManager::updateDeviceStatus()
{
    // 检查设备状态并更新端口可用性
    if (!deviceManager_) return;
    
    QMutexLocker locker(&portMutex_);
    
    for (auto it = devicePorts_.begin(); it != devicePorts_.end(); ++it) {
        const QString& deviceName = it.key();
        // 这里可以根据设备状态更新端口可用性
        // 暂时保持现有状态
    }
}

void PortManager::onDeviceStatusChanged(const QString& device, DeviceStatus status)
{
    QMutexLocker locker(&portMutex_);
    
    if (devicePorts_.contains(device)) {
        // 根据设备状态更新对应端口的可用性
        bool deviceAvailable = (status == DeviceStatus::CONNECTED);
        
        for (PortInfo& port : devicePorts_[device]) {
            // 如果设备断开连接，将端口标记为不可用
            // 如果设备重新连接，恢复端口可用性（除非已被分配）
            if (!deviceAvailable) {
                if (port.isAvailable) {
                    emit portStatusChanged(device, port.portNumber, false);
                }
            } else {
                if (port.isAvailable) {
                    emit portStatusChanged(device, port.portNumber, true);
                }
            }
        }
        
        qDebug() << "设备" << device << "状态变更，端口状态已更新";
    }
}

QString PortManager::generatePortKey(const QString& deviceName, int portNumber) const
{
    return QString("%1:%2").arg(deviceName).arg(portNumber);
}

PortInfo* PortManager::findPort(const QString& deviceName, int portNumber)
{
    if (!devicePorts_.contains(deviceName)) {
        return nullptr;
    }
    
    QVector<PortInfo>& ports = devicePorts_[deviceName];
    for (PortInfo& port : ports) {
        if (port.portNumber == portNumber) {
            return &port;
        }
    }
    
    return nullptr;
}

const PortInfo* PortManager::findPort(const QString& deviceName, int portNumber) const
{
    if (!devicePorts_.contains(deviceName)) {
        return nullptr;
    }
    
    const QVector<PortInfo>& ports = devicePorts_[deviceName];
    for (const PortInfo& port : ports) {
        if (port.portNumber == portNumber) {
            return &port;
        }
    }
    
    return nullptr;
}

bool PortManager::savePortConfiguration(const QString& filePath) const
{
    QJsonObject rootObj;
    QJsonArray devicesArray;
    
    QMutexLocker locker(&portMutex_);
    
    for (auto it = devicePorts_.begin(); it != devicePorts_.end(); ++it) {
        QJsonObject deviceObj;
        deviceObj["deviceName"] = it.key();
        
        QJsonArray portsArray;
        for (const PortInfo& port : it.value()) {
            QJsonObject portObj;
            portObj["portNumber"] = port.portNumber;
            portObj["portType"] = static_cast<int>(port.portType);
            portObj["description"] = port.description;
            portObj["maxVoltage"] = port.maxVoltage;
            portObj["maxCurrent"] = port.maxCurrent;
            portObj["isAvailable"] = port.isAvailable;
            portObj["allocatedTo"] = port.allocatedTo;
            portsArray.append(portObj);
        }
        
        deviceObj["ports"] = portsArray;
        devicesArray.append(deviceObj);
    }
    
    rootObj["devices"] = devicesArray;
    rootObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(rootObj);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        return true;
    }
    
    return false;
}

bool PortManager::loadPortConfiguration(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject rootObj = doc.object();
    QJsonArray devicesArray = rootObj["devices"].toArray();
    
    QMutexLocker locker(&portMutex_);
    devicePorts_.clear();
    
    for (const QJsonValue& deviceValue : devicesArray) {
        QJsonObject deviceObj = deviceValue.toObject();
        QString deviceName = deviceObj["deviceName"].toString();
        
        QVector<PortInfo> ports;
        QJsonArray portsArray = deviceObj["ports"].toArray();
        
        for (const QJsonValue& portValue : portsArray) {
            QJsonObject portObj = portValue.toObject();
            
            PortInfo port;
            port.deviceName = deviceName;
            port.portNumber = portObj["portNumber"].toInt();
            port.portType = static_cast<PortType>(portObj["portType"].toInt());
            port.description = portObj["description"].toString();
            port.maxVoltage = portObj["maxVoltage"].toDouble();
            port.maxCurrent = portObj["maxCurrent"].toDouble();
            port.isAvailable = portObj["isAvailable"].toBool();
            port.allocatedTo = portObj["allocatedTo"].toString();
            
            ports.append(port);
        }
        
        devicePorts_[deviceName] = ports;
    }
    
    return true;
}

QVector<PortInfo> PortManager::autoAllocatePorts(ComponentType componentType, const QString& allocatedTo)
{
    // 检查用户名是否有效
    if (allocatedTo.isEmpty() || allocatedTo.trimmed().isEmpty()) {
        qWarning() << "警告：尝试自动分配端口给空用户名，操作被拒绝";
        return QVector<PortInfo>();
    }
    
    // 在分配前清理无效分配
    cleanupInvalidAllocations();
    
    QVector<PortInfo> recommendedPorts = getRecommendedPorts(componentType);
    QVector<PortInfo> allocatedPorts;
    
    qDebug() << "开始自动分配端口，用户:" << allocatedTo << "组件类型:" << static_cast<int>(componentType);
    
    for (const PortInfo& recommendedPort : recommendedPorts) {
        if (allocatePort(recommendedPort.deviceName, recommendedPort.portNumber, allocatedTo)) {
            allocatedPorts.append(getPortInfo(recommendedPort.deviceName, recommendedPort.portNumber));
        }
    }
    
    qDebug() << "自动分配端口完成:" << componentType << "分配了" << allocatedPorts.size() << "个端口";
    return allocatedPorts;
}

bool PortManager::isPortAvailable(const QString& deviceName, int portNumber) const
{
    QMutexLocker locker(&portMutex_);
    const PortInfo* port = findPort(deviceName, portNumber);
    return port ? port->isAvailable : false;
}

bool PortManager::isPortAllocated(const QString& deviceName, int portNumber) const
{
    return !isPortAvailable(deviceName, portNumber);
}

QString PortManager::getPortAllocatedTo(const QString& deviceName, int portNumber) const
{
    QMutexLocker locker(&portMutex_);
    const PortInfo* port = findPort(deviceName, portNumber);
    return port ? port->allocatedTo : QString();
}
