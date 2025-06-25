#include "diodediagnostic.h"
#include "commontypes.h"
#include "../../devicemanager.h"
#include <QtMath>
#include <QDebug>
#include <algorithm>

// 静态常量定义
const double DiodeDiagnostic::DEFAULT_FORWARD_MAX_CURRENT = 0.1;       // 100 mA
const double DiodeDiagnostic::DEFAULT_FORWARD_MAX_VOLTAGE = 2.0;       // 2 V
const int DiodeDiagnostic::DEFAULT_FORWARD_STEPS = 50;
const double DiodeDiagnostic::DEFAULT_REVERSE_MAX_VOLTAGE = -10.0;     // -10 V
const double DiodeDiagnostic::DEFAULT_LEAKAGE_THRESHOLD = 1e-6;        // 1 µA
const double DiodeDiagnostic::DEFAULT_BREAKDOWN_MAX_VOLTAGE = -50.0;   // -50 V
const double DiodeDiagnostic::DEFAULT_BREAKDOWN_CURRENT_LIMIT = 1e-3;  // 1 mA
const double DiodeDiagnostic::DEFAULT_CAPACITANCE_FREQUENCY = 1000.0;  // 1 kHz
const double DiodeDiagnostic::DEFAULT_CAPACITANCE_AC_VOLTAGE = 0.1;    // 100 mV
const double DiodeDiagnostic::DEFAULT_PULSE_DURATION = 1e-6;           // 1 µs
const double DiodeDiagnostic::DEFAULT_PULSE_AMPLITUDE = 1.0;           // 1 V

const double DiodeDiagnostic::FORWARD_CURRENT_THRESHOLD = 1e-6;        // 1 µA
const double DiodeDiagnostic::REVERSE_CURRENT_THRESHOLD = 1e-9;        // 1 nA
const double DiodeDiagnostic::VF_TOLERANCE_PERCENT = 20.0;             // 20%
const double DiodeDiagnostic::IDEALITY_FACTOR_MIN = 1.0;
const double DiodeDiagnostic::IDEALITY_FACTOR_MAX = 3.0;

DiodeDiagnostic::DiodeDiagnostic(DeviceManager* deviceManager, QObject* parent)
    : BaseComponentDiagnostic(deviceManager, parent)
    , forwardMaxCurrent_(DEFAULT_FORWARD_MAX_CURRENT)
    , forwardMaxVoltage_(DEFAULT_FORWARD_MAX_VOLTAGE)
    , forwardCurrentSteps_(DEFAULT_FORWARD_STEPS)
    , reverseMaxVoltage_(DEFAULT_REVERSE_MAX_VOLTAGE)
    , leakageThreshold_(DEFAULT_LEAKAGE_THRESHOLD)
    , breakdownTestEnabled_(false)
    , breakdownMaxVoltage_(DEFAULT_BREAKDOWN_MAX_VOLTAGE)
    , breakdownCurrentLimit_(DEFAULT_BREAKDOWN_CURRENT_LIMIT)
    , capacitanceFrequency_(DEFAULT_CAPACITANCE_FREQUENCY)
    , capacitanceACVoltage_(DEFAULT_CAPACITANCE_AC_VOLTAGE)
    , capacitanceDCBias_(0.0)
    , dynamicTestEnabled_(false)
    , pulseDuration_(DEFAULT_PULSE_DURATION)
    , pulseAmplitude_(DEFAULT_PULSE_AMPLITUDE)
    , temperatureTestEnabled_(false)
{
    setObjectName("DiodeDiagnostic");
    testTemperatures_ = {25.0}; // 默认室温测试
}

ComponentType DiodeDiagnostic::getSupportedComponentType() const
{
    return ComponentType::DIODE;
}

QString DiodeDiagnostic::getComponentTypeName() const
{
    return "二极管";
}

QVector<PortRequirement> DiodeDiagnostic::getPortRequirements(const ComponentSpec& component) const
{
    Q_UNUSED(component)
    
    QVector<PortRequirement> requirements;
    
    // 阳极端口（正极）
    PortRequirement anodePort;
    anodePort.portType = PortType::ANALOG_OUTPUT;
    anodePort.count = 1;
    anodePort.description = "二极管阳极（正极）";
    anodePort.specs["minVoltage"] = -60.0;  // 支持击穿测试
    anodePort.specs["maxVoltage"] = 10.0;
    anodePort.specs["minCurrent"] = -0.1;
    anodePort.specs["maxCurrent"] = 0.2;    // 支持正向大电流
    requirements.append(anodePort);
    
    // 阴极端口（负极）
    PortRequirement cathodePort;
    cathodePort.portType = PortType::ANALOG_INPUT;
    cathodePort.count = 1;
    cathodePort.description = "二极管阴极（负极）";
    cathodePort.specs["minVoltage"] = -5.0;
    cathodePort.specs["maxVoltage"] = 5.0;
    cathodePort.specs["minCurrent"] = -0.1;
    cathodePort.specs["maxCurrent"] = 0.2;
    requirements.append(cathodePort);
    
    // 如果启用动态测试，需要额外的脉冲端口
    if (dynamicTestEnabled_) {
        PortRequirement pulsePort;
        pulsePort.portType = PortType::DIGITAL_OUTPUT;
        pulsePort.count = 1;
        pulsePort.description = "脉冲发生器端口";
        pulsePort.specs["minVoltage"] = -5.0;
        pulsePort.specs["maxVoltage"] = 5.0;
        pulsePort.specs["minCurrent"] = -0.01;
        pulsePort.specs["maxCurrent"] = 0.01;
        requirements.append(pulsePort);
    }
    
    return requirements;
}

