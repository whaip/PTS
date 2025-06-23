#ifndef COMMONTYPES_H
#define COMMONTYPES_H

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QDateTime>
#include <QDebug>

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

// 为ComponentType提供哈希支持，使其可以作为QMap的键
inline uint qHash(ComponentType key, uint seed = 0)
{
    return qHash(static_cast<int>(key), seed);
}

// ComponentType转换为字符串的辅助函数
inline QString componentTypeToString(ComponentType type) {    switch (type) {
        case ComponentType::RESISTOR: return "电阻";
        case ComponentType::CAPACITOR: return "电容";
        case ComponentType::INDUCTOR: return "电感";
        case ComponentType::DIODE: return "二极管";
        case ComponentType::TRANSISTOR: return "晶体管";
        case ComponentType::IC: return "集成电路";
        case ComponentType::UNKNOWN:
        default: return "未知";
    }
}

// QDebug operator for ComponentType
inline QDebug operator<<(QDebug debug, ComponentType type) {
    debug.nospace() << "ComponentType(" << componentTypeToString(type) << ")";
    return debug.space();
}

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

// 信号类型枚举
enum class SignalType {
    VOLTAGE_DC,          // 直流电压
    VOLTAGE_AC,          // 交流电压
    CURRENT_DC,          // 直流电流
    CURRENT_AC,          // 交流电流
    DIGITAL_OUTPUT,      // 数字输出
    DIGITAL_INPUT,       // 数字输入
    ANALOG_OUTPUT,       // 模拟输出
    ANALOG_INPUT,        // 模拟输入
    DMM_MEASUREMENT,     // 万用表测量
    RESISTANCE,          // 电阻测量
    CAPACITANCE,         // 电容测量
    INDUCTANCE,          // 电感测量
    FREQUENCY,           // 频率测量
    POWER               // 功率测量
};

// 端口配置结构
struct PortConfig {
    QString deviceName;           // 设备名称 (如"5711", "5320", "8902")
    int channel;                  // 通道号
    SignalType signalType;        // 信号类型
    QString signalName;           // 信号名称
    double amplitude;             // 信号幅值
    double frequency;             // 信号频率
    double phase;                 // 信号相位
    int samplesPerChannel;        // 每通道采样数
    bool isOutput;                // 是否为输出端口
    
    // 新增成员
    double rangeMin;              // 范围最小值
    double rangeMax;              // 范围最大值
    double sampleRate;            // 采样率
    
    PortConfig() : channel(0), signalType(SignalType::VOLTAGE_DC), 
                  amplitude(1.0), frequency(1000.0), phase(0.0),
                  samplesPerChannel(1000), isOutput(false),
                  rangeMin(-10.0), rangeMax(10.0), sampleRate(10000.0) {}
};

// ComponentSpec结构 - 元件规格定义
struct ComponentSpec {
    QString reference;           // 元件标识 (如 R1, C1, U1)
    QString name;               // 元件名称
    ComponentType type;         // 元件类型
    QString value;              // 元件值 (如 "100K", "10uF", "555")
    QString package;            // 封装类型 (如 "0603", "SOP8", "DIP14")
    QString tolerance;          // 容差 (如 "±5%", "±10%")
    QString voltage;            // 额定电压
    QString power;              // 额定功率
    QString manufacturer;       // 制造商
    QString partNumber;         // 器件型号
    QString description;        // 描述
    QStringList testPoints;     // 测试点列表
    QMap<QString, QString> parameters;  // 其他参数
    
    // 测试相关属性
    bool requiresTesting;       // 是否需要测试
    QString testCategory;       // 测试类别
    QStringList testMethods;    // 测试方法列表
    
    // 数值型参数 (用于故障诊断)
    double nominal_value = 0.0; // 标称值
    double tolerance_percent = 5.0; // 容差百分比
    
