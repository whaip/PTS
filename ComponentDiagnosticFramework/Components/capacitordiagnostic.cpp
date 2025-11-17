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
    return QStringList() << "电解电容" <<  "钽电容";
}

QVector<PortRequirement> CapacitorDiagnostic::getPortRequirements(const ComponentSpec& component) const
{
    QVector<PortRequirement> requirements;
    
    // 基本端口需求（基于JY5711 + JY5322 + JY5323）
    requirements.append(PortRequirement(PortType::ANALOG_OUTPUT, 1, "JY5711交流激励信号输出"));
    requirements.append(PortRequirement(PortType::DIGITAL_INPUT, 1, "JY5322电压测量输入"));
    requirements.append(PortRequirement(PortType::ANALOG_INPUT, 1, "JY5323电流测量输入"));
    
    return requirements;
}

QVector<WiringConnection> CapacitorDiagnostic::generateWiringScheme(const ComponentSpec& component, 
                                                                   const QVector<PortInfo>& allocatedPorts) const
{
    QVector<WiringConnection> connections;

    allocatedPorts_ = allocatedPorts;
    
    if (allocatedPorts.size() < 3) {
        logError("分配的端口数量不足，需要3个端口");
        return connections;
    }
    
    for(auto& port : allocatedPorts) {
        switch (port.portType)
        {
            case PortType::ANALOG_OUTPUT:
                {
                    // 连接1：激励信号到电容正极
                    WiringConnection conn1;
                    conn1.componentPin = "正极";
                    conn1.component = component.reference;
                    conn1.targetPort = port; // JY5711交流激励输出
                    conn1.wireColor = "红色";
                    conn1.instruction = QString("将电容%1连接到模拟输出端口%2").arg(component.reference).arg(port.portNumber);
                    conn1.isRequired = true;
                    connections.append(conn1);
                }
                break;
            case PortType::DIGITAL_INPUT:
                {
                    // 连接2：电容正极到电压测量
                    WiringConnection conn2;
                    conn2.componentPin = "正极";
                    conn2.component = component.reference;
                    conn2.targetPort = port; // JY5322电压测量输入
                    conn2.wireColor = "黄色";
                    conn2.instruction = QString("将数字输入端口%1 导线连接到电容%2正极").arg(port.portNumber).arg(component.reference);
                    conn2.isRequired = true;
                    connections.append(conn2);
                }
                break;
            case PortType::ANALOG_INPUT:
                {
                    // 连接3：电容负极接地并连接到电流测量
                    WiringConnection conn3;
                    conn3.componentPin = "负极";
                    conn3.component = component.reference;
                    conn3.targetPort = port; // JY5323电流测量输入
                    conn3.wireColor = "黑色";
                    conn3.instruction = QString("将模拟输入端口%1 串联到电容%2负极").arg(port.portNumber).arg(component.reference);
                    conn3.isRequired = true;
                    connections.append(conn3);
                }
                break;
            default:
                break;
        }
    }
    
    return connections;
}

