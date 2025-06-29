// 故障分析模块 - 测试完成后进行数据分析和故障诊断
#ifndef FAULTANALYSIS_H
#define FAULTANALYSIS_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include <QVariant>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include "commontypes.h"

// Forward declarations
struct TestData;
struct TestConfiguration;

// 前向声明
struct TestData;

// 测量数据结构
struct MeasurementData {
    QString signalName;                 // 信号名称
    SignalType signalType;              // 信号类型
    QVector<double> rawData;            // 原始数据
    double mean;                        // 平均值
    double rms;                         // 有效值
    double peak;                        // 峰值
    double frequency;                   // 频率
    double phase;                       // 相位
    bool isValid;                       // 数据有效性
    QString errorMessage;               // 错误信息
    QMap<QString, QVariant> statistics; // 统计数据
    
    // 新增成员变量
    double value;                       // 主要测量值
    double voltage;                     // 电压值
    double current;                     // 电流值
    double power;                       // 功率值
    double esr;                         // ESR值
    double leakageCurrent;              // 漏电流值
    
    MeasurementData() : signalType(SignalType::VOLTAGE_DC), mean(0), rms(0), 
                       peak(0), frequency(0), phase(0), isValid(false),
                       value(0), voltage(0), current(0), power(0), 
                       esr(0), leakageCurrent(0) {}
};

// 分析算法基类
class AnalysisAlgorithm : public QObject
{
    Q_OBJECT

public:
    explicit AnalysisAlgorithm(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~AnalysisAlgorithm() {}
    
    virtual AnalysisResult analyze(const TestConfiguration& config,
                                  const QVector<MeasurementData>& data,
                                  const QMap<QString, QVariant>& componentSpecs) = 0;
    
    virtual QString getAlgorithmName() const = 0;
    virtual QString getComponentType() const = 0;
    virtual QStringList getSupportedSignalTypes() const = 0;

protected:
    // 通用分析工具方法
    double calculateMean(const QVector<double>& data);
    double calculateRMS(const QVector<double>& data);
    double calculateStdDev(const QVector<double>& data);
    double calculatePeak(const QVector<double>& data);
    bool isWithinTolerance(double value, double expected, double tolerance);
    double calculateDeviation(double value, double expected);
    double calculateHealthScore(double deviation, double tolerance);
};

// 电阻分析算法
class ResistorAnalysisAlgorithm : public AnalysisAlgorithm
{
    Q_OBJECT

public:
    explicit ResistorAnalysisAlgorithm(QObject *parent = nullptr);
    
    AnalysisResult analyze(const TestConfiguration& config,
                          const QVector<MeasurementData>& data,
                          const QMap<QString, QVariant>& componentSpecs) override;
    
    QString getAlgorithmName() const override { return "Resistor Analysis"; }
    QString getComponentType() const override { return "RESISTOR"; }
    QStringList getSupportedSignalTypes() const override;

private:
    double calculateResistanceFromVI(double voltage, double current);
    double calculateResistanceFromDirect(const MeasurementData& resistanceData);
    bool checkForShortCircuit(double resistance, double tolerance);
    bool checkForOpenCircuit(double resistance, double maxResistance);
    AnalysisResult::TestResult evaluateResistance(double measured, double expected, double tolerance);
};

// 电容分析算法
class CapacitorAnalysisAlgorithm : public AnalysisAlgorithm
{
    Q_OBJECT

public:
    explicit CapacitorAnalysisAlgorithm(QObject *parent = nullptr);
    
    AnalysisResult analyze(const TestConfiguration& config,
                          const QVector<MeasurementData>& data,
                          const QMap<QString, QVariant>& componentSpecs) override;
    
    QString getAlgorithmName() const override { return "Capacitor Analysis"; }
    QString getComponentType() const override { return "CAPACITOR"; }
    QStringList getSupportedSignalTypes() const override;

private:
    double calculateCapacitanceFromImpedance(double impedance, double frequency);
    double calculateESR(const MeasurementData& esrData);
    double calculateLeakageCurrent(const MeasurementData& currentData);
    bool checkESRLimit(double esr, double maxESR);
    bool checkLeakageLimit(double leakage, double maxLeakage);
    AnalysisResult::TestResult evaluateCapacitor(double capacitance, double esr, double leakage,
                                                const QMap<QString, QVariant>& specs);
};

// 电感分析算法
class InductorAnalysisAlgorithm : public AnalysisAlgorithm
{
    Q_OBJECT

public:
    explicit InductorAnalysisAlgorithm(QObject *parent = nullptr);
    
    AnalysisResult analyze(const TestConfiguration& config,
                          const QVector<MeasurementData>& data,
                          const QMap<QString, QVariant>& componentSpecs) override;
    
    QString getAlgorithmName() const override { return "Inductor Analysis"; }
    QString getComponentType() const override { return "INDUCTOR"; }
    QStringList getSupportedSignalTypes() const override;

private:
    double calculateInductanceFromImpedance(double impedance, double frequency);
    double calculateDCR(const MeasurementData& resistanceData);
    double calculateQFactor(double inductance, double dcr, double frequency);
    AnalysisResult::TestResult evaluateInductor(double inductance, double dcr, double qFactor,
                                               const QMap<QString, QVariant>& specs);
};

// 二极管分析算法
class DiodeAnalysisAlgorithm : public AnalysisAlgorithm
{
    Q_OBJECT

public:
    explicit DiodeAnalysisAlgorithm(QObject *parent = nullptr);
    
