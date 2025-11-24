#include "optocouplerdiagnostic.h"
#include "../../devicemanager.h"
#include <QtMath>

OptocouplerDiagnostic::OptocouplerDiagnostic(DeviceManager* deviceManager, QObject* parent)
    : BaseComponentDiagnostic(deviceManager, parent)
{
    setObjectName("OptocouplerDiagnostic");
}

ComponentType OptocouplerDiagnostic::getSupportedComponentType() const
{
    return ComponentType::IC; // 如有需要可在ComponentType中扩展专门的OPTO类型
}

QString OptocouplerDiagnostic::getComponentTypeName() const
{
    return "光电耦合器";
}

QStringList OptocouplerDiagnostic::getSupportedModels() const
{
    return QStringList() << "通用光耦" << "高速光耦";
}

QVector<PortRequirement> OptocouplerDiagnostic::getPortRequirements(const ComponentSpec& component) const
{
    Q_UNUSED(component)
    QVector<PortRequirement> reqs;

    // 1. 输入侧LED驱动：使用JY5711模拟输出
    PortRequirement ledDrive(PortType::ANALOG_OUTPUT, 1, "光耦输入LED驱动 - JY5711");
    reqs.append(ledDrive);

    // 2. 输出侧电压测量：JY5322
    PortRequirement outVolt(PortType::DIGITAL_INPUT, 1, "光耦输出侧电压测量 - JY5322");
    reqs.append(outVolt);

    // 3. 输出侧电流测量：JY5323
    PortRequirement outCurr(PortType::ANALOG_INPUT, 1, "光耦输出侧电流测量 - JY5323");
    reqs.append(outCurr);

    return reqs;
}

QMap<QString, QVariant> OptocouplerDiagnostic::getRequiredParameters() const
{
    QMap<QString, QVariant> params;
    params["测试电流/mA"] = 5.0;       // LED测试电流，默认5mA
    params["最小CTR/%"] = 50.0;        // 最小允许CTR
    params["最大漏电流/uA"] = 10.0;    // 最大允许漏电流
    return params;
}

QVector<WiringConnection> OptocouplerDiagnostic::generateWiringScheme(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts) const
{
    QVector<WiringConnection> conns;
    if (allocatedPorts.size() < 3) {
        logError("分配的端口数量不足，光耦测试需要3个端口");
        return conns;
    }

    for (const auto& port : allocatedPorts) {
        switch (port.portType) {
        case PortType::ANALOG_OUTPUT: {
            WiringConnection c;
            c.component = component.reference;
            c.componentPin = "LED+";
            c.targetPort = port;
            c.wireColor = "红色";
            c.instruction = QString("将模拟输出端口%1连接到光耦%2输入LED正端(阳极)，经限流电阻形成测试电流")
                                .arg(port.portNumber).arg(component.reference);
            c.isRequired = true;
            conns.append(c);
            break;
        }
        case PortType::DIGITAL_INPUT: {
            WiringConnection c;
            c.component = component.reference;
            c.componentPin = "输出集电极";
            c.targetPort = port;
            c.wireColor = "黑色";
            c.instruction = QString("将电压测量端口%1连接到光耦%2输出集电极节点，用于测量VCE/负载电压")
                                .arg(port.portNumber).arg(component.reference);
            c.isRequired = true;
            conns.append(c);
            break;
        }
        case PortType::ANALOG_INPUT: {
            WiringConnection c;
            c.component = component.reference;
            c.componentPin = "输出电流回路";
            c.targetPort = port;
            c.wireColor = "绿色";
            c.instruction = QString("将模拟输入端口%1串入光耦%2输出回路中，用于测量集电极电流IC")
                                .arg(port.portNumber).arg(component.reference);
            c.isRequired = true;
            conns.append(c);
            break;
        }
        default:
            break;
        }
    }

    return conns;
}

