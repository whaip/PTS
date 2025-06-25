#include "faultdiagnostic.h"
#include "ComponentDiagnosticFramework/componentdiagnosticframework.h"
#include <QDebug>
#include <QUuid>
#include <QTimer>
#include <QApplication>
#include <QMessageBox>

FaultDiagnostic::FaultDiagnostic(DeviceManager* deviceManager, QObject *parent)
    : QObject(parent)
    , device_manager_(deviceManager)
    , diagnostic_manager_(nullptr)
{
    qDebug() << "初始化基于ComponentDiagnosticFramework的FaultDiagnostic...";
    
    if (!initializeDiagnosticFramework()) {
        qCritical() << "诊断框架初始化失败";
        setError("诊断框架初始化失败");
    }
}

FaultDiagnostic::~FaultDiagnostic()
{
    qDebug() << "FaultDiagnostic 析构...";
    
    if (diagnostic_manager_) {
        // 取消所有活动任务
        auto activeTasks = diagnostic_manager_->getActiveTasks();
        for (const QString& taskId : activeTasks) {
            diagnostic_manager_->cancelTask(taskId);
        }
        
        // 清理已完成的任务
        diagnostic_manager_->cleanupCompletedTasks();
        
        delete diagnostic_manager_;
        diagnostic_manager_ = nullptr;
    }
}

bool FaultDiagnostic::initializeDiagnosticFramework()
{
    try {
        qInfo() << "开始创建诊断管理器...";
        
        // 创建诊断管理器
        diagnostic_manager_ = ComponentDiagnosticFramework::createDiagnosticManager(device_manager_, this);
        
        if (!diagnostic_manager_) {
            setError("无法创建诊断管理器");
            return false;
        }
        
        qInfo() << "诊断管理器创建成功";
        
        // 连接框架信号
        connectFrameworkSignals();
        
        qInfo() << "诊断框架初始化成功";
        // 暂时注释掉这行以避免卡住
        // qInfo() << "支持的组件类型:" << getSupportedComponentTypes();
        
        return true;
        
    } catch (const std::exception& e) {
        setError(QString("框架初始化异常: %1").arg(e.what()));
        return false;
    }
}

void FaultDiagnostic::connectFrameworkSignals()
{
    if (!diagnostic_manager_) {
        return;
    }
    
    // 连接单个组件诊断信号
    connect(diagnostic_manager_, &ComponentDiagnosticManager::componentDiagnosisStarted,
            this, &FaultDiagnostic::onFrameworkComponentDiagnosisStarted);
    connect(diagnostic_manager_, &ComponentDiagnosticManager::componentDiagnosisProgress,
            this, &FaultDiagnostic::onFrameworkComponentDiagnosisProgress);
    connect(diagnostic_manager_, &ComponentDiagnosticManager::componentDiagnosisCompleted,
            this, &FaultDiagnostic::onFrameworkComponentDiagnosisCompleted);
    connect(diagnostic_manager_, &ComponentDiagnosticManager::componentDiagnosisError,
            this, &FaultDiagnostic::onFrameworkComponentDiagnosisError);
    
    // 连接批量诊断信号
    connect(diagnostic_manager_, &ComponentDiagnosticManager::batchDiagnosisStarted,
            this, &FaultDiagnostic::onFrameworkBatchDiagnosisStarted);
    connect(diagnostic_manager_, &ComponentDiagnosticManager::batchDiagnosisProgress,
            this, &FaultDiagnostic::onFrameworkBatchDiagnosisProgress);
    connect(diagnostic_manager_, &ComponentDiagnosticManager::batchDiagnosisCompleted,
            this, &FaultDiagnostic::onFrameworkBatchDiagnosisCompleted);
    connect(diagnostic_manager_, &ComponentDiagnosticManager::batchDiagnosisError,
            this, &FaultDiagnostic::onFrameworkBatchDiagnosisError);
}

// === 主要诊断接口实现 ===

