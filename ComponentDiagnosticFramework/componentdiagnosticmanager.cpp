#include "componentdiagnosticmanager.h"
#include <QUuid>
#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>
#include <QApplication>

// 默认配置常量
static const int DEFAULT_TIMEOUT = 30000;          // 30秒
static const int DEFAULT_MAX_CONCURRENCY = 4;      // 最大并发数
static const int CLEANUP_INTERVAL = 60000;         // 清理间隔：1分钟

ComponentDiagnosticManager::ComponentDiagnosticManager(DeviceManager* deviceManager, QObject* parent)
    : QObject(parent)
    , deviceManager_(deviceManager)
    , globalTimeout_(DEFAULT_TIMEOUT)
    , maxConcurrency_(DEFAULT_MAX_CONCURRENCY)
{
    // 启动定期清理定时器
    QTimer* cleanupTimer = new QTimer(this);
    connect(cleanupTimer, &QTimer::timeout, this, &ComponentDiagnosticManager::cleanupCompletedTasks);
    cleanupTimer->start(CLEANUP_INTERVAL);
}

ComponentDiagnosticManager::~ComponentDiagnosticManager()
{
    // 取消所有活动任务
    QStringList activeTasks = getActiveTasks();
    for (const QString& taskId : activeTasks) {
        cancelTask(taskId);
    }
    
    // 等待所有工作线程完成
    QMutexLocker locker(&tasksMutex_);
    for (auto it = workers_.begin(); it != workers_.end(); ++it) {
        if (it.value() && it.value()->isRunning()) {
            it.value()->quit();
            it.value()->wait(5000); // 等待5秒
        }
    }
    
    // 清理诊断器
    for (auto it = diagnostics_.begin(); it != diagnostics_.end(); ++it) {
        delete it.value();
    }
    diagnostics_.clear();
    
    // 清理任务
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
        delete it.value();
    }
    tasks_.clear();
}

bool ComponentDiagnosticManager::initialize()
{
    if (!deviceManager_) {
        qCritical() << "ComponentDiagnosticManager: DeviceManager is null";
        return false;
    }
    
    // 重置统计信息
    resetStatistics();
    
    qInfo() << "ComponentDiagnosticManager initialized successfully";
    return true;
}

void ComponentDiagnosticManager::registerDiagnostic(ComponentType type, BaseComponentDiagnostic* diagnostic)
{
    if (!diagnostic) {
        qWarning() << "ComponentDiagnosticManager: Cannot register null diagnostic";
        return;
    }
    
    // 设置诊断器的设备管理器
    diagnostic->setDeviceManager(deviceManager_);
    
    // 连接信号
    connectDiagnosticSignals(diagnostic);
    
    // 存储诊断器
    diagnostics_[type] = diagnostic;
    
    QString typeName = BaseComponentDiagnostic::componentTypeToString(type);
    qInfo() << "Registered diagnostic for component type:" << typeName;
    
    emit diagnosticRegistered(type, typeName);
}

QVector<ComponentType> ComponentDiagnosticManager::getSupportedComponentTypes() const
{
    QVector<ComponentType> types;
    for (auto it = diagnostics_.begin(); it != diagnostics_.end(); ++it) {
        types.append(it.key());
    }
    return types;
}

bool ComponentDiagnosticManager::isComponentTypeSupported(ComponentType type) const
{
    return diagnostics_.contains(type);
}

ComponentDiagnosticResult ComponentDiagnosticManager::diagnoseComponent(const ComponentSpec& component)
{
    QString taskId = generateTaskId();
    
    // 创建同步任务
    {
        QMutexLocker locker(&tasksMutex_);
        createTask(taskId, TaskType::SINGLE_COMPONENT);
        DiagnosticTask* task = getTask(taskId);
        if (task) {
            task->component = component;
        }
    }
    
    // 执行诊断
    ComponentDiagnosticResult result = executeSingleDiagnosis(component);
    
    // 更新任务状态
    {
        QMutexLocker locker(&tasksMutex_);
        DiagnosticTask* task = getTask(taskId);
        if (task) {
            task->result = result;
            task->status = result.isPassed ? TaskStatus::COMPLETED : TaskStatus::FAILED;
            task->endTime = QDateTime::currentDateTime();
        }
    }
    
    return result;
}

