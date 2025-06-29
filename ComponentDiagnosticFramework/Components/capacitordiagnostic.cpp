#include "capacitordiagnostic.h"
#include "../../devicemanager.h"
#include <QDebug>
#include <QtMath>
#include <QThread>
#include <numeric>
#include <QRandomGenerator>

CapacitorDiagnostic::CapacitorDiagnostic(DeviceManager* deviceManager, QObject* parent)
    : BaseComponentDiagnostic(deviceManager, parent)
{
    logInfo("电容器诊断模块初始化");
}

ComponentType CapacitorDiagnostic::getSupportedComponentType() const
{
    return ComponentType::CAPACITOR;
}

QString CapacitorDiagnostic::getComponentTypeName() const
{
    return "电容器";
}

QStringList CapacitorDiagnostic::getSupportedModels() const
{
    return QStringList() << "陶瓷电容" << "电解电容" << "薄膜电容" << "钽电容" << "超级电容";
}

QVector<PortRequirement> CapacitorDiagnostic::getPortRequirements(const ComponentSpec& component) const
{
    QVector<PortRequirement> requirements;
    
    // 基本端口需求
    requirements.append(PortRequirement(PortType::ANALOG_OUTPUT, 1, "交流激励信号输出"));
    requirements.append(PortRequirement(PortType::ANALOG_INPUT, 1, "电压测量输入"));
    requirements.append(PortRequirement(PortType::ANALOG_INPUT, 1, "电流测量输入"));
    
    // 如果需要测量漏电流，添加直流电源
    if (component.max_leakage > 0) {
        requirements.append(PortRequirement(PortType::POWER_OUTPUT, 1, "直流偏置电源"));
    }
    
    // 对于大容值电容，可能需要万用表进行低频或直流测量
    if (component.nominal_value > 1e-3) { // 大于1mF
        requirements.append(PortRequirement(PortType::DMM_MEASUREMENT, 2, "万用表测量"));
    }
    
    return requirements;
}

QVector<WiringConnection> CapacitorDiagnostic::generateWiringScheme(const ComponentSpec& component, 
                                                                   const QVector<PortInfo>& allocatedPorts) const
{
    QVector<WiringConnection> connections;
    
    if (allocatedPorts.size() < 3) {
        logError("分配的端口数量不足");
        return connections;
    }
    
    // 连接1：激励信号到电容正极
    WiringConnection conn1;
    conn1.componentPin = "正极";
    conn1.targetPort = allocatedPorts[0]; // 交流激励输出
    conn1.wireColor = "红色";
    conn1.instruction = "将红色导线连接电容正极到交流信号输出";
    conn1.isRequired = true;
    connections.append(conn1);
    
    // 连接2：电容正极到电压测量
    WiringConnection conn2;
    conn2.componentPin = "正极";
    conn2.targetPort = allocatedPorts[1]; // 电压测量输入
    conn2.wireColor = "黄色";
    conn2.instruction = "将黄色导线连接电容正极到电压测量输入";
    conn2.isRequired = true;
    connections.append(conn2);
    
    // 连接3：电容负极接地并连接到电流测量
    WiringConnection conn3;
    conn3.componentPin = "负极";
    conn3.targetPort = allocatedPorts[2]; // 电流测量输入
    conn3.wireColor = "黑色";
    conn3.instruction = "将黑色导线连接电容负极到电流测量输入和地";
    conn3.isRequired = true;
    connections.append(conn3);
    
    // 如果有直流偏置电源
    if (allocatedPorts.size() > 3) {
        WiringConnection conn4;
        conn4.componentPin = "正极";
        conn4.targetPort = allocatedPorts[3]; // 直流偏置电源
        conn4.wireColor = "橙色";
        conn4.instruction = "将橙色导线连接电容正极到直流偏置电源正输出（用于漏电流测试）";
        conn4.isRequired = false; // 可选连接
        connections.append(conn4);
    }
    
    return connections;
}

