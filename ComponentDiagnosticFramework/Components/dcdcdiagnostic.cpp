#include "dcdcdiagnostic.h"
#include "../../devicemanager.h"
#include <QtMath>

DcdcDiagnostic::DcdcDiagnostic(DeviceManager* deviceManager, QObject* parent)
    : BaseComponentDiagnostic(deviceManager, parent)
{
    setObjectName("DcdcDiagnostic");
}

ComponentType DcdcDiagnostic::getSupportedComponentType() const
{
    return ComponentType::IC; // 暂归类为IC模块
}

QString DcdcDiagnostic::getComponentTypeName() const
{
    return "DC/DC电源模块";
}

QStringList DcdcDiagnostic::getSupportedModels() const
{
    return QStringList() << "降压模块" << "升压模块" << "升降压模块";
}

QVector<PortRequirement> DcdcDiagnostic::getPortRequirements(const ComponentSpec& component) const
{
    Q_UNUSED(component)
    QVector<PortRequirement> reqs;

    // 1. 输入激励：Vin 由JY5711提供或控制（如需）
    reqs.append(PortRequirement(PortType::ANALOG_OUTPUT, 1, "DC/DC 输入激励 - Vin (JY5711)"));

    // 2. 输出电压测量：JY5322
    reqs.append(PortRequirement(PortType::DIGITAL_INPUT, 1, "DC/DC 输出电压测量 - Vout (JY5322)"));

    // 3. 输出电流测量：JY5323
    reqs.append(PortRequirement(PortType::ANALOG_INPUT, 1, "DC/DC 输出电流测量 - Iout (JY5323)"));

    return reqs;
}

QMap<QString, QVariant> DcdcDiagnostic::getRequiredParameters() const
{
    QMap<QString, QVariant> p;
    p["Vin_min/V"] = 4.5;
    p["Vin_nom/V"] = 5.0;
    p["Vin_max/V"] = 5.5;
    p["Vout_nom/V"] = 3.3;
    p["Iout_nom/A"] = 1.0;
    p["Vout_tol/%"] = 2.0;    // 输出容差
    p["Ripple_pp_max/mV"] = 50.0;
    p["Eff_min/%"] = 80.0;    // 最低效率要求
    return p;
}

