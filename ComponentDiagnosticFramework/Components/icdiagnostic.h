#ifndef ICDIAGNOSTIC_H
#define ICDIAGNOSTIC_H

#include "../basecomponentdiagnostic.h"
#include <QMap>
#include <QVector>

/**
 * @brief 集成电路诊断实现
 * 
 * 支持多种IC类型的诊断：
 * - 数字IC功能测试（逻辑门、计数器、微处理器等）
 * - 模拟IC参数测试（运放、比较器、电压调节器等）
 * - 电源管理IC测试（LDO、开关电源控制器等）
 * - 通信接口IC测试（UART、SPI、I2C等）
 * - 引脚连通性测试
 * - 静态电流测试
 * - 工作电压范围测试
 * - 频率响应测试
 */
class ICDiagnostic : public BaseComponentDiagnostic
{
    Q_OBJECT
    
public:
    // IC类型枚举
    enum class ICType {
        UNKNOWN,
        DIGITAL_LOGIC,      // 数字逻辑IC
        ANALOG_LINEAR,      // 模拟线性IC
        MICROCONTROLLER,    // 微控制器
        MEMORY,            // 存储器
        POWER_MANAGEMENT,   // 电源管理IC
        INTERFACE,         // 接口IC
        SENSOR,            // 传感器IC
        CUSTOM             // 自定义IC
    };
      explicit ICDiagnostic(DeviceManager* deviceManager, QObject* parent = nullptr);
    virtual ~ICDiagnostic() = default;
    
    // === 基础接口重载 ===
    ComponentType getSupportedComponentType() const override;
    QString getComponentTypeName() const override;
    QStringList getSupportedModels() const override;
    
protected:
    // === 必须实现的虚函数 ===
    QVector<PortRequirement> getPortRequirements(const ComponentSpec& component) const override;
    QVector<WiringConnection> generateWiringScheme(const ComponentSpec& component,
                                                  const QVector<PortInfo>& allocatedPorts) const override;
    ComponentTestConfig configureDataAcquisition(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts) const override;
    TestData executeDataAcquisition(const ComponentTestConfig& config) override;
    ComponentDiagnosticResult analyzeFaults(const ComponentSpec& component,
                                           const TestData& testData) override;

public:
    
    // === IC特有配置 ===
    
    /**
     * @brief 设置IC类型和引脚配置
     * @param icType IC类型
     * @param pinCount 引脚数量
     * @param packageType 封装类型
     */
    void setICConfiguration(ICType icType, int pinCount, const QString& packageType = "DIP");
    
    /**
     * @brief 设置电源引脚
     * @param vccPins 电源正极引脚列表
     * @param gndPins 电源负极引脚列表
     * @param nominalVoltage 标称电压 (V)
     */
    void setPowerPins(const QVector<int>& vccPins, const QVector<int>& gndPins, double nominalVoltage = 5.0);
    
    /**
     * @brief 设置输入输出引脚
     * @param inputPins 输入引脚列表
     * @param outputPins 输出引脚列表
     * @param bidirectionalPins 双向引脚列表
     */
    void setIOPins(const QVector<int>& inputPins, 
                   const QVector<int>& outputPins, 
                   const QVector<int>& bidirectionalPins = {});
    
    /**
     * @brief 设置功能测试参数
     * @param testVectors 测试向量表
     * @param clockPin 时钟引脚（-1表示无时钟）
     * @param resetPin 复位引脚（-1表示无复位）
     */
    void setFunctionalTestParams(const QVector<QMap<QString, QVariant>>& testVectors, 
                                int clockPin = -1, int resetPin = -1);
    
    /**
     * @brief 设置模拟参数测试
     * @param testFrequencies 测试频率列表
     * @param inputAmplitudes 输入信号幅度列表
     * @param loadConditions 负载条件
     */
    void setAnalogTestParams(const QVector<double>& testFrequencies = {1000.0},
                            const QVector<double>& inputAmplitudes = {1.0},
                            const QMap<QString, double>& loadConditions = {});
    
    /**
     * @brief 设置通信接口测试参数
     * @param protocol 通信协议 ("UART", "SPI", "I2C", "CAN"等)
     * @param baudRate 波特率
     * @param testData 测试数据
     */
    void setCommunicationTestParams(const QString& protocol, 
                                   int baudRate = 115200,
                                   const QByteArray& testData = QByteArray());
    
