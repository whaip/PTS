#ifndef PORTDEFINITIONS_H
#define PORTDEFINITIONS_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVector>
#include "commontypes.h"

// 设备端口定义
namespace PortDefinitions {

// JY5711 端口分配
struct JY5711Ports {
    static constexpr int ANALOG_OUTPUT_START = 0;
    static constexpr int ANALOG_OUTPUT_END = 15;
    static constexpr int DIGITAL_OUTPUT_START = 16;
    static constexpr int DIGITAL_OUTPUT_END = 27;
    static constexpr int POWER_OUTPUT_START = 28;
    static constexpr int POWER_OUTPUT_END = 31;
    static constexpr int TOTAL_PORTS = 32;
};

// JY5323 端口分配
struct JY5323Ports {
    static constexpr int ANALOG_INPUT_START = 0;
    static constexpr int ANALOG_INPUT_END = 31;
    static constexpr int TOTAL_PORTS = 32;
};

// JY5322 端口分配
struct JY5322Ports {
    static constexpr int DIGITAL_INPUT_START = 0;
    static constexpr int DIGITAL_INPUT_END = 15;
    static constexpr int TOTAL_PORTS = 16;
};

// 端口类型枚举
enum class PortType {
    ANALOG_OUTPUT,    // 模拟输出
    DIGITAL_OUTPUT,   // 数字输出
    POWER_OUTPUT,     // 电源输出
    ANALOG_INPUT,     // 模拟输入
    DIGITAL_INPUT     // 数字输入
};

// 端口信息结构
struct PortInfo {
    QString deviceName;
    int portNumber;
    PortType portType;
    QString description;
    double maxVoltage;
    double maxCurrent;
    bool isAvailable;
    QString allocatedTo;  // 分配给哪个测试/元件
    
    PortInfo() : portNumber(-1), portType(PortType::ANALOG_INPUT), 
                maxVoltage(0), maxCurrent(0), isAvailable(true) {}
                
    PortInfo(const QString& device, int port, PortType type, 
             const QString& desc, double voltage = 0, double current = 0)
        : deviceName(device), portNumber(port), portType(type), 
          description(desc), maxVoltage(voltage), maxCurrent(current),
          isAvailable(true) {}
};

// 连接信息结构
struct ConnectionInfo {
    PortInfo sourcePort;      // 源端口
    PortInfo targetPort;      // 目标端口
    QString wireColor;        // 线缆颜色
    QString instruction;      // 连接说明
    bool isCompleted;         // 是否已完成连接
    
    ConnectionInfo() : isCompleted(false) {}
};

// 接线方案结构
struct WiringScheme {
    QString schemeId;
    QString schemeName;
    ComponentType componentType;
    QString description;
    QVector<ConnectionInfo> connections;
    QMap<QString, QVariant> testParameters;
    
    WiringScheme() : componentType(ComponentType::RESISTOR) {}
};

} // namespace PortDefinitions

Q_DECLARE_METATYPE(PortDefinitions::PortInfo)
Q_DECLARE_METATYPE(PortDefinitions::ConnectionInfo)
Q_DECLARE_METATYPE(PortDefinitions::WiringScheme)

#endif // PORTDEFINITIONS_H
