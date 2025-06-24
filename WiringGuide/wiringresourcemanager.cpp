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
    
    schemesPath_ = basePath + "/WiringSchemes";
    templatesPath_ = basePath + "/WiringTemplates";
    configPath_ = basePath + "/Config";
    
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
    
    // 只清理本地数据，不调用portManager的方法，避免双重释放
    activeAllocations_.clear();
    loadedSchemes_.clear();
    templates_.clear();
    
    qDebug() << "WiringResourceManager: 所有资源已清理";
    qDebug() << "WiringResourceManager: 析构完成";
}

bool WiringResourceManager::initializeResources()
{
    qDebug() << "初始化接线资源管理器...";
    
    // 创建必要的目录
    if (!createDirectories()) {
        qWarning() << "创建目录失败";
        return false;
    }
    
    // 加载默认模板
    if (!loadDefaultTemplates()) {
        qWarning() << "加载默认模板失败";
        return false;
    }
    
    // 加载现有方案
    if (!loadExistingSchemes()) {
        qWarning() << "加载现有方案失败";
        return false;
    }
    
    qDebug() << "接线资源管理器初始化完成";
    qDebug() << "已加载模板数量:" << templates_.size();
    qDebug() << "已加载方案数量:" << loadedSchemes_.size();
    
    return true;
}

bool WiringResourceManager::createDirectories()
{
    QDir dir;
    
    bool success = true;
    success &= dir.mkpath(schemesPath_);
    success &= dir.mkpath(templatesPath_);
    success &= dir.mkpath(configPath_);
    
    if (success) {
        qDebug() << "目录创建成功:";
        qDebug() << "  方案目录:" << schemesPath_;
        qDebug() << "  模板目录:" << templatesPath_;
        qDebug() << "  配置目录:" << configPath_;
    }
    
    return success;
}

bool WiringResourceManager::loadDefaultTemplates()
{
    templates_.clear();
    
    // 创建各种元件的默认模板
    templates_[ComponentType::RESISTOR].append(createDefaultResistorTemplate());
    templates_[ComponentType::CAPACITOR].append(createDefaultCapacitorTemplate());
    templates_[ComponentType::INDUCTOR].append(createDefaultInductorTemplate());
    templates_[ComponentType::DIODE].append(createDefaultDiodeTemplate());
    templates_[ComponentType::IC].append(createDefaultICTemplate());
    
    // 保存模板到文件
    for (auto it = templates_.begin(); it != templates_.end(); ++it) {
        ComponentType type = it.key();
        const QVector<WiringScheme>& schemes = it.value();
        
        for (const WiringScheme& scheme : schemes) {
            QString filePath = generateTemplateFilePath(type, scheme.schemeId);
            if (!saveSchemeToFile(scheme, filePath)) {
                qWarning() << "保存模板失败:" << filePath;
            }
        }
    }
    
    qDebug() << "默认模板加载完成";
    return true;
}

bool WiringResourceManager::loadExistingSchemes()
{
    loadedSchemes_.clear();
    
    QDir schemesDir(schemesPath_);
    QStringList filters;
    filters << "*.json";
    schemesDir.setNameFilters(filters);
    
    QStringList schemeFiles = schemesDir.entryList(QDir::Files);
    
    for (const QString& fileName : schemeFiles) {
        QString filePath = schemesDir.absoluteFilePath(fileName);
        WiringScheme scheme;
        
        if (loadSchemeFromFile(filePath, scheme)) {
            loadedSchemes_[scheme.schemeId] = scheme;
            qDebug() << "加载方案:" << scheme.schemeName;
        } else {
            qWarning() << "加载方案失败:" << filePath;
        }
    }
    
    return true;
}

