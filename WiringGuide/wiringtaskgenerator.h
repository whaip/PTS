#ifndef WIRINGTASKGENERATOR_H
#define WIRINGTASKGENERATOR_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QMap>
#include <QVector>
#include "portdefinitions.h"
#include "wiringresourcemanager.h"
#include "commontypes.h"

using namespace PortDefinitions;

// 测试任务结构
struct TestTask {
    QString taskId;
    QString taskName;
    ComponentSpec component;
    WiringScheme wiringScheme;
    QMap<QString, QVariant> testParameters;
    QDateTime createdTime;
    QDateTime scheduledTime;
    QString status; // "pending", "ready", "running", "completed", "failed"
    QString notes;
    
    TestTask() : createdTime(QDateTime::currentDateTime()) {}
};

Q_DECLARE_METATYPE(TestTask)

class WiringTaskGenerator : public QObject
{
    Q_OBJECT

public:
    explicit WiringTaskGenerator(WiringResourceManager* resourceManager, QObject *parent = nullptr);
    ~WiringTaskGenerator();

    // 任务生成
    QString generateTask(const ComponentSpec& component);
    QString generateTaskFromScheme(const WiringScheme& scheme, const ComponentSpec& component);
    QString generateBatchTasks(const QVector<ComponentSpec>& components);
    
    // 任务管理
    TestTask getTask(const QString& taskId) const;
    QVector<TestTask> getAllTasks() const;
    QVector<TestTask> getTasksByStatus(const QString& status) const;
    QVector<TestTask> getTasksByComponent(ComponentType componentType) const;
    
    // 任务状态管理
    bool updateTaskStatus(const QString& taskId, const QString& status);
    bool scheduleTask(const QString& taskId, const QDateTime& scheduledTime);
    bool cancelTask(const QString& taskId);
    bool deleteTask(const QString& taskId);
    
    // 任务验证
    bool validateTask(const QString& taskId, QStringList& errors) const;
    bool isTaskReady(const QString& taskId) const;
    
    // 任务执行准备
    bool prepareTaskExecution(const QString& taskId);
    bool finalizeTaskExecution(const QString& taskId, bool success, const QString& result = QString());
    
    // 批量操作
    QStringList generateTasksFromTemplate(ComponentType componentType, int count);
    bool executeBatchTasks(const QStringList& taskIds);
    
    // 统计信息
    int getTotalTasks() const;
    int getCompletedTasks() const;
    int getPendingTasks() const;
    QMap<QString, int> getTaskStatistics() const;

signals:
    void taskGenerated(const TestTask& task);
    void taskUpdated(const QString& taskId, const QString& status);
    void taskCompleted(const QString& taskId, bool success);
    void taskFailed(const QString& taskId, const QString& error);
    void batchTasksGenerated(const QStringList& taskIds);
    void errorOccurred(const QString& error);

private slots:
    void onSchemeCreated(const QString& schemeId);
    void onResourcesAllocated(const QString& schemeId);
    void onResourcesReleased(const QString& schemeId);
    void checkTaskSchedule();

private:
    WiringResourceManager* resourceManager_;
    QMap<QString, TestTask> tasks_;
    QTimer* scheduleTimer_;
    
    // 任务生成辅助方法
    TestTask createTaskFromComponent(const ComponentSpec& component);
    TestTask createTaskFromScheme(const WiringScheme& scheme, const ComponentSpec& component);
    QString generateUniqueTaskId() const;
    QString generateTaskName(const ComponentSpec& component) const;
    
    // 测试参数生成
    QMap<QString, QVariant> generateTestParameters(const ComponentSpec& component) const;
    QMap<QString, QVariant> generateResistorTestParameters(const ComponentSpec& component) const;
    QMap<QString, QVariant> generateCapacitorTestParameters(const ComponentSpec& component) const;
    QMap<QString, QVariant> generateInductorTestParameters(const ComponentSpec& component) const;
    QMap<QString, QVariant> generateDiodeTestParameters(const ComponentSpec& component) const;
    QMap<QString, QVariant> generateICTestParameters(const ComponentSpec& component) const;
      // 任务验证
    bool validateTaskConfiguration(const TestTask& task, QStringList& errors) const;
    bool validateWiringScheme(const WiringScheme& scheme, QStringList& errors) const;
    bool validateWiringScheme(const WiringScheme& scheme, QStringList& errors, const QString& currentUser) const;
    bool validateTestParameters(const QMap<QString, QVariant>& parameters, ComponentType type, QStringList& errors) const;
    
    // 任务调度
    void scheduleNextTask();
    QVector<TestTask> getScheduledTasks() const;
    
    // 工具方法
    QString formatTaskId(const QString& prefix) const;
    QString componentTypeToString(ComponentType type) const;
    double calculateTestTimeout(const ComponentSpec& component) const;
};

#endif // WIRINGTASKGENERATOR_H