ComponentTestConfig CapacitorDiagnostic::configureDataAcquisition(const ComponentSpec& component,
                                                                 const QVector<PortInfo>& ports) const
{
    ComponentTestConfig config;
    // config.testName = QString("电容器测试_%1").arg(component.reference);
    
    // // 计算最优测试参数
    // CapacitorTestParams testParams = calculateOptimalTestParams(component);
    
    // // 配置输出端口（交流激励）
    // if (!ports.isEmpty()) {
    //     PortConfig outputPort;
    //     outputPort.deviceName = ports[0].deviceName;
    //     outputPort.channel = ports[0].portNumber;
    //     outputPort.signalType = SignalType::VOLTAGE_AC;
    //     outputPort.signalName = "交流激励";
    //     outputPort.amplitude = testParams.testVoltage;
    //     outputPort.frequency = testParams.testFrequencies.first();
    //     outputPort.isOutput = true;
    //     outputPort.rangeMin = -testParams.testVoltage * 1.5;
    //     outputPort.rangeMax = testParams.testVoltage * 1.5;
        
    //     config.portConfigs.append(outputPort);
    // }
    
    // // 配置电压测量端口
    // if (ports.size() > 1) {
    //     PortConfig voltagePort;
    //     voltagePort.deviceName = ports[1].deviceName;
    //     voltagePort.channel = ports[1].portNumber;
    //     voltagePort.signalType = SignalType::VOLTAGE_AC;
    //     voltagePort.signalName = "电压测量";
    //     voltagePort.isOutput = false;
    //     voltagePort.rangeMin = -testParams.testVoltage * 2;
    //     voltagePort.rangeMax = testParams.testVoltage * 2;
    //     voltagePort.samplesPerChannel = testParams.measurementPoints;
    //     voltagePort.sampleRate = 10000.0; // 10kHz采样率
        
    //     config.portConfigs.append(voltagePort);
    // }
    
    // // 配置电流测量端口
    // if (ports.size() > 2) {
    //     PortConfig currentPort;
    //     currentPort.deviceName = ports[2].deviceName;
    //     currentPort.channel = ports[2].portNumber;
    //     currentPort.signalType = SignalType::CURRENT_AC;
    //     currentPort.signalName = "电流测量";
    //     currentPort.isOutput = false;
    //     // 根据容值估算电流范围
    //     double expectedCurrent = 2 * M_PI * testParams.testFrequencies.first() *
    //                             component.nominal_value * testParams.testVoltage;
    //     currentPort.rangeMin = -expectedCurrent * 10;
    //     currentPort.rangeMax = expectedCurrent * 10;
    //     currentPort.samplesPerChannel = testParams.measurementPoints;
    //     currentPort.sampleRate = 10000.0;
        
    //     config.portConfigs.append(currentPort);
    // }
    
    // // 设置测试参数
    // config.parameters["test_frequencies"] = QVariant::fromValue(testParams.testFrequencies);
    // config.parameters["test_voltage"] = testParams.testVoltage;
    // config.parameters["dc_bias_voltage"] = testParams.dcBiasVoltage;
    // config.parameters["measurement_points"] = testParams.measurementPoints;
    // config.parameters["settling_time"] = testParams.settlingTime;
    // config.parameters["measure_leakage"] = testParams.measureLeakage;
    // config.parameters["leakage_test_voltage"] = testParams.leakageTestVoltage;
    // config.parameters["expected_capacitance"] = component.nominal_value;
    // config.parameters["tolerance"] = component.tolerance_percent;
    // config.parameters["max_esr"] = component.max_esr;
    // config.parameters["max_leakage"] = component.max_leakage;
    
    // config.requiresSynchronization = true;
    // config.syncGroup = "capacitor_measurement";
    // config.timeout = 60000; // 60秒超时
    
    return config;
}

