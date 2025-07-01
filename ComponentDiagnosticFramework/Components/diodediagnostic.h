#ifndef DIODEDIAGNOSTIC_H
#define DIODEDIAGNOSTIC_H

#include "../basecomponentdiagnostic.h"
#include <QMap>
#include <QVector>

/**
 * @brief 二极管诊断实现
 * 
 * 支持多种二极管类型的诊断：
 * - 正向压降测量（I-V曲线）
 * - 反向漏电流测量
 * - 结电容测量
 * - 开启电压检测
 * - 击穿电压测试
 * - 动态特性测试（开关时间等）
 * - 温度特性测试
 */
class DiodeDiagnostic : public BaseComponentDiagnostic
{
    Q_OBJECT
    
public:
    explicit DiodeDiagnostic(DeviceManager* deviceManager, QObject* parent = nullptr);
    virtual ~DiodeDiagnostic() = default;
    
    // === 基础接口重载 ===
    ComponentType getSupportedComponentType() const override;
    QString getComponentTypeName() const override;
    QVector<PortRequirement> getPortRequirements(const ComponentSpec& component) const override;
    QVector<WiringConnection> generateWiringScheme(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts) const override;
    ComponentTestConfig configureDataAcquisition(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts) const override;
    TestData executeDataAcquisition(const ComponentTestConfig& config) override;
    ComponentDiagnosticResult analyzeFaults(const ComponentSpec& component, const TestData& testData) override;
    
    // === 二极管特有配置 ===
    
    /**
     * @brief 设置正向测试参数
     * @param maxCurrent 最大正向电流 (A)
     * @param maxVoltage 最大正向电压 (V)
     * @param currentSteps 电流测试点数
     */
    void setForwardTestParams(double maxCurrent = 0.1, double maxVoltage = 2.0, int currentSteps = 50);
    
    /**
     * @brief 设置反向测试参数
     * @param maxVoltage 最大反向电压 (V)
     * @param leakageThreshold 漏电流阈值 (A)
     */
    void setReverseTestParams(double maxVoltage = -10.0, double leakageThreshold = 1e-6);
    
    /**
     * @brief 设置击穿测试参数
     * @param enable 是否启用击穿测试
     * @param maxVoltage 最大测试电压 (V)
     * @param currentLimit 电流限制 (A)
     */
    void setBreakdownTestParams(bool enable = false, double maxVoltage = -50.0, double currentLimit = 1e-3);
    
    /**
     * @brief 设置容量测试参数
     * @param frequency 测试频率 (Hz)
     * @param acVoltage AC电压幅度 (V)
     * @param dcBias DC偏置电压 (V)
     */
    void setCapacitanceTestParams(double frequency = 1000.0, double acVoltage = 0.1, double dcBias = 0.0);
    
    /**
     * @brief 设置动态测试参数
     * @param enable 是否启用动态测试
     * @param pulseWidth 脉冲宽度 (s)
     * @param pulseAmplitude 脉冲幅度 (V)
     */
    void setDynamicTestParams(bool enable = false, double pulseWidth = 1e-6, double pulseAmplitude = 1.0);
    
    /**
     * @brief 设置温度测试参数
     * @param enable 是否启用温度测试
     * @param temperatures 测试温度列表 (°C)
     */
    void setTemperatureTestParams(bool enable = false, const QVector<double>& temperatures = {25.0});
    
private:
    // === 测量方法 ===
    
    /**
     * @brief 测量I-V特性曲线
     * @param maxCurrent 最大电流
     * @param maxVoltage 最大电压
     * @param points 测试点数
     * @param reverse 是否反向测试
     * @return 电流-电压映射
     */
    QMap<double, double> measureIVCurve(double maxCurrent, double maxVoltage, int points, bool reverse = false);
    
    /**
     * @brief 测量正向压降
     * @param testCurrent 测试电流 (A)
     * @return 正向压降 (V)
     */
    double measureForwardDrop(double testCurrent);
    
    /**
     * @brief 测量反向漏电流
     * @param reverseVoltage 反向电压 (V)
     * @return 漏电流 (A)
     */
    double measureReverseCurrent(double reverseVoltage);
    
    /**
     * @brief 测量结电容
     * @param frequency 测试频率 (Hz)
     * @param dcBias DC偏置电压 (V)
     * @param acAmplitude AC信号幅度 (V)
     * @return 结电容 (F)
     */
    double measureJunctionCapacitance(double frequency, double dcBias = 0.0, double acAmplitude = 0.1);
    
    /**
     * @brief 检测开启电压
     * @param currentThreshold 电流阈值 (A)
     * @return 开启电压 (V)
     */
    double detectThresholdVoltage(double currentThreshold = 1e-6);
    
    /**
     * @brief 测量击穿电压
     * @param currentThreshold 击穿电流阈值 (A)
     * @return 击穿电压 (V)，正值表示未击穿
     */
    double measureBreakdownVoltage(double currentThreshold = 1e-3);
    
    /**
     * @brief 测量开关时间
     * @param pulseAmplitude 脉冲幅度 (V)
     * @param loadResistance 负载电阻 (Ω)
     * @return 开关时间映射 {rise_time, fall_time} (s)
     */
    QMap<QString, double> measureSwitchingTimes(double pulseAmplitude, double loadResistance = 1000.0);
    
