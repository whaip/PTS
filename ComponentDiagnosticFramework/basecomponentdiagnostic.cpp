#include "basecomponentdiagnostic.h"
#include "../devicemanager.h"
#include "../commontypes.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QtMath>
#include <functional>

// 静态成员初始化
QMap<ComponentType, std::function<BaseComponentDiagnostic*()>> ComponentDiagnosticFactory::creators_;

BaseComponentDiagnostic::BaseComponentDiagnostic(DeviceManager* deviceManager, QObject* parent)
    : QObject(parent)
    , deviceManager_(deviceManager)
{
    if (!deviceManager_) {
        qCritical() << "BaseComponentDiagnostic: DeviceManager不能为空";
    }
}

void BaseComponentDiagnostic::setDeviceManager(DeviceManager* deviceManager)
{
    deviceManager_ = deviceManager;
    if (!deviceManager_) {
        qWarning() << "BaseComponentDiagnostic: DeviceManager set to null";
    }
}

QString BaseComponentDiagnostic::componentTypeToString(ComponentType type)
{
    return ::componentTypeToString(type);
}

ComponentDiagnosticResult BaseComponentDiagnostic::diagnoseComponent(const ComponentSpec& component)
{
    ComponentDiagnosticResult result;
    result.componentId = component.reference;
    result.componentType = componentTypeToString(component.type);
    result.timestamp = QDateTime::currentDateTime();
    
    currentComponentId_ = component.reference;
    
    // 发出诊断开始信号
    emit diagnosisStarted(currentComponentId_);
    
    QElapsedTimer timer;
    timer.start();
    
    try {
        // 1. 验证组件规格
        logInfo(QString("开始诊断组件: %1 (%2)").arg(component.reference).arg(getComponentTypeName()));
        
        // 4. 预处理设置
        if (!preTestSetup(component)) {
            throw std::runtime_error("预处理设置失败");
        }
        
        emit diagnosisProgress(currentComponentId_, 50);
        
        // 5. 配置数据采集
        ComponentTestConfig testConfig = configureDataAcquisition(component, component.allocatedPorts);
        
        if(testConfig.parameters.isEmpty()) {
            throw std::runtime_error("配置数据采集失败, 配置参数为空");
        }
        emit diagnosisProgress(currentComponentId_, 60);
        
        // 6. 执行数据采集
        emit testStarted(currentComponentId_);
        TestData testData = executeDataAcquisition(testConfig);
        emit testCompleted(currentComponentId_, testData);
        
        if (!validateTestData(testData)) {
            throw std::runtime_error("测试数据验证失败");
        }
        
        emit diagnosisProgress(currentComponentId_, 80);
        
        // 7. 故障分析
        result = analyzeFaults(component, testData);
        
        emit diagnosisProgress(currentComponentId_, 90);
        
        // 8. 后处理清理
        postTestCleanup();
        
        // 设置诊断成功的基本信息
        result.componentId = component.reference;
        result.componentType = getComponentTypeName();
        result.timestamp = QDateTime::currentDateTime();
        
        emit diagnosisProgress(currentComponentId_, 100);
        
        logInfo(QString("组件诊断完成: %1, 耗时: %2ms, 结果: %3")
                .arg(component.reference)
                .arg(timer.elapsed())
                .arg(result.isPassed ? "通过" : "失败"));
        
    } catch (const std::exception& e) {
        result.isPassed = false;
        result.healthScore = 0.0;
        result.confidence = 0.0;
        result.summary = QString("诊断过程异常: %1").arg(e.what());
        result.faultTypes.append("DIAGNOSIS_ERROR");
        result.notes = e.what();
        
        logError(QString("组件诊断异常: %1 - %2").arg(component.reference).arg(e.what()));
        emit diagnosisError(currentComponentId_, e.what());
    }
    
    // 发出诊断完成信号
    emit diagnosisCompleted(currentComponentId_, result);
    
    return result;
}

bool BaseComponentDiagnostic::validateComponentSpec(const ComponentSpec& component) const
{
    if (component.reference.isEmpty()) {
        logError("组件标识不能为空");
        return false;
    }
    
    if (component.type == ComponentType::UNKNOWN) {
        logError("未知组件类型");
        return false;
    }
    
    return true;
}

bool BaseComponentDiagnostic::validateTestData(const TestData& testData) const
{
    if (testData.testId.isEmpty()) {
        logError("测试数据ID为空");
        return false;
    }
    
    if (testData.measurements.isEmpty()) {
        logError("测试数据为空");
        return false;
    }
    
    if (!testData.valid) {
        logError(QString("测试数据无效: %1").arg(testData.errorMessage));
        return false;
    }
    
    return true;
}

