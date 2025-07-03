#include "resistordiagnostic.h"
#include "../../devicemanager.h"
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
    return QStringList() << "通用电阻" << "精密电阻";
}

QVector<PortRequirement> ResistorDiagnostic::getPortRequirements(const ComponentSpec& component) const
{
    QVector<PortRequirement> requirements;

    // 电阻测量需要以下端口配置（基于JY5711 + JY5322 + JY5323）

    // 1. 模拟输出端口 - JY5711提供测试信号
    PortRequirement analogPort;
    analogPort.portType = PortType::ANALOG_OUTPUT;
    analogPort.count = 1;
    analogPort.description = "模拟输出端口 - JY5711提供测试信号";
    requirements.append(analogPort);

    // 2. 电压测量端口 - JY5322测量电阻两端电压
    PortRequirement voltagePort;
    voltagePort.portType = PortType::DIGITAL_INPUT;
    voltagePort.count = 1;
    voltagePort.description = "电压测量端口 - JY5322测量电阻两端电压";
    requirements.append(voltagePort);

    // 3. 电流测量端口 - JY5323测量流经电阻的电流
    PortRequirement currentPort;
    currentPort.portType = PortType::ANALOG_INPUT;
    currentPort.count = 1;
    currentPort.description = "电流测量端口 - JY5323测量流经电阻的电流";
    requirements.append(currentPort);

    logInfo(QString("电阻器 %1 端口需求：模拟输出(1个) + 电压测量(1个) + 电流测量(1个) = 总共3个端口")
           .arg(component.reference));

    return requirements;
}

QMap<QString, QVariant> ResistorDiagnostic::getRequiredParameters() const
{
    QMap<QString, QVariant> params;

    // 电阻测试的标准参数
    params["标称值"] = 100.0; // 默认标称值为100欧姆
    params["容差/%"] = 5.0;     // 默认容差为5%

    logInfo("电阻器测试所需标准参数已配置");

    return params;
}
QVector<WiringConnection> ResistorDiagnostic::generateWiringScheme(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts) const
{
    QVector<WiringConnection> connections;

    if (allocatedPorts.size() < 3) {
        logError("分配的端口数量不足，需要3个端口");
        return connections;
    }

    for(auto& port : allocatedPorts) {
        switch (port.portType)
        {
            case PortType::ANALOG_OUTPUT:
                {
                    WiringConnection conn1;
                    conn1.componentPin = "引脚1";
                    conn1.targetPort = port; // JY5711模拟输出
                    conn1.wireColor = "红色";
                    conn1.instruction = "将红色导线连接电阻引脚1到信号输出";
                    conn1.isRequired = true;
                    connections.append(conn1);
                }
                break;
            case PortType::DIGITAL_INPUT:
                {
                    WiringConnection conn2;
                    conn2.componentPin = "引脚2";
                    conn2.targetPort = port; // 电压测量
                    conn2.wireColor = "黑色";
                    conn2.instruction = "将黑色导线连接电阻引脚2到信号测量输入";
                    conn2.isRequired = true;
                    connections.append(conn2);
                }
                break;
            case PortType::DMM_MEASUREMENT:
                {
                    WiringConnection conn3;
                    conn3.componentPin = "引脚1";
                    conn3.targetPort = port; // 电阻测量
                    conn3.wireColor = "蓝色";
                    conn3.instruction = "将蓝色导线连接电阻引脚1到电阻测量";
                    conn3.isRequired = true;
                    connections.append(conn3);
                }
                break;
            case PortType::ANALOG_INPUT:
                {
                    WiringConnection conn4;
                    conn4.componentPin = "引脚1";
                    conn4.targetPort = port; // 电流测量
                    conn4.wireColor = "绿色";
                    conn4.instruction = "将测量导线串联到电阻电路中";
                    conn4.isRequired = true;
                    connections.append(conn4);
                }
                break;
            default:
                break;
        }
    }

    return connections;
}

