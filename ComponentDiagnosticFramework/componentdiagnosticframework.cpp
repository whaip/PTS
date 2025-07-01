#include "componentdiagnosticframework.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>

namespace ComponentDiagnosticFramework {

// === 静态私有辅助函数声明 ===
static QString generateHtmlReport(const QVector<ComponentDiagnosticResult>& results);
static QString generateCsvReport(const QVector<ComponentDiagnosticResult>& results);
static QString generateTextReport(const QVector<ComponentDiagnosticResult>& results);

ComponentDiagnosticManager* createDiagnosticManager(DeviceManager* deviceManager, QObject* parent)
{
    if (!deviceManager) {
        qCritical() << "ComponentDiagnosticFramework: DeviceManager不能为空";
        return nullptr;
    }
    
    qInfo() << "ComponentDiagnosticFramework: 创建ComponentDiagnosticManager...";
    auto manager = new ComponentDiagnosticManager(deviceManager, parent);
    
    qInfo() << "ComponentDiagnosticFramework: 初始化ComponentDiagnosticManager...";
    if (!manager->initialize()) {
        qCritical() << "ComponentDiagnosticFramework: 诊断管理器初始化失败";
        delete manager;
        return nullptr;
    }
    
    qInfo() << "ComponentDiagnosticFramework: 诊断管理器初始化成功";
    
    int registeredCount = registerBuiltinDiagnostics(manager, deviceManager);
    
    qInfo() << "ComponentDiagnosticFramework: 诊断管理器创建完成";
    return manager;
}

int registerBuiltinDiagnostics(ComponentDiagnosticManager* manager, DeviceManager* deviceManager)
{
    if (!manager || !deviceManager) {
        return 0;
    }
    
    int count = 0;
    
    // 注册电阻器诊断器
    try {
        auto resistorDiagnostic = new ResistorDiagnostic(deviceManager);
        manager->registerDiagnostic(ComponentType::RESISTOR, resistorDiagnostic);
        count++;
        qInfo() << "已注册电阻器诊断器";
    } catch (const std::exception& e) {
        qWarning() << "注册电阻器诊断器失败:" << e.what();
    }
    
    // 注册电容器诊断器
    try {
        auto capacitorDiagnostic = new CapacitorDiagnostic(deviceManager);
        manager->registerDiagnostic(ComponentType::CAPACITOR, capacitorDiagnostic);
        count++;
        qInfo() << "已注册电容器诊断器";
    } catch (const std::exception& e) {
        qWarning() << "注册电容器诊断器失败:" << e.what();
    }
    
    // 未来可以添加更多诊断器
    /*
    try {
        auto inductorDiagnostic = new InductorDiagnostic(deviceManager);
        manager->registerDiagnostic(ComponentType::INDUCTOR, inductorDiagnostic);
        count++;
        qInfo() << "已注册电感器诊断器";
    } catch (const std::exception& e) {
        qWarning() << "注册电感器诊断器失败:" << e.what();
    }
    
    try {
        auto icDiagnostic = new ICDiagnostic(deviceManager);
        manager->registerDiagnostic(ComponentType::IC, icDiagnostic);
        count++;
        qInfo() << "已注册集成电路诊断器";
    } catch (const std::exception& e) {
        qWarning() << "注册集成电路诊断器失败:" << e.what();
    }
    */
    
    return count;
}

BaseComponentDiagnostic* createComponentDiagnostic(ComponentType type, DeviceManager* deviceManager, QObject* parent)
{
    if (!deviceManager) {
        return nullptr;
    }
    
    switch (type) {
        case ComponentType::RESISTOR:
            return new ResistorDiagnostic(deviceManager, parent);
            
        case ComponentType::CAPACITOR:
            return new CapacitorDiagnostic(deviceManager, parent);
            
        // 未来添加更多组件类型
        /*
        case ComponentType::INDUCTOR:
            return new InductorDiagnostic(deviceManager, parent);
            
        case ComponentType::DIODE:
            return new DiodeDiagnostic(deviceManager, parent);
            
        case ComponentType::IC:
            return new ICDiagnostic(deviceManager, parent);
        */
        
        default:
            qWarning() << "不支持的组件类型:" << static_cast<int>(type);
            return nullptr;
    }
}

QString getComponentTypeDisplayName(ComponentType type)
{
    switch (type) {
        case ComponentType::RESISTOR:
            return "电阻器";
        case ComponentType::CAPACITOR:
            return "电容器";
        case ComponentType::INDUCTOR:
            return "电感器";
        case ComponentType::DIODE:
            return "二极管";
        case ComponentType::TRANSISTOR:
            return "晶体管";
        case ComponentType::IC:
            return "集成电路";
        case ComponentType::UNKNOWN:
        default:
            return "未知组件";
    }
}

QVector<ComponentType> getAllSupportedComponentTypes()
{
    // 返回当前实现的组件类型
    return QVector<ComponentType>() 
           << ComponentType::RESISTOR 
           << ComponentType::CAPACITOR;
           // 未来添加: << ComponentType::INDUCTOR << ComponentType::DIODE << ComponentType::IC;
}

bool validateComponentSpec(const ComponentSpec& component, QStringList& errors)
{
    errors.clear();
    
    // 检查基本字段
    if (component.reference.isEmpty()) {
        errors.append("组件标识不能为空");
    }
    
    if (component.type == ComponentType::UNKNOWN) {
        errors.append("组件类型不能为未知");
    }
    
    if (component.nominal_value <= 0) {
        errors.append("标称值必须大于0");
    }
    
    if (component.tolerance_percent < 0 || component.tolerance_percent > 100) {
        errors.append("容差百分比必须在0-100之间");
    }
    
    // 根据组件类型进行特定验证
    switch (component.type) {
        case ComponentType::RESISTOR:
            if (component.nominal_value > 1e8) {
                errors.append("电阻值不能超过100MΩ");
            }
            break;
            
        case ComponentType::CAPACITOR:
            if (component.nominal_value > 1.0) {
                errors.append("电容值不能超过1F");
            }
            if (component.max_esr < 0) {
                errors.append("最大ESR不能为负值");
            }
            if (component.max_leakage < 0) {
                errors.append("最大漏电流不能为负值");
            }
            break;
            
        // 其他组件类型的验证...
        default:
            break;
    }
    
    return errors.isEmpty();
}

ComponentSpec createDefaultComponentSpec(ComponentType type, const QString& reference, 
                                        double nominalValue, double tolerance)
{
    ComponentSpec spec;
    spec.reference = reference;
    spec.type = type;
    spec.nominal_value = nominalValue;
    spec.tolerance_percent = tolerance;
    spec.requiresTesting = true;
    spec.description = QString("%1 - %2").arg(getComponentTypeDisplayName(type)).arg(reference);
    
    // 根据组件类型设置默认参数
    switch (type) {
        case ComponentType::RESISTOR:
            spec.test_voltage = 1.0;
            spec.test_current = 0.001;
            spec.max_voltage = 50.0;
            spec.max_current = 0.1;
            break;
            
        case ComponentType::CAPACITOR:
            spec.test_voltage = 1.0;
            spec.max_voltage = 50.0;
            spec.max_esr = 10.0;
            spec.max_leakage = 1e-9;
            break;
            
        // 其他组件类型的默认参数...
        default:
            break;
    }
    
    return spec;
}

QString formatDiagnosticResult(const ComponentDiagnosticResult& result, bool detailed)
{
    QString formatted;
    QTextStream stream(&formatted);
    
    // 基本信息
    stream << QString("=== %1 诊断结果 ===").arg(result.componentId) << Qt::endl;
    stream << QString("组件类型: %1").arg(result.componentType) << Qt::endl;
    stream << QString("测试时间: %1").arg(result.timestamp.toString("yyyy-MM-dd hh:mm:ss")) << Qt::endl;
    stream << QString("测试结果: %1").arg(result.isPassed ? "✓ 通过" : "✗ 失败") << Qt::endl;
    stream << QString("健康评分: %1/100").arg(result.healthScore, 0, 'f', 1) << Qt::endl;
    stream << QString("置信度: %1%").arg(result.confidence * 100, 0, 'f', 1) << Qt::endl;
    
    if (!result.faultTypes.isEmpty()) {
        stream << QString("故障类型: %1").arg(result.faultTypes.join(", ")) << Qt::endl;
    }
    
    if (detailed) {
        // 详细测量数据
        if (!result.measurements.isEmpty()) {
            stream << Qt::endl << "=== 测量数据 ===" << Qt::endl;
            for (auto it = result.measurements.begin(); it != result.measurements.end(); ++it) {
                stream << QString("%1: %2").arg(it.key()).arg(QString::number(it.value())) << Qt::endl;
            }
        }
        
        // 分析数据
        if (!result.analysisData.isEmpty()) {
            stream << Qt::endl << "=== 分析数据 ===" << Qt::endl;
            for (auto it = result.analysisData.begin(); it != result.analysisData.end(); ++it) {
                stream << QString("%1: %2").arg(it.key()).arg(it.value().toString()) << Qt::endl;
            }
        }
        
        // 建议
        if (!result.recommendations.isEmpty()) {
            stream << Qt::endl << "=== 建议措施 ===" << Qt::endl;
            for (const QString& recommendation : result.recommendations) {
                stream << QString("• %1").arg(recommendation) << Qt::endl;
            }
        }
    }
    
    if (!result.summary.isEmpty()) {
        stream << Qt::endl << "=== 总结 ===" << Qt::endl;
        stream << result.summary << Qt::endl;
    }
    
    if (!result.notes.isEmpty()) {
        stream << Qt::endl << "=== 备注 ===" << Qt::endl;
        stream << result.notes << Qt::endl;
    }
    
    return formatted;
}

QString exportResultToJson(const ComponentDiagnosticResult& result)
{
    QJsonObject jsonObj;
    
    jsonObj["componentId"] = result.componentId;
    jsonObj["componentType"] = result.componentType;
    jsonObj["isPassed"] = result.isPassed;
    jsonObj["healthScore"] = result.healthScore;
    jsonObj["confidence"] = result.confidence;
    jsonObj["timestamp"] = result.timestamp.toString(Qt::ISODate);
    jsonObj["summary"] = result.summary;
    jsonObj["notes"] = result.notes;
    
    // 故障类型数组
    QJsonArray faultTypesArray;
    for (const QString& faultType : result.faultTypes) {
        faultTypesArray.append(faultType);
    }
    jsonObj["faultTypes"] = faultTypesArray;
    
    // 测量数据
    QJsonObject measurementsObj;
    for (auto it = result.measurements.begin(); it != result.measurements.end(); ++it) {
        measurementsObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    jsonObj["measurements"] = measurementsObj;
    
    // 分析数据
    QJsonObject analysisDataObj;
    for (auto it = result.analysisData.begin(); it != result.analysisData.end(); ++it) {
        analysisDataObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    jsonObj["analysisData"] = analysisDataObj;
    
    // 建议数组
    QJsonArray recommendationsArray;
    for (const QString& recommendation : result.recommendations) {
        recommendationsArray.append(recommendation);
    }
    jsonObj["recommendations"] = recommendationsArray;
    
    QJsonDocument doc(jsonObj);
    return doc.toJson(QJsonDocument::Compact);
}

bool importResultFromJson(const QString& jsonString, ComponentDiagnosticResult& result)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON解析错误:" << error.errorString();
        return false;
    }
    
    QJsonObject jsonObj = doc.object();
    
    result.componentId = jsonObj["componentId"].toString();
    result.componentType = jsonObj["componentType"].toString();
    result.isPassed = jsonObj["isPassed"].toBool();
    result.healthScore = jsonObj["healthScore"].toDouble();
    result.confidence = jsonObj["confidence"].toDouble();
    result.timestamp = QDateTime::fromString(jsonObj["timestamp"].toString(), Qt::ISODate);
    result.summary = jsonObj["summary"].toString();
    result.notes = jsonObj["notes"].toString();
    
    // 故障类型
    result.faultTypes.clear();
    QJsonArray faultTypesArray = jsonObj["faultTypes"].toArray();
    for (const QJsonValue& value : faultTypesArray) {
        result.faultTypes.append(value.toString());
    }
    
    // 测量数据
    result.measurements.clear();
    QJsonObject measurementsObj = jsonObj["measurements"].toObject();
    for (auto it = measurementsObj.begin(); it != measurementsObj.end(); ++it) {
        result.measurements[it.key()] = it.value().toDouble();
    }
    
    // 分析数据
    result.analysisData.clear();
    QJsonObject analysisDataObj = jsonObj["analysisData"].toObject();
    for (auto it = analysisDataObj.begin(); it != analysisDataObj.end(); ++it) {
        result.analysisData[it.key()] = it.value().toVariant();
    }
    
    // 建议
    result.recommendations.clear();
    QJsonArray recommendationsArray = jsonObj["recommendations"].toArray();
    for (const QJsonValue& value : recommendationsArray) {
        result.recommendations.append(value.toString());
    }
    
    return true;
}

QString generateDiagnosticReport(const QVector<ComponentDiagnosticResult>& results, const QString& format)
{
    if (format.toLower() == "html") {
        return generateHtmlReport(results);
    } else if (format.toLower() == "csv") {
        return generateCsvReport(results);
    } else {
        return generateTextReport(results);
    }
}

QMap<QString, QVariant> getRecommendedTestConfig(const ComponentSpec& component)
{
    QMap<QString, QVariant> config;
    
    // 通用配置
    config["timeout"] = 30000; // 30秒
    config["measurement_points"] = 10;
    config["settling_time"] = 100; // 100ms
    
    // 根据组件类型设置特定配置
    switch (component.type) {
        case ComponentType::RESISTOR:
            config["use_4wire_method"] = false;
            config["test_current"] = 0.001; // 1mA
            config["max_voltage"] = 10.0;
            break;
            
        case ComponentType::CAPACITOR:
            config["test_voltage"] = 1.0;
            config["measure_leakage"] = true;
            config["leakage_test_voltage"] = qMin(component.max_voltage * 0.8, 10.0);
            
            // 根据容值选择测试频率
            if (component.nominal_value >= 1e-3) {
                config["test_frequencies"] = QVariantList() << 10 << 100 << 1000;
            } else if (component.nominal_value >= 1e-6) {
                config["test_frequencies"] = QVariantList() << 100 << 1000 << 10000;
            } else {
                config["test_frequencies"] = QVariantList() << 1000 << 10000 << 100000;
            }
            break;
            
        default:
            break;
    }
    
    return config;
}

double calculateDiagnosticConfidence(const ComponentDiagnosticResult& result)
{
    if (!result.isPassed) {
        return result.confidence; // 失败情况下直接返回原始置信度
    }
    
    double confidence = result.confidence;
    
    // 根据健康评分调整置信度
    if (result.healthScore >= 95.0) {
        confidence = qMin(confidence * 1.1, 1.0); // 提高置信度
    } else if (result.healthScore < 70.0) {
        confidence *= 0.9; // 降低置信度
    }
    
    // 根据故障类型数量调整
    if (result.faultTypes.isEmpty()) {
        confidence = qMin(confidence * 1.05, 1.0);
    } else if (result.faultTypes.size() > 2) {
        confidence *= 0.8;
    }
    
    return qBound(0.0, confidence, 1.0);
}

FrameworkInfo getFrameworkInfo()
{
    FrameworkInfo info;
    info.version = DIAGNOSTIC_FRAMEWORK_VERSION;
    info.buildDate = DIAGNOSTIC_FRAMEWORK_BUILD_DATE;
    info.description = "组件诊断框架 - 基于虚函数和多态的可扩展故障诊断系统";
    
    // 获取支持的组件类型
    auto supportedTypes = getAllSupportedComponentTypes();
    for (ComponentType type : supportedTypes) {
        info.supportedComponents.append(getComponentTypeDisplayName(type));
    }
    
    return info;
}

QString getErrorDescription(ErrorCode code)
{
    switch (code) {
        case ErrorCode::SUCCESS:
            return "操作成功";
        case ErrorCode::INVALID_COMPONENT_SPEC:
            return "无效的组件规格";
        case ErrorCode::UNSUPPORTED_COMPONENT_TYPE:
            return "不支持的组件类型";
        case ErrorCode::DEVICE_NOT_READY:
            return "设备未就绪";
        case ErrorCode::PORT_ALLOCATION_FAILED:
            return "端口分配失败";
        case ErrorCode::WIRING_SETUP_FAILED:
            return "接线设置失败";
        case ErrorCode::DATA_ACQUISITION_FAILED:
            return "数据采集失败";
        case ErrorCode::ANALYSIS_FAILED:
            return "数据分析失败";
        case ErrorCode::TIMEOUT:
            return "操作超时";
        case ErrorCode::CANCELLED:
            return "操作被取消";
        case ErrorCode::INTERNAL_ERROR:
            return "内部错误";
        default:
            return "未知错误";
    }
}

// === 静态私有辅助函数实现 ===

static QString generateHtmlReport(const QVector<ComponentDiagnosticResult>& results)
{
    QString html;
    QTextStream stream(&html);
    
    stream << "<!DOCTYPE html>" << Qt::endl;
    stream << "<html><head>" << Qt::endl;
    stream << "<title>组件诊断报告</title>" << Qt::endl;
    stream << "<meta charset='utf-8'>" << Qt::endl;
    stream << "<style>" << Qt::endl;
    stream << "body { font-family: Arial, sans-serif; margin: 20px; }" << Qt::endl;
    stream << "table { border-collapse: collapse; width: 100%; }" << Qt::endl;
    stream << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }" << Qt::endl;
    stream << "th { background-color: #f2f2f2; }" << Qt::endl;
    stream << ".pass { color: green; font-weight: bold; }" << Qt::endl;
    stream << ".fail { color: red; font-weight: bold; }" << Qt::endl;
    stream << "</style>" << Qt::endl;
    stream << "</head><body>" << Qt::endl;
    
