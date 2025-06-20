#ifndef FAULTDIAGNOSTIC_H
#define FAULTDIAGNOSTIC_H

#include "devicemanager.h"
#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include <QStringList>
#include <memory>

// 元件类型枚举
enum class ComponentType {
    RESISTOR,
    CAPACITOR,
    INDUCTOR,
    DIODE,
    TRANSISTOR,
    IC,
    UNKNOWN
};

// 故障类型枚举
enum class FaultType {
    COMPONENT_OK,
    SHORT_CIRCUIT,
    OPEN_CIRCUIT,
    OUT_OF_TOLERANCE,
    HIGH_ESR,
    HIGH_LEAKAGE,
    DIODE_SHORT,
    DIODE_OPEN,
    DIODE_LEAKAGE,
    IC_OVERCURRENT,
    IC_LOGIC_ERROR,
    NO_CONNECTION,
    UNKNOWN_FAULT
};

// 元件规格结构体
struct ComponentSpec {
    ComponentType type;
    QString reference;          // 器件标识 (如R1, C2, IC3)
    double nominal_value;       // 标称值
    double tolerance;           // 容差 (如0.05表示5%)
    double max_voltage;         // 最大工作电压
    double max_current;         // 最大工作电流
    int channel;               // 测试通道
    QString description;        // 描述信息
    // 新增：测试参数
    double test_voltage;        // 测试电压
    double test_current;        // 测试电流
    bool requires_dmm;          // 是否需要万用表
    QString connection_notes;   // 连接注意事项
    
    // 高级参数
    double temp_coefficient = 0.0;  // 温度系数
    double max_esr = 0.0;           // 最大ESR (电容)
    double max_leakage = 0.0;       // 最大漏电流
    
    ComponentSpec() : type(ComponentType::UNKNOWN), nominal_value(0), 
                     tolerance(0.05), max_voltage(0), max_current(0), channel(0) {}
};

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
struct ComponentSpecs;

class FaultDiagnostic : public QObject
{
    Q_OBJECT

public:
    explicit FaultDiagnostic(DeviceManager* deviceManager, QObject *parent = nullptr);
    ~FaultDiagnostic();// 主要诊断接口
    DiagnosticResult diagnoseComponent(const ComponentSpec& component);
    void diagnoseComponentAsync(const QString& componentType, const QString& testId, const ComponentSpecs& specs);
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

signals:
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

private:
    DeviceManager* device_manager_;
    
    // Threading support
    bool use_threaded_manager_;
    DeviceManager* threaded_device_manager_;
    
    // 基础测量方法
    MeasurementResult measureResistance(int channel, double test_voltage = 1.0);
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
    
    // 温度补偿
    double applyTemperatureCompensation(double value, double temp_coeff, double temperature);
    
    // 统计分析
    double calculateConfidence(const DiagnosticResult& result);
    
    QString last_error_;
    void setError(const QString& error);

    // Internal diagnosis method (doesn't emit signals)
    DiagnosticResult diagnoseComponentInternal(const ComponentSpec& component);
};

#endif // FAULTDIAGNOSTIC_H
