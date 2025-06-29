#ifndef COMMONTYPES_H
#define COMMONTYPES_H

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QDateTime>
#include <QDebug>


// 设备操作命令枚举
enum class DeviceCommand {
    INITIALIZE,
    SHUTDOWN,
    CONFIGURE_CHANNEL,
    START_MEASUREMENT,
    STOP_MEASUREMENT,
    READ_DATA,
    WRITE_DATA,
    SYNC_TRIGGER,
    CALIBRATE
};

// 设备操作参数结构
struct DeviceOperation {
    DeviceCommand command;
    int channel = -1;
    double value = 0.0;
    double sampleRate = 0.0;
    int timeout = 5000;
    bool blocking = true;
    QString deviceName;
    QVariantMap parameters;

    // 同步相关
    QString syncGroup;
    int syncDelay = 0;  // 微秒级延迟

    // 数据采集相关参数
    int samplesPerChannel = 1000;       // 每通道采样点数
    QVector<int> channels;              // 多通道采集的通道列表
    QString acquisitionMode = "single"; // "single", "multi", "continuous"
    double inputRangeMin = -10.0;       // 输入范围最小值
    double inputRangeMax = 10.0;        // 输入范围最大值
    bool useCallback = false;           // 是否使用回调方式获取数据

    DeviceOperation(DeviceCommand cmd = DeviceCommand::INITIALIZE) : command(cmd) {}
};

// 设备操作结果
struct DeviceResult {
    bool success = false;
    double value = 0.0;
    QString error;
    QVariantMap data;
    QDateTime timestamp;
    DeviceCommand command = DeviceCommand::INITIALIZE;  // 添加command成员

    DeviceResult(bool ok = false) : success(ok), timestamp(QDateTime::currentDateTime()) {}
};

// 端口类型枚举
enum class PortType {
    ANALOG_OUTPUT,    // 模拟输出
    DIGITAL_OUTPUT,   // 数字输出
    POWER_OUTPUT,     // 电源输出
    ANALOG_INPUT,     // 模拟输入
    DIGITAL_INPUT,    // 数字输入
    DMM_MEASUREMENT   // 万用表测量端口
};

// 端口配置需求结构
struct PortRequirement {
    PortType portType;              // 端口类型
    int count;                      // 需要的端口数量
    QString description;            // 描述
    QMap<QString, QVariant> specs;  // 技术规格要求
    
    PortRequirement() : portType(PortType::ANALOG_INPUT), count(1) {}
    PortRequirement(PortType type, int cnt, const QString& desc = "")
        : portType(type), count(cnt), description(desc) {}
};

// 为PortRequirement提供哈希支持
inline uint qHash(const PortRequirement& key, uint seed = 0)
{
    uint h1 = qHash(static_cast<int>(key.portType), seed);
    uint h2 = qHash(key.count, seed);
    uint h3 = qHash(key.description, seed);
    uint h4 = 0;
    
    // 为specs生成哈希
    for (auto it = key.specs.begin(); it != key.specs.end(); ++it) {
        h4 ^= qHash(it.key(), seed) ^ qHash(it.value().toString(), seed);
    }
    
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
}

// 端口信息结构（增强版）
struct PortInfo {
    QString deviceName;
    int portNumber;
    PortType portType;
    QString description;
    double maxVoltage;
    double maxCurrent;
    double impedance;
    bool isAvailable;
    QString allocatedTo;        // 分配给哪个测试/元件
    QDateTime allocationTime;   // 分配时间
    QString allocationId;       // 分配ID（用于跟踪）
    int usageCount;            // 使用次数
    QMap<QString, QVariant> capabilities; // 端口能力描述

    PortInfo() : portNumber(-1), portType(PortType::ANALOG_INPUT),
                maxVoltage(0), maxCurrent(0), impedance(1e6),
                isAvailable(true), usageCount(0) {}

    PortInfo(const QString& device, int port, PortType type,
             const QString& desc, double voltage = 0, double current = 0, double imp = 1e6)
        : deviceName(device), portNumber(port), portType(type),
          description(desc), maxVoltage(voltage), maxCurrent(current), impedance(imp),
          isAvailable(true), usageCount(0) {}

    // 便利方法
    QString getPortKey() const { return QString("%1:%2").arg(deviceName).arg(portNumber); }
};

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
inline QString componentTypeToString(const ComponentType& type) {
    switch (type) {
        case ComponentType::RESISTOR: return "resistor";
        case ComponentType::CAPACITOR: return "capacitor";
        case ComponentType::INDUCTOR: return "inductor";
        case ComponentType::DIODE: return "diode";
        case ComponentType::TRANSISTOR: return "transistor";
        case ComponentType::IC: return "integrated_circuit";
        case ComponentType::UNKNOWN:
        default: return "unknown";
    }
}

inline ComponentType stringToComponentType(const QString& type) {
    if (type == "resistor") return ComponentType::RESISTOR;
    if (type == "capacitor") return ComponentType::CAPACITOR;
    if (type == "inductor") return ComponentType::INDUCTOR;
    if (type == "diode") return ComponentType::DIODE;
    if (type == "transistor") return ComponentType::TRANSISTOR;
    if (type == "integrated_circuit") return ComponentType::IC;
    if (type == "unknown") return ComponentType::UNKNOWN;
    return ComponentType::UNKNOWN;
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
    QMap<QString, QVariant> params;

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

    // 兼容性访问器 - id字段作为reference的别名
    const QString& id() const { return reference; }
    void setId(const QString& newId) { reference = newId; }
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
    QMap<QString, DeviceOperation> metadata;               // 元数据

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