QString BaseComponentDiagnostic::generateTestReport(const ComponentDiagnosticResult& result) const
{
    QString report;
    QTextStream stream(&report);
    
    stream << "=== 组件诊断报告 ===" << Qt::endl;
    stream << "组件ID: " << result.componentId << Qt::endl;
    stream << "组件类型: " << result.componentType << Qt::endl;
    stream << "测试时间: " << result.timestamp.toString("yyyy-MM-dd hh:mm:ss") << Qt::endl;
    stream << "测试结果: " << (result.isPassed ? "通过" : "失败") << Qt::endl;
    stream << "健康评分: " << formatPercentage(result.healthScore) << Qt::endl;
    stream << "置信度: " << formatPercentage(result.confidence * 100) << Qt::endl;
    
    if (!result.faultTypes.isEmpty()) {
        stream << "故障类型: " << result.faultTypes.join(", ") << Qt::endl;
    }
    
    if (!result.measurements.isEmpty()) {
        stream << Qt::endl << "=== 测量数据 ===" << Qt::endl;
        for (auto it = result.measurements.begin(); it != result.measurements.end(); ++it) {
            stream << it.key() << ": " << formatValue(it.value()) << Qt::endl;
        }
    }
    
    if (!result.summary.isEmpty()) {
        stream << Qt::endl << "=== 分析总结 ===" << Qt::endl;
        stream << result.summary << Qt::endl;
    }
    
    if (!result.recommendations.isEmpty()) {
        stream << Qt::endl << "=== 建议措施 ===" << Qt::endl;
        for (const QString& recommendation : result.recommendations) {
            stream << "- " << recommendation << Qt::endl;
        }
    }
    
    if (!result.notes.isEmpty()) {
        stream << Qt::endl << "=== 备注 ===" << Qt::endl;
        stream << result.notes << Qt::endl;
    }
    
    return report;
}

QString BaseComponentDiagnostic::formatValue(double value, const QString& unit) const
{
    QString formatted;
    
    if (qAbs(value) >= 1e9) {
        formatted = QString::number(value / 1e9, 'f', 3) + "G";
    } else if (qAbs(value) >= 1e6) {
        formatted = QString::number(value / 1e6, 'f', 3) + "M";
    } else if (qAbs(value) >= 1e3) {
        formatted = QString::number(value / 1e3, 'f', 3) + "k";
    } else if (qAbs(value) >= 1) {
        formatted = QString::number(value, 'f', 3);
    } else if (qAbs(value) >= 1e-3) {
        formatted = QString::number(value * 1e3, 'f', 3) + "m";
    } else if (qAbs(value) >= 1e-6) {
        formatted = QString::number(value * 1e6, 'f', 3) + "µ";
    } else if (qAbs(value) >= 1e-9) {
        formatted = QString::number(value * 1e9, 'f', 3) + "n";
    } else {
        formatted = QString::number(value, 'e', 3);
    }
    
    if (!unit.isEmpty()) {
        formatted += unit;
    }
    
    return formatted;
}

QString BaseComponentDiagnostic::formatPercentage(double value) const
{
    return QString::number(value, 'f', 1) + "%";
}

bool BaseComponentDiagnostic::isWithinTolerance(double measured, double expected, double tolerancePercent) const
{
    if (expected == 0.0) {
        return qAbs(measured) <= (tolerancePercent / 100.0);
    }
    
    double tolerance = qAbs(expected * tolerancePercent / 100.0);
    return qAbs(measured - expected) <= tolerance;
}

double BaseComponentDiagnostic::calculateDeviation(double measured, double expected) const
{
    if (expected == 0.0) {
        return measured == 0.0 ? 0.0 : 100.0;
    }
    
    return ((measured - expected) / expected) * 100.0;
}

double BaseComponentDiagnostic::calculateHealthScore(const QMap<QString, double>& measurements,
                                                   const ComponentSpec& component) const
{
    if (measurements.isEmpty()) {
        return 0.0;
    }
    
    double totalScore = 0.0;
    int validMeasurements = 0;
    
    for (auto it = measurements.begin(); it != measurements.end(); ++it) {
        double measured = it.value();
        double expected = component.nominal_value;  // 简化处理，实际应根据参数名确定期望值
        
        if (isWithinTolerance(measured, expected, component.tolerance_percent)) {
            double deviation = calculateDeviation(measured, expected);
            double score = qMax(0.0, 100.0 - deviation * 2); // 偏差越小分数越高
            totalScore += score;
        } else {
            totalScore += 0.0; // 超出容差得0分
        }
        
        validMeasurements++;
    }
    
    return validMeasurements > 0 ? totalScore / validMeasurements : 0.0;
}

void BaseComponentDiagnostic::logInfo(const QString& message) const
{
    qInfo() << QString("[%1] %2").arg(getComponentTypeName()).arg(message);
}

void BaseComponentDiagnostic::logWarning(const QString& message) const
{
    qWarning() << QString("[%1] %2").arg(getComponentTypeName()).arg(message);
}

void BaseComponentDiagnostic::logError(const QString& message) const
{
    qDebug() << QString("[%1] %2").arg(getComponentTypeName()).arg(message);
}

BaseComponentDiagnostic* BaseComponentDiagnostic::createDiagnostic(ComponentType type, DeviceManager* deviceManager, QObject* parent)
{
    return ComponentDiagnosticFactory::create(type, deviceManager, parent);
}

// === ComponentDiagnosticFactory 实现 ===

BaseComponentDiagnostic* ComponentDiagnosticFactory::create(ComponentType type, DeviceManager* deviceManager, QObject* parent)
{
    auto it = creators_.find(type);
    if (it != creators_.end()) {
        BaseComponentDiagnostic* diagnostic = it.value()();
        if (diagnostic) {
            diagnostic->setParent(parent);
            // 这里需要设置设备管理器，但基类构造函数已经处理了
            return diagnostic;
        }
    }
    
    qWarning() << "未找到组件类型的诊断器:" << static_cast<int>(type);
    return nullptr;
}

QVector<ComponentType> ComponentDiagnosticFactory::getSupportedTypes()
{
    QVector<ComponentType> types;
    for (auto it = creators_.begin(); it != creators_.end(); ++it) {
        types.append(it.key());
    }
    return types;
}
