#include "resistordiagnostic.h"
#include "../../devicemanager.h"
#include "../../include/JY8902.h"
#include <QDebug>
#include <QtMath>
#include <QThread>
#include <QRandomGenerator>

ResistorDiagnostic::ResistorDiagnostic(DeviceManager* deviceManager, QObject* parent)
    : BaseComponentDiagnostic(deviceManager, parent)
{
    logInfo("电阻器诊断模块初始化");
}

ComponentType ResistorDiagnostic::getSupportedComponentType() const
{
    return ComponentType::RESISTOR;
}

QString ResistorDiagnostic::getComponentTypeName() const
{
    return "电阻器";
}

QStringList ResistorDiagnostic::getSupportedModels() const
{
    return QStringList() << "通用电阻" << "精密电阻" << "功率电阻" << "热敏电阻" << "光敏电阻";
}

QVector<PortRequirement> ResistorDiagnostic::getPortRequirements(const ComponentSpec& component) const
{
    QVector<PortRequirement> requirements;
    
    // 根据电阻值选择测试方法
    bool use4WireMethod = false; // 不使用4线法测量
    
    if (use4WireMethod) {
        // 4线法测量需要4个端口
        requirements.append(PortRequirement(PortType::ANALOG_OUTPUT, 1, "电流源输出+"));
        requirements.append(PortRequirement(PortType::ANALOG_OUTPUT, 1, "电流源输出-"));
        requirements.append(PortRequirement(PortType::ANALOG_INPUT, 1, "电压测量+"));
        requirements.append(PortRequirement(PortType::ANALOG_INPUT, 1, "电压测量-"));
    } else {
        // 2线法测量需要2个端口
        requirements.append(PortRequirement(PortType::ANALOG_OUTPUT, 1, "电流源输出"));
        requirements.append(PortRequirement(PortType::ANALOG_INPUT, 1, "电压测量"));
    }
    
    // 如果需要高精度测量，添加万用表端口
    if (component.nominal_value > 1e6) { // 高阻值使用万用表
        requirements.append(PortRequirement(PortType::DMM_MEASUREMENT, 2, "万用表测量"));
    }
    
    return requirements;
}

QVector<WiringConnection> ResistorDiagnostic::generateWiringScheme(const ComponentSpec& component, 
                                                                  const QVector<PortInfo>& allocatedPorts) const
{
    QVector<WiringConnection> connections;
    
    if (allocatedPorts.size() < 2) {
        logError("分配的端口数量不足");
        return connections;
    }
    
    bool use4WireMethod = component.nominal_value < 100.0;
    
    if (use4WireMethod && allocatedPorts.size() >= 4) {
        // 4线法接线
        WiringConnection conn1;
        conn1.componentPin = "引脚1";
        conn1.targetPort = allocatedPorts[0]; // 电流源+
        conn1.wireColor = "红色";
        conn1.instruction = "将红色导线连接电阻引脚1到电流源正输出";
        conn1.isRequired = true;
        connections.append(conn1);
        
        WiringConnection conn2;
        conn2.componentPin = "引脚1";
        conn2.targetPort = allocatedPorts[2]; // 电压测量+
        conn2.wireColor = "黄色";
        conn2.instruction = "将黄色导线连接电阻引脚1到电压测量正输入";
        conn2.isRequired = true;
        connections.append(conn2);
        
        WiringConnection conn3;
        conn3.componentPin = "引脚2";
        conn3.targetPort = allocatedPorts[1]; // 电流源-
        conn3.wireColor = "黑色";
        conn3.instruction = "将黑色导线连接电阻引脚2到电流源负输出";
        conn3.isRequired = true;
        connections.append(conn3);
        
        WiringConnection conn4;
        conn4.componentPin = "引脚2";
        conn4.targetPort = allocatedPorts[3]; // 电压测量-
        conn4.wireColor = "蓝色";
        conn4.instruction = "将蓝色导线连接电阻引脚2到电压测量负输入";
        conn4.isRequired = true;
        connections.append(conn4);
        
    } else {
        // 2线法接线
        WiringConnection conn1;
        conn1.componentPin = "引脚1";
        conn1.targetPort = allocatedPorts[0]; // 电流源输出
        conn1.wireColor = "红色";
        conn1.instruction = "将红色导线连接电阻引脚1到信号输出";
        conn1.isRequired = true;
        connections.append(conn1);
        
        WiringConnection conn2;
        conn2.componentPin = "引脚2";
        conn2.targetPort = allocatedPorts[1]; // 电压测量
        conn2.wireColor = "黑色";
        conn2.instruction = "将黑色导线连接电阻引脚2到信号测量输入";
        conn2.isRequired = true;
        connections.append(conn2);
    }
    
    return connections;
}