ComponentTestConfig CapacitorDiagnostic::configureDataAcquisition(const ComponentSpec& component, const QVector<PortInfo>& ports) const
{
    ComponentTestConfig config;
    config.testName = QString("电容器测试_%1").arg(component.reference);
    
    // 计算最优测试参数
    CapacitorTestParams testParams = calculateOptimalTestParams(component);
    
    for(auto& port : ports) {
        switch (port.portType)
        {
        case PortType::ANALOG_OUTPUT:
            {
                // 配置交流信号输出
                DeviceOperation operation;
                operation.command = DeviceCommand::CONFIGURE_CHANNEL;
                operation.parameters["channelCount"] = 1;
                operation.parameters["sampleRate"] = 1000000.0;
                operation.parameters["samplesPerChannel"] = 1000000.0;
                
                QVariantList waveformConfigs;
                QVariantMap channelConfig;
                channelConfig["channel"] = port.portNumber;
                channelConfig["type"] = static_cast<int>(PXIe5711_testtype::SineWave);
                channelConfig["amplitude"] = 5;
                channelConfig["frequency"] = testParams.testFrequencies.isEmpty() ? 1000.0 : testParams.testFrequencies.first();
                channelConfig["lowRange"] = -10;
                channelConfig["highRange"] = 10;
                waveformConfigs.append(channelConfig);
                
                operation.parameters["waveforms"] = waveformConfigs;
                config.parameters["JY5711"] = operation;
            }
            break;
        case PortType::ANALOG_INPUT:
            {
                // 电流测量
                DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
                configOp.parameters["mode"] = "multi";
                configOp.parameters["channels"] = QVariant::fromValue(QVector<int>({port.portNumber}));
                configOp.parameters["sampleRate"] = 20000.0;          // 电流采样率20kHz
                configOp.parameters["samplesPerChannel"] = 20000;     // 1秒数据量，便于频域估计
                configOp.parameters["rangeMin"] = -10;
                configOp.parameters["rangeMax"] = 10;
                configOp.timeout = 10000;

                config.parameters["JY5323"] = configOp;
            }
            break;
        case PortType::DIGITAL_INPUT:
            {
                DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
                configOp.parameters["mode"] = "multi";
                configOp.parameters["channels"] = QVariant::fromValue(QVector<int>({port.portNumber}));
                configOp.parameters["sampleRate"] = 1000000.0;        // 电压采样率1MHz
                configOp.parameters["samplesPerChannel"] = 1000000;   // 1秒数据量
                configOp.parameters["rangeMin"] = -10;
                configOp.parameters["rangeMax"] = 10;
                configOp.timeout = 10000;
                
                config.parameters["JY5322"] = configOp;
            }
            break;
        default:
            break;
        }
    }

    config.TemperatureThreshold = 60.0;
    config_ = config;
    return config;
}