ComponentTestConfig OptocouplerDiagnostic::configureDataAcquisition(const ComponentSpec& component, const QVector<PortInfo>& ports) const
{
    ComponentTestConfig config;
    if (ports.isEmpty()) {
        logError("未分配端口");
        return config;
    }

    config.testName = QString("光耦测试_%1").arg(component.reference);

    // 从参数获取测试电流（mA）
    double testCurrent_mA = component.params.value("测试电流/mA", 5.0).toDouble();
    double testCurrent_A = testCurrent_mA / 1000.0;

    DeviceOperation op5711(DeviceCommand::CONFIGURE_CHANNEL);
    op5711.parameters["sampleRate"] = 100000.0;
    op5711.parameters["samplesPerChannel"] = 100000.0;
    QVariantList waveforms;

    for (const auto& port : ports) {
        switch (port.portType) {
        case PortType::ANALOG_OUTPUT: {
            QVariantMap ch;
            ch["channel"] = port.portNumber;
            ch["type"] = static_cast<int>(PXIe5711_testtype::HighLevelWave);
            ch["amplitude"] = 2.0;    // 2V 高电平，经外部电阻设定IF
            ch["frequency"] = 1000;   // 1kHz
            ch["lowRange"] = 0.0;
            ch["highRange"] = 5.0;
            waveforms.append(ch);
            break;
        }
        case PortType::DIGITAL_INPUT: {
            DeviceOperation cfg(DeviceCommand::CONFIGURE_CHANNEL);
            cfg.parameters["mode"] = "multi";
            cfg.parameters["channels"] = QVariant::fromValue(QVector<int>({port.portNumber}));
            cfg.parameters["sampleRate"] = 1000000.0;
            cfg.parameters["samplesPerChannel"] = 1000000;
            cfg.parameters["rangeMin"] = 0.0;
            cfg.parameters["rangeMax"] = 10.0;
            cfg.timeout = 10000;
            config.parameters["JY5322"] = cfg;
            break;
        }
        case PortType::ANALOG_INPUT: {
            DeviceOperation cfg(DeviceCommand::CONFIGURE_CHANNEL);
            cfg.parameters["mode"] = "multi";
            cfg.parameters["channels"] = QVariant::fromValue(QVector<int>({port.portNumber}));
            cfg.parameters["sampleRate"] = 200000.0;
            cfg.parameters["samplesPerChannel"] = 200000;
            cfg.parameters["rangeMin"] = -0.02;  // ±20mA量程（通过分流/换算）
            cfg.parameters["rangeMax"] = 0.02;
            cfg.timeout = 10000;
            config.parameters["JY5323"] = cfg;
            break;
        }
        default:
            break;
        }
    }

    op5711.parameters["waveforms"] = waveforms;
    op5711.parameters["channelCount"] = waveforms.size();
    if (!waveforms.isEmpty()) {
        config.parameters["JY5711"] = op5711;
    }

    config.TemperatureThreshold = 60.0;
    // 额外在metadata中记录计划测试电流
    DeviceOperation meta;
    meta.value = testCurrent_A;
    config.parameters["TEST_IF_A"] = meta;

    return config;
}

TestData OptocouplerDiagnostic::executeDataAcquisition(const ComponentTestConfig& config)
{
    TestData data;
    data.testId = config.testName;
    data.timestamp = QDateTime::currentDateTime();
    data.valid = false;

    if (!getDeviceManager() || !getDeviceManager()->isSystemReady()) {
        data.errorMessage = "设备管理器未就绪";
        return data;
    }

    getDeviceManager()->setTemperatureThreshold(config.TemperatureThreshold);
    if (getDeviceManager()->getCameraManager()) {
        getDeviceManager()->getCameraManager()->startCamera(CameraType::IR_CAMERA);
    }

    try {
        // 配置设备
        for (auto it = config.parameters.begin(); it != config.parameters.end(); ++it) {
            if (it.key() == "TEST_IF_A") continue; // 元数据，不作为设备
            if (!getDeviceManager()->submitOperation(it.key(), it.value())) {
                throw std::runtime_error(QString("设备 %1 配置失败: %2").arg(it.key(), getDeviceManager()->getLastError()).toStdString());
            }
            DeviceResult r = getDeviceManager()->waitForResult(it.key(), 10000);
            if (!r.success) {
                throw std::runtime_error(QString("设备 %1 配置失败: %2").arg(it.key(), r.error).toStdString());
            }
        }

        // 写入驱动波形
        if (config.parameters.contains("JY5711")) {
            DeviceOperation writeOp(DeviceCommand::WRITE_DATA);
            writeOp.parameters["waveforms"] = config.parameters["JY5711"].parameters["waveforms"];
            writeOp.parameters["sampleRate"] = config.parameters["JY5711"].parameters["sampleRate"];
            writeOp.parameters["samplesPerChannel"] = config.parameters["JY5711"].parameters["samplesPerChannel"];
            if (!getDeviceManager()->submitOperation("JY5711", writeOp)) {
                throw std::runtime_error(QString("JY5711写入数据失败: %1").arg(getDeviceManager()->getLastError()).toStdString());
            }
            DeviceResult wr = getDeviceManager()->waitForResult("JY5711", 15000);
            if (!wr.success) {
                throw std::runtime_error(QString("JY5711写入数据失败: %1").arg(wr.error).toStdString());
            }
        }

        // 这里可以类似电阻测试，使用同步组触发与多通道采集；
        // 简化：直接读取各通道的多点数据
        QVector<QVector<double>> icData, vceData;
        DeviceManager::WaitResult waitIc = getDeviceManager()->waitForDataWithEventLoop("JY5323", icData, 30, 200, 8000);
        DeviceManager::WaitResult waitV = getDeviceManager()->waitForDataWithEventLoop("JY5322", vceData, 30, 200, 8000);

        if (!waitIc.success || icData.isEmpty() || icData[0].isEmpty()) {
            throw std::runtime_error("输出电流数据为空或读取失败");
        }
        if (!waitV.success || vceData.isEmpty() || vceData[0].isEmpty()) {
            throw std::runtime_error("输出电压数据为空或读取失败");
        }

        QMap<QString, QVariant> meas;
        meas["collector_currents"] = QVariant::fromValue(icData[0]);
        meas["collector_voltages"] = QVariant::fromValue(vceData[0]);
        data.measurements.append(meas);

        data.thermalidata = getDeviceManager()->getLatestThermalData();
        data.valid = true;
    } catch (const std::exception& e) {
        data.errorMessage = QString("光耦数据采集异常: %1").arg(e.what());
        logError(data.errorMessage);
    }

    return data;
}

