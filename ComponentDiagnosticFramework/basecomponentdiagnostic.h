#ifndef BASECOMPONENTDIAGNOSTIC_H
#define BASECOMPONENTDIAGNOSTIC_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QVariant>
#include <QDateTime>
#include "../commontypes.h"
#include "../WiringGuide/portdefinitions.h"

using namespace PortDefinitions;

// 前向声明
class DeviceManager;
struct TestData;
struct AnalysisResult;

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

// 接线连接定义
struct WiringConnection {
    QString componentPin;           // 元件引脚
    PortInfo targetPort;           // 目标端口
    QString wireColor;             // 导线颜色
    QString instruction;           // 接线说明
    bool isRequired;               // 是否必需
    
    WiringConnection() : isRequired(true) {}
};

// 测试配置参数
struct ComponentTestConfig {
    QString testName;              // 测试名称
    QVector<PortConfig> portConfigs; // 端口配置
    QMap<QString, QVariant> parameters; // 测试参数
    bool requiresSynchronization;   // 是否需要同步
    QString syncGroup;             // 同步组
    int timeout;                   // 超时时间(ms)
    
    ComponentTestConfig() : requiresSynchronization(false), timeout(30000) {}
};

// 故障诊断结果
struct ComponentDiagnosticResult {
    QString componentId;           // 元件ID
    QString componentType;         // 元件类型
    bool isPassed;                 // 是否通过
    bool success;                  // 兼容性字段，与isPassed相同
    double healthScore;            // 健康评分 (0-100)
    double confidence;             // 置信度 (0-1)
    QStringList faultTypes;        // 故障类型列表
    QMap<QString, double> measurements; // 测量值
    QMap<QString, QVariant> analysisData; // 分析数据
    QString summary;               // 总结
    QStringList recommendations;   // 建议
    QDateTime timestamp;           // 时间戳
    QString notes;                 // 备注
    
    // 新增字段以兼容现有代码
    QDateTime startTime;           // 开始时间
    QDateTime endTime;             // 结束时间
    QString errorMessage;          // 错误信息
    QString faultType;             // 单一故障类型（兼容性字段）
    QString faultDescription;      // 故障描述（兼容性字段）
    QMap<QString, QVariant> analysis; // 分析数据（兼容性字段）
    
    ComponentDiagnosticResult() : isPassed(false), success(false), healthScore(0.0), confidence(0.0),
                                 timestamp(QDateTime::currentDateTime()),
                                 startTime(QDateTime::currentDateTime()),
                                 endTime(QDateTime::currentDateTime()) {}
    
    // 设置isPassed时同时设置success
    void setResult(bool passed) {
        isPassed = passed;
        success = passed;
    }
};

/**
 * @brief 基础组件诊断抽象类
 * 
 * 所有具体的组件诊断类都必须继承此基类并实现虚函数
 * 提供了组件诊断的完整流程框架
 */
class BaseComponentDiagnostic : public QObject
{
    Q_OBJECT
    
public:
    explicit BaseComponentDiagnostic(DeviceManager* deviceManager, QObject* parent = nullptr);
    virtual ~BaseComponentDiagnostic() = default;
    
    // 主要诊断接口
    ComponentDiagnosticResult diagnoseComponent(const ComponentSpec& component);
    
    // 兼容性接口（为了兼容现有代码）
    ComponentDiagnosticResult diagnose(const ComponentSpec& component) {
        return diagnoseComponent(component);
    }
    
    // 获取组件信息
    virtual ComponentType getSupportedComponentType() const = 0;
    virtual QString getComponentTypeName() const = 0;
    virtual QStringList getSupportedModels() const { return QStringList(); }
    
    // 设备管理器操作
    void setDeviceManager(DeviceManager* deviceManager);
    
    // 静态工具方法
    static QString componentTypeToString(ComponentType type);
    
    // 静态工厂方法
    static BaseComponentDiagnostic* createDiagnostic(ComponentType type, DeviceManager* deviceManager, QObject* parent = nullptr);
    
protected:
    // === 需要子类实现的虚函数 ===
    
    /**
     * @brief 获取端口配置需求
     * @param component 组件规格
     * @return 端口需求列表
     */
    virtual QVector<PortRequirement> getPortRequirements(const ComponentSpec& component) const = 0;
    
