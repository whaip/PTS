#include "faultdiagnostic.h"
#include "testsequencemanager.h"
#include "include/JY8902.h"
#include "5711waveformconfig.h"
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>
#include <QTimer>
#include <QElapsedTimer>
#include <QCoreApplication>

FaultDiagnostic::FaultDiagnostic(DeviceManager* deviceManager, QObject *parent)
    : QObject(parent)
    , device_manager_(deviceManager)
    , use_threaded_manager_(false)
    , threaded_device_manager_(deviceManager)
{
}

FaultDiagnostic::~FaultDiagnostic()
{
}

DiagnosticResult FaultDiagnostic::diagnoseComponent(const ComponentSpec& component)
{
    // 首先发出接线引导信号
    emit wiringRequired(component);
    
    // 等待接线完成的信号
    // 在实际应用中，这里可能需要更复杂的同步机制
    
    DiagnosticResult result;
    result.componentId = component.reference;
    result.componentType = QString::number(static_cast<int>(component.type));    
    if (!device_manager_ || !device_manager_->isSystemReady()) {
        result.result = DiagnosticResult::ERROR;
        result.faultTypes.append("UNKNOWN_FAULT");
        result.notes = "测试系统未就绪";
        result.confidence = 0.0;
        return result;
    }
    
    // 首先检查连接性
    if (!checkComponentConnection(component.channel)) {
        result.result = DiagnosticResult::FAIL;
        result.faultTypes.append("NO_CONNECTION");
        result.notes = "元件未连接或连接不良";
        result.confidence = 0.95;
        result.healthScore = 0.0;
        return result;
    }
    
    // 根据元件类型选择诊断方法
    switch (component.type) {
        case ComponentType::RESISTOR:
            result = diagnoseResistor(component);
            break;
        case ComponentType::CAPACITOR:
            result = diagnoseCapacitor(component);
            break;
        case ComponentType::INDUCTOR:
            result = diagnoseInductor(component);
            break;
        case ComponentType::DIODE:
            result = diagnoseDiode(component);
            break;
        case ComponentType::IC:
            result = diagnoseIC(component);
            break;        default:
            result.result = DiagnosticResult::ERROR;
            result.faultTypes.append("UNKNOWN_FAULT");
            result.notes = "不支持的元件类型";
            result.confidence = 0.0;
            break;
    }
    
    // 计算健康度和建议
    result.healthScore = calculateHealthScore(result);
    result.notes += generateRecommendation(result);
    result.confidence = calculateConfidence(result);
    
    emit diagnosticCompleted(result);
    return result;
}

// Internal diagnosis function that doesn't emit signals (used by async function)
DiagnosticResult FaultDiagnostic::diagnoseComponentInternal(const ComponentSpec& component)
{
    DiagnosticResult result;
    result.componentId = component.reference;
    result.componentType = QString::number(static_cast<int>(component.type));
    
    if (!device_manager_ || !device_manager_->isSystemReady()) {
        result.result = DiagnosticResult::ERROR;
        result.faultTypes.append("UNKNOWN_FAULT");
        result.notes = "测试系统未就绪";
        result.confidence = 0.0;
        return result;
    }
    
    // 首先检查连接性
    if (!checkComponentConnection(component.channel)) {
        result.result = DiagnosticResult::FAIL;
        result.faultTypes.append("NO_CONNECTION");
        result.notes = "元件未连接或连接不良";
        result.confidence = 0.95;
        result.healthScore = 0.0;
        return result;
    }
    
    // 根据元件类型选择诊断方法
    switch (component.type) {
        case ComponentType::RESISTOR:
            result = diagnoseResistor(component);
            break;
        case ComponentType::CAPACITOR:
            result = diagnoseCapacitor(component);
            break;
        case ComponentType::INDUCTOR:
            result = diagnoseInductor(component);
            break;
        case ComponentType::DIODE:
            result = diagnoseDiode(component);
            break;
        case ComponentType::IC:
            result = diagnoseIC(component);
            break;
        default:
            result.result = DiagnosticResult::ERROR;
            result.faultTypes.append("UNKNOWN_FAULT");
            result.notes = "不支持的元件类型";
            result.confidence = 0.0;
            break;
    }
    
    // 计算健康度和建议
    result.healthScore = calculateHealthScore(result);
    result.notes += generateRecommendation(result);
    result.confidence = calculateConfidence(result);
    
    // Note: No signal emission here - that's handled by the caller
    return result;
}

DiagnosticResult FaultDiagnostic::diagnoseResistor(const ComponentSpec& spec)
{
    DiagnosticResult result;
    result.componentId = spec.reference;
    result.componentType = "resistor";
    result.expectedValue = spec.nominal_value;
    result.tolerance = spec.tolerance;
    
    qDebug() << "Diagnosing resistor:" << spec.reference;
    
    // 测量电阻值
    result.measurementData = measureResistance(spec.channel, 1.0);
    
    if (!result.measurementData.valid) {
        result.result = DiagnosticResult::ERROR;
        result.faultTypes.append("UNKNOWN_FAULT");
        result.notes = "测量失败: " + result.measurementData.error_message;
        return result;
    }
    
    // 温度补偿
    double compensated_value = applyTemperatureCompensation(
        result.measurementData.primary_value, 
        spec.temp_coefficient, 
        result.measurementData.temperature
    );
    
    // 故障分析
    FaultType fault_type = analyzeResistorFault(spec, result.measurementData);
      switch (fault_type) {
        case FaultType::COMPONENT_OK:
            result.result = DiagnosticResult::PASS;
            result.notes = QString("电阻正常，测量值: %1Ω").arg(compensated_value, 0, 'f', 2);
            break;
        case FaultType::SHORT_CIRCUIT:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("SHORT_CIRCUIT");
            result.notes = QString("电阻短路，测量值: %1Ω (< %2Ω)")
                .arg(compensated_value, 0, 'f', 2)
                .arg(spec.nominal_value * 0.1, 0, 'f', 2);
            break;
        case FaultType::OPEN_CIRCUIT:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("OPEN_CIRCUIT");
            result.notes = QString("电阻开路，测量值: %1Ω (> %2Ω)")
                .arg(compensated_value, 0, 'f', 2)
                .arg(spec.nominal_value * 10, 0, 'f', 2);
            break;
        case FaultType::OUT_OF_TOLERANCE:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("OUT_OF_TOLERANCE");
            result.notes = QString("电阻超差，测量值: %1Ω，标称值: %2Ω ±%3%")
                .arg(compensated_value, 0, 'f', 2)
                .arg(spec.nominal_value, 0, 'f', 2)
                .arg(spec.tolerance * 100, 0, 'f', 1);
            break;
        default:
            result.result = DiagnosticResult::ERROR;
            result.faultTypes.append("UNKNOWN_FAULT");
            result.notes = "未知故障";
            break;
    }
    
    return result;
}

