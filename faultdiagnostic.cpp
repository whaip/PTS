#include "faultdiagnostic.h"
#include "ComponentDiagnosticFramework/componentdiagnosticframework.h"
#include <QDebug>
#include <QUuid>
#include <QTimer>
#include <QApplication>
#include <QMessageBox>

FaultDiagnostic::FaultDiagnostic(DeviceManager* deviceManager, ComponentDiagnosticManager* diagnosticManager, QObject *parent)
    : QObject(parent)
    , device_manager_(deviceManager)
    , diagnostic_manager_(diagnosticManager)
{
    qDebug() << "初始化基于ComponentDiagnosticFramework的FaultDiagnostic...";
}

FaultDiagnostic::~FaultDiagnostic()
{
    qDebug() << "FaultDiagnostic 析构...";
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
    
    QString taskId = diagnostic_manager_->diagnoseComponentAsync(component);
    QObject::connect(diagnostic_manager_, &ComponentDiagnosticManager::componentDiagnosisCompleted,
                        this, [this, taskId](const QString& taskId, const QString& componentId, const ComponentDiagnosticResult& frameworkResult) {
                        auto result = convertFromComponentDiagnosticResult(frameworkResult);
                        emit diagnosisCompleted(result);
                        emit diagnosticCompleted(result);
                        });
    QObject::connect(diagnostic_manager_, &ComponentDiagnosticManager::componentDiagnosisError,
                        this, [this, taskId](const QString& taskId, const QString& componentId, const QString& error) {
                        setError(error);
                        emit errorOccurred(error);
                        });
    return taskId;
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

DiagnosticResult FaultDiagnostic::convertFromComponentDiagnosticResult(const ComponentDiagnosticResult& result)
{
    DiagnosticResult diagnosticResult;
    
    diagnosticResult.componentId = result.componentId;
    diagnosticResult.componentType = result.componentType;
    diagnosticResult.metameasurementData = result.metameasurements;
    diagnosticResult.thermalData = result.thermalData;
    diagnosticResult.timestamp = result.endTime.isValid() ? result.endTime : result.startTime;
    
    // 转换测试结果
    if (result.isPassed) {
        diagnosticResult.result = DiagnosticResult::TestResult::PASS;
    } else {
        diagnosticResult.result = DiagnosticResult::TestResult::FAIL;
    }
    
    // 设置健康分数和置信度
    diagnosticResult.healthScore = result.healthScore;
    diagnosticResult.confidence = result.confidence;
    diagnosticResult.MetaResult = result;
    
    // 设置故障描述
    diagnosticResult.diagnosticSummary = formatDiagnosticResult(result, true);

    return diagnosticResult;
}

ComponentDiagnosticResult FaultDiagnostic::convertToComponentDiagnosticResult(const DiagnosticResult& result)
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

ComponentSpec FaultDiagnostic::convertToLegacyComponentSpec(const TestSchemeSignals& scheme, const TestConfiguration& config)
{
    ComponentSpec spec;
    spec.reference = config.testId;
    spec.name = config.testName;
    // 其他字段根据需要填充
    return spec;
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

void FaultDiagnostic::setError(const QString& error)
{
    last_error_ = error;
    emit errorOccurred(error);
    qWarning() << "FaultDiagnostic error:" << error;
}
