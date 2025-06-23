#include "faultdiagnostic.h"
#include "signalconfiguration.h"
#include "portconfiguration.h"
#include "faultanalysis.h"
#include "testsequencemanager.h"
#include "include/JY8902.h"
#include "5711waveformconfig.h"
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QJsonArray>
#include <QtMath>
#include <QTimer>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QMessageBox>

FaultDiagnostic::FaultDiagnostic(DeviceManager* deviceManager, QObject *parent)
    : QObject(parent)
    , device_manager_(deviceManager)
    , use_threaded_manager_(false)
    , threaded_device_manager_(deviceManager)
    , currentState_(WorkflowState::IDLE)
    , wiringGuideEnabled_(true)
{
    // 初始化三大模块
    initializeModules();
    connectModuleSignals();
}

FaultDiagnostic::~FaultDiagnostic()
{
    qDebug() << "FaultDiagnostic 析构开始...";
    
    // 设置析构标志，防止新的操作
    currentState_ = WorkflowState::ERROR;
    
    try {
        // 断开所有信号连接，防止析构过程中触发槽函数
        disconnect(this, nullptr, nullptr, nullptr);
        
        // 停止所有异步操作（直接调用，不使用QMetaObject::invokeMethod）
        if (faultAnalysis_) {
            qDebug() << "停止故障分析模块...";
            
            // 断开故障分析模块的信号连接
            disconnect(faultAnalysis_.get(), nullptr, nullptr, nullptr);
            
            // 直接调用停止方法
            faultAnalysis_->stopAllAnalysis();
            
            qDebug() << "故障分析模块信号已断开";
        }
        
        // 停止端口配置模块
        if (portConfig_) {
            qDebug() << "停止端口配置的所有操作...";
            
            // 断开端口配置模块的信号连接
            disconnect(portConfig_.get(), nullptr, nullptr, nullptr);
            
            // 停止任何正在进行的端口操作（直接调用）
            // portConfig_->stopAllOperations(); // 如果这个方法存在的话
            
            qDebug() << "端口配置操作已停止";
            qDebug() << "端口配置模块信号已断开";
        }
        
        // 停止信号配置模块
        if (signalConfig_) {
            disconnect(signalConfig_.get(), nullptr, nullptr, nullptr);
            qDebug() << "信号配置模块信号已断开";
        }
        
        // 处理待处理事件，让所有信号处理完成
        QApplication::processEvents();
        QThread::msleep(50);  // 减少等待时间
        
        // 按依赖顺序释放模块（故障分析 -> 端口配置 -> 信号配置）
        if (faultAnalysis_) {
            qDebug() << "释放故障分析模块...";
            faultAnalysis_.reset();
        }
        
        if (portConfig_) {
            qDebug() << "释放端口配置模块...";
            portConfig_.reset();
        }
        
        if (signalConfig_) {
            qDebug() << "释放信号配置模块...";
            signalConfig_.reset();
        }
        
        // 最后处理事件队列
        QApplication::processEvents();
        
        qDebug() << "FaultDiagnostic 析构完成";
    } catch (const std::exception& e) {
        qCritical() << "FaultDiagnostic析构过程中发生异常:" << e.what();
    } catch (...) {
        qCritical() << "FaultDiagnostic析构过程中发生未知异常";
    }
}

DiagnosticResult FaultDiagnostic::diagnoseComponent(const ComponentSpec& component)
{
    // === 新的模块化诊断流程 ===
    
    // 发出诊断开始信号
    emit diagnosisStarted(component);
    
    // 如果模块化系统可用，使用新的工作流程
    if (signalConfig_ && portConfig_ && faultAnalysis_) {
        return diagnoseComponentWithModules(component);
    }
    
    // === 回退到传统诊断方法 ===
    
    // 首先发出接线引导信号
    emit wiringRequired(component);
    
    DiagnosticResult result = diagnoseComponentInternal(component);
    
    // 发出诊断完成信号
    emit diagnosisCompleted(result);
    emit diagnosticCompleted(result);
    
    return result;
}