ComponentDiagnosticResult OptocouplerDiagnostic::analyzeFaults(const ComponentSpec& component, const TestData& testData)
{
    ComponentDiagnosticResult result;
    result.componentId = component.reference;
    result.componentType = getComponentTypeName();
    result.timestamp = QDateTime::currentDateTime();
    result.thermalData = testData.thermalidata;

    if (!testData.valid || testData.measurements.isEmpty()) {
        result.isPassed = false;
        result.healthScore = 0.0;
        result.confidence = 0.0;
        result.summary = "测试数据无效";
        result.faultTypes.append("DATA_INVALID");
        return result;
    }

    QMap<QString, QVariant> meas = testData.measurements.first();
    QVector<double> ic = meas.value("collector_currents").value<QVector<double>>();
    QVector<double> vce = meas.value("collector_voltages").value<QVector<double>>();

    if (ic.isEmpty() || vce.isEmpty()) {
        result.isPassed = false;
        result.summary = "采集数据为空";
        result.faultTypes.append("NO_DATA");
        return result;
    }

    double testCurrent_mA = component.params.value("测试电流/mA", 5.0).toDouble();
    double testCurrent_A = testCurrent_mA / 1000.0;
    double minCTR_percent = component.params.value("最小CTR/%", 50.0).toDouble();
    double maxLeak_uA = component.params.value("最大漏电流/uA", 10.0).toDouble();

    // 简化：使用平均IC作为工作点
    double avgIc = 0.0;
    for (double v : ic) avgIc += v;
    avgIc /= ic.size();

    double ctr = calculateCTR(testCurrent_A, avgIc); // 比值
    double ctr_percent = ctr * 100.0;

    result.measurements["测试IF/mA"] = testCurrent_mA;
    result.measurements["平均IC/mA"] = avgIc * 1000.0;
    result.measurements["CTR/%"] = ctr_percent;
    result.measurements["最高温度"] = testData.thermalidata.maxTemp;

    bool hasFault = false;

    // CTR判定
    if (ctr_percent < minCTR_percent) {
        result.faultTypes.append("CTR_TOO_LOW");
        result.recommendations.append("光耦CTR过低，可能LED老化或光敏管退化，建议更换元件");
        hasFault = true;
    }

    // 漏电流判定：简单用最小IC近似关断电流
    double minIc = *std::min_element(ic.begin(), ic.end());
    double minIc_uA = minIc * 1e6;
    result.measurements["最小IC/uA"] = minIc_uA;
    if (qAbs(minIc_uA) > maxLeak_uA) {
        result.faultTypes.append("LEAKAGE_TOO_HIGH");
        result.recommendations.append("光耦输出漏电流过大，绝缘性能下降");
        hasFault = true;
    }

    // 温度判定
    if (testData.thermalidata.maxTemp > 60.0) {
        result.faultTypes.append("TEMP_TOO_HIGH");
        result.recommendations.append("光耦在测试过程中温度过高，可能过载或散热不良");
        hasFault = true;
    }

    result.isPassed = !hasFault;
    result.healthScore = hasFault ? 60.0 : 95.0; // 简化健康度
    result.confidence = 0.9;

    result.summary = hasFault ? "光耦存在异常，详见故障类型与建议" : "光耦工作正常，CTR与漏电流在允许范围内";

    if (!hasFault) {
        result.recommendations.append("元件工作正常");
    }

    return result;
}

bool OptocouplerDiagnostic::validateComponentSpec(const ComponentSpec& component) const
{
    Q_UNUSED(component)
    return true;
}

bool OptocouplerDiagnostic::preTestSetup(const ComponentSpec& component)
{
    logInfo(QString("光耦测试预设置: %1").arg(component.reference));
    return true;
}

void OptocouplerDiagnostic::postTestCleanup()
{
    logInfo("光耦测试后清理");
}

double OptocouplerDiagnostic::calculateCTR(double ledCurrent, double collectorCurrent) const
{
    if (ledCurrent <= 0) return 0.0;
    return collectorCurrent / ledCurrent;
}