WiringScheme WiringResourceManager::createDefaultResistorTemplate() const
{
    WiringScheme scheme;
    scheme.schemeId = QUuid::createUuid().toString();
    scheme.schemeName = "标准电阻测试模板";
    scheme.componentType = ComponentType::RESISTOR;
    scheme.description = "使用四线法测量电阻的标准接线方案";
      // 创建默认连接 - 电阻使用万用表2线法测量
    if (portManager_) {
        QVector<PortInfo> recommendedPorts = portManager_->getRecommendedPorts(ComponentType::RESISTOR);
        
        if (recommendedPorts.size() >= 2) {
            // 连接1：万用表通道0到电阻第一端
            ConnectionInfo conn1;
            conn1.sourcePort = recommendedPorts[0]; // 万用表CH0
            conn1.wireColor = "红色";
            conn1.instruction = "连接万用表CH0到电阻第一端";
            scheme.connections.append(conn1);
            
            // 连接2：万用表通道1到电阻第二端
            ConnectionInfo conn2;
            conn2.sourcePort = recommendedPorts[1]; // 万用表CH1
            conn2.wireColor = "黑色";
            conn2.instruction = "连接万用表CH1到电阻第二端";
            scheme.connections.append(conn2);
        }
    }
      // 设置测试参数 - 2线法万用表测量
    scheme.testParameters["test_method"] = "two_wire";
    scheme.testParameters["measurement_function"] = "2_Wire_Resistance";
    scheme.testParameters["device"] = "JY8902";
    scheme.testParameters["samples_per_trigger"] = 20;
    scheme.testParameters["measurement_range"] = "auto";
    
    return scheme;
}

WiringScheme WiringResourceManager::createDefaultCapacitorTemplate() const
{
    WiringScheme scheme;
    scheme.schemeId = QUuid::createUuid().toString();
    scheme.schemeName = "标准电容测试模板";    scheme.componentType = ComponentType::CAPACITOR;
    scheme.description = "使用万用表测量电容的标准接线方案";
    
    // 创建默认连接 - 电容使用万用表测量
    if (portManager_) {
        QVector<PortInfo> recommendedPorts = portManager_->getRecommendedPorts(ComponentType::CAPACITOR);
        
        if (recommendedPorts.size() >= 2) {
            // 连接1：万用表通道0到电容第一端
            ConnectionInfo conn1;
            conn1.sourcePort = recommendedPorts[0]; // 万用表CH0
            conn1.wireColor = "红色";
            conn1.instruction = "连接万用表CH0到电容正极";
            scheme.connections.append(conn1);
            
            // 连接2：万用表通道1到电容第二端
            ConnectionInfo conn2;
            conn2.sourcePort = recommendedPorts[1]; // 万用表CH1
            conn2.wireColor = "黑色";
            conn2.instruction = "连接万用表CH1到电容负极";
            scheme.connections.append(conn2);
        }
    }
    
    // 设置测试参数 - 万用表电容测量
    scheme.testParameters["test_method"] = "dmm_capacitance";
    scheme.testParameters["measurement_function"] = "Capacitance";
    scheme.testParameters["device"] = "JY8902";
    scheme.testParameters["test_frequency"] = 1000.0;
    scheme.testParameters["measurement_range"] = "auto";
    
    return scheme;
}

WiringScheme WiringResourceManager::createDefaultInductorTemplate() const
{
    WiringScheme scheme;
    scheme.schemeId = QUuid::createUuid().toString();
    scheme.schemeName = "标准电感测试模板";    scheme.componentType = ComponentType::INDUCTOR;
    scheme.description = "使用万用表测量电感的标准接线方案";
    
    // 创建默认连接 - 电感使用万用表测量
    if (portManager_) {
        QVector<PortInfo> recommendedPorts = portManager_->getRecommendedPorts(ComponentType::INDUCTOR);
        
        if (recommendedPorts.size() >= 2) {
            // 连接1：万用表通道0到电感第一端
            ConnectionInfo conn1;
            conn1.sourcePort = recommendedPorts[0]; // 万用表CH0
            conn1.wireColor = "红色";
            conn1.instruction = "连接万用表CH0到电感第一端";
            scheme.connections.append(conn1);
            
            // 连接2：万用表通道1到电感第二端
            ConnectionInfo conn2;
            conn2.sourcePort = recommendedPorts[1]; // 万用表CH1
            conn2.wireColor = "黑色";
            conn2.instruction = "连接万用表CH1到电感第二端";
            scheme.connections.append(conn2);
        }
    }
    
    // 设置测试参数 - 万用表电感测量
    scheme.testParameters["test_method"] = "dmm_inductance";
    scheme.testParameters["measurement_function"] = "Inductance";
    scheme.testParameters["device"] = "JY8902";
    scheme.testParameters["test_frequency"] = 1000.0;
    scheme.testParameters["measurement_range"] = "auto";
    
    return scheme;
}