QString ComponentDiagnosticManager::diagnoseComponentAsync(const ComponentSpec& component)
{
    QString taskId = generateTaskId();
    
    // 创建异步任务
    {
        QMutexLocker locker(&tasksMutex_);
        createTask(taskId, TaskType::SINGLE_COMPONENT);
        DiagnosticTask* task = getTask(taskId);
        if (task) {
            task->component = component;
            task->timeout = globalTimeout_;
        }
    }
    
    // 启动工作线程
    DiagnosticWorker* worker = new DiagnosticWorker(this, taskId);
    workers_[taskId] = worker;
    
    // 连接工作线程信号
    connect(worker, &DiagnosticWorker::taskProgress, this, [this, taskId](const QString& tid, const QString& componentId, int percentage) {
        if (tid == taskId) {
            emit componentDiagnosisProgress(taskId, componentId, percentage);
        }
    });
    
    connect(worker, &DiagnosticWorker::taskCompleted, this, [this, taskId](const QString& tid) {
        if (tid == taskId) {
            onWorkerFinished();
        }
    });
    
    connect(worker, &DiagnosticWorker::taskError, this, [this, taskId](const QString& tid, const QString& error) {
        if (tid == taskId) {
            QMutexLocker locker(&tasksMutex_);
            DiagnosticTask* task = getTask(taskId);
            if (task) {
                task->status = TaskStatus::FAILED;
                task->errorMessage = error;
                task->endTime = QDateTime::currentDateTime();
            }
            emit componentDiagnosisError(taskId, task ? task->component.reference : "", error);
        }
    });
    
    // 启动超时定时器
    startTaskTimer(taskId);
    
    // 启动工作线程
    worker->start();
    
    emit componentDiagnosisStarted(taskId, component.id());
    
    return taskId;
}

QVector<ComponentDiagnosticResult> ComponentDiagnosticManager::diagnoseBatch(const QVector<ComponentSpec>& components)
{
    QString taskId = generateTaskId();
    
    // 创建批量任务
    {
        QMutexLocker locker(&tasksMutex_);
        createTask(taskId, TaskType::BATCH_SEQUENTIAL);
        DiagnosticTask* task = getTask(taskId);
        if (task) {
            task->components = components;
            task->totalCount = components.size();
            task->results.resize(components.size());
        }
    }
    
    QVector<ComponentDiagnosticResult> results;
    results.reserve(components.size());
    
    emit batchDiagnosisStarted(taskId, components.size());
    
    // 顺序执行诊断
    for (int i = 0; i < components.size(); ++i) {
        ComponentDiagnosticResult result = executeSingleDiagnosis(components[i]);
        results.append(result);
        
        // 更新任务进度
        {
            QMutexLocker locker(&tasksMutex_);
            DiagnosticTask* task = getTask(taskId);
            if (task) {
                task->results[i] = result;
                task->completedCount = i + 1;
            }
        }
        
        emit batchDiagnosisProgress(taskId, i + 1, components.size());
        
        // 检查是否被取消
        {
            QMutexLocker locker(&tasksMutex_);
            DiagnosticTask* task = getTask(taskId);
            if (task && task->status == TaskStatus::CANCELLED) {
                break;
            }
        }
    }
    
    // 更新任务状态
    {
        QMutexLocker locker(&tasksMutex_);
        DiagnosticTask* task = getTask(taskId);
        if (task) {
            task->status = TaskStatus::COMPLETED;
            task->endTime = QDateTime::currentDateTime();
        }
    }
    
    emit batchDiagnosisCompleted(taskId, results);
    
    return results;
}

QString ComponentDiagnosticManager::diagnoseBatchAsync(const QVector<ComponentSpec>& components)
{
    QString taskId = generateTaskId();
    
    // 创建异步批量任务
    {
        QMutexLocker locker(&tasksMutex_);
        createTask(taskId, TaskType::BATCH_SEQUENTIAL);
        DiagnosticTask* task = getTask(taskId);
        if (task) {
            task->components = components;
            task->totalCount = components.size();
            task->results.resize(components.size());
            task->timeout = globalTimeout_ * components.size(); // 按组件数量调整超时
        }
    }
    
    // 启动工作线程
    DiagnosticWorker* worker = new DiagnosticWorker(this, taskId);
    workers_[taskId] = worker;
    
    // 连接信号
    connect(worker, &DiagnosticWorker::taskCompleted, this, &ComponentDiagnosticManager::onWorkerFinished);
    connect(worker, &DiagnosticWorker::taskError, this, [this, taskId](const QString& tid, const QString& error) {
        if (tid == taskId) {
            emit batchDiagnosisError(taskId, error);
        }
    });
    
    // 启动超时定时器
    startTaskTimer(taskId);
    
    // 启动工作线程
    worker->start();
    
    emit batchDiagnosisStarted(taskId, components.size());
    
    return taskId;
}

