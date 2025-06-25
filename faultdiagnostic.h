#ifndef FAULTDIAGNOSTIC_H
#define FAULTDIAGNOSTIC_H

#include "devicemanager.h"
#include "commontypes.h"
#include "ComponentDiagnosticFramework/componentdiagnosticframework.h"
#include "ComponentDiagnosticFramework/componentdiagnosticmanager.h"
#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include <QStringList>
#include <memory>

// 前向声明，避免循环依赖
class SignalConfiguration;
class PortConfiguration;
class FaultAnalysis;
struct AnalysisResult;
struct TestSchemeSignals;
struct ComponentSpecs;
struct TestSequence;
class ComponentDiagnosticManager;



// 测量结果结构体
struct MeasurementResult {
    double primary_value;       // 主要测量值 (阻值/容值等)
    double voltage;            // 电压测量值
    double current;            // 电流测量值
    double power;              // 功耗
    double temperature;        // 温度
    double esr;               // 等效串联电阻
    double leakage_current;   // 漏电流
    bool valid;               // 测量是否有效
    QString error_message;    // 错误信息
    
    MeasurementResult() : primary_value(0), voltage(0), current(0), 
                         power(0), temperature(25.0), esr(0), 
                         leakage_current(0), valid(false) {}
};

// 诊断结果结构体
struct DiagnosticResult {
    enum TestResult {
        PASS = 0,
        FAIL = 1,
        ERROR = 2
    };
    QString testId;             // 测试ID
    QString componentType;      // 组件类型
    QString componentId;        // 组件标识
    TestResult result;          // 测试结果
    double healthScore;         // 健康度评分 (0-100)
    double confidence;          // 诊断置信度 (0-100)
    QStringList faultTypes;     // 故障类型列表
    QDateTime timestamp;        // 测试时间戳
    QString testEquipment;      // 测试设备信息
    double expectedValue;       // 期望值
    double tolerance;           // 容差
    QString notes;              // 备注信息
    MeasurementResult measurementData; // 测量数据
    
    DiagnosticResult() : result(ERROR), healthScore(0), confidence(0),
                        timestamp(QDateTime::currentDateTime()), expectedValue(0), tolerance(0) {}
};

// Forward declaration
struct TestSequence;

// === 状态管理枚举 ===
enum class WorkflowState {
    IDLE,
    CONFIGURING_SIGNALS,
    CONFIGURING_PORTS,
    SHOWING_WIRING_GUIDE,
    EXECUTING_TESTS,
    ANALYZING_RESULTS,
    COMPLETED,
    ERROR,
    TEST_COMPLETED,
    ANALYSIS_COMPLETED,
    WIRING_COMPLETED,
    WIRING_VERIFIED
};

// 使用commontypes.h中的TestData, TestConfiguration, PortMapping

/**
 * @brief 故障诊断主类（重构版本）
 * 
 * 基于ComponentDiagnosticFramework的重构版本，提供：
 * - 统一的组件诊断接口
 * - 支持多种组件类型
 * - 异步诊断能力
 * - 批量诊断功能
 * - 向后兼容的API
 */
class FaultDiagnostic : public QObject
{
    Q_OBJECT

public:
    explicit FaultDiagnostic(DeviceManager* deviceManager, QObject *parent = nullptr);
    virtual ~FaultDiagnostic();

    // === 主要诊断接口 ===
    
    /**
     * @brief 诊断单个组件（同步版本，保持向后兼容）
     * @param component 组件规格
     * @return 诊断结果
     */
    DiagnosticResult diagnoseComponent(const ComponentSpec& component);
    
    /**
     * @brief 诊断单个组件（异步版本）
     * @param component 组件规格
     * @return 任务ID
     */
    QString diagnoseComponentAsync(const ComponentSpec& component);
    
    /**
     * @brief 批量诊断组件
     * @param components 组件列表
     * @return 诊断结果列表
     */
    QVector<DiagnosticResult> diagnoseBatch(const QVector<ComponentSpec>& components);
    
    /**
     * @brief 异步批量诊断
     * @param components 组件列表
     * @return 批量任务ID
     */
    QString diagnoseBatchAsync(const QVector<ComponentSpec>& components);
    
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
     * @return 诊断结果
     */
    QVariant getTaskResult(const QString& taskId) const;
    
    /**
     * @brief 取消任务
     * @param taskId 任务ID
     * @return 是否成功取消
     */
    bool cancelTask(const QString& taskId);
    
    // === 配置和信息接口 ===
    
    /**
     * @brief 获取支持的组件类型
     * @return 组件类型列表
     */
    QStringList getSupportedComponentTypes() const;
    
    /**
     * @brief 检查是否支持指定组件类型
     * @param type 组件类型
     * @return 是否支持
     */
    bool isComponentTypeSupported(ComponentType type) const;
    
    /**
     * @brief 获取诊断统计信息
     * @return 统计信息
     */
    QMap<QString, QVariant> getDiagnosticStatistics() const;
    
    /**
     * @brief 设置全局超时时间
     * @param timeout 超时时间（毫秒）
     */
    void setGlobalTimeout(int timeout);
    
    // === 向后兼容接口 ===
    
    /**
     * @brief 兼容旧版本的模块化诊断方法
     */
    DiagnosticResult diagnoseComponentWithModules(const ComponentSpec& component);
    
    /**
     * @brief 兼容旧版本的同步测试执行
     */
    TestData executeSynchronousTest(const ComponentSpec& component);
    
