#ifndef COMPONENTDIAGNOSTICMANAGER_H
#define COMPONENTDIAGNOSTICMANAGER_H

#include <QObject>
#include <QMap>
#include <QHash>
#include <QVector>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <memory>
#include "basecomponentdiagnostic.h"
#include "../commontypes.h"

// class DeviceManager;

// 前向声明
class ComponentDiagnosticManager;
class DiagnosticWorker;

// === 工作线程类 ===
class DiagnosticWorker : public QThread
{
    Q_OBJECT
    
public:
    DiagnosticWorker(ComponentDiagnosticManager* manager, const QString& taskId, QObject* parent = nullptr);
    
protected:
    void run() override;
    
signals:
    void taskProgress(const QString& taskId, const QString& componentId, int percentage);
    void taskCompleted(const QString& taskId);
    void taskError(const QString& taskId, const QString& error);
    
private:
    ComponentDiagnosticManager* manager_;
    QString taskId_;
};

/**
 * @brief 组件诊断管理器
 * 
 * 负责管理所有组件诊断器的生命周期，提供统一的诊断接口，
 * 支持批量诊断、并发诊断等高级功能
 */
class ComponentDiagnosticManager : public QObject
{
    Q_OBJECT
    
    // 友元声明
    friend class DiagnosticWorker;
    
public:
    explicit ComponentDiagnosticManager(DeviceManager* deviceManager, QObject* parent = nullptr);
    virtual ~ComponentDiagnosticManager();
    
    // === 初始化和配置 ===
    
    /**
     * @brief 初始化管理器
     * @return 是否成功初始化
     */
    bool initialize();
    
    /**
     * @brief 注册组件诊断器
     * @param type 组件类型
     * @param diagnostic 诊断器实例（管理器将获得所有权）
     */
    void registerDiagnostic(ComponentType type, BaseComponentDiagnostic* diagnostic);
    
    /**
     * @brief 获取支持的组件类型列表
     */
    QVector<ComponentType> getSupportedComponentTypes() const;
    
    /**
     * @brief 检查是否支持指定组件类型
     */
    bool isComponentTypeSupported(ComponentType type) const;
    
    // === 单个组件诊断接口 ===
    
    /**
     * @brief 诊断单个组件（同步）
     * @param component 组件规格
     * @return 诊断结果
     */
    ComponentDiagnosticResult diagnoseComponent(const ComponentSpec& component);
    
    /**
     * @brief 诊断单个组件（异步）
     * @param component 组件规格
     * @return 任务ID
     */
    QString diagnoseComponentAsync(const ComponentSpec& component);
    
    // === 批量诊断接口 ===
    
    /**
     * @brief 批量诊断组件（同步）
     * @param components 组件列表
     * @return 诊断结果列表
     */
    QVector<ComponentDiagnosticResult> diagnoseBatch(const QVector<ComponentSpec>& components);
    
    /**
     * @brief 批量诊断组件（异步）
     * @param components 组件列表
     * @return 批量任务ID
     */
    QString diagnoseBatchAsync(const QVector<ComponentSpec>& components);
    
    // === 并发诊断接口 ===
    
    /**
     * @brief 并发诊断组件（支持多线程）
     * @param components 组件列表
     * @param maxConcurrency 最大并发数
     * @return 批量任务ID
     */
    QString diagnoseConcurrently(const QVector<ComponentSpec>& components, int maxConcurrency = 4);
    
    // === 任务管理接口 ===
    
    /**
     * @brief 获取任务状态
     * @param taskId 任务ID
     * @return 任务状态信息
     */
    QMap<QString, QVariant> getTaskStatus(const QString& taskId) const;
    
    /**
     * @brief 获取任务结果
     * @param taskId 任务ID
     * @return 诊断结果（单个组件）或结果列表（批量）
     */
    QVariant getTaskResult(const QString& taskId) const;
    
    /**
     * @brief 取消任务
     * @param taskId 任务ID
     * @return 是否成功取消
     */
    bool cancelTask(const QString& taskId);
    
    /**
     * @brief 获取所有活动任务ID
     */
    QStringList getActiveTasks() const;
    
    /**
     * @brief 清理已完成的任务
     */
    void cleanupCompletedTasks();
    
    // === 配置和统计接口 ===
    
    /**
     * @brief 设置全局超时时间
     * @param timeout 超时时间（毫秒）
     */
    void setGlobalTimeout(int timeout);
    
    /**
     * @brief 获取诊断统计信息
     */
    QMap<QString, QVariant> getDiagnosticStatistics() const;
    
    /**
     * @brief 重置统计信息
     */
    void resetStatistics();
    