QVector<WiringConnection> DiodeDiagnostic::generateWiringScheme(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts) const
{
    Q_UNUSED(component)
    
    QVector<WiringConnection> connections;
    
    if (allocatedPorts.size() >= 2) {
        // 阳极连接
        WiringConnection anodeConn;
        anodeConn.componentPin = "ANODE";
        anodeConn.targetPort = allocatedPorts[0];
        anodeConn.wireColor = "RED";
        anodeConn.instruction = "连接二极管阳极（正极）到测试端子";
        anodeConn.isRequired = true;
        connections.append(anodeConn);
        
        // 阴极连接
        WiringConnection cathodeConn;
        cathodeConn.componentPin = "CATHODE";
        cathodeConn.targetPort = allocatedPorts[1];
        cathodeConn.wireColor = "BLACK";
        cathodeConn.instruction = "连接二极管阴极（负极）到测试端子";
        cathodeConn.isRequired = true;
        connections.append(cathodeConn);
        
        // 动态测试连接（如果启用且有第三个端口）
        if (dynamicTestEnabled_ && allocatedPorts.size() >= 3) {
            WiringConnection pulseConn;
            pulseConn.componentPin = "PULSE_GEN";
            pulseConn.targetPort = allocatedPorts[2];
            pulseConn.wireColor = "YELLOW";
            pulseConn.instruction = "连接脉冲发生器到二极管阳极（AC耦合）";
            pulseConn.isRequired = false;
            connections.append(pulseConn);
        }
    }
    
    return connections;
}

ComponentTestConfig DiodeDiagnostic::configureDataAcquisition(const ComponentSpec& component, const QVector<PortInfo>& ports) const
{
    ComponentTestConfig config;
    
    if (!getDeviceManager()) {
        return config;
    }
    
    try {
        config.testName = "二极管特性测试";
        config.timeout = 30000;  // 30秒超时
        
        // 设置端口配置
        for (const auto& port : ports) {
            PortConfig portConfig;
            portConfig.deviceName = port.deviceName;
            portConfig.channel = port.portNumber;
            config.portConfigs.append(portConfig);
        }
        
        // 配置SMU（源测量单元）进行I-V测量
        config.parameters["mode"] = "VOLTAGE_SOURCE";
        config.parameters["voltage_range"] = QVariantList{reverseMaxVoltage_, forwardMaxVoltage_};
        config.parameters["current_range"] = QVariantList{-forwardMaxCurrent_, forwardMaxCurrent_};
        config.parameters["compliance_current"] = forwardMaxCurrent_;
        config.parameters["measurement_speed"] = "MEDIUM";
        config.parameters["auto_range"] = true;
        
        // 根据二极管类型调整参数
        if (component.parameters.contains("type")) {
            QString diodeType = component.parameters["type"];
            
            if (diodeType.contains("SCHOTTKY")) {
                // 肖特基二极管：低正向压降，高频特性好
                config.parameters["voltage_range"] = QVariantList{reverseMaxVoltage_, 1.0};
                config.parameters["measurement_speed"] = "FAST";
            } else if (diodeType.contains("ZENER")) {
                // 齐纳二极管：需要测试击穿特性
                config.parameters["voltage_range"] = QVariantList{-20.0, forwardMaxVoltage_};
            } else if (diodeType.contains("LED")) {
                // LED：较高正向压降
                config.parameters["voltage_range"] = QVariantList{reverseMaxVoltage_, 5.0};
            }
        }
        
        return config;
        
    } catch (const std::exception& e) {
        qWarning() << "Configuration failed:" << e.what();
        return config;
    }
}

TestData DiodeDiagnostic::executeDataAcquisition(const ComponentTestConfig& config)
{
    TestData data;
    
    try {
        QMap<QString, QVariant> measurement;
        
        // 1. 正向I-V特性测量
        QMap<double, double> forwardIV = measureIVCurve(forwardMaxCurrent_, forwardMaxVoltage_, forwardCurrentSteps_, false);
        measurement["forward_iv_curve"] = QVariant::fromValue(forwardIV);
        
        // 2. 反向I-V特性测量
        QMap<double, double> reverseIV = measureIVCurve(leakageThreshold_, reverseMaxVoltage_, 20, true);
        measurement["reverse_iv_curve"] = QVariant::fromValue(reverseIV);
        
        // 3. 开启电压检测
        double thresholdVoltage = detectThresholdVoltage(FORWARD_CURRENT_THRESHOLD);
        measurement["threshold_voltage"] = thresholdVoltage;
        
        // 4. 标准电流下的正向压降
        QVector<double> testCurrents = {0.001, 0.01, 0.1}; // 1mA, 10mA, 100mA
        QMap<double, double> forwardDrops;
        for (double current : testCurrents) {
            if (current <= forwardMaxCurrent_) {
                double vf = measureForwardDrop(current);
                if (vf > 0) {
                    forwardDrops[current] = vf;
                }
            }
        }
        measurement["forward_drops"] = QVariant::fromValue(forwardDrops);
        
        // 5. 反向漏电流测量
        QVector<double> reverseVoltages = {-1.0, -5.0, -10.0};
        QMap<double, double> leakageCurrents;
        for (double voltage : reverseVoltages) {
            if (voltage >= reverseMaxVoltage_) {
                double leakage = measureReverseCurrent(voltage);
                if (leakage >= 0) {
                    leakageCurrents[voltage] = leakage;
                }
            }
        }
        measurement["leakage_currents"] = QVariant::fromValue(leakageCurrents);
        
        // 6. 结电容测量
        double junctionCapacitance = measureJunctionCapacitance(capacitanceFrequency_, capacitanceDCBias_, capacitanceACVoltage_);
        measurement["junction_capacitance"] = junctionCapacitance;
        
        // 7. 击穿电压测试（如果启用）
        if (breakdownTestEnabled_) {
            double breakdownVoltage = measureBreakdownVoltage(breakdownCurrentLimit_);
            measurement["breakdown_voltage"] = breakdownVoltage;
        }
        
        // 8. 动态特性测试（如果启用）
        if (dynamicTestEnabled_) {
            QMap<QString, double> switchingTimes = measureSwitchingTimes(pulseAmplitude_, 1000.0);
            measurement["switching_times"] = QVariant::fromValue(switchingTimes);
        }
        
        // 9. 计算二极管参数
        if (!forwardIV.isEmpty()) {
            double idealityFactor = calculateIdealityFactor(forwardIV);
            double saturationCurrent = calculateSaturationCurrent(forwardIV);
            double seriesResistance = calculateSeriesResistance(forwardIV);
            
            measurement["ideality_factor"] = idealityFactor;
            measurement["saturation_current"] = saturationCurrent;
            measurement["series_resistance"] = seriesResistance;
        }
        
        // 10. 二极管类型检测
        QString detectedType = detectDiodeType(forwardIV);
        measurement["detected_type"] = detectedType;
        
        data.measurements.append(measurement);
        data.timestamp = QDateTime::currentDateTime();
        data.valid = true;
        
    } catch (const std::exception& e) {
        data.errorMessage = QString("Data acquisition failed: %1").arg(e.what());
        data.valid = false;
    }
    
    return data;
}