    AnalysisResult analyze(const TestConfiguration& config,
                          const QVector<MeasurementData>& data,
                          const QMap<QString, QVariant>& componentSpecs) override;
    
    QString getAlgorithmName() const override { return "Diode Analysis"; }
    QString getComponentType() const override { return "DIODE"; }
    QStringList getSupportedSignalTypes() const override;

private:
    double calculateForwardVoltage(const MeasurementData& voltageData, double current);
    double calculateReverseLeakage(const MeasurementData& currentData);
    bool checkForwardVoltageRange(double vf, double minVf, double maxVf);
    bool checkReverseLeakageLimit(double leakage, double maxLeakage);
    AnalysisResult::TestResult evaluateDiode(double forwardVoltage, double reverseLeakage,
                                            const QMap<QString, QVariant>& specs);
};

// IC分析算法
class ICAnalysisAlgorithm : public AnalysisAlgorithm
{
    Q_OBJECT

public:
    explicit ICAnalysisAlgorithm(QObject *parent = nullptr);
    
    AnalysisResult analyze(const TestConfiguration& config,
                          const QVector<MeasurementData>& data,
                          const QMap<QString, QVariant>& componentSpecs) override;
    
    QString getAlgorithmName() const override { return "IC Analysis"; }
    QString getComponentType() const override { return "IC"; }
    QStringList getSupportedSignalTypes() const override;

private:
    double calculateSupplyCurrent(const MeasurementData& currentData);
    bool checkSupplyCurrentLimit(double current, double maxCurrent);
    bool checkLogicLevels(const QVector<MeasurementData>& digitalData);
    AnalysisResult::TestResult evaluateIC(double supplyCurrent, bool logicOK,
                                         const QMap<QString, QVariant>& specs);
};

// 故障分析器主类
class FaultAnalysis : public QObject
{
    Q_OBJECT

public:
    explicit FaultAnalysis(QObject *parent = nullptr);
    ~FaultAnalysis();

    // 注册分析算法
    void registerAlgorithm(const QString& componentType, AnalysisAlgorithm* algorithm);
    void unregisterAlgorithm(const QString& componentType);
    
    // 执行故障分析
    AnalysisResult analyzeTestResults(const TestConfiguration& config,
                                     const QMap<QString, QVector<double>>& rawResults,
                                     const QMap<QString, QVariant>& componentSpecs);
    
    // 同步分析（新增方法）
    AnalysisResult analyzeSynchronously(const QString& testId, const TestData& testData);
    
    // 异步分析
    void analyzeTestResultsAsync(const TestConfiguration& config,
                                const QMap<QString, QVector<double>>& rawResults,
                                const QMap<QString, QVariant>& componentSpecs);
    
    // 异步分析（使用TestData）
    void startAnalysisAsync(const QString& testId, const TestData& testData);
    
    // 处理原始测试数据
    QVector<MeasurementData> processRawData(const TestConfiguration& config,
                                           const QMap<QString, QVector<double>>& rawResults);
    
    // 计算统计数据
    void calculateStatistics(MeasurementData& data);
    
    // 获取支持的组件类型
    QStringList getSupportedComponentTypes() const;
    
    // 验证分析输入
    bool validateAnalysisInput(const TestConfiguration& config,
                              const QVector<MeasurementData>& data) const;
    
    // 生成分析报告
    QString generateAnalysisReport(const AnalysisResult& result) const;
      // 导出分析结果
    bool exportResults(const AnalysisResult& result, const QString& filePath) const;
    
    // 停止所有分析操作（公共方法，供外部调用）
    void stopAllAnalysis();

signals:
    void analysisStarted(const QString& testId);
    void analysisProgress(int percentage);
    void analysisCompleted(const QString& testId, const AnalysisResult& result);  // 修改参数
    void analysisFailed(const QString& error);

private slots:
    void onAsyncAnalysisFinished();

private:
    // 异步分析工作线程
    class AnalysisWorker;
    friend class AnalysisWorker;
    
    QMap<QString, AnalysisAlgorithm*> algorithms_;
    AnalysisWorker* currentWorker_;
    QThread* workerThread_;
    QMutex analysisMutex_;
    
    void initializeBuiltinAlgorithms();
    QString determineComponentType(const TestConfiguration& config,
                                  const QMap<QString, QVariant>& componentSpecs);
    AnalysisResult createErrorResult(const QString& error, const QString& testId = QString());
    
    // 新增：数据转换辅助方法
    QMap<QString, QVector<double>> convertTestDataToRawResults(const TestData& testData);
    QMap<QString, DeviceOperation> extractComponentSpecs(const TestData& testData);
      // 数据处理辅助方法
    MeasurementData processSignalData(const QString& signalName,
                                     SignalType signalType,
                                     const QVector<double>& rawData);
    
    // 统计计算方法
    double calculateMean(const QVector<double>& data);
    double calculateRMS(const QVector<double>& data);
    double calculateStdDev(const QVector<double>& data);
    double calculatePeak(const QVector<double>& data);
    
    // 结果合成
    AnalysisResult combineResults(const QVector<AnalysisResult>& partialResults);
    double calculateOverallHealthScore(const QVector<AnalysisResult>& results);
    double calculateOverallConfidence(const QVector<AnalysisResult>& results);
    
    // 静态统计计算方法
    static double calculateMeanValue(const QVector<double>& data);
    static double calculateRMSValue(const QVector<double>& data);
    static double calculatePeakValue(const QVector<double>& data);
    static double calculateStdDevValue(const QVector<double>& data);
};

#endif // FAULTANALYSIS_H
