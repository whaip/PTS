#include "inductordiagnostic.h"
#include "../../devicemanager.h"
#include <QtMath>
#include <QDebug>
#include <QUuid>
#include <algorithm>

// 静态常量定义
const double InductorDiagnostic::DEFAULT_START_FREQ = 100.0;        // 100 Hz
const double InductorDiagnostic::DEFAULT_END_FREQ = 1000000.0;      // 1 MHz
const int InductorDiagnostic::DEFAULT_FREQ_POINTS = 50;
const double InductorDiagnostic::DEFAULT_MIN_CURRENT = 0.0001;      // 0.1 mA
const double InductorDiagnostic::DEFAULT_MAX_CURRENT = 0.1;         // 100 mA
const double InductorDiagnostic::DEFAULT_TOLERANCE = 10.0;          // 10%
const double InductorDiagnostic::DEFAULT_MIN_Q_FACTOR = 10.0;

const double InductorDiagnostic::OPEN_CIRCUIT_THRESHOLD = 1e12;     // 1TΩ
const double InductorDiagnostic::SHORT_CIRCUIT_THRESHOLD = 0.001;   // 1mΩ
const double InductorDiagnostic::SATURATION_THRESHOLD = 20.0;       // 20%

InductorDiagnostic::InductorDiagnostic(DeviceManager* deviceManager, QObject* parent)
    : BaseComponentDiagnostic(deviceManager, parent)
    , startFrequency_(DEFAULT_START_FREQ)
    , endFrequency_(DEFAULT_END_FREQ)
    , frequencyPoints_(DEFAULT_FREQ_POINTS)
    , minTestCurrent_(DEFAULT_MIN_CURRENT)
    , maxTestCurrent_(DEFAULT_MAX_CURRENT)
    , tolerance_(DEFAULT_TOLERANCE)
    , minQualityFactor_(DEFAULT_MIN_Q_FACTOR)
    , saturationTestEnabled_(false)
    , saturationMaxCurrent_(0.1)
    , saturationStepSize_(0.01)
{
    QObject::setObjectName("InductorDiagnostic");
}

ComponentType InductorDiagnostic::getSupportedComponentType() const
{
    return ComponentType::INDUCTOR;
}

QString InductorDiagnostic::getComponentTypeName() const
{
    return "电感器";
}

QVector<PortRequirement> InductorDiagnostic::getPortRequirements(const ComponentSpec& component) const
{
    Q_UNUSED(component)
    
    QVector<PortRequirement> requirements;
    
    // 电感器需要2个端口用于电感量测量
    PortRequirement port1;
    port1.portType = PortType::ANALOG_INPUT;
    port1.count = 1;
    port1.description = "电感器第一端 - AC电流输入";
    port1.specs["signalType"] = "AC_CURRENT";
    port1.specs["voltageRange"] = QVariantList{-10.0, 10.0};
    port1.specs["currentRange"] = QVariantList{-0.1, 0.1};
    port1.specs["frequencyRange"] = QVariantList{startFrequency_, endFrequency_};
    requirements.append(port1);
    
    PortRequirement port2;
    port2.portType = PortType::ANALOG_OUTPUT;
    port2.count = 1;
    port2.description = "电感器第二端 - AC电压输出";
    port2.specs["signalType"] = "AC_VOLTAGE";
    port2.specs["voltageRange"] = QVariantList{-10.0, 10.0};
    port2.specs["currentRange"] = QVariantList{-0.1, 0.1};
    port2.specs["frequencyRange"] = QVariantList{startFrequency_, endFrequency_};
    requirements.append(port2);
    
    return requirements;
}

QVector<WiringConnection> InductorDiagnostic::generateWiringScheme(const ComponentSpec& component, 
                                                                 const QVector<PortInfo>& allocatedPorts) const
{
    Q_UNUSED(component)
    Q_UNUSED(allocatedPorts)
    
    QVector<WiringConnection> connections;
    
    // 基本连接：将电感器连接到测量端口
    WiringConnection conn1;
    conn1.componentPin = "L1";
    conn1.wireColor = "红色";
    conn1.instruction = "电感器引脚1连接到输入端口";
    conn1.isRequired = true;
    connections.append(conn1);
    
    WiringConnection conn2;
    conn2.componentPin = "L2";
    conn2.wireColor = "黑色";
    conn2.instruction = "电感器引脚2连接到输出端口";
    conn2.isRequired = true;
    connections.append(conn2);
    
    return connections;
}

ComponentTestConfig InductorDiagnostic::configureDataAcquisition(const ComponentSpec& component,
                                                                const QVector<PortInfo>& ports) const
{
    Q_UNUSED(ports)
    
    ComponentTestConfig config;
    config.testName = QString("Inductor Test - %1").arg(component.reference);
    config.timeout = 30000;
    
    // 根据电感器规格设置测量参数
    config.parameters["frequency_start"] = startFrequency_;
    config.parameters["frequency_end"] = endFrequency_;
    config.parameters["frequency_points"] = frequencyPoints_;
    config.parameters["test_current"] = minTestCurrent_;
    config.parameters["measurement_mode"] = "INDUCTANCE";
    config.parameters["measurement_speed"] = "MEDIUM";
    config.parameters["averaging"] = 4;
    
    // 根据电感器规格调整测量参数
    if (component.parameters.contains("nominal_value")) {
        double nominalL = component.parameters["nominal_value"].toDouble();
        
        // 根据电感量调整测试频率范围
        if (nominalL > 1e-3) {
            // 大电感：降低测试频率
            config.parameters["frequency_end"] = qMin(endFrequency_, 100000.0);
        } else if (nominalL < 1e-6) {
            // 小电感：提高测试频率
            config.parameters["frequency_start"] = qMax(startFrequency_, 1000.0);
        }
    }
    
    return config;
}

TestData InductorDiagnostic::executeDataAcquisition(const ComponentTestConfig& config)
{
    TestData testData;
    testData.testId = QUuid::createUuid().toString();
    testData.timestamp = QDateTime::currentDateTime();
    testData.valid = false;
    
    QString componentRef = config.testName.contains("-") ? 
                          config.testName.split("-").last().trimmed() : "Unknown";
    
    emit diagnosisStarted(componentRef);
    emit diagnosisProgress(componentRef, 10);
    
    try {
        // 1. 基本连通性检查
        int faultType = detectOpenShortFault();
        testData.measurements.append(QVariantMap{{"open_short_fault", faultType}});
        
        if (faultType == 1) {
            testData.errorMessage = "Open circuit detected";
            testData.measurements.append(QVariantMap{{"error", "Open circuit"}});
            return testData;
        } else if (faultType == 2) {
            testData.errorMessage = "Short circuit detected";
            testData.measurements.append(QVariantMap{{"error", "Short circuit"}});
            return testData;
        }
        
        emit diagnosisProgress(componentRef, 25);
        
        // 2. 多频率电感量测量
        QVector<double> frequencies = generateFrequencySequence(startFrequency_, endFrequency_, frequencyPoints_);
        QMap<double, double> inductanceSpectrum = measureInductanceSpectrum(frequencies, minTestCurrent_);
        testData.measurements.append(QVariantMap{{"inductance_spectrum", QVariant::fromValue(inductanceSpectrum)}});
        
        emit diagnosisProgress(componentRef, 50);
        
        // 3. Q值测量
        QMap<double, double> qValues;
        for (double freq : frequencies) {
            if (qValues.size() >= 10) break; // 限制测量点数
            double q = measureQualityFactor(freq, minTestCurrent_);
            if (q > 0) {
                qValues[freq] = q;
            }
        }
        testData.measurements.append(QVariantMap{{"quality_factors", QVariant::fromValue(qValues)}});
        
        emit diagnosisProgress(componentRef, 70);
        
        // 4. ESR测量
        QMap<double, double> esrValues;
        for (auto it = qValues.begin(); it != qValues.end(); ++it) {
            double esr = measureESR(it.key(), minTestCurrent_);
            if (esr > 0) {
                esrValues[it.key()] = esr;
            }
        }
        testData.measurements.append(QVariantMap{{"esr_values", QVariant::fromValue(esrValues)}});
        
        emit diagnosisProgress(componentRef, 85);
        
        // 5. 谐振频率检测
        double resonantFreq = detectResonantFrequency();
        testData.measurements.append(QVariantMap{{"resonant_frequency", resonantFreq}});
        
        // 6. 饱和特性测试（如果启用）
        if (saturationTestEnabled_) {
            double testFreq = 1000.0; // 使用1kHz进行饱和测试
            QMap<double, double> saturationData = measureSaturationCharacteristic(testFreq);
            testData.measurements.append(QVariantMap{{"saturation_characteristic", QVariant::fromValue(saturationData)}});
        }
        
        emit diagnosisProgress(componentRef, 100);
        
        // 记录测量条件
        testData.measurements.append(QVariantMap{
            {"test_conditions", QVariantMap{
                {"frequency_range", QVariantList{startFrequency_, endFrequency_}},
                {"frequency_points", frequencyPoints_},
                {"test_current", minTestCurrent_},
                {"saturation_test_enabled", saturationTestEnabled_}
            }}
        });
        
        testData.valid = true;
        
    } catch (const std::exception& e) {
        testData.errorMessage = QString("Data acquisition failed: %1").arg(e.what());
        testData.measurements.append(QVariantMap{{"error", testData.errorMessage}});
    }
    
    return testData;
}

ComponentDiagnosticResult InductorDiagnostic::analyzeFaults(const ComponentSpec& component, 
                                                           const TestData& testData)
{
    ComponentDiagnosticResult result;
    result.componentId = component.reference;
    result.componentType = "INDUCTOR";
    result.timestamp = QDateTime::currentDateTime();
    result.setResult(true);
    
    // 检查是否有错误
    for (const auto& measurement : testData.measurements) {
        if (measurement.contains("error")) {
            result.setResult(false);
            result.notes = measurement["error"].toString();
            emit diagnosisCompleted(component.reference, result);
            return result;
        }
    }
    
    try {
        // 获取标称值
        double nominalInductance = 0;
        if (component.parameters.contains("nominal_value")) {
            nominalInductance = component.parameters["nominal_value"].toDouble();
        }
        
        QStringList faultMessages;
        
        // 处理测量数据
        QMap<double, double> inductanceSpectrum;
        QMap<double, double> qValues;
        QMap<double, double> esrValues;
        double resonantFreq = -1.0;
        QMap<double, double> saturationData;
        
        // 从testData中提取数据
        for (const auto& measurement : testData.measurements) {
            if (measurement.contains("open_short_fault")) {
                int faultType = measurement["open_short_fault"].toInt();
                if (faultType == 1) {
                    faultMessages.append("开路故障");
                    result.setResult(false);
                } else if (faultType == 2) {
                    faultMessages.append("短路故障");
                    result.setResult(false);
                }
            } else if (measurement.contains("inductance_spectrum")) {
                inductanceSpectrum = measurement["inductance_spectrum"].value<QMap<double, double>>();
            } else if (measurement.contains("quality_factors")) {
                qValues = measurement["quality_factors"].value<QMap<double, double>>();
            } else if (measurement.contains("esr_values")) {
                esrValues = measurement["esr_values"].value<QMap<double, double>>();
            } else if (measurement.contains("resonant_frequency")) {
                resonantFreq = measurement["resonant_frequency"].toDouble();
            } else if (measurement.contains("saturation_characteristic")) {
                saturationData = measurement["saturation_characteristic"].value<QMap<double, double>>();
            }
        }
        
        // 2. 分析电感量测量结果
        if (!inductanceSpectrum.isEmpty()) {
            auto inductanceAnalysis = analyzeInductanceValues(nominalInductance, inductanceSpectrum);
            result.analysisData["inductance_analysis"] = inductanceAnalysis;
            
            // 记录主要电感量值
            double avgInductance = 0;
            for (auto value : inductanceSpectrum.values()) {
                avgInductance += value;
            }
            avgInductance /= inductanceSpectrum.size();
            result.measurements["average_inductance"] = avgInductance;
            
            // 检查容差
            if (nominalInductance > 0) {
                double deviation = qAbs(avgInductance - nominalInductance) / nominalInductance * 100.0;
                result.measurements["deviation_percent"] = deviation;
                
                if (deviation > tolerance_) {
                    faultMessages.append(QString("电感量超出容差：偏差%.1f%%").arg(deviation));
                    result.setResult(false);
                }
            }
        }
        
        // 3. 分析Q值
        if (!qValues.isEmpty()) {
            auto qAnalysis = analyzeQualityFactor(qValues);
            result.analysisData["quality_factor_analysis"] = qAnalysis;
            
            double avgQ = 0;
            for (auto value : qValues.values()) {
                avgQ += value;
            }
            avgQ /= qValues.size();
            result.measurements["average_q_factor"] = avgQ;
            
            if (avgQ < minQualityFactor_) {
                faultMessages.append(QString("Q值过低：%.1f < %.1f").arg(avgQ).arg(minQualityFactor_));
                result.setResult(false);
            }
        }
        
        // 4. 分析ESR
        if (!esrValues.isEmpty()) {
            double avgESR = 0;
            for (auto value : esrValues.values()) {
                avgESR += value;
            }
            avgESR /= esrValues.size();
            result.measurements["average_esr"] = avgESR;
            
            // ESR过高检查
            double expectedESR = nominalInductance > 0 ? qSqrt(nominalInductance * 1e-6) : 1.0;
            if (avgESR > expectedESR * 10) {
                faultMessages.append(QString("等效串联电阻过高：%.3fΩ").arg(avgESR));
            }
        }
        
        // 5. 分析频率特性
        if (!inductanceSpectrum.isEmpty()) {
            auto freqAnalysis = analyzeFrequencyCharacteristic(inductanceSpectrum);
            result.analysisData["frequency_characteristic"] = freqAnalysis;
        }
        
        // 6. 分析谐振频率
        if (resonantFreq > 0) {
            result.measurements["resonant_frequency"] = resonantFreq;
            
            // 估算寄生电容
            if (nominalInductance > 0) {
                double parasiticC = 1.0 / (4 * M_PI * M_PI * resonantFreq * resonantFreq * nominalInductance);
                result.measurements["estimated_parasitic_capacitance"] = parasiticC;
            }
        }
        
        // 7. 分析饱和特性
        if (!saturationData.isEmpty()) {
            auto saturationAnalysis = analyzeSaturationCharacteristic(saturationData);
            result.analysisData["saturation_analysis"] = saturationAnalysis;
            
            if (saturationAnalysis.contains("saturated") && saturationAnalysis["saturated"].toBool()) {
                faultMessages.append("检测到磁芯饱和");
            }
        }
        
        // 设置结果
        if (faultMessages.isEmpty()) {
            result.faultTypes << "NORMAL";
            result.summary = "电感器工作正常";
        } else {
            result.faultTypes << "PARAMETER_DEVIATION";
            result.summary = faultMessages.join("; ");
        }
        
        // 计算健康评分
        result.healthScore = result.isPassed ? 95.0 : 60.0;
        result.confidence = 0.85;
        
        // 生成建议
        QStringList recommendations;
        if (result.measurements.contains("deviation_percent")) {
            double deviation = result.measurements["deviation_percent"];
            if (deviation > tolerance_) {
                recommendations.append("检查电感器是否为正确型号");
                recommendations.append("检查测量连接和校准");
            }
        }
        
        if (result.measurements.contains("average_q_factor")) {
            double avgQ = result.measurements["average_q_factor"];
            if (avgQ < minQualityFactor_) {
                recommendations.append("检查电感器是否损坏或老化");
                recommendations.append("考虑更换高品质电感器");
            }
        }
        
        result.recommendations = recommendations;
        
    } catch (const std::exception& e) {
        result.setResult(false);
        result.notes = QString("Analysis failed: %1").arg(e.what());
    }
    
    emit diagnosisCompleted(component.reference, result);
    
    return result;
}

// === 配置方法实现 ===

void InductorDiagnostic::setFrequencyRange(double startFreq, double endFreq, int points)
{
    startFrequency_ = qMax(1.0, startFreq);
    endFrequency_ = qMax(startFrequency_ * 10, endFreq);
    frequencyPoints_ = qBound(10, points, 1000);
    
    qInfo() << "Inductor diagnostic frequency range set:" << startFrequency_ << "to" << endFrequency_ << "Hz," << frequencyPoints_ << "points";
}

void InductorDiagnostic::setCurrentRange(double minCurrent, double maxCurrent)
{
    minTestCurrent_ = qMax(1e-6, minCurrent);
    maxTestCurrent_ = qMax(minTestCurrent_ * 10, maxCurrent);
    
    qInfo() << "Inductor diagnostic current range set:" << minTestCurrent_ << "to" << maxTestCurrent_ << "A";
}

void InductorDiagnostic::setSaturationTestParams(bool enable, double maxCurrent, double stepSize)
{
    saturationTestEnabled_ = enable;
    saturationMaxCurrent_ = qMax(0.001, maxCurrent);
    saturationStepSize_ = qMax(0.0001, stepSize);
    
    qInfo() << "Saturation test" << (enable ? "enabled" : "disabled") 
            << "- max current:" << saturationMaxCurrent_ << "A, step:" << saturationStepSize_ << "A";
}

void InductorDiagnostic::setToleranceParams(double tolerance, double qFactorMin)
{
    tolerance_ = qMax(0.1, tolerance);
    minQualityFactor_ = qMax(1.0, qFactorMin);
    
    qInfo() << "Tolerance set to" << tolerance_ << "%, min Q factor:" << minQualityFactor_;
}

// === 私有测量方法实现 ===

double InductorDiagnostic::measureInductance(double frequency, double current)
{
    if (!getDeviceManager()) {
        return -1.0;
    }
    
    try {
        // 设置测量参数
        QMap<QString, QVariant> params;
        params["frequency"] = frequency;
        params["test_current"] = current;
        params["measurement_type"] = "INDUCTANCE";
        
        // 执行测量
        QMap<QString, QVariant> result = getDeviceManager()->measureLCR(params);
        
        if (result.contains("inductance")) {
            double inductance = result["inductance"].toDouble();
            return inductance > 0 ? inductance : -1.0;
        }
        
    } catch (const std::exception& e) {
        qWarning() << "Inductance measurement failed:" << e.what();
    }
    
    return -1.0;
}

QMap<double, double> InductorDiagnostic::measureInductanceSpectrum(const QVector<double>& frequencies, double current)
{
    QMap<double, double> spectrum;
    
    for (int i = 0; i < frequencies.size(); ++i) {
        double freq = frequencies[i];
        double inductance = measureInductance(freq, current);
        
        if (inductance > 0) {
            spectrum[freq] = inductance;
        }
        
        // 更新进度
        if (frequencies.size() > 10) {
            int progress = 25 + (i * 25 / frequencies.size());
            // Progress will be emitted by caller
        }
    }
    
    return spectrum;
}

double InductorDiagnostic::measureQualityFactor(double frequency, double current)
{
    if (!getDeviceManager()) {
        return -1.0;
    }
    
    try {
        QMap<QString, QVariant> params;
        params["frequency"] = frequency;
        params["test_current"] = current;
        params["measurement_type"] = "Q_FACTOR";
        
        QMap<QString, QVariant> result = getDeviceManager()->measureLCR(params);
        
        if (result.contains("q_factor")) {
            double q = result["q_factor"].toDouble();
            return q > 0 ? q : -1.0;
        }
        
    } catch (const std::exception& e) {
        qWarning() << "Q factor measurement failed:" << e.what();
    }
    
    return -1.0;
}

double InductorDiagnostic::measureESR(double frequency, double current)
{
    if (!getDeviceManager()) {
        return -1.0;
    }
    
    try {
        QMap<QString, QVariant> params;
        params["frequency"] = frequency;
        params["test_current"] = current;
        params["measurement_type"] = "ESR";
        
        QMap<QString, QVariant> result = getDeviceManager()->measureLCR(params);
        
        if (result.contains("esr")) {
            double esr = result["esr"].toDouble();
            return esr > 0 ? esr : -1.0;
        }
        
    } catch (const std::exception& e) {
        qWarning() << "ESR measurement failed:" << e.what();
    }
    
    return -1.0;
}

double InductorDiagnostic::detectResonantFrequency()
{
    // 通过扫描频率范围，寻找电感量急剧下降的点
    QVector<double> frequencies = generateFrequencySequence(startFrequency_, endFrequency_, 100, true);
    QMap<double, double> inductanceData;
    
    for (double freq : frequencies) {
        double inductance = measureInductance(freq, minTestCurrent_);
        if (inductance > 0) {
            inductanceData[freq] = inductance;
        }
    }
    
    if (inductanceData.size() < 10) {
        return -1.0; // 数据不足
    }
    
    // 寻找电感量下降最快的频率点
    double maxDerivative = 0;
    double resonantFreq = -1.0;
    
    auto it = inductanceData.begin();
    auto prev = it++;
    
    while (it != inductanceData.end()) {
        double freqDiff = it.key() - prev.key();
        double inductanceDiff = prev.value() - it.value(); // 注意符号
        
        if (freqDiff > 0) {
            double derivative = inductanceDiff / freqDiff;
            if (derivative > maxDerivative) {
                maxDerivative = derivative;
                resonantFreq = it.key();
            }
        }
        
        prev = it++;
    }
    
    return resonantFreq;
}

QMap<double, double> InductorDiagnostic::measureSaturationCharacteristic(double frequency)
{
    QMap<double, double> saturationData;
    
    QVector<double> currents = generateCurrentSequence(minTestCurrent_, saturationMaxCurrent_, saturationStepSize_);
    
    for (double current : currents) {
        double inductance = measureInductance(frequency, current);
        if (inductance > 0) {
            saturationData[current] = inductance;
        }
    }
    
    return saturationData;
}

int InductorDiagnostic::detectOpenShortFault()
{
    if (!getDeviceManager()) {
        return 0; // 无法检测
    }
    
    try {
        // 使用低频率进行基本连通性检查
        QMap<QString, QVariant> params;
        params["frequency"] = 100.0; // 100 Hz
        params["test_current"] = minTestCurrent_;
        params["measurement_type"] = "IMPEDANCE";
        
        QMap<QString, QVariant> result = getDeviceManager()->measureLCR(params);
        
        if (result.contains("impedance")) {
            double impedance = result["impedance"].toDouble();
            
            if (impedance > OPEN_CIRCUIT_THRESHOLD) {
                return 1; // 开路
            } else if (impedance < SHORT_CIRCUIT_THRESHOLD) {
                return 2; // 短路
            }
        }
        
    } catch (const std::exception& e) {
        qWarning() << "Open/short detection failed:" << e.what();
    }
    
    return 0; // 正常
}

// === 分析方法实现 ===

QMap<QString, QVariant> InductorDiagnostic::analyzeInductanceValues(double nominalValue, 
                                                                   const QMap<double, double>& measuredValues)
{
    QMap<QString, QVariant> analysis;
    
    if (measuredValues.isEmpty()) {
        analysis["valid"] = false;
        analysis["error"] = "No measurement data";
        return analysis;
    }
    
    // 统计信息
    QVector<double> values = measuredValues.values().toVector();
    std::sort(values.begin(), values.end());
    
    double minValue = values.first();
    double maxValue = values.last();
    double avgValue = 0;
    for (double value : values) {
        avgValue += value;
    }
    avgValue /= values.size();
    
    analysis["min_inductance"] = minValue;
    analysis["max_inductance"] = maxValue;
    analysis["average_inductance"] = avgValue;
    analysis["measurement_count"] = values.size();
    
    // 频率稳定性分析
    double variation = (maxValue - minValue) / avgValue * 100.0;
    analysis["frequency_variation_percent"] = variation;
    
    if (variation > 50.0) {
        analysis["frequency_stability"] = "POOR";
        analysis["stability_note"] = "电感量随频率变化过大，可能存在寄生效应";
    } else if (variation > 20.0) {
        analysis["frequency_stability"] = "MODERATE";
        analysis["stability_note"] = "电感量随频率有一定变化";
    } else {
        analysis["frequency_stability"] = "GOOD";
        analysis["stability_note"] = "电感量随频率变化较小";
    }
    
    // 与标称值比较
    if (nominalValue > 0) {
        double deviation = qAbs(avgValue - nominalValue) / nominalValue * 100.0;
        analysis["nominal_deviation_percent"] = deviation;
        analysis["nominal_comparison"] = deviation <= tolerance_ ? "WITHIN_TOLERANCE" : "OUT_OF_TOLERANCE";
    }
    
    analysis["valid"] = true;
    return analysis;
}

QMap<QString, QVariant> InductorDiagnostic::analyzeQualityFactor(const QMap<double, double>& qValues)
{
    QMap<QString, QVariant> analysis;
    
    if (qValues.isEmpty()) {
        analysis["valid"] = false;
        return analysis;
    }
    
    QVector<double> values = qValues.values().toVector();
    std::sort(values.begin(), values.end());
    
    double minQ = values.first();
    double maxQ = values.last();
    double avgQ = 0;
    for (double value : values) {
        avgQ += value;
    }
    avgQ /= values.size();
    
    analysis["min_q_factor"] = minQ;
    analysis["max_q_factor"] = maxQ;
    analysis["average_q_factor"] = avgQ;
    
    // Q值等级评估
    if (avgQ >= 100) {
        analysis["quality_grade"] = "EXCELLENT";
    } else if (avgQ >= 50) {
        analysis["quality_grade"] = "GOOD";
    } else if (avgQ >= minQualityFactor_) {
        analysis["quality_grade"] = "ACCEPTABLE";
    } else {
        analysis["quality_grade"] = "POOR";
    }
    
    analysis["meets_minimum"] = avgQ >= minQualityFactor_;
    analysis["valid"] = true;
    
    return analysis;
}

QMap<QString, QVariant> InductorDiagnostic::analyzeFrequencyCharacteristic(const QMap<double, double>& inductanceSpectrum)
{
    QMap<QString, QVariant> analysis;
    
    if (inductanceSpectrum.size() < 3) {
        analysis["valid"] = false;
        return analysis;
    }
    
    // 分析频率特性趋势
    QVector<double> frequencies = inductanceSpectrum.keys().toVector();
    QVector<double> inductances = inductanceSpectrum.values().toVector();
    std::sort(frequencies.begin(), frequencies.end());
    
    // 计算斜率（简单线性回归）
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int n = frequencies.size();
    
    for (int i = 0; i < n; ++i) {
        double x = qLn(frequencies[i]); // 使用对数频率
        double y = qLn(inductances[i]);  // 使用对数电感量
        
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }
    
    double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    analysis["frequency_slope"] = slope;
    
    // 解释斜率
    if (qAbs(slope) < 0.1) {
        analysis["characteristic"] = "FLAT";
        analysis["characteristic_note"] = "电感量随频率变化很小，接近理想电感器";
    } else if (slope < -0.1) {
        analysis["characteristic"] = "DECREASING";
        analysis["characteristic_note"] = "电感量随频率下降，可能存在寄生电容";
    } else {
        analysis["characteristic"] = "INCREASING";
        analysis["characteristic_note"] = "电感量随频率上升，不常见的特性";
    }
    
    analysis["valid"] = true;
    return analysis;
}

QMap<QString, QVariant> InductorDiagnostic::analyzeSaturationCharacteristic(const QMap<double, double>& saturationData)
{
    QMap<QString, QVariant> analysis;
    
    if (saturationData.size() < 3) {
        analysis["valid"] = false;
        return analysis;
    }
    
    QVector<double> currents = saturationData.keys().toVector();
    QVector<double> inductances = saturationData.values().toVector();
    std::sort(currents.begin(), currents.end());
    
    // 找到初始电感量（最小电流时）
    double initialInductance = saturationData[currents.first()];
    
    // 检查是否存在明显的饱和现象
    bool saturated = false;
    double saturationCurrent = -1.0;
    
    for (int i = 1; i < currents.size(); ++i) {
        double current = currents[i];
        double inductance = saturationData[current];
        
        // 如果电感量下降超过阈值，认为发生饱和
        double decrease = (initialInductance - inductance) / initialInductance * 100.0;
        if (decrease > SATURATION_THRESHOLD) {
            saturated = true;
            saturationCurrent = current;
            break;
        }
    }
    
    analysis["saturated"] = saturated;
    analysis["initial_inductance"] = initialInductance;
    
    if (saturated) {
        analysis["saturation_current"] = saturationCurrent;
        analysis["saturation_note"] = QString("在电流%.3fA时检测到饱和").arg(saturationCurrent);
    } else {
        analysis["saturation_note"] = "在测试电流范围内未检测到明显饱和";
    }
    
    analysis["valid"] = true;
    return analysis;
}

// === 辅助方法实现 ===

QVector<double> InductorDiagnostic::generateFrequencySequence(double startFreq, double endFreq, int points, bool logScale)
{
    QVector<double> frequencies;
    frequencies.reserve(points);
    
    if (logScale) {
        double logStart = qLn(startFreq);
        double logEnd = qLn(endFreq);
        double logStep = (logEnd - logStart) / (points - 1);
        
        for (int i = 0; i < points; ++i) {
            double logFreq = logStart + i * logStep;
            frequencies.append(qExp(logFreq));
        }
    } else {
        double step = (endFreq - startFreq) / (points - 1);
        for (int i = 0; i < points; ++i) {
            frequencies.append(startFreq + i * step);
        }
    }
    
    return frequencies;
}

QVector<double> InductorDiagnostic::generateCurrentSequence(double minCurrent, double maxCurrent, double stepSize)
{
    QVector<double> currents;
    
    double current = minCurrent;
    while (current <= maxCurrent) {
        currents.append(current);
        current += stepSize;
    }
    
    return currents;
}

double InductorDiagnostic::calculateResonantFrequency(double inductance, double parasticCapacitance)
{
    if (inductance <= 0 || parasticCapacitance <= 0) {
        return -1.0;
    }
    
    return 1.0 / (2.0 * M_PI * qSqrt(inductance * parasticCapacitance));
}

double InductorDiagnostic::estimateParasiticCapacitance(const QMap<double, double>& inductanceSpectrum)
{
    // 简化的寄生电容估算
    // 基于电感量随频率变化的趋势
    
    if (inductanceSpectrum.size() < 10) {
        return -1.0;
    }
    
    QVector<double> frequencies = inductanceSpectrum.keys().toVector();
    QVector<double> inductances = inductanceSpectrum.values().toVector();
    std::sort(frequencies.begin(), frequencies.end());
    
    // 寻找电感量开始显著下降的频率点
    double nominalL = inductances.first(); // 使用最低频率的电感量作为标称值
    
    for (int i = 1; i < frequencies.size(); ++i) {
        double freq = frequencies[i];
        double inductance = inductanceSpectrum[freq];
        
        if (inductance < nominalL * 0.7) { // 电感量下降到70%
            // 估算寄生电容
            return 1.0 / (4.0 * M_PI * M_PI * freq * freq * nominalL);
        }
    }
    
    return -1.0; // 未检测到明显的谐振特征
}