DiagnosticResult FaultDiagnostic::diagnoseCapacitor(const ComponentSpec& spec)
{
    DiagnosticResult result;
    result.componentId = spec.reference;
    result.componentType = "capacitor";
    result.expectedValue = spec.nominal_value;
    result.tolerance = spec.tolerance;
    
    qDebug() << "Diagnosing capacitor:" << spec.reference;
    
    // 测量电容值和ESR
    result.measurementData = measureCapacitance(spec.channel, 1000.0);
    
    if (!result.measurementData.valid) {
        result.result = DiagnosticResult::ERROR;
        result.faultTypes.append("UNKNOWN_FAULT");
        result.notes = "测量失败: " + result.measurementData.error_message;
        return result;
    }
    
    // 故障分析
    FaultType fault_type = analyzeCapacitorFault(spec, result.measurementData);
    
    switch (fault_type) {
        case FaultType::COMPONENT_OK:
            result.result = DiagnosticResult::PASS;
            result.notes = QString("电容正常，容值: %1μF，ESR: %2Ω")
                .arg(result.measurementData.primary_value * 1e6, 0, 'f', 2)
                .arg(result.measurementData.esr, 0, 'f', 2);
            break;
        case FaultType::SHORT_CIRCUIT:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("SHORT_CIRCUIT");
            result.notes = "电容短路";
            break;
        case FaultType::OPEN_CIRCUIT:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("OPEN_CIRCUIT");
            result.notes = "电容开路";
            break;
        case FaultType::OUT_OF_TOLERANCE:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("OUT_OF_TOLERANCE");
            result.notes = QString("电容容值超差，测量值: %1μF，标称值: %2μF")
                .arg(result.measurementData.primary_value * 1e6, 0, 'f', 2)
                .arg(spec.nominal_value * 1e6, 0, 'f', 2);
            break;        case FaultType::HIGH_ESR:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("HIGH_ESR");
            result.notes = QString("电容ESR过高: %1Ω (最大允许: %2Ω)")
                .arg(result.measurementData.esr, 0, 'f', 2)
                .arg(spec.max_esr, 0, 'f', 2);
            break;
        case FaultType::HIGH_LEAKAGE:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("HIGH_LEAKAGE");
            result.notes = QString("电容漏电流过大: %1μA (最大允许: %2μA)")
                .arg(result.measurementData.leakage_current * 1e6, 0, 'f', 2)
                .arg(spec.max_leakage * 1e6, 0, 'f', 2);
            break;        default:
            result.result = DiagnosticResult::ERROR;
            result.faultTypes.append("UNKNOWN_FAULT");
            result.notes = "未知故障";
            break;
    }
    
    return result;
}

DiagnosticResult FaultDiagnostic::diagnoseInductor(const ComponentSpec& spec)
{
    DiagnosticResult result;
    result.componentId = spec.reference;
    result.componentType = "inductor";
    result.expectedValue = spec.nominal_value;
    result.tolerance = spec.tolerance;
    
    qDebug() << "Diagnosing inductor:" << spec.reference;
    
    // 测量电感值
    result.measurementData = measureInductance(spec.channel, 10000.0);
    
    if (!result.measurementData.valid) {
        result.result = DiagnosticResult::ERROR;
        result.faultTypes.append("UNKNOWN_FAULT");
        result.notes = "测量失败: " + result.measurementData.error_message;
        return result;
    }
    
    // 简单的电感故障判断
    if (result.measurementData.primary_value < spec.nominal_value * 0.1) {
        result.result = DiagnosticResult::FAIL;
        result.faultTypes.append("SHORT_CIRCUIT");
        result.notes = "电感短路";
    } else if (result.measurementData.primary_value > spec.nominal_value * 10) {
        result.result = DiagnosticResult::FAIL;
        result.faultTypes.append("OPEN_CIRCUIT");
        result.notes = "电感开路";
    } else if (!isWithinTolerance(spec.nominal_value, result.measurementData.primary_value, spec.tolerance)) {
        result.result = DiagnosticResult::FAIL;
        result.faultTypes.append("OUT_OF_TOLERANCE");
        result.notes = QString("电感值超差，测量值: %1mH，标称值: %2mH")
            .arg(result.measurementData.primary_value * 1000, 0, 'f', 2)
            .arg(spec.nominal_value * 1000, 0, 'f', 2);    } else {
        result.result = DiagnosticResult::PASS;
        result.notes = QString("电感正常，测量值: %1mH")
            .arg(result.measurementData.primary_value * 1000, 0, 'f', 2);
    }
    
    return result;
}

DiagnosticResult FaultDiagnostic::diagnoseDiode(const ComponentSpec& spec)
{
    DiagnosticResult result;
    result.componentId = spec.reference;
    result.componentType = "diode";
    result.expectedValue = spec.nominal_value;
    result.tolerance = spec.tolerance;
    
    qDebug() << "Diagnosing diode:" << spec.reference;
    
    result.measurementData = measureDiodeCharacteristics(spec.channel);
    
    if (!result.measurementData.valid) {
        result.result = DiagnosticResult::ERROR;
        result.faultTypes.append("UNKNOWN_FAULT");
        result.notes = "测量失败: " + result.measurementData.error_message;
        return result;
    }
    
    FaultType fault_type = analyzeDiodeFault(spec, result.measurementData);
    
    switch (fault_type) {
        case FaultType::COMPONENT_OK:
            result.result = DiagnosticResult::PASS;
            result.notes = QString("二极管正常，正向压降: %1V，反向漏电流: %2μA")
                .arg(result.measurementData.voltage, 0, 'f', 2)
                .arg(result.measurementData.leakage_current * 1e6, 0, 'f', 2);
            break;        case FaultType::DIODE_SHORT:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("DIODE_SHORT");
            result.notes = "二极管短路";
            break;
        case FaultType::DIODE_OPEN:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("DIODE_OPEN");
            result.notes = "二极管开路";
            break;
        case FaultType::DIODE_LEAKAGE:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("DIODE_LEAKAGE");            result.notes = QString("二极管反向漏电流过大: %1μA")
                .arg(result.measurementData.leakage_current * 1e6, 0, 'f', 2);
            break;
        default:
            result.result = DiagnosticResult::ERROR;
            result.faultTypes.append("UNKNOWN_FAULT");
            result.notes = "未知故障";
            break;
    }
    
    return result;
}