DiagnosticResult FaultDiagnostic::diagnoseComponent(const ComponentSpec& component)
{
    if (!diagnostic_manager_) {
        return createErrorResult("无诊断管理器", "诊断管理器未初始化");
    }
    
    try {
        qDebug() << "开始诊断组件:" << component.reference << "类型:" << componentTypeToString(component.type);
        
        // 发射开始信号以保持向后兼容
        emit diagnosisStarted(component);
        
        // 使用框架进行诊断
        auto frameworkResult = diagnostic_manager_->diagnoseComponent(component);
        
        // 转换结果格式
        auto result = convertFromComponentDiagnosticResult(frameworkResult);
        
        // 发射完成信号以保持向后兼容
        emit diagnosisCompleted(result);
        emit diagnosticCompleted(result);
        
        qDebug() << "组件诊断完成:" << component.reference << "结果:" << (result.result == DiagnosticResult::PASS ? "PASS" : "FAIL");
        
        return result;
        
    } catch (const ComponentDiagnosticFramework::DiagnosticException& e) {
        QString error = QString("诊断异常: %1").arg(e.message());
        setError(error);
        emit errorOccurred(error);
        return createErrorResult(component.reference, error);
        
    } catch (const std::exception& e) {
        QString error = QString("未知异常: %1").arg(e.what());
        setError(error);
        emit errorOccurred(error);
        return createErrorResult(component.reference, error);
    }
}

QString FaultDiagnostic::diagnoseComponentAsync(const ComponentSpec& component)
{
    if (!diagnostic_manager_) {
        setError("诊断管理器未初始化");
        return QString();
    }
    
    try {
        qDebug() << "开始异步诊断组件:" << component.reference;
        return diagnostic_manager_->diagnoseComponentAsync(component);
        
    } catch (const std::exception& e) {
        setError(QString("异步诊断失败: %1").arg(e.what()));
        return QString();
    }
}

QVector<DiagnosticResult> FaultDiagnostic::diagnoseBatch(const QVector<ComponentSpec>& components)
{
    if (!diagnostic_manager_) {
        setError("诊断管理器未初始化");
        return QVector<DiagnosticResult>();
    }
    
    try {
        qDebug() << "开始批量诊断" << components.size() << "个组件";
        
        auto frameworkResults = diagnostic_manager_->diagnoseBatch(components);
        
        QVector<DiagnosticResult> results;
        for (const auto& frameworkResult : frameworkResults) {
            results.append(convertFromComponentDiagnosticResult(frameworkResult));
        }
        
        qDebug() << "批量诊断完成，共" << results.size() << "个结果";
        return results;
        
    } catch (const std::exception& e) {
        setError(QString("批量诊断失败: %1").arg(e.what()));
        return QVector<DiagnosticResult>();
    }
}

QString FaultDiagnostic::diagnoseBatchAsync(const QVector<ComponentSpec>& components)
{
    if (!diagnostic_manager_) {
        setError("诊断管理器未初始化");
        return QString();
    }
    
    try {
        qDebug() << "开始异步批量诊断" << components.size() << "个组件";
        return diagnostic_manager_->diagnoseBatchAsync(components);
        
    } catch (const std::exception& e) {
        setError(QString("异步批量诊断失败: %1").arg(e.what()));
        return QString();
    }
}

// === 任务管理接口实现 ===

QMap<QString, QVariant> FaultDiagnostic::getTaskStatus(const QString& taskId) const
{
    if (!diagnostic_manager_) {
        return QMap<QString, QVariant>();
    }
    
    return diagnostic_manager_->getTaskStatus(taskId);
}

QVariant FaultDiagnostic::getTaskResult(const QString& taskId) const
{
    if (!diagnostic_manager_) {
        return QVariant();
    }
    
    return diagnostic_manager_->getTaskResult(taskId);
}

bool FaultDiagnostic::cancelTask(const QString& taskId)
{
    if (!diagnostic_manager_) {
        return false;
    }
    
    return diagnostic_manager_->cancelTask(taskId);
}

// === 配置和信息接口实现 ===