ComponentTestConfig ResistorDiagnostic::configureDataAcquisition(const ComponentSpec& component,
                                                                const QVector<PortInfo>& ports) const
{
    ComponentTestConfig config;
    config.testName = QString("电阻器测试_%1").arg(component.reference);
    
    // 计算最优测试参数
    ResistorTestParams testParams = calculateOptimalTestParams(component);
    
    // 配置端口 - 使用 PortConfig 结构
    if (!ports.isEmpty()) {
        PortConfig outputPort;
        outputPort.deviceName = ports[0].deviceName;
        outputPort.channel = ports[0].portNumber;
        outputPort.signalType = SignalType::CURRENT_DC;
        outputPort.signalName = "测试电流";
        outputPort.amplitude = testParams.testCurrent;
        outputPort.isOutput = true;
        outputPort.rangeMin = -testParams.testCurrent * 2;
        outputPort.rangeMax = testParams.testCurrent * 2;
        
        config.portConfigs.append(outputPort);
    }
    
    // 配置输入端口（电压测量）
    if (ports.size() > 1) {
        PortConfig inputPort;
        inputPort.deviceName = ports[1].deviceName;
        inputPort.channel = ports[1].portNumber;
        inputPort.signalType = SignalType::VOLTAGE_DC;
        inputPort.signalName = "测量电压";
        inputPort.isOutput = false;
        inputPort.rangeMin = -testParams.maxVoltage;
        inputPort.rangeMax = testParams.maxVoltage;
        inputPort.samplesPerChannel = testParams.measurementPoints;
        inputPort.sampleRate = 1000.0; // 1kHz采样率
        
        config.portConfigs.append(inputPort);
    }
    
    // 设置测试参数
    config.parameters["test_current"] = testParams.testCurrent;
    config.parameters["max_voltage"] = testParams.maxVoltage;
    config.parameters["measurement_points"] = testParams.measurementPoints;
    config.parameters["use_4wire_method"] = testParams.use4WireMethod;
    config.parameters["settling_time"] = testParams.settlingTime;
    config.parameters["expected_resistance"] = component.nominal_value;
    config.parameters["tolerance"] = component.tolerance_percent;
    
    config.requiresSynchronization = testParams.use4WireMethod;
    config.syncGroup = "resistor_measurement";
    config.timeout = 30000; // 30秒超时
    
    return config;
}