QString ComponentDiagnosticManager::diagnoseConcurrently(const QVector<ComponentSpec>& components, int maxConcurrency)
{
    QString taskId = generateTaskId();
    
    // 创建并发批量任务
    {
        QMutexLocker locker(&tasksMutex_);
        createTask(taskId, TaskType::BATCH_CONCURRENT);
        DiagnosticTask* task = getTask(taskId);
        if (task) {
            task->components = components;
            task->totalCount = components.size();
            task->results.resize(components.size());
            task->maxConcurrency = qMin(maxConcurrency, maxConcurrency_);
            task->timeout = globalTimeout_ * components.size(); // 按组件数量调整超时
        }
    }
    
    // 启动工作线程
    DiagnosticWorker* worker = new DiagnosticWorker(this, taskId);
    workers_[taskId] = worker;
    
    // 连接信号
    connect(worker, &DiagnosticWorker::taskCompleted, this, &ComponentDiagnosticManager::onWorkerFinished);
    connect(worker, &DiagnosticWorker::taskError, this, [this, taskId](const QString& tid, const QString& error) {
        if (tid == taskId) {
            emit batchDiagnosisError(taskId, error);
        }
    });
    
    // 启动超时定时器
    startTaskTimer(taskId);
    
    // 启动工作线程
    worker->start();
    
    emit batchDiagnosisStarted(taskId, components.size());
    
    return taskId;
}

QMap<QString, QVariant> ComponentDiagnosticManager::getTaskStatus(const QString& taskId) const
{
    QMutexLocker locker(&tasksMutex_);
    QMap<QString, QVariant> status;
    
    const DiagnosticTask* task = getTask(taskId);
    if (!task) {
        status["exists"] = false;
        return status;
    }
    
    status["exists"] = true;
    status["taskId"] = task->taskId;
    status["type"] = static_cast<int>(task->type);
    status["status"] = static_cast<int>(task->status);
    status["startTime"] = task->startTime;
    status["endTime"] = task->endTime;
    status["currentComponent"] = task->currentComponentId;
    
    if (task->type != TaskType::SINGLE_COMPONENT) {
        status["completedCount"] = task->completedCount;
        status["totalCount"] = task->totalCount;
        status["progress"] = task->totalCount > 0 ? (task->completedCount * 100 / task->totalCount) : 0;
    }
    
    if (!task->errorMessage.isEmpty()) {
        status["error"] = task->errorMessage;
    }
    
    return status;
}

QVariant ComponentDiagnosticManager::getTaskResult(const QString& taskId) const
{
    QMutexLocker locker(&tasksMutex_);
    const DiagnosticTask* task = getTask(taskId);
    
    if (!task || task->status != TaskStatus::COMPLETED) {
        return QVariant();
    }
    
    if (task->type == TaskType::SINGLE_COMPONENT) {
        return QVariant::fromValue(task->result);
    } else {
        // 批量任务返回结果列表
        QVariantList resultList;
        for (const auto& result : task->results) {
            resultList.append(QVariant::fromValue(result));
        }
        return resultList;
    }
}

bool ComponentDiagnosticManager::cancelTask(const QString& taskId)
{
    QMutexLocker locker(&tasksMutex_);
    DiagnosticTask* task = getTask(taskId);
    
    if (!task) {
        return false;
    }
    
    if (task->status != TaskStatus::PENDING && task->status != TaskStatus::RUNNING) {
        return false; // 任务已完成或已取消
    }
    
    // 更新任务状态
    task->status = TaskStatus::CANCELLED;
    task->endTime = QDateTime::currentDateTime();
    
    // 停止定时器
    stopTaskTimer(taskId);
    
    // 终止工作线程
    if (workers_.contains(taskId)) {
        DiagnosticWorker* worker = workers_[taskId];
        if (worker && worker->isRunning()) {
            worker->quit();
            worker->wait(3000); // 等待3秒
        }
    }
    
    emit taskCancelled(taskId);
    emit taskStatusChanged(taskId, "cancelled");
    
    return true;
}