QStringList FaultDiagnostic::getSupportedComponentTypes() const
{
    if (!diagnostic_manager_) {
        return QStringList();
    }
    
    QStringList typeStrings;
    auto types = diagnostic_manager_->getSupportedComponentTypes();
    for (auto type : types) {
        typeStrings.append(componentTypeToString(type));
    }
    
    return typeStrings;
}

bool FaultDiagnostic::isComponentTypeSupported(ComponentType type) const
{
    if (!diagnostic_manager_) {
        return false;
    }
    
    return diagnostic_manager_->isComponentTypeSupported(type);
}

QMap<QString, QVariant> FaultDiagnostic::getDiagnosticStatistics() const
{
    if (!diagnostic_manager_) {
        return QMap<QString, QVariant>();
    }
    
    return diagnostic_manager_->getDiagnosticStatistics();
}

void FaultDiagnostic::setGlobalTimeout(int timeout)
{
    if (diagnostic_manager_) {
        diagnostic_manager_->setGlobalTimeout(timeout);
    }
}

// === 向后兼容接口实现 ===

DiagnosticResult FaultDiagnostic::diagnoseComponentWithModules(const ComponentSpec& component)
{
    // 直接使用新的框架方法
    return diagnoseComponent(component);
}

TestData FaultDiagnostic::executeSynchronousTest(const ComponentSpec& component)
{
    // 为了向后兼容，从诊断结果中提取测试数据
    auto result = diagnoseComponent(component);
    
    TestData testData;
    testData.testId = result.testId;
    testData.timestamp = result.timestamp;
    testData.valid = (result.result != DiagnosticResult::ERROR);
    testData.errorMessage = result.notes;
    
    // 转换测量数据
    QMap<QString, QVariant> measurement;
    measurement["primary_value"] = result.measurementData.primary_value;
    measurement["voltage"] = result.measurementData.voltage;
    measurement["current"] = result.measurementData.current;
    measurement["power"] = result.measurementData.power;
    measurement["temperature"] = result.measurementData.temperature;
    measurement["esr"] = result.measurementData.esr;
    measurement["leakage_current"] = result.measurementData.leakage_current;
    measurement["valid"] = result.measurementData.valid;
    
    testData.measurements.append(measurement);
    
    return testData;
}

AnalysisResult FaultDiagnostic::executeSynchronousAnalysis(const QString& testId, const TestData& testData)
{
    // 为了向后兼容，从测试数据创建分析结果
    AnalysisResult analysisResult;
    analysisResult.testId = testId;
    analysisResult.timestamp = testData.timestamp;
    analysisResult.result = testData.valid ? AnalysisResult::PASS : AnalysisResult::FAIL;
    analysisResult.summary = testData.errorMessage;
    
    return analysisResult;
}

// === 具体组件诊断方法（向后兼容） ===

DiagnosticResult FaultDiagnostic::diagnoseResistor(const ComponentSpec& spec)
{
    ComponentSpec resistorSpec = spec;
    resistorSpec.type = ComponentType::RESISTOR;
    return diagnoseComponent(resistorSpec);
}

DiagnosticResult FaultDiagnostic::diagnoseCapacitor(const ComponentSpec& spec)
{
    ComponentSpec capacitorSpec = spec;
    capacitorSpec.type = ComponentType::CAPACITOR;
    return diagnoseComponent(capacitorSpec);
}

DiagnosticResult FaultDiagnostic::diagnoseInductor(const ComponentSpec& spec)
{
    ComponentSpec inductorSpec = spec;
    inductorSpec.type = ComponentType::INDUCTOR;
    return diagnoseComponent(inductorSpec);
}

DiagnosticResult FaultDiagnostic::diagnoseDiode(const ComponentSpec& spec)
{
    ComponentSpec diodeSpec = spec;
    diodeSpec.type = ComponentType::DIODE;
    return diagnoseComponent(diodeSpec);
}

DiagnosticResult FaultDiagnostic::diagnoseIC(const ComponentSpec& spec)
{
    ComponentSpec icSpec = spec;
    icSpec.type = ComponentType::IC;
    return diagnoseComponent(icSpec);
}