DiagnosticResult FaultDiagnostic::diagnoseIC(const ComponentSpec& spec)
{
    DiagnosticResult result;
    result.componentId = spec.reference;
    result.componentType = "ic";
    result.expectedValue = spec.nominal_value;
    result.tolerance = spec.tolerance;
    
    qDebug() << "Diagnosing IC:" << spec.reference;
    
    result.measurementData = measureICParameters(spec.channel, spec);
    
    if (!result.measurementData.valid) {
        result.result = DiagnosticResult::ERROR;
        result.faultTypes.append("UNKNOWN_FAULT");
        result.notes = "测量失败: " + result.measurementData.error_message;
        return result;
    }
      // IC故障判断主要基于电源电流
    FaultType fault_type = analyzeICFault(spec, result.measurementData);
    
    switch (fault_type) {
        case FaultType::COMPONENT_OK:
            result.result = DiagnosticResult::PASS;
            result.notes = QString("IC工作正常，电源电流: %1mA")
                .arg(result.measurementData.current * 1000, 0, 'f', 2);
            break;
        case FaultType::IC_OVERCURRENT:
            result.result = DiagnosticResult::FAIL;
            result.faultTypes.append("IC_OVERCURRENT");
            result.notes = QString("IC功耗异常，电流: %1mA (最大允许: %2mA)")
                .arg(result.measurementData.current * 1000, 0, 'f', 2)
                .arg(spec.max_current * 1000, 0, 'f', 2);
            break;
        default:
            result.result = DiagnosticResult::ERROR;
            result.faultTypes.append("UNKNOWN_FAULT");
            result.notes = "未知故障";
            break;
    }
    
    return result;
}

MeasurementResult FaultDiagnostic::measureResistance(int channel, double test_voltage)
{
    MeasurementResult result;
    
    // 使用DMM测量电阻 - 使用DeviceOperation方式
    qDebug() << "开始电阻测量 - 通道:" << channel << "测试电压:" << test_voltage;
    
    // 1. 配置DMM连续电阻测量
    DeviceOperation configOp;
    configOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    configOp.parameters["range"] = "auto";
    configOp.parameters["samplesPerTrigger"] = 20;
    configOp.parameters["sampleInterval"] = 0.02;
    configOp.parameters["useNPLC"] = false;
    configOp.parameters["apertureTime"] = 0.02;
    configOp.parameters["nplcValue"] = 3;
    configOp.parameters["triggerDelay"] = 10;
    configOp.parameters["bufferSize"] = 1000;
    configOp.parameters["timeout"] = 5000;
    configOp.timeout = 5000;
    
    if (!device_manager_->submitOperation("JY8902", configOp)) {
        result.error_message = "DMM配置操作提交失败";
        result.valid = false;
        return result;
    }
    
    DeviceResult configResult = device_manager_->waitForResult("JY8902", 5000);
    if (!configResult.success) {
        result.error_message = "DMM配置失败: " + configResult.error;
        result.valid = false;
        return result;
    }
    
    // 2. 启动连续测量
    DeviceOperation startOp;
    startOp.command = DeviceCommand::START_MEASUREMENT;
    startOp.timeout = 5000;
    
    if (!device_manager_->submitOperation("JY8902", startOp)) {
        result.error_message = "DMM启动操作提交失败";
        result.valid = false;
        return result;
    }
    
    DeviceResult startResult = device_manager_->waitForResult("JY8902", 5000);
    if (!startResult.success) {
        result.error_message = "DMM启动失败: " + startResult.error;
        result.valid = false;
        return result;
    }
    
    // 3. 发送软件触发
    DeviceOperation triggerOp;
    triggerOp.command = DeviceCommand::SYNC_TRIGGER;
    triggerOp.timeout = 5000;
    
    if (!device_manager_->submitOperation("JY8902", triggerOp)) {
        result.error_message = "DMM软件触发操作提交失败";
        result.valid = false;
        return result;
    }
    
    DeviceResult triggerResult = device_manager_->waitForResult("JY8902", 5000);
    if (!triggerResult.success) {
        result.error_message = "DMM软件触发失败: " + triggerResult.error;
        result.valid = false;
        return result;
    }
    
    // 4. 读取电阻数据
    QVector<QVector<double>> channelData;
    
    DeviceManager::WaitResult waitResult = device_manager_->waitForDataWithEventLoop(
        "JY8902", channelData, 50, 100, 15000);
    if (waitResult.success && !channelData.isEmpty() && !channelData[0].isEmpty()) {
        QVector<double> resistanceData = channelData[0];
        double avgResistance = 0.0;
        for (double val : resistanceData) {
            avgResistance += val;
        }
        avgResistance /= resistanceData.size();
        result.primary_value = avgResistance;
        result.valid = true;
        qDebug() << "电阻测量成功:" << result.primary_value << "Ω";
    } else {
        result.error_message = "DMM读取失败: " + waitResult.errorMessage;
        result.valid = false;
    }
    
    // 5. 停止连续测量
    DeviceOperation stopOp;
    stopOp.command = DeviceCommand::STOP_MEASUREMENT;
    stopOp.timeout = 3000;
    device_manager_->submitOperation("JY8902", stopOp);
    device_manager_->waitForResult("JY8902", 3000);
    
    return result;
}

