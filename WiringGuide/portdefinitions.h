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

// JY8902 万用表端口分配
struct JY8902Ports {
    static constexpr int DMM_CHANNEL_START = 0;
    static constexpr int DMM_CHANNEL_END = 1;  // 2线法测量：通道0和通道1
    static constexpr int TOTAL_PORTS = 2;
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

Q_DECLARE_METATYPE(PortInfo)
Q_DECLARE_METATYPE(PortDefinitions::ConnectionInfo)
Q_DECLARE_METATYPE(PortDefinitions::WiringScheme)

#endif // PORTDEFINITIONS_H