// === 测量方法（向后兼容） ===

MeasurementResult FaultDiagnostic::measureResistance(int channel, double test_voltage, double nominalValue)
{
    // 创建临时组件规格进行测量
    ComponentSpec tempSpec;
    tempSpec.type = ComponentType::RESISTOR;
    tempSpec.channel = channel;
    tempSpec.test_voltage = test_voltage;
    tempSpec.nominal_value = nominalValue;
    tempSpec.reference = QString("TEMP_R_%1").arg(channel);
    
    auto result = diagnoseComponent(tempSpec);
    return result.measurementData;
}

MeasurementResult FaultDiagnostic::measureCapacitance(int channel, double test_frequency)
{
    // 创建临时组件规格进行测量
    ComponentSpec tempSpec;
    tempSpec.type = ComponentType::CAPACITOR;
    tempSpec.channel = channel;
    tempSpec.reference = QString("TEMP_C_%1").arg(channel);
    
    auto result = diagnoseComponent(tempSpec);
    return result.measurementData;
}

MeasurementResult FaultDiagnostic::measureInductance(int channel, double test_frequency)
{
    // 创建临时组件规格进行测量
    ComponentSpec tempSpec;
    tempSpec.type = ComponentType::INDUCTOR;
    tempSpec.channel = channel;
    tempSpec.reference = QString("TEMP_L_%1").arg(channel);
    
    auto result = diagnoseComponent(tempSpec);
    return result.measurementData;
}

MeasurementResult FaultDiagnostic::measureDiodeCharacteristics(int channel)
{
    // 创建临时组件规格进行测量
    ComponentSpec tempSpec;
    tempSpec.type = ComponentType::DIODE;
    tempSpec.channel = channel;
    tempSpec.reference = QString("TEMP_D_%1").arg(channel);
    
    auto result = diagnoseComponent(tempSpec);
    return result.measurementData;
}

MeasurementResult FaultDiagnostic::measureICParameters(int channel, const ComponentSpec& spec)
{
    ComponentSpec tempSpec = spec;
    tempSpec.type = ComponentType::IC;
    tempSpec.channel = channel;
    
    auto result = diagnoseComponent(tempSpec);
    return result.measurementData;
}

// === 辅助方法实现 ===

bool FaultDiagnostic::checkComponentConnection(int channel)
{
    // 简单的连接性检查
    auto measurement = measureResistance(channel, 1.0, 0.0);
    return measurement.valid;
}

bool FaultDiagnostic::executeSyncMeasurement(const QString& syncGroup, const QStringList& deviceNames, 
                                            const QList<ComponentSpec>& components)
{
    // 批量诊断所有组件
    QVector<ComponentSpec> componentVector;
    for (const auto& component : components) {
        componentVector.append(component);
    }
    
    auto results = diagnoseBatch(componentVector);
    return !results.isEmpty();
}

double FaultDiagnostic::calculateHealthScore(const DiagnosticResult& result)
{
    return ComponentDiagnosticFramework::calculateDiagnosticConfidence(
        convertToComponentDiagnosticResult(result)) * 100.0;
}

QString FaultDiagnostic::generateRecommendation(const DiagnosticResult& result)
{
    auto frameworkResult = convertToComponentDiagnosticResult(result);
    return ComponentDiagnosticFramework::formatDiagnosticResult(frameworkResult, false);
}

double FaultDiagnostic::calculateConfidence(const DiagnosticResult& result)
{
    return ComponentDiagnosticFramework::calculateDiagnosticConfidence(
        convertToComponentDiagnosticResult(result));
}

bool FaultDiagnostic::isWithinTolerance(double nominal, double measured, double tolerance_percent)
{
    if (nominal == 0.0) {
        return qAbs(measured) <= tolerance_percent / 100.0;
    }
    
    double deviation = qAbs((measured - nominal) / nominal) * 100.0;
    return deviation <= tolerance_percent;
}

// === 数据转换方法实现 ===