QStringList ComponentDiagnosticManager::getActiveTasks() const
{
    QMutexLocker locker(&tasksMutex_);
    QStringList activeTasks;
    
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
        const DiagnosticTask* task = it.value();
        if (task && (task->status == TaskStatus::PENDING || task->status == TaskStatus::RUNNING)) {
            activeTasks.append(it.key());
        }
    }
    
    return activeTasks;
}

void ComponentDiagnosticManager::cleanupCompletedTasks()
{
    QMutexLocker locker(&tasksMutex_);
    QDateTime cutoffTime = QDateTime::currentDateTime().addSecs(-300); // 保留5分钟内的任务
    
    QStringList tasksToRemove;
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
        const DiagnosticTask* task = it.value();
        if (task && task->status != TaskStatus::PENDING && task->status != TaskStatus::RUNNING) {
            if (task->endTime.isValid() && task->endTime < cutoffTime) {
                tasksToRemove.append(it.key());
            }
        }
    }
    
    for (const QString& taskId : tasksToRemove) {
        // 清理工作线程
        if (workers_.contains(taskId)) {
            DiagnosticWorker* worker = workers_[taskId];
            if (worker) {
                worker->deleteLater();
            }
            workers_.remove(taskId);
        }
        
        // 清理定时器
        if (taskTimers_.contains(taskId)) {
            QTimer* timer = taskTimers_[taskId];
            if (timer) {
                timer->deleteLater();
            }
            taskTimers_.remove(taskId);
        }
        
        // 清理任务
        DiagnosticTask* task = tasks_.value(taskId);
        if (task) {
            delete task;
        }
        tasks_.remove(taskId);
    }
    
    if (!tasksToRemove.isEmpty()) {
        qDebug() << "Cleaned up" << tasksToRemove.size() << "completed tasks";
    }
}

void ComponentDiagnosticManager::setGlobalTimeout(int timeout)
{
    globalTimeout_ = qMax(1000, timeout); // 最小1秒
    qInfo() << "Global timeout set to" << globalTimeout_ << "ms";
}

QMap<QString, QVariant> ComponentDiagnosticManager::getDiagnosticStatistics() const
{
    QMutexLocker locker(&statisticsMutex_);
    QMap<QString, QVariant> stats;
    
    QMap<QString, QVariant> counts;
    QMap<QString, QVariant> successRates;
    QMap<QString, QVariant> averageDurations;
    
    for (auto it = componentCounts_.begin(); it != componentCounts_.end(); ++it) {
        const QString& type = it.key();
        int total = it.value();
        int success = successCounts_.value(type, 0);
        int failure = failureCounts_.value(type, 0);
        qint64 totalDuration = totalDurations_.value(type, 0);
        
        counts[type] = total;
        successRates[type] = total > 0 ? (double(success) / total * 100.0) : 0.0;
        averageDurations[type] = total > 0 ? (totalDuration / total) : 0;
    }
    
    stats["componentCounts"] = counts;
    stats["successRates"] = successRates;
    stats["averageDurations"] = averageDurations;
    stats["lastUpdated"] = QDateTime::currentDateTime();
    
    return stats;
}

void ComponentDiagnosticManager::resetStatistics()
{
    {
        QMutexLocker locker(&statisticsMutex_);
        componentCounts_.clear();
        successCounts_.clear();
        failureCounts_.clear();
        totalDurations_.clear();
    }
    
    // 在锁外发射信号，避免死锁
    emit statisticsUpdated(getDiagnosticStatistics());
}

void ComponentDiagnosticManager::setMaxConcurrency(int maxConcurrency)
{
    maxConcurrency_ = qMax(1, maxConcurrency);
    qInfo() << "Max concurrency set to" << maxConcurrency_;
}

// === 私有槽函数实现 ===

void ComponentDiagnosticManager::onDiagnosisStarted(const QString& componentId)
{
    QMutexLocker locker(&currentTaskMutex_);
    if (!currentTaskId_.isEmpty()) {
        emit componentDiagnosisStarted(currentTaskId_, componentId);
    }
}