QVector<WiringConnection> DcdcDiagnostic::generateWiringScheme(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts) const
{
    QVector<WiringConnection> conns;
    if (allocatedPorts.size() < 3) {
        logError("分配的端口数量不足，DC/DC测试需要3个端口");
        return conns;
    }

    for (const auto& port : allocatedPorts) {
        switch (port.portType) {
        case PortType::ANALOG_OUTPUT: {
            WiringConnection c;
            c.component = component.reference;
            c.componentPin = "VIN+";
            c.targetPort = port;
            c.wireColor = "红色";
            c.instruction = QString("将模拟输出端口%1连接到DC/DC模块%2的输入正端VIN+，用于提供/调节输入电压")
                                .arg(port.portNumber).arg(component.reference);
            c.isRequired = true;
            conns.append(c);
            break;
        }
        case PortType::DIGITAL_INPUT: {
            WiringConnection c;
            c.component = component.reference;
            c.componentPin = "VOUT";
            c.targetPort = port;
            c.wireColor = "黄色";
            c.instruction = QString("将电压测量端口%1连接到DC/DC模块%2输出端VOUT，用于测量输出电压与纹波")
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
            c.instruction = QString("将模拟输入端口%1串入DC/DC模块%2输出回路中，用于测量输出电流Iout")
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

ComponentTestConfig DcdcDiagnostic::configureDataAcquisition(const ComponentSpec& component, const QVector<PortInfo>& ports) const
{
    ComponentTestConfig config;
    if (ports.isEmpty()) {
        logError("未分配端口");
        return config;
    }

    config.testName = QString("DC/DC测试_%1").arg(component.reference);

    double vin_nom = component.params.value("Vin_nom/V", 5.0).toDouble();

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
            ch["amplitude"] = vin_nom;   // 近似恒压输出
            ch["frequency"] = 0;         // 直流
            ch["lowRange"] = 0.0;
            ch["highRange"] = vin_nom * 1.2;
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
            cfg.parameters["rangeMax"] = 20.0;
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
            cfg.parameters["rangeMin"] = 0.0;
            cfg.parameters["rangeMax"] = component.params.value("Iout_nom/A", 1.0).toDouble() * 1.5;
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

    config.TemperatureThreshold = 80.0;
    return config;
}

TestData DcdcDiagnostic::executeDataAcquisition(const ComponentTestConfig& config)
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
            if (!getDeviceManager()->submitOperation(it.key(), it.value())) {
                throw std::runtime_error(QString("设备 %1 配置失败: %2").arg(it.key(), getDeviceManager()->getLastError()).toStdString());
            }
            DeviceResult r = getDeviceManager()->waitForResult(it.key(), 10000);
            if (!r.success) {
                throw std::runtime_error(QString("设备 %1 配置失败: %2").arg(it.key(), r.error).toStdString());
            }
        }

        // 写入Vin波形
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

        QVector<QVector<double>> voutData, ioutData;
        auto waitVout = getDeviceManager()->waitForDataWithEventLoop("JY5322", voutData, 30, 200, 8000);
        auto waitIout = getDeviceManager()->waitForDataWithEventLoop("JY5323", ioutData, 30, 200, 8000);

        if (!waitVout.success || voutData.isEmpty() || voutData[0].isEmpty()) {
            throw std::runtime_error("输出电压数据为空或读取失败");
        }
        if (!waitIout.success || ioutData.isEmpty() || ioutData[0].isEmpty()) {
            throw std::runtime_error("输出电流数据为空或读取失败");
        }

        QMap<QString, QVariant> meas;
        meas["vout_waveform"] = QVariant::fromValue(voutData[0]);
        meas["iout_waveform"] = QVariant::fromValue(ioutData[0]);
        data.measurements.append(meas);

        data.thermalidata = getDeviceManager()->getLatestThermalData();
        data.valid = true;
    } catch (const std::exception& e) {
        data.errorMessage = QString("DC/DC数据采集异常: %1").arg(e.what());
        logError(data.errorMessage);
    }

    return data;
}

ComponentDiagnosticResult DcdcDiagnostic::analyzeFaults(const ComponentSpec& component, const TestData& testData)
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

    auto meas = testData.measurements.first();
    QVector<double> vout = meas.value("vout_waveform").value<QVector<double>>();
    QVector<double> iout = meas.value("iout_waveform").value<QVector<double>>();

    if (vout.isEmpty() || iout.isEmpty()) {
        result.isPassed = false;
        result.summary = "采集数据为空";
        result.faultTypes.append("NO_DATA");
        return result;
    }

    double vout_nom = component.params.value("Vout_nom/V", 3.3).toDouble();
    double vout_tol_percent = component.params.value("Vout_tol/%", 2.0).toDouble();
    double ripple_max_mV = component.params.value("Ripple_pp_max/mV", 50.0).toDouble();

    double vAvg = 0.0, vP2P = 0.0;
    double ripple_rms = calculateRipple(vout, vAvg, vP2P);

    double iAvg = 0.0;
    for (double v : iout) iAvg += v;
    iAvg /= iout.size();

    result.measurements["Vout_avg/V"] = vAvg;
    result.measurements["Iout_avg/A"] = iAvg;
    result.measurements["Vripple_pp/mV"] = vP2P * 1000.0;
    result.measurements["Vripple_rms/mV"] = ripple_rms * 1000.0;
    result.measurements["最高温度"] = testData.thermalidata.maxTemp;

    bool hasFault = false;

    // 输出电压精度
    double vErrPercent = calculateDeviation(vAvg, vout_nom);
    result.measurements["Vout误差/%"] = vErrPercent;
    if (qAbs(vErrPercent) > vout_tol_percent) {
        result.faultTypes.append("VOUT_OUT_OF_TOL");
        result.recommendations.append("输出电压偏差超出容差，检查反馈/负载配置");
        hasFault = true;
    }

    // 纹波
    if (vP2P * 1000.0 > ripple_max_mV) {
        result.faultTypes.append("RIPPLE_TOO_HIGH");
        result.recommendations.append("输出纹波过大，检查输出电容与布局");
        hasFault = true;
    }

    // 温度
    if (testData.thermalidata.maxTemp > 80.0) {
        result.faultTypes.append("TEMP_TOO_HIGH");
        result.recommendations.append("DC/DC模块温度过高，可能过载或散热不良");
        hasFault = true;
    }

    result.isPassed = !hasFault;
    result.healthScore = hasFault ? 60.0 : 95.0;
    result.confidence = 0.9;
    result.summary = hasFault ? "DC/DC模块存在异常，详见故障类型与建议" : "DC/DC模块输出电压与纹波在规定范围内";

    if (!hasFault) {
        result.recommendations.append("元件工作正常");
    }

    return result;
}

bool DcdcDiagnostic::validateComponentSpec(const ComponentSpec& component) const
{
    Q_UNUSED(component)
    return true;
}

bool DcdcDiagnostic::preTestSetup(const ComponentSpec& component)
{
    logInfo(QString("DC/DC测试预设置: %1").arg(component.reference));
    return true;
}

void DcdcDiagnostic::postTestCleanup()
{
    logInfo("DC/DC测试后清理");
}

double DcdcDiagnostic::calculateEfficiency(double vin, double iin, double vout, double iout) const
{
    double pin = vin * iin;
    double pout = vout * iout;
    if (pin <= 0) return 0.0;
    return pout / pin;
}

double DcdcDiagnostic::calculateRipple(const QVector<double>& waveform, double& vAvg, double& vP2P) const
{
    if (waveform.isEmpty()) {
        vAvg = 0.0;
        vP2P = 0.0;
        return 0.0;
    }

    vAvg = 0.0;
    double vMin = waveform[0];
    double vMax = waveform[0];
    for (double v : waveform) {
        vAvg += v;
        if (v < vMin) vMin = v;
        if (v > vMax) vMax = v;
    }
    vAvg /= waveform.size();
    vP2P = vMax - vMin;

    double var = 0.0;
    for (double v : waveform) {
        var += qPow(v - vAvg, 2);
    }
    var /= waveform.size();
    return qSqrt(var);
}
