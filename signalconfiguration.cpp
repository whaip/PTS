// 信号配置模块实现
#include "signalconfiguration.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

SignalConfiguration::SignalConfiguration(QObject *parent)
    : QObject(parent)
{
    initializePredefinedSchemes();
}

SignalConfiguration::~SignalConfiguration()
{
}

void SignalConfiguration::initializePredefinedSchemes()
{
    setupResistorSchemes();
    setupCapacitorSchemes();
    setupInductorSchemes();
    setupDiodeSchemes();
    setupICSchemes();
}

void SignalConfiguration::setupResistorSchemes()
{
    // 标准电阻测试方案
    TestSchemeSignals resistorStandard;
    resistorStandard.name = "Resistor_Standard";
    resistorStandard.description = "标准电阻测试方案：DC电压激励 + 电流测量 + 直接阻值测量";
    resistorStandard.compatibleComponents = QStringList() << "RESISTOR";
    resistorStandard.requiresSynchronization = true;
    resistorStandard.syncGroupName = "ResistorTest";
    
    // 信号1：DC电压输出
    SignalDefinition dcVoltage;
    dcVoltage.name = "DC_Voltage_Output";
    dcVoltage.type = SignalType::VOLTAGE_DC;
    dcVoltage.amplitude = 5.0;  // 5V测试电压
    dcVoltage.frequency = 0.0;
    dcVoltage.isRequired = true;
    dcVoltage.description = "直流电压激励信号";
    dcVoltage.parameters["device_type"] = "5711";
    dcVoltage.parameters["range"] = "±10V";
    
    // 信号2：电流测量
    SignalDefinition currentMeasure;
    currentMeasure.name = "Current_Measurement";
    currentMeasure.type = SignalType::CURRENT_DC;
    currentMeasure.amplitude = 0.0;  // 测量信号
    currentMeasure.frequency = 0.0;
    currentMeasure.isRequired = true;
    currentMeasure.description = "直流电流测量";
    currentMeasure.parameters["device_type"] = "5323";
    currentMeasure.parameters["range"] = "±1A";
    
    // 信号3：直接电阻测量
    SignalDefinition resistanceMeasure;
    resistanceMeasure.name = "Resistance_Measurement";
    resistanceMeasure.type = SignalType::RESISTANCE;
    resistanceMeasure.amplitude = 0.0;
    resistanceMeasure.frequency = 1000.0;  // 1kHz测试频率
    resistanceMeasure.isRequired = true;
    resistanceMeasure.description = "直接电阻值测量";
    resistanceMeasure.parameters["device_type"] = "8902";
    resistanceMeasure.parameters["range"] = "auto";
    
    resistorStandard.signals_ << dcVoltage << currentMeasure << resistanceMeasure;
    predefinedSchemes_["Resistor_Standard"] = resistorStandard;
    
    // 更新组件映射
    componentSchemeMap_[ComponentType::RESISTOR] = QStringList()
        << "Resistor_Standard";
}