    stream << "<h1>组件诊断报告</h1>" << Qt::endl;
    stream << "<p>生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "</p>" << Qt::endl;
    stream << "<p>测试组件数量: " << results.size() << "</p>" << Qt::endl;
    
    // 统计信息
    int passCount = 0;
    for (const auto& result : results) {
        if (result.isPassed) passCount++;
    }
    stream << "<p>通过率: " << (results.isEmpty() ? 0 : passCount * 100 / results.size()) << "%</p>" << Qt::endl;
    
    // 结果表格
    stream << "<table>" << Qt::endl;
    stream << "<tr><th>组件ID</th><th>类型</th><th>结果</th><th>健康度</th><th>故障类型</th><th>备注</th></tr>" << Qt::endl;
    
    for (const auto& result : results) {
        stream << "<tr>" << Qt::endl;
        stream << "<td>" << result.componentId << "</td>" << Qt::endl;
        stream << "<td>" << result.componentType << "</td>" << Qt::endl;
        stream << "<td class='" << (result.isPassed ? "pass" : "fail") << "'>" 
               << (result.isPassed ? "通过" : "失败") << "</td>" << Qt::endl;
        stream << "<td>" << QString::number(result.healthScore, 'f', 1) << "%</td>" << Qt::endl;
        stream << "<td>" << result.faultTypes.join(", ") << "</td>" << Qt::endl;
        stream << "<td>" << result.notes << "</td>" << Qt::endl;
        stream << "</tr>" << Qt::endl;
    }
    