TestData ResistorDiagnostic::executeDataAcquisition(const ComponentTestConfig& config)
{
    TestData testData;
    testData.testId = config.testName;
    testData.timestamp = QDateTime::currentDateTime();
    testData.valid = false;
    
    if (!getDeviceManager() || !getDeviceManager()->isSystemReady()) {
        testData.errorMessage = "设备管理器未就绪";
        return testData;
    }
    
    try {
        logInfo("开始执行电阻器数据采集");
        
        // 获取测试参数
        double testCurrent = config.parameters.value("test_current", 0.001).toDouble();
        int measurementPoints = config.parameters.value("measurement_points", 10).toInt();
        double settlingTime = config.parameters.value("settling_time", 100).toDouble();
        bool use4WireMethod = config.parameters.value("use_4wire_method", false).toBool();
        
        QVector<double> voltages;
        QVector<double> currents;
        QVector<double> timestamps;
        
        // 配置设备
        for (const PortConfig& portConfig : config.portConfigs) {
            if (portConfig.isOutput) {
                // 配置电流源输出
                logInfo(QString("配置电流源: %1, 通道: %2, 电流: %3A")
                       .arg(portConfig.deviceName)
                       .arg(portConfig.channel)
                       .arg(portConfig.amplitude));
                       
                // 这里应该调用实际的设备配置函数
                // 简化实现，假设配置成功
            } else {
                // 配置电压测量
                logInfo(QString("配置电压测量: %1, 通道: %2, 量程: ±%3V")
                       .arg(portConfig.deviceName)
                       .arg(portConfig.channel)
                       .arg(portConfig.rangeMax));
            }
        }
        
        // 执行测量序列
        for (int i = 0; i < measurementPoints; ++i) {
            // 等待稳定
            QThread::msleep(settlingTime);
            
            // 记录时间戳
            timestamps.append(QDateTime::currentMSecsSinceEpoch());
              // 测量电压（模拟数据，实际应该从设备读取）
            double measuredVoltage = testCurrent * config.parameters.value("expected_resistance", 1000).toDouble();
            measuredVoltage += (QRandomGenerator::global()->bounded(100) - 50) / 1000.0; // 添加小量噪声
            
            voltages.append(measuredVoltage);
            currents.append(testCurrent);
            
            logInfo(QString("测量点 %1: U=%2V, I=%3A").arg(i+1).arg(measuredVoltage).arg(testCurrent));
        }
        
        // 存储测量数据 - 修复数据结构访问
        QMap<QString, QVariant> measurement;
        measurement["voltages"] = QVariant::fromValue(voltages);
        measurement["currents"] = QVariant::fromValue(currents);
        measurement["timestamps"] = QVariant::fromValue(timestamps);
        measurement["measurement_count"] = measurementPoints;
        
        // 计算基本统计数据
        double avgVoltage = std::accumulate(voltages.begin(), voltages.end(), 0.0) / voltages.size();
        double avgCurrent = std::accumulate(currents.begin(), currents.end(), 0.0) / currents.size();
        double measuredResistance = avgCurrent != 0 ? avgVoltage / avgCurrent : 0;
        
        measurement["avg_voltage"] = avgVoltage;
        measurement["avg_current"] = avgCurrent;
        measurement["calculated_resistance"] = measuredResistance;
        
        // 将测量数据添加到列表中
        testData.measurements.append(measurement);
        
        // 设置元数据
        testData.metadata = config.parameters;
        testData.metadata["test_method"] = use4WireMethod ? "4-wire" : "2-wire";
        
        testData.valid = true;
        logInfo(QString("数据采集完成，测量电阻: %1Ω").arg(measuredResistance));
        
    } catch (const std::exception& e) {
        testData.errorMessage = QString("数据采集异常: %1").arg(e.what());
        logError(testData.errorMessage);
    }
    
    return testData;
}

