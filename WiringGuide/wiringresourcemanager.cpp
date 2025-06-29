#include "wiringresourcemanager.h"
#include <QDebug>
#include <QUuid>
#include <QMessageBox>

WiringResourceManager::WiringResourceManager(PortManager* portManager, QObject *parent)
    : QObject(parent)
    , portManager_(portManager)
    , isDestructing_(false)
{
    // 设置路径
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString basePath = documentsPath + "/FaultDetect";
    
    // 连接端口管理器信号
    if (portManager_) {
        connect(portManager_, &PortManager::portStatusChanged,
                this, &WiringResourceManager::onPortStatusChanged);
    }
}

WiringResourceManager::~WiringResourceManager()
{
    qDebug() << "WiringResourceManager: 开始析构...";
    
    // 设置析构标志
    isDestructing_ = true;
    
    // 先断开所有信号连接，避免析构过程中触发信号
    if (portManager_) {
        disconnect(portManager_, nullptr, this, nullptr);
    }
    disconnect(this, nullptr, nullptr, nullptr);
    
    qDebug() << "WiringResourceManager: 所有资源已清理";
    qDebug() << "WiringResourceManager: 析构完成";
}

bool WiringResourceManager::validateWiringScheme(const WiringScheme& scheme, QStringList& errors, const QString& currentUser) const
{
    errors.clear();
    bool isValid = true;
    
    // 验证基本信息
    if (scheme.schemeName.isEmpty()) {
        errors.append("方案名称不能为空");
        isValid = false;
    }
    
    if (scheme.connections.isEmpty()) {
        errors.append("连接列表不能为空");
        isValid = false;
    }
    
    // 验证端口可用性（使用当前用户信息）
    if (!validatePortAvailability(scheme.connections, errors, currentUser)) {
        isValid = false;
    }
    
    // 验证信号兼容性
    if (!validateSignalCompatibility(scheme.connections, errors)) {
        isValid = false;
    }
    
    return isValid;
}

bool WiringResourceManager::checkPortConflicts(const WiringScheme& scheme, QStringList& conflicts, const QString& currentUser) const
{
    conflicts.clear();
    
    if (!portManager_) {
        conflicts.append("端口管理器未初始化");
        return false;
    }
    
    QSet<QString> usedPorts;
    
    for (const ConnectionInfo& connection : scheme.connections) {
        QString portKey = QString("%1:%2")
                         .arg(connection.sourcePort.deviceName)
                         .arg(connection.sourcePort.portNumber);
        
        if (usedPorts.contains(portKey)) {
            conflicts.append(QString("端口冲突: %1").arg(portKey));
        } else {
            usedPorts.insert(portKey);
        }
        
        // 检查端口是否可用
        if (!portManager_->isPortAvailable(connection.sourcePort.deviceName, 
                                          connection.sourcePort.portNumber)) {
            QString allocatedTo = portManager_->getPortAllocatedTo(
                connection.sourcePort.deviceName, connection.sourcePort.portNumber);
            
            // 如果端口已分配给当前用户，则不是冲突
            if (allocatedTo != currentUser) {
                conflicts.append(QString("端口已被占用: %1 (分配给: %2)")
                               .arg(portKey).arg(allocatedTo));
            } else {
                qDebug() << "端口" << portKey << "已正确分配给当前用户:" << currentUser;
            }
        }
    }
    
    return conflicts.isEmpty();
}

bool WiringResourceManager::releaseResourcesForScheme(const WiringScheme& scheme)
{
    if (!portManager_) {
        qWarning() << "端口管理器未初始化";
        return false;
    }
    
    // 如果正在析构，不发送信号避免崩溃
    if (!isDestructing_) {
        for (const ConnectionInfo& connection : scheme.connections) {
            portManager_->releasePort(connection.sourcePort.deviceName,
                                     connection.sourcePort.portNumber);
        }
    }
    
    // 如果正在析构，不发送信号
    if (!isDestructing_) {
        emit resourcesReleased(scheme.schemeId);
    }
    
    qDebug() << "资源释放成功:" << scheme.schemeName;
    return true;
}

void WiringResourceManager::releaseAllResources()
{
    // 如果正在析构，不调用portManager的方法，避免双重释放
    if (!isDestructing_ && portManager_) {
        portManager_->releaseAllPorts();
    }

    qDebug() << "所有资源已释放";
}

void WiringResourceManager::onPortStatusChanged(const QString& deviceName, int portNumber, bool available)
{
    // 端口状态变化时的处理
    qDebug() << "端口状态变化:" << deviceName << portNumber << (available ? "可用" : "不可用");
}

bool WiringResourceManager::validatePortAvailability(const QVector<ConnectionInfo>& connections, QStringList& errors, const QString& currentUser) const
{
    if (!portManager_) {
        errors.append("端口管理器未初始化");
        return false;
    }
    
    bool isValid = true;
    
    for (const ConnectionInfo& connection : connections) {
        QString allocatedTo = portManager_->getPortAllocatedTo(
            connection.sourcePort.deviceName, connection.sourcePort.portNumber);
        
        // 检查端口是否可用
        bool isAvailable = portManager_->isPortAvailable(connection.sourcePort.deviceName,
                                                        connection.sourcePort.portNumber);
        
        if (isAvailable) {
            // 端口可用，验证通过
            continue;
        } else if (allocatedTo == currentUser) {
            // 端口已分配给当前用户，这是预期的，验证通过
            qDebug() << "端口" << connection.sourcePort.deviceName << connection.sourcePort.portNumber 
                     << "已正确分配给当前用户:" << currentUser;
            continue;
        } else {
            // 端口被其他用户占用
            errors.append(QString("端口不可用: %1:%2 (已分配给: %3)")
                         .arg(connection.sourcePort.deviceName)
                         .arg(connection.sourcePort.portNumber)
                         .arg(allocatedTo));
            isValid = false;
        }
    }
    
    return isValid;
}

bool WiringResourceManager::validateSignalCompatibility(const QVector<ConnectionInfo>& connections, QStringList& errors) const
{
    // 这里可以添加信号兼容性检查
    // 例如：检查电压电流是否在安全范围内
    Q_UNUSED(connections)
    Q_UNUSED(errors)
    return true;
}