void ComponentDiagnosticManager::onDiagnosisProgress(const QString& componentId, int percentage)
{
    QMutexLocker locker(&currentTaskMutex_);
    if (!currentTaskId_.isEmpty()) {
        emit componentDiagnosisProgress(currentTaskId_, componentId, percentage);
    }
}

void ComponentDiagnosticManager::onDiagnosisCompleted(const QString& componentId, const ComponentDiagnosticResult& result)
{
    QMutexLocker locker(&currentTaskMutex_);
    if (!currentTaskId_.isEmpty()) {
        emit componentDiagnosisCompleted(currentTaskId_, componentId, result);
    }
}

void ComponentDiagnosticManager::onDiagnosisError(const QString& componentId, const QString& error)
{
    QMutexLocker locker(&currentTaskMutex_);
    if (!currentTaskId_.isEmpty()) {
        emit componentDiagnosisError(currentTaskId_, componentId, error);
    }
}

void ComponentDiagnosticManager::onTaskTimeout()
{
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    
    // 查找对应的任务ID
    QString taskId;
    for (auto it = taskTimers_.begin(); it != taskTimers_.end(); ++it) {
        if (it.value() == timer) {
            taskId = it.key();
            break;
        }
    }
    
    if (taskId.isEmpty()) return;
    
    // 取消超时任务
    {
        QMutexLocker locker(&tasksMutex_);
        DiagnosticTask* task = getTask(taskId);
        if (task && (task->status == TaskStatus::PENDING || task->status == TaskStatus::RUNNING)) {
            task->status = TaskStatus::TIMEOUT;
            task->errorMessage = "Task timeout";
            task->endTime = QDateTime::currentDateTime();
        }
    }
    
    // 终止工作线程
    if (workers_.contains(taskId)) {
        DiagnosticWorker* worker = workers_[taskId];
        if (worker && worker->isRunning()) {
            worker->quit();
            worker->wait(1000);
        }
    }
    
    emit taskStatusChanged(taskId, "timeout");
    qWarning() << "Task" << taskId << "timed out";
}

void ComponentDiagnosticManager::onWorkerFinished()
{
    DiagnosticWorker* worker = qobject_cast<DiagnosticWorker*>(sender());
    if (!worker) return;
    
    // 查找对应的任务ID
    QString taskId;
    for (auto it = workers_.begin(); it != workers_.end(); ++it) {
        if (it.value() == worker) {
            taskId = it.key();
            break;
        }
    }
    
    if (!taskId.isEmpty()) {
        // 停止定时器
        stopTaskTimer(taskId);
        
        // 获取任务结果并发射完成信号
        QMutexLocker locker(&tasksMutex_);
        DiagnosticTask* task = getTask(taskId);
        if (task) {
            if (task->type == TaskType::SINGLE_COMPONENT) {
                emit componentDiagnosisCompleted(taskId, task->component.id(), task->result);
            } else {
                emit batchDiagnosisCompleted(taskId, task->results);
            }
        }
    }
    
    // 延迟删除工作线程
    worker->deleteLater();
}

// === 私有方法实现 ===

QString ComponentDiagnosticManager::generateTaskId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ComponentDiagnosticManager::createTask(const QString& taskId, TaskType type)
{
    auto task = new DiagnosticTask();
    task->taskId = taskId;
    task->type = type;
    task->status = TaskStatus::PENDING;
    task->startTime = QDateTime::currentDateTime();
    task->timeout = globalTimeout_;
    
    tasks_[taskId] = task;
}

ComponentDiagnosticManager::DiagnosticTask* ComponentDiagnosticManager::getTask(const QString& taskId)
{
    auto it = tasks_.find(taskId);
    return it != tasks_.end() ? it.value() : nullptr;
}

const ComponentDiagnosticManager::DiagnosticTask* ComponentDiagnosticManager::getTask(const QString& taskId) const
{
    auto it = tasks_.find(taskId);
    return it != tasks_.end() ? it.value() : nullptr;
}

void ComponentDiagnosticManager::updateTaskStatus(const QString& taskId, TaskStatus status)
{
    DiagnosticTask* task = getTask(taskId);
    if (task) {
        task->status = status;
        emit taskStatusChanged(taskId, QString::number(static_cast<int>(status)));
    }
}