ComponentDiagnosticResult DiodeDiagnostic::analyzeFaults(const ComponentSpec& component, 
                                                        const TestData& testData)
{
    ComponentDiagnosticResult result;
    result.componentId = component.id();
    result.componentType = "DIODE";
    result.timestamp = QDateTime::currentDateTime();
    result.isPassed = true;
    result.success = true;
    
    // 检查是否有错误
    if (!testData.valid) {
        result.isPassed = false;
        result.success = false;
        result.errorMessage = testData.errorMessage;
        return result;
    }
    
    if (testData.measurements.isEmpty()) {
        result.isPassed = false;
        result.success = false;
        result.errorMessage = "No measurement data available";
        return result;
    }
    
    // 获取第一个测量数据集
    QMap<QString, QVariant> measurements = testData.measurements.first();
    
    try {
        QStringList faultMessages;
        
        // 获取标称参数
        double nominalVf = 0.7; // 默认硅二极管正向压降
        if (component.parameters.contains("forward_voltage")) {
            nominalVf = component.parameters["forward_voltage"].toDouble();
        }
        
        double maxLeakage = leakageThreshold_;
        if (component.parameters.contains("max_leakage_current")) {
            maxLeakage = component.parameters["max_leakage_current"].toDouble();
        }
        
        // 1. 分析正向特性
        if (measurements.contains("forward_iv_curve") && measurements.contains("forward_drops")) {
            QMap<QString, QVariant> forwardData;
            forwardData["iv_curve"] = measurements["forward_iv_curve"];
            forwardData["forward_drops"] = measurements["forward_drops"];
            forwardData["threshold_voltage"] = measurements.value("threshold_voltage", 0.0);
            
            auto forwardAnalysis = analyzeForwardCharacteristic(forwardData, nominalVf);
            result.analysisData["forward_analysis"] = forwardAnalysis;
            
            // 检查正向压降
            auto forwardDrops = measurements["forward_drops"].value<QMap<double, double>>();
            if (forwardDrops.contains(0.01)) { // 10mA时的压降
                double vf_10mA = forwardDrops[0.01];
                result.measurements["vf_10mA"] = vf_10mA;
                
                double deviation = qAbs(vf_10mA - nominalVf) / nominalVf * 100.0;
                result.measurements["vf_deviation_percent"] = deviation;
                
                if (deviation > VF_TOLERANCE_PERCENT) {
                    faultMessages.append(QString("正向压降偏差过大：%.3fV vs %.3fV (%.1f%%)").arg(vf_10mA).arg(nominalVf).arg(deviation));
                }
            }
        }
        
        // 2. 分析反向特性
        if (measurements.contains("reverse_iv_curve") && measurements.contains("leakage_currents")) {
            QMap<QString, QVariant> reverseData;
            reverseData["iv_curve"] = measurements["reverse_iv_curve"];
            reverseData["leakage_currents"] = measurements["leakage_currents"];
            
            auto reverseAnalysis = analyzeReverseCharacteristic(reverseData, maxLeakage);
            result.analysisData["reverse_analysis"] = reverseAnalysis;
            
            // 检查漏电流
            auto leakageCurrents = measurements["leakage_currents"].value<QMap<double, double>>();
            if (leakageCurrents.contains(-5.0)) { // -5V时的漏电流
                double leakage_5V = leakageCurrents[-5.0];
                result.measurements["leakage_5V"] = leakage_5V;
                
                if (leakage_5V > maxLeakage) {
                    faultMessages.append(QString("反向漏电流过大：%.2eA > %.2eA").arg(leakage_5V).arg(maxLeakage));
                    result.isPassed = false;
                }
            }
        }
        
        // 3. 分析开启电压
        if (measurements.contains("threshold_voltage")) {
            double thresholdVoltage = measurements["threshold_voltage"].toDouble();
            result.measurements["threshold_voltage"] = thresholdVoltage;
            
            if (thresholdVoltage <= 0 || thresholdVoltage > 2.0) {
                faultMessages.append(QString("异常的开启电压：%.3fV").arg(thresholdVoltage));
            }
        }
        
        // 4. 分析理想因子
        if (measurements.contains("ideality_factor")) {
            double idealityFactor = measurements["ideality_factor"].toDouble();
            result.measurements["ideality_factor"] = idealityFactor;
            
            if (idealityFactor < IDEALITY_FACTOR_MIN || idealityFactor > IDEALITY_FACTOR_MAX) {
                faultMessages.append(QString("理想因子异常：%.2f (正常范围: %.1f-%.1f)").arg(idealityFactor).arg(IDEALITY_FACTOR_MIN).arg(IDEALITY_FACTOR_MAX));
            }
        }
        
        // 5. 分析饱和电流
        if (measurements.contains("saturation_current")) {
            double saturationCurrent = measurements["saturation_current"].toDouble();
            result.measurements["saturation_current"] = saturationCurrent;
            
            // 饱和电流检查（通常应该很小）
            if (saturationCurrent > 1e-9) {
                faultMessages.append(QString("饱和电流异常：%.2eA").arg(saturationCurrent));
            }
        }
        
        // 6. 分析串联电阻
        if (measurements.contains("series_resistance")) {
            double seriesResistance = measurements["series_resistance"].toDouble();
            result.measurements["series_resistance"] = seriesResistance;
            
            // 串联电阻检查
            if (seriesResistance > 100.0) {
                faultMessages.append(QString("串联电阻过大：%.1fΩ").arg(seriesResistance));
            }
        }
        
        // 7. 分析结电容
        if (measurements.contains("junction_capacitance")) {
            double junctionCapacitance = measurements["junction_capacitance"].toDouble();
            if (junctionCapacitance > 0) {
                result.measurements["junction_capacitance"] = junctionCapacitance;
            }
        }
        
        // 8. 分析击穿电压（如果测试）
        if (measurements.contains("breakdown_voltage")) {
            double breakdownVoltage = measurements["breakdown_voltage"].toDouble();
            result.measurements["breakdown_voltage"] = qAbs(breakdownVoltage);
            
            if (component.parameters.contains("breakdown_voltage")) {
                double nominalBreakdown = component.parameters["breakdown_voltage"].toDouble();
                double deviation = qAbs(qAbs(breakdownVoltage) - nominalBreakdown) / nominalBreakdown * 100.0;
                result.measurements["breakdown_deviation_percent"] = deviation;
                
                if (deviation > 20.0) {
                    faultMessages.append(QString("击穿电压偏差：%.1fV vs %.1fV").arg(qAbs(breakdownVoltage)).arg(nominalBreakdown));
                }
            }
        }
        
        // 9. 分析开关特性（如果测试）
        if (measurements.contains("switching_times")) {
            auto switchingTimes = measurements["switching_times"].value<QMap<QString, double>>();
            auto switchingAnalysis = analyzeSwitchingCharacteristic(QVariantMap{{"switching_times", QVariant::fromValue(switchingTimes)}});
            result.analysisData["switching_analysis"] = switchingAnalysis;
            
            if (switchingTimes.contains("rise_time")) {
                result.measurements["rise_time"] = switchingTimes["rise_time"];
            }
            if (switchingTimes.contains("fall_time")) {
                result.measurements["fall_time"] = switchingTimes["fall_time"];
            }
        }
        
        // 10. 检查二极管方向
        bool hasForwardData = measurements.contains("forward_iv_curve") && !measurements["forward_iv_curve"].value<QMap<double, double>>().isEmpty();
        bool hasReverseData = measurements.contains("reverse_iv_curve") && !measurements["reverse_iv_curve"].value<QMap<double, double>>().isEmpty();
        
        if (hasForwardData && hasReverseData) {
            auto forwardIV = measurements["forward_iv_curve"].value<QMap<double, double>>();
            auto reverseIV = measurements["reverse_iv_curve"].value<QMap<double, double>>();
            
            double forwardCurrent = 0;
            double reverseCurrent = 0;
            
            if (!forwardIV.isEmpty()) {
                forwardCurrent = forwardIV.values().last(); // 最大正向电流
            }
            if (!reverseIV.isEmpty()) {
                reverseCurrent = reverseIV.values().last(); // 反向电流
            }
            
            int polarityCheck = checkDiodePolarity(forwardCurrent, reverseCurrent);
            if (polarityCheck == 1) {
                faultMessages.append("二极管接反");
                result.isPassed = false;
            } else if (polarityCheck == -1) {
                faultMessages.append("无法确定二极管方向");
            }
        }
        
        // 设置结果
        result.faultDescription = faultMessages.join("; ");
        
        if (faultMessages.isEmpty()) {
            result.faultType = "NORMAL";
            result.faultDescription = "二极管工作正常";
        } else {
            result.faultType = "PARAMETER_DEVIATION";
            result.faultDescription = faultMessages.join("; ");
        }
        
        // 生成建议
        QStringList recommendations;
        if (!result.isPassed) {
            recommendations.append("检查二极管连接和极性");
            recommendations.append("验证测试条件和参数");
        }
        
        if (measurements.contains("detected_type")) {
            QString detectedType = measurements["detected_type"].toString();
            if (!detectedType.isEmpty() && component.parameters.contains("type")) {
                QString expectedType = component.parameters["type"];
                if (!detectedType.contains(expectedType, Qt::CaseInsensitive)) {
                    result.recommendations.append(QString("检测到的类型(%1)与期望类型(%2)不符").arg(detectedType).arg(expectedType));
                }
            }
        }
        
        if (!recommendations.isEmpty()) {
            result.recommendations = recommendations;
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = QString("Analysis failed: %1").arg(e.what());
    }
    
    return result;
}

// === 配置方法实现 ===

void DiodeDiagnostic::setForwardTestParams(double maxCurrent, double maxVoltage, int currentSteps)
{
    forwardMaxCurrent_ = qMax(0.001, maxCurrent);
    forwardMaxVoltage_ = qMax(0.5, maxVoltage);
    forwardCurrentSteps_ = qBound(10, currentSteps, 200);
    
    qInfo() << "Forward test params set - Current:" << forwardMaxCurrent_ << "A, Voltage:" << forwardMaxVoltage_ << "V, Steps:" << forwardCurrentSteps_;
}

void DiodeDiagnostic::setReverseTestParams(double maxVoltage, double leakageThreshold)
{
    reverseMaxVoltage_ = qMin(-0.5, maxVoltage);
    leakageThreshold_ = qMax(1e-12, leakageThreshold);
    
    qInfo() << "Reverse test params set - Voltage:" << reverseMaxVoltage_ << "V, Leakage threshold:" << leakageThreshold_ << "A";
}

void DiodeDiagnostic::setBreakdownTestParams(bool enable, double maxVoltage, double currentLimit)
{
    breakdownTestEnabled_ = enable;
    breakdownMaxVoltage_ = qMin(-1.0, maxVoltage);
    breakdownCurrentLimit_ = qMax(1e-6, currentLimit);
    
    qInfo() << "Breakdown test" << (enable ? "enabled" : "disabled") 
            << "- Max voltage:" << breakdownMaxVoltage_ << "V, Current limit:" << breakdownCurrentLimit_ << "A";
}

void DiodeDiagnostic::setCapacitanceTestParams(double frequency, double acVoltage, double dcBias)
{
    capacitanceFrequency_ = qMax(100.0, frequency);
    capacitanceACVoltage_ = qBound(0.01, acVoltage, 1.0);
    capacitanceDCBias_ = qBound(-10.0, dcBias, 10.0);
    
    qInfo() << "Capacitance test params set - Frequency:" << capacitanceFrequency_ << "Hz, AC:" << capacitanceACVoltage_ << "V, DC bias:" << capacitanceDCBias_ << "V";
}

void DiodeDiagnostic::setDynamicTestParams(bool enable, double pulseWidth, double pulseAmplitude)
{
    dynamicTestEnabled_ = enable;
    pulseDuration_ = qBound(1e-9, pulseWidth, 1e-3);
    pulseAmplitude_ = qBound(0.1, pulseAmplitude, 10.0);
    
    qInfo() << "Dynamic test" << (enable ? "enabled" : "disabled") 
            << "- Pulse width:" << pulseDuration_ << "s, Amplitude:" << pulseAmplitude_ << "V";
}

void DiodeDiagnostic::setTemperatureTestParams(bool enable, const QVector<double>& temperatures)
{
    temperatureTestEnabled_ = enable;
    testTemperatures_ = temperatures;
    
    qInfo() << "Temperature test" << (enable ? "enabled" : "disabled") << "- Temperatures:" << temperatures;
}

// === 私有测量方法实现 ===

QMap<double, double> DiodeDiagnostic::measureIVCurve(double maxCurrent, double maxVoltage, int points, bool reverse)
{
    QMap<double, double> ivCurve;
    
    DeviceManager* deviceManager = getDeviceManager();
    if (!deviceManager) {
        return ivCurve;
    }
    
    try {
        // 生成测试序列
        QVector<double> voltages;
        if (reverse) {
            voltages = generateTestSequence(0.0, maxVoltage, points, false);
        } else {
            voltages = generateTestSequence(0.0, maxVoltage, points, false);
        }
        
        for (double voltage : voltages) {
            QMap<QString, QVariant> params;
            params["voltage"] = voltage;
            params["current_compliance"] = maxCurrent;
            params["measurement_type"] = "IV_SWEEP";
            
            QMap<QString, QVariant> result = deviceManager->measureSMU(params);
            
            if (result.contains("current")) {
                double current = result["current"].toDouble();
                ivCurve[voltage] = current;
            }
        }
        
    } catch (const std::exception& e) {
        qWarning() << "I-V curve measurement failed:" << e.what();
    }
    
    return ivCurve;
}

double DiodeDiagnostic::measureForwardDrop(double testCurrent)
{
    DeviceManager* deviceManager = getDeviceManager();
    if (!deviceManager) {
        return -1.0;
    }
    
    try {
        QMap<QString, QVariant> params;
        params["current"] = testCurrent;
        params["voltage_compliance"] = forwardMaxVoltage_;
        params["measurement_type"] = "CURRENT_SOURCE";
        
        QMap<QString, QVariant> result = deviceManager->measureSMU(params);
        
        if (result.contains("voltage")) {
            double voltage = result["voltage"].toDouble();
            return voltage > 0 ? voltage : -1.0;
        }
        
    } catch (const std::exception& e) {
        qWarning() << "Forward drop measurement failed:" << e.what();
    }
    
    return -1.0;
}

double DiodeDiagnostic::measureReverseCurrent(double reverseVoltage)
{
    DeviceManager* deviceManager = getDeviceManager();
    if (!deviceManager) {
        return -1.0;
    }
    
    try {
        QMap<QString, QVariant> params;
        params["voltage"] = reverseVoltage;
        params["current_compliance"] = leakageThreshold_ * 10; // 10倍阈值作为限制
        params["measurement_type"] = "VOLTAGE_SOURCE";
        
        QMap<QString, QVariant> result = deviceManager->measureSMU(params);
        
        if (result.contains("current")) {
            double current = qAbs(result["current"].toDouble());
            return current;
        }
        
    } catch (const std::exception& e) {
        qWarning() << "Reverse current measurement failed:" << e.what();
    }
    
    return -1.0;
}

double DiodeDiagnostic::measureJunctionCapacitance(double frequency, double dcBias, double acAmplitude)
{
    DeviceManager* deviceManager = getDeviceManager();
    if (!deviceManager) {
        return -1.0;
    }
    
    try {
        QMap<QString, QVariant> params;
        params["frequency"] = frequency;
        params["ac_voltage"] = acAmplitude;
        params["dc_bias"] = dcBias;
        params["measurement_type"] = "CAPACITANCE";
        
        QMap<QString, QVariant> result = deviceManager->measureLCR(params);
        
        if (result.contains("capacitance")) {
            double capacitance = result["capacitance"].toDouble();
            return capacitance > 0 ? capacitance : -1.0;
        }
        
    } catch (const std::exception& e) {
        qWarning() << "Junction capacitance measurement failed:" << e.what();
    }
    
    return -1.0;
}

double DiodeDiagnostic::detectThresholdVoltage(double currentThreshold)
{
    DeviceManager* deviceManager = getDeviceManager();
    if (!deviceManager) {
        return -1.0;
    }
    
    // 二分法寻找开启电压
    double lowVoltage = 0.0;
    double highVoltage = 2.0;
    double tolerance = 0.001; // 1mV精度
    
    while (highVoltage - lowVoltage > tolerance) {
        double testVoltage = (lowVoltage + highVoltage) / 2.0;
        
        try {
            QMap<QString, QVariant> params;
            params["voltage"] = testVoltage;
            params["current_compliance"] = forwardMaxCurrent_;
            params["measurement_type"] = "VOLTAGE_SOURCE";
            
            QMap<QString, QVariant> result = deviceManager->measureSMU(params);
            
            if (result.contains("current")) {
                double current = result["current"].toDouble();
                
                if (current >= currentThreshold) {
                    highVoltage = testVoltage;
                } else {
                    lowVoltage = testVoltage;
                }
            } else {
                break;
            }
            
        } catch (const std::exception& e) {
            qWarning() << "Threshold detection failed:" << e.what();
            break;
        }
    }
    
    return (lowVoltage + highVoltage) / 2.0;
}

double DiodeDiagnostic::measureBreakdownVoltage(double currentThreshold)
{
    DeviceManager* deviceManager = getDeviceManager();
    if (!deviceManager) {
        return -1.0;
    }
    
    // 从小的反向电压开始，逐步增加到击穿
    double startVoltage = -0.5;
    double voltageStep = -0.5;
    double currentVoltage = startVoltage;
    
    while (currentVoltage >= breakdownMaxVoltage_) {
        try {
            QMap<QString, QVariant> params;
            params["voltage"] = currentVoltage;
            params["current_compliance"] = currentThreshold;
            params["measurement_type"] = "VOLTAGE_SOURCE";
            
            QMap<QString, QVariant> result = deviceManager->measureSMU(params);
            
            if (result.contains("current")) {
                double current = qAbs(result["current"].toDouble());
                
                if (current >= currentThreshold) {
                    return currentVoltage; // 击穿电压
                }
            }
            
        } catch (const std::exception& e) {
            qWarning() << "Breakdown measurement failed:" << e.what();
            break;
        }
        
        currentVoltage += voltageStep;
    }
    
    return breakdownMaxVoltage_; // 未击穿，返回最大测试电压
}

QMap<QString, double> DiodeDiagnostic::measureSwitchingTimes(double pulseAmplitude, double loadResistance)
{
    QMap<QString, double> switchingTimes;
    
    DeviceManager* deviceManager = getDeviceManager();
    if (!deviceManager || !dynamicTestEnabled_) {
        return switchingTimes;
    }
    
    try {
        QMap<QString, QVariant> params;
        params["pulse_amplitude"] = pulseAmplitude;
        params["pulse_width"] = pulseDuration_;
        params["load_resistance"] = loadResistance;
        params["measurement_type"] = "SWITCHING_TIME";
        
        QMap<QString, QVariant> result = deviceManager->measurePulse(params);
        
        if (result.contains("rise_time")) {
            switchingTimes["rise_time"] = result["rise_time"].toDouble();
        }
        if (result.contains("fall_time")) {
            switchingTimes["fall_time"] = result["fall_time"].toDouble();
        }
        if (result.contains("turn_on_time")) {
            switchingTimes["turn_on_time"] = result["turn_on_time"].toDouble();
        }
        if (result.contains("turn_off_time")) {
            switchingTimes["turn_off_time"] = result["turn_off_time"].toDouble();
        }
        
    } catch (const std::exception& e) {
        qWarning() << "Switching time measurement failed:" << e.what();
    }
    
    return switchingTimes;
}

double DiodeDiagnostic::calculateIdealityFactor(const QMap<double, double>& ivCurve)
{
    if (ivCurve.size() < 10) {
        return -1.0;
    }
    
    // 使用肖克利方程 I = Is * (exp(qV/nkT) - 1) 计算理想因子
    // 在正向偏置下：ln(I) ≈ ln(Is) + qV/nkT
    // 斜率 = q/nkT，其中 q/kT ≈ 38.7 at 25°C
    
    const double qOverKT = 38.7; // q/kT at 25°C (V^-1)
    
    QVector<double> voltages;
    QVector<double> logCurrents;
    
    for (auto it = ivCurve.begin(); it != ivCurve.end(); ++it) {
        double voltage = it.key();
        double current = it.value();
        
        if (voltage > 0.3 && current > 1e-9) { // 只使用正向导通区域
            voltages.append(voltage);
            logCurrents.append(qLn(current));
        }
    }
    
    if (voltages.size() < 5) {
        return -1.0;
    }
    
    // 线性回归计算斜率
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int n = voltages.size();
    
    for (int i = 0; i < n; ++i) {
        sumX += voltages[i];
        sumY += logCurrents[i];
        sumXY += voltages[i] * logCurrents[i];
        sumX2 += voltages[i] * voltages[i];
    }
    
    double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    double idealityFactor = qOverKT / slope;
    
    return idealityFactor > 0 ? idealityFactor : -1.0;
}

double DiodeDiagnostic::calculateSaturationCurrent(const QMap<double, double>& ivCurve)
{
    if (ivCurve.size() < 5) {
        return -1.0;
    }
    
    // 使用最小电压点的电流来估算饱和电流
    double minVoltage = 1e6;
    double minCurrent = 0;
    
    for (auto it = ivCurve.begin(); it != ivCurve.end(); ++it) {
        double voltage = it.key();
        double current = it.value();
        
        if (voltage > 0 && voltage < minVoltage && current > 0) {
            minVoltage = voltage;
            minCurrent = current;
        }
    }
    
    if (minVoltage < 1e5 && minCurrent > 0) {
        // Is ≈ I / (exp(qV/nkT) - 1)
        double idealityFactor = calculateIdealityFactor(ivCurve);
        if (idealityFactor > 0) {
            const double qOverKT = 38.7;
            double expTerm = qExp(minVoltage * qOverKT / idealityFactor);
            return minCurrent / (expTerm - 1.0);
        }
    }
    
    return -1.0;
}

QString DiodeDiagnostic::detectDiodeType(const QMap<double, double>& ivCurve)
{
    if (ivCurve.isEmpty()) {
        return "UNKNOWN";
    }
    
    // 分析I-V特性来判断二极管类型
    double vfAt1mA = -1.0;
    double vfAt10mA = -1.0;
    
    // 找到1mA和10mA时的电压
    for (auto it = ivCurve.begin(); it != ivCurve.end(); ++it) {
        double voltage = it.key();
        double current = it.value();
        
        if (current >= 0.001 && vfAt1mA < 0) {
            vfAt1mA = voltage;
        }
        if (current >= 0.01 && vfAt10mA < 0) {
            vfAt10mA = voltage;
        }
    }
    
    if (vfAt1mA > 0) {
        if (vfAt1mA < 0.4) {
            return "SCHOTTKY";
        } else if (vfAt1mA > 1.5) {
            return "LED";
        } else if (vfAt1mA >= 0.6 && vfAt1mA <= 0.8) {
            return "SILICON_DIODE";
        } else if (vfAt1mA >= 0.2 && vfAt1mA <= 0.4) {
            return "GERMANIUM_DIODE";
        }
    }
    
    return "STANDARD";
}

// === 分析方法实现 ===

QMap<QString, QVariant> DiodeDiagnostic::analyzeIVCharacteristic(const QMap<double, double>& ivCurve, bool reverse)
{
    QMap<QString, QVariant> analysis;
    
    if (ivCurve.isEmpty()) {
        analysis["valid"] = false;
        return analysis;
    }
    
    QVector<double> voltages = ivCurve.keys().toVector();
    QVector<double> currents = ivCurve.values().toVector();
    std::sort(voltages.begin(), voltages.end());
    
    analysis["measurement_points"] = ivCurve.size();
    analysis["voltage_range"] = QVariantList{voltages.first(), voltages.last()};
    
    if (reverse) {
        // 反向特性分析
        double maxCurrent = *std::max_element(currents.begin(), currents.end());
        analysis["max_reverse_current"] = maxCurrent;
        analysis["reverse_quality"] = maxCurrent < leakageThreshold_ ? "GOOD" : "POOR";
    } else {
        // 正向特性分析
        double maxCurrent = *std::max_element(currents.begin(), currents.end());
        analysis["max_forward_current"] = maxCurrent;
        
        // 计算导通斜率
        if (voltages.size() > 2) {
            int midIndex = voltages.size() / 2;
            double deltaV = voltages.last() - voltages[midIndex];
            double deltaI = currents.last() - currents[midIndex];
            
            if (deltaV > 0) {
                double slope = deltaI / deltaV;
                analysis["forward_slope"] = slope;
                analysis["dynamic_resistance"] = 1.0 / slope;
            }
        }
    }
    
    analysis["valid"] = true;
    return analysis;
}

QMap<QString, QVariant> DiodeDiagnostic::analyzeForwardCharacteristic(const QMap<QString, QVariant>& forwardData, double nominalVf)
{
    QMap<QString, QVariant> analysis;
    
    if (!forwardData.contains("forward_drops")) {
        analysis["valid"] = false;
        return analysis;
    }
    
    auto forwardDrops = forwardData["forward_drops"].value<QMap<double, double>>();
    
    // 分析不同电流下的压降
    QMap<QString, double> vfAnalysis;
    for (auto it = forwardDrops.begin(); it != forwardDrops.end(); ++it) {
        double current = it.key();
        double voltage = it.value();
        
        QString currentStr = QString::number(current * 1000) + "mA";
        vfAnalysis[currentStr] = voltage;
    }
    
    analysis["vf_at_currents"] = QVariant::fromValue(vfAnalysis);
    
    // 温度系数估算（如果有多个测量点）
    if (forwardDrops.size() >= 2) {
        auto it1 = forwardDrops.begin();
        auto it2 = std::next(it1);
        
        double deltaI = it2.key() - it1.key();
        double deltaV = it2.value() - it1.value();
        
        if (deltaI > 0) {
            double tempCoeff = deltaV / deltaI * 1000; // mV/mA
            analysis["temperature_coefficient_estimate"] = tempCoeff;
        }
    }
    
    // 与标称值比较
    if (nominalVf > 0 && forwardDrops.contains(0.01)) {
        double measuredVf = forwardDrops[0.01];
        double deviation = (measuredVf - nominalVf) / nominalVf * 100.0;
        analysis["nominal_deviation_percent"] = deviation;
        analysis["meets_specification"] = qAbs(deviation) <= VF_TOLERANCE_PERCENT;
    }
    
    analysis["valid"] = true;
    return analysis;
}

QMap<QString, QVariant> DiodeDiagnostic::analyzeReverseCharacteristic(const QMap<QString, QVariant>& reverseData, double maxLeakage)
{
    QMap<QString, QVariant> analysis;
    
    if (!reverseData.contains("leakage_currents")) {
        analysis["valid"] = false;
        return analysis;
    }
    
    auto leakageCurrents = reverseData["leakage_currents"].value<QMap<double, double>>();
    
    // 分析不同电压下的漏电流
    double maxLeakageMeasured = 0;
    for (auto current : leakageCurrents.values()) {
        if (current > maxLeakageMeasured) {
            maxLeakageMeasured = current;
        }
    }
    
    analysis["max_leakage_current"] = maxLeakageMeasured;
    analysis["leakage_quality"] = maxLeakageMeasured <= maxLeakage ? "GOOD" : "POOR";
    
    // 电压依赖性分析
    if (leakageCurrents.size() >= 2) {
        QVector<double> voltages = leakageCurrents.keys().toVector();
        QVector<double> currents = leakageCurrents.values().toVector();
        std::sort(voltages.begin(), voltages.end());
        
        double voltageRange = voltages.last() - voltages.first();
        double currentRange = *std::max_element(currents.begin(), currents.end()) - 
                             *std::min_element(currents.begin(), currents.end());
        
        if (voltageRange > 0) {
            double sensitivity = currentRange / voltageRange;
            analysis["voltage_sensitivity"] = sensitivity;
        }
    }
    
    analysis["valid"] = true;
    return analysis;
}

QMap<QString, QVariant> DiodeDiagnostic::analyzeCapacitanceCharacteristic(const QMap<QString, QVariant>& capacitanceData)
{
    QMap<QString, QVariant> analysis;
    
    if (!capacitanceData.contains("junction_capacitance")) {
        analysis["valid"] = false;
        return analysis;
    }
    
    double junctionCapacitance = capacitanceData["junction_capacitance"].toDouble();
    
    if (junctionCapacitance > 0) {
        analysis["junction_capacitance"] = junctionCapacitance;
        
        // 电容等级评估
        if (junctionCapacitance < 10e-12) {
            analysis["capacitance_grade"] = "LOW";
            analysis["suitability"] = "HIGH_FREQUENCY";
        } else if (junctionCapacitance < 100e-12) {
            analysis["capacitance_grade"] = "MEDIUM";
            analysis["suitability"] = "GENERAL_PURPOSE";
        } else {
            analysis["capacitance_grade"] = "HIGH";
            analysis["suitability"] = "LOW_FREQUENCY";
        }
    }
    
    analysis["valid"] = true;
    return analysis;
}

QMap<QString, QVariant> DiodeDiagnostic::analyzeSwitchingCharacteristic(const QMap<QString, QVariant>& switchingData)
{
    QMap<QString, QVariant> analysis;
    
    if (!switchingData.contains("switching_times")) {
        analysis["valid"] = false;
        return analysis;
    }
    
    auto switchingTimes = switchingData["switching_times"].value<QMap<QString, double>>();
    
    // 分析开关速度
    double totalSwitchingTime = 0;
    if (switchingTimes.contains("rise_time") && switchingTimes.contains("fall_time")) {
        totalSwitchingTime = switchingTimes["rise_time"] + switchingTimes["fall_time"];
        analysis["total_switching_time"] = totalSwitchingTime;
        
        // 速度等级评估
        if (totalSwitchingTime < 10e-9) {
            analysis["speed_grade"] = "FAST";
        } else if (totalSwitchingTime < 100e-9) {
            analysis["speed_grade"] = "MEDIUM";
        } else {
            analysis["speed_grade"] = "SLOW";
        }
    }
    
    // 对称性分析
    if (switchingTimes.contains("rise_time") && switchingTimes.contains("fall_time")) {
        double riseTime = switchingTimes["rise_time"];
        double fallTime = switchingTimes["fall_time"];
        
        double asymmetry = qAbs(riseTime - fallTime) / qMax(riseTime, fallTime) * 100.0;
        analysis["switching_asymmetry_percent"] = asymmetry;
        analysis["switching_symmetry"] = asymmetry < 20.0 ? "GOOD" : "POOR";
    }
    
    analysis["valid"] = true;
    return analysis;
}

int DiodeDiagnostic::checkDiodePolarity(double forwardCurrent, double reverseCurrent)
{
    // 检查正向电流应该远大于反向电流
    if (forwardCurrent > FORWARD_CURRENT_THRESHOLD && reverseCurrent < REVERSE_CURRENT_THRESHOLD) {
        return 0; // 正确方向
    } else if (reverseCurrent > forwardCurrent * 10) {
        return 1; // 可能接反
    } else {
        return -1; // 无法确定
    }
}

// === 辅助方法实现 ===

QVector<double> DiodeDiagnostic::generateTestSequence(double start, double end, int points, bool logScale)
{
    QVector<double> sequence;
    sequence.reserve(points);
    
    if (logScale && start > 0 && end > 0) {
        double logStart = qLn(start);
        double logEnd = qLn(end);
        double logStep = (logEnd - logStart) / (points - 1);
        
        for (int i = 0; i < points; ++i) {
            double logValue = logStart + i * logStep;
            sequence.append(qExp(logValue));
        }
    } else {
        double step = (end - start) / (points - 1);
        for (int i = 0; i < points; ++i) {
            sequence.append(start + i * step);
        }
    }
    
    return sequence;
}

QMap<QString, double> DiodeDiagnostic::fitShockleyEquation(const QMap<double, double>& ivCurve)
{
    QMap<QString, double> params;
    
    // 简化的肖克利方程拟合
    // 这里提供基本实现，实际应用中可能需要更复杂的非线性拟合算法
    
    double idealityFactor = calculateIdealityFactor(ivCurve);
    double saturationCurrent = calculateSaturationCurrent(ivCurve);
    double seriesResistance = calculateSeriesResistance(ivCurve);
    
    if (idealityFactor > 0) params["n"] = idealityFactor;
    if (saturationCurrent > 0) params["Is"] = saturationCurrent;
    if (seriesResistance > 0) params["Rs"] = seriesResistance;
    
    return params;
}

double DiodeDiagnostic::calculateSeriesResistance(const QMap<double, double>& ivCurve)
{
    if (ivCurve.size() < 10) {
        return -1.0;
    }
    
    // 在高电流区域计算串联电阻
    // Rs ≈ dV/dI in the high current region
    
    QVector<double> voltages = ivCurve.keys().toVector();
    QVector<double> currents = ivCurve.values().toVector();
    std::sort(voltages.begin(), voltages.end());
    
    // 使用最后几个点计算串联电阻
    int startIndex = voltages.size() * 3 / 4; // 使用最后25%的数据点
    if (startIndex < voltages.size() - 2) {
        double deltaV = voltages.last() - voltages[startIndex];
        double deltaI = ivCurve[voltages.last()] - ivCurve[voltages[startIndex]];
        
        if (deltaI > 0) {
            return deltaV / deltaI;
        }
    }
    
    return -1.0;
}