TestData CapacitorDiagnostic::executeDataAcquisition(const ComponentTestConfig& config)
{
    TestData testData;
    testData.testId = config.testName;
    testData.timestamp = QDateTime::currentDateTime();
    testData.valid = false;
    
    if (!getDeviceManager() || !getDeviceManager()->isSystemReady()) {
        testData.errorMessage = "设备管理器未就绪";
        return testData;
    }
    
    // try {
    //     logInfo("开始执行电容器数据采集");
        
        // 获取测试参数
        // QVector<double> testFrequencies = config.parameters.value("test_frequencies").value<QVector<double>>();
        // double testVoltage = config.parameters.value("test_voltage", 1.0).toDouble();
        // int measurementPoints = config.parameters.value("measurement_points", 10).toInt();
        // double settlingTime = config.parameters.value("settling_time", 500).toDouble();
        // bool measureLeakage = config.parameters.value("measure_leakage", true).toBool();
        // double leakageTestVoltage = config.parameters.value("leakage_test_voltage", 10.0).toDouble();
        
        // QVector<FrequencyResponse> frequencyResponses;
        
        // 对每个频率进行测量
    //     for (double frequency : testFrequencies) {
    //         logInfo(QString("测量频率: %1Hz").arg(frequency));
            
    //         // 设置激励频率
    //         // 这里应该调用实际的设备配置函数
    //         // 简化实现，假设设备配置成功
            
    //         // 等待稳定
    //         QThread::msleep(settlingTime);
            
    //         QVector<double> voltages, currents, voltagePhases, currentPhases;
            
    //         // 进行多点测量以提高精度
    //         for (int i = 0; i < measurementPoints; ++i) {
    //             // 模拟测量数据（实际应该从设备获取）
    //             double freq = frequency;
                
    //             // 从配置中获取预期电容值
    //             double expectedCapacitance = config.parameters.value("expected_capacitance", 1e-6).toDouble();
                
    //             // 模拟容抗计算：Xc = 1/(2πfC)
    //             double Xc = 1.0 / (2.0 * M_PI * freq * expectedCapacitance);
                
    //             // 添加一些随机噪声模拟真实测量
    //             double noise = (QRandomGenerator::global()->bounded(100) - 50) / 1000.0;
    //             double impedance = Xc * (1.0 + noise);
                
    //             // 计算相位（理想电容为-90度）
    //             double phase = -90.0 + (QRandomGenerator::global()->bounded(100) - 50) / 10.0;
                
    //             voltages.append(impedance * qCos(phase * M_PI / 180.0));
    //             currents.append(impedance * qSin(phase * M_PI / 180.0));
    //             voltagePhases.append(phase);
    //             currentPhases.append(phase);
    //         }
            
    //         // 计算平均值
    //         double avgVoltage = std::accumulate(voltages.begin(), voltages.end(), 0.0) / voltages.size();
    //         double avgCurrent = std::accumulate(currents.begin(), currents.end(), 0.0) / currents.size();
    //         double avgVoltagePhase = std::accumulate(voltagePhases.begin(), voltagePhases.end(), 0.0) / voltagePhases.size();
    //         double avgCurrentPhase = std::accumulate(currentPhases.begin(), currentPhases.end(), 0.0) / currentPhases.size();
            
    //         // 计算频率响应
    //         FrequencyResponse response;
    //         response.frequency = frequency;
    //         response.impedance = avgVoltage / avgCurrent;
    //         response.phase = avgCurrentPhase - avgVoltagePhase;
    //         response.capacitance = calculateCapacitance(frequency, avgVoltage, avgCurrent, response.phase);
    //         response.esr = calculateESR(avgVoltage, avgCurrent, response.phase);
            
    //         frequencyResponses.append(response);
            
    //         logInfo(QString("频率 %1Hz: Z=%2Ω, φ=%3°, C=%4F, ESR=%5Ω")
    //                .arg(frequency)
    //                .arg(response.impedance)
    //                .arg(response.phase)
    //                .arg(response.capacitance)
    //                .arg(response.esr));
    //     }
        
    //     // 存储频率响应数据
    //     QVector<double> frequencies, impedances, phases, capacitances, esrValues;
    //     for (const FrequencyResponse& resp : frequencyResponses) {
    //         frequencies.append(resp.frequency);
    //         impedances.append(resp.impedance);
    //         phases.append(resp.phase);
    //         capacitances.append(resp.capacitance);
    //         esrValues.append(resp.esr);
    //     }
        
    //     testData.measurements.clear(); // 确保清空列表
        
    //     // 存储测量数据 - 修复数据结构访问
    //     QMap<QString, QVariant> measurement;
    //     measurement["frequencies"] = QVariant::fromValue(frequencies);
    //     measurement["impedances"] = QVariant::fromValue(impedances);
    //     measurement["phases"] = QVariant::fromValue(phases);
    //     measurement["capacitances"] = QVariant::fromValue(capacitances);
    //     measurement["esr_values"] = QVariant::fromValue(esrValues);
        
    //     // 计算平均容值和ESR
    //     double avgCapacitance = std::accumulate(capacitances.begin(), capacitances.end(), 0.0) / capacitances.size();
    //     double avgESR = std::accumulate(esrValues.begin(), esrValues.end(), 0.0) / esrValues.size();
        
    //     measurement["avg_capacitance"] = avgCapacitance;
    //     measurement["avg_esr"] = avgESR;
        
    //     // 漏电流测量
    //     if (measureLeakage) {
    //         logInfo("开始漏电流测量");
            
    //         // 施加直流电压并测量漏电流
    //         // 这里应该调用实际的设备操作
    //         // 简化实现，模拟漏电流测量
            
    //         QThread::msleep(1000); // 等待稳定
            
    //         double leakageCurrent = 1e-9 + (QRandomGenerator::global()->bounded(100) / 1e12); // 模拟1nA级别的漏电流
    //         measurement["leakage_current"] = leakageCurrent;
            
    //         logInfo(QString("漏电流: %1A").arg(leakageCurrent));
    //     }
        
    //     // 将测量数据添加到列表中
    //     testData.measurements.append(measurement);
        
    //     // 设置元数据
    //     testData.metadata = config.parameters;
    //     testData.metadata["measurement_method"] = "AC_impedance";
        
    //     testData.valid = true;
    //     logInfo(QString("电容器数据采集完成，平均容值: %1F, 平均ESR: %2Ω")
    //            .arg(avgCapacitance).arg(avgESR));
        
    // } catch (const std::exception& e) {
    //     testData.errorMessage = QString("数据采集异常: %1").arg(e.what());
    //     logError(testData.errorMessage);
    // }
    
    return testData;
}