void SignalConfiguration::setupCapacitorSchemes()
{
    // 标准电容测试方案
    TestSchemeSignals capacitorStandard;
    capacitorStandard.name = "Capacitor_Standard";
    capacitorStandard.description = "标准电容测试方案：AC阻抗测量 + ESR测量 + 漏电流测试";
    capacitorStandard.compatibleComponents = QStringList() << "CAPACITOR";
    capacitorStandard.requiresSynchronization = true;
    capacitorStandard.syncGroupName = "CapacitorTest";
    
    // 信号1：AC电压激励（电容值测量）
    SignalDefinition acVoltage;
    acVoltage.name = "AC_Voltage_Output";
    acVoltage.type = SignalType::VOLTAGE_AC;
    acVoltage.amplitude = 1.0;  // 1V AC
    acVoltage.frequency = 1000.0;  // 1kHz
    acVoltage.isRequired = true;
    acVoltage.description = "交流电压激励信号";
    acVoltage.parameters["device_type"] = "5711";
    acVoltage.parameters["waveform"] = "sine";
    
    // 信号2：电容值测量
    SignalDefinition capacitanceMeasure;
    capacitanceMeasure.name = "Capacitance_Measurement";
    capacitanceMeasure.type = SignalType::CAPACITANCE;
    capacitanceMeasure.amplitude = 0.0;
    capacitanceMeasure.frequency = 1000.0;
    capacitanceMeasure.isRequired = true;
    capacitanceMeasure.description = "电容值测量";
    capacitanceMeasure.parameters["device_type"] = "8902";
    capacitanceMeasure.parameters["range"] = "auto";
    
    // 信号3：ESR测量
    SignalDefinition esrMeasure;
    esrMeasure.name = "ESR_Measurement";
    esrMeasure.type = SignalType::RESISTANCE;
    esrMeasure.amplitude = 0.0;
    esrMeasure.frequency = 100000.0;  // 100kHz
    esrMeasure.isRequired = false;
    esrMeasure.description = "等效串联电阻测量";
    esrMeasure.parameters["device_type"] = "8902";
    esrMeasure.parameters["measurement_type"] = "ESR";
    
    // 信号4：漏电流测试
    SignalDefinition leakageCurrent;
    leakageCurrent.name = "Leakage_Current_Test";
    leakageCurrent.type = SignalType::CURRENT_DC;
    leakageCurrent.amplitude = 0.0;
    leakageCurrent.frequency = 0.0;
    leakageCurrent.isRequired = false;
    leakageCurrent.description = "漏电流测试";
    leakageCurrent.parameters["device_type"] = "5320";
    leakageCurrent.parameters["range"] = "±1mA";
    leakageCurrent.parameters["test_voltage"] = "rated_voltage";
    
    capacitorStandard.signals_ << acVoltage << capacitanceMeasure << esrMeasure << leakageCurrent;
    predefinedSchemes_["Capacitor_Standard"] = capacitorStandard;
    
    componentSchemeMap_[ComponentType::CAPACITOR] = QStringList() << "Capacitor_Standard";
}

void SignalConfiguration::setupInductorSchemes()
{
    // 标准电感测试方案
    TestSchemeSignals inductorStandard;
    inductorStandard.name = "Inductor_Standard";
    inductorStandard.description = "标准电感测试方案：AC阻抗测量 + 电感值测量 + DCR测量";
    inductorStandard.compatibleComponents = QStringList() << "INDUCTOR";
    inductorStandard.requiresSynchronization = true;
    inductorStandard.syncGroupName = "InductorTest";
    
    // 信号1：AC电压激励
    SignalDefinition acVoltage;
    acVoltage.name = "AC_Voltage_Output";
    acVoltage.type = SignalType::VOLTAGE_AC;
    acVoltage.amplitude = 1.0;
    acVoltage.frequency = 10000.0;  // 10kHz
    acVoltage.isRequired = true;
    acVoltage.description = "交流电压激励信号";
    acVoltage.parameters["device_type"] = "5711";
    
    // 信号2：电感值测量
    SignalDefinition inductanceMeasure;
    inductanceMeasure.name = "Inductance_Measurement";
    inductanceMeasure.type = SignalType::INDUCTANCE;
    inductanceMeasure.amplitude = 0.0;
    inductanceMeasure.frequency = 10000.0;
    inductanceMeasure.isRequired = true;
    inductanceMeasure.description = "电感值测量";
    inductanceMeasure.parameters["device_type"] = "8902";
    
    // 信号3：DCR测量
    SignalDefinition dcrMeasure;
    dcrMeasure.name = "DCR_Measurement";
    dcrMeasure.type = SignalType::RESISTANCE;
    dcrMeasure.amplitude = 0.0;
    dcrMeasure.frequency = 0.0;
    dcrMeasure.isRequired = true;
    dcrMeasure.description = "直流电阻测量";
    dcrMeasure.parameters["device_type"] = "8902";
    
    inductorStandard.signals_ << acVoltage << inductanceMeasure << dcrMeasure;
    predefinedSchemes_["Inductor_Standard"] = inductorStandard;
    
    componentSchemeMap_[ComponentType::INDUCTOR] = QStringList() << "Inductor_Standard";
}