// 使用模块化系统的诊断方法
DiagnosticResult FaultDiagnostic::diagnoseComponentWithModules(const ComponentSpec& component)
{
    qDebug() << "使用模块化系统诊断元件:" << component.reference;
    
    try {
        // 1. 根据元件类型设置测试方案
        QString componentTypeStr;
        switch (component.type) {
            case ComponentType::RESISTOR:
                componentTypeStr = "resistor";
                break;
            case ComponentType::CAPACITOR:
                componentTypeStr = "capacitor";
                break;
            case ComponentType::INDUCTOR:
                componentTypeStr = "inductor";
                break;
            case ComponentType::DIODE:
                componentTypeStr = "diode";
                break;
            case ComponentType::IC:
                componentTypeStr = "ic";
                break;
            default:
                throw std::runtime_error("不支持的元件类型");
        }
        
        // 2. 配置信号方案
        if (!step1_ConfigureSignals(componentTypeStr)) {
            throw std::runtime_error("信号配置失败");
        }
        
        // 3. 自动配置端口（基于传统ComponentSpec）
        if (!autoConfigurePortsFromComponentSpec(component)) {
            throw std::runtime_error("端口配置失败");
        }
        
        // 4. 执行测试
        QString testId = QString("component_test_%1_%2")
                           .arg(component.reference)
                           .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        
        currentTestId_ = testId;
        
        // 执行同步测试（简化版）
        TestData testData = executeSynchronousTest(component);
          if (!testData.valid) {
            throw std::runtime_error(testData.errorMessage.toStdString());
        }
        
        // 5. 分析结果
        AnalysisResult analysisResult = executeSynchronousAnalysis(testId, testData);
        
        // 6. 转换为传统格式
        DiagnosticResult result = convertFromAnalysisResult(analysisResult);
          // 补充传统格式的测量数据
        if (!testData.measurements.isEmpty()) {
            auto measurement = testData.measurements.first();
            result.measurementData.primary_value = measurement["primary_value"].toDouble();
            result.measurementData.voltage = measurement["voltage"].toDouble();
            result.measurementData.current = measurement["current"].toDouble();
            result.measurementData.power = measurement["power"].toDouble();
            result.measurementData.temperature = measurement["temperature"].toDouble();
            result.measurementData.esr = measurement["esr"].toDouble();
            result.measurementData.leakage_current = measurement["leakage_current"].toDouble();
            result.measurementData.valid = measurement["valid"].toBool();
            result.measurementData.error_message = measurement["error_message"].toString();        }
        
        qDebug() << "模块化诊断完成:" << result.componentId << "结果:" << result.result;
        
        // 发出诊断完成信号
        emit diagnosisCompleted(result);
        emit diagnosticCompleted(result);
        
        return result;    }
    catch (const std::exception& e) {
        qWarning() << "模块化诊断失败，回退到传统方法:" << e.what();
        
        // 回退到传统诊断方法
        DiagnosticResult result = diagnoseComponentInternal(component);
        
        // 发出信号（因为internal方法不发送信号）
        emit diagnosisCompleted(result);
        emit diagnosticCompleted(result);
        
        return result;
    }
}

// 自动端口配置方法
bool FaultDiagnostic::autoConfigurePortsFromComponentSpec(const ComponentSpec& component)
{
    if (!portConfig_) {
        return false;
    }
    
    // 创建简化的测试配置
    TestConfiguration config;
    config.testId = component.reference;
    config.timeout = 30000; // 30秒超时
    config.enableSynchronization = false;
    
    // 根据元件规格创建端口映射
    PortMapping mapping;
    mapping.deviceName = "默认设备";
    mapping.channel = component.channel;
    mapping.signalType = "测试信号";
    
    config.portMappings["测试端口"] = mapping;
    
    // 设置测试参数
    config.testParameters["voltage"] = component.test_voltage;
    config.testParameters["current"] = component.test_current;
    config.testParameters["nominal_value"] = component.nominal_value;
    config.testParameters["tolerance"] = component.tolerance;
    
    // 应用配置
    currentTestConfig_ = config;
    
    return true;
}

// 同步测试执行方法
TestData FaultDiagnostic::executeSynchronousTest(const ComponentSpec& component)
{
    TestData testData;
    testData.testId = currentTestId_;
    testData.timestamp = QDateTime::currentDateTime();
    
    try {
        // 根据元件类型执行相应的测量
        MeasurementResult measurement;
        
        switch (component.type) {
            case ComponentType::RESISTOR:
                measurement = measureResistance(component.channel, component.test_voltage);
                break;
            case ComponentType::CAPACITOR:
                measurement = measureCapacitance(component.channel, 1000.0);
                break;
            case ComponentType::INDUCTOR:
                measurement = measureInductance(component.channel, 10000.0);
                break;
            case ComponentType::DIODE:
                measurement = measureDiodeCharacteristics(component.channel);
                break;
            case ComponentType::IC:
                measurement = measureICParameters(component.channel, component);
                break;
            default:
                throw std::runtime_error("不支持的元件类型");
        }
        
        // 转换测量结果为TestData格式
        testData = convertFromMeasurementResult(measurement);
        
        qDebug() << "同步测试完成:" << component.reference << "有效:" << testData.valid;
    }
    catch (const std::exception& e) {
        testData.valid = false;
        testData.errorMessage = e.what();
        qWarning() << "同步测试失败:" << e.what();
    }
    
    return testData;
}