    /**
     * @brief 兼容旧版本的同步分析执行
     */
    AnalysisResult executeSynchronousAnalysis(const QString& testId, const TestData& testData);
    
    // === 具体组件诊断方法（向后兼容） ===
    DiagnosticResult diagnoseResistor(const ComponentSpec& spec);
    DiagnosticResult diagnoseCapacitor(const ComponentSpec& spec);
    DiagnosticResult diagnoseInductor(const ComponentSpec& spec);
    DiagnosticResult diagnoseDiode(const ComponentSpec& spec);
    DiagnosticResult diagnoseIC(const ComponentSpec& spec);
    
    // === 测量方法（向后兼容） ===
    MeasurementResult measureResistance(int channel, double test_voltage, double nominalValue = 0.0);
    MeasurementResult measureCapacitance(int channel, double test_frequency);
    MeasurementResult measureInductance(int channel, double test_frequency);
    MeasurementResult measureDiodeCharacteristics(int channel);
    MeasurementResult measureICParameters(int channel, const ComponentSpec& spec);
    
    // === 辅助方法 ===
    bool checkComponentConnection(int channel);
    bool executeSyncMeasurement(const QString& syncGroup, const QStringList& deviceNames, 
                               const QList<ComponentSpec>& components);
    double calculateHealthScore(const DiagnosticResult& result);
    QString generateRecommendation(const DiagnosticResult& result);
    double calculateConfidence(const DiagnosticResult& result);
    bool isWithinTolerance(double nominal, double measured, double tolerance_percent);
    
    // === 数据转换方法（保持兼容性） ===
    ComponentSpec convertToLegacyComponentSpec(const TestSchemeSignals& scheme, const TestConfiguration& config);
    DiagnosticResult convertFromAnalysisResult(const AnalysisResult& analysisResult);
    TestData convertFromMeasurementResult(const MeasurementResult& measurement);
    DiagnosticResult convertFromComponentDiagnosticResult(const ComponentDiagnosticResult& result);
    ComponentDiagnosticResult convertToComponentDiagnosticResult(const DiagnosticResult& result);

signals:
    // === 诊断过程信号 ===
    void componentDiagnosisStarted(const QString& taskId, const QString& componentId);
    void componentDiagnosisProgress(const QString& taskId, const QString& componentId, int percentage);
    void componentDiagnosisCompleted(const QString& taskId, const QString& componentId, const DiagnosticResult& result);
    void componentDiagnosisError(const QString& taskId, const QString& componentId, const QString& error);
    
    // === 批量诊断信号 ===
    void batchDiagnosisStarted(const QString& taskId, int totalComponents);
    void batchDiagnosisProgress(const QString& taskId, int completedComponents, int totalComponents);
    void batchDiagnosisCompleted(const QString& taskId, const QVector<DiagnosticResult>& results);
    void batchDiagnosisError(const QString& taskId, const QString& error);
    
    // === 向后兼容信号 ===
    void diagnosisStarted(const ComponentSpec& component);
    void diagnosisCompleted(const DiagnosticResult& result);
    void diagnosticProgress(int percentage);
    void diagnosticCompleted(const DiagnosticResult& result);
    void progressUpdated(int percentage);
    void errorOccurred(const QString& error);

private slots:
    // 框架信号的转发槽
    void onFrameworkComponentDiagnosisStarted(const QString& taskId, const QString& componentId);
    void onFrameworkComponentDiagnosisProgress(const QString& taskId, const QString& componentId, int percentage);
    void onFrameworkComponentDiagnosisCompleted(const QString& taskId, const QString& componentId, const ComponentDiagnosticResult& result);
    void onFrameworkComponentDiagnosisError(const QString& taskId, const QString& componentId, const QString& error);
    void onFrameworkBatchDiagnosisStarted(const QString& taskId, int totalComponents);
    void onFrameworkBatchDiagnosisProgress(const QString& taskId, int completedComponents, int totalComponents);
    void onFrameworkBatchDiagnosisCompleted(const QString& taskId, const QVector<ComponentDiagnosticResult>& results);
    void onFrameworkBatchDiagnosisError(const QString& taskId, const QString& error);

private:
    DeviceManager* device_manager_;
    ComponentDiagnosticManager* diagnostic_manager_;
    
    // 初始化方法
    bool initializeDiagnosticFramework();
    void connectFrameworkSignals();
    
    // 错误处理
    void setError(const QString& error);
    QString last_error_;
    
    // 向后兼容的内部方法
    DiagnosticResult diagnoseComponentInternal(const ComponentSpec& component);
    
    // 旧版本的故障分析方法（保持兼容）
    FaultType analyzeResistorFault(const ComponentSpec& spec, const MeasurementResult& measurement);
    FaultType analyzeCapacitorFault(const ComponentSpec& spec, const MeasurementResult& measurement);
    FaultType analyzeDiodeFault(const ComponentSpec& spec, const MeasurementResult& measurement);
    FaultType analyzeICFault(const ComponentSpec& spec, const MeasurementResult& measurement);
      // 工具方法
    QString faultTypeToString(FaultType type) const;
    bool applyTestVoltage(int channel, double voltage);
    void waitForStabilization(int delay_ms = 10);
    double applyTemperatureCompensation(double value, double temp_coeff, double temperature);
    
    // 数据转换方法
    static ComponentDiagnosticResult convertToComponentResult(const DiagnosticResult& result);
    static DiagnosticResult convertFromComponentResult(const ComponentDiagnosticResult& result);
    
    // 错误结果创建方法
    static DiagnosticResult createErrorResult(const QString& componentType, const QString& errorMessage);
};

#endif