ComponentDiagnosticResult ResistorDiagnostic::analyzeFaults(const ComponentSpec& component,
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
        // 提取测量数据 - 修复访问方式
        QMap<QString, QVariant> measurementData;
        if (!testData.measurements.isEmpty()) {
            measurementData = testData.measurements.first();
        }
        
        QVector<double> voltages = measurementData.value("voltages").value<QVector<double>>();
        QVector<double> currents = measurementData.value("currents").value<QVector<double>>();
        double measuredResistance = measurementData.value("calculated_resistance", 0.0).toDouble();
        
        if (voltages.isEmpty() || currents.isEmpty()) {
            throw std::runtime_error("测量数据为空");
        }
        
        // 重新计算电阻值以确保准确性
        double calculatedResistance = calculateResistance(voltages, currents);
        
        // 存储测量结果
        result.measurements["measured_resistance"] = calculatedResistance;
        result.measurements["expected_resistance"] = component.nominal_value;
        result.measurements["deviation_percent"] = calculateDeviation(calculatedResistance, component.nominal_value);
        result.measurements["tolerance_percent"] = component.tolerance_percent;
        
        // 故障分析
        bool hasFault = false;
        
        // 1. 检查开路
        if (isOpenCircuit(calculatedResistance, component.nominal_value)) {
            result.faultTypes.append("OPEN_CIRCUIT");
            result.recommendations.append("检查元件引脚连接");
            result.recommendations.append("检查元件是否烧断");
            hasFault = true;
        }
        
        // 2. 检查短路
        if (isShortCircuit(calculatedResistance)) {
            result.faultTypes.append("SHORT_CIRCUIT");
            result.recommendations.append("检查是否有导线短路");
            result.recommendations.append("检查元件是否被导体物质污染");
            hasFault = true;
        }
        
        // 3. 检查阻值偏差
        if (!hasFault && isOutOfTolerance(calculatedResistance, component.nominal_value, component.tolerance_percent)) {
            result.faultTypes.append("OUT_OF_TOLERANCE");
            result.recommendations.append("元件阻值超出规定容差范围");
            result.recommendations.append("建议更换元件");
            hasFault = true;
        }
        
        // 4. 计算稳定性
        double stability = calculateStability(voltages);
        result.measurements["stability_percent"] = stability;
        
        if (stability > 2.0) { // 超过2%的波动认为不稳定
            result.faultTypes.append("UNSTABLE_VALUE");
            result.recommendations.append("元件值不稳定，可能老化或损坏");
            hasFault = true;
        }
        
        // 设置总体结果
        result.isPassed = !hasFault;
        result.healthScore = calculateHealthScore(result.measurements, component);
        result.confidence = result.isPassed ? 0.95 : 0.90;
        
        // 生成分析总结
        result.summary = generateDiagnosticSummary(component, result);
        
        if (!hasFault) {
            result.recommendations.append("元件工作正常");
        }
        
        logInfo(QString("电阻器故障分析完成: %1, 结果: %2, 健康度: %3%")
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

bool ResistorDiagnostic::validateComponentSpec(const ComponentSpec& component) const
{
    if (!BaseComponentDiagnostic::validateComponentSpec(component)) {
        return false;
    }
    
    // 电阻器特有的验证
    if (component.nominal_value <= 0 || component.nominal_value > 1e12) {
        logError("电阻值必须在0到1TΩ之间");
        return false;
    }
    
    if (component.tolerance_percent <= 0 || component.tolerance_percent > 50) {
        logError("容差必须在0-50%之间");
        return false;
    }
    
    return true;
}

bool ResistorDiagnostic::preTestSetup(const ComponentSpec& component)
{
    logInfo(QString("电阻器测试预设置: %1 (%2Ω ±%3%)")
           .arg(component.reference)
           .arg(formatValue(component.nominal_value, "Ω"))
           .arg(component.tolerance_percent));
    
    // 这里可以添加特定的预设置操作
    // 例如：设备校准、温度稳定等
    
    return true;
}

void ResistorDiagnostic::postTestCleanup()
{
    logInfo("电阻器测试后清理");
    
    // 这里可以添加清理操作
    // 例如：关闭电流源、复位设备状态等
}

// === 私有方法实现 ===

double ResistorDiagnostic::calculateResistance(const QVector<double>& voltages, const QVector<double>& currents) const
{
    if (voltages.size() != currents.size() || voltages.isEmpty()) {
        return 0.0;
    }
    
    double sumR = 0.0;
    int validPoints = 0;
    
    for (int i = 0; i < voltages.size(); ++i) {
        if (qAbs(currents[i]) > 1e-12) { // 避免除零
            sumR += voltages[i] / currents[i];
            validPoints++;
        }
    }
    
    return validPoints > 0 ? sumR / validPoints : 0.0;
}

bool ResistorDiagnostic::isOpenCircuit(double resistance, double expectedResistance) const
{
    // 如果测量电阻超过期望值的1000倍，认为是开路
    return resistance > expectedResistance * 1000 || resistance > 1e9;
}

bool ResistorDiagnostic::isShortCircuit(double resistance) const
{
    // 如果电阻小于1mΩ，认为是短路
    return resistance < 1e-3;
}

bool ResistorDiagnostic::isOutOfTolerance(double measured, double expected, double tolerancePercent) const
{
    return !isWithinTolerance(measured, expected, tolerancePercent);
}

double ResistorDiagnostic::calculateStability(const QVector<double>& measurements) const
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

double ResistorDiagnostic::calculateTemperatureCoefficient(const QMap<double, double>& tempResistanceMap) const
{
    // 简化实现，实际应该使用线性回归
    if (tempResistanceMap.size() < 2) {
        return 0.0;
    }
    
    auto keys = tempResistanceMap.keys();
    double tempDiff = keys.last() - keys.first();
    double resDiff = tempResistanceMap[keys.last()] - tempResistanceMap[keys.first()];
    double avgRes = (tempResistanceMap[keys.last()] + tempResistanceMap[keys.first()]) / 2.0;
    
    if (tempDiff != 0 && avgRes != 0) {
        return (resDiff / avgRes) / tempDiff; // ppm/°C
    }
    
    return 0.0;
}

ResistorDiagnostic::ResistorTestParams ResistorDiagnostic::calculateOptimalTestParams(const ComponentSpec& component) const
{
    ResistorTestParams params;
    
    // 根据电阻值选择最优测试电流
    double resistance = component.nominal_value;
    
    if (resistance < 1.0) {
        params.testCurrent = 0.1;  // 100mA for very low resistance
        params.use4WireMethod = true;
    } else if (resistance < 100.0) {
        params.testCurrent = 0.01; // 10mA for low resistance
        params.use4WireMethod = true;
    } else if (resistance < 10000.0) {
        params.testCurrent = 0.001; // 1mA for medium resistance
    } else if (resistance < 1e6) {
        params.testCurrent = 1e-6; // 1µA for high resistance
    } else {
        params.testCurrent = 1e-9; // 1nA for very high resistance
    }
    
    // 计算最大预期电压
    params.maxVoltage = params.testCurrent * resistance * 1.5; // 1.5倍安全系数
    params.maxVoltage = qMin(params.maxVoltage, 10.0); // 限制在10V以内
    
    // 根据容差要求调整测量点数
    if (component.tolerance_percent < 1.0) {
        params.measurementPoints = 20; // 高精度测量
    } else if (component.tolerance_percent < 5.0) {
        params.measurementPoints = 10; // 中等精度
    } else {
        params.measurementPoints = 5;  // 标准测量
    }
    
    return params;
}

QString ResistorDiagnostic::generateDiagnosticSummary(const ComponentSpec& component, 
                                                     const ComponentDiagnosticResult& result) const
{
    QString summary;
    
    double measured = result.measurements.value("measured_resistance", 0.0);
    double expected = component.nominal_value;
    double deviation = result.measurements.value("deviation_percent", 0.0);
    
    summary += QString("电阻器 %1 诊断结果:\n").arg(component.reference);
    summary += QString("标称值: %1\n").arg(formatValue(expected, "Ω"));
    summary += QString("测量值: %1\n").arg(formatValue(measured, "Ω"));
    summary += QString("偏差: %1\n").arg(formatPercentage(deviation));
    summary += QString("容差: ±%1\n").arg(formatPercentage(component.tolerance_percent));
    
    if (result.isPassed) {
        summary += "结论: 元件工作正常，所有参数在规定范围内。";
    } else {
        summary += "结论: 元件存在故障，需要进一步检查或更换。";
    }
    
    return summary;
}