// 同步分析执行方法
AnalysisResult FaultDiagnostic::executeSynchronousAnalysis(const QString& testId, const TestData& testData)
{
    if (!faultAnalysis_) {
        throw std::runtime_error("故障分析模块未初始化");
    }
    
    // 执行同步分析
    AnalysisResult result = faultAnalysis_->analyzeSynchronously(testId, testData);
    
    qDebug() << "同步分析完成:" << testId << "健康度:" << result.healthScore;
    
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
    result.componentId = spec.reference;    result.componentType = "resistor";
    result.expectedValue = spec.nominal_value;
    result.tolerance = spec.tolerance_percent;
    
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
                .arg(compensated_value, 0, 'f', 2)                .arg(spec.nominal_value, 0, 'f', 2)
                .arg(spec.tolerance_percent * 100, 0, 'f', 1);
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
    result.componentId = spec.reference;    result.componentType = "capacitor";
    result.expectedValue = spec.nominal_value;
    result.tolerance = spec.tolerance_percent;
    
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
            break;        
        case FaultType::HIGH_ESR:
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
    result.componentId = spec.reference;    result.componentType = "inductor";
    result.expectedValue = spec.nominal_value;
    result.tolerance = spec.tolerance_percent;
    
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
        result.notes = "电感开路";    } else if (!isWithinTolerance(spec.nominal_value, result.measurementData.primary_value, spec.tolerance_percent)) {
        result.result = DiagnosticResult::FAIL;
        result.faultTypes.append("OUT_OF_TOLERANCE");
        result.notes = QString("电感值超差，测量值: %1mH，标称值: %2mH")
            .arg(result.measurementData.primary_value * 1000, 0, 'f', 2)
            .arg(spec.nominal_value * 1000, 0, 'f', 2);} else {
        result.result = DiagnosticResult::PASS;
        result.notes = QString("电感正常，测量值: %1mH")
            .arg(result.measurementData.primary_value * 1000, 0, 'f', 2);
    }
    
    return result;
}