ComponentDiagnosticResult CapacitorDiagnostic::analyzeFaults(const ComponentSpec& component,
                                                            const TestData& testData)
{
    ComponentDiagnosticResult result;
    result.componentId = component.reference;
    result.componentType = getComponentTypeName();
    result.timestamp = QDateTime::currentDateTime();
    
    if (!testData.valid) {
        result.isPassed = false;
        result.healthScore = 0.0;
        result.confidence = 0.0;
        result.summary = "测试数据无效";
        result.faultTypes.append("DATA_INVALID");
        return result;
    }
    
    try {
        // 获取测量数据 - 修复访问方式
        QMap<QString, QVariant> measurementData;
        if (!testData.measurements.isEmpty()) {
            measurementData = testData.measurements.first();
        }
        
        double measuredCapacitance = measurementData.value("capacitance", 0.0).toDouble();
        double measuredESR = measurementData.value("esr", 0.0).toDouble();
        double measuredTanDelta = measurementData.value("tan_delta", 0.0).toDouble();
        
        double leakageCurrent = measurementData.value("leakage_current", 0.0).toDouble();
        
        // 获取频率响应数据
        QVector<double> frequencies = measurementData.value("frequencies").value<QVector<double>>();
        QVector<double> capacitances = measurementData.value("capacitances").value<QVector<double>>();
        QVector<double> esrValues = measurementData.value("esr_values").value<QVector<double>>();
        
        // 存储测量结果
        result.measurements["measured_capacitance"] = measuredCapacitance;
        result.measurements["expected_capacitance"] = component.nominal_value;
        result.measurements["capacitance_deviation_percent"] = calculateDeviation(measuredCapacitance, component.nominal_value);
        result.measurements["measured_esr"] = measuredESR;
        result.measurements["max_esr_spec"] = component.max_esr;
        result.measurements["measured_leakage"] = leakageCurrent;
        result.measurements["max_leakage_spec"] = component.max_leakage;
        result.measurements["tolerance_percent"] = component.tolerance_percent;
        
        // 故障分析
        bool hasFault = false;
        
        // 1. 检查开路
        if (isOpenCircuit(measuredCapacitance, component.nominal_value)) {
            result.faultTypes.append("OPEN_CIRCUIT");
            result.recommendations.append("检查元件引脚连接");
            result.recommendations.append("检查电容是否内部断路");
            hasFault = true;
        }
        
        // 2. 检查短路
        if (isShortCircuit(measuredCapacitance)) {
            result.faultTypes.append("SHORT_CIRCUIT");
            result.recommendations.append("电容内部短路，需要更换");
            result.recommendations.append("检查是否有外部短路路径");
            hasFault = true;
        }
        
        // 3. 检查容值偏差
        if (!hasFault && !isWithinTolerance(measuredCapacitance, component.nominal_value, component.tolerance_percent)) {
            result.faultTypes.append("CAPACITANCE_OUT_OF_TOLERANCE");
            result.recommendations.append("电容值超出规定容差范围");
            result.recommendations.append("可能是老化或制造偏差导致");
            hasFault = true;
        }
        
        // 4. 检查ESR
        if (component.max_esr > 0 && isHighESR(measuredESR, component.max_esr)) {
            result.faultTypes.append("HIGH_ESR");
            result.recommendations.append("等效串联电阻过高");
            result.recommendations.append("电容可能老化或品质降低");
            hasFault = true;
        }
        
        // 5. 检查漏电流
        if (component.max_leakage > 0 && isHighLeakage(leakageCurrent, component.max_leakage)) {
            result.faultTypes.append("HIGH_LEAKAGE");
            result.recommendations.append("漏电流过高");
            result.recommendations.append("电容可能损坏或污染");
            hasFault = true;
        }
        
        // 6. 检查频率稳定性
        if (!capacitances.isEmpty()) {
            double stability = calculateStability(capacitances);
            result.measurements["capacitance_stability_percent"] = stability;
            
            if (stability > 5.0) { // 超过5%的频率变化认为不稳定
                result.faultTypes.append("FREQUENCY_INSTABILITY");
                result.recommendations.append("电容值在不同频率下变化较大");
                result.recommendations.append("可能是介质损耗或寄生效应");
                hasFault = true;
            }
        }
        
        // 7. 计算品质因数
        if (!frequencies.isEmpty()) {
            double testFreq = frequencies.first();
            double qualityFactor = calculateQualityFactor(measuredCapacitance, measuredESR, testFreq);
            result.measurements["quality_factor"] = qualityFactor;
            
            if (qualityFactor < 10) { // Q值过低
                result.faultTypes.append("LOW_QUALITY_FACTOR");
                result.recommendations.append("品质因数过低，损耗较大");
            }
        }
        
        // 设置总体结果
        result.isPassed = !hasFault;
        result.healthScore = calculateHealthScore(result.measurements, component);
        result.confidence = result.isPassed ? 0.90 : 0.85;
        
        // 生成分析总结
        result.summary = generateDiagnosticSummary(component, result);
        
        if (!hasFault) {
            result.recommendations.append("电容器工作正常");
        }
        
        logInfo(QString("电容器故障分析完成: %1, 结果: %2, 健康度: %3%")
               .arg(component.reference)
               .arg(result.isPassed ? "通过" : "失败")
               .arg(result.healthScore));
        
    } catch (const std::exception& e) {
        result.isPassed = false;
        result.healthScore = 0.0;
        result.confidence = 0.0;
        result.summary = QString("故障分析异常: %1").arg(e.what());
        result.faultTypes.append("ANALYSIS_ERROR");
        logError(result.summary);
    }
    
    return result;
}