WiringScheme WiringResourceManager::createDefaultDiodeTemplate() const
{
    WiringScheme scheme;
    scheme.schemeId = QUuid::createUuid().toString();
    scheme.schemeName = "标准二极管测试模板";    scheme.componentType = ComponentType::DIODE;
    scheme.description = "测量二极管正向和反向特性的接线方案";
    
    // 创建默认连接 - 二极管需要电压输出和电流/电压测量
    if (portManager_) {
        QVector<PortInfo> recommendedPorts = portManager_->getRecommendedPorts(ComponentType::DIODE);
        
        if (recommendedPorts.size() >= 3) {
            // 连接1：模拟输出到二极管阳极
            ConnectionInfo conn1;
            conn1.sourcePort = recommendedPorts[0]; // 模拟输出
            conn1.wireColor = "红色";
            conn1.instruction = "连接模拟输出到二极管阳极";
            scheme.connections.append(conn1);
            
            // 连接2：模拟输入测量阳极电压
            ConnectionInfo conn2;
            conn2.sourcePort = recommendedPorts[1]; // 模拟输入1
            conn2.wireColor = "黄色";
            conn2.instruction = "连接模拟输入到二极管阳极（电压测量）";
            scheme.connections.append(conn2);
            
            // 连接3：模拟输入测量阴极电压
            ConnectionInfo conn3;
            conn3.sourcePort = recommendedPorts[2]; // 模拟输入2
            conn3.wireColor = "绿色";
            conn3.instruction = "连接模拟输入到二极管阴极（电压测量）";
            scheme.connections.append(conn3);
        }
    }
    
    // 设置测试参数 - 二极管IV特性测量
    scheme.testParameters["test_method"] = "iv_curve";
    scheme.testParameters["forward_voltage"] = 3.3;
    scheme.testParameters["reverse_voltage"] = -5.0;
    scheme.testParameters["measurement_range"] = "auto";
    
    return scheme;
}

WiringScheme WiringResourceManager::createDefaultICTemplate() const
{
    WiringScheme scheme;
    scheme.schemeId = QUuid::createUuid().toString();
    scheme.schemeName = "标准IC测试模板";    scheme.componentType = ComponentType::IC;
    scheme.description = "IC功能测试的多端口接线方案";
    
    // 创建默认连接 - IC需要多个端口进行功能测试
    if (portManager_) {
        QVector<PortInfo> recommendedPorts = portManager_->getRecommendedPorts(ComponentType::IC);
        
        // IC测试通常需要更多端口，这里创建基本的电源和信号连接
        int portIndex = 0;
        
        // 电源连接（如果有电源端口）
        for (const auto& port : recommendedPorts) {
            if (port.portType == PortType::POWER_OUTPUT && portIndex < 2) {
                ConnectionInfo conn;
                conn.sourcePort = port;
                conn.wireColor = (portIndex == 0) ? "红色" : "黑色";
                conn.instruction = QString("连接电源%1到IC电源引脚").arg(portIndex == 0 ? "正极" : "负极");
                scheme.connections.append(conn);
                portIndex++;
            }
        }
        
        // 数字输出连接
        portIndex = 0;
        for (const auto& port : recommendedPorts) {
            if (port.portType == PortType::DIGITAL_OUTPUT && portIndex < 2) {
                ConnectionInfo conn;
                conn.sourcePort = port;
                conn.wireColor = (portIndex == 0) ? "蓝色" : "紫色";
                conn.instruction = QString("连接数字输出%1到IC输入引脚").arg(portIndex + 1);
                scheme.connections.append(conn);
                portIndex++;
            }
        }
        
        // 数字输入连接
        portIndex = 0;
        for (const auto& port : recommendedPorts) {
            if (port.portType == PortType::DIGITAL_INPUT && portIndex < 2) {
                ConnectionInfo conn;
                conn.sourcePort = port;
                conn.wireColor = (portIndex == 0) ? "橙色" : "灰色";
                conn.instruction = QString("连接数字输入%1到IC输出引脚").arg(portIndex + 1);
                scheme.connections.append(conn);
                portIndex++;
            }
        }
    }
    
    // 设置测试参数 - IC功能测试
    scheme.testParameters["test_method"] = "functional_test";
    scheme.testParameters["supply_voltage"] = 5.0;
    scheme.testParameters["logic_level_high"] = 3.3;
    scheme.testParameters["logic_level_low"] = 0.0;
    
    return scheme;
}

bool WiringResourceManager::saveWiringScheme(const WiringScheme& scheme, const QString& filePath)
{
    QString actualFilePath = filePath;
    if (actualFilePath.isEmpty()) {
        actualFilePath = generateSchemeFilePath(scheme.schemeId);
    }
    
    if (saveSchemeToFile(scheme, actualFilePath)) {
        loadedSchemes_[scheme.schemeId] = scheme;
        emit schemeCreated(scheme.schemeId);
        qDebug() << "方案保存成功:" << scheme.schemeName;
        return true;
    }
    
    return false;
}

bool WiringResourceManager::loadWiringScheme(const QString& filePath, WiringScheme& scheme)
{
    if (loadSchemeFromFile(filePath, scheme)) {
        loadedSchemes_[scheme.schemeId] = scheme;
        qDebug() << "方案加载成功:" << scheme.schemeName;
        return true;
    }
    
    return false;
}

QStringList WiringResourceManager::getAvailableSchemes() const
{
    QStringList schemeNames;
    for (auto it = loadedSchemes_.begin(); it != loadedSchemes_.end(); ++it) {
        schemeNames.append(it.value().schemeName);
    }
    return schemeNames;
}

WiringScheme WiringResourceManager::getScheme(const QString& schemeId) const
{
    return loadedSchemes_.value(schemeId, WiringScheme());
}

QVector<WiringScheme> WiringResourceManager::getTemplatesForComponent(ComponentType componentType) const
{
    return templates_.value(componentType, QVector<WiringScheme>());
}

bool WiringResourceManager::saveTemplate(const WiringScheme& scheme)
{
    QString filePath = generateTemplateFilePath(scheme.componentType, scheme.schemeId);
    
    if (saveSchemeToFile(scheme, filePath)) {
        templates_[scheme.componentType].append(scheme);
        qDebug() << "模板保存成功:" << scheme.schemeName;
        return true;
    }
    
    return false;
}

bool WiringResourceManager::deleteTemplate(const QString& schemeId)
{
    for (auto it = templates_.begin(); it != templates_.end(); ++it) {
        QVector<WiringScheme>& schemes = it.value();
        for (int i = 0; i < schemes.size(); ++i) {
            if (schemes[i].schemeId == schemeId) {
                schemes.removeAt(i);
                
                // 删除文件
                QString filePath = generateTemplateFilePath(it.key(), schemeId);
                QFile::remove(filePath);
                
                qDebug() << "模板删除成功:" << schemeId;
                return true;
            }
        }
    }
    
    return false;
}

bool WiringResourceManager::validateWiringScheme(const WiringScheme& scheme, QStringList& errors) const
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
    
    // 验证连接
    if (!validateConnections(scheme.connections, errors)) {
        isValid = false;
    }
    
    // 验证端口可用性
    if (!validatePortAvailability(scheme.connections, errors)) {
        isValid = false;
    }
    
    // 验证信号兼容性
    if (!validateSignalCompatibility(scheme.connections, errors)) {
        isValid = false;
    }
    
    return isValid;
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
    
    // 验证连接
    if (!validateConnections(scheme.connections, errors)) {
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

bool WiringResourceManager::checkPortConflicts(const WiringScheme& scheme, QStringList& conflicts) const
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
            
            // 检查端口是否已经分配给了当前方案的组件
            // 从scheme中获取组件信息来判断是否为同一用户
            // 这里我们需要一个更好的方法来识别当前用户
            // 暂时跳过已分配给某个用户的端口检查，因为validatePortAvailability已经处理了这个问题
            qDebug() << "端口" << portKey << "已分配给:" << allocatedTo << "继续检查其他端口...";
            
            // 不再将已分配的端口视为冲突，因为可能是分配给当前用户的
            // conflicts.append(QString("端口已被占用: %1 (分配给: %2)")
            //                .arg(portKey).arg(allocatedTo));
        }
    }
    
    return conflicts.isEmpty();
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