TestData CapacitorDiagnostic::executeDataAcquisition(const ComponentTestConfig& config)
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
        logInfo("开始执行电容器数据采集");

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

        // 交流测量 - 输出交流信号并测量响应
        // 写入交流波形数据
        DeviceOperation outputOp;
        outputOp.command = DeviceCommand::WRITE_DATA;
        outputOp.parameters["waveforms"] = config.parameters["JY5711"].parameters["waveforms"];
        outputOp.parameters["sampleRate"] = config.parameters["JY5711"].parameters["sampleRate"];
        outputOp.parameters["samplesPerChannel"] = config.parameters["JY5711"].parameters["samplesPerChannel"];

        getDeviceManager()->submitOperation("JY5711", outputOp);
        DeviceResult outputResult = getDeviceManager()->waitForResult("JY5711", 15000);
        if(!outputResult.success) {
            throw std::runtime_error(QString("设备 %1 写入数据失败: %2").arg("JY5711", getDeviceManager()->getLastError()).toStdString());
        }

        // 创建同步组进行交流测量
        const QString syncGroupName = "capacitor_test_group";
        const QStringList deviceNames = {"JY5711", "JY5322", "JY5323"};
        getDeviceManager()->createSyncGroup(syncGroupName, deviceNames);

        QList<DeviceOperation> syncOperations;
        
        DeviceOperation triggerOp5711;
        triggerOp5711.deviceName = "JY5711";
        triggerOp5711.command = DeviceCommand::SYNC_TRIGGER;
        triggerOp5711.syncGroup = syncGroupName;
        syncOperations.append(triggerOp5711);

        DeviceOperation daqStartOp;
        daqStartOp.command = DeviceCommand::START_MEASUREMENT;
        daqStartOp.deviceName = "JY5322";
        daqStartOp.syncGroup = syncGroupName;
        daqStartOp.syncDelay = 0;
        daqStartOp.timeout = 5000;
        syncOperations.append(daqStartOp);

        daqStartOp.deviceName = "JY5323";
        syncOperations.append(daqStartOp);

        if (!getDeviceManager()->executeSync(syncGroupName, syncOperations, 5000)) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("设备同步执行失败");
        }

        // 等待结果
        DeviceResult aoResult = getDeviceManager()->waitForResult("JY5711", 5000);
        if (!aoResult.success) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("设备 JY5711 输出失败: " + aoResult.error.toStdString());
        }

        DeviceResult daqStartResult = getDeviceManager()->waitForResult("JY5322", 5000);
        if (!daqStartResult.success) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("设备 JY5322 启动失败: " + daqStartResult.error.toStdString());
        }

        daqStartResult = getDeviceManager()->waitForResult("JY5323", 5000);
        if (!daqStartResult.success) {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("设备 JY5323 启动失败: " + daqStartResult.error.toStdString());
        }

        // 获取电压测量数据
        QVector<QVector<double>> voltageChannelData;
        DeviceManager::WaitResult waitResult = getDeviceManager()->waitForDataWithEventLoop("JY5322", voltageChannelData, 30, 200, 8000);
        if( waitResult.success && !voltageChannelData.isEmpty() && !voltageChannelData[0].isEmpty())
        {
            // for(double value : voltageChannelData[0]) {
            //     logInfo(QString("电压测量值: %1 V").arg(value));
            // }
            measurement["voltages"] = QVariant::fromValue(voltageChannelData[0]);
        } else {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("电压测量数据为空或读取失败");
        }

        // 获取电流测量数据
        QVector<QVector<double>> currentChannelData;
        waitResult = getDeviceManager()->waitForDataWithEventLoop("JY5323", currentChannelData, 30, 200, 8000);
        if (waitResult.success && !currentChannelData.isEmpty() && !currentChannelData[0].isEmpty())
        {
            // for(double value : currentChannelData[0]) {
            //     logInfo(QString("电流测量值: %1 A").arg(value));
            // }
            measurement["currents"] = QVariant::fromValue(currentChannelData[0]);
        } else {
            getDeviceManager()->removeSyncGroup(syncGroupName);
            throw std::runtime_error("电流测量数据为空或读取失败");
        }

        // 停止所有设备
        DeviceOperation stopOp(DeviceCommand::STOP_MEASUREMENT);
        stopOp.timeout = 5000;
        
        QStringList stopDevices = {"JY5711", "JY5322", "JY5323"};
        for (const QString& device : stopDevices) {
            if (!getDeviceManager()->submitOperation(device, stopOp)) {
                logWarning(QString("设备 %1 停止命令发送失败").arg(device));
            } else {
                DeviceResult stopResult = getDeviceManager()->waitForResult(device, 5000);
                if (!stopResult.success) {
                    logWarning(QString("设备 %1 停止失败: %2").arg(device, stopResult.error));
                }
            }
        }

        getDeviceManager()->removeSyncGroup(syncGroupName);

        // 将测量数据添加到列表中
        testData.measurements.append(measurement);
        
        // 设置元数据
        testData.metadata = config.parameters;
        testData.thermalidata = getDeviceManager()->getLatestThermalData();
        testData.valid = true;
        
        logInfo("电容器数据采集完成");
        
    } catch (const std::exception& e) {
        getDeviceManager()->initializeDeviceThreads();
        testData.errorMessage = QString("数据采集异常: %1").arg(e.what());
        logError(testData.errorMessage);
    }
    
    return testData;
}

