#include "wiringtaskgenerator.h"
#include <QDebug>
#include <QUuid>
#include <QDateTime>

WiringTaskGenerator::WiringTaskGenerator(WiringResourceManager* resourceManager, QObject *parent)
    : QObject(parent)
    , resourceManager_(resourceManager)
    , scheduleTimer_(new QTimer(this))
{
    // 连接资源管理器信号
    if (resourceManager_) {
        connect(resourceManager_, &WiringResourceManager::schemeCreated,
                this, &WiringTaskGenerator::onSchemeCreated);
        connect(resourceManager_, &WiringResourceManager::resourcesAllocated,
                this, &WiringTaskGenerator::onResourcesAllocated);
        connect(resourceManager_, &WiringResourceManager::resourcesReleased,
                this, &WiringTaskGenerator::onResourcesReleased);
    }
    
    // 设置调度定时器
    connect(scheduleTimer_, &QTimer::timeout, this, &WiringTaskGenerator::checkTaskSchedule);
    scheduleTimer_->start(5000); // 每5秒检查一次调度
}

WiringTaskGenerator::~WiringTaskGenerator()
{
}

QString WiringTaskGenerator::generateTask(const ComponentSpec& component)
{
    qDebug() << "生成测试任务:" << component.reference;
    
    try {
        // 生成最优接线方案
        WiringScheme optimalScheme = resourceManager_->generateOptimalScheme(component);
        if (optimalScheme.schemeId.isEmpty()) {
            QString error = QString("无法为元件 %1 生成接线方案").arg(component.reference);
            emit errorOccurred(error);
            return QString();
        }
        
        // 创建测试任务
        TestTask task = createTaskFromScheme(optimalScheme, component);
        
        // 验证任务
        QStringList errors;
        if (!validateTaskConfiguration(task, errors)) {
            QString error = QString("任务验证失败: %1").arg(errors.join("; "));
            emit errorOccurred(error);
            return QString();
        }
        
        // 保存任务
        tasks_[task.taskId] = task;
        
        qDebug() << "测试任务生成成功:" << task.taskName;
        emit taskGenerated(task);
        
        return task.taskId;
        
    } catch (const std::exception& e) {
        QString error = QString("生成任务时发生异常: %1").arg(e.what());
        emit errorOccurred(error);
        return QString();
    }
}

QString WiringTaskGenerator::generateTaskFromScheme(const WiringScheme& scheme, const ComponentSpec& component)
{
    qDebug() << "从方案生成测试任务:" << scheme.schemeName;
    
    TestTask task = createTaskFromScheme(scheme, component);
    
    // 验证任务
    QStringList errors;
    if (!validateTaskConfiguration(task, errors)) {
        QString error = QString("任务验证失败: %1").arg(errors.join("; "));
        emit errorOccurred(error);
        return QString();
    }
    
    // 保存任务
    tasks_[task.taskId] = task;
    
    emit taskGenerated(task);
    return task.taskId;
}

QString WiringTaskGenerator::generateBatchTasks(const QVector<ComponentSpec>& components)
{
    qDebug() << "生成批量测试任务，数量:" << components.size();
    
    QStringList taskIds;
    QString batchId = QUuid::createUuid().toString();
    
    for (const ComponentSpec& component : components) {
        QString taskId = generateTask(component);
        if (!taskId.isEmpty()) {
            taskIds.append(taskId);
            
            // 为批量任务添加标记
            if (tasks_.contains(taskId)) {
                tasks_[taskId].notes += QString(" [批量任务: %1]").arg(batchId);
            }
        }
    }
    
    if (!taskIds.isEmpty()) {
        emit batchTasksGenerated(taskIds);
        qDebug() << "批量任务生成成功，数量:" << taskIds.size();
    } else {
        emit errorOccurred("批量任务生成失败");
    }
    
    return batchId;
}

TestTask WiringTaskGenerator::getTask(const QString& taskId) const
{
    return tasks_.value(taskId, TestTask());
}

QVector<TestTask> WiringTaskGenerator::getAllTasks() const
{
    QVector<TestTask> allTasks;
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
        allTasks.append(it.value());
    }
    return allTasks;
}