MeasurementResult FaultDiagnostic::measureCapacitance(int channel, double test_frequency)
{
    MeasurementResult result;
    
    // 使用AC分析法测量电容
    qDebug() << "开始电容测量 - 通道:" << channel << "测试频率:" << test_frequency;
    
    // 1. 输出测试信号
    if (!applyTestVoltage(channel, 1.0)) {
        result.valid = false;
        result.error_message = "无法输出测试信号";
        return result;
    }
    
    // 2. 测量电压 - 使用DAQ设备
    DeviceOperation voltageReadOp;
    voltageReadOp.command = DeviceCommand::READ_DATA;
    voltageReadOp.channel = channel;
    voltageReadOp.timeout = 5000;
    
    if (!device_manager_->submitOperation("JY5322", voltageReadOp)) {
        result.valid = false;
        result.error_message = "电压测量操作提交失败";
        return result;
    }
    
    DeviceResult voltageResult = device_manager_->waitForResult("JY5322", 5000);
    if (!voltageResult.success) {
        result.valid = false;
        result.error_message = "电压测量失败: " + voltageResult.error;
        return result;
    }
    
    double voltage = voltageResult.value;
    
    // 3. 测量电阻 - 使用DMM设备
    DeviceOperation resistanceConfigOp;
    resistanceConfigOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    resistanceConfigOp.parameters["range"] = "auto";
    resistanceConfigOp.timeout = 3000;
    
    if (device_manager_->submitOperation("JY8902", resistanceConfigOp)) {
        device_manager_->waitForResult("JY8902", 3000);
        
        DeviceOperation resistanceReadOp;
        resistanceReadOp.command = DeviceCommand::READ_DATA;
        resistanceReadOp.timeout = 5000;
        
        if (device_manager_->submitOperation("JY8902", resistanceReadOp)) {
            DeviceResult resistanceResult = device_manager_->waitForResult("JY8902", 5000);
            if (resistanceResult.success) {
                double resistance = resistanceResult.value;
                
                // 简化的电容计算 (基于电阻值估算)
                double impedance = resistance; 
                double capacitance = 1.0 / (2 * M_PI * test_frequency * impedance);
                
                result.primary_value = qAbs(capacitance);
                result.voltage = voltage;
                result.current = voltage / (resistance + 1e-12); // 基于欧姆定律估算电流
                result.esr = impedance * 0.1; // 简化的ESR估算
                result.valid = true;
                
                qDebug() << "电容测量成功:" << result.primary_value << "F";
            } else {
                result.valid = false;
                result.error_message = "电阻测量失败: " + resistanceResult.error;
            }
        } else {
            result.valid = false;
            result.error_message = "电阻测量操作提交失败";
        }
    } else {
        result.valid = false;
        result.error_message = "电阻测量配置失败";
    }
    
    return result;
}

MeasurementResult FaultDiagnostic::measureInductance(int channel, double test_frequency)
{
    MeasurementResult result;
    
    // 使用AC分析法测量电感
    qDebug() << "开始电感测量 - 通道:" << channel << "测试频率:" << test_frequency;
    
    if (!applyTestVoltage(channel, 1.0)) {
        result.valid = false;
        result.error_message = "无法输出测试信号";
        return result;
    }
    
    // 测量电压 - 使用DAQ设备
    DeviceOperation voltageReadOp;
    voltageReadOp.command = DeviceCommand::READ_DATA;
    voltageReadOp.channel = channel;
    voltageReadOp.timeout = 5000;
    
    if (!device_manager_->submitOperation("JY5322", voltageReadOp)) {
        result.valid = false;
        result.error_message = "电压测量操作提交失败";
        return result;
    }
    
    DeviceResult voltageResult = device_manager_->waitForResult("JY5322", 5000);
    if (!voltageResult.success) {
        result.valid = false;
        result.error_message = "电压测量失败: " + voltageResult.error;
        return result;
    }
    
    double voltage = voltageResult.value;
    
    // 测量电阻 - 使用DMM设备
    DeviceOperation resistanceConfigOp;
    resistanceConfigOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    resistanceConfigOp.parameters["range"] = "auto";
    resistanceConfigOp.timeout = 3000;
    
    if (device_manager_->submitOperation("JY8902", resistanceConfigOp)) {
        device_manager_->waitForResult("JY8902", 3000);
        
        DeviceOperation resistanceReadOp;
        resistanceReadOp.command = DeviceCommand::READ_DATA;
        resistanceReadOp.timeout = 5000;
        
        if (device_manager_->submitOperation("JY8902", resistanceReadOp)) {
            DeviceResult resistanceResult = device_manager_->waitForResult("JY8902", 5000);
            if (resistanceResult.success) {
                double resistance = resistanceResult.value;
                double impedance = resistance;
                double inductance = impedance / (2 * M_PI * test_frequency);
                
                result.primary_value = qAbs(inductance);
                result.voltage = voltage;
                result.current = voltage / (resistance + 1e-12); // 基于欧姆定律估算电流
                result.valid = true;
                
                qDebug() << "电感测量成功:" << result.primary_value << "H";
            } else {
                result.valid = false;
                result.error_message = "电阻测量失败: " + resistanceResult.error;
            }
        } else {
            result.valid = false;
            result.error_message = "电阻测量操作提交失败";
        }
    } else {
        result.valid = false;
        result.error_message = "电阻测量配置失败";
    }
    
    return result;
}