    stream << "</table>" << Qt::endl;
    stream << "</body></html>" << Qt::endl;
    
    return html;
}

static QString generateCsvReport(const QVector<ComponentDiagnosticResult>& results)
{
    QString csv;
    QTextStream stream(&csv);
    
    // CSV标题行
    stream << "组件ID,类型,结果,健康度,置信度,故障类型,测试时间,备注" << Qt::endl;
    
    // 数据行
    for (const auto& result : results) {
        stream << result.componentId << ","
               << result.componentType << ","
               << (result.isPassed ? "通过" : "失败") << ","
               << result.healthScore << ","
               << result.confidence << ","
               << "\"" << result.faultTypes.join("; ") << "\","
               << result.timestamp.toString("yyyy-MM-dd hh:mm:ss") << ","
               << "\"" << result.notes << "\"" << Qt::endl;
    }
    
    return csv;
}

static QString generateTextReport(const QVector<ComponentDiagnosticResult>& results)
{
    QString report;
    QTextStream stream(&report);
    
    stream << "========================================" << Qt::endl;
    stream << "            组件诊断报告" << Qt::endl;
    stream << "========================================" << Qt::endl;
    stream << "生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << Qt::endl;
    stream << "测试组件数量: " << results.size() << Qt::endl;
    
    // 统计信息
    int passCount = 0;
    for (const auto& result : results) {
        if (result.isPassed) passCount++;
    }
    stream << "通过数量: " << passCount << Qt::endl;
    stream << "失败数量: " << (results.size() - passCount) << Qt::endl;
    stream << "通过率: " << (results.isEmpty() ? 0 : passCount * 100 / results.size()) << "%" << Qt::endl;
    stream << Qt::endl;
    
    // 详细结果
    for (int i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        stream << "----------------------------------------" << Qt::endl;
        stream << QString("组件 %1: %2").arg(i+1).arg(result.componentId) << Qt::endl;
        stream << "类型: " << result.componentType << Qt::endl;
        stream << "结果: " << (result.isPassed ? "✓ 通过" : "✗ 失败") << Qt::endl;
        stream << "健康度: " << result.healthScore << "%" << Qt::endl;
        stream << "置信度: " << (result.confidence * 100) << "%" << Qt::endl;
        
        if (!result.faultTypes.isEmpty()) {
            stream << "故障类型: " << result.faultTypes.join(", ") << Qt::endl;
        }
        
        if (!result.summary.isEmpty()) {
            stream << "总结: " << result.summary << Qt::endl;
        }
        
        if (!result.notes.isEmpty()) {
            stream << "备注: " << result.notes << Qt::endl;
        }
        
        stream << Qt::endl;
    }
    
    return report;
}

} // namespace ComponentDiagnosticFramework