ComponentTestConfig ResistorDiagnostic::configureDataAcquisition(const ComponentSpec& component, const QVector<PortInfo>& ports) const
{
    if(ports.isEmpty()) {
        logError("未分配端口");
        return ComponentTestConfig();
    }

    ComponentTestConfig config;
    config.testName = QString("电阻器测试_%1").arg(component.reference);

    // 计算最优测试参数
    ResistorTestParams testParams = calculateOptimalTestParams(component);

    DeviceOperation operation5711;
    operation5711.command = DeviceCommand::CONFIGURE_CHANNEL;
    operation5711.parameters["sampleRate"] = 100000.0;
    operation5711.parameters["samplesPerChannel"] = 100000.0;
    QVariantList waveformConfigs;
    for(auto& port : ports) {
        switch (port.portType)
        {
        case PortType::ANALOG_OUTPUT:
            {
                QVariantMap channelConfig;
                channelConfig["channel"] = port.portNumber;
                channelConfig["type"] = static_cast<int>(PXIe5711_testtype::HighLevelWave);
                channelConfig["amplitude"] = 5;
                channelConfig["frequency"] = 1000;
                channelConfig["lowRange"] = -10.0;
                channelConfig["highRange"] = 10.0;
                waveformConfigs.append(channelConfig);
            }
            break;
        case PortType::DIGITAL_INPUT:
            {
                DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
                configOp.parameters["mode"] = "multi";
                configOp.parameters["channels"] = QVariant::fromValue(QVector<int>({port.portNumber}));
                configOp.parameters["sampleRate"] = 1000000.0;               // 采样率1MHz
                configOp.parameters["samplesPerChannel"] = 1000000;          // 缓冲区大小
                configOp.parameters["rangeMin"] = -10.0;
                configOp.parameters["rangeMax"] = 10.0;
                configOp.timeout = 10000;

                config.parameters["JY5322"] = configOp;
            }
            break;
        case PortType::ANALOG_INPUT:
            {
                DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
            configOp.parameters["mode"] = "multi";
            configOp.parameters["channels"] = QVariant::fromValue(QVector<int>({port.portNumber}));
            configOp.parameters["sampleRate"] = 200000.0;               // 采样率200kHz
            configOp.parameters["samplesPerChannel"] = 200000;          // 缓冲区大小
            configOp.parameters["rangeMin"] = -10.0;
            configOp.parameters["rangeMax"] = 10.0;
            configOp.timeout = 10000;


                config.parameters["JY5323"] = configOp;
            }
            break;
        default:
            break;
        }
    }

    operation5711.parameters["waveforms"] = waveformConfigs;
    operation5711.parameters["channelCount"] = waveformConfigs.size();
    if(waveformConfigs.size() > 0) {
        config.parameters["JY5711"] = operation5711;
    }
    
    config.TemperatureThreshold = 60.0;
    return config;
}