    /**
     * @brief 生成接线方案
     * @param component 组件规格
     * @param allocatedPorts 分配的端口列表
     * @return 接线连接定义
     */
    virtual QVector<WiringConnection> generateWiringScheme(const ComponentSpec& component, 
                                                          const QVector<PortInfo>& allocatedPorts) const = 0;
    
    /**
     * @brief 配置数据采集参数
     * @param component 组件规格
     * @param ports 端口信息
     * @return 测试配置
     */
    virtual ComponentTestConfig configureDataAcquisition(const ComponentSpec& component,
                                                        const QVector<PortInfo>& ports) const = 0;
    
    /**
     * @brief 执行数据采集
     * @param config 测试配置
     * @return 测试数据
     */
    virtual TestData executeDataAcquisition(const ComponentTestConfig& config) = 0;
    
    /**
     * @brief 故障分析函数
     * @param component 组件规格
     * @param testData 测试数据
     * @return 诊断结果
     */
    virtual ComponentDiagnosticResult analyzeFaults(const ComponentSpec& component,
                                                   const TestData& testData) = 0;
    
    // === 可选的虚函数（有默认实现） ===
    
    /**
     * @brief 验证组件规格
     * @param component 组件规格
     * @return 是否有效
     */
    virtual bool validateComponentSpec(const ComponentSpec& component) const;
    
    /**
     * @brief 预处理操作（在测试前）
     * @param component 组件规格
     * @return 是否成功
     */
    virtual bool preTestSetup(const ComponentSpec& component) { Q_UNUSED(component); return true; }
    
    /**
     * @brief 后处理操作（在测试后）
     */
    virtual void postTestCleanup() {}
    
    /**
     * @brief 校验测试数据
     * @param testData 测试数据
     * @return 是否有效
     */
    virtual bool validateTestData(const TestData& testData) const;
    
    /**
     * @brief 生成测试报告
     * @param result 诊断结果
     * @return 报告内容
     */
    virtual QString generateTestReport(const ComponentDiagnosticResult& result) const;
    
protected:
    // 辅助方法
    QString formatValue(double value, const QString& unit = "") const;
    QString formatPercentage(double value) const;
    bool isWithinTolerance(double measured, double expected, double tolerancePercent) const;
    double calculateDeviation(double measured, double expected) const;
    double calculateHealthScore(const QMap<QString, double>& measurements,
                               const ComponentSpec& component) const;
    
    // 设备管理器访问
    DeviceManager* getDeviceManager() const { return deviceManager_; }
    
    // 日志方法
    void logInfo(const QString& message) const;
    void logWarning(const QString& message) const;
    void logError(const QString& message) const;
    
signals:
    // 诊断过程信号
    void diagnosisStarted(const QString& componentId);
    void diagnosisProgress(const QString& componentId, int percentage);
    void diagnosisCompleted(const QString& componentId, const ComponentDiagnosticResult& result);
    void diagnosisError(const QString& componentId, const QString& error);
    
    // 接线信号
    void wiringRequired(const QString& componentId, const QVector<WiringConnection>& connections);
    void wiringCompleted(const QString& componentId);
    
    // 测试信号
    void testStarted(const QString& componentId);
    void testCompleted(const QString& componentId, const TestData& data);
    
private:
    DeviceManager* deviceManager_;
    QString currentComponentId_;
    
    // 诊断流程的私有方法
    bool allocatePorts(const ComponentSpec& component, QVector<PortInfo>& allocatedPorts);
    bool setupWiring(const ComponentSpec& component, const QVector<PortInfo>& ports);
    void releasePorts(const QVector<PortInfo>& ports);
};

// 组件诊断工厂类
class ComponentDiagnosticFactory
{
public:
    // 注册诊断器
    template<typename T>
    static void registerDiagnostic(ComponentType type) {
        creators_[type] = []() -> BaseComponentDiagnostic* {
            return new T();
        };
    }
    
    // 创建诊断器
    static BaseComponentDiagnostic* create(ComponentType type, DeviceManager* deviceManager, QObject* parent = nullptr);
    
    // 获取所有支持的组件类型
    static QVector<ComponentType> getSupportedTypes();
    
private:
    using CreatorFunc = std::function<BaseComponentDiagnostic*()>;
    static QMap<ComponentType, CreatorFunc> creators_;
};

#endif // BASECOMPONENTDIAGNOSTIC_H