DiagnosticResult FaultDiagnostic::convertFromComponentDiagnosticResult(const ComponentDiagnosticResult& result)
{
    DiagnosticResult diagnosticResult;
    
    diagnosticResult.componentId = result.componentId;
    diagnosticResult.componentType = result.componentType;
    diagnosticResult.timestamp = result.endTime.isValid() ? result.endTime : result.startTime;
    
    // 转换测试结果
    if (result.success) {
        diagnosticResult.result = DiagnosticResult::TestResult::PASS;
    } else {
        diagnosticResult.result = DiagnosticResult::TestResult::FAIL;
    }
    
    // 设置健康分数和置信度
    diagnosticResult.healthScore = result.healthScore;
    diagnosticResult.confidence = result.confidence;
    
    // 设置故障描述
    diagnosticResult.notes = result.faultDescription;
    
    return diagnosticResult;
}

ComponentDiagnosticResult FaultDiagnostic::convertToComponentResult(const DiagnosticResult& result) 
{
    ComponentDiagnosticResult componentResult;
    
    componentResult.componentId = result.componentId;
    componentResult.componentType = result.componentType;
    componentResult.isPassed = (result.result == DiagnosticResult::PASS);
    componentResult.healthScore = result.healthScore;
    componentResult.confidence = result.confidence / 100.0; // 转换为0-1范围
    componentResult.faultTypes = result.faultTypes;
    componentResult.timestamp = result.timestamp;
    componentResult.notes = result.notes;
    
    // 转换测量数据
    componentResult.measurements["primary_value"] = result.measurementData.primary_value;
    componentResult.measurements["voltage"] = result.measurementData.voltage;
    componentResult.measurements["current"] = result.measurementData.current;
    componentResult.measurements["power"] = result.measurementData.power;
    componentResult.measurements["temperature"] = result.measurementData.temperature;
    componentResult.measurements["esr"] = result.measurementData.esr;
    componentResult.measurements["leakage_current"] = result.measurementData.leakage_current;
    
    // 添加分析数据
    componentResult.analysisData["expected_value"] = result.expectedValue;
    componentResult.analysisData["tolerance"] = result.tolerance;
    componentResult.analysisData["test_equipment"] = result.testEquipment;
    
    // 生成摘要
    if (componentResult.isPassed) {
        componentResult.summary = QString("Component %1 passed diagnostic test with health score %.1f")
                                .arg(componentResult.componentType)
                                .arg(componentResult.healthScore);
    } else {
        componentResult.summary = QString("Component %1 failed diagnostic test. Faults: %2")
                                .arg(componentResult.componentType)
                                .arg(componentResult.faultTypes.join(", "));
    }
    
    return componentResult;
}

DiagnosticResult FaultDiagnostic::convertFromComponentResult(const ComponentDiagnosticResult& result) 
{
    DiagnosticResult diagnosticResult;
    
    diagnosticResult.testId = QString("component_%1_%2")
                            .arg(result.componentType)
                            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    diagnosticResult.componentType = result.componentType;
    diagnosticResult.componentId = result.componentId;
    diagnosticResult.result = result.isPassed ? DiagnosticResult::PASS : DiagnosticResult::FAIL;
    diagnosticResult.healthScore = result.healthScore;
    diagnosticResult.confidence = result.confidence * 100.0; // 转换为0-100范围
    diagnosticResult.faultTypes = result.faultTypes;
    diagnosticResult.timestamp = result.timestamp;
    diagnosticResult.notes = result.notes;
    
    // 转换测量数据
    if (result.measurements.contains("primary_value")) {
        diagnosticResult.measurementData.primary_value = result.measurements["primary_value"];
    }
    if (result.measurements.contains("voltage")) {
        diagnosticResult.measurementData.voltage = result.measurements["voltage"];
    }
    if (result.measurements.contains("current")) {
        diagnosticResult.measurementData.current = result.measurements["current"];
    }
    if (result.measurements.contains("power")) {
        diagnosticResult.measurementData.power = result.measurements["power"];
    }
    if (result.measurements.contains("temperature")) {
        diagnosticResult.measurementData.temperature = result.measurements["temperature"];
    }
    if (result.measurements.contains("esr")) {
        diagnosticResult.measurementData.esr = result.measurements["esr"];
    }
    if (result.measurements.contains("leakage_current")) {
        diagnosticResult.measurementData.leakage_current = result.measurements["leakage_current"];
    }
    
    diagnosticResult.measurementData.valid = result.isPassed;
    if (!result.isPassed && !result.faultTypes.isEmpty()) {
        diagnosticResult.measurementData.error_message = result.faultTypes.first();
    }
    
    // 从分析数据中提取信息
    if (result.analysisData.contains("expected_value")) {
        diagnosticResult.expectedValue = result.analysisData["expected_value"].toDouble();
    }
    if (result.analysisData.contains("tolerance")) {
        diagnosticResult.tolerance = result.analysisData["tolerance"].toDouble();
    }
    if (result.analysisData.contains("test_equipment")) {
        diagnosticResult.testEquipment = result.analysisData["test_equipment"].toString();
    }
    
    return diagnosticResult;
}

