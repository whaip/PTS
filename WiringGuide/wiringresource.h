#ifndef WIRINGRESOURCE_H
#define WIRINGRESOURCE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

// 资源类型枚举
enum class ResourceType {
    DIGITAL_OUTPUT,    // 数字输出端口
    POWER_OUTPUT,      // 电源输出端口
    ANALOG_OUTPUT,     // 模拟输出端口
    DIGITAL_INPUT,     // 数字输入端口
    ANALOG_INPUT,      // 模拟输入端口
    DMM               // 数字万用表
};

// 端口极性枚举
enum class Polarity {
    POSITIVE,
    NEGATIVE,
    NEUTRAL
};

// 连接点类型枚举
enum class ConnectionType {
    COMPONENT_PIN,     // 元件引脚
    TEST_POINT,        // 测试点
    GROUND_POINT,      // 接地点
    POWER_RAIL        // 电源轨
};

// 单个资源端口信息
struct ResourcePort {
    int port_id;                    // 端口ID
    ResourceType type;              // 资源类型
    QString name;                   // 端口名称
    QString description;            // 端口描述
    bool is_available;              // 是否可用
    double max_voltage;             // 最大电压
    double max_current;             // 最大电流
    QString device_name;            // 设备名称
    int device_channel;             // 设备通道
    
    QJsonObject toJson() const;
    static ResourcePort fromJson(const QJsonObject& json);
};

// 连接点信息
struct ConnectionPoint {
    QString point_id;               // 连接点ID
    ConnectionType type;            // 连接点类型
    QString label;                  // 标签
    QString description;            // 描述
    Polarity polarity;              // 极性
    double x_position;              // X坐标（用于图形显示）
    double y_position;              // Y坐标（用于图形显示）
    
    QJsonObject toJson() const;
    static ConnectionPoint fromJson(const QJsonObject& json);
};

// 接线连接信息
struct WiringConnection {
    QString connection_id;          // 连接ID
    ResourcePort resource_port;     // 资源端口
    ConnectionPoint target_point;   // 目标连接点
    QString wire_color;             // 导线颜色建议
    int priority;                   // 连接优先级
    QString instruction;            // 连接说明
    bool is_completed;              // 是否已完成
    
    QJsonObject toJson() const;
    static WiringConnection fromJson(const QJsonObject& json);
};

// 接线步骤
struct WiringStep {
    int step_number;                // 步骤编号
    QString title;                  // 步骤标题
    QString description;            // 详细描述
    QList<WiringConnection> connections; // 本步骤的连接
    QString warning;                // 警告信息
    QString image_path;             // 步骤图片路径
    int estimated_time_seconds;     // 预估完成时间（秒）
    bool is_critical;               // 是否为关键步骤
    
    QJsonObject toJson() const;
    static WiringStep fromJson(const QJsonObject& json);
};

// 完整接线配置
struct WiringConfiguration {
    QString config_id;              // 配置ID
    QString component_reference;    // 元件标识
    QString test_name;              // 测试名称
    QList<WiringStep> steps;        // 接线步骤
    QList<ResourcePort> used_resources; // 使用的资源
    double total_voltage;           // 总电压
    double total_current;           // 总电流
    QDateTime created_time;         // 创建时间
    QString notes;                  // 备注
    
    QJsonObject toJson() const;
    static WiringConfiguration fromJson(const QJsonObject& json);
};

// 资源管理器类
class WiringResourceManager : public QObject
{
    Q_OBJECT

public:
    explicit WiringResourceManager(QObject *parent = nullptr);
    ~WiringResourceManager();

    // 资源管理
    bool initializeResources();
    QList<ResourcePort> getAvailableResources(ResourceType type) const;
    QList<ResourcePort> getAllResources() const;
    bool allocateResource(const ResourcePort& port);
    bool releaseResource(int port_id);
    void releaseAllResources();
    
    // 资源查询
    bool isResourceAvailable(int port_id) const;
    ResourcePort getResourcePort(int port_id) const;
    QList<ResourcePort> getResourcesByType(ResourceType type) const;
    int getAvailableResourceCount(ResourceType type) const;
    
    // 接线配置生成
    WiringConfiguration generateWiringConfig(const QString& component_ref, 
                                            double voltage, 
                                            double current, 
                                            bool use_dmm = true);
    
    // 接线步骤生成
    QList<WiringStep> generateWiringSteps(const WiringConfiguration& config);
    
    // 验证功能
    bool validateWiringConfiguration(const WiringConfiguration& config, QStringList& errors);
    bool validateResourceAllocation(const QList<ResourcePort>& ports, QStringList& errors);
    
    // 数据持久化
    bool saveConfiguration(const WiringConfiguration& config, const QString& file_path);
    bool loadConfiguration(const QString& file_path, WiringConfiguration& config);
    bool saveResourceState(const QString& file_path);
    bool loadResourceState(const QString& file_path);

signals:
    void resourceAllocated(const ResourcePort& port);
    void resourceReleased(int port_id);
    void configurationGenerated(const WiringConfiguration& config);
    void errorOccurred(const QString& error);

private:
    void setupDefaultResources();
    ResourcePort createResourcePort(int id, ResourceType type, const QString& name, 
                                   const QString& device, int channel, 
                                   double max_v = 0.0, double max_i = 0.0);
    
    QList<ConnectionPoint> generateConnectionPoints(const QString& component_ref);
    QString getRecommendedWireColor(ResourceType type, Polarity polarity);
    
    QMap<int, ResourcePort> resources_;
    QList<int> allocated_resources_;
    QString resource_config_path_;
};

#endif // WIRINGRESOURCE_H