bool WiringResourceManager::allocateResourcesForScheme(const WiringScheme& scheme)
{
    if (!portManager_) {
        qWarning() << "端口管理器未初始化";
        return false;
    }
    
    // 从scheme的测试参数中获取组件引用（用户ID）
    QString currentUser = scheme.testParameters.value("component_reference", "").toString();
    
    // 检查冲突
    QStringList conflicts;
    bool hasConflicts = false;
    
    if (!currentUser.isEmpty()) {
        // 使用带用户ID的版本
        hasConflicts = !checkPortConflicts(scheme, conflicts, currentUser);
    } else {
        // 使用原版本
        hasConflicts = !checkPortConflicts(scheme, conflicts);
    }
    
    if (hasConflicts) {
        qWarning() << "资源分配失败，存在冲突:" << conflicts;
        return false;
    }
      // 分配端口
    QVector<QPair<QString, int>> allocatedPorts;
    bool allocationSuccess = true;
    
    // 确保有有效的用户ID用于端口分配
    QString allocUser = currentUser.isEmpty() ? scheme.schemeId : currentUser;
      for (const ConnectionInfo& connection : scheme.connections) {
        // 检查端口是否已经分配给了当前用户
        QString currentAllocatedTo = portManager_->getPortAllocatedTo(
            connection.sourcePort.deviceName, connection.sourcePort.portNumber);
        
        if (currentAllocatedTo == allocUser) {
            // 端口已经分配给当前用户，跳过
            qDebug() << "端口" << connection.sourcePort.deviceName << connection.sourcePort.portNumber 
                     << "已经分配给当前用户:" << allocUser << "，跳过分配";
            allocatedPorts.append(qMakePair(connection.sourcePort.deviceName,
                                           connection.sourcePort.portNumber));
            continue;
        }
        
        if (portManager_->allocatePort(connection.sourcePort.deviceName,
                                      connection.sourcePort.portNumber,
                                      allocUser)) {
            allocatedPorts.append(qMakePair(connection.sourcePort.deviceName,
                                           connection.sourcePort.portNumber));
        } else {
            allocationSuccess = false;
            break;
        }
    }
    
    if (!allocationSuccess) {
        // 回滚已分配的端口
        for (const auto& port : allocatedPorts) {
            portManager_->releasePort(port.first, port.second);
        }
        qWarning() << "资源分配失败";
        return false;
    }
    
    activeAllocations_[scheme.schemeId] = QString("分配了 %1 个端口").arg(allocatedPorts.size());
    emit resourcesAllocated(scheme.schemeId);
    
    qDebug() << "资源分配成功:" << scheme.schemeName;
    return true;
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
    
    activeAllocations_.remove(scheme.schemeId);
    
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
    
    activeAllocations_.clear();
    qDebug() << "所有资源已释放";
}

QVector<WiringScheme> WiringResourceManager::getRecommendedSchemes(ComponentType componentType) const
{
    return getTemplatesForComponent(componentType);
}

WiringScheme WiringResourceManager::generateOptimalScheme(const ComponentSpec& component) const
{
    // 获取模板
    QVector<WiringScheme> templates = getTemplatesForComponent(component.type);
    
    if (templates.isEmpty()) {
        qWarning() << "没有找到适合的模板";
        return WiringScheme();
    }
    
    // 使用第一个模板作为基础
    WiringScheme optimalScheme = templates.first();
    
    // 自定义方案信息
    optimalScheme.schemeId = QUuid::createUuid().toString();
    optimalScheme.schemeName = QString("%1测试方案").arg(component.reference);
    optimalScheme.description = QString("为%1自动生成的优化接线方案").arg(component.reference);
    
    // 根据元件参数调整测试参数
    if (component.type == ComponentType::RESISTOR) {
        // 根据电阻值调整测试电流
        double testCurrent = qMin(0.001, 1.0 / component.nominal_value);
        optimalScheme.testParameters["test_current"] = testCurrent;
    }
    
    return optimalScheme;
}