DiagnosticResult FaultDiagnostic::createErrorResult(const QString& componentType, const QString& errorMessage) 
{
    DiagnosticResult result;
    
    result.testId = QString("error_%1_%2")
                  .arg(componentType)
                  .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    result.componentType = componentType;
    result.componentId = "unknown";
    result.result = DiagnosticResult::ERROR;
    result.healthScore = 0.0;
    result.confidence = 0.0;
    result.faultTypes << "ERROR";
    result.timestamp = QDateTime::currentDateTime();
    result.notes = errorMessage;
    result.measurementData.valid = false;
    result.measurementData.error_message = errorMessage;
    result.expectedValue = 0.0;
    result.tolerance = 0.0;
    result.testEquipment = "Unknown";
    
    return result;
}

// === 框架信号处理槽 ===

void FaultDiagnostic::onFrameworkComponentDiagnosisStarted(const QString& taskId, const QString& componentId)
{
    emit componentDiagnosisStarted(taskId, componentId);
}

void FaultDiagnostic::onFrameworkComponentDiagnosisProgress(const QString& taskId, const QString& componentId, int percentage)
{
    emit componentDiagnosisProgress(taskId, componentId, percentage);
    emit diagnosticProgress(percentage);
    emit progressUpdated(percentage);
}

void FaultDiagnostic::onFrameworkComponentDiagnosisCompleted(const QString& taskId, const QString& componentId, const ComponentDiagnosticResult& result)
{
    auto diagnosticResult = convertFromComponentDiagnosticResult(result);
    emit componentDiagnosisCompleted(taskId, componentId, diagnosticResult);
}

void FaultDiagnostic::onFrameworkComponentDiagnosisError(const QString& taskId, const QString& componentId, const QString& error)
{
    emit componentDiagnosisError(taskId, componentId, error);
    emit errorOccurred(error);
}

void FaultDiagnostic::onFrameworkBatchDiagnosisStarted(const QString& taskId, int totalComponents)
{
    emit batchDiagnosisStarted(taskId, totalComponents);
}

void FaultDiagnostic::onFrameworkBatchDiagnosisProgress(const QString& taskId, int completedComponents, int totalComponents)
{
    emit batchDiagnosisProgress(taskId, completedComponents, totalComponents);
    int percentage = (completedComponents * 100) / totalComponents;
    emit diagnosticProgress(percentage);
    emit progressUpdated(percentage);
}

void FaultDiagnostic::onFrameworkBatchDiagnosisCompleted(const QString& taskId, const QVector<ComponentDiagnosticResult>& results)
{
    QVector<DiagnosticResult> diagnosticResults;
    for (const auto& result : results) {
        diagnosticResults.append(convertFromComponentDiagnosticResult(result));
    }
    emit batchDiagnosisCompleted(taskId, diagnosticResults);
}