bool CapacitorDiagnostic::validateComponentSpec(const ComponentSpec& component) const
{
    if (!BaseComponentDiagnostic::validateComponentSpec(component)) {
        return false;
    }
    
    // 电容器特有的验证
    if (component.nominal_value <= 0 || component.nominal_value > 1.0) { // 最大1F
        logError("电容值必须在0到1F之间");
        return false;
    }
    
    if (component.tolerance_percent <= 0 || component.tolerance_percent > 80) {
        logError("容差必须在0-80%之间");
        return false;
    }
    
    return true;
}

bool CapacitorDiagnostic::preTestSetup(const ComponentSpec& component)
{
    logInfo(QString("电容器测试预设置: %1 (%2F ±%3%)")
           .arg(component.reference)
           .arg(formatValue(component.nominal_value, "F"))
           .arg(component.tolerance_percent));
    
    // 对于电解电容，需要特别注意极性
    if (component.value.contains("电解", Qt::CaseInsensitive)) {
        logWarning("注意：电解电容具有极性，请确保正确连接正负极");
    }
    
    return true;
}

void CapacitorDiagnostic::postTestCleanup()
{
    logInfo("电容器测试后清理");
    
    // 放电处理，确保安全
    logInfo("正在对电容器进行安全放电...");
    QThread::msleep(500); // 等待放电完成
}