void ComponentDiagnosticManager::startTaskTimer(const QString& taskId)
{
    DiagnosticTask* task = getTask(taskId);
    if (!task) return;
    
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(task->timeout);
    connect(timer, &QTimer::timeout, this, &ComponentDiagnosticManager::onTaskTimeout);
    
    taskTimers_[taskId] = timer;
    timer->start();
}

void ComponentDiagnosticManager::stopTaskTimer(const QString& taskId)
{
    if (taskTimers_.contains(taskId)) {
        QTimer* timer = taskTimers_[taskId];
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
        taskTimers_.remove(taskId);
    }
}

ComponentDiagnosticResult ComponentDiagnosticManager::executeSingleDiagnosis(const ComponentSpec& component)
{
    ComponentDiagnosticResult result;
    result.componentId = component.id();
    result.componentType = BaseComponentDiagnostic::componentTypeToString(component.type);
    result.startTime = QDateTime::currentDateTime();
    
    // 检查是否支持该组件类型
    if (!isComponentTypeSupported(component.type)) {
        result.success = false;
        result.errorMessage = QString("Unsupported component type: %1").arg(result.componentType);
        result.endTime = QDateTime::currentDateTime();
        return result;
    }
    
    // 获取诊断器
    BaseComponentDiagnostic* diagnostic = diagnostics_[component.type];
    if (!diagnostic) {
        result.success = false;
        result.errorMessage = "Diagnostic not available";
        result.endTime = QDateTime::currentDateTime();
        return result;
    }
    
    // 设置当前任务上下文（用于信号转发）
    {
        QMutexLocker locker(&currentTaskMutex_);
        currentTaskId_ = result.componentId; // 临时使用组件ID作为任务上下文
    }
    
    try {
        // 执行诊断
        result = diagnostic->diagnose(component);
        
        // 更新统计信息
        QString typeName = BaseComponentDiagnostic::componentTypeToString(component.type);
        int duration = result.startTime.msecsTo(result.endTime);
        updateStatistics(typeName, result.success, duration);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = QString("Exception during diagnosis: %1").arg(e.what());
        result.endTime = QDateTime::currentDateTime();
        
        qCritical() << "Exception in diagnosis:" << e.what();
    }
    
    // 清除当前任务上下文
    {
        QMutexLocker locker(&currentTaskMutex_);
        currentTaskId_.clear();
    }
    
    return result;
}

void ComponentDiagnosticManager::executeBatchDiagnosis(const QString& taskId)
{
    QMutexLocker locker(&tasksMutex_);
    ComponentDiagnosticManager::DiagnosticTask* task = getTask(taskId);
    if (!task) return;
    
    locker.unlock();
    
    // 更新任务状态
    updateTaskStatus(taskId, ComponentDiagnosticManager::TaskStatus::RUNNING);
    
    // 设置当前任务上下文
    {
        QMutexLocker contextLocker(&currentTaskMutex_);
        currentTaskId_ = taskId;
    }
    
    // 顺序执行每个组件的诊断
    for (int i = 0; i < task->components.size(); ++i) {
        // 检查任务是否被取消
        {
            QMutexLocker taskLocker(&tasksMutex_);
            if (task->status == ComponentDiagnosticManager::TaskStatus::CANCELLED) {
                break;
            }
        }
        
        const ComponentSpec& component = task->components[i];
        task->currentComponentId = component.reference; // 使用reference而非id()
        
        // 执行单个组件诊断
        ComponentDiagnosticResult result = executeSingleDiagnosis(component);
        
        // 保存结果并更新进度
        {
            QMutexLocker taskLocker(&tasksMutex_);
            if (i < task->results.size()) {
                task->results[i] = result;
            }
            task->completedCount = i + 1;
        }
        
        emit batchDiagnosisProgress(taskId, i + 1, task->components.size());
    }
    
    // 更新最终状态
    {
        QMutexLocker taskLocker(&tasksMutex_);
        if (task->status != ComponentDiagnosticManager::TaskStatus::CANCELLED) {
            task->status = ComponentDiagnosticManager::TaskStatus::COMPLETED;
            task->endTime = QDateTime::currentDateTime();
        }
    }
    
    // 清除当前任务上下文
    {
        QMutexLocker contextLocker(&currentTaskMutex_);
        currentTaskId_.clear();
    }
}

