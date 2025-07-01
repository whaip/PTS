#ifndef INDUCTORDIAGNOSTIC_H
#define INDUCTORDIAGNOSTIC_H

#include "../basecomponentdiagnostic.h"
#include <QMap>
#include <QVector>

/**
 * @brief 电感器诊断实现
 * 
 * 支持多种电感器类型的诊断：
 * - 电感量测量（多频率扫描）
 * - 品质因子(Q值)测量
 * - 等效串联电阻(ESR)测量
 * - 谐振频率检测
 * - 开路/短路检测
 * - 饱和特性测试
 */
class InductorDiagnostic : public BaseComponentDiagnostic
{
    Q_OBJECT
    
public:
    explicit InductorDiagnostic(DeviceManager* deviceManager, QObject* parent = nullptr);
    virtual ~InductorDiagnostic() = default;
    
    // === 基础接口重载 ===
    ComponentType getSupportedComponentType() const override;
    QString getComponentTypeName() const override;
    QVector<PortRequirement> getPortRequirements(const ComponentSpec& component) const override;
    QVector<WiringConnection> generateWiringScheme(const ComponentSpec& component,
                                                  const QVector<PortInfo>& allocatedPorts) const override;
    ComponentTestConfig configureDataAcquisition(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts) const override;
    TestData executeDataAcquisition(const ComponentTestConfig& config) override;
    ComponentDiagnosticResult analyzeFaults(const ComponentSpec& component, 
                                           const TestData& testData) override;
    
    // === 电感器特有配置 ===
    
    /**
     * @brief 设置测试频率范围
     * @param startFreq 起始频率 (Hz)
     * @param endFreq 结束频率 (Hz)
     * @param points 频率点数
     */
    void setFrequencyRange(double startFreq, double endFreq, int points = 50);
    
    /**
     * @brief 设置测试电流范围
     * @param minCurrent 最小测试电流 (A)
     * @param maxCurrent 最大测试电流 (A)
     */
    void setCurrentRange(double minCurrent, double maxCurrent);
    
    /**
     * @brief 设置饱和测试参数
     * @param enable 是否启用饱和测试
     * @param maxCurrent 最大测试电流 (A)
     * @param stepSize 电流步进 (A)
     */
    void setSaturationTestParams(bool enable, double maxCurrent = 1.0, double stepSize = 0.01);
    
    /**
     * @brief 设置容差检查参数
     * @param tolerance 容差 (%)
     * @param qFactorMin 最小Q值
     */
    void setToleranceParams(double tolerance = 10.0, double qFactorMin = 10.0);
    
private:
    // === 测量方法 ===
    
    /**
     * @brief 测量电感量（单频率）
     * @param frequency 测试频率 (Hz)
     * @param current 测试电流 (A)
     * @return 电感量 (H)
     */
    double measureInductance(double frequency, double current = 0.001);
    
    /**
     * @brief 多频率电感量扫描
     * @param frequencies 频率列表
     * @param current 测试电流
     * @return 频率-电感量映射
     */
    QMap<double, double> measureInductanceSpectrum(const QVector<double>& frequencies, double current = 0.001);
    
    /**
     * @brief 测量品质因子
     * @param frequency 测试频率 (Hz)
     * @param current 测试电流 (A)
     * @return Q值
     */
    double measureQualityFactor(double frequency, double current = 0.001);
    
    /**
     * @brief 测量等效串联电阻
     * @param frequency 测试频率 (Hz)
     * @param current 测试电流 (A)
     * @return ESR (Ω)
     */
    double measureESR(double frequency, double current = 0.001);
    
    /**
     * @brief 检测谐振频率
     * @return 谐振频率 (Hz)，-1表示未检测到
     */
    double detectResonantFrequency();
    
    /**
     * @brief 饱和特性测试
     * @param frequency 测试频率 (Hz)
     * @return 电流-电感量映射
     */
    QMap<double, double> measureSaturationCharacteristic(double frequency = 1000.0);
    
    /**
     * @brief 检测开路/短路故障
     * @return 故障类型：0=正常，1=开路，2=短路
     */
    int detectOpenShortFault();
    
    // === 分析方法 ===
    
    /**
     * @brief 分析电感量测量结果
     * @param nominalValue 标称值 (H)
     * @param measuredValues 测量值映射
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeInductanceValues(double nominalValue, 
                                                   const QMap<double, double>& measuredValues);
    
    /**
     * @brief 分析Q值特性
     * @param qValues Q值映射
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeQualityFactor(const QMap<double, double>& qValues);
    
    /**
     * @brief 分析频率特性
     * @param inductanceSpectrum 频率-电感量谱
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeFrequencyCharacteristic(const QMap<double, double>& inductanceSpectrum);
    
    /**
     * @brief 分析饱和特性
     * @param saturationData 饱和测试数据
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeSaturationCharacteristic(const QMap<double, double>& saturationData);
    
    /**
     * @brief 生成测试频率序列
     * @param startFreq 起始频率
     * @param endFreq 结束频率
     * @param points 点数
     * @param logScale 是否使用对数刻度
     * @return 频率序列
     */
    QVector<double> generateFrequencySequence(double startFreq, double endFreq, 
                                            int points, bool logScale = true);
    
    /**
     * @brief 生成电流序列
     * @param minCurrent 最小电流
     * @param maxCurrent 最大电流
     * @param stepSize 步进
     * @return 电流序列
     */
    QVector<double> generateCurrentSequence(double minCurrent, double maxCurrent, double stepSize);
    
    /**
     * @brief 计算理论谐振频率
     * @param inductance 电感量 (H)
     * @param parasticCapacitance 寄生电容 (F)
     * @return 谐振频率 (Hz)
     */
    double calculateResonantFrequency(double inductance, double parasticCapacitance);
    
    /**
     * @brief 估计寄生电容
     * @param inductanceSpectrum 电感量频谱
     * @return 寄生电容 (F)
     */
    double estimateParasiticCapacitance(const QMap<double, double>& inductanceSpectrum);
    
private:
    // === 配置参数 ===
    double startFrequency_;         // 起始频率 (Hz)
    double endFrequency_;           // 结束频率 (Hz)
    int frequencyPoints_;           // 频率点数
    double minTestCurrent_;         // 最小测试电流 (A)
    double maxTestCurrent_;         // 最大测试电流 (A)
    double tolerance_;              // 容差 (%)
    double minQualityFactor_;       // 最小Q值
    
    // 饱和测试参数
    bool saturationTestEnabled_;    // 是否启用饱和测试
    double saturationMaxCurrent_;   // 饱和测试最大电流 (A)
    double saturationStepSize_;     // 饱和测试电流步进 (A)
    
    // 测量常量
    static const double DEFAULT_START_FREQ;     // 默认起始频率
    static const double DEFAULT_END_FREQ;       // 默认结束频率
    static const int DEFAULT_FREQ_POINTS;       // 默认频率点数
    static const double DEFAULT_MIN_CURRENT;    // 默认最小电流
    static const double DEFAULT_MAX_CURRENT;    // 默认最大电流
    static const double DEFAULT_TOLERANCE;      // 默认容差
    static const double DEFAULT_MIN_Q_FACTOR;   // 默认最小Q值
    
    // 故障判断阈值
    static const double OPEN_CIRCUIT_THRESHOLD;    // 开路判断阈值
    static const double SHORT_CIRCUIT_THRESHOLD;   // 短路判断阈值
    static const double SATURATION_THRESHOLD;      // 饱和判断阈值 (%)
};

#endif // INDUCTORDIAGNOSTIC_H