MeasurementResult FaultDiagnostic::measureDiodeCharacteristics(int channel)
{
    MeasurementResult result;

    qDebug() << "Starting diode characteristics measurement on channel:" << channel;

    // 正向偏置测试 - 使用较低电压以防止过流
    qDebug() << "Step 1: Forward bias test";
    if (!applyTestVoltage(channel, 1.5)) {  // 降低测试电压，避免过流
        result.valid = false;
        result.error_message = "无法输出正向测试电压";
        return result;
    }

    waitForStabilization(50);  // 增加稳定时间

    // 测量正向电压 - 使用DAQ设备
    qDebug() << "Measuring forward voltage using DAQ device";
    DeviceOperation forwardVoltageOp;
    forwardVoltageOp.command = DeviceCommand::READ_DATA;
    forwardVoltageOp.channel = channel;
    forwardVoltageOp.timeout = 10000;
    
    if (!device_manager_->submitOperation("JY5322", forwardVoltageOp)) {
        result.valid = false;
        result.error_message = "正向电压测量操作提交失败";
        qDebug() << "Forward voltage operation submit failed";
        return result;
    }
    
    DeviceResult forwardVoltageResult = device_manager_->waitForResult("JY5322", 10000);
    if (!forwardVoltageResult.success) {
        result.valid = false;
        result.error_message = "正向电压测量失败: " + forwardVoltageResult.error;
        qDebug() << "Forward voltage measurement failed:" << forwardVoltageResult.error;
        return result;
    }
    
    double forward_voltage = forwardVoltageResult.value;
    qDebug() << "Forward voltage measured:" << forward_voltage << "V";

    // 测量正向电阻 - 使用DMM设备
    qDebug() << "Measuring forward resistance using DMM device";
    
    // 配置DMM
    DeviceOperation forwardResConfigOp;
    forwardResConfigOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    forwardResConfigOp.parameters["range"] = "auto";
    forwardResConfigOp.parameters["timeout"] = 10000;
    forwardResConfigOp.timeout = 5000;
    
    if (!device_manager_->submitOperation("JY8902", forwardResConfigOp)) {
        result.valid = false;
        result.error_message = "正向电阻配置操作提交失败";
        return result;
    }
    
    device_manager_->waitForResult("JY8902", 5000);
    
    // 启动DMM测量
    DeviceOperation forwardResStartOp;
    forwardResStartOp.command = DeviceCommand::START_MEASUREMENT;
    forwardResStartOp.timeout = 5000;
    
    if (device_manager_->submitOperation("JY8902", forwardResStartOp)) {
        device_manager_->waitForResult("JY8902", 5000);
        
        // 发送软件触发
        DeviceOperation forwardResTriggerOp;
        forwardResTriggerOp.command = DeviceCommand::SYNC_TRIGGER;
        forwardResTriggerOp.timeout = 5000;
        
        if (device_manager_->submitOperation("JY8902", forwardResTriggerOp)) {
            device_manager_->waitForResult("JY8902", 5000);
            
            // 读取电阻值
            DeviceOperation forwardResReadOp;
            forwardResReadOp.command = DeviceCommand::READ_DATA;
            forwardResReadOp.parameters["timeout"] = 10000;
            forwardResReadOp.timeout = 10000;
            
            if (!device_manager_->submitOperation("JY8902", forwardResReadOp)) {
                result.valid = false;
                result.error_message = "正向电阻读取操作提交失败";
                qDebug() << "Forward resistance read operation submit failed";
                return result;
            }
            
            DeviceResult forwardResResult = device_manager_->waitForResult("JY8902", 10000);
            if (!forwardResResult.success) {
                result.valid = false;
                result.error_message = "正向电阻测量失败: " + forwardResResult.error;
                qDebug() << "Forward resistance measurement failed:" << forwardResResult.error;
                return result;
            }
            
            double forward_resistance = forwardResResult.value;
            qDebug() << "Forward resistance measured:" << forward_resistance << "Ω";

            // 反向偏置测试 - 使用较小的反向电压
            qDebug() << "Step 2: Reverse bias test";
            if (!applyTestVoltage(channel, -1.0)) {  // 降低反向电压，避免击穿
                result.valid = false;
                result.error_message = "无法输出反向测试电压";
                return result;
            }

            waitForStabilization(100);  // 反向测试需要更长稳定时间

            // 反向测试主要测量漏电阻 - 使用DMM设备
            DeviceOperation reverseResTriggerOp;
            reverseResTriggerOp.command = DeviceCommand::SYNC_TRIGGER;
            reverseResTriggerOp.timeout = 5000;
            
            if (device_manager_->submitOperation("JY8902", reverseResTriggerOp)) {
                device_manager_->waitForResult("JY8902", 5000);
                
                DeviceOperation reverseResReadOp;
                reverseResReadOp.command = DeviceCommand::READ_DATA;
                reverseResReadOp.parameters["timeout"] = 15000;
                reverseResReadOp.timeout = 15000;
                
                double reverse_resistance = 1e6;  // 默认高阻值
                
                if (device_manager_->submitOperation("JY8902", reverseResReadOp)) {
                    DeviceResult reverseResResult = device_manager_->waitForResult("JY8902", 15000);
                    if (reverseResResult.success) {
                        reverse_resistance = reverseResResult.value;
                        qDebug() << "Reverse resistance measured:" << reverse_resistance << "Ω";
                    } else {
                        qDebug() << "Warning: Reverse resistance measurement failed, using default value";
                    }
                } else {
                    qDebug() << "Warning: Reverse resistance read operation submit failed, using default value";
                }

                // 停止DMM测量
                DeviceOperation stopOp;
                stopOp.command = DeviceCommand::STOP_MEASUREMENT;
                stopOp.timeout = 3000;
                device_manager_->submitOperation("JY8902", stopOp);
                device_manager_->waitForResult("JY8902", 3000);

                // 恢复到0V
                applyTestVoltage(channel, 0.0);

                result.voltage = forward_voltage;
                result.current = forward_voltage / (forward_resistance + 1e-12); // 基于欧姆定律估算正向电流
                result.leakage_current = qAbs(1.0 / (reverse_resistance + 1e-12)); // 基于反向电阻估算漏电流
                result.valid = true;

                qDebug() << "Diode measurement completed successfully";
                qDebug() << "Results: Vf=" << forward_voltage << "V, Rf=" << forward_resistance << "Ω, Rr=" << reverse_resistance << "Ω";
            } else {
                result.valid = false;
                result.error_message = "反向电阻测量触发失败";
            }
        } else {
            result.valid = false;
            result.error_message = "正向电阻测量触发失败";
        }
    } else {
        result.valid = false;
        result.error_message = "正向电阻测量启动失败";
    }

    return result;
}

MeasurementResult FaultDiagnostic::measureICParameters(int channel, const ComponentSpec& spec)
{
    MeasurementResult result;
    
    qDebug() << "开始IC参数测量 - 通道:" << channel << "最大电压:" << spec.max_voltage;
    
    // 给IC上电
    if (!applyTestVoltage(channel, spec.max_voltage)) {
        result.valid = false;
        result.error_message = "无法给IC上电";
        return result;
    }
    
    waitForStabilization(100); // IC上电稳定时间
    
    // 测量电压 - 使用DAQ设备
    DeviceOperation voltageReadOp;
    voltageReadOp.command = DeviceCommand::READ_DATA;
    voltageReadOp.channel = channel;
    voltageReadOp.timeout = 5000;
    
    if (!device_manager_->submitOperation("JY5322", voltageReadOp)) {
        result.valid = false;
        result.error_message = "电压测量操作提交失败";
        return result;
    }
    
    DeviceResult voltageResult = device_manager_->waitForResult("JY5322", 5000);
    if (!voltageResult.success) {
        result.valid = false;
        result.error_message = "电压测量失败: " + voltageResult.error;
        return result;
    }
    
    double voltage = voltageResult.value;
    
    // 测量电阻 - 使用DMM设备
    DeviceOperation resistanceConfigOp;
    resistanceConfigOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    resistanceConfigOp.parameters["range"] = "auto";
    resistanceConfigOp.timeout = 3000;
    
    if (device_manager_->submitOperation("JY8902", resistanceConfigOp)) {
        device_manager_->waitForResult("JY8902", 3000);
        
        DeviceOperation resistanceReadOp;
        resistanceReadOp.command = DeviceCommand::READ_DATA;
        resistanceReadOp.timeout = 5000;
        
        if (device_manager_->submitOperation("JY8902", resistanceReadOp)) {
            DeviceResult resistanceResult = device_manager_->waitForResult("JY8902", 5000);
            if (resistanceResult.success) {
                double resistance = resistanceResult.value;
                
                result.voltage = voltage;
                result.current = voltage / (resistance + 1e-12); // 基于欧姆定律估算电流
                result.power = voltage * result.current;
                result.valid = true;
                
                qDebug() << "IC参数测量成功 - 电压:" << voltage << "V, 电阻:" << resistance << "Ω, 功率:" << result.power << "W";
            } else {
                result.valid = false;
                result.error_message = "电阻测量失败: " + resistanceResult.error;
            }
        } else {
            result.valid = false;
            result.error_message = "电阻测量操作提交失败";
        }
    } else {
        result.valid = false;
        result.error_message = "电阻测量配置失败";
    }
    
    return result;
}