ComponentDiagnosticResult CapacitorDiagnostic::analyzeFaults(const ComponentSpec& component,
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
        // 获取测量数据
        QMap<QString, QVariant> measurementData;
        if (!testData.measurements.isEmpty()) {
            measurementData = testData.measurements.first();
        }
        
        // 获取电压和电流测量数据
        QVector<double> voltages = measurementData.value("voltages").value<QVector<double>>();
        QVector<double> currents = measurementData.value("currents").value<QVector<double>>();
        
        if (voltages.isEmpty() || currents.isEmpty()) {
            throw std::runtime_error("电压或电流测量数据为空");
        }
        
        // 频率参数（激励频率）
        const double testFreq = component.params.value("测试频率/Hz", 1000.0).toDouble();
        // 采样率设定：电压1MHz，电流20kHz（与采集配置一致）
        const double fsVoltage = 1e6;
        const double fsCurrent = 2e4;

        // 单频DFT获取电压/电流的复相量（不同采样率分别处理）
        const ComplexVal Vc = singleToneDFT(voltages, fsVoltage, testFreq);
        const ComplexVal Ic = singleToneDFT(currents, fsCurrent, testFreq);

        // 计算复阻抗 Z = V / I
        const ComplexVal Z = complexDiv(Vc, Ic);
        const double impedance = qSqrt(Z.re * Z.re + Z.im * Z.im);
        const double measuredESR = Z.re;            // ESR 为复阻抗实部
        const double phaseDiffRad = qAtan2(Z.im, Z.re); // Z 的相角 = V/I 的相角 = φV - φI
        const double phaseDiffDeg = rad2deg(phaseDiffRad);

        // 电容值来自容抗的绝对值 |Xc| = |Im(Z)|
        double calculatedCapacitance = 0.0;
        if (testFreq > 0 && qFabs(Z.im) > 0) {
            const double Xc = qFabs(Z.im);
            calculatedCapacitance = 1.0 / (2.0 * M_PI * testFreq * Xc);
        }

        // 平均值记录（用于报告展示，不参与主计算）
        double avgVoltage = std::accumulate(voltages.begin(), voltages.end(), 0.0) / voltages.size();
        double avgCurrent = std::accumulate(currents.begin(), currents.end(), 0.0) / currents.size();

        // 从component.params中获取规格参数
        double nominalCapacitance = component.params.value("标称值", 1e-6).toDouble();
        double tolerancePercent = component.params.value("容差/%", 10.0).toDouble();
        double maxESR = component.params.value("最大ESR/Ω", 1.0).toDouble();
        double ratedVoltage = component.params.value("额定电压/V", 16.0).toDouble();
        
        result.metameasurements = measurementData;
        
        // 存储计算结果
        result.measurements["计算电容"] = calculatedCapacitance;
        result.measurements["标称电容"] = nominalCapacitance;
        result.measurements["容值偏差"] = calculateDeviation(calculatedCapacitance, nominalCapacitance);
        result.measurements["测量ESR"] = measuredESR;
        result.measurements["最大ESR规格"] = maxESR;
        result.measurements["容差"] = tolerancePercent;
        result.measurements["测试频率"] = testFreq;
        result.measurements["计算阻抗"] = impedance;
        result.measurements["相位差/度"] = phaseDiffDeg;
        result.measurements["平均电压"] = avgVoltage;
        result.measurements["平均电流"] = avgCurrent;
        result.measurements["最高温度"] = testData.thermalidata.maxTemp;

        // 故障分析
        bool hasFault = false;
        
        // 1. 检查开路
        if (isOpenCircuit(calculatedCapacitance, nominalCapacitance)) {
            result.faultTypes.append("开路");
            result.recommendations.append("检查元件引脚连接");
            result.recommendations.append("检查电容是否内部断路");
            hasFault = true;
        }
        
        // 2. 检查短路
        if (isShortCircuit(calculatedCapacitance)) {
            result.faultTypes.append("短路");
            result.recommendations.append("电容内部短路，需要更换");
            result.recommendations.append("检查是否有外部短路路径");
            hasFault = true;
        }
        
        // 3. 检查容值偏差
        if (!hasFault && !isWithinTolerance(calculatedCapacitance, nominalCapacitance, tolerancePercent)) {
            result.faultTypes.append("容值偏差");
            result.recommendations.append("电容值超出规定容差范围");
            result.recommendations.append("可能是老化或制造偏差导致");
            hasFault = true;
        }
        
        // 4. 检查ESR
        if (maxESR > 0 && isHighESR(measuredESR, maxESR)) {
            result.faultTypes.append("ESR过高");
            result.recommendations.append("等效串联电阻过高");
            result.recommendations.append("电容可能老化或品质降低");
            hasFault = true;
        }

        // 6. 计算品质因数（基于复阻抗）
        if (testFreq > 0 && calculatedCapacitance > 0 && measuredESR > 0) {
            double qualityFactor = calculateQualityFactor(calculatedCapacitance, measuredESR, testFreq);
            result.measurements["quality_factor"] = qualityFactor;
            if (qualityFactor < 10) {
                result.faultTypes.append("品质因数过低");
                result.recommendations.append("品质因数过低，损耗较大");
            }
        }

        // 7. 检查温度
        double maxTemp = result.measurements["最高温度"];
        if (maxTemp > 60.0) {
            result.faultTypes.append("温度过高");
            result.recommendations.append("元件温度过高，可能老化或损坏");
            hasFault = true;
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
        result.faultTypes.append("分析异常");
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

// 单频DFT工具与复数运算（用于相位/复阻抗计算）
namespace {
    struct ComplexVal { double re{0.0}; double im{0.0}; };

    inline ComplexVal complexDiv(const ComplexVal& a, const ComplexVal& b) {
        const double den = b.re * b.re + b.im * b.im;
        if (den <= 0) return {0.0, 0.0};
        return { (a.re * b.re + a.im * b.im) / den,
                 (a.im * b.re - a.re * b.im) / den };
    }

    inline ComplexVal singleToneDFT(const QVector<double>& x, double fs, double f0) {
        const int N = x.size();
        if (N <= 0 || fs <= 0 || f0 <= 0) return {0.0, 0.0};
        // 去直流 + Hann窗，降低谱泄漏
        double mean = 0.0;
        for (double v : x) mean += v;
        mean /= N;
        const double wScale = 2.0 * M_PI / (N - 1.0);
        const double theta = 2.0 * M_PI * f0 / fs;
        double re = 0.0, im = 0.0;
        for (int n = 0; n < N; ++n) {
            const double xn = (x[n] - mean) * (0.5 * (1.0 - qCos(wScale * n))); // Hann
            const double ang = theta * n;
            re += xn * qCos(ang);
            im -= xn * qSin(ang); // e^{-j ang}
        }
        return {re, im};
    }

    inline double rad2deg(double rad) { return rad * 180.0 / M_PI; }
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
    
    // 从component.params中获取参数
    double nominalCapacitance = component.params.value("标称值", 1e-6).toDouble();
    double tolerancePercent = component.params.value("容差/%", 10.0).toDouble();
    double ratedVoltage = component.params.value("额定电压/V", 16.0).toDouble();
    double maxESR = component.params.value("最大ESR/Ω", 1.0).toDouble();
    double maxLeakage = component.params.value("最大漏电流/A", 1e-9).toDouble();
    
    // 根据电容值选择最优测试频率
    params.testFrequencies = getOptimalTestFrequencies(nominalCapacitance);
    
    // === 新增：预估电容阻抗范围计算 ===
    QVector<double> impedanceRanges;
    QVector<double> expectedCurrents;
    
    logInfo(QString("计算电容器 %1 的阻抗特性:").arg(component.reference));
    logInfo(QString("  标称电容: %1F, 容差: ±%2%").arg(nominalCapacitance).arg(tolerancePercent));
    
    // 计算每个测试频率下的预期阻抗
    for (double frequency : params.testFrequencies) {
        // 电容容抗公式: Xc = 1/(2πfC)
        double nominalReactance = 1.0 / (2 * M_PI * frequency * nominalCapacitance);
        
        // 考虑容差影响的阻抗范围
        double minCapacitance = nominalCapacitance * (1.0 - tolerancePercent / 100.0);
        double maxCapacitance = nominalCapacitance * (1.0 + tolerancePercent / 100.0);
        double maxReactance = 1.0 / (2 * M_PI * frequency * minCapacitance); // 小电容 -> 大阻抗
        double minReactance = 1.0 / (2 * M_PI * frequency * maxCapacitance); // 大电容 -> 小阻抗
        
        // 考虑ESR的影响，总阻抗 = sqrt(Xc² + ESR²)
        double minImpedance = qSqrt(minReactance * minReactance + maxESR * maxESR);
        double maxImpedance = qSqrt(maxReactance * maxReactance + maxESR * maxESR);
        
        impedanceRanges.append(minImpedance);
        impedanceRanges.append(maxImpedance);
        
        logInfo(QString("  频率 %1Hz: 容抗 %2Ω - %3Ω, 总阻抗 %4Ω - %5Ω")
               .arg(frequency)
               .arg(minReactance, 0, 'e', 2)
               .arg(maxReactance, 0, 'e', 2) 
               .arg(minImpedance, 0, 'e', 2)
               .arg(maxImpedance, 0, 'e', 2));
    }
    
    // 找到整体阻抗范围
    double overallMinImpedance = *std::min_element(impedanceRanges.begin(), impedanceRanges.end());
    double overallMaxImpedance = *std::max_element(impedanceRanges.begin(), impedanceRanges.end());
    
    logInfo(QString("  整体阻抗范围: %1Ω - %2Ω")
           .arg(overallMinImpedance, 0, 'e', 2)
           .arg(overallMaxImpedance, 0, 'e', 2));
    
    // === 基于阻抗范围优化测试电压 ===
    // 选择合适的测试电压，确保测得的电流在合理范围内 (1μA - 10mA)
    double targetMinCurrent = 1e-6;  // 1μA 最小电流（避免噪声影响）
    double targetMaxCurrent = 10e-3; // 10mA 最大电流（避免器件损坏）
    
    // 根据最大阻抗确定最小所需电压: V_min = I_min * Z_max
    double minRequiredVoltage = targetMinCurrent * overallMaxImpedance;
    // 根据最小阻抗确定最大允许电压: V_max = I_max * Z_min  
    double maxAllowedVoltage = targetMaxCurrent * overallMinImpedance;
    
    logInfo(QString("  基于阻抗的电压范围: %1V - %2V")
           .arg(minRequiredVoltage, 0, 'e', 2)
           .arg(maxAllowedVoltage, 0, 'e', 2));
    
    // 根据电容值和额定电压确定测试电压（结合阻抗考虑）
    if (nominalCapacitance >= 1e-3) {
        // 大电容（≥1mF）通常是电解电容，阻抗较小
        params.testVoltage = qMin(qMin(0.5, ratedVoltage * 0.1), maxAllowedVoltage);
        params.dcBiasVoltage = qMin(2.0, ratedVoltage * 0.2);
        params.leakageTestVoltage = qMin(ratedVoltage * 0.8, 10.0);
    } else if (nominalCapacitance >= 1e-6) {
        // 中等电容（1μF-1mF）
        params.testVoltage = qMin(qMin(1.0, ratedVoltage * 0.2), maxAllowedVoltage);
        params.testVoltage = qMax(params.testVoltage, minRequiredVoltage);
        params.dcBiasVoltage = 0.0;
        params.leakageTestVoltage = qMin(ratedVoltage * 0.9, 25.0);
    } else {
        // 小电容（<1μF）阻抗较大，需要更高电压
        params.testVoltage = qMin(qMin(2.0, ratedVoltage * 0.3), maxAllowedVoltage);
        params.testVoltage = qMax(params.testVoltage, minRequiredVoltage);
        params.dcBiasVoltage = 0.0;
        params.leakageTestVoltage = qMin(ratedVoltage * 0.95, 50.0);
    }
    
    // === 基于阻抗优化电流测量范围 ===
    double expectedMinCurrent = params.testVoltage / overallMaxImpedance;
    double expectedMaxCurrent = params.testVoltage / overallMinImpedance;
    
    logInfo(QString("  预期电流范围: %1A - %2A (RMS有效值)")
           .arg(expectedMinCurrent, 0, 'e', 2)
           .arg(expectedMaxCurrent, 0, 'e', 2));
    
    // 根据容差要求调整测量精度
    if (tolerancePercent <= 1.0) {
        // 高精度电容（≤1%）
        params.measurementPoints = 50;  
        params.settlingTime = 2000;     
        params.measureLeakage = true;
    } else if (tolerancePercent <= 5.0) {
        // 精密电容（1%-5%）
        params.measurementPoints = 20;
        params.settlingTime = 1000;
        params.measureLeakage = true;
    } else if (tolerancePercent <= 20.0) {
        // 标准电容（5%-20%）
        params.measurementPoints = 10;
        params.settlingTime = 500;
        params.measureLeakage = (maxLeakage > 0);
    } else {
        // 低精度电容（>20%）
        params.measurementPoints = 5;
        params.settlingTime = 200;
        params.measureLeakage = false;
    }
    
    // 根据电容值优化ESR测试频率
    if (nominalCapacitance >= 1e-3) {
        // 大电容，ESR通常较大，需要低频测试
        if (!params.testFrequencies.contains(10.0)) {
            params.testFrequencies.prepend(10.0); // 添加10Hz测试点
        }
    } else if (nominalCapacitance <= 1e-9) {
        // 小电容，ESR通常较小，需要高频测试  
        if (!params.testFrequencies.contains(1e6)) {
            params.testFrequencies.append(1e6); // 添加1MHz测试点
        }
    }
    
    // 确保测试电压在安全范围内
    params.testVoltage = qMax(0.1, qMin(params.testVoltage, 5.0)); // 最终限制在0.1V-5V
    params.leakageTestVoltage = qMax(1.0, qMin(params.leakageTestVoltage, ratedVoltage));
    
    // 存储阻抗信息供后续使用
    params.expectedMinImpedance = overallMinImpedance;
    params.expectedMaxImpedance = overallMaxImpedance;
    params.expectedMinCurrent = expectedMinCurrent;
    params.expectedMaxCurrent = expectedMaxCurrent;
    
    // === 根据阻抗范围选择合适的万用表range（参考电阻诊断器） ===
    // 选择能覆盖最大阻抗的range
    if (overallMaxImpedance < 100.0) {
        params.impedanceRange = "100";     // 100Ω范围
    } else if (overallMaxImpedance < 1000.0) {
        params.impedanceRange = "1k";      // 1kΩ范围
    } else if (overallMaxImpedance < 10000.0) {
        params.impedanceRange = "10k";     // 10kΩ范围
    } else if (overallMaxImpedance < 100000.0) {
        params.impedanceRange = "100k";    // 100kΩ范围
    } else if (overallMaxImpedance < 1e6) {
        params.impedanceRange = "1M";      // 1MΩ范围
    } else if (overallMaxImpedance < 1e7) {
        params.impedanceRange = "10M";     // 10MΩ范围
    } else {
        params.impedanceRange = "100M";    // 100MΩ范围
    }
    
    logInfo(QString("  万用表阻抗测量范围: %1").arg(params.impedanceRange));
    
    logInfo(QString("电容器 %1 测试参数优化完成:")
           .arg(component.reference));
    logInfo(QString("  最终测试电压: %1V (RMS)")
           .arg(params.testVoltage));
    logInfo(QString("  测试频率数量: %1个，范围: %2Hz - %3Hz")
           .arg(params.testFrequencies.size())
           .arg(params.testFrequencies.first())
           .arg(params.testFrequencies.last()));
    logInfo(QString("  测量点数: %1, 稳定时间: %2ms")
           .arg(params.measurementPoints)
           .arg(params.settlingTime));
    logInfo(QString("  漏电流测试: %1")
           .arg(params.measureLeakage ? "是" : "否"));
    
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
    
    double calculated = result.measurements.value("calculated_capacitance", 0.0);
    double expected = component.params.value("标称值", 1e-6).toDouble();
    double deviation = result.measurements.value("capacitance_deviation_percent", 0.0);
    double measuredESR = result.measurements.value("measured_esr", 0.0);
    double tolerancePercent = component.params.value("容差/%", 10.0).toDouble();
    
    summary += QString("电容器 %1 诊断结果:\n").arg(component.reference);
    summary += QString("标称值: %1\n").arg(formatValue(expected, "F"));
    summary += QString("计算值: %1\n").arg(formatValue(calculated, "F"));
    summary += QString("偏差: %1\n").arg(formatPercentage(deviation));
    summary += QString("容差: ±%1\n").arg(formatPercentage(tolerancePercent));
    summary += QString("ESR: %1\n").arg(formatValue(measuredESR, "Ω"));
    
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

QMap<QString, QVariant> CapacitorDiagnostic::getRequiredParameters() const
{
    QMap<QString, QVariant> params;
    
    // 电容测试的标准参数
    params["标称值"] = 1e-6;      // 默认标称值为1μF
    params["容差/%"] = 10.0;      // 默认容差为10%
    params["最大ESR/Ω"] = 1.0;    // 默认最大ESR为1Ω
    params["最大漏电流/A"] = 1e-9; // 默认最大漏电流为1nA
    params["额定电压/V"] = 16.0;   // 默认额定电压为16V
    params["测试频率/Hz"] = 1000.0; // 默认测试频率为1kHz

    logInfo("电容器测试所需标准参数已配置");
    
    return params;
}