void ComponentDiagnosticManager::executeConcurrentDiagnosis(const QString& taskId)
{
    // 并发诊断的实现比较复杂，这里提供一个简化版本
    // 实际实现可能需要使用线程池和更复杂的同步机制
    
    QMutexLocker locker(&tasksMutex_);
    ComponentDiagnosticManager::DiagnosticTask* task = getTask(taskId);
    if (!task) return;
    
    // 当前简化实现：退化为顺序执行
    // TODO: 实现真正的并发诊断
    locker.unlock();
    
    qWarning() << "Concurrent diagnosis not fully implemented, falling back to sequential";
    executeBatchDiagnosis(taskId);
}

void ComponentDiagnosticManager::updateStatistics(const QString& componentType, bool success, int duration)
{
    QMutexLocker locker(&statisticsMutex_);
    
    componentCounts_[componentType]++;
    if (success) {
        successCounts_[componentType]++;
    } else {
        failureCounts_[componentType]++;
    }
    totalDurations_[componentType] += duration;
    
    locker.unlock();
    
    // 异步发射统计更新信号（避免在锁内发射信号）
    QMetaObject::invokeMethod(this, [this]() {
        emit statisticsUpdated(getDiagnosticStatistics());
    }, Qt::QueuedConnection);
}

void ComponentDiagnosticManager::connectDiagnosticSignals(BaseComponentDiagnostic* diagnostic)
{
    if (!diagnostic) return;
    
    connect(diagnostic, &BaseComponentDiagnostic::diagnosisStarted,
            this, &ComponentDiagnosticManager::onDiagnosisStarted);
    connect(diagnostic, &BaseComponentDiagnostic::diagnosisProgress,
            this, &ComponentDiagnosticManager::onDiagnosisProgress);
    connect(diagnostic, &BaseComponentDiagnostic::diagnosisCompleted,
            this, &ComponentDiagnosticManager::onDiagnosisCompleted);
    connect(diagnostic, &BaseComponentDiagnostic::diagnosisError,
            this, &ComponentDiagnosticManager::onDiagnosisError);
}

void ComponentDiagnosticManager::disconnectDiagnosticSignals(BaseComponentDiagnostic* diagnostic)
{
    if (!diagnostic) return;
    
    disconnect(diagnostic, nullptr, this, nullptr);
}

// === DiagnosticWorker 实现 ===

DiagnosticWorker::DiagnosticWorker(ComponentDiagnosticManager* manager, const QString& taskId, QObject* parent)
    : QThread(parent), manager_(manager), taskId_(taskId)
{
}

void DiagnosticWorker::run()
{
    if (!manager_) {
        emit taskError(taskId_, "Manager is null");
        return;
    }
    
    try {
        QMutexLocker locker(&manager_->tasksMutex_);
        ComponentDiagnosticManager::DiagnosticTask* task = manager_->getTask(taskId_);
        if (!task) {
            emit taskError(taskId_, "Task not found");
            return;
        }
        
        ComponentDiagnosticManager::TaskType taskType = task->type;
        locker.unlock();
        
        // 根据任务类型执行相应的诊断
        if (taskType == ComponentDiagnosticManager::TaskType::SINGLE_COMPONENT) {
            QMutexLocker taskLocker(&manager_->tasksMutex_);
            if (task) {
                ComponentDiagnosticResult result = manager_->executeSingleDiagnosis(task->component);
                task->result = result;
                task->status = result.isPassed ? ComponentDiagnosticManager::TaskStatus::COMPLETED : ComponentDiagnosticManager::TaskStatus::FAILED;
                task->endTime = QDateTime::currentDateTime();
            }
        } else if (taskType == ComponentDiagnosticManager::TaskType::BATCH_SEQUENTIAL) {
            manager_->executeBatchDiagnosis(taskId_);
        } else if (taskType == ComponentDiagnosticManager::TaskType::BATCH_CONCURRENT) {
            manager_->executeConcurrentDiagnosis(taskId_);
        }
        
        emit taskCompleted(taskId_);
        
    } catch (const std::exception& e) {
        emit taskError(taskId_, QString("Worker exception: %1").arg(e.what()));
    } catch (...) {
        emit taskError(taskId_, "Unknown worker exception");
    }
}