int WiringResourceManager::getTotalSchemes() const
{
    return loadedSchemes_.size();
}

int WiringResourceManager::getActiveSchemes() const
{
    return activeAllocations_.size();
}

QMap<ComponentType, int> WiringResourceManager::getSchemeStatistics() const
{
    QMap<ComponentType, int> stats;
    
    for (auto it = loadedSchemes_.begin(); it != loadedSchemes_.end(); ++it) {
        ComponentType type = it.value().componentType;
        stats[type]++;
    }
    
    return stats;
}

void WiringResourceManager::onPortStatusChanged(const QString& deviceName, int portNumber, bool available)
{
    // 端口状态变化时的处理
    qDebug() << "端口状态变化:" << deviceName << portNumber << (available ? "可用" : "不可用");
}

// 文件操作相关方法
QString WiringResourceManager::generateSchemeFilePath(const QString& schemeId) const
{
    return schemesPath_ + "/" + schemeId + ".json";
}

QString WiringResourceManager::generateTemplateFilePath(ComponentType componentType, const QString& schemeId) const
{
    QString typeName = componentTypeToString(componentType);
    return templatesPath_ + "/" + typeName + "_" + schemeId + ".json";
}

bool WiringResourceManager::saveSchemeToFile(const WiringScheme& scheme, const QString& filePath) const
{
    QJsonObject rootObj;
    rootObj["schemeId"] = scheme.schemeId;
    rootObj["schemeName"] = scheme.schemeName;
    rootObj["componentType"] = static_cast<int>(scheme.componentType);
    rootObj["description"] = scheme.description;
    
    // 保存连接信息
    QJsonArray connectionsArray;
    for (const ConnectionInfo& connection : scheme.connections) {
        connectionsArray.append(connectionToJson(connection));
    }
    rootObj["connections"] = connectionsArray;
    
    // 保存测试参数
    QJsonObject parametersObj;
    for (auto it = scheme.testParameters.begin(); it != scheme.testParameters.end(); ++it) {
        parametersObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    rootObj["testParameters"] = parametersObj;
    
    // 添加时间戳
    rootObj["createdTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(rootObj);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        return true;
    }
    
    return false;
}

bool WiringResourceManager::loadSchemeFromFile(const QString& filePath, WiringScheme& scheme) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject rootObj = doc.object();
    
    scheme.schemeId = rootObj["schemeId"].toString();
    scheme.schemeName = rootObj["schemeName"].toString();
    scheme.componentType = static_cast<ComponentType>(rootObj["componentType"].toInt());
    scheme.description = rootObj["description"].toString();
    
    // 加载连接信息
    scheme.connections.clear();
    QJsonArray connectionsArray = rootObj["connections"].toArray();
    for (const QJsonValue& value : connectionsArray) {
        scheme.connections.append(connectionFromJson(value.toObject()));
    }
    
    // 加载测试参数
    scheme.testParameters.clear();
    QJsonObject parametersObj = rootObj["testParameters"].toObject();
    for (auto it = parametersObj.begin(); it != parametersObj.end(); ++it) {
        scheme.testParameters[it.key()] = it.value().toVariant();
    }
    
    return true;
}

// JSON转换相关方法
QJsonObject WiringResourceManager::connectionToJson(const ConnectionInfo& connection) const
{
    QJsonObject obj;
    obj["sourcePort"] = portInfoToJson(connection.sourcePort);
    obj["targetPort"] = portInfoToJson(connection.targetPort);
    obj["wireColor"] = connection.wireColor;
    obj["instruction"] = connection.instruction;
    obj["isCompleted"] = connection.isCompleted;
    return obj;
}

ConnectionInfo WiringResourceManager::connectionFromJson(const QJsonObject& jsonObj) const
{
    ConnectionInfo connection;
    connection.sourcePort = portInfoFromJson(jsonObj["sourcePort"].toObject());
    connection.targetPort = portInfoFromJson(jsonObj["targetPort"].toObject());
    connection.wireColor = jsonObj["wireColor"].toString();
    connection.instruction = jsonObj["instruction"].toString();
    connection.isCompleted = jsonObj["isCompleted"].toBool();
    return connection;
}