bool FaultDiagnostic::checkComponentConnection(int channel)
{
    qDebug() << "检查元件连接 - 通道:" << channel;
    
    // 使用DAQ设备测量电压来检查连接
    DeviceOperation voltageReadOp;
    voltageReadOp.command = DeviceCommand::READ_DATA;
    voltageReadOp.channel = channel;
    voltageReadOp.timeout = 5000;
    
    if (!device_manager_->submitOperation("JY5322", voltageReadOp)) {
        qDebug() << "电压测量操作提交失败";
        return false;
    }
    
    DeviceResult voltageResult = device_manager_->waitForResult("JY5322", 5000);
    if (!voltageResult.success) {
        qDebug() << "电压测量失败:" << voltageResult.error;
        return false;
    }
    
    double voltage = voltageResult.value;
    qDebug() << "测量电压:" << voltage << "V";
    
    // 简单的连接检查：如果能读取到合理的电压值，认为连接正常
    bool connected = (qAbs(voltage) < 50.0);  // 电压在合理范围内
    
    qDebug() << "元件连接状态:" << (connected ? "已连接" : "未连接");
    return connected;
}

bool FaultDiagnostic::executeSyncMeasurement(const QString& syncGroup, const QStringList& deviceNames, 
                                            const QList<ComponentSpec>& components)
{
    if (!device_manager_) {
        setError("Device manager not available");
        return false;
    }
    
    if (deviceNames.size() != components.size()) {
        setError("Device names and components count mismatch");
        return false;
    }
      qDebug() << "Executing synchronized measurement for group:" << syncGroup;
    
    // 创建同步组
    device_manager_->createSyncGroup(syncGroup, deviceNames);
    
    // 准备同步操作列表
    QList<DeviceOperation> operations;
    
    for (int i = 0; i < deviceNames.size(); ++i) {
        const QString& deviceName = deviceNames[i];
        const ComponentSpec& component = components[i];
        
        DeviceOperation op;
        op.command = DeviceCommand::READ_DATA;
        op.deviceName = deviceName;
        op.channel = component.channel;
        op.syncGroup = syncGroup;
        op.timeout = 5000;
        
        // 根据组件类型设置特定参数
        switch (component.type) {
            case ComponentType::RESISTOR:
                op.parameters["function"] = static_cast<int>(JY8902_2_Wire_Resistance);
                break;
            case ComponentType::CAPACITOR:                
                op.parameters["function"] = static_cast<int>(JY8902_AC_Volts);
                op.parameters["frequency"] = 1000.0;
                break;
            default:
                op.parameters["function"] = static_cast<int>(JY8902_DC_Volts);
                break;
        }
        
        operations.append(op);
    }
      // 执行同步测量
    bool success = device_manager_->executeSync(syncGroup, operations, 10000);
    
    // 清理同步组
    device_manager_->removeSyncGroup(syncGroup);
    
    if (success) {
        qDebug() << "Synchronized measurement completed successfully";
        emit diagnosticProgress(100);
    } else {
        setError(QString("Synchronized measurement failed for group: %1").arg(syncGroup));
    }
    
    return success;
}

FaultType FaultDiagnostic::analyzeResistorFault(const ComponentSpec& spec, const MeasurementResult& measurement)
{
    double measured_value = measurement.primary_value;
    
    // 温度补偿
    measured_value = applyTemperatureCompensation(measured_value, spec.temp_coefficient, measurement.temperature);
    
    // 短路检查
    if (measured_value < spec.nominal_value * 0.1) {
        return FaultType::SHORT_CIRCUIT;
    }
    
    // 开路检查
    if (measured_value > spec.nominal_value * 10) {
        return FaultType::OPEN_CIRCUIT;
    }
    
    // 容差检查
    if (!isWithinTolerance(spec.nominal_value, measured_value, spec.tolerance)) {
        return FaultType::OUT_OF_TOLERANCE;
    }
    
    return FaultType::COMPONENT_OK;
}

FaultType FaultDiagnostic::analyzeCapacitorFault(const ComponentSpec& spec, const MeasurementResult& measurement)
{
    // 短路检查
    if (measurement.primary_value > spec.nominal_value * 100) {
        return FaultType::SHORT_CIRCUIT;
    }
    
    // 开路检查
    if (measurement.primary_value < spec.nominal_value * 0.01) {
        return FaultType::OPEN_CIRCUIT;
    }
    
    // ESR检查
    if (spec.max_esr > 0 && measurement.esr > spec.max_esr) {
        return FaultType::HIGH_ESR;
    }
    
    // 漏电流检查
    if (spec.max_leakage > 0 && measurement.leakage_current > spec.max_leakage) {
        return FaultType::HIGH_LEAKAGE;
    }
    
    // 容差检查
    if (!isWithinTolerance(spec.nominal_value, measurement.primary_value, spec.tolerance)) {
        return FaultType::OUT_OF_TOLERANCE;
    }
    
    return FaultType::COMPONENT_OK;
}

FaultType FaultDiagnostic::analyzeDiodeFault(const ComponentSpec& spec, const MeasurementResult& measurement)
{
    // 正向压降检查
    if (measurement.voltage < 0.3 || measurement.voltage > 1.2) {
        if (measurement.voltage < 0.1) {
            return FaultType::DIODE_SHORT;
        } else {
            return FaultType::DIODE_OPEN;
        }
    }
      // 反向漏电流检查
    if (measurement.leakage_current > 1e-6) { // 1μA
        return FaultType::DIODE_LEAKAGE;
    }
    
    return FaultType::COMPONENT_OK;
}