    /**
     * @brief 启用/禁用特定测试
     */
    void enablePinContinuityTest(bool enable = true);
    void enablePowerConsumptionTest(bool enable = true);
    void enableFunctionalTest(bool enable = true);
    void enableParametricTest(bool enable = true);
    void enableTimingTest(bool enable = true);
    
private:
    // === 测试方法 ===
    
    /**
     * @brief 引脚连通性测试
     * @return 连通性测试结果
     */
    QMap<QString, QVariant> testPinContinuity();
    
    /**
     * @brief 电源电流测试
     * @param voltage 供电电压
     * @return 电流测试结果
     */
    QMap<QString, QVariant> testPowerConsumption(double voltage);
    
    /**
     * @brief 功能测试
     * @return 功能测试结果
     */
    QMap<QString, QVariant> testFunctionality();
    
    /**
     * @brief 参数测试（模拟特性）
     * @return 参数测试结果
     */
    QMap<QString, QVariant> testParametrics();
    
    /**
     * @brief 时序测试
     * @return 时序测试结果
     */
    QMap<QString, QVariant> testTiming();
    
    /**
     * @brief 数字逻辑测试
     * @param testVectors 测试向量
     * @return 逻辑测试结果
     */
    QMap<QString, QVariant> testDigitalLogic(const QVector<QMap<QString, QVariant>>& testVectors);
    
    /**
     * @brief 模拟线性测试
     * @return 模拟特性测试结果
     */
    QMap<QString, QVariant> testAnalogLinear();
    
    /**
     * @brief 通信接口测试
     * @return 通信测试结果
     */
    QMap<QString, QVariant> testCommunication();
    
    /**
     * @brief 存储器测试
     * @return 存储器测试结果
     */
    QMap<QString, QVariant> testMemory();
    
    /**
     * @brief 电源管理IC测试
     * @return 电源管理测试结果
     */
    QMap<QString, QVariant> testPowerManagement();
    
    // === 引脚操作方法 ===
    
    /**
     * @brief 设置引脚电平
     * @param pin 引脚号
     * @param level 电平值 (V)
     * @return 是否成功
     */
    bool setPinLevel(int pin, double level);
    
    /**
     * @brief 读取引脚电平
     * @param pin 引脚号
     * @return 引脚电平 (V)
     */
    double readPinLevel(int pin);
    
    /**
     * @brief 测量引脚电流
     * @param pin 引脚号
     * @return 引脚电流 (A)
     */
    double measurePinCurrent(int pin);
    
    /**
     * @brief 设置引脚为输入/输出模式
     * @param pin 引脚号
     * @param isOutput 是否为输出模式
     * @return 是否成功
     */
    bool configurePinDirection(int pin, bool isOutput);
    
    /**
     * @brief 施加时钟信号
     * @param pin 时钟引脚
     * @param frequency 频率 (Hz)
     * @param cycles 周期数
     * @return 是否成功
     */
    bool applyClock(int pin, double frequency, int cycles = 1);
    
    /**
     * @brief 施加复位信号
     * @param pin 复位引脚
     * @param duration 复位持续时间 (s)
     * @return 是否成功
     */
    bool applyReset(int pin, double duration = 1e-3);
    
    // === 测量方法 ===
    
    /**
     * @brief 测量输入偏置电流
     * @param pin 输入引脚
     * @return 偏置电流 (A)
     */
    double measureInputBiasCurrent(int pin);
    
    /**
     * @brief 测量输入失调电压
     * @param posPin 正输入引脚
     * @param negPin 负输入引脚
     * @return 失调电压 (V)
     */
    double measureInputOffsetVoltage(int posPin, int negPin);
    
    /**
     * @brief 测量输出驱动能力
     * @param pin 输出引脚
     * @param loadCurrent 负载电流 (A)
     * @return 输出电压 (V)
     */
    double measureOutputDriveCapability(int pin, double loadCurrent);
    
    /**
     * @brief 测量传播延迟
     * @param inputPin 输入引脚
     * @param outputPin 输出引脚
     * @return 传播延迟 (s)
     */
    double measurePropagationDelay(int inputPin, int outputPin);
    