QVector<TestTask> WiringTaskGenerator::getTasksByStatus(const QString& status) const
{
    QVector<TestTask> filteredTasks;
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
        if (it.value().status == status) {
            filteredTasks.append(it.value());
        }
    }
    return filteredTasks;
}

QVector<TestTask> WiringTaskGenerator::getTasksByComponent(ComponentType componentType) const
{
    QVector<TestTask> filteredTasks;
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
        if (it.value().component.type == componentType) {
            filteredTasks.append(it.value());
        }
    }
    return filteredTasks;
}

bool WiringTaskGenerator::updateTaskStatus(const QString& taskId, const QString& status)
{
    if (!tasks_.contains(taskId)) {
        qWarning() << "任务不存在:" << taskId;
        return false;
    }
    
    QString oldStatus = tasks_[taskId].status;
    tasks_[taskId].status = status;
    
    qDebug() << "任务状态更新:" << taskId << oldStatus << "->" << status;
    emit taskUpdated(taskId, status);
    
    if (status == "completed") {
        emit taskCompleted(taskId, true);
    } else if (status == "failed") {
        emit taskCompleted(taskId, false);
    }
    
    return true;
}

bool WiringTaskGenerator::scheduleTask(const QString& taskId, const QDateTime& scheduledTime)
{
    if (!tasks_.contains(taskId)) {
        qWarning() << "任务不存在:" << taskId;
        return false;
    }
    
    tasks_[taskId].scheduledTime = scheduledTime;
    tasks_[taskId].status = "scheduled";
    
    qDebug() << "任务调度成功:" << taskId << "调度时间:" << scheduledTime.toString();
    emit taskUpdated(taskId, "scheduled");
    
    return true;
}

bool WiringTaskGenerator::cancelTask(const QString& taskId)
{
    if (!tasks_.contains(taskId)) {
        qWarning() << "任务不存在:" << taskId;
        return false;
    }
    
    TestTask& task = tasks_[taskId];
    
    // 释放资源
    if (resourceManager_) {
        resourceManager_->releaseResourcesForScheme(task.wiringScheme);
    }
    
    task.status = "cancelled";
    
    qDebug() << "任务取消成功:" << taskId;
    emit taskUpdated(taskId, "cancelled");
    
    return true;
}

bool WiringTaskGenerator::deleteTask(const QString& taskId)
{
    if (!tasks_.contains(taskId)) {
        qWarning() << "任务不存在:" << taskId;
        return false;
    }
    
    // 先取消任务（释放资源）
    cancelTask(taskId);
    
    // 删除任务
    tasks_.remove(taskId);
    
    qDebug() << "任务删除成功:" << taskId;
    return true;
}

bool WiringTaskGenerator::validateTask(const QString& taskId, QStringList& errors) const
{
    if (!tasks_.contains(taskId)) {
        errors.append("任务不存在");
        return false;
    }
    
    const TestTask& task = tasks_[taskId];
    return validateTaskConfiguration(task, errors);
}

bool WiringTaskGenerator::isTaskReady(const QString& taskId) const
{
    if (!tasks_.contains(taskId)) {
        return false;
    }
    
    const TestTask& task = tasks_[taskId];
    
    // 检查任务状态
    if (task.status != "pending" && task.status != "ready") {
        return false;
    }
      // 检查接线方案是否有效
    QStringList errors;
    if (!validateWiringScheme(task.wiringScheme, errors, task.component.reference)) {
        return false;
    }
    
    // 检查资源是否可用
    if (resourceManager_) {
        QStringList conflicts;
        if (!resourceManager_->checkPortConflicts(task.wiringScheme, conflicts)) {
            return false;
        }
    }
    
    return true;
}

bool WiringTaskGenerator::prepareTaskExecution(const QString& taskId)
{
    if (!tasks_.contains(taskId)) {
        qWarning() << "任务不存在:" << taskId;
        return false;
    }
    
    TestTask& task = tasks_[taskId];
    
    // 分配资源
    if (resourceManager_) {
        if (!resourceManager_->allocateResourcesForScheme(task.wiringScheme)) {
            qWarning() << "资源分配失败:" << taskId;
            emit taskFailed(taskId, "资源分配失败");
            return false;
        }
    }
    
    // 更新任务状态
    task.status = "ready";
    emit taskUpdated(taskId, "ready");
    
    qDebug() << "任务执行准备完成:" << taskId;
    return true;
}