    /**
     * @brief 测量理想因子
     * @param ivCurve I-V特性曲线
     * @return 理想因子 n
     */
    double calculateIdealityFactor(const QMap<double, double>& ivCurve);
    
    /**
     * @brief 计算饱和电流
     * @param ivCurve I-V特性曲线
     * @return 饱和电流 (A)
     */
    double calculateSaturationCurrent(const QMap<double, double>& ivCurve);
    
    /**
     * @brief 检测二极管类型
     * @param ivCurve I-V特性曲线
     * @return 二极管类型字符串
     */
    QString detectDiodeType(const QMap<double, double>& ivCurve);
    
    // === 分析方法 ===
    
    /**
     * @brief 分析I-V特性
     * @param ivCurve I-V特性曲线
     * @param reverse 是否为反向特性
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeIVCharacteristic(const QMap<double, double>& ivCurve, bool reverse = false);
    
    /**
     * @brief 分析正向特性
     * @param forwardData 正向测试数据
     * @param nominalVf 标称正向压降 (V)
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeForwardCharacteristic(const QMap<QString, QVariant>& forwardData, double nominalVf);
    
    /**
     * @brief 分析反向特性
     * @param reverseData 反向测试数据
     * @param maxLeakage 最大允许漏电流 (A)
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeReverseCharacteristic(const QMap<QString, QVariant>& reverseData, double maxLeakage);
    
    /**
     * @brief 分析结电容特性
     * @param capacitanceData 电容测试数据
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeCapacitanceCharacteristic(const QMap<QString, QVariant>& capacitanceData);
    
    /**
     * @brief 分析开关特性
     * @param switchingData 开关测试数据
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeSwitchingCharacteristic(const QMap<QString, QVariant>& switchingData);
    
    /**
     * @brief 检查二极管方向
     * @param forwardCurrent 正向电流
     * @param reverseCurrent 反向电流
     * @return 0=正确，1=反向，-1=无法确定
     */
    int checkDiodePolarity(double forwardCurrent, double reverseCurrent);
    
    /**
     * @brief 生成电流/电压序列
     * @param start 起始值
     * @param end 结束值
     * @param points 点数
     * @param logScale 是否使用对数刻度
     * @return 数值序列
     */
    QVector<double> generateTestSequence(double start, double end, int points, bool logScale = false);
    
    /**
     * @brief 拟合肖克利方程
     * @param ivCurve I-V特性曲线
     * @return 拟合参数 {Is, n, Rs}
     */
    QMap<QString, double> fitShockleyEquation(const QMap<double, double>& ivCurve);
    
    /**
     * @brief 计算串联电阻
     * @param ivCurve I-V特性曲线
     * @return 串联电阻 (Ω)
     */
    double calculateSeriesResistance(const QMap<double, double>& ivCurve);
    
private:
    // === 配置参数 ===
    
    // 正向测试参数
    double forwardMaxCurrent_;      // 最大正向电流 (A)
    double forwardMaxVoltage_;      // 最大正向电压 (V)
    int forwardCurrentSteps_;       // 正向电流测试点数
    
    // 反向测试参数
    double reverseMaxVoltage_;      // 最大反向电压 (V)
    double leakageThreshold_;       // 漏电流阈值 (A)
    
    // 击穿测试参数
    bool breakdownTestEnabled_;     // 是否启用击穿测试
    double breakdownMaxVoltage_;    // 击穿测试最大电压 (V)
    double breakdownCurrentLimit_;  // 击穿测试电流限制 (A)
    
    // 电容测试参数
    double capacitanceFrequency_;   // 电容测试频率 (Hz)
    double capacitanceACVoltage_;   // AC电压幅度 (V)
    double capacitanceDCBias_;      // DC偏置电压 (V)
    
    // 动态测试参数
    bool dynamicTestEnabled_;       // 是否启用动态测试
    double pulseDuration_;          // 脉冲宽度 (s)
    double pulseAmplitude_;         // 脉冲幅度 (V)
    
    // 温度测试参数
    bool temperatureTestEnabled_;   // 是否启用温度测试
    QVector<double> testTemperatures_; // 测试温度列表 (°C)
    
    // 默认值常量
    static const double DEFAULT_FORWARD_MAX_CURRENT;
    static const double DEFAULT_FORWARD_MAX_VOLTAGE;
    static const int DEFAULT_FORWARD_STEPS;
    static const double DEFAULT_REVERSE_MAX_VOLTAGE;
    static const double DEFAULT_LEAKAGE_THRESHOLD;
    static const double DEFAULT_BREAKDOWN_MAX_VOLTAGE;
    static const double DEFAULT_BREAKDOWN_CURRENT_LIMIT;
    static const double DEFAULT_CAPACITANCE_FREQUENCY;
    static const double DEFAULT_CAPACITANCE_AC_VOLTAGE;
    static const double DEFAULT_PULSE_DURATION;
    static const double DEFAULT_PULSE_AMPLITUDE;
    
    // 判断阈值
    static const double FORWARD_CURRENT_THRESHOLD;      // 正向导通电流阈值
    static const double REVERSE_CURRENT_THRESHOLD;      // 反向漏电流阈值
    static const double VF_TOLERANCE_PERCENT;           // 正向压降容差
    static const double IDEALITY_FACTOR_MIN;            // 最小理想因子
    static const double IDEALITY_FACTOR_MAX;            // 最大理想因子
};

#endif // DIODEDIAGNOSTIC_H