void FaultDiagnostic::onFrameworkBatchDiagnosisError(const QString& taskId, const QString& error)
{
    emit batchDiagnosisError(taskId, error);
    emit errorOccurred(error);
}

// === 向后兼容的私有方法（桩实现） ===

DiagnosticResult FaultDiagnostic::diagnoseComponentInternal(const ComponentSpec& component)
{
    return diagnoseComponent(component);
}

FaultType FaultDiagnostic::analyzeResistorFault(const ComponentSpec& spec, const MeasurementResult& measurement)
{
    if (!measurement.valid) {
        return FaultType::UNKNOWN_FAULT;
    }
    
    if (measurement.primary_value <= 0.1) {
        return FaultType::SHORT_CIRCUIT;
    }
    
    if (measurement.primary_value > 1e6) {
        return FaultType::OPEN_CIRCUIT;
    }
    
    if (!isWithinTolerance(spec.nominal_value, measurement.primary_value, spec.tolerance_percent)) {
        return FaultType::OUT_OF_TOLERANCE;
    }
    
    return FaultType::COMPONENT_OK;
}

FaultType FaultDiagnostic::analyzeCapacitorFault(const ComponentSpec& spec, const MeasurementResult& measurement)
{
    if (!measurement.valid) {
        return FaultType::UNKNOWN_FAULT;
    }
    
    if (measurement.esr > spec.max_esr) {
        return FaultType::HIGH_ESR;
    }
    
    if (measurement.leakage_current > spec.max_leakage) {
        return FaultType::HIGH_LEAKAGE;
    }
    
    if (!isWithinTolerance(spec.nominal_value, measurement.primary_value, spec.tolerance_percent)) {
        return FaultType::OUT_OF_TOLERANCE;
    }
    
    return FaultType::COMPONENT_OK;
}

FaultType FaultDiagnostic::analyzeDiodeFault(const ComponentSpec& spec, const MeasurementResult& measurement)
{
    Q_UNUSED(spec);
    
    if (!measurement.valid) {
        return FaultType::UNKNOWN_FAULT;
    }
    
    if (measurement.voltage < 0.3) {
        return FaultType::DIODE_SHORT;
    }
    
    if (measurement.voltage > 0.9) {
        return FaultType::DIODE_OPEN;
    }
    
    if (measurement.leakage_current > 1e-6) {
        return FaultType::DIODE_LEAKAGE;
    }
    
    return FaultType::COMPONENT_OK;
}

FaultType FaultDiagnostic::analyzeICFault(const ComponentSpec& spec, const MeasurementResult& measurement)
{
    Q_UNUSED(spec);
    
    if (!measurement.valid) {
        return FaultType::UNKNOWN_FAULT;
    }
    
    if (measurement.current > 1.0) {
        return FaultType::IC_OVERCURRENT;
    }
    
    return FaultType::COMPONENT_OK;
}

QString FaultDiagnostic::faultTypeToString(FaultType type) const
{
    switch (type) {
        case FaultType::COMPONENT_OK: return "正常";
        case FaultType::SHORT_CIRCUIT: return "短路";
        case FaultType::OPEN_CIRCUIT: return "开路";
        case FaultType::OUT_OF_TOLERANCE: return "参数超差";
        case FaultType::HIGH_ESR: return "ESR过高";
        case FaultType::HIGH_LEAKAGE: return "漏电流过大";
        case FaultType::DIODE_SHORT: return "二极管短路";
        case FaultType::DIODE_OPEN: return "二极管开路";
        case FaultType::DIODE_LEAKAGE: return "二极管漏电";
        case FaultType::IC_OVERCURRENT: return "IC过流";
        case FaultType::IC_LOGIC_ERROR: return "IC逻辑错误";
        case FaultType::NO_CONNECTION: return "无连接";
        case FaultType::UNKNOWN_FAULT: return "未知故障";
        default: return "未定义故障";
    }
}

bool FaultDiagnostic::applyTestVoltage(int channel, double voltage)
{
    // 这个方法在新框架中由具体的诊断器处理
    Q_UNUSED(channel);
    Q_UNUSED(voltage);
    return true;
}