DiagnosticResult FaultDiagnostic::diagnoseDiode(const ComponentSpec& spec)
{
    DiagnosticResult result;
    result.componentId = spec.reference;      result.componentType = "diode";
    result.expectedValue = spec.nominal_value;
    result.tolerance = spec.tolerance_percent;
    
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
            result.faultTypes.append("DIODE_LEAKAGE");            
            result.notes = QString("二极管反向漏电流过大: %1μA")
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
    result.componentId = spec.reference;      result.componentType = "ic";
    result.expectedValue = spec.nominal_value;
    result.tolerance = spec.tolerance_percent;
    
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
    if (!isWithinTolerance(spec.nominal_value, measured_value, spec.tolerance_percent)) {
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
    if (!isWithinTolerance(spec.nominal_value, measurement.primary_value, spec.tolerance_percent)) {
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
    if (measurement.leakage_current > 1e-6) // 1μA
        return FaultType::DIODE_LEAKAGE;
    
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
    // 验证通道号有效性 (JY5711支持0-31通道)
    if (channel < 0 || channel > 31) {
        qDebug() << "Invalid channel number:" << channel << "Valid range: 0-31";
        return false;
    }
    
    // 验证电压范围 (JY5711支持±10V)
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

// === 数据转换方法实现 ===

ComponentSpec FaultDiagnostic::convertToLegacyComponentSpec(const TestSchemeSignals& scheme, const TestConfiguration& config)
{
    ComponentSpec spec;
    
    // 基于测试方案确定元件类型
    if (scheme.name.contains("电阻", Qt::CaseInsensitive)) {
        spec.type = ComponentType::RESISTOR;
    } else if (scheme.name.contains("电容", Qt::CaseInsensitive)) {
        spec.type = ComponentType::CAPACITOR;
    } else if (scheme.name.contains("电感", Qt::CaseInsensitive)) {
        spec.type = ComponentType::INDUCTOR;
    } else if (scheme.name.contains("二极管", Qt::CaseInsensitive)) {
        spec.type = ComponentType::DIODE;
    } else if (scheme.name.contains("IC", Qt::CaseInsensitive)) {
        spec.type = ComponentType::IC;
    } else {
        spec.type = ComponentType::UNKNOWN;
    }
    
    // 从配置中提取参数
    spec.reference = config.testId;
    spec.description = scheme.description;
    
    // 从测试配置中获取测试参数
    if (!config.portMappings.isEmpty()) {
        auto firstMapping = config.portMappings.first();
        spec.channel = firstMapping.channel;
        spec.test_voltage = config.testParameters.value("voltage", 1.0).toDouble();
        spec.test_current = config.testParameters.value("current", 0.001).toDouble();
    }    // 设置默认值
    spec.nominal_value = config.testParameters.value("nominal_value", 1000.0).toDouble();
    spec.tolerance_percent = config.testParameters.value("tolerance", 0.05).toDouble();
    spec.max_voltage = config.testParameters.value("max_voltage", 10.0).toDouble();
    spec.max_current = config.testParameters.value("max_current", 0.1).toDouble();
    
    return spec;
}

DiagnosticResult FaultDiagnostic::convertFromAnalysisResult(const AnalysisResult& analysisResult)
{
    DiagnosticResult result;
    
    result.testId = analysisResult.testId;
    result.componentType = analysisResult.componentType;
    result.componentId = analysisResult.componentId;
    result.timestamp = analysisResult.timestamp;
    result.testEquipment = analysisResult.deviceInfo;
    result.notes = analysisResult.summary;
    result.healthScore = analysisResult.healthScore;
    result.confidence = analysisResult.confidence;
    
    // 转换测试结果
    if (analysisResult.faultType == "PASS" || analysisResult.faultType == "NORMAL") {
        result.result = DiagnosticResult::PASS;
    } else if (analysisResult.faultType == "ERROR" || analysisResult.faultType == "UNKNOWN") {
        result.result = DiagnosticResult::ERROR;
    } else {
        result.result = DiagnosticResult::FAIL;
    }
    
    // 转换故障类型列表
    result.faultTypes = analysisResult.details.value("fault_types", QStringList()).toStringList();
    if (result.faultTypes.isEmpty() && !analysisResult.faultType.isEmpty()) {
        result.faultTypes.append(analysisResult.faultType);
    }
    
    // 提取测量数据
    result.expectedValue = analysisResult.details.value("nominal_value", 0.0).toDouble();
    result.tolerance = analysisResult.details.value("tolerance", 0.05).toDouble();
    
    return result;
}

TestData FaultDiagnostic::convertFromMeasurementResult(const MeasurementResult& measurement)
{
    TestData testData;
    
    // 创建基本的测量数据结构
    QMap<QString, QVariant> measurementMap;
    measurementMap["primary_value"] = measurement.primary_value;
    measurementMap["voltage"] = measurement.voltage;
    measurementMap["current"] = measurement.current;
    measurementMap["power"] = measurement.power;
    measurementMap["temperature"] = measurement.temperature;
    measurementMap["esr"] = measurement.esr;
    measurementMap["leakage_current"] = measurement.leakage_current;
    measurementMap["valid"] = measurement.valid;
    measurementMap["error_message"] = measurement.error_message;
    
    testData.measurements.append(measurementMap);
    testData.timestamp = QDateTime::currentDateTime();
    testData.testId = QString("measurement_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    testData.valid = measurement.valid;
    
    if (!measurement.valid) {
        testData.errorMessage = measurement.error_message;
    }
    
    return testData;
}

void FaultDiagnostic::setError(const QString& error)
{
    last_error_ = error;
    qWarning() << "FaultDiagnostic Error:" << error;
}

// === 向后兼容的diagnoseComponent方法增强 ===

// === 高级工作流程方法实现 ===

bool FaultDiagnostic::executeFullDiagnosticWorkflow(const QString& componentType)
{
    qDebug() << "开始完整诊断工作流程:" << componentType;
      setState(WorkflowState::CONFIGURING_SIGNALS);
    emit workflowStarted(currentWorkflowType_);
    
    // 步骤1: 配置信号
    if (!step1_ConfigureSignals(componentType)) {
        setState(WorkflowState::ERROR);
        emit workflowError(currentWorkflowType_, last_error_);
        return false;
    }
    
    // 步骤2: 配置端口
    if (!step2_ConfigurePorts()) {
        setState(WorkflowState::ERROR);
        emit workflowError(currentWorkflowType_, last_error_);
        return false;
    }
    
    // 步骤3: 执行测试
    if (!step3_ExecuteTests()) {
        setState(WorkflowState::ERROR);
        emit workflowError(currentWorkflowType_, last_error_);
        return false;
    }
    
    // 步骤4: 分析结果
    if (!step4_AnalyzeResults()) {
        setState(WorkflowState::ERROR);
        emit workflowError(currentWorkflowType_, last_error_);
        return false;
    }
    
    setState(WorkflowState::COMPLETED);
    emit workflowCompleted(currentWorkflowType_);
    
    return true;
}

bool FaultDiagnostic::executeCustomWorkflow(const TestSchemeSignals& scheme, const QStringList& portList)
{
    qDebug() << "开始自定义工作流程:" << scheme.name;
      setState(WorkflowState::CONFIGURING_SIGNALS);
    currentWorkflowType_ = "CustomWorkflow";
    
    emit workflowStarted(currentWorkflowType_);
    
    // 使用自定义方案和端口列表
    // 这里可以直接跳过信号配置和端口配置步骤
    
    // 直接执行测试
    if (!step3_ExecuteTests()) {
        setState(WorkflowState::ERROR);
        emit workflowError(currentWorkflowType_, last_error_);
        return false;
    }
    
    // 分析结果
    if (!step4_AnalyzeResults()) {
        setState(WorkflowState::ERROR);
        emit workflowError(currentWorkflowType_, last_error_);
        return false;
    }
    
    setState(WorkflowState::COMPLETED);
    emit workflowCompleted(currentWorkflowType_);
    
    return true;
}

// === 分步骤工作流程实现 ===

bool FaultDiagnostic::step1_ConfigureSignals(const QString& componentType)
{
    setState(WorkflowState::CONFIGURING_SIGNALS);
    emit workflowStepCompleted(1, "配置信号");
    
    // 根据元件类型选择合适的测试方案
    QString schemeName;
    if (componentType == "resistor") {
        schemeName = "电阻测试(标准)";
    } else if (componentType == "capacitor") {
        schemeName = "电容测试";
    } else if (componentType == "inductor") {
        schemeName = "电感测试";
    } else if (componentType == "diode") {
        schemeName = "二极管测试";
    } else if (componentType == "ic") {
        schemeName = "IC测试";
    } else {
        setError(QString("不支持的元件类型: %1").arg(componentType));
        return false;
    }
    
    return setActiveTestScheme(schemeName);
}

bool FaultDiagnostic::step2_ConfigurePorts()
{
    setState(WorkflowState::CONFIGURING_PORTS);
    emit workflowStepCompleted(2, "配置端口");
    
    bool result = showPortConfigurationDialog();
    
    if (result && shouldShowWiringGuide(currentTestConfig_)) {
        setState(WorkflowState::SHOWING_WIRING_GUIDE);
        return startWiringGuide(currentTestConfig_);
    }
    
    return result;
}

bool FaultDiagnostic::step3_ExecuteTests()
{
    setState(WorkflowState::EXECUTING_TESTS);
    emit workflowStepCompleted(3, "执行测试");
    
    if (!portConfig_ || !portConfig_->getTestExecutor()) {
        setError("测试执行器未初始化");
        return false;
    }
    
    QString testId = QString("test_%1_%2")
                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
                        .arg(currentWorkflowType_);
    
    currentTestId_ = testId;
    
    emit testExecutionStarted(testId);
    
    // 执行测试
    TestExecutor* executor = portConfig_->getTestExecutor();
    bool result = executor->executeTest(testId, currentTestConfig_);
    
    if (result) {
        // 等待测试完成 - 这里应该是异步的
        // 实际实现中，测试完成会通过信号通知
        qDebug() << "测试执行启动成功:" << testId;
    } else {
        setError("测试执行失败");
        emit testExecutionFailed(testId, last_error_);
    }
    
    return result;
}

// 内部测试执行方法（简化版）
bool FaultDiagnostic::step3_ExecuteTest()
{
    setState(WorkflowState::EXECUTING_TESTS);
    emit workflowStepCompleted(3, "执行测试");
    
    if (!portConfig_) {
        setError("端口配置模块未初始化");
        return false;
    }
    
    QString testId = QString("test_%1_%2")
                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
                        .arg(currentWorkflowType_);
    
    currentTestId_ = testId;
    
    emit testExecutionStarted(testId);
    
    // 简化的测试执行：直接模拟测试完成
    QTimer::singleShot(1000, [this, testId]() {
        TestData testData;
        testData.testId = testId;
        testData.timestamp = QDateTime::currentDateTime();
        testData.valid = true;
        
        // 模拟一些测量数据
        QMap<QString, QVariant> measurement;
        measurement["primary_value"] = 1000.0; // 1kΩ
        measurement["voltage"] = 1.0;
        measurement["current"] = 0.001;
        measurement["valid"] = true;
        testData.measurements.append(measurement);
        
        onTestExecutionCompleted(testId, testData);
    });
    
    return true;
}

bool FaultDiagnostic::step4_AnalyzeResults()
{
    setState(WorkflowState::ANALYZING_RESULTS);
    emit workflowStepCompleted(4, "分析结果");
    
    if (currentTestData_.measurements.isEmpty()) {
        setError("没有测试数据可供分析");
        return false;
    }
    
    // 启动异步分析
    startFaultAnalysisAsync(currentTestId_, currentTestData_);
    
    // 异步分析，返回true表示分析已启动
    return true;
}

//=============================================================================
// 缺失方法的实现
//=============================================================================

void FaultDiagnostic::diagnoseComponentAsync(const QString& componentId, 
                                           const QString& testScheme, 
                                           const ComponentSpec& component)
{
    qDebug() << "开始异步诊断组件:" << componentId << "测试方案:" << testScheme;
    
    currentTestId_ = QString("async_test_%1_%2")
                     .arg(componentId)
                     .arg(QDateTime::currentMSecsSinceEpoch());
    
    // 设置当前组件规格
    currentComponentSpecs_ = component;
    
    setState(WorkflowState::CONFIGURING_SIGNALS);
    
    // 设置测试方案
    if (!setActiveTestScheme(testScheme)) {
        emit diagnosisCompleted(createErrorResult(currentTestId_, "设置测试方案失败"));
        return;
    }
    
    // 启动完整工作流程
    QTimer::singleShot(100, [this, componentId]() {
        bool success = executeFullDiagnosticWorkflow(componentId);
        if (!success) {
            emit diagnosisCompleted(createErrorResult(currentTestId_, "诊断工作流程执行失败"));
        }
    });
}

bool FaultDiagnostic::setActiveTestScheme(const QString& schemeName)
{    if (!signalConfig_) {
        setError("信号配置模块未初始化");
        return false;
    }
    
    // 从信号配置模块获取测试方案
    QStringList availableSchemes = signalConfig_->getAvailableTestSchemes();
    if (availableSchemes.isEmpty()) {
        return false;
    }
    
    activeTestScheme_ = schemeName;
}

bool FaultDiagnostic::showPortConfigurationDialog()
{
    if (!portConfig_) {
        setError("端口配置模块未初始化");
        return false;
    }
    
    if (activeTestScheme_.isEmpty()) {
        setError("请先选择测试方案");
        return false;
    }
    
    // 创建测试方案信号对象（简化版）
    TestSchemeSignals scheme;
    scheme.name = activeTestScheme_;
    scheme.description = "自动生成的测试方案";    // 创建端口配置对话框
    PortConfigurationDialog dialog(scheme, device_manager_, nullptr);
    
    // 模拟对话框信号连接（实际实现中需要根据具体信号名称）
    // connect(&dialog, &PortConfigurationDialog::configurationCompleted,
    //         this, &FaultDiagnostic::onPortConfigurationCompleted);
    // connect(&dialog, &PortConfigurationDialog::configurationFailed,
    //         this, &FaultDiagnostic::onPortValidationFailed);
    
    int result = dialog.exec();
    
    if (result == QDialog::Accepted) {
        currentTestConfig_ = dialog.getConfiguration();
        setState(WorkflowState::CONFIGURING_PORTS);
        return true;
    }
    
    return false;
}

void FaultDiagnostic::startFaultAnalysisAsync(const QString& testId, const TestData& testData)
{    if (!faultAnalysis_) {
        setError("故障分析模块未初始化");
        emit analysisCompleted(testId, createErrorAnalysisResult(testId, "故障分析模块未初始化"));
        return;
    }
    
    qDebug() << "启动异步故障分析:" << testId;
    setState(WorkflowState::ANALYZING_RESULTS);
    
    // 启动异步分析
    faultAnalysis_->startAnalysisAsync(testId, testData);
}

bool FaultDiagnostic::startWiringGuide(const TestConfiguration& config)
{
    setState(WorkflowState::SHOWING_WIRING_GUIDE);
    
    // 这里应该启动接线指导
    // 由于WiringGuide模块还没有实现，我们暂时模拟
    qDebug() << "启动接线指导，配置:" << config.testName;
    
    // 模拟接线指导完成
    QTimer::singleShot(2000, [this, config]() {
        onWiringGuideCompleted(config.testId);
    });
    
    return true;
}

bool FaultDiagnostic::initializeModules()
{
    qDebug() << "初始化模块...";
    
    // 检查设备管理器
    if (!device_manager_) {
        setError("设备管理器未初始化");
        return false;
    }
      // 初始化信号配置模块
    if (!signalConfig_) {
        signalConfig_ = std::make_unique<SignalConfiguration>(this);
    }
    
    // 初始化端口配置模块
    if (!portConfig_) {
        portConfig_ = std::make_unique<PortConfiguration>(device_manager_, this);
    }
    
    // 初始化故障分析模块
    if (!faultAnalysis_) {
        faultAnalysis_ = std::make_unique<FaultAnalysis>(this);
    }
    
    qDebug() << "模块初始化完成";
    return true;
}

void FaultDiagnostic::connectModuleSignals()
{
    if (!signalConfig_ || !portConfig_ || !faultAnalysis_) {
        qWarning() << "模块未完全初始化，无法连接信号";
        return;
    }
    
    // 连接信号配置模块信号 (暂时注释掉，等待实际信号实现)
    // connect(signalConfig_.get(), &SignalConfiguration::configurationChanged,
    //         this, &FaultDiagnostic::onSignalConfigurationChanged);
    
    // 连接端口配置模块信号 (暂时注释掉，等待实际信号实现)
    // connect(portConfig_.get(), &PortConfiguration::portAllocated,
    //         this, [this](const QString& device, int channel) {
    //             emit portConfigurationChanged(QString("端口已分配: %1 通道 %2").arg(device).arg(channel));
    //         });
    
    // 连接故障分析模块信号 (暂时注释掉，等待实际信号实现)
    // connect(faultAnalysis_.get(), &FaultAnalysis::analysisCompleted,
    //         this, &FaultDiagnostic::onAnalysisCompleted);
    
    qDebug() << "模块信号连接完成（部分信号暂时禁用）";
}

void FaultDiagnostic::setState(WorkflowState newState)
{
    if (currentState_ != newState) {
        WorkflowState oldState = currentState_;
        currentState_ = newState;
        
        qDebug() << "工作流状态变更:" << static_cast<int>(oldState) 
                 << "->" << static_cast<int>(newState);
        
        emit workflowStateChanged(newState);
    }
}

bool FaultDiagnostic::shouldShowWiringGuide(const TestConfiguration& config)
{
    // 检查是否需要显示接线指导
    // 1. 如果有输出端口，需要接线指导
    if (!config.outputPorts.isEmpty()) {
        return true;
    }
    
    // 2. 如果有多个输入端口，需要接线指导
    if (config.inputPorts.size() > 1) {
        return true;
    }
    
    // 3. 如果需要同步，需要接线指导
    if (config.requiresSynchronization) {
        return true;
    }
    
    return false;
}

//=============================================================================
// 私有槽方法实现
//=============================================================================

void FaultDiagnostic::onSignalConfigurationChanged()
{
    qDebug() << "信号配置已更改";
    emit signalConfigurationChanged();
}

void FaultDiagnostic::onPortConfigurationCompleted(const TestConfiguration& config)
{
    qDebug() << "端口配置完成:" << config.testName;
    currentTestConfig_ = config;
    
    setState(WorkflowState::CONFIGURING_PORTS);
    emit portConfigurationCompleted(config);
    
    // 自动进入下一步
    if (shouldShowWiringGuide(config)) {
        startWiringGuide(config);
    } else {
        // 直接进行测试执行
        step3_ExecuteTest();
    }
}

void FaultDiagnostic::onPortValidationFailed(const QString& error)
{
    qWarning() << "端口验证失败:" << error;
    setError(error);
    emit portValidationFailed(error);
}

void FaultDiagnostic::onTestExecutionCompleted(const QString& testId, const TestData& testData)
{
    qDebug() << "测试执行完成:" << testId;
    currentTestData_ = testData;
    
    setState(WorkflowState::TEST_COMPLETED);
    emit testExecutionCompleted(testId, testData);
    
    // 自动启动故障分析
    step4_AnalyzeResults();
}

void FaultDiagnostic::onTestExecutionFailed(const QString& testId, const QString& error)
{
    qWarning() << "测试执行失败:" << testId << error;
    setError(error);
    setState(WorkflowState::ERROR);
    emit testExecutionFailed(testId, error);
}

void FaultDiagnostic::onTestExecutionProgress(const QString& testId, int percentage)
{
    emit testExecutionProgress(testId, percentage);
}

void FaultDiagnostic::onAnalysisCompleted(const QString& testId, const AnalysisResult& result)
{
    qDebug() << "分析完成:" << testId << "结果:" << static_cast<int>(result.result);
    currentAnalysisResult_ = result;
    
    setState(WorkflowState::ANALYSIS_COMPLETED);
    emit analysisCompleted(testId, result);
      // 如果是通过 diagnoseComponentAsync 启动的，发射完成信号
    if (testId.startsWith("async_test_")) {
        QString componentId = testId.section('_', 2, 2);
        DiagnosticResult diagnosticResult = convertFromAnalysisResult(result);
        emit diagnosisCompleted(diagnosticResult);
    }
}

void FaultDiagnostic::onAnalysisProgress(const QString& testId, int percentage)
{
    emit analysisProgress(testId, percentage);
}

void FaultDiagnostic::onWiringGuideCompleted(const QString& testId)
{
    qDebug() << "接线指导完成:" << testId;
    setState(WorkflowState::WIRING_COMPLETED);
    emit wiringGuideCompleted(testId);
    
    // 自动进入测试执行步骤
    step3_ExecuteTest();
}

void FaultDiagnostic::onWiringVerificationCompleted(bool success, const QString& message)
{
    qDebug() << "接线验证完成:" << success << message;
    
    if (success) {
        setState(WorkflowState::WIRING_VERIFIED);
        emit wiringVerificationCompleted(true, message);
        
        // 验证成功，继续测试
        step3_ExecuteTest();
    } else {
        setError(message);
        setState(WorkflowState::ERROR);
        emit wiringVerificationCompleted(false, message);
    }
}

//=============================================================================
// 缺失的槽函数实现
//=============================================================================

void FaultDiagnostic::onTestCompleted()
{
    qDebug() << "测试完成回调";
    
    // 处理测试完成逻辑
    setState(WorkflowState::TEST_COMPLETED);
    
    // 发射测试完成信号
    emit progressUpdated(100);
    
    // 如果有待处理的测试数据，启动分析
    if (!currentTestData_.measurements.isEmpty()) {
        // 启动故障分析
        step4_AnalyzeResults();
    }
}

//=============================================================================
// 辅助方法
//=============================================================================

DiagnosticResult FaultDiagnostic::createErrorResult(const QString& testId, const QString& error)
{
    DiagnosticResult result;
    result.testId = testId;
    result.componentType = "UNKNOWN";
    result.componentId = "UNKNOWN";
    result.result = DiagnosticResult::ERROR;
    result.healthScore = 0.0;
    result.confidence = 0.0;
    result.timestamp = QDateTime::currentDateTime();
    result.notes = error;
    result.faultTypes.append("UNKNOWN_FAULT");
    
    return result;
}

AnalysisResult FaultDiagnostic::createErrorAnalysisResult(const QString& testId, const QString& error)
{
    AnalysisResult result;
    result.testId = testId;
    result.componentType = "UNKNOWN";
    result.componentId = "UNKNOWN";
    result.healthScore = 0.0;
    result.confidence = 0.0;
    result.timestamp = QDateTime::currentDateTime();
    result.summary = error;
    result.notes.append(error);
    result.faultType = "ERROR";
    
    return result;
}

// === ComponentSpecs 到 ComponentSpec 的转换函数 ===

ComponentSpec FaultDiagnostic::convertFromComponentSpecs(const ComponentSpecs& specs, const QString& componentType, const QString& reference)
{
    ComponentSpec spec;
    
    // 设置基本信息
    spec.reference = reference;
    spec.description = QString("从ComponentSpecs转换的%1").arg(componentType);
    spec.channel = 0; // 默认通道，需要后续设置
      // 根据组件类型设置参数
    if (componentType == "resistor" || componentType == "电阻") {
        spec.type = ComponentType::RESISTOR;
        spec.nominal_value = specs.resistance.nominal;
        spec.tolerance_percent = specs.resistance.tolerance;
        spec.temp_coefficient = specs.resistance.tempCoefficient;
        spec.test_voltage = 1.0; // 默认测试电压
        spec.test_current = 0.001; // 默认测试电流
        spec.max_voltage = 50.0; // 默认最大电压
        spec.max_current = 0.1; // 默认最大电流
    }    else if (componentType == "capacitor" || componentType == "电容") {
        spec.type = ComponentType::CAPACITOR;
        spec.nominal_value = specs.capacitance.nominal;
        spec.tolerance_percent = specs.capacitance.tolerance;
        spec.max_esr = specs.capacitance.esr;
        spec.max_leakage = specs.capacitance.leakageCurrent;
        spec.test_voltage = 1.0;
        spec.test_current = 0.001;
        spec.max_voltage = 25.0;
        spec.max_current = 0.05;
    }    else if (componentType == "inductor" || componentType == "电感") {
        spec.type = ComponentType::INDUCTOR;        
        spec.nominal_value = specs.inductance.nominal;
        spec.tolerance_percent = specs.inductance.tolerance;
        spec.test_voltage = 1.0;
        spec.test_current = 0.001;
        spec.max_voltage = 10.0;
        spec.max_current = 0.1;
    }    else if (componentType == "diode" || componentType == "二极管") {
        spec.type = ComponentType::DIODE;        
        spec.nominal_value = specs.diode.forwardVoltage;
        spec.tolerance_percent = 0.1; // 10% 默认容差
        spec.max_leakage = specs.diode.reverseLeakage;
        spec.max_voltage = specs.diode.breakdownVoltage;
        spec.test_voltage = 1.5;
        spec.test_current = 0.01;
        spec.max_current = 0.1;    }    else if (componentType == "ic" || componentType == "IC" || componentType == "集成电路") {
        spec.type = ComponentType::IC;
        spec.nominal_value = specs.ic.supplyVoltage;
        spec.tolerance_percent = 0.05; // 5% 默认容差
        spec.max_voltage = specs.ic.supplyVoltage;
        spec.max_current = specs.ic.supplyCurrent;
        spec.test_voltage = specs.ic.supplyVoltage;
        spec.test_current = specs.ic.supplyCurrent;
    }    else {
        spec.type = ComponentType::UNKNOWN;
        spec.nominal_value = 0.0;
        spec.tolerance_percent = 0.05;
        spec.test_voltage = 1.0;
        spec.test_current = 0.001;
        spec.max_voltage = 10.0;
        spec.max_current = 0.1;
    }
    
    return spec;
}

// === 信号配置接口方法实现 ===

QStringList FaultDiagnostic::getAvailableTestSchemes()
{
    if (!signalConfig_) {
        qWarning() << "信号配置模块未初始化";
        return QStringList();
    }
    
    // 从信号配置模块获取可用的测试方案
    return signalConfig_->getAvailableTestSchemes();
}

QString FaultDiagnostic::getActiveTestScheme() const
{
    return activeTestScheme_;
}

// === 端口配置接口方法实现 ===

bool FaultDiagnostic::isPortConfigured() const
{
    if (!portConfig_) {
        return false;
    }
    
    // 检查是否有有效的测试配置
    return !currentTestConfig_.testId.isEmpty();
}

TestConfiguration FaultDiagnostic::getCurrentTestConfiguration() const
{
    return currentTestConfig_;
}

// === 故障分析接口方法实现 ===

AnalysisResult FaultDiagnostic::getLastAnalysisResult() const
{
    return lastAnalysisResult_;
}