FaultType FaultDiagnostic::analyzeICFault(const ComponentSpec& spec, const MeasurementResult& measurement)
{
    // IC故障判断主要基于电源电流
    if (measurement.current > spec.max_current) {
        return FaultType::IC_OVERCURRENT;
    }
    
    // 电压范围检查
    if (measurement.voltage < spec.nominal_value * 0.9 || 
        measurement.voltage > spec.nominal_value * 1.1) {
        return FaultType::IC_LOGIC_ERROR;
    }
    
    // 功耗检查
    double power = measurement.voltage * measurement.current;
    if (power > spec.max_voltage * spec.max_current) {
        return FaultType::IC_OVERCURRENT;
    }
    
    return FaultType::COMPONENT_OK;
}

bool FaultDiagnostic::applyTestVoltage(int channel, double voltage)
{
    // 验证通道号有效性 (JY5711通常支持0-31通道)
    if (channel < 0 || channel > 31) {
        qDebug() << "Invalid channel number:" << channel << "Valid range: 0-31";
        return false;
    }
    
    // 验证电压范围 (JY5711通常支持±10V)
    if (voltage < -10.0 || voltage > 10.0) {
        qDebug() << "Invalid voltage:" << voltage << "V. Valid range: ±10V";
        return false;
    }
    
    // 使用DeviceOperation方式配置和输出电压
    DeviceOperation configOp;
    configOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    configOp.parameters["channelCount"] = 1;
    configOp.parameters["sampleRate"] = 1000000.0;
    
    QVariantList waveforms;
    QVariantMap waveform;
    waveform["channel"] = channel;
    waveform["type"] = static_cast<int>(PXIe5711_testtype::HighLevelWave); // 直流电压输出
    waveform["amplitude"] = qAbs(voltage);
    waveform["frequency"] = 1.0; // 直流，频率设为1Hz
    waveform["lowRange"] = -10.0;
    waveform["highRange"] = 10.0;
    if (voltage < 0) {
        waveform["amplitude"] = -waveform["amplitude"].toDouble(); // 负电压
    }
    waveforms.append(waveform);
    
    configOp.parameters["waveforms"] = waveforms;
    configOp.timeout = 5000;
    
    if (!device_manager_->submitOperation("JY5711", configOp)) {
        qDebug() << "Failed to submit voltage configuration operation for channel" << channel;
        return false;
    }
    
    DeviceResult configResult = device_manager_->waitForResult("JY5711", 5000);
    if (!configResult.success) {
        qDebug() << "Failed to configure voltage output for channel" << channel << ":" << configResult.error;
        return false;
    }
    
    // 开始输出电压
    DeviceOperation outputOp;
    outputOp.command = DeviceCommand::WRITE_DATA;
    outputOp.parameters["waveforms"] = waveforms;
    outputOp.parameters["sampleRate"] = 1000000;
    outputOp.parameters["samplesPerChannel"] = 1000000; // 1秒的数据，用于持续输出
    outputOp.timeout = 10000;
    
    if (!device_manager_->submitOperation("JY5711", outputOp)) {
        qDebug() << "Failed to submit voltage output operation for channel" << channel;
        return false;
    }
    
    DeviceResult outputResult = device_manager_->waitForResult("JY5711", 10000);
    if (!outputResult.success) {
        qDebug() << "Failed to start voltage output for channel" << channel << ":" << outputResult.error;
        return false;
    }
    
    // 发送软件触发开始输出
    DeviceOperation triggerOp;
    triggerOp.command = DeviceCommand::SYNC_TRIGGER;
    triggerOp.timeout = 3000;
    
    if (!device_manager_->submitOperation("JY5711", triggerOp)) {
        qDebug() << "Failed to submit trigger operation for channel" << channel;
        return false;
    }
    
    DeviceResult triggerResult = device_manager_->waitForResult("JY5711", 3000);
    if (!triggerResult.success) {
        qDebug() << "Failed to trigger voltage output for channel" << channel << ":" << triggerResult.error;
        return false;
    }
    
    qDebug() << "Successfully applied" << voltage << "V to channel" << channel;
    return true;
}

void FaultDiagnostic::waitForStabilization(int delay_ms)
{
    QThread::msleep(delay_ms);
}

bool FaultDiagnostic::isWithinTolerance(double nominal, double measured, double tolerance_percent)
{
    double min_value = nominal * (1.0 - tolerance_percent);
    double max_value = nominal * (1.0 + tolerance_percent);
    return (measured >= min_value && measured <= max_value);
}

double FaultDiagnostic::applyTemperatureCompensation(double value, double temp_coeff, double temperature)
{
    double temp_correction = temp_coeff * (temperature - 25.0);
    return value * (1.0 + temp_correction);
}

double FaultDiagnostic::calculateHealthScore(const DiagnosticResult& result)
{
    // If test passed, return high health score
    if (result.result == DiagnosticResult::PASS) {
        return 100.0; // 100%健康
    }
    
    // If test error, return zero
    if (result.result == DiagnosticResult::ERROR) {
        return 0.0;
    }
    
    // Check specific fault types for failed tests
    if (result.faultTypes.contains("SHORT_CIRCUIT") || 
        result.faultTypes.contains("OPEN_CIRCUIT") ||
        result.faultTypes.contains("DIODE_SHORT") ||
        result.faultTypes.contains("DIODE_OPEN") ||
        result.faultTypes.contains("IC_OVERCURRENT")) {
        return 0.0; // 完全故障
    }
    
    if (result.faultTypes.contains("OUT_OF_TOLERANCE") ||
        result.faultTypes.contains("HIGH_ESR")) {
        return 70.0; // 70%健康，需监控
    }
    
    if (result.faultTypes.contains("HIGH_LEAKAGE") ||
        result.faultTypes.contains("DIODE_LEAKAGE")) {
        return 40.0; // 40%健康，建议更换
    }
    
    return 0.0; // 默认返回最低分
}