TestData ResistorDiagnostic::executeDataAcquisition(const ComponentTestConfig& config)
{
    TestData testData;
    testData.testId = config.testName;
    testData.timestamp = QDateTime::currentDateTime();
    testData.valid = false;
    QMap<QString, QVariant> measurement;

    if (!getDeviceManager() || !getDeviceManager()->isSystemReady()) {
        testData.errorMessage = "设备管理器未就绪";
        return testData;
    }

    getDeviceManager()->setTemperatureThreshold(config.TemperatureThreshold);
    getDeviceManager()->getCameraManager()->startCamera(CameraType::IR_CAMERA);

    try {
        logInfo("开始执行电阻器数据采集");


        // 配置设备
        for (auto it = config.parameters.begin(); it != config.parameters.end(); ++it) {
            if(!getDeviceManager()->submitOperation(it.key(), it.value())) {
                throw std::runtime_error(QString("设备 %1 配置失败: %2").arg(it.key(), getDeviceManager()->getLastError()).toStdString());
            }
            DeviceResult configResult = getDeviceManager()->waitForResult(it.key(), 10000);
            if (!configResult.success) {
                throw std::runtime_error(QString("设备 %1 配置失败: %2").arg(it.key(), configResult.error).toStdString());
            }
        }
        
        // 写入数据
        DeviceOperation outputOp;
        outputOp.command = DeviceCommand::WRITE_DATA;
        outputOp.parameters["waveforms"] = config.parameters["JY5711"].parameters["waveforms"];
        outputOp.parameters["sampleRate"] = config.parameters["JY5711"].parameters["sampleRate"];
        outputOp.parameters["samplesPerChannel"] = config.parameters["JY5711"].parameters["samplesPerChannel"];

        if (!getDeviceManager()->submitOperation("JY5711", outputOp)) {
            throw std::runtime_error(QString("JY5711写入数据失败: %1").arg(getDeviceManager()->getLastError()).toStdString());
        }
        DeviceResult outputResult = getDeviceManager()->waitForResult("JY5711", 15000);
        if(!outputResult.success) {
            throw std::runtime_error(QString("JY5711写入数据失败: %1").arg(outputResult.error).toStdString());
        }
        // 创建同步组
        const QString syncGroupName = "resistor_test_group";
        const QStringList deviceNames = {"JY5711", "JY5323", "JY5322"};
        getDeviceManager()->createSyncGroup(syncGroupName, deviceNames);

        QList<DeviceOperation> syncOperations;
        DeviceOperation triggerOp5711;
        triggerOp5711.deviceName = "JY5711";
        triggerOp5711.command = DeviceCommand::SYNC_TRIGGER;
        triggerOp5711.syncGroup = syncGroupName;
        syncOperations.append(triggerOp5711);

        DeviceOperation daqStartOp;
        daqStartOp.command = DeviceCommand::START_MEASUREMENT;
        daqStartOp.deviceName = "JY5323";
        daqStartOp.syncGroup = syncGroupName;
        daqStartOp.syncDelay = 0;
        daqStartOp.timeout = 5000;
        syncOperations.append(daqStartOp);

        daqStartOp.command = DeviceCommand::START_MEASUREMENT;
        daqStartOp.deviceName = "JY5322";
        daqStartOp.syncGroup = syncGroupName;
        daqStartOp.syncDelay = 0;
        daqStartOp.timeout = 5000;
        syncOperations.append(daqStartOp);

        if (!getDeviceManager()->executeSync(syncGroupName, syncOperations, 5000))
        {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("设备同步执行失败");
        }
        DeviceResult aoResult = getDeviceManager()->waitForResult("JY5711", 5000);
        if (!aoResult.success) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("设备 JY5711 输出失败: " + aoResult.error.toStdString());
        }
        DeviceResult daqStartResult = getDeviceManager()->waitForResult("JY5323", 5000);
        if (!daqStartResult.success) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("设备 JY5323 启动失败: " + daqStartResult.error.toStdString());
        }

        daqStartResult = getDeviceManager()->waitForResult("JY5322", 5000);
        if (!daqStartResult.success) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("设备 JY5322 启动失败: " + daqStartResult.error.toStdString());
        }
        
        // 读取电流测量数据
        QVector<QVector<double>> currentsChannelData;
        DeviceManager::WaitResult waitResult = getDeviceManager()->waitForDataWithEventLoop("JY5323", currentsChannelData, 30, 200, 8000);
        if (waitResult.success && !currentsChannelData.isEmpty() && !currentsChannelData[0].isEmpty())
        {
            logInfo(QString("成功采集到 %1 个电流测量点").arg(currentsChannelData[0].size()));
            measurement["currents"] = QVariant::fromValue(currentsChannelData[0]);
        } else {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("电流测量数据为空或读取失败");
        }

        // 读取电压测量数据
        QVector<QVector<double>> voltageChannelData;
        waitResult = getDeviceManager()->waitForDataWithEventLoop("JY5322", voltageChannelData, 30, 200, 8000);
        if( waitResult.success && !voltageChannelData.isEmpty() && !voltageChannelData[0].isEmpty())
        {
            logInfo(QString("成功采集到 %1 个电压测量点").arg(voltageChannelData[0].size()));
            measurement["voltages"] = QVariant::fromValue(voltageChannelData[0]);
        } else {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("电压测量数据为空或读取失败");
        }
        
        // 将测量数据添加到列表中
        testData.measurements.append(measurement);

        // 设置元数据
        testData.metadata = config.parameters;

        // 停止所有设备
        DeviceOperation stopOp(DeviceCommand::STOP_MEASUREMENT);
        stopOp.timeout = 5000;
        
        if (!getDeviceManager()->submitOperation("JY5711", stopOp)) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error(QString("设备 %1 停止失败: %2").arg("JY5711", getDeviceManager()->getLastError()).toStdString());
        }

        DeviceResult stopResult = getDeviceManager()->waitForResult("JY5711", 5000);
        if (!stopResult.success) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            qDebug() << (QString("设备 %1 停止失败: %2").arg("JY5711", stopResult.error).toStdString());
        }
        if (!getDeviceManager()->submitOperation("JY5323", stopOp)) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            qDebug() << (QString("设备 %1 停止失败: %2").arg("JY5323", getDeviceManager()->getLastError()).toStdString());
        }
        DeviceResult stopResult2 = getDeviceManager()->waitForResult("JY5323", 5000);
        if (!stopResult2.success) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            qDebug() << (QString("设备 %1 停止失败: %2").arg("JY5323", stopResult2.error).toStdString());
        }
        if (!getDeviceManager()->submitOperation("JY5322", stopOp)) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            qDebug() << (QString("设备 %1 停止失败: %2").arg("JY5322", getDeviceManager()->getLastError()).toStdString());
        }
        stopResult2 = getDeviceManager()->waitForResult("JY5322", 5000);
        if (!stopResult2.success) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            qDebug() << (QString("设备 %1 停止失败: %2").arg("JY5322", stopResult2.error).toStdString());
        }
        getDeviceManager()->removeSyncGroup(syncGroupName);
        testData.thermalidata = getDeviceManager()->getLatestThermalData();
        getDeviceManager()->getCameraManager()->stopCamera(CameraType::IR_CAMERA);
        testData.valid = true;

    } catch (const std::exception& e) {
        getDeviceManager()->initializeDeviceThreads();
        getDeviceManager()->getCameraManager()->stopCamera(CameraType::IR_CAMERA);
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
    result.metameasurements = testData.measurements.first();
    result.thermalData = testData.thermalidata;

    if (!testData.valid) {
        result.isPassed = false;
        result.healthScore = 0.0;
        result.confidence = 0.0;
        result.summary = "测试数据无效";
        result.faultTypes.append("DATA_INVALID");
        return result;
    }

    try {
        // 提取测量数据
        QMap<QString, QVariant> measurementData;
        if (!testData.measurements.isEmpty()) {
            measurementData = testData.measurements.first();
        }

        QVector<double> voltages = measurementData.value("voltages").value<QVector<double>>();
        QVector<double> currents = measurementData.value("currents").value<QVector<double>>();

        if (voltages.isEmpty() || currents.isEmpty()) {
            throw std::runtime_error("电压或电流测量数据为空");
        }

        // 基于电压电流计算电阻值
        double calculatedResistance = calculateResistance(voltages, currents);
        double nominalValue = component.params.value("标称值", 100.0).toDouble();
        double tolerancePercent = component.params.value("容差/%", 5.0).toDouble();
        
        // 存储测量结果
        result.measurements["计算电阻"] = calculatedResistance;
        result.measurements["标称值"] = nominalValue;
        result.measurements["偏差"] = calculateDeviation(calculatedResistance, nominalValue);
        result.measurements["容差/%"] = tolerancePercent;
        result.measurements["平均电压"] = calculateMean(voltages);
        result.measurements["平均电流"] = calculateMean(currents);
        result.measurements["最高温度"] = testData.thermalidata.maxTemp;

        // 故障分析
        bool hasFault = false;

        // 1. 检查开路
        if (isOpenCircuit(calculatedResistance, nominalValue)) {
            result.faultTypes.append("开路");
            result.recommendations.append("检查元件引脚连接");
            result.recommendations.append("检查元件是否烧断");
            hasFault = true;
        }

        // 2. 检查短路
        if (isShortCircuit(calculateMean(voltages), calculateMean(currents))) {
            result.faultTypes.append("短路");
            result.recommendations.append("检查是否有导线短路");
            result.recommendations.append("检查元件是否被导体物质污染");
            hasFault = true;
        }

        // 3. 检查阻值偏差
        if (!hasFault && isOutOfTolerance(calculatedResistance, nominalValue, tolerancePercent)) {
            result.faultTypes.append("超出容差");
            result.recommendations.append("元件阻值超出规定容差范围");
            result.recommendations.append("建议更换元件");
            hasFault = true;
        }

        // 4. 计算稳定性
        double stability = calculateStability(voltages);
        result.measurements["稳定性"] = stability;

        if (stability > 2.0) { // 超过2%的波动认为不稳定
            result.faultTypes.append("不稳定");
            result.recommendations.append("元件值不稳定，可能老化或损坏");
            hasFault = true;
        }
        // 5. 检查温度
        double maxTemp = result.measurements["最高温度"];
        if (maxTemp > 60.0) {
            result.faultTypes.append("温度过高");
            result.recommendations.append("元件温度过高，可能老化或损坏");
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
        result.faultTypes.append("分析异常");
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
    if (voltages.isEmpty() || currents.isEmpty() || voltages.size() != currents.size()) {
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
    // 如果测量电阻小于期望值的10%，认为是开路
    return resistance <= 1e-6;
}

bool ResistorDiagnostic::isShortCircuit(double voltage, double current) const
{
    // 如果电阻小于1mΩ，认为是短路
    return voltage <= 1e-3 && current >= 1e-3;
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

    // 根据电阻值选择最优测试配置
    double resistance = component.params["标称值"].toDouble();

    if (resistance < 1.0) {
        params.testCurrent = 10;  // 10A for very low resistance
        params.range = "100"; // 100Ω范围
    } else if (resistance < 100.0) {
        params.testCurrent = 0.1; // 10mA for low resistance
        params.range = "1k"; // 1kΩ范围
    } else if (resistance < 10000.0) {
        params.testCurrent = 0.001; // 1mA for medium resistance
        params.range = "10k"; // 10kΩ范围
    } else if (resistance < 1e6) {
        params.testCurrent = 1e-5; // 10µA for high resistance
        params.range = "1M"; // 1MΩ范围
    } else if (resistance < 1e7) {
        params.testCurrent = 1e-6; // 1µA for very high resistance
        params.range = "10M"; // 10MΩ范围
    } else if (resistance < 1e8) {
        params.testCurrent = 1e-7; // 100nA for ultra high resistance
        params.range = "100M"; // 100MΩ范围
    } else {
        params.testCurrent = 1e-7; // 100nA for ultra high resistance
        params.range = "auto";
    }

    // 计算最大预期电压
    params.maxVoltage = params.testCurrent * resistance * 1.5; // 1.5倍安全系数
    params.maxVoltage = qMin(params.maxVoltage, 10.0); // 限制在10V以内

    return params;
}

QString ResistorDiagnostic::generateDiagnosticSummary(const ComponentSpec& component,
                                                     const ComponentDiagnosticResult& result) const
{
    QString summary;

    double calculated = result.measurements.value("计算电阻", 0.0);
    double expected = result.measurements.value("标称值", 0.0);
    double deviation = result.measurements.value("偏差", 0.0);

    summary += QString("电阻器 %1 诊断结果:\n").arg(component.reference);
    summary += QString("标称值: %1\n").arg(formatValue(expected, "Ω"));
    summary += QString("计算值: %1\n").arg(formatValue(calculated, "Ω"));
    summary += QString("偏差: %1\n").arg(formatPercentage(deviation));
    summary += QString("容差: ±%1\n").arg(formatPercentage(result.measurements.value("容差/%", 0.0)));

    if (result.isPassed) {
        summary += "结论: 元件工作正常，所有参数在规定范围内。";
    } else {
        summary += "结论: 元件存在故障，需要进一步检查或更换。";
    }

    return summary;
}

double ResistorDiagnostic::calculateMean(const QVector<double> &values)
{
    if(values.empty()) return 0;

    double sum = 0;
    for(auto &val : values)
    {
        sum += val;
    }
    return sum / values.size();
}

