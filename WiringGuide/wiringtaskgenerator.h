#ifndef WIRINGTASKGENERATOR_H
#define WIRINGTASKGENERATOR_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include "wiringresource.h"
#include "../devicemanager.h"
#include "../faultdiagnostic.h"

// 测试任务结构
struct TestTask {
    QString task_id;                    // 任务ID
    QString component_reference;        // 元件标识
    QString wiring_config_id;          // 接线配置ID
    QList<DeviceOperation> operations;  // 设备操作列表
    QDateTime created_time;             // 创建时间
    QString description;                // 任务描述
    bool is_executed;                   // 是否已执行
    QString execution_result;           // 执行结果
    
    QJsonObject toJson() const;
    static TestTask fromJson(const QJsonObject& json);
};

// 测试任务生成器
class WiringTaskGenerator : public QObject
{
    Q_OBJECT

public:
    explicit WiringTaskGenerator(QObject *parent = nullptr);
    ~WiringTaskGenerator();

    // 从接线配置生成测试任务
    TestTask generateTestTask(const WiringConfiguration& wiringConfig, 
                             const ComponentSpec& component);
    
    // 执行测试任务
    bool executeTestTask(const TestTask& task, DeviceManager* deviceManager);
    
    // 验证测试任务
    bool validateTestTask(const TestTask& task, QStringList& errors);
    
    // 保存和加载任务
    bool saveTestTask(const TestTask& task, const QString& filePath);
    bool loadTestTask(const QString& filePath, TestTask& task);
    
    // 获取任务执行状态
    QString getTaskExecutionStatus(const QString& taskId) const;

signals:
    void taskGenerated(const TestTask& task);
    void taskExecutionStarted(const QString& taskId);
    void taskExecutionCompleted(const QString& taskId, bool success);
    void errorOccurred(const QString& error);

private slots:
    void onDeviceOperationCompleted(const QString& deviceName, const DeviceResult& result);

private:
    // 根据元件类型生成特定的设备操作
    QList<DeviceOperation> generateResistorTestOperations(const ComponentSpec& component, 
                                                          const WiringConfiguration& config);
    QList<DeviceOperation> generateCapacitorTestOperations(const ComponentSpec& component, 
                                                           const WiringConfiguration& config);
    QList<DeviceOperation> generateInductorTestOperations(const ComponentSpec& component, 
                                                          const WiringConfiguration& config);
    QList<DeviceOperation> generateDiodeTestOperations(const ComponentSpec& component, 
                                                       const WiringConfiguration& config);
    
    // 创建基本设备操作
    DeviceOperation createPowerOutputOperation(const ResourcePort& powerPort, 
                                              double voltage, double current);
    DeviceOperation createDMMOperation(const ResourcePort& dmmPort, 
                                      const QString& measurement_type);
    DeviceOperation createDigitalIOOperation(const ResourcePort& ioPort, 
                                            bool output_value);
    DeviceOperation createAnalogIOOperation(const ResourcePort& ioPort, 
                                           double value);
    
    // 操作序列生成
    QList<DeviceOperation> createInitializationSequence(const WiringConfiguration& config);
    QList<DeviceOperation> createMeasurementSequence(const ComponentSpec& component, 
                                                     const WiringConfiguration& config);
    QList<DeviceOperation> createShutdownSequence(const WiringConfiguration& config);
    
    // 私有成员
    QMap<QString, TestTask> active_tasks_;
    QMap<QString, QString> task_execution_status_;
    QString task_storage_path_;
};

#endif // WIRINGTASKGENERATOR_H