// === 私有方法实现 ===

double CapacitorDiagnostic::calculateCapacitance(double frequency, double voltage, double current, double phase) const
{
    if (frequency <= 0 || voltage <= 0 || current <= 0) {
        return 0.0;
    }
    
    // C = I / (2πfV) 对于理想电容器
    // 考虑相位修正
    double phaseRad = phase * M_PI / 180.0;
    double reactiveCurrent = current * qSin(qAbs(phaseRad));
    
    return reactiveCurrent / (2 * M_PI * frequency * voltage);
}

double CapacitorDiagnostic::calculateESR(double voltage, double current, double phase) const
{
    if (current <= 0) {
        return 0.0;
    }
    
    // ESR = (V_real) / I = V * cos(φ) / I
    double phaseRad = phase * M_PI / 180.0;
    double resistiveCurrent = current * qCos(phaseRad);
    
    return voltage * qCos(phaseRad) / resistiveCurrent;
}

bool CapacitorDiagnostic::isOpenCircuit(double capacitance, double expectedCapacitance) const
{
    // 如果测量电容远小于期望值，认为是开路
    return capacitance < expectedCapacitance * 0.01 || capacitance < 1e-12;
}

bool CapacitorDiagnostic::isShortCircuit(double capacitance) const
{
    // 如果电容值异常大，认为是短路
    return capacitance > 1.0; // 大于1F认为异常
}

bool CapacitorDiagnostic::isHighESR(double esr, double maxESR) const
{
    return esr > maxESR;
}

bool CapacitorDiagnostic::isHighLeakage(double leakageCurrent, double maxLeakage) const
{
    return leakageCurrent > maxLeakage;
}

double CapacitorDiagnostic::calculateQualityFactor(double capacitance, double esr, double frequency) const
{
    if (esr <= 0 || capacitance <= 0 || frequency <= 0) {
        return 0.0;
    }
    
    // Q = 1 / (2πfRC) = Xc / R
    double reactance = 1.0 / (2 * M_PI * frequency * capacitance);
    return reactance / esr;
}

double CapacitorDiagnostic::calculateStability(const QVector<double>& measurements) const
{
    if (measurements.size() < 2) {
        return 0.0;
    }
    
    double mean = std::accumulate(measurements.begin(), measurements.end(), 0.0) / measurements.size();
    double variance = 0.0;
    
    for (double value : measurements) {
        variance += qPow(value - mean, 2);
    }
    
    variance /= measurements.size();
    double stdDev = qSqrt(variance);
    
    return mean != 0 ? (stdDev / qAbs(mean)) * 100.0 : 0.0;
}

