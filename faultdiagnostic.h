#ifndef FAULTDIAGNOSTIC_H
#define FAULTDIAGNOSTIC_H

#include "devicemanager.h"
#include "commontypes.h"
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

class FaultDiagnostic : public QObject
{
    Q_OBJECT

public:
    explicit FaultDiagnostic(DeviceManager* deviceManager, QObject *parent = nullptr);
    ~FaultDiagnostic();

    // === 新的模块化接口 ===
      // 1. 信号配置接口
    SignalConfiguration* getSignalConfiguration() { return signalConfig_.get(); }
    QStringList getAvailableTestSchemes();
    QString getActiveTestScheme() const;
      // 2. 端口配置接口
    PortConfiguration* getPortConfiguration() { return portConfig_.get(); }
    bool isPortConfigured() const;
    TestConfiguration getCurrentTestConfiguration() const;
    
    // 3. 故障分析接口
    FaultAnalysis* getFaultAnalysis() { return faultAnalysis_.get(); }
    AnalysisResult getLastAnalysisResult() const;// === 向后兼容的传统接口 ===
    
    // 主要诊断接口
    DiagnosticResult diagnoseComponent(const ComponentSpec& component);
    QVector<DiagnosticResult> diagnoseSequence(const TestSequence& sequence);
    
    // 具体元件诊断方法
    DiagnosticResult diagnoseResistor(const ComponentSpec& spec);
    DiagnosticResult diagnoseCapacitor(const ComponentSpec& spec);
    DiagnosticResult diagnoseInductor(const ComponentSpec& spec);
    DiagnosticResult diagnoseDiode(const ComponentSpec& spec);
    DiagnosticResult diagnoseIC(const ComponentSpec& spec);
      // 连接性测试
    bool checkComponentConnection(int channel);
    
    // 同步测量操作（支持多设备协调）
    bool executeSyncMeasurement(const QString& syncGroup, const QStringList& deviceNames, 
                               const QList<ComponentSpec>& components);
    
    // 测试序列管理
    void loadTestSequence(const QString& filename);
    void saveTestSequence(const TestSequence& sequence, const QString& filename);
    
    // 结果分析
    double calculateHealthScore(const DiagnosticResult& result);
    QString generateRecommendation(const DiagnosticResult& result);
      // 获取支持的元件类型
    QStringList getSupportedComponentTypes() const;
    QString faultTypeToString(FaultType type) const;
    
    // === 数据转换方法（公有接口） ===
    ComponentSpec convertFromComponentSpecs(const ComponentSpecs& specs, const QString& componentType, const QString& reference = "AUTO");
    
    // === 新增模块化工作流程方法 ===
    bool executeFullDiagnosticWorkflow(const QString& componentType);
    bool executeCustomWorkflow(const TestSchemeSignals& scheme, const QStringList& portList);
    
    // 工作流程步骤方法
    bool step1_ConfigureSignals(const QString& componentType);
    bool step2_ConfigurePorts();
    bool step3_ExecuteTests();
    bool step4_AnalyzeResults();
    
    // 模块化诊断方法
    DiagnosticResult diagnoseComponentWithModules(const ComponentSpec& component);
      // 异步操作支持
    void diagnoseComponentAsync(const QString& componentId, const QString& testScheme, const ComponentSpec& component);
    void startFaultAnalysisAsync(const QString& testId, const TestData& testData);
    
    // 同步操作包装器
    TestData executeSynchronousTest(const ComponentSpec& component);
    AnalysisResult executeSynchronousAnalysis(const QString& testId, const TestData& testData);
    
    // 配置和状态管理
    bool setActiveTestScheme(const QString& schemeName);
    bool showPortConfigurationDialog();
    bool autoConfigurePortsFromComponentSpec(const ComponentSpec& component);
    
    // WiringGuide 集成
    bool startWiringGuide(const TestConfiguration& config);
    bool shouldShowWiringGuide(const TestConfiguration& config);
      // 模块初始化和连接
    bool initializeModules();
    void connectModuleSignals();
    
private:
    // 内部状态管理方法
    void setState(WorkflowState newState);

signals:
    // === 向后兼容信号 ===
    void diagnosticProgress(int percentage);
    void diagnosticCompleted(const DiagnosticResult& result);
    void sequenceCompleted(const QVector<DiagnosticResult>& results);
    void errorOccurred(const QString& error);
    void diagnosisStarted(const ComponentSpec& component);
    void diagnosisCompleted(const DiagnosticResult& result);
    void progressUpdated(int percentage);
    
    // 新增：接线引导相关信号
    void wiringRequired(const ComponentSpec& component);  // 需要接线
    void wiringCompleted(const ComponentSpec& component, const QString& configId);  // 接线完成
      // === 新增模块化工作流程信号 ===
    void workflowStarted(const QString& workflowType);
    void workflowCompleted(const QString& workflowType);
    void workflowError(const QString& workflowType, const QString& error);
    void workflowStepCompleted(int stepNumber, const QString& stepName);
    void workflowStateChanged(WorkflowState newState);
    
    // 测试执行信号
    void testExecutionStarted(const QString& testId);
    void testExecutionCompleted(const QString& testId, const TestData& testData);
    void testExecutionFailed(const QString& testId, const QString& error);
    void testExecutionProgress(const QString& testId, int percentage);
    
