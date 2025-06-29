#ifndef RESISTORDIAGNOSTIC_H
#define RESISTORDIAGNOSTIC_H

#include "../basecomponentdiagnostic.h"

/**
 * @brief 电阻器诊断类
 * 
 * 实现电阻器的完整诊断流程，包括：
 * - 端口配置（电流源输出 + 电压测量输入）
 * - 接线方案生成
 * - 数据采集配置
 * - 故障分析
 */
class ResistorDiagnostic : public BaseComponentDiagnostic
{
    Q_OBJECT
    
public:
    explicit ResistorDiagnostic(DeviceManager* deviceManager, QObject* parent = nullptr);
    virtual ~ResistorDiagnostic() = default;
    
    // 基础信息
    ComponentType getSupportedComponentType() const override;
    QString getComponentTypeName() const override;
    QStringList getSupportedModels() const override;
    
protected:
    // === 必须实现的虚函数 ===
    
    /**
     * @brief 获取电阻器测试的端口需求
     * 需要：1个电流输出端口 + 1个电压测量端口（或2个端口用于4线法测量）
     */
    QVector<PortRequirement> getPortRequirements(const ComponentSpec& component) const override;

    /**
     * @brief 获取电阻器测试所需的标准参数
     * 包括标称值、容差数等
     */
    QMap<QString, QVariant> getRequiredParameters() const override;

    /**
     * @brief 生成电阻器接线方案
     * 支持2线法和4线法测量
     */
    QVector<WiringConnection> generateWiringScheme(const ComponentSpec& component, 
                                                  const QVector<PortInfo>& allocatedPorts) const override;
    
    /**
     * @brief 配置电阻器数据采集
     * 设置电流源输出和电压测量参数
     */
    ComponentTestConfig configureDataAcquisition(const ComponentSpec& component,
                                                const QVector<PortInfo>& ports) const override;
    
    /**
     * @brief 执行电阻器数据采集
     * 执行I-V特性测量
     */
    TestData executeDataAcquisition(const ComponentTestConfig& config) override;
    
    /**
     * @brief 电阻器故障分析
     * 分析开路、短路、阻值偏差等故障
     */
    ComponentDiagnosticResult analyzeFaults(const ComponentSpec& component,
                                           const TestData& testData) override;
    
    // === 可选的重写函数 ===
    
    bool validateComponentSpec(const ComponentSpec& component) const override;
    bool preTestSetup(const ComponentSpec& component) override;
    void postTestCleanup() override;
    
private:
    // 电阻器特有的分析方法
    double calculateResistance(const QVector<double>& voltages, const QVector<double>& currents) const;
    bool isOpenCircuit(double resistance, double expectedResistance) const;
    bool isShortCircuit(double voltage, double current) const;
    bool isOutOfTolerance(double measured, double expected, double tolerancePercent) const;
    double calculateStability(const QVector<double>& measurements) const;
    double calculateTemperatureCoefficient(const QMap<double, double>& tempResistanceMap) const;
    double calculateMean(const QVector<double> &values);
    
    // 测试配置参数
    struct ResistorTestParams {
        double testCurrent;        // 测试电流 (A)
        double testVoltage;        // 最大电压 (V)
        int measurementPoints;     // 测量点数
        bool use4WireMethod;       // 是否使用4线法
        double settlingTime;       // 稳定时间 (ms)
        double maxVoltage;
        QString range;
        
        ResistorTestParams() : testCurrent(0.001), maxVoltage(10.0), 
                              measurementPoints(10), use4WireMethod(false), settlingTime(100) {}
    };
    
    ResistorTestParams calculateOptimalTestParams(const ComponentSpec& component) const;
    QString generateDiagnosticSummary(const ComponentSpec& component, 
                                     const ComponentDiagnosticResult& result) const;
};

#endif // RESISTORDIAGNOSTIC_H