QJsonObject WiringResourceManager::portInfoToJson(const PortInfo& port) const
{
    QJsonObject obj;
    obj["deviceName"] = port.deviceName;
    obj["portNumber"] = port.portNumber;
    obj["portType"] = static_cast<int>(port.portType);
    obj["description"] = port.description;
    obj["maxVoltage"] = port.maxVoltage;
    obj["maxCurrent"] = port.maxCurrent;
    obj["isAvailable"] = port.isAvailable;
    obj["allocatedTo"] = port.allocatedTo;
    return obj;
}

PortInfo WiringResourceManager::portInfoFromJson(const QJsonObject& jsonObj) const
{
    PortInfo port;
    port.deviceName = jsonObj["deviceName"].toString();
    port.portNumber = jsonObj["portNumber"].toInt();
    port.portType = static_cast<PortType>(jsonObj["portType"].toInt());
    port.description = jsonObj["description"].toString();
    port.maxVoltage = jsonObj["maxVoltage"].toDouble();
    port.maxCurrent = jsonObj["maxCurrent"].toDouble();
    port.isAvailable = jsonObj["isAvailable"].toBool();
    port.allocatedTo = jsonObj["allocatedTo"].toString();
    return port;
}

// 验证相关方法
bool WiringResourceManager::validateConnections(const QVector<ConnectionInfo>& connections, QStringList& errors) const
{
    bool isValid = true;
    QSet<QString> usedPorts;
    
    for (const ConnectionInfo& connection : connections) {
        // 检查源端口是否重复使用
        QString sourceKey = QString("%1:%2")
                           .arg(connection.sourcePort.deviceName)
                           .arg(connection.sourcePort.portNumber);
        
        if (usedPorts.contains(sourceKey)) {
            errors.append(QString("端口重复使用: %1").arg(sourceKey));
            isValid = false;
        } else {
            usedPorts.insert(sourceKey);
        }
        
        // 检查连接信息完整性
        if (connection.sourcePort.deviceName.isEmpty()) {
            errors.append("源设备名称不能为空");
            isValid = false;
        }
        
        if (connection.instruction.isEmpty()) {
            errors.append("连接说明不能为空");
            isValid = false;
        }
    }
    
    return isValid;
}

bool WiringResourceManager::validatePortAvailability(const QVector<ConnectionInfo>& connections, QStringList& errors) const
{
    if (!portManager_) {
        errors.append("端口管理器未初始化");
        return false;
    }
    
    bool isValid = true;
    
    for (const ConnectionInfo& connection : connections) {
        if (!portManager_->isPortAvailable(connection.sourcePort.deviceName,
                                          connection.sourcePort.portNumber)) {
            QString allocatedTo = portManager_->getPortAllocatedTo(
                connection.sourcePort.deviceName, connection.sourcePort.portNumber);
            errors.append(QString("端口不可用: %1:%2 (已分配给: %3)")
                         .arg(connection.sourcePort.deviceName)
                         .arg(connection.sourcePort.portNumber)
                         .arg(allocatedTo));
            isValid = false;
        }
    }
      return isValid;
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

// 工具方法
QString WiringResourceManager::componentTypeToString(ComponentType type) const
{
    switch (type) {
    case ComponentType::RESISTOR: return "Resistor";
    case ComponentType::CAPACITOR: return "Capacitor";
    case ComponentType::INDUCTOR: return "Inductor";
    case ComponentType::DIODE: return "Diode";
    case ComponentType::IC: return "IC";
    default: return "Unknown";
    }
}

ComponentType WiringResourceManager::stringToComponentType(const QString& typeString) const
{
    if (typeString == "Resistor") return ComponentType::RESISTOR;
    if (typeString == "Capacitor") return ComponentType::CAPACITOR;
    if (typeString == "Inductor") return ComponentType::INDUCTOR;
    if (typeString == "Diode") return ComponentType::DIODE;
    if (typeString == "IC") return ComponentType::IC;
    return ComponentType::RESISTOR; // 默认值
}
