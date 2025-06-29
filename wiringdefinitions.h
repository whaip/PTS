#ifndef WIRINGDEFINITIONS_H
#define WIRINGDEFINITIONS_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QVariant>
#include <QDateTime>
#include "commontypes.h"

// 接线方案结构
struct WiringScheme {
    QString schemeId;              // 方案ID
    QString schemeName;            // 方案名称
    QString description;           // 描述
    ComponentType componentType;   // 适用的组件类型
    QVector<WiringConnection> connections; // 连接信息
    QMap<QString, QVariant> testParameters; // 测试参数
    QString createdBy;             // 创建者
    QDateTime createdTime;         // 创建时间
    double qualityScore;           // 质量评分
    bool isTemplate;               // 是否为模板
    
    WiringScheme() : componentType(ComponentType::UNKNOWN), qualityScore(0.0), isTemplate(false) {}
};

// 连接信息结构
struct ConnectionInfo {
    PortInfo sourcePort;           // 源端口
    PortInfo targetPort;           // 目标端口
    QString wireColor;             // 导线颜色
    QString instruction;           // 连接说明
    bool isCompleted;              // 是否已完成连接
    int priority;                  // 连接优先级
    
    ConnectionInfo() : isCompleted(false), priority(0) {}
};

// 端口分配策略
enum class AllocationStrategy {
    MINIMIZE_DEVICES,              // 最小化设备数量
    MINIMIZE_DISTANCE,             // 最小化连线距离
    LOAD_BALANCE,                  // 负载均衡
    OPTIMIZE_QUALITY               // 优化信号质量
};

// 接线任务结构
struct WiringTask {
    QString taskId;                // 任务ID
    QString taskName;              // 任务名称
    ComponentSpec component;       // 组件规格
    WiringScheme scheme;           // 接线方案
    QString status;                // 任务状态: "pending", "active", "completed", "failed"
    QDateTime createdTime;         // 创建时间
    QDateTime startTime;           // 开始时间
    QDateTime endTime;             // 结束时间
    QString userId;                // 用户ID
    QMap<QString, QVariant> metadata; // 元数据
    
    WiringTask() : status("pending") {}
};

// 端口需求结构（从basecomponentdiagnostic.h移过来，避免重复定义）
struct WiringPortRequirement {
    PortType portType;             // 端口类型
    int count;                     // 需要的端口数量
    QString description;           // 描述
    QMap<QString, QVariant> specs; // 技术规格要求
    bool isOptional;               // 是否可选
    
    WiringPortRequirement() : portType(PortType::ANALOG_INPUT), count(1), isOptional(false) {}
    WiringPortRequirement(PortType type, int cnt, const QString& desc = "", bool optional = false)
        : portType(type), count(cnt), description(desc), isOptional(optional) {}
};

#endif // WIRINGDEFINITIONS_H 