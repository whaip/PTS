// 故障分析模块实现
#include "faultanalysis.h"
#include <QDebug>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QtMath>
#include <numeric>

// 异步分析工作线程
class FaultAnalysis::AnalysisWorker : public QThread
{
    Q_OBJECT
    
public:
    AnalysisWorker(FaultAnalysis* parent, const TestConfiguration& config,
                   const QVector<MeasurementData>& data,
                   const QMap<QString, QVariant>& specs)
        : QThread(parent)
        , faultAnalysis_(parent)
        , config_(config)
        , measurementData_(data)
        , componentSpecs_(specs)
    {}
    
protected:
    void run() override {
        try {
            QString componentType = faultAnalysis_->determineComponentType(config_, componentSpecs_);
            
            if (faultAnalysis_->algorithms_.contains(componentType)) {
                AnalysisAlgorithm* algorithm = faultAnalysis_->algorithms_[componentType];                
                AnalysisResult result = algorithm->analyze(config_, measurementData_, componentSpecs_);
                result.testId = config_.testName;
                result.componentType = componentType;
                result.componentReference = config_.componentReference;
                result.testConfig = config_;
                
                // 转换 MeasurementData 为 QMap<QString,QVariant>
                for (const MeasurementData& data : measurementData_) {
                    QMap<QString, QVariant> dataMap;
                    dataMap["signal_name"] = data.signalName;
                    dataMap["value"] = data.value;
                    dataMap["voltage"] = data.voltage;
                    dataMap["current"] = data.current;
                    dataMap["power"] = data.power;
                    dataMap["frequency"] = data.frequency;
                    dataMap["phase"] = data.phase;
                    dataMap["esr"] = data.esr;
                    dataMap["leakage_current"] = data.leakageCurrent;
                    dataMap["valid"] = data.isValid;
                    result.measurementData.append(dataMap);
                }
                
                emit analysisCompleted(result);
            } else {
                emit analysisFailed(QString("未找到组件类型 %1 的分析算法").arg(componentType));
            }
        } catch (const std::exception& e) {
            emit analysisFailed(QString("分析过程中发生异常: %1").arg(e.what()));
        }
    }
    
signals:
    void analysisCompleted(const AnalysisResult& result);
    void analysisFailed(const QString& error);

private:
    FaultAnalysis* faultAnalysis_;
    TestConfiguration config_;
    QVector<MeasurementData> measurementData_;
    QMap<QString, QVariant> componentSpecs_;
};

// === FaultAnalysis主类实现 ===

FaultAnalysis::FaultAnalysis(QObject *parent)
    : QObject(parent)
    , currentWorker_(nullptr)
    , workerThread_(nullptr)
{
    initializeBuiltinAlgorithms();
}

// 同步分析方法实现
AnalysisResult FaultAnalysis::analyzeSynchronously(const QString& testId, const TestData& testData)
{
    qDebug() << "开始同步分析:" << testId;
    AnalysisResult result;
    
    // try {
    //     // 将TestData转换为内部数据格式
        QMap<QString, QVector<double>> rawResults = convertTestDataToRawResults(testData);
    //     QMap<QString, QVariant> componentSpecs = extractComponentSpecs(testData);
        
    //     // 创建临时配置
    //     TestConfiguration config;
    //     config.testId = testId;
    //     config.timeout = 30000;
    //     config.enableSynchronization = false;
        
    //     // 处理原始数据
    //     QVector<MeasurementData> measurementData = processRawData(config, rawResults);
        
    //     // 确定组件类型
    //     QString componentType = determineComponentType(config, componentSpecs);
        
    //     // 执行分析
    //     if (algorithms_.contains(componentType)) {
    //         AnalysisAlgorithm* algorithm = algorithms_[componentType];
    //         AnalysisResult result = algorithm->analyze(config, measurementData, componentSpecs);
            
    //         // 设置结果元数据
    //         result.testId = testId;
    //         result.componentType = componentType;
    //         result.timestamp = testData.timestamp;
            
    //         qDebug() << "同步分析完成:" << testId << "健康度:" << result.healthScore;
    //         return result;
    //     } else {
    //         return createErrorResult(QString("未找到组件类型 %1 的分析算法").arg(componentType), testId);
    //     }
    // }
    // catch (const std::exception& e) {
    //     return createErrorResult(QString("同步分析异常: %1").arg(e.what()), testId);
    // }
    return result;
}

// 异步分析方法实现（使用TestData）
void FaultAnalysis::startAnalysisAsync(const QString& testId, const TestData& testData)
{
    qDebug() << "启动异步分析:" << testId;
    
    emit analysisStarted(testId);
    
    // 在单独的线程中执行分析
    QThread* workerThread = new QThread(this);
    
    // 创建lambda函数来执行分析
    QTimer::singleShot(0, [this, testId, testData, workerThread]() {
        workerThread->start();
        
        // 在工作线程中执行分析
        QMetaObject::invokeMethod(workerThread, [this, testId, testData]() {
            AnalysisResult result = analyzeSynchronously(testId, testData);
            
            // 回到主线程发射信号
            QMetaObject::invokeMethod(this, [this, testId, result]() {
                emit analysisCompleted(testId, result);
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    });
    
    // 清理线程
    connect(workerThread, &QThread::finished, workerThread, &QThread::deleteLater);
}

// 数据转换辅助方法
QMap<QString, QVector<double>> FaultAnalysis::convertTestDataToRawResults(const TestData& testData)
{
    QMap<QString, QVector<double>> rawResults;
    
    for (const auto& measurement : testData.measurements) {
        for (auto it = measurement.begin(); it != measurement.end(); ++it) {
            const QString& key = it.key();
            const QVariant& value = it.value();
            
            // 尝试转换为数值数据
            bool ok;
            double numValue = value.toDouble(&ok);
            if (ok) {
                if (!rawResults.contains(key)) {
                    rawResults[key] = QVector<double>();
                }
                rawResults[key].append(numValue);
            }
        }
    }
    
    return rawResults;
}

QMap<QString, DeviceOperation> FaultAnalysis::extractComponentSpecs(const TestData& testData)
{
    QMap<QString, DeviceOperation> specs = testData.metadata;
    return specs;
}

//=============================================================================
// AnalysisAlgorithm 基类实现
//=============================================================================

double AnalysisAlgorithm::calculateMean(const QVector<double>& data)
{
    if (data.isEmpty()) return 0.0;
    
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / data.size();
}

double AnalysisAlgorithm::calculateRMS(const QVector<double>& data)
{
    if (data.isEmpty()) return 0.0;
    
    double sumSquares = 0.0;
    for (double value : data) {
        sumSquares += value * value;
    }
    
    return qSqrt(sumSquares / data.size());
}

double AnalysisAlgorithm::calculateStdDev(const QVector<double>& data)
{
    if (data.size() < 2) return 0.0;
    
    double mean = calculateMean(data);
    double sumSquareDiffs = 0.0;
    
    for (double value : data) {
        double diff = value - mean;
        sumSquareDiffs += diff * diff;
    }
    
    return qSqrt(sumSquareDiffs / (data.size() - 1));
}

double AnalysisAlgorithm::calculatePeak(const QVector<double>& data)
{
    if (data.isEmpty()) return 0.0;
    
    auto minMax = std::minmax_element(data.begin(), data.end());
    return qMax(qAbs(*minMax.first), qAbs(*minMax.second));
}

bool AnalysisAlgorithm::isWithinTolerance(double value, double expected, double tolerance)
{
    double allowedDeviation = expected * tolerance;
    return qAbs(value - expected) <= allowedDeviation;
}

double AnalysisAlgorithm::calculateDeviation(double value, double expected)
{
    if (expected == 0.0) return qAbs(value);
    
    // 对于极端值（如开路），计算相对偏差
    double deviation = qAbs(value - expected) / qAbs(expected);
    
    // 开路检测：如果测量值超过100MΩ，认为是开路
    if (value > 1e8) {
        return 1000.0;  // 返回一个很大的偏差值表示开路
    }
    
    return deviation;
}

double AnalysisAlgorithm::calculateHealthScore(double deviation, double tolerance)
{
    if (deviation <= tolerance * 0.1) return 100.0;  // 优秀
    if (deviation <= tolerance * 0.5) return 90.0;   // 良好
    if (deviation <= tolerance * 0.8) return 75.0;   // 一般
    if (deviation <= tolerance) return 60.0;         // 合格
    if (deviation <= tolerance * 1.5) return 40.0;   // 边缘
    return 20.0;  // 不合格
}

//=============================================================================
// ResistorAnalysisAlgorithm 实现
//=============================================================================

ResistorAnalysisAlgorithm::ResistorAnalysisAlgorithm(QObject *parent)
    : AnalysisAlgorithm(parent)
{
}

QStringList ResistorAnalysisAlgorithm::getSupportedSignalTypes() const
{
    return QStringList() << "VOLTAGE_DC" << "CURRENT_DC" << "RESISTANCE";
}

AnalysisResult ResistorAnalysisAlgorithm::analyze(const TestConfiguration& config,
                                                 const QVector<MeasurementData>& data,
                                                 const QMap<QString, QVariant>& componentSpecs)
{
    AnalysisResult result;
    result.timestamp = QDateTime::currentDateTime();
    result.testScheme = "Resistor_Analysis";
    result.units = "Ω";
    
    // 获取期望值和容差
    double expectedResistance = componentSpecs.value("nominal_value", 1000.0).toDouble();
    double tolerance = componentSpecs.value("tolerance", 0.05).toDouble();
    
    result.expectedValue = expectedResistance;
    result.tolerance = tolerance;
    
    // 查找测量数据
    const MeasurementData* voltageData = nullptr;
    const MeasurementData* currentData = nullptr;
    const MeasurementData* resistanceData = nullptr;
    
    for (const MeasurementData& measurement : data) {
        if (measurement.signalType == SignalType::VOLTAGE_DC) {
            voltageData = &measurement;
        } else if (measurement.signalType == SignalType::CURRENT_DC) {
            currentData = &measurement;
        } else if (measurement.signalType == SignalType::RESISTANCE) {
            resistanceData = &measurement;
        }
    }
    
    double measuredResistance = 0.0;
    QString calculationMethod;
    
    // 优先使用直接电阻测量
    if (resistanceData && resistanceData->isValid) {
        measuredResistance = calculateResistanceFromDirect(*resistanceData);
        calculationMethod = "直接测量";
        result.detailedResults["direct_measurement"] = measuredResistance;
    }
    // 使用电压电流计算
    else if (voltageData && currentData && voltageData->isValid && currentData->isValid) {
        measuredResistance = calculateResistanceFromVI(voltageData->mean, currentData->mean);
        calculationMethod = "欧姆定律计算";
        result.detailedResults["voltage_measurement"] = voltageData->mean;
        result.detailedResults["current_measurement"] = currentData->mean;
        result.detailedResults["calculated_resistance"] = measuredResistance;
    } else {
        result.result = AnalysisResult::ERROR;
        result.notes.append("缺少有效的测量数据");
        result.confidence = 0.0;
        return result;
    }
    
    result.calculatedValue = measuredResistance;
    result.deviation = calculateDeviation(measuredResistance, expectedResistance);    // 故障检测（增强版）
    QStringList faults;
    
    // 检测开路（异常高电阻）
    if (measuredResistance > 1e8) {  // 大于100MΩ认为是开路
        faults << "OPEN_CIRCUIT";
        result.notes.append(QString("检测到开路故障：测量电阻值 %1 Ω 远超正常范围").arg(measuredResistance, 0, 'e', 2));
        result.notes.append("可能原因：元件断路、焊点开路、连接不良");
        result.result = AnalysisResult::FAIL;
        result.healthScore = 0.0;  // 开路时健康度为0
        result.faultTypes = faults;
        result.confidence = 95.0;  // 开路检测置信度很高
    }    // 检测短路
    else if (checkForShortCircuit(measuredResistance, expectedResistance)) {
        faults << "SHORT_CIRCUIT";
        result.result = AnalysisResult::FAIL;
        result.healthScore = 10.0;  // 短路时健康度很低
    }
    // 检测容差
    else if (!isWithinTolerance(measuredResistance, expectedResistance, tolerance)) {
        faults << "OUT_OF_TOLERANCE";
        result.result = AnalysisResult::FAIL;
        result.healthScore = calculateHealthScore(result.deviation, tolerance);
        result.notes.append(QString("电阻值超出容差范围：偏差 %1%").arg(result.deviation * 100, 0, 'f', 2));
    }
    // 正常情况
    else {
        result.result = AnalysisResult::PASS;
        result.healthScore = calculateHealthScore(result.deviation, tolerance);
        result.notes.append("电阻值在正常范围内");
    }
    
    result.faultTypes = faults;
    result.confidence = (result.result == AnalysisResult::PASS) ? 95.0 : 90.0;
    
    // 生成分析报告
    result.notes.append(QString("电阻分析结果 (%1):").arg(calculationMethod));
    result.notes.append(QString("测量值: %1Ω").arg(measuredResistance, 0, 'f', 3));
    result.notes.append(QString("期望值: %1Ω").arg(expectedResistance, 0, 'f', 3));
    result.notes.append(QString("偏差: %1%").arg(result.deviation * 100, 0, 'f', 2));
    result.notes.append(QString("容差: ±%1%").arg(tolerance * 100, 0, 'f', 1));
    
    if (!faults.isEmpty()) {
        result.notes.append(QString("检测到故障: %1").arg(faults.join(", ")));
    }
    
    return result;
}

double ResistorAnalysisAlgorithm::calculateResistanceFromVI(double voltage, double current)
{
    if (qAbs(current) < 1e-9) {  // 避免除零
        return 1e9;  // 返回一个很大的值表示开路
    }
    return qAbs(voltage / current);
}

double ResistorAnalysisAlgorithm::calculateResistanceFromDirect(const MeasurementData& resistanceData)
{
    return qAbs(resistanceData.mean);
}

bool ResistorAnalysisAlgorithm::checkForShortCircuit(double resistance, double expectedResistance)
{
    return resistance < expectedResistance * 0.01;  // 小于期望值的1%
}

bool ResistorAnalysisAlgorithm::checkForOpenCircuit(double resistance, double maxResistance)
{
    return resistance > maxResistance;
}

AnalysisResult::TestResult ResistorAnalysisAlgorithm::evaluateResistance(double measured, double expected, double tolerance)
{
    if (isWithinTolerance(measured, expected, tolerance)) {
        return AnalysisResult::PASS;
    } else {
        return AnalysisResult::FAIL;
    }
}

//=============================================================================
// CapacitorAnalysisAlgorithm 实现
//=============================================================================

CapacitorAnalysisAlgorithm::CapacitorAnalysisAlgorithm(QObject *parent)
    : AnalysisAlgorithm(parent)
{
}

QStringList CapacitorAnalysisAlgorithm::getSupportedSignalTypes() const
{
    return QStringList() << "VOLTAGE_AC" << "CURRENT_AC" << "CAPACITANCE" << "CURRENT_DC";
}

AnalysisResult CapacitorAnalysisAlgorithm::analyze(const TestConfiguration& config,
                                                  const QVector<MeasurementData>& data,
                                                  const QMap<QString, QVariant>& componentSpecs)
{
    AnalysisResult result;
    result.timestamp = QDateTime::currentDateTime();
    result.testScheme = "Capacitor_Analysis";
    result.units = "F";
    
    // 获取规格参数
    double expectedCapacitance = componentSpecs.value("nominal_value", 1e-6).toDouble();
    double tolerance = componentSpecs.value("tolerance", 0.2).toDouble();
    double maxESR = componentSpecs.value("max_esr", 1.0).toDouble();
    double maxLeakage = componentSpecs.value("max_leakage", 1e-6).toDouble();
    
    result.expectedValue = expectedCapacitance;
    result.tolerance = tolerance;
    
    // 查找测量数据
    const MeasurementData* capacitanceData = nullptr;
    const MeasurementData* esrData = nullptr;
    const MeasurementData* leakageData = nullptr;
    
    for (const MeasurementData& measurement : data) {
        if (measurement.signalType == SignalType::CAPACITANCE) {
            capacitanceData = &measurement;
        } else if (measurement.signalName.contains("ESR")) {
            esrData = &measurement;
        } else if (measurement.signalName.contains("Leakage") && 
                   measurement.signalType == SignalType::CURRENT_DC) {
            leakageData = &measurement;
        }
    }
      if (!capacitanceData || !capacitanceData->isValid) {
        result.result = AnalysisResult::ERROR;
        result.notes.append("缺少有效的电容测量数据");
        return result;
    }
    
    double measuredCapacitance = qAbs(capacitanceData->mean);
    double measuredESR = esrData ? calculateESR(*esrData) : 0.0;
    double measuredLeakage = leakageData ? calculateLeakageCurrent(*leakageData) : 0.0;
    
    result.calculatedValue = measuredCapacitance;
    result.deviation = calculateDeviation(measuredCapacitance, expectedCapacitance);
    
    // 详细结果
    result.detailedResults["capacitance"] = measuredCapacitance;
    result.detailedResults["esr"] = measuredESR;
    result.detailedResults["leakage_current"] = measuredLeakage;
    
    // 故障检测
    QStringList faults;
    
    if (!isWithinTolerance(measuredCapacitance, expectedCapacitance, tolerance)) {
        faults << "OUT_OF_TOLERANCE";
    }
    
    if (esrData && !checkESRLimit(measuredESR, maxESR)) {
        faults << "HIGH_ESR";
    }
    
    if (leakageData && !checkLeakageLimit(measuredLeakage, maxLeakage)) {
        faults << "HIGH_LEAKAGE";
    }
    
    if (measuredCapacitance < expectedCapacitance * 0.01) {
        faults << "SHORT_CIRCUIT";
    }
    
    if (measuredCapacitance < expectedCapacitance * 0.1) {
        faults << "OPEN_CIRCUIT";
    }
    
    // 评估结果
    result.result = evaluateCapacitor(measuredCapacitance, measuredESR, measuredLeakage, componentSpecs);
    result.faultTypes = faults;
    result.healthScore = calculateHealthScore(result.deviation, tolerance);
    result.confidence = 90.0;
      // 生成报告
    result.notes.append("电容分析结果:");
    result.notes.append(QString("电容值: %1µF").arg(measuredCapacitance * 1e6, 0, 'f', 3));
    result.notes.append(QString("期望值: %1µF").arg(expectedCapacitance * 1e6, 0, 'f', 3));    if (esrData) {
        result.notes.append(QString("ESR: %1Ω").arg(measuredESR, 0, 'f', 3));
    }
    if (leakageData) {
        result.notes.append(QString("漏电流: %1µA").arg(measuredLeakage * 1e6, 0, 'f', 3));
    }
    
    return result;
}

double CapacitorAnalysisAlgorithm::calculateCapacitanceFromImpedance(double impedance, double frequency)
{
    if (frequency <= 0.0 || impedance <= 0.0) return 0.0;
    return 1.0 / (2.0 * M_PI * frequency * impedance);
}

double CapacitorAnalysisAlgorithm::calculateESR(const MeasurementData& esrData)
{
    return qAbs(esrData.mean);
}

double CapacitorAnalysisAlgorithm::calculateLeakageCurrent(const MeasurementData& currentData)
{
    return qAbs(currentData.mean);
}

bool CapacitorAnalysisAlgorithm::checkESRLimit(double esr, double maxESR)
{
    return esr <= maxESR;
}

bool CapacitorAnalysisAlgorithm::checkLeakageLimit(double leakage, double maxLeakage)
{
    return leakage <= maxLeakage;
}

AnalysisResult::TestResult CapacitorAnalysisAlgorithm::evaluateCapacitor(double capacitance, double esr, double leakage,
                                                                        const QMap<QString, QVariant>& specs)
{
    double expectedCapacitance = specs.value("nominal_value").toDouble();
    double tolerance = specs.value("tolerance").toDouble();
    double maxESR = specs.value("max_esr", 999.0).toDouble();
    double maxLeakage = specs.value("max_leakage", 999.0).toDouble();
    
    if (!isWithinTolerance(capacitance, expectedCapacitance, tolerance)) {
        return AnalysisResult::FAIL;
    }
    
    if (esr > maxESR || leakage > maxLeakage) {
        return AnalysisResult::FAIL;
    }
      return AnalysisResult::PASS;
}

//=============================================================================
// InductorAnalysisAlgorithm 实现
//=============================================================================

InductorAnalysisAlgorithm::InductorAnalysisAlgorithm(QObject *parent)
    : AnalysisAlgorithm(parent)
{
}

QStringList InductorAnalysisAlgorithm::getSupportedSignalTypes() const
{
    return QStringList() << "VOLTAGE_AC" << "CURRENT_AC" << "INDUCTANCE" << "RESISTANCE";
}

AnalysisResult InductorAnalysisAlgorithm::analyze(const TestConfiguration& config,
                                                 const QVector<MeasurementData>& data,
                                                 const QMap<QString, QVariant>& componentSpecs)
{
    AnalysisResult result;
    result.timestamp = QDateTime::currentDateTime();
    result.testScheme = "Inductor_Analysis";
    result.units = "H";
    
    // 获取规格参数
    double expectedInductance = componentSpecs.value("nominal_value", 1e-3).toDouble();
    double tolerance = componentSpecs.value("tolerance", 0.2).toDouble();
    double maxDCR = componentSpecs.value("max_dcr", 10.0).toDouble();
    
    result.expectedValue = expectedInductance;
    result.tolerance = tolerance;
    
    // 查找测量数据
    const MeasurementData* inductanceData = nullptr;
    const MeasurementData* dcrData = nullptr;
    
    for (const MeasurementData& measurement : data) {
        if (measurement.signalType == SignalType::INDUCTANCE) {
            inductanceData = &measurement;
        } else if (measurement.signalName.contains("DCR") || 
                   (measurement.signalType == SignalType::RESISTANCE)) {
            dcrData = &measurement;
        }
    }
    
    if (!inductanceData || !inductanceData->isValid) {
        result.result = AnalysisResult::ERROR;
        result.notes.append("缺少有效的电感测量数据");
        return result;
    }
    
    double measuredInductance = qAbs(inductanceData->mean);
    double measuredDCR = dcrData ? calculateDCR(*dcrData) : 0.0;
    double qFactor = calculateQFactor(measuredInductance, measuredDCR, 1000.0); // 1kHz
    
    result.calculatedValue = measuredInductance;
    result.deviation = calculateDeviation(measuredInductance, expectedInductance);
    
    // 详细结果
    result.detailedResults["inductance"] = measuredInductance;
    result.detailedResults["dcr"] = measuredDCR;
    result.detailedResults["q_factor"] = qFactor;
    
    // 故障检测
    QStringList faults;
    
    if (!isWithinTolerance(measuredInductance, expectedInductance, tolerance)) {
        faults << "OUT_OF_TOLERANCE";
    }
    
    if (dcrData && measuredDCR > maxDCR) {
        faults << "HIGH_DCR";
    }
    
    // 评估结果
    result.result = evaluateInductor(measuredInductance, measuredDCR, qFactor, componentSpecs);
    result.faultTypes = faults;
    result.healthScore = calculateHealthScore(result.deviation, tolerance);
    result.confidence = 85.0;
    
    // 生成报告
    result.notes.append("电感分析结果:");
    result.notes.append(QString("电感值: %1mH").arg(measuredInductance * 1e3, 0, 'f', 3));
    result.notes.append(QString("期望值: %1mH").arg(expectedInductance * 1e3, 0, 'f', 3));
    if (dcrData) {
        result.notes.append(QString("直流电阻: %1Ω").arg(measuredDCR, 0, 'f', 3));
        result.notes.append(QString("品质因数: %1").arg(qFactor, 0, 'f', 1));
    }
    
    return result;
}

double InductorAnalysisAlgorithm::calculateInductanceFromImpedance(double impedance, double frequency)
{
    if (frequency <= 0.0 || impedance <= 0.0) return 0.0;
    return impedance / (2.0 * M_PI * frequency);
}

double InductorAnalysisAlgorithm::calculateDCR(const MeasurementData& resistanceData)
{
    return qAbs(resistanceData.mean);
}

double InductorAnalysisAlgorithm::calculateQFactor(double inductance, double dcr, double frequency)
{
    if (dcr <= 0.0 || inductance <= 0.0) return 0.0;
    double reactance = 2.0 * M_PI * frequency * inductance;
    return reactance / dcr;
}

AnalysisResult::TestResult InductorAnalysisAlgorithm::evaluateInductor(double inductance, double dcr, double qFactor,
                                                                      const QMap<QString, QVariant>& specs)
{
    double expectedInductance = specs.value("nominal_value").toDouble();
    double tolerance = specs.value("tolerance").toDouble();
    double maxDCR = specs.value("max_dcr", 999.0).toDouble();
    
    if (!isWithinTolerance(inductance, expectedInductance, tolerance)) {
        return AnalysisResult::FAIL;
    }
    
    if (dcr > maxDCR) {
        return AnalysisResult::FAIL;
    }
    
    return AnalysisResult::PASS;
}

//=============================================================================
// DiodeAnalysisAlgorithm 实现
//=============================================================================

DiodeAnalysisAlgorithm::DiodeAnalysisAlgorithm(QObject *parent)
    : AnalysisAlgorithm(parent)
{
}

QStringList DiodeAnalysisAlgorithm::getSupportedSignalTypes() const
{
    return QStringList() << "VOLTAGE_DC" << "CURRENT_DC";
}

AnalysisResult DiodeAnalysisAlgorithm::analyze(const TestConfiguration& config,
                                              const QVector<MeasurementData>& data,
                                              const QMap<QString, QVariant>& componentSpecs)
{
    AnalysisResult result;
    result.timestamp = QDateTime::currentDateTime();
    result.testScheme = "Diode_Analysis";
    result.units = "V";
    
    // 获取规格参数
    double minForwardVoltage = componentSpecs.value("min_vf", 0.3).toDouble();
    double maxForwardVoltage = componentSpecs.value("max_vf", 0.7).toDouble();
    double maxReverseLeakage = componentSpecs.value("max_leakage", 1e-6).toDouble();
    double testCurrent = componentSpecs.value("test_current", 0.01).toDouble();
    
    result.expectedValue = (minForwardVoltage + maxForwardVoltage) / 2.0;
    result.tolerance = (maxForwardVoltage - minForwardVoltage) / result.expectedValue;
    
    // 查找测量数据
    const MeasurementData* forwardVoltageData = nullptr;
    const MeasurementData* reverseCurrentData = nullptr;
    
    for (const MeasurementData& measurement : data) {
        if (measurement.signalName.contains("Forward") && 
            measurement.signalType == SignalType::VOLTAGE_DC) {
            forwardVoltageData = &measurement;
        } else if (measurement.signalName.contains("Reverse") && 
                   measurement.signalType == SignalType::CURRENT_DC) {
            reverseCurrentData = &measurement;
        }
    }
    
    if (!forwardVoltageData || !forwardVoltageData->isValid) {
        result.result = AnalysisResult::ERROR;
        result.notes.append("缺少有效的正向电压测量数据");
        return result;
    }
    
    double forwardVoltage = calculateForwardVoltage(*forwardVoltageData, testCurrent);
    double reverseLeakage = reverseCurrentData ? calculateReverseLeakage(*reverseCurrentData) : 0.0;
    
    result.calculatedValue = forwardVoltage;
    result.deviation = calculateDeviation(forwardVoltage, result.expectedValue);
    
    // 详细结果
    result.detailedResults["forward_voltage"] = forwardVoltage;
    result.detailedResults["reverse_leakage"] = reverseLeakage;
    
    // 故障检测
    QStringList faults;
    
    if (!checkForwardVoltageRange(forwardVoltage, minForwardVoltage, maxForwardVoltage)) {
        faults << "OUT_OF_TOLERANCE";
    }
    
    if (reverseCurrentData && !checkReverseLeakageLimit(reverseLeakage, maxReverseLeakage)) {
        faults << "HIGH_LEAKAGE";
    }
    
    if (forwardVoltage < 0.1) {
        faults << "DIODE_SHORT";
    }
    
    if (forwardVoltage > 2.0) {
        faults << "DIODE_OPEN";
    }
    
    // 评估结果
    result.result = evaluateDiode(forwardVoltage, reverseLeakage, componentSpecs);
    result.faultTypes = faults;
    result.healthScore = calculateHealthScore(result.deviation, result.tolerance);
    result.confidence = 90.0;
    
    // 生成报告
    result.notes.append("二极管分析结果:");
    result.notes.append(QString("正向电压: %1V").arg(forwardVoltage, 0, 'f', 3));
    result.notes.append(QString("期望范围: %1V - %2V").arg(minForwardVoltage, 0, 'f', 3).arg(maxForwardVoltage, 0, 'f', 3));
    if (reverseCurrentData) {
        result.notes.append(QString("反向漏电流: %1µA").arg(reverseLeakage * 1e6, 0, 'f', 3));
    }
    
    return result;
}

double DiodeAnalysisAlgorithm::calculateForwardVoltage(const MeasurementData& voltageData, double current)
{
    return qAbs(voltageData.mean);
}

double DiodeAnalysisAlgorithm::calculateReverseLeakage(const MeasurementData& currentData)
{
    return qAbs(currentData.mean);
}

bool DiodeAnalysisAlgorithm::checkForwardVoltageRange(double vf, double minVf, double maxVf)
{
    return (vf >= minVf && vf <= maxVf);
}

bool DiodeAnalysisAlgorithm::checkReverseLeakageLimit(double leakage, double maxLeakage)
{
    return leakage <= maxLeakage;
}

AnalysisResult::TestResult DiodeAnalysisAlgorithm::evaluateDiode(double forwardVoltage, double reverseLeakage,
                                                               const QMap<QString, QVariant>& specs)
{
    double minVf = specs.value("min_vf", 0.3).toDouble();
    double maxVf = specs.value("max_vf", 0.7).toDouble();
    double maxLeakage = specs.value("max_leakage", 1e-6).toDouble();
    
    if (!checkForwardVoltageRange(forwardVoltage, minVf, maxVf)) {
        return AnalysisResult::FAIL;
    }
    
    if (reverseLeakage > maxLeakage) {
        return AnalysisResult::FAIL;
    }
    
    return AnalysisResult::PASS;
}

//=============================================================================
// ICAnalysisAlgorithm 实现
//=============================================================================

ICAnalysisAlgorithm::ICAnalysisAlgorithm(QObject *parent)
    : AnalysisAlgorithm(parent)
{
}

QStringList ICAnalysisAlgorithm::getSupportedSignalTypes() const
{    return QStringList() << "VOLTAGE_DC" << "CURRENT_DC" << "DIGITAL_INPUT" << "DIGITAL_OUTPUT";
}

AnalysisResult ICAnalysisAlgorithm::analyze(const TestConfiguration& config,
                                           const QVector<MeasurementData>& data,
                                           const QMap<QString, QVariant>& componentSpecs)
{
    AnalysisResult result;
    result.timestamp = QDateTime::currentDateTime();
    result.testScheme = "IC_Analysis";
    result.units = "A";
    
    // 获取规格参数
    double maxSupplyCurrent = componentSpecs.value("max_supply_current", 0.1).toDouble();
    
    result.expectedValue = maxSupplyCurrent / 2.0; // 期望在最大值的一半左右
    result.tolerance = 0.5; // 50%容差
    
    // 查找测量数据
    const MeasurementData* supplyCurrentData = nullptr;
    QVector<const MeasurementData*> digitalData;
    
    for (const MeasurementData& measurement : data) {
        if (measurement.signalName.contains("Supply") && 
            measurement.signalType == SignalType::CURRENT_DC) {
            supplyCurrentData = &measurement;
        } else if (measurement.signalType == SignalType::DIGITAL_INPUT ||
                   measurement.signalType == SignalType::DIGITAL_OUTPUT) {
            digitalData.append(&measurement);
        }
    }
    
    if (!supplyCurrentData || !supplyCurrentData->isValid) {
        result.result = AnalysisResult::ERROR;
        result.notes.append("缺少有效的供电电流测量数据");
        return result;
    }
      double supplyCurrent = calculateSupplyCurrent(*supplyCurrentData);
    
    // 将指针数组转换为值数组
    QVector<MeasurementData> digitalDataValues;
    for (const MeasurementData* data : digitalData) {
        digitalDataValues.append(*data);
    }
    bool logicOK = checkLogicLevels(digitalDataValues);
    
    result.calculatedValue = supplyCurrent;
    result.deviation = calculateDeviation(supplyCurrent, result.expectedValue);
    
    // 详细结果
    result.detailedResults["supply_current"] = supplyCurrent;
    result.detailedResults["logic_levels_ok"] = logicOK;
    
    // 故障检测
    QStringList faults;
    
    if (!checkSupplyCurrentLimit(supplyCurrent, maxSupplyCurrent)) {
        faults << "IC_OVERCURRENT";
    }
    
    if (!logicOK) {
        faults << "IC_LOGIC_ERROR";
    }
    
    // 评估结果
    result.result = evaluateIC(supplyCurrent, logicOK, componentSpecs);
    result.faultTypes = faults;
    result.healthScore = calculateHealthScore(result.deviation, result.tolerance);
    result.confidence = 80.0;
    
    // 生成报告
    result.notes.append("IC分析结果:");
    result.notes.append(QString("供电电流: %1mA").arg(supplyCurrent * 1e3, 0, 'f', 3));
    result.notes.append(QString("最大允许: %1mA").arg(maxSupplyCurrent * 1e3, 0, 'f', 3));
    result.notes.append(QString("逻辑电平: %1").arg(logicOK ? "正常" : "异常"));
    
    return result;
}

double ICAnalysisAlgorithm::calculateSupplyCurrent(const MeasurementData& currentData)
{
    return qAbs(currentData.mean);
}

bool ICAnalysisAlgorithm::checkSupplyCurrentLimit(double current, double maxCurrent)
{
    return current <= maxCurrent;
}

bool ICAnalysisAlgorithm::checkLogicLevels(const QVector<MeasurementData>& digitalData)
{
    // 简化的逻辑电平检查
    for (const MeasurementData& data : digitalData) {
        if (!data.isValid) {
            return false;
        }
        
        // 检查数字信号是否在合理范围内 (0-5V)
        if (data.mean < 0.0 || data.mean > 5.5) {
            return false;
        }
    }
    
    return true;
}

AnalysisResult::TestResult ICAnalysisAlgorithm::evaluateIC(double supplyCurrent, bool logicOK,
                                                          const QMap<QString, QVariant>& specs)
{
    double maxCurrent = specs.value("max_supply_current", 0.1).toDouble();
    
    if (!checkSupplyCurrentLimit(supplyCurrent, maxCurrent)) {
        return AnalysisResult::FAIL;
    }
    
    if (!logicOK) {
        return AnalysisResult::FAIL;
    }
    
    return AnalysisResult::PASS;
}

//=============================================================================
// 缺失的成员函数实现
//=============================================================================

FaultAnalysis::~FaultAnalysis()
{
    qDebug() << "FaultAnalysis 析构开始...";
    
    // 设置析构标志，防止其他操作
    static bool isDestructing = false;
    if (isDestructing) {
        qWarning() << "检测到重复析构调用，直接返回";
        return;
    }
    isDestructing = true;
    
    try {
        // 断开所有信号连接，防止析构过程中触发槽函数
        disconnect(this, nullptr, nullptr, nullptr);
        
        // 直接清理工作线程，不使用复杂的锁操作
        if (currentWorker_) {
            qDebug() << "清理分析工作线程...";
            
            // 断开工作线程的所有信号连接
            disconnect(currentWorker_, nullptr, nullptr, nullptr);
            
            if (currentWorker_->isRunning()) {
                currentWorker_->requestInterruption();
                currentWorker_->quit();
                
                // 简化等待逻辑，不使用锁
                if (!currentWorker_->wait(2000)) {
                    qWarning() << "工作线程未能在2秒内停止，强制终止";
                    currentWorker_->terminate();
                    currentWorker_->wait(500);
                }
            }
            
            // 直接删除，不使用deleteLater
            delete currentWorker_;
            currentWorker_ = nullptr;
        }
        
        if (workerThread_) {
            qDebug() << "清理辅助工作线程...";
            
            if (workerThread_->isRunning()) {
                workerThread_->quit();
                if (!workerThread_->wait(2000)) {
                    workerThread_->terminate();
                    workerThread_->wait(500);
                }
            }
            
            // 直接删除，不使用deleteLater
            delete workerThread_;
            workerThread_ = nullptr;
        }
          // 清理算法对象
        qDebug() << "清理算法对象...";
        
        // 使用更安全的方式清理算法对象
        QList<AnalysisAlgorithm*> algorithmsToDelete;
        
        // 首先收集所有唯一的算法指针
        QSet<AnalysisAlgorithm*> uniqueAlgorithms;
        for (auto it = algorithms_.begin(); it != algorithms_.end(); ++it) {
            if (it.value() && !uniqueAlgorithms.contains(it.value())) {
                uniqueAlgorithms.insert(it.value());
                algorithmsToDelete.append(it.value());
            }
        }
        
        // 清空映射表
        algorithms_.clear();
          // 安全删除算法对象
        qDebug() << "准备删除" << algorithmsToDelete.size() << "个唯一算法对象";
        for (int i = 0; i < algorithmsToDelete.size(); ++i) {
            AnalysisAlgorithm* algorithm = algorithmsToDelete[i];
            try {
                if (algorithm) {
                    QString algorithmName = algorithm->getAlgorithmName();
                    qDebug() << "删除算法对象" << (i+1) << "/" << algorithmsToDelete.size() << ":" << algorithmName;
                    delete algorithm;
                    qDebug() << "算法对象删除成功:" << algorithmName;
                } else {
                    qDebug() << "跳过空算法对象指针" << (i+1) << "/" << algorithmsToDelete.size();
                }
            } catch (const std::exception& e) {
                qWarning() << "删除算法对象" << (i+1) << "时发生异常:" << e.what();
            } catch (...) {
                qWarning() << "删除算法对象" << (i+1) << "时发生未知异常";
            }
        }
        
        qDebug() << "算法对象清理完成";
        qDebug() << "FaultAnalysis 析构完成";
    } catch (const std::exception& e) {
        qCritical() << "FaultAnalysis析构过程中发生异常:" << e.what();
    } catch (...) {
        qCritical() << "FaultAnalysis析构过程中发生未知异常";
    }
    
    isDestructing = false;
}

QVector<MeasurementData> FaultAnalysis::processRawData(const TestConfiguration& config, 
                                                      const QMap<QString, QVector<double>>& rawResults)
{
    QVector<MeasurementData> processedData;
    
    for (auto it = rawResults.begin(); it != rawResults.end(); ++it) {
        const QString& signalName = it.key();
        const QVector<double>& rawData = it.value();
        
        if (rawData.isEmpty()) {
            continue;
        }
        
        MeasurementData data;
        data.signalName = signalName;
        data.rawData = rawData;
        data.isValid = true;
        
        // 确定信号类型
        if (signalName.contains("voltage", Qt::CaseInsensitive)) {
            data.signalType = SignalType::VOLTAGE_DC;
        } else if (signalName.contains("current", Qt::CaseInsensitive)) {
            data.signalType = SignalType::CURRENT_DC;
        } else if (signalName.contains("resistance", Qt::CaseInsensitive)) {
            data.signalType = SignalType::RESISTANCE;
        } else if (signalName.contains("capacitance", Qt::CaseInsensitive)) {
            data.signalType = SignalType::CAPACITANCE;
        } else if (signalName.contains("inductance", Qt::CaseInsensitive)) {
            data.signalType = SignalType::INDUCTANCE;
        } else if (signalName.contains("digital", Qt::CaseInsensitive)) {
            data.signalType = SignalType::DIGITAL_INPUT;
        } else {
            data.signalType = SignalType::VOLTAGE_DC; // 默认类型
        }
          // 计算统计值
        data.mean = this->calculateMean(rawData);
        data.rms = this->calculateRMS(rawData);
        data.peak = this->calculatePeak(rawData);
        data.value = data.mean; // 主要值使用平均值
        
        // 根据信号类型设置相应的值
        switch (data.signalType) {
            case SignalType::VOLTAGE_DC:
            case SignalType::VOLTAGE_AC:
                data.voltage = data.mean;
                break;
            case SignalType::CURRENT_DC:
            case SignalType::CURRENT_AC:
                data.current = data.mean;
                break;
            case SignalType::POWER:
                data.power = data.mean;
                break;
            default:
                // 其他类型保持默认值
                break;
        }
          // 添加统计信息
        data.statistics["std_dev"] = this->calculateStdDev(rawData);
        data.statistics["sample_count"] = rawData.size();
        data.statistics["min_value"] = *std::min_element(rawData.begin(), rawData.end());
        data.statistics["max_value"] = *std::max_element(rawData.begin(), rawData.end());
        
        processedData.append(data);
    }
    
    qDebug() << "处理原始数据完成，生成" << processedData.size() << "个测量数据";
    return processedData;
}

void FaultAnalysis::initializeBuiltinAlgorithms()
{
    qDebug() << "初始化内置分析算法...";
    
    // 使用更安全的方式清理现有算法
    QSet<AnalysisAlgorithm*> uniqueAlgorithms;
    for (auto it = algorithms_.begin(); it != algorithms_.end(); ++it) {
        if (it.value() && !uniqueAlgorithms.contains(it.value())) {
            uniqueAlgorithms.insert(it.value());
        }
    }
    
    // 清空映射表
    algorithms_.clear();
    
    // 删除唯一的算法对象
    for (AnalysisAlgorithm* algorithm : uniqueAlgorithms) {
        delete algorithm;
    }
    
    // 创建各种分析算法实例
    AnalysisAlgorithm* resistorAlgorithm = new ResistorAnalysisAlgorithm(this);
    algorithms_["RESISTOR"] = resistorAlgorithm;
    algorithms_["resistor"] = resistorAlgorithm; // 别名，指向同一对象
    algorithms_["电阻"] = resistorAlgorithm; // 中文别名，指向同一对象
    
    AnalysisAlgorithm* capacitorAlgorithm = new CapacitorAnalysisAlgorithm(this);
    algorithms_["CAPACITOR"] = capacitorAlgorithm;
    algorithms_["capacitor"] = capacitorAlgorithm;
    algorithms_["电容"] = capacitorAlgorithm;
    
    AnalysisAlgorithm* inductorAlgorithm = new InductorAnalysisAlgorithm(this);
    algorithms_["INDUCTOR"] = inductorAlgorithm;
    algorithms_["inductor"] = inductorAlgorithm;
    algorithms_["电感"] = inductorAlgorithm;
    
    AnalysisAlgorithm* diodeAlgorithm = new DiodeAnalysisAlgorithm(this);
    algorithms_["DIODE"] = diodeAlgorithm;
    algorithms_["diode"] = diodeAlgorithm;
    algorithms_["二极管"] = diodeAlgorithm;
    
    AnalysisAlgorithm* icAlgorithm = new ICAnalysisAlgorithm(this);
    algorithms_["IC"] = icAlgorithm;
    algorithms_["ic"] = icAlgorithm;
    algorithms_["集成电路"] = icAlgorithm;
    
    qDebug() << "内置分析算法初始化完成，共" << algorithms_.keys().size() << "种算法";
}

QString FaultAnalysis::determineComponentType(const TestConfiguration& config, 
                                             const QMap<QString, QVariant>& componentSpecs)
{
    // 优先从组件规格中获取类型
    if (componentSpecs.contains("component_type")) {
        QString type = componentSpecs["component_type"].toString().toUpper();
        if (algorithms_.contains(type)) {
            return type;
        }
    }
    
    // 从测试配置名称推断
    QString configName = config.testName.toLower();
    if (configName.contains("resistor") || configName.contains("电阻")) {
        return "RESISTOR";
    } else if (configName.contains("capacitor") || configName.contains("电容")) {
        return "CAPACITOR";
    } else if (configName.contains("inductor") || configName.contains("电感")) {
        return "INDUCTOR";
    } else if (configName.contains("diode") || configName.contains("二极管")) {
        return "DIODE";
    } else if (configName.contains("ic") || configName.contains("集成电路")) {
        return "IC";
    }
    
    // 从测试ID推断（增强逻辑）
    QString testId = config.testId.toLower();
    if (testId.contains("r") && (testId.contains("ohm") || testId.contains("resistance") || testId.startsWith("r"))) {
        return "RESISTOR";
    } else if (testId.contains("c") && (testId.contains("cap") || testId.contains("farad") || testId.startsWith("c"))) {
        return "CAPACITOR";
    } else if (testId.contains("l") && (testId.contains("ind") || testId.contains("henry") || testId.startsWith("l"))) {
        return "INDUCTOR";
    } else if (testId.contains("d") && (testId.contains("diode") || testId.startsWith("d"))) {
        return "DIODE";    }
    
    // 从端口配置推断（新增）
    for (const PortConfig& port : config.inputPorts) {
        if (port.signalType == SignalType::RESISTANCE) {
            return "RESISTOR";
        } else if (port.signalType == SignalType::CAPACITANCE) {
            return "CAPACITOR";
        } else if (port.signalType == SignalType::INDUCTANCE) {
            return "INDUCTOR";
        }
    }
    
    // 默认情况：如果测量了电阻，很可能是电阻器
    qDebug() << "无法确定组件类型，使用默认推断";
    return "RESISTOR";  // 改为默认返回RESISTOR而不是空字符串
}

AnalysisResult FaultAnalysis::createErrorResult(const QString& error, const QString& testId)
{
    AnalysisResult result;
    result.testId = testId;
    result.result = AnalysisResult::ERROR;
    result.timestamp = QDateTime::currentDateTime();
    result.confidence = 0.0;
    result.healthScore = 0.0;
    result.summary = error;
    result.notes.append(QString("错误: %1").arg(error));
    result.faultTypes.append("ERROR");
    
    qWarning() << "创建错误分析结果:" << error << "测试ID:" << testId;
    return result;
}

// 辅助函数实现
double FaultAnalysis::calculateMean(const QVector<double>& data)
{
    if (data.isEmpty()) return 0.0;
    
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / data.size();
}

double FaultAnalysis::calculateRMS(const QVector<double>& data)
{
    if (data.isEmpty()) return 0.0;
    
    double sumSquares = 0.0;
    for (double value : data) {
        sumSquares += value * value;
    }
    
    return qSqrt(sumSquares / data.size());
}

double FaultAnalysis::calculateStdDev(const QVector<double>& data)
{
    if (data.size() < 2) return 0.0;
    
    double mean = this->calculateMean(data);
    double sumSquareDiffs = 0.0;
    
    for (double value : data) {
        double diff = value - mean;
        sumSquareDiffs += diff * diff;
    }
    
    return qSqrt(sumSquareDiffs / (data.size() - 1));
}

double FaultAnalysis::calculatePeak(const QVector<double>& data)
{
    if (data.isEmpty()) return 0.0;
    
    auto minMax = std::minmax_element(data.begin(), data.end());
    return qMax(qAbs(*minMax.first), qAbs(*minMax.second));
}

//=============================================================================
// 槽函数实现
//=============================================================================

void FaultAnalysis::onAsyncAnalysisFinished()
{
    qDebug() << "异步分析完成";
    
    // 清理工作线程
    if (currentWorker_) {
        currentWorker_->deleteLater();
        currentWorker_ = nullptr;
    }
    
    if (workerThread_) {
        workerThread_->deleteLater();
        workerThread_ = nullptr;
    }
}

void FaultAnalysis::stopAllAnalysis()
{
    static QMutex stopMutex;
    QMutexLocker locker(&stopMutex);
    
    static bool isStoppping = false;
    if (isStoppping) {
        qDebug() << "stopAllAnalysis 已在执行中，跳过重复调用";
        return;
    }
    
    isStoppping = true;
    qDebug() << "强制停止所有分析操作...";
    
    try {
        // 停止工作线程
        if (currentWorker_ && currentWorker_->isRunning()) {
            qDebug() << "停止分析工作线程...";
            currentWorker_->requestInterruption();
            currentWorker_->quit();
            
            if (!currentWorker_->wait(1500)) {
                qWarning() << "工作线程未能在1.5秒内停止，强制终止";
                currentWorker_->terminate();
                currentWorker_->wait(500);
            }
        }
        
        // 不在这里删除工作线程对象，留给析构函数处理
        
        if (workerThread_ && workerThread_->isRunning()) {
            workerThread_->quit();
            if (!workerThread_->wait(1500)) {
                workerThread_->terminate();
                workerThread_->wait(500);
            }
        }
        
        qDebug() << "所有分析操作已停止";
    } catch (const std::exception& e) {
        qWarning() << "停止分析操作时发生异常:" << e.what();
    } catch (...) {
        qWarning() << "停止分析操作时发生未知异常";
    }
    
    isStoppping = false;
}