bool WiringTaskGenerator::finalizeTaskExecution(const QString& taskId, bool success, const QString& result)
{
    if (!tasks_.contains(taskId)) {
        qWarning() << "任务不存在:" << taskId;
        return false;
    }
    
    TestTask& task = tasks_[taskId];
    
    // 释放资源
    if (resourceManager_) {
        resourceManager_->releaseResourcesForScheme(task.wiringScheme);
    }
    
    // 更新任务状态
    if (success) {
        task.status = "completed";
        task.notes += QString(" [执行成功: %1]").arg(result);
        emit taskCompleted(taskId, true);
    } else {
        task.status = "failed";
        task.notes += QString(" [执行失败: %1]").arg(result);
        emit taskFailed(taskId, result);
    }
    
    emit taskUpdated(taskId, task.status);
    
    qDebug() << "任务执行完成:" << taskId << (success ? "成功" : "失败");
    return true;
}

QStringList WiringTaskGenerator::generateTasksFromTemplate(ComponentType componentType, int count)
{
    QStringList taskIds;
    
    if (!resourceManager_) {
        emit errorOccurred("资源管理器未初始化");
        return taskIds;
    }
    
    QVector<WiringScheme> templates = resourceManager_->getTemplatesForComponent(componentType);
    if (templates.isEmpty()) {
        emit errorOccurred(QString("没有找到 %1 的模板").arg(componentTypeToString(componentType)));
        return taskIds;
    }
    
    WiringScheme templateScheme = templates.first();
    
    for (int i = 0; i < count; ++i) {
        // 创建虚拟元件规格
        ComponentSpec component;
        component.reference = QString("%1_%2").arg(componentTypeToString(componentType)).arg(i + 1);
        component.type = componentType;
        component.nominal_value = 1000; // 默认值
        component.tolerance_percent = 5.0;
        
        QString taskId = generateTaskFromScheme(templateScheme, component);
        if (!taskId.isEmpty()) {
            taskIds.append(taskId);
        }
    }
    
    return taskIds;
}

bool WiringTaskGenerator::executeBatchTasks(const QStringList& taskIds)
{
    qDebug() << "执行批量任务，数量:" << taskIds.size();
    
    bool allSuccess = true;
    
    for (const QString& taskId : taskIds) {
        if (!prepareTaskExecution(taskId)) {
            allSuccess = false;
            qWarning() << "任务准备失败:" << taskId;
        }
    }
    
    return allSuccess;
}

int WiringTaskGenerator::getTotalTasks() const
{
    return tasks_.size();
}

int WiringTaskGenerator::getCompletedTasks() const
{
    return getTasksByStatus("completed").size();
}

int WiringTaskGenerator::getPendingTasks() const
{
    return getTasksByStatus("pending").size();
}

QMap<QString, int> WiringTaskGenerator::getTaskStatistics() const
{
    QMap<QString, int> stats;
    
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
        const QString& status = it.value().status;
        stats[status]++;
    }
    
    return stats;
}

// 私有方法实现
TestTask WiringTaskGenerator::createTaskFromComponent(const ComponentSpec& component)
{
    TestTask task;
    task.taskId = generateUniqueTaskId();
    task.taskName = generateTaskName(component);
    task.component = component;
    task.testParameters = generateTestParameters(component);
    task.status = "pending";
    task.notes = QString("为元件 %1 自动生成的测试任务").arg(component.reference);
    
    return task;
}

TestTask WiringTaskGenerator::createTaskFromScheme(const WiringScheme& scheme, const ComponentSpec& component)
{
    TestTask task = createTaskFromComponent(component);
    task.wiringScheme = scheme;
    
    // 合并测试参数
    for (auto it = scheme.testParameters.begin(); it != scheme.testParameters.end(); ++it) {
        task.testParameters[it.key()] = it.value();
    }
    
    return task;
}

QString WiringTaskGenerator::generateUniqueTaskId() const
{
    return QUuid::createUuid().toString();
}

QString WiringTaskGenerator::generateTaskName(const ComponentSpec& component) const
{
    return QString("%1测试任务_%2").arg(component.reference).arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
}