void SignalConfiguration::setupDiodeSchemes()
{
    // 标准二极管测试方案
    TestSchemeSignals diodeStandard;
    diodeStandard.name = "Diode_Standard";
    diodeStandard.description = "标准二极管测试方案：正向压降测试 + 反向漏电流测试";
    diodeStandard.compatibleComponents = QStringList() << "DIODE";
    diodeStandard.requiresSynchronization = false;
    
    // 信号1：正向电流激励
    SignalDefinition forwardCurrent;
    forwardCurrent.name = "Forward_Current_Source";
    forwardCurrent.type = SignalType::CURRENT_DC;
    forwardCurrent.amplitude = 0.01;  // 10mA
    forwardCurrent.frequency = 0.0;
    forwardCurrent.isRequired = true;
    forwardCurrent.description = "正向电流激励";
    forwardCurrent.parameters["device_type"] = "5711";
    forwardCurrent.parameters["mode"] = "current_source";
    
    // 信号2：正向电压测量
    SignalDefinition forwardVoltage;
    forwardVoltage.name = "Forward_Voltage_Measurement";
    forwardVoltage.type = SignalType::VOLTAGE_DC;
    forwardVoltage.amplitude = 0.0;
    forwardVoltage.frequency = 0.0;
    forwardVoltage.isRequired = true;
    forwardVoltage.description = "正向压降测量";
    forwardVoltage.parameters["device_type"] = "5322";
    
    // 信号3：反向漏电流测试
    SignalDefinition reverseCurrent;
    reverseCurrent.name = "Reverse_Leakage_Current";
    reverseCurrent.type = SignalType::CURRENT_DC;
    reverseCurrent.amplitude = 0.0;
    reverseCurrent.frequency = 0.0;
    reverseCurrent.isRequired = true;
    reverseCurrent.description = "反向漏电流测量";
    reverseCurrent.parameters["device_type"] = "5320";
    reverseCurrent.parameters["reverse_voltage"] = "rated_voltage";
    
    diodeStandard.signals_ << forwardCurrent << forwardVoltage << reverseCurrent;
    predefinedSchemes_["Diode_Standard"] = diodeStandard;
    
    componentSchemeMap_[ComponentType::DIODE] = QStringList() << "Diode_Standard";
}

void SignalConfiguration::setupICSchemes()
{
    // 标准IC测试方案
    TestSchemeSignals icStandard;
    icStandard.name = "IC_Standard";
    icStandard.description = "标准IC测试方案：电源电流测试 + 基本功能验证";
    icStandard.compatibleComponents = QStringList() << "IC";
    icStandard.requiresSynchronization = true;
    icStandard.syncGroupName = "ICTest";
    
    // 信号1：电源电压
    SignalDefinition supplyVoltage;
    supplyVoltage.name = "Supply_Voltage";
    supplyVoltage.type = SignalType::VOLTAGE_DC;
    supplyVoltage.amplitude = 5.0;  // 5V供电
    supplyVoltage.frequency = 0.0;
    supplyVoltage.isRequired = true;
    supplyVoltage.description = "电源电压";
    supplyVoltage.parameters["device_type"] = "5711";
    
    // 信号2：电源电流测量
    SignalDefinition supplyCurrent;
    supplyCurrent.name = "Supply_Current_Measurement";
    supplyCurrent.type = SignalType::CURRENT_DC;
    supplyCurrent.amplitude = 0.0;
    supplyCurrent.frequency = 0.0;
    supplyCurrent.isRequired = true;
    supplyCurrent.description = "电源电流测量";
    supplyCurrent.parameters["device_type"] = "5320";
    
    // 信号3：数字IO测试
    SignalDefinition digitalIO;
    digitalIO.name = "Digital_IO_Test";
    digitalIO.type = SignalType::DIGITAL_OUTPUT;
    digitalIO.amplitude = 5.0;  // 逻辑高电平
    digitalIO.frequency = 0.0;
    digitalIO.isRequired = false;
    digitalIO.description = "数字IO功能测试";
    digitalIO.parameters["device_type"] = "5320";
    digitalIO.parameters["pattern"] = "toggle";
    
    icStandard.signals_ << supplyVoltage << supplyCurrent << digitalIO;
    predefinedSchemes_["IC_Standard"] = icStandard;
    
    componentSchemeMap_[ComponentType::IC] = QStringList() << "IC_Standard";
}

