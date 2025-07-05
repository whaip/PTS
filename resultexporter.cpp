#include "resultexporter.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamWriter>
#include <QDir>
#include <QDebug>
#include <QApplication>
#include <QtMath>

ResultExporter::ResultExporter(QObject *parent)
    : QObject(parent)
{
}

bool ResultExporter::exportToCSV(const QList<DiagnosticResult>& results,
                                 const QString& filePath,
                                 const ExportOptions& options)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFailed(QString("无法创建CSV文件: %1").arg(filePath));
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // CSV头部
    QStringList headers;
    headers << "Component Type"
            << "Component ID"
            << "Result"
            << "Health Score"
            << "Confidence"
            << "Fault Types";
    if (options.includeTimestamp) {
        headers << "Timestamp";
    }
    headers << "Measurements"
            << "Summary"
            << "Recommendations";

    out << headers.join(options.delimiter) << "\n";

    int total = results.size();
    for (int i = 0; i < total; ++i) {
        const DiagnosticResult& r = results[i];
        QStringList row;

        // 基本信息
        row << escapeCSVField(r.componentType, options.delimiter)
            << escapeCSVField(r.componentId, options.delimiter)
            << (r.MetaResult.isPassed ? "PASS" : "FAIL")
            << QString::number(r.healthScore, 'f', 2)
            << QString::number(r.confidence, 'f', 2)
            << escapeCSVField(r.MetaResult.faultTypes.join("; "), options.delimiter);

        if (options.includeTimestamp) {
            row << r.timestamp.toString(options.dateFormat);
        }

        // 测量结果
        QStringList measList;
        for (auto it = r.MetaResult.measurements.constBegin(); it != r.MetaResult.measurements.constEnd(); ++it) {
            measList.append(QString("%1:%2").arg(it.key(), QString::number(it.value())));
        }
        row << escapeCSVField(measList.join("; \n"), options.delimiter);

        // 分析总结与建议
        row << escapeCSVField(r.MetaResult.summary, options.delimiter)
            << escapeCSVField(r.MetaResult.recommendations.join("; "), options.delimiter);

        out << row.join(options.delimiter) << "\n";

        emit exportProgress((i + 1) * 100 / total);
    }

    file.close();
    emit exportCompleted(filePath);
    qDebug() << "CSV export completed:" << filePath;
    return true;
}

bool ResultExporter::exportToJSON(const QList<DiagnosticResult>& results,
                                  const QString& filePath)
{
    QJsonObject root;
    root["export_info"] = QJsonObject{
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)},
        {"version", QApplication::applicationVersion()},
        {"total_results", results.size()}
    };
    
    QJsonArray resultsArray;
    for (const DiagnosticResult& result : results) {
        resultsArray.append(resultToJson(result));
    }
    root["results"] = resultsArray;
    
    // 添加统计信息
    Statistics stats = calculateStatistics(results);
    QJsonObject statsObj;
    statsObj["total_tests"] = stats.totalTests;
    statsObj["passed_tests"] = stats.passedTests;
    statsObj["failed_tests"] = stats.failedTests;
    statsObj["error_tests"] = stats.errorTests;
    statsObj["pass_rate"] = stats.passRate;
    root["statistics"] = statsObj;
    
    QJsonDocument doc(root);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit exportFailed(QString("Cannot create JSON file: %1").arg(filePath));
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    emit exportCompleted(filePath);
    qDebug() << "JSON export completed:" << filePath;
    return true;
}

QString ResultExporter::escapeCSVField(const QString& field, const QString& delimiter)
{
    QString escaped = field;
    
    // 如果字段包含分隔符、引号或换行符，需要用引号包围
    if (field.contains(delimiter) || field.contains("\"") || field.contains("\n")) {
        escaped.replace("\"", "\"\""); // 转义引号
        escaped = "\"" + escaped + "\"";
    }
    
    return escaped;
}

QJsonObject ResultExporter::resultToJson(const DiagnosticResult& result)
{
    QJsonObject json;
    
    // 基本信息
    json["component_type"] = result.componentType;
    json["component_id"] = result.componentId;
    
    // 检测结果
    json["result"] = result.MetaResult.isPassed ? "PASS" : "FAIL";
    
    // 分数与置信度
    json["health_score"] = result.healthScore;
    json["confidence"] = result.confidence;
    
    // 故障类型
    QJsonArray faultArray;
    for (const QString& fault : result.MetaResult.faultTypes) {
        faultArray.append(fault);
    }
    json["fault_types"] = faultArray;
    
    // 时间戳
    json["timestamp"] = result.timestamp.toString(Qt::ISODate);
    
    // 测量结果
    QJsonObject measObj;
    for (auto it = result.MetaResult.measurements.constBegin(); it != result.MetaResult.measurements.constEnd(); ++it) {
        measObj[it.key()] = it.value();
    }
    json["measurements"] = measObj;
    
    // 分析总结
    json["summary"] = result.MetaResult.summary;
    
    // 建议
    QJsonArray recArray;
    for (const QString& rec : result.MetaResult.recommendations) {
        recArray.append(rec);
    }
    json["recommendations"] = recArray;
    
    return json;
}

ResultExporter::Statistics ResultExporter::calculateStatistics(const QList<DiagnosticResult>& results)
{
    Statistics stats;
    
    stats.totalTests = results.size();
    if (stats.totalTests == 0) {
        return stats;
    }
    
    stats.firstTest = results.first().timestamp;
    stats.lastTest = results.first().timestamp;
    
    for (const DiagnosticResult& result : results) {
        // 统计结果类型
        switch (result.result) {
            case DiagnosticResult::TestResult::PASS:
                stats.passedTests++;
                break;
            case DiagnosticResult::TestResult::FAIL:
                stats.failedTests++;
                break;
        }
        
        // 统计组件类型
        stats.componentTypeCounts[result.componentType]++;
        
        // 统计故障类型
        for (const QString& fault : result.MetaResult.faultTypes) {
            stats.faultTypeCounts[fault]++;
        }
        
        // 更新时间范围
        if (result.timestamp < stats.firstTest) {
            stats.firstTest = result.timestamp;
        }
        if (result.timestamp > stats.lastTest) {
            stats.lastTest = result.timestamp;
        }
    }
    
    stats.passRate = 100.0 * stats.passedTests / stats.totalTests;
    
    return stats;
}
