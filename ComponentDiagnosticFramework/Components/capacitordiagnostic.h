#ifndef CAPACITORDIAGNOSTIC_H
#define CAPACITORDIAGNOSTIC_H

#include "../basecomponentdiagnostic.h"

/**
 * @brief 电容器诊断类
 * 
 * 实现电容器的完整诊断流程，包括：
 * - 端口配置（交流激励 + 电压/电流测量）
 * - 接线方案生成
 * - 数据采集配置（容值、ESR、漏电流测量）
 * - 故障分析
 */
class CapacitorDiagnostic : public BaseComponentDiagnostic
{
    Q_OBJECT
    
public:
    explicit CapacitorDiagnostic(DeviceManager* deviceManager, QObject* parent = nullptr);
    virtual ~CapacitorDiagnostic() = default;
    
    // 基础信息
    ComponentType getSupportedComponentType() const override;
    QString getComponentTypeName() const override;
    QStringList getSupportedModels() const override;
    
protected:
    // === 必须实现的虚函数 ===
    
    /**
     * @brief 获取电容器测试的端口需求
     * 需要：1个交流信号输出 + 1个电压测量 + 1个电流测量端口
     */
    QVector<PortRequirement> getPortRequirements(const ComponentSpec& component) const override;
    
    /**
     * @brief 生成电容器接线方案
     * 支持容值测量和ESR测量
     */
    QVector<WiringConnection> generateWiringScheme(const ComponentSpec& component, 
                                                  const QVector<PortInfo>& allocatedPorts) const override;
    
    /**
     * @brief 配置电容器数据采集
     * 设置交流激励和测量参数
     */
    ComponentTestConfig configureDataAcquisition(const ComponentSpec& component,
                                                const QVector<PortInfo>& ports) const override;
    
    /**
     * @brief 执行电容器数据采集
     * 执行多频率阻抗特性测量
     */
    TestData executeDataAcquisition(const ComponentTestConfig& config) override;
    
    /**
     * @brief 电容器故障分析
     * 分析开路、短路、容值偏差、高ESR、高漏电流等故障
     */
    ComponentDiagnosticResult analyzeFaults(const ComponentSpec& component,
                                           const TestData& testData) override;
    
    // === 可选的重写函数 ===
    
    bool validateComponentSpec(const ComponentSpec& component) const override;
    bool preTestSetup(const ComponentSpec& component) override;
    void postTestCleanup() override;
    
private:
    // 电容器特有的分析方法
    double calculateCapacitance(double frequency, double voltage, double current, double phase) const;
    double calculateESR(double voltage, double current, double phase) const;
    bool isOpenCircuit(double capacitance, double expectedCapacitance) const;
    bool isShortCircuit(double capacitance) const;
    bool isHighESR(double esr, double maxESR) const;
    bool isHighLeakage(double leakageCurrent, double maxLeakage) const;
    double calculateQualityFactor(double capacitance, double esr, double frequency) const;
    double calculateStability(const QVector<double>& measurements) const;
    
    // 多频率测量结果
    struct FrequencyResponse {
        double frequency;        // 频率 (Hz)
        double impedance;        // 阻抗 (Ω)
        double phase;           // 相位 (度)
        double capacitance;     // 计算的容值 (F)
        double esr;            // 等效串联电阻 (Ω)
        
        FrequencyResponse() : frequency(0), impedance(0), phase(0), capacitance(0), esr(0) {}
    };
    
    // 测试配置参数
    struct CapacitorTestParams {
        QVector<double> testFrequencies; // 测试频率列表
        double testVoltage;              // 测试电压 (V)
        double dcBiasVoltage;           // 直流偏置电压 (V)
        int measurementPoints;          // 每个频率的测量点数
        double settlingTime;            // 稳定时间 (ms)
        bool measureLeakage;            // 是否测量漏电流
        double leakageTestVoltage;      // 漏电流测试电压 (V)
        
        CapacitorTestParams() : testVoltage(1.0), dcBiasVoltage(0.0), 
                               measurementPoints(10), settlingTime(500),
                               measureLeakage(true), leakageTestVoltage(10.0) {}
    };
    
    CapacitorTestParams calculateOptimalTestParams(const ComponentSpec& component) const;
    QVector<double> getOptimalTestFrequencies(double nominalCapacitance) const;
    QString generateDiagnosticSummary(const ComponentSpec& component, 
                                     const ComponentDiagnosticResult& result) const;
    
    // 复数运算辅助函数
    struct ComplexNumber {
        double real;
        double imag;
        
        ComplexNumber(double r = 0, double i = 0) : real(r), imag(i) {}
        double magnitude() const { return qSqrt(real*real + imag*imag); }
        double phase() const { return qAtan2(imag, real) * 180.0 / M_PI; }
    };
    
    ComplexNumber calculateComplexImpedance(double voltage, double current, 
                                          double voltagePhase, double currentPhase) const;
};

#endif // CAPACITORDIAGNOSTIC_H