void FaultDiagnostic::waitForStabilization(int delay_ms)
{
    QApplication::processEvents();
    QThread::msleep(delay_ms);
}

double FaultDiagnostic::applyTemperatureCompensation(double value, double temp_coeff, double temperature)
{
    // 简单的线性温度补偿
    double tempDiff = temperature - 25.0;  // 以25°C为基准
    return value * (1.0 + temp_coeff * tempDiff / 100.0);
}

// === 数据转换辅助方法 ===

ComponentSpec FaultDiagnostic::convertToLegacyComponentSpec(const TestSchemeSignals& scheme, const TestConfiguration& config)
{
    ComponentSpec spec;
    spec.reference = config.testId;
    spec.name = config.testName;
    // 其他字段根据需要填充
    return spec;
}

DiagnosticResult FaultDiagnostic::convertFromAnalysisResult(const AnalysisResult& analysisResult)
{
    DiagnosticResult result;
    result.testId = analysisResult.testId;
    result.componentType = analysisResult.componentType;
    result.componentId = analysisResult.componentId;
    result.result = static_cast<DiagnosticResult::TestResult>(analysisResult.result);
    result.healthScore = analysisResult.healthScore;
    result.confidence = analysisResult.confidence;
    result.timestamp = analysisResult.timestamp;
    result.notes = analysisResult.summary;
    
    return result;
}

TestData FaultDiagnostic::convertFromMeasurementResult(const MeasurementResult& measurement)
{
    TestData testData;
    testData.testId = QUuid::createUuid().toString();
    testData.timestamp = QDateTime::currentDateTime();
    testData.valid = measurement.valid;
    testData.errorMessage = measurement.error_message;
    
    QMap<QString, QVariant> measurementMap;
    measurementMap["primary_value"] = measurement.primary_value;
    measurementMap["voltage"] = measurement.voltage;
    measurementMap["current"] = measurement.current;
    measurementMap["power"] = measurement.power;
    measurementMap["temperature"] = measurement.temperature;
    measurementMap["esr"] = measurement.esr;
    measurementMap["leakage_current"] = measurement.leakage_current;
    
    testData.measurements.append(measurementMap);
    
    return testData;
}

ComponentDiagnosticResult FaultDiagnostic::convertToComponentDiagnosticResult(const DiagnosticResult& result)
{
    ComponentDiagnosticResult componentResult;
    
    componentResult.componentId = result.componentId;
    componentResult.componentType = result.componentType;
    componentResult.startTime = result.timestamp;
    componentResult.endTime = result.timestamp;
    
    // 转换测试结果
    componentResult.success = (result.result == DiagnosticResult::TestResult::PASS);
    componentResult.isPassed = componentResult.success;
    
    // 转换故障信息
    if (result.result != DiagnosticResult::TestResult::PASS) {
        QString faultMessage;
        switch (result.result) {
            case DiagnosticResult::TestResult::FAIL:
                faultMessage = "组件诊断失败";
                break;
            case DiagnosticResult::TestResult::ERROR:
                faultMessage = "诊断过程出错";
                break;
            default:
                faultMessage = "未知故障";
                break;
        }
        componentResult.faultDescription = faultMessage + (result.notes.isEmpty() ? "" : ": " + result.notes);
    }
    
    // 设置健康分数和置信度
    componentResult.healthScore = result.healthScore;
    componentResult.confidence = result.confidence;
    
    // 设置分析数据
    componentResult.analysisData["health_score"] = result.healthScore;
    componentResult.analysisData["confidence"] = result.confidence;
    componentResult.analysisData["test_result"] = static_cast<int>(result.result);
    
    if (!result.notes.isEmpty()) {
        componentResult.analysisData["notes"] = result.notes;
    }
    
    return componentResult;
}

void FaultDiagnostic::setError(const QString& error)
{
    last_error_ = error;
    emit errorOccurred(error);
    qWarning() << "FaultDiagnostic error:" << error;
}