QString FaultDiagnostic::generateRecommendation(const DiagnosticResult& result)
{
    // If test passed, return positive message
    if (result.result == DiagnosticResult::PASS) {
        return "元件工作正常，继续使用";
    }
    
    // If test error, return error message
    if (result.result == DiagnosticResult::ERROR) {
        return "测试出现错误，请检查设备连接或重新测试";
    }
    
    // Check specific fault types for failed tests
    if (result.faultTypes.contains("SHORT_CIRCUIT") || 
        result.faultTypes.contains("OPEN_CIRCUIT")) {
        return "元件已损坏，必须立即更换";
    }
    
    if (result.faultTypes.contains("DIODE_SHORT") ||
        result.faultTypes.contains("DIODE_OPEN")) {
        return "二极管已损坏，必须立即更换";
    }
    
    if (result.faultTypes.contains("IC_OVERCURRENT")) {
        return "IC功耗异常，可能已损坏，建议立即更换";
    }
    
    if (result.faultTypes.contains("OUT_OF_TOLERANCE")) {
        return "元件参数超差，建议在下次维护时更换";
    }
    
    if (result.faultTypes.contains("HIGH_ESR")) {
        return "电容ESR偏高，建议尽快更换以避免性能下降";
    }
    
    if (result.faultTypes.contains("HIGH_LEAKAGE") ||
        result.faultTypes.contains("DIODE_LEAKAGE")) {
        return "漏电流偏高，建议更换以防止系统异常";
    }
    
    if (result.faultTypes.contains("NO_CONNECTION")) {
        return "检查元件连接，确保接触良好";
    }
    
    return "需要进一步检查分析";
}

double FaultDiagnostic::calculateConfidence(const DiagnosticResult& result)
{
    if (!result.measurementData.valid) {
        return 0.0;
    }
    
    // 基于测量质量和故障类型计算置信度
    double base_confidence = 80.0;
    
    // 根据故障类型调整置信度
    if (result.faultTypes.contains("SHORT_CIRCUIT") ||
        result.faultTypes.contains("OPEN_CIRCUIT")) {
        return 95.0; // 短路开路比较容易判断
    }
    
    if (result.faultTypes.contains("NO_CONNECTION")) {
        return 90.0;
    }
    
    if (result.result == DiagnosticResult::PASS) {
        return base_confidence;
    }
    
    return base_confidence * 0.8;
}

QString FaultDiagnostic::faultTypeToString(FaultType type) const
{
    switch (type) {
        case FaultType::COMPONENT_OK: return "正常";
        case FaultType::SHORT_CIRCUIT: return "短路";
        case FaultType::OPEN_CIRCUIT: return "开路";
        case FaultType::OUT_OF_TOLERANCE: return "超差";
        case FaultType::HIGH_ESR: return "ESR过高";
        case FaultType::HIGH_LEAKAGE: return "漏电流过大";
        case FaultType::DIODE_SHORT: return "二极管短路";
        case FaultType::DIODE_OPEN: return "二极管开路";
        case FaultType::DIODE_LEAKAGE: return "二极管漏电";
        case FaultType::IC_OVERCURRENT: return "IC过流";
        case FaultType::IC_LOGIC_ERROR: return "IC逻辑错误";
        case FaultType::NO_CONNECTION: return "未连接";
        default: return "未知故障";
    }
}

QStringList FaultDiagnostic::getSupportedComponentTypes() const
{
    return {"电阻", "电容", "电感", "二极管", "三极管", "集成电路"};
}

void FaultDiagnostic::setError(const QString& error)
{
    last_error_ = error;
    emit errorOccurred(error);
}

void FaultDiagnostic::diagnoseComponentAsync(const QString& componentType, const QString& testId, const ComponentSpecs& specs)
{
    // 使用 QTimer 来创建异步调用
    QTimer::singleShot(0, this, [this, componentType, testId, specs]() {
        DiagnosticResult result;
        result.testId = testId;
        result.componentType = componentType;
        result.componentId = testId;
        result.timestamp = QDateTime::currentDateTime();
        result.testEquipment = "JYTEK System";
        
        // 根据组件类型创建临时 ComponentSpec 进行诊断
        ComponentSpec component;
        
        if (componentType.toLower() == "resistor") {
            component.type = ComponentType::RESISTOR;
            component.nominal_value = specs.resistance.nominal;
            component.tolerance = specs.resistance.tolerance;
            component.temp_coefficient = specs.resistance.tempCoefficient;
            result.expectedValue = specs.resistance.nominal;
            result.tolerance = specs.resistance.tolerance;
        }
        else if (componentType.toLower() == "capacitor") {
            component.type = ComponentType::CAPACITOR;
            component.nominal_value = specs.capacitance.nominal;
            component.tolerance = specs.capacitance.tolerance;
            result.expectedValue = specs.capacitance.nominal;
            result.tolerance = specs.capacitance.tolerance;
        }
        else if (componentType.toLower() == "inductor") {
            component.type = ComponentType::INDUCTOR;
            component.nominal_value = specs.inductance.nominal;
            component.tolerance = specs.inductance.tolerance;
            result.expectedValue = specs.inductance.nominal;
            result.tolerance = specs.inductance.tolerance;
        }
        else if (componentType.toLower() == "diode") {
            component.type = ComponentType::DIODE;
            component.nominal_value = specs.diode.forwardVoltage;
            component.tolerance = 0.1; // 默认10%
            result.expectedValue = specs.diode.forwardVoltage;
            result.tolerance = 0.1;
        }
        else if (componentType.toLower() == "ic") {
            component.type = ComponentType::IC;
            component.nominal_value = specs.ic.supplyVoltage;
            component.tolerance = 0.05; // 默认5%
            result.expectedValue = specs.ic.supplyVoltage;
            result.tolerance = 0.05;
        }
        else {
            component.type = ComponentType::UNKNOWN;
            result.result = DiagnosticResult::ERROR;
            result.healthScore = 0.0;
            result.confidence = 0.0;
            result.faultTypes.append("Unknown component type");
            result.notes = QString("Unsupported component type: %1").arg(componentType);
            emit diagnosticCompleted(result);
            return;
        }
        
        // 设置默认参数
        component.reference = testId;
        component.channel = 1; // 默认通道1
        component.max_voltage = 50.0; // 默认最大电压
        component.max_current = 1.0;  // 默认最大电流        // 执行诊断 (不触发信号的内部版本)
        DiagnosticResult syncResult = diagnoseComponentInternal(component);
        
        // 更新异步结果
        result.result = syncResult.result;
        result.healthScore = syncResult.healthScore;
        result.confidence = syncResult.confidence;
        result.measurementData = syncResult.measurementData;
        
        // 设置故障类型
        result.faultTypes = syncResult.faultTypes;
        
        result.notes = syncResult.notes;
        
        emit diagnosticCompleted(result);
    });
}