    // 分析相关信号
    void analysisStarted(const QString& testId);
    void analysisCompleted(const QString& testId, const AnalysisResult& result);
    void analysisProgress(const QString& testId, int percentage);
    void analysisFailed(const QString& testId, const QString& error);
      // 配置相关信号
    void signalConfigurationChanged();
    void portConfigurationCompleted(const TestConfiguration& config);
    void portConfigurationChanged(const QString& message);
    void portValidationFailed(const QString& error);
    void testSchemeChanged(const QString& scheme);
    
    // WiringGuide 信号
    void wiringGuideCompleted(const QString& testId);
    void wiringVerificationCompleted(bool success, const QString& message);

private:
    // === 核心组件 ===
    DeviceManager* device_manager_;
    
    // === 三大模块 ===
    std::unique_ptr<SignalConfiguration> signalConfig_;
    std::unique_ptr<PortConfiguration> portConfig_;
    std::unique_ptr<FaultAnalysis> faultAnalysis_;
      // === 状态管理 ===
    WorkflowState currentState_;
    QString currentWorkflowType_;
    QString activeTestScheme_;
    TestConfiguration currentTestConfig_;
    QString currentTestId_;
    TestData currentTestData_;
    AnalysisResult lastAnalysisResult_;
    
    // === 新增缺失的成员变量 ===
    ComponentSpec currentComponentSpecs_;  // 当前元件规格
    AnalysisResult currentAnalysisResult_;  // 当前分析结果
    QString currentTestScheme_;            // 当前测试方案名称
    
    // === WiringGuide 集成 ===
    bool wiringGuideEnabled_;
    QString wiringGuideConfigId_;
    
    // === 向后兼容支持 ===
    // Threading support
    bool use_threaded_manager_;
    DeviceManager* threaded_device_manager_;
    QString last_error_;
      // === 私有方法 ===
      // 向后兼容的私有方法
    void setError(const QString& error);
    
    // === 新增私有辅助方法 ===
    // 错误结果创建
    DiagnosticResult createErrorResult(const QString& testId, const QString& error);
    AnalysisResult createErrorAnalysisResult(const QString& testId, const QString& error);
    
    // 工作流程内部方法
    bool step3_ExecuteTest();    // 模块间数据转换
    ComponentSpec convertToLegacyComponentSpec(const TestSchemeSignals& scheme, const TestConfiguration& config);
    DiagnosticResult convertFromAnalysisResult(const AnalysisResult& analysisResult);
    TestData convertFromMeasurementResult(const MeasurementResult& measurement);
    
    // === 向后兼容的私有方法 ===
      // 基础测量方法
    MeasurementResult measureResistance(int channel, double test_voltage, double nominalValue);  // 带标称值的重载版本
    MeasurementResult measureCapacitance(int channel, double test_frequency = 1000.0);
    MeasurementResult measureInductance(int channel, double test_frequency = 10000.0);
    MeasurementResult measureDiodeCharacteristics(int channel);
    MeasurementResult measureICParameters(int channel, const ComponentSpec& spec);
    
    // 故障判断算法
    FaultType analyzeResistorFault(const ComponentSpec& spec, const MeasurementResult& measurement);
    FaultType analyzeCapacitorFault(const ComponentSpec& spec, const MeasurementResult& measurement);
    FaultType analyzeDiodeFault(const ComponentSpec& spec, const MeasurementResult& measurement);
    FaultType analyzeICFault(const ComponentSpec& spec, const MeasurementResult& measurement);
      // 辅助方法
    bool applyTestVoltage(int channel, double voltage);
    void waitForStabilization(int delay_ms = 10);
    double calculateTolerance(double nominal, double measured, double tolerance_percent);
    bool isWithinTolerance(double nominal, double measured, double tolerance_percent);
    QString selectOptimalResistanceRange(double nominalValue);  // 根据标称值选择最佳电阻量程
    
    // 温度补偿
    double applyTemperatureCompensation(double value, double temp_coeff, double temperature);
    
    // 统计分析
    double calculateConfidence(const DiagnosticResult& result);
    
    // Internal diagnosis method (doesn't emit signals)
    DiagnosticResult diagnoseComponentInternal(const ComponentSpec& component);

private slots:
    // 保留必要的槽方法用于内部使用
    void onTestCompleted();
    
    // === 新增模块信号处理槽 ===
    // 信号配置模块槽
    void onSignalConfigurationChanged();
    
    // 端口配置模块槽
    void onPortConfigurationCompleted(const TestConfiguration& config);
    void onPortValidationFailed(const QString& error);
    
    // 测试执行模块槽
    void onTestExecutionCompleted(const QString& testId, const TestData& testData);
    void onTestExecutionFailed(const QString& testId, const QString& error);
    void onTestExecutionProgress(const QString& testId, int percentage);
    
    // 故障分析模块槽
    void onAnalysisCompleted(const QString& testId, const AnalysisResult& result);
    void onAnalysisProgress(const QString& testId, int percentage);
    
    // WiringGuide 集成槽
    void onWiringGuideCompleted(const QString& testId);
    void onWiringVerificationCompleted(bool success, const QString& message);
};

#endif // FAULTDIAGNOSTIC_H