    /**
     * @brief 设置并发限制
     * @param maxConcurrency 最大并发任务数
     */
    void setMaxConcurrency(int maxConcurrency);
    
signals:
    // === 单个组件诊断信号 ===
    void componentDiagnosisStarted(const QString& taskId, const QString& componentId);
    void componentDiagnosisProgress(const QString& taskId, const QString& componentId, int percentage);
    void componentDiagnosisCompleted(const QString& taskId, const QString& componentId, const ComponentDiagnosticResult& result);
    void componentDiagnosisError(const QString& taskId, const QString& componentId, const QString& error);
    
    // === 批量诊断信号 ===
    void batchDiagnosisStarted(const QString& taskId, int totalComponents);
    void batchDiagnosisProgress(const QString& taskId, int completedComponents, int totalComponents);
    void batchDiagnosisCompleted(const QString& taskId, const QVector<ComponentDiagnosticResult>& results);
    void batchDiagnosisError(const QString& taskId, const QString& error);
    
    // === 任务管理信号 ===
    void taskStatusChanged(const QString& taskId, const QString& status);
    void taskCancelled(const QString& taskId);
    
    // === 系统信号 ===
    void diagnosticRegistered(ComponentType type, const QString& typeName);
    void statisticsUpdated(const QMap<QString, QVariant>& statistics);
    
private slots:
    // 诊断器信号的转发槽
    void onDiagnosisStarted(const QString& componentId);
    void onDiagnosisProgress(const QString& componentId, int percentage);
    void onDiagnosisCompleted(const QString& componentId, const ComponentDiagnosticResult& result);
    void onDiagnosisError(const QString& componentId, const QString& error);
    
    // 任务管理槽
    void onTaskTimeout();
    void onWorkerFinished();
    
private:
    // === 任务相关数据结构 ===
    
    enum class TaskType {
        SINGLE_COMPONENT,
        BATCH_SEQUENTIAL,
        BATCH_CONCURRENT
    };
    
    enum class TaskStatus {
        PENDING,
        RUNNING,
        COMPLETED,
        FAILED,
        CANCELLED,
        TIMEOUT
    };
    
    struct DiagnosticTask {
        QString taskId;
        TaskType type;
        TaskStatus status;
        QDateTime startTime;
        QDateTime endTime;
        int timeout;
        QString currentComponentId;
        
        // 单个组件任务
        ComponentSpec component;
        ComponentDiagnosticResult result;
        
        // 批量任务
        QVector<ComponentSpec> components;
        QVector<ComponentDiagnosticResult> results;
        int completedCount;
        int totalCount;
        int maxConcurrency;
        
        // 错误信息
        QString errorMessage;
        
        DiagnosticTask() : type(TaskType::SINGLE_COMPONENT), status(TaskStatus::PENDING),
                          timeout(30000), completedCount(0), totalCount(0), maxConcurrency(1) {}
    };
    
    // === 私有方法 ===
    
    // 任务管理
    QString generateTaskId() const;
    void createTask(const QString& taskId, TaskType type);
    DiagnosticTask* getTask(const QString& taskId);
    const DiagnosticTask* getTask(const QString& taskId) const;
    void updateTaskStatus(const QString& taskId, TaskStatus status);
    void startTaskTimer(const QString& taskId);
    void stopTaskTimer(const QString& taskId);
    
    // 诊断执行
    ComponentDiagnosticResult executeSingleDiagnosis(const ComponentSpec& component);
    void executeBatchDiagnosis(const QString& taskId);
    void executeConcurrentDiagnosis(const QString& taskId);
    
    // 统计更新
    void updateStatistics(const QString& componentType, bool success, int duration);
    
    // 信号连接
    void connectDiagnosticSignals(BaseComponentDiagnostic* diagnostic);
    void disconnectDiagnosticSignals(BaseComponentDiagnostic* diagnostic);
    
private:
    DeviceManager* deviceManager_;
    
    // 诊断器管理
    QMap<ComponentType, BaseComponentDiagnostic*> diagnostics_;
    
    // 任务管理
    QHash<QString, DiagnosticTask*> tasks_;
    QMap<QString, QTimer*> taskTimers_;
    QMap<QString, DiagnosticWorker*> workers_;
    mutable QMutex tasksMutex_;
    
    // 配置参数
    int globalTimeout_;
    int maxConcurrency_;
    
    // 统计信息
    mutable QMutex statisticsMutex_;
    QMap<QString, int> componentCounts_;
    QMap<QString, int> successCounts_;
    QMap<QString, int> failureCounts_;
    QMap<QString, qint64> totalDurations_;
    
    // 当前任务跟踪（用于信号转发）
    QString currentTaskId_;
    mutable QMutex currentTaskMutex_;
};

#endif // COMPONENTDIAGNOSTICMANAGER_H