QMap<QString, QVariant> WiringTaskGenerator::generateTestParameters(const ComponentSpec& component) const
{
    switch (component.type) {
    case ComponentType::RESISTOR:
        return generateResistorTestParameters(component);
    case ComponentType::CAPACITOR:
        return generateCapacitorTestParameters(component);
    case ComponentType::INDUCTOR:
        return generateInductorTestParameters(component);
    case ComponentType::DIODE:
        return generateDiodeTestParameters(component);
    case ComponentType::IC:
        return generateICTestParameters(component);
    default:
        return generateResistorTestParameters(component);
    }
}

QMap<QString, QVariant> WiringTaskGenerator::generateResistorTestParameters(const ComponentSpec& component) const
{
    QMap<QString, QVariant> params;
    
    params["test_method"] = "four_wire";
    params["test_voltage"] = 1.0;
    params["test_current"] = qMin(0.001, 1.0 / component.nominal_value);
    params["measurement_range"] = "auto";
    params["expected_value"] = component.nominal_value;
    params["tolerance"] = component.tolerance_percent;
    params["timeout"] = calculateTestTimeout(component);
    
    return params;
}

QMap<QString, QVariant> WiringTaskGenerator::generateCapacitorTestParameters(const ComponentSpec& component) const
{
    QMap<QString, QVariant> params;
    
    params["test_method"] = "ac_measurement";
    params["test_frequency"] = 1000.0;
    params["test_voltage"] = 1.0;
    params["measurement_range"] = "auto";
    params["expected_value"] = component.nominal_value;
    params["tolerance"] = component.tolerance_percent;
    params["timeout"] = calculateTestTimeout(component);
    
    return params;
}

QMap<QString, QVariant> WiringTaskGenerator::generateInductorTestParameters(const ComponentSpec& component) const
{
    QMap<QString, QVariant> params;
    
    params["test_method"] = "ac_measurement";
    params["test_frequency"] = 1000.0;
    params["test_voltage"] = 1.0;
    params["measurement_range"] = "auto";
    params["expected_value"] = component.nominal_value;
    params["tolerance"] = component.tolerance_percent;
    params["timeout"] = calculateTestTimeout(component);
    
    return params;
}

QMap<QString, QVariant> WiringTaskGenerator::generateDiodeTestParameters(const ComponentSpec& component) const
{
    QMap<QString, QVariant> params;
    
    params["test_method"] = "iv_curve";
    params["forward_voltage"] = 3.3;
    params["reverse_voltage"] = -5.0;
    params["current_limit"] = 0.01;
    params["measurement_range"] = "auto";
    params["timeout"] = calculateTestTimeout(component);
    
    return params;
}

QMap<QString, QVariant> WiringTaskGenerator::generateICTestParameters(const ComponentSpec& component) const
{
    QMap<QString, QVariant> params;
    
    params["test_method"] = "functional_test";
    params["supply_voltage"] = 5.0;
    params["logic_level_high"] = 3.3;
    params["logic_level_low"] = 0.0;
    params["clock_frequency"] = 10000.0;
    params["timeout"] = calculateTestTimeout(component) * 2; // IC测试通常需要更长时间
    
    return params;
}

bool WiringTaskGenerator::validateTaskConfiguration(const TestTask& task, QStringList& errors) const
{
    errors.clear();
    bool isValid = true;
    
    // 验证基本信息
    if (task.taskId.isEmpty()) {
        errors.append("任务ID不能为空");
        isValid = false;
    }
    
    if (task.taskName.isEmpty()) {
        errors.append("任务名称不能为空");
        isValid = false;
    }
    
    if (task.component.reference.isEmpty()) {
        errors.append("元件编号不能为空");
        isValid = false;
    }
      // 验证接线方案
    if (!validateWiringScheme(task.wiringScheme, errors, task.component.reference)) {
        isValid = false;
    }
    
    // 验证测试参数
    if (!validateTestParameters(task.testParameters, task.component.type, errors)) {
        isValid = false;
    }
    
    return isValid;
}