QStringList SignalConfiguration::getAvailableTestSchemes() const
{
    return predefinedSchemes_.keys();
}

TestSchemeSignals SignalConfiguration::getTestScheme(const QString& schemeName) const
{
    return predefinedSchemes_.value(schemeName, TestSchemeSignals());
}

bool SignalConfiguration::addCustomSignal(const SignalConfig& signal)
{
    if (signal.name.isEmpty()) {
        return false;
    }
    
    customSignals_[signal.name] = signal;
    emit customSignalAdded(signal.name);
    return true;
}

bool SignalConfiguration::removeCustomSignal(const QString& signalName)
{
    if (customSignals_.remove(signalName) > 0) {
        emit customSignalRemoved(signalName);
        return true;
    }
    return false;
}

QStringList SignalConfiguration::getCustomSignals() const
{
    return customSignals_.keys();
}

QStringList SignalConfiguration::getRecommendedSchemes(ComponentType componentType) const
{
    return componentSchemeMap_.value(componentType, QStringList());
}

bool SignalConfiguration::createCustomScheme(const TestSchemeSignals& scheme)
{
    if (scheme.name.isEmpty() || scheme.signals_.isEmpty()) {
        return false;
    }
    
    predefinedSchemes_[scheme.name] = scheme;
    emit schemeAdded(scheme.name);
    return true;
}

bool SignalConfiguration::validateTestScheme(const TestSchemeSignals& scheme, QStringList& errors) const
{
    errors.clear();
    
    if (scheme.name.isEmpty()) {
        errors << "测试方案名称不能为空";
    }
    
    if (scheme.signals_.isEmpty()) {
        errors << "测试方案必须包含至少一个信号";
    }
    
    // 检查必需信号
    bool hasRequiredSignal = false;
    for (const SignalDefinition& signal : scheme.signals_) {
        if (signal.isRequired) {
            hasRequiredSignal = true;
            break;
        }
    }
    
    if (!hasRequiredSignal) {
        errors << "测试方案必须包含至少一个必需信号";
    }
    
    // 检查同步配置
    if (scheme.requiresSynchronization && scheme.syncGroupName.isEmpty()) {
        errors << "需要同步的测试方案必须指定同步组名称";
    }
    
    return errors.isEmpty();
}

QString SignalConfiguration::getSignalTypeDescription(SignalType type) const
{
    switch (type) {
        case SignalType::VOLTAGE_DC: return "直流电压";
        case SignalType::VOLTAGE_AC: return "交流电压";
        case SignalType::CURRENT_DC: return "直流电流";
        case SignalType::CURRENT_AC: return "交流电流";
        case SignalType::DIGITAL_OUTPUT: return "数字输出";
        case SignalType::DIGITAL_INPUT: return "数字输入";
        case SignalType::ANALOG_OUTPUT: return "模拟输出";
        case SignalType::ANALOG_INPUT: return "模拟输入";
        case SignalType::DMM_MEASUREMENT: return "万用表测量";
        case SignalType::RESISTANCE: return "电阻测量";
        case SignalType::CAPACITANCE: return "电容测量";
        case SignalType::INDUCTANCE: return "电感测量";
        case SignalType::FREQUENCY: return "频率测量";
        case SignalType::POWER: return "功率测量";
        default: return "未知信号类型";
    }
}

QStringList SignalConfiguration::getAllSignalTypes() const
{
    QStringList types;
    types << "直流电压" << "交流电压" << "直流电流" << "交流电流"
          << "数字输出" << "数字输入" << "模拟输出" << "模拟输入"
          << "万用表测量" << "电阻测量" << "电容测量" << "电感测量"
          << "频率测量" << "功率测量";
    return types;
}