    // 测试配置参数
    int channel = 0;            // 测试通道
    double test_voltage = 5.0;  // 测试电压
    double test_current = 0.1;  // 测试电流
    double max_voltage = 10.0;  // 最大电压
    double max_current = 1.0;   // 最大电流
    double temp_coefficient = 0.0; // 温度系数
    double max_esr = 100.0;     // 最大等效串联电阻
    double max_leakage = 1e-6;  // 最大漏电流
    bool requires_dmm = false;  // 是否需要万用表
    
    ComponentSpec() : type(ComponentType::UNKNOWN), requiresTesting(true) {}
    
    // 便利构造函数
    ComponentSpec(const QString& ref, ComponentType t, const QString& val = "") 
        : reference(ref), type(t), value(val), requiresTesting(true) {}
};

// 端口映射结构
struct PortMapping {
    QString deviceName;                             // 设备名称
    int channel;                                    // 通道号
    QString signalType;                             // 信号类型
    QMap<QString, QVariant> parameters;             // 参数
    
    PortMapping() : channel(0) {}
};

// 测试配置结构体（统一定义）
struct TestConfiguration {
    QString testId;                                 // 测试ID
    QString testName;                               // 测试名称
    QString componentReference;                     // 元件标识
    QVector<PortConfig> outputPorts;               // 输出端口配置
    QVector<PortConfig> inputPorts;                // 输入端口配置
    QMap<QString, PortMapping> portMappings;       // 端口映射
    QMap<QString, QVariant> testParameters;        // 测试参数
    bool enableSynchronization;                     // 是否启用同步
    bool requiresSynchronization;                   // 是否需要同步（兼容性）
    QString syncGroupName;                          // 同步组名称
    int timeout;                                    // 超时时间(ms)
    int testTimeout;                               // 测试超时时间(ms)（兼容性）
    QStringList deviceNames;                        // 设备名称列表
    
    TestConfiguration() : enableSynchronization(false), requiresSynchronization(false),
                         timeout(30000), testTimeout(30000) {}
};

// 测试数据结构
struct TestData {
    QString testId;                                 // 测试ID
    QDateTime timestamp;                            // 时间戳
    QVector<QMap<QString, QVariant>> measurements;  // 测量数据集合
    bool valid;                                     // 数据有效性
    QString errorMessage;                           // 错误信息
    QMap<QString, QVariant> metadata;               // 元数据
    
    TestData() : valid(false) {}
};

// 前向声明，避免循环依赖
struct MeasurementData;

// 分析结果结构体
struct AnalysisResult {
    enum TestResult {
        PASS = 0,
        FAIL = 1,
        ERROR = 2
    };
    
    QString testId;                          // 测试ID
    QString componentType;                   // 组件类型
    QString componentId;                     // 组件标识
    QString componentReference;              // 组件参考标识
    QString faultType;                       // 故障类型
    TestResult result;                       // 测试结果
    double healthScore;                      // 健康度评分 (0-100)
    double confidence;                       // 分析置信度 (0-100)
    QDateTime timestamp;                     // 分析时间戳
    QString summary;                         // 分析摘要
    QString deviceInfo;                      // 设备信息
    QMap<QString, QVariant> details;         // 详细分析数据
    
    // 新增成员变量
    TestConfiguration testConfig;            // 测试配置
    QList<QMap<QString, QVariant>> measurementData;  // 测量数据
    QString testScheme;                      // 测试方案
    QString units;                           // 单位
    double expectedValue;                    // 期望值
    double tolerance;                        // 容差
    QMap<QString, QVariant> detailedResults; // 详细结果
    QStringList notes;                       // 注释列表
    double calculatedValue;                  // 计算值
    double deviation;                        // 偏差
    QStringList faultTypes;                  // 故障类型列表
    
    AnalysisResult() : result(ERROR), healthScore(0), confidence(0),
                      timestamp(QDateTime::currentDateTime()),
                      expectedValue(0.0), tolerance(0.0), 
                      calculatedValue(0.0), deviation(0.0) {}
};

#endif // COMMONTYPES_H