CapacitorDiagnostic::CapacitorTestParams CapacitorDiagnostic::calculateOptimalTestParams(const ComponentSpec& component) const
{
    CapacitorTestParams params;
    
    // 根据电容值选择最优测试频率
    params.testFrequencies = getOptimalTestFrequencies(component.nominal_value);
    
    // 根据电容类型调整测试电压
    if (component.value.contains("电解", Qt::CaseInsensitive)) {
        params.testVoltage = 0.5; // 电解电容使用较低电压
        params.dcBiasVoltage = 2.0; // 适当的直流偏置
        params.leakageTestVoltage = qMin(component.max_voltage * 0.8, 10.0);
    } else {
        params.testVoltage = 1.0; // 其他电容使用标准电压
        params.leakageTestVoltage = qMin(component.max_voltage * 0.9, 50.0);
    }
    
    // 根据容差要求调整测量点数
    if (component.tolerance_percent < 5.0) {
        params.measurementPoints = 20; // 高精度测量
        params.settlingTime = 1000;    // 更长稳定时间
    } else if (component.tolerance_percent < 20.0) {
        params.measurementPoints = 10; // 中等精度
        params.settlingTime = 500;
    } else {
        params.measurementPoints = 5;  // 标准测量
        params.settlingTime = 200;
    }
    
    return params;
}

QVector<double> CapacitorDiagnostic::getOptimalTestFrequencies(double nominalCapacitance) const
{
    QVector<double> frequencies;
    
    // 根据电容值选择合适的测试频率范围
    if (nominalCapacitance >= 1e-3) { // ≥1mF
        frequencies << 10 << 100 << 1000;
    } else if (nominalCapacitance >= 1e-6) { // 1µF - 1mF
        frequencies << 100 << 1000 << 10000;
    } else if (nominalCapacitance >= 1e-9) { // 1nF - 1µF
        frequencies << 1000 << 10000 << 100000;
    } else { // <1nF
        frequencies << 10000 << 100000 << 1000000;
    }
    
    return frequencies;
}

QString CapacitorDiagnostic::generateDiagnosticSummary(const ComponentSpec& component, 
                                                      const ComponentDiagnosticResult& result) const
{
    QString summary;
    
    double measured = result.measurements.value("measured_capacitance", 0.0);
    double expected = component.nominal_value;
    double deviation = result.measurements.value("capacitance_deviation_percent", 0.0);
    double measuredESR = result.measurements.value("measured_esr", 0.0);
    double leakage = result.measurements.value("measured_leakage", 0.0);
    
    summary += QString("电容器 %1 诊断结果:\n").arg(component.reference);
    summary += QString("标称值: %1\n").arg(formatValue(expected, "F"));
    summary += QString("测量值: %1\n").arg(formatValue(measured, "F"));
    summary += QString("偏差: %1\n").arg(formatPercentage(deviation));
    summary += QString("容差: ±%1\n").arg(formatPercentage(component.tolerance_percent));
    summary += QString("ESR: %1\n").arg(formatValue(measuredESR, "Ω"));
    
    if (component.max_leakage > 0) {
        summary += QString("漏电流: %1 (最大: %2)\n")
                  .arg(formatValue(leakage, "A"))
                  .arg(formatValue(component.max_leakage, "A"));
    }
    
    if (result.isPassed) {
        summary += "结论: 电容器工作正常，所有参数在规定范围内。";
    } else {
        summary += "结论: 电容器存在故障，需要进一步检查或更换。";
    }
    
    return summary;
}

CapacitorDiagnostic::ComplexNumber CapacitorDiagnostic::calculateComplexImpedance(double voltage, double current, 
                                                                                 double voltagePhase, double currentPhase) const
{
    double phaseDiff = (voltagePhase - currentPhase) * M_PI / 180.0;
    double impedanceMag = voltage / current;
    
    return ComplexNumber(impedanceMag * qCos(phaseDiff), impedanceMag * qSin(phaseDiff));
}