bool WiringTaskGenerator::validateWiringScheme(const WiringScheme& scheme, QStringList& errors) const
{
    if (scheme.schemeId.isEmpty()) {
        errors.append("接线方案ID不能为空");
        return false;
    }
    
    if (scheme.connections.isEmpty()) {
        errors.append("接线连接不能为空");
        return false;
    }
    
    // 使用资源管理器验证
    if (resourceManager_) {
        return resourceManager_->validateWiringScheme(scheme, errors);
    }
    
    return true;
}

bool WiringTaskGenerator::validateWiringScheme(const WiringScheme& scheme, QStringList& errors, const QString& currentUser) const
{
    if (scheme.schemeId.isEmpty()) {
        errors.append("接线方案ID不能为空");
        return false;
    }
    
    if (scheme.connections.isEmpty()) {
        errors.append("接线连接不能为空");
        return false;
    }
    
    // 使用资源管理器验证（传递当前用户信息）
    if (resourceManager_) {
        return resourceManager_->validateWiringScheme(scheme, errors, currentUser);
    }
    
    return true;
}

bool WiringTaskGenerator::validateTestParameters(const QMap<QString, QVariant>& parameters, ComponentType type, QStringList& errors) const
{
    // 检查必需的测试参数
    QStringList requiredParams;
    
    switch (type) {
    case ComponentType::RESISTOR:
        requiredParams << "test_method" << "test_voltage" << "expected_value";
        break;
    case ComponentType::CAPACITOR:
    case ComponentType::INDUCTOR:
        requiredParams << "test_method" << "test_frequency" << "expected_value";
        break;
    case ComponentType::DIODE:
        requiredParams << "test_method" << "forward_voltage";
        break;
    case ComponentType::IC:
        requiredParams << "test_method" << "supply_voltage";
        break;
    }
    
    for (const QString& param : requiredParams) {
        if (!parameters.contains(param)) {
            errors.append(QString("缺少必需的测试参数: %1").arg(param));
        }
    }
    
    return errors.isEmpty();
}

void WiringTaskGenerator::checkTaskSchedule()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QVector<TestTask> scheduledTasks = getScheduledTasks();
    
    for (const TestTask& task : scheduledTasks) {
        if (task.scheduledTime <= currentTime && task.status == "scheduled") {
            // 自动准备执行
            if (prepareTaskExecution(task.taskId)) {
                qDebug() << "自动执行调度任务:" << task.taskName;
            }
        }
    }
}

QVector<TestTask> WiringTaskGenerator::getScheduledTasks() const
{
    return getTasksByStatus("scheduled");
}

void WiringTaskGenerator::scheduleNextTask()
{
    // 实现任务调度逻辑
    // 这里可以添加优先级、资源可用性等考虑因素
}

QString WiringTaskGenerator::formatTaskId(const QString& prefix) const
{
    return QString("%1_%2").arg(prefix).arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));
}

QString WiringTaskGenerator::componentTypeToString(ComponentType type) const
{
    switch (type) {
    case ComponentType::RESISTOR: return "R";
    case ComponentType::CAPACITOR: return "C";
    case ComponentType::INDUCTOR: return "L";
    case ComponentType::DIODE: return "D";
    case ComponentType::IC: return "IC";
    default: return "UNKNOWN";
    }
}

double WiringTaskGenerator::calculateTestTimeout(const ComponentSpec& component) const
{
    // 根据元件类型和参数计算测试超时时间（秒）
    switch (component.type) {
    case ComponentType::RESISTOR:
        return 10.0; // 电阻测试通常很快
    case ComponentType::CAPACITOR:
        return 15.0; // 电容测试可能需要充放电时间
    case ComponentType::INDUCTOR:
        return 15.0; // 电感测试需要建立磁场
    case ComponentType::DIODE:
        return 20.0; // 二极管IV特性测试需要多个测试点
    case ComponentType::IC:
        return 60.0; // IC功能测试可能很复杂
    default:
        return 30.0;
    }
}

// 槽函数实现
void WiringTaskGenerator::onSchemeCreated(const QString& schemeId)
{
    qDebug() << "接收到方案创建信号:" << schemeId;
}

void WiringTaskGenerator::onResourcesAllocated(const QString& schemeId)
{
    qDebug() << "接收到资源分配信号:" << schemeId;
}

void WiringTaskGenerator::onResourcesReleased(const QString& schemeId)
{
    qDebug() << "接收到资源释放信号:" << schemeId;
}