    /**
     * @brief 测量建立时间和保持时间
     * @param dataPin 数据引脚
     * @param clockPin 时钟引脚
     * @return {setup_time, hold_time} (s)
     */
    QMap<QString, double> measureSetupHoldTime(int dataPin, int clockPin);
    
    // === 分析方法 ===
    
    /**
     * @brief 分析引脚连通性结果
     * @param continuityData 连通性测试数据
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzePinContinuity(const QMap<QString, QVariant>& continuityData);
    
    /**
     * @brief 分析功能测试结果
     * @param functionalData 功能测试数据
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeFunctional(const QMap<QString, QVariant>& functionalData);
    
    /**
     * @brief 分析参数测试结果
     * @param parametricData 参数测试数据
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzeParametric(const QMap<QString, QVariant>& parametricData);
    
    /**
     * @brief 分析电源消耗
     * @param powerData 电源测试数据
     * @return 分析结果
     */
    QMap<QString, QVariant> analyzePowerConsumption(const QMap<QString, QVariant>& powerData);
    
    /**
     * @brief 检测IC类型
     * @param pinData 引脚测试数据
     * @return 检测到的IC类型
     */
    ICType detectICType(const QMap<QString, QVariant>& pinData);
    
    /**
     * @brief 生成测试向量
     * @param icType IC类型
     * @param pinCount 引脚数
     * @return 测试向量列表
     */
    QVector<QMap<QString, QVariant>> generateTestVectors(ICType icType, int pinCount);
    
    /**
     * @brief 验证引脚配置
     * @return 是否配置有效
     */
    bool validatePinConfiguration();
    
    /**
     * @brief IC类型转字符串
     */
    static QString icTypeToString(ICType type);
    static ICType stringToICType(const QString& typeStr);
    
private:
    // === 配置参数 ===
    ICType icType_;                     // IC类型
    int pinCount_;                      // 引脚数量
    QString packageType_;               // 封装类型
    
    // 引脚配置
    QVector<int> vccPins_;              // 电源正极引脚
    QVector<int> gndPins_;              // 电源负极引脚
    QVector<int> inputPins_;            // 输入引脚
    QVector<int> outputPins_;           // 输出引脚
    QVector<int> bidirectionalPins_;    // 双向引脚
    
    double nominalVoltage_;             // 标称工作电压
    int clockPin_;                      // 时钟引脚
    int resetPin_;                      // 复位引脚
    
    // 测试参数
    QVector<QMap<QString, QVariant>> testVectors_;  // 功能测试向量
    QVector<double> testFrequencies_;               // 测试频率
    QVector<double> inputAmplitudes_;               // 输入信号幅度
    QMap<QString, double> loadConditions_;          // 负载条件
    QVariantList logicTestVoltages_;                // 逻辑测试电压
    
    // 通信测试参数
    QString communicationProtocol_;     // 通信协议
    int baudRate_;                      // 波特率
    QByteArray testData_;              // 测试数据
    
    // 测试使能标志
    bool pinContinuityTestEnabled_;     // 引脚连通性测试
    bool powerConsumptionTestEnabled_;  // 功耗测试
    bool functionalTestEnabled_;        // 功能测试
    bool parametricTestEnabled_;        // 参数测试
    bool timingTestEnabled_;           // 时序测试
    
    // 测试限制
    static const double DEFAULT_VCC_VOLTAGE;        // 默认电源电压
    static const double MAX_SUPPLY_CURRENT;         // 最大供电电流
    static const double MAX_INPUT_LEAKAGE;          // 最大输入漏电流
    static const double MIN_OUTPUT_HIGH;            // 最小输出高电平
    static const double MAX_OUTPUT_LOW;             // 最大输出低电平
    static const double MAX_PROPAGATION_DELAY;      // 最大传播延迟
    static const double MIN_SETUP_TIME;             // 最小建立时间
    static const double MIN_HOLD_TIME;              // 最小保持时间
    
    // 容差
    static const double VOLTAGE_TOLERANCE;          // 电压容差
    static const double CURRENT_TOLERANCE;          // 电流容差
    static const double TIMING_TOLERANCE;           // 时序容差
    
    // === 私有测试方法 ===
    ComponentTestConfig configureGenericICTesting(const ComponentSpec& component, 
                                                 const QVector<PortInfo>& ports) const;
    double generateRandomValue(double min, double max) const;
    double measureVoltage(int channel) const;
};

#endif // ICDIAGNOSTIC_H
