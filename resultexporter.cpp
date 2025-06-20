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
        emit exportFailed(QString("Cannot create CSV file: %1").arg(filePath));
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // CSV头部
    QStringList headers;
    headers << "Test ID" << "Component Type" << "Component ID" << "Test Result" 
            << "Health Score" << "Confidence" << "Fault Types";
    
    if (options.includeTimestamp) {
        headers << "Test Time";
    }
    
    if (options.includeDeviceInfo) {
        headers << "Test Equipment";
    }
    
    headers << "Measured Values" << "Expected Values" << "Tolerance" << "Notes";
    
    out << headers.join(options.delimiter) << "\n";
    
    // 写入数据
    int totalResults = results.size();
    for (int i = 0; i < totalResults; ++i) {
        const DiagnosticResult& result = results[i];
        
        QStringList row;
        row << escapeCSVField(result.testId, options.delimiter);
        row << escapeCSVField(result.componentType, options.delimiter);
        row << escapeCSVField(result.componentId, options.delimiter);
        
        // 测试结果
        QString testResult;
        switch (result.result) {
            case DiagnosticResult::PASS: testResult = "PASS"; break;
            case DiagnosticResult::FAIL: testResult = "FAIL"; break;
            case DiagnosticResult::ERROR: testResult = "ERROR"; break;
            default: testResult = "UNKNOWN"; break;
        }
        row << testResult;
        
        row << QString::number(result.healthScore, 'f', 2);
        row << QString::number(result.confidence, 'f', 2);
        
        // 故障类型
        QStringList faultTypes;
        for (const auto& fault : result.faultTypes) {
            faultTypes << fault;
        }
        row << escapeCSVField(faultTypes.join("; "), options.delimiter);
        
        if (options.includeTimestamp) {
            row << result.timestamp.toString(options.dateFormat);
        }
        
        if (options.includeDeviceInfo) {
            row << escapeCSVField(result.testEquipment, options.delimiter);
        }
        
        // 测量值和期望值
        row << formatMeasurementDataForCSV(result.measurementData, options);
        row << QString::number(result.expectedValue, 'g', 6);
        row << QString::number(result.tolerance, 'g', 6);
        row << escapeCSVField(result.notes, options.delimiter);
        
        out << row.join(options.delimiter) << "\n";
        
        // 更新进度
        int progress = (i + 1) * 100 / totalResults;
        emit exportProgress(progress);
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

bool ResultExporter::exportToHTML(const QList<DiagnosticResult>& results,
                                  const QString& filePath,
                                  const QString& title)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFailed(QString("Cannot create HTML file: %1").arg(filePath));
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    out << generateHTMLHeader(title);
    out << generateHTMLSummary(results);
    out << generateHTMLTable(results);
    out << generateHTMLFooter();
    
    file.close();
    emit exportCompleted(filePath);
    qDebug() << "HTML export completed:" << filePath;
    return true;
}

bool ResultExporter::generateStatisticsReport(const QList<DiagnosticResult>& results,
                                              const QString& filePath)
{
    Statistics stats = calculateStatistics(results);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFailed(QString("Cannot create statistics report: %1").arg(filePath));
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    out << "===============================================\n";
    out << "      PCB Component Test Statistics Report\n";
    out << "===============================================\n\n";
    
    out << "Report Generated: " << QDateTime::currentDateTime().toString() << "\n\n";
    
    out << "OVERALL RESULTS:\n";
    out << "----------------\n";
    out << QString("Total Tests: %1\n").arg(stats.totalTests);
    out << QString("Passed: %1 (%.1f%%)\n").arg(stats.passedTests).arg(stats.passRate);
    out << QString("Failed: %1 (%.1f%%)\n").arg(stats.failedTests)
           .arg(stats.totalTests > 0 ? 100.0 * stats.failedTests / stats.totalTests : 0.0);
    out << QString("Errors: %1 (%.1f%%)\n").arg(stats.errorTests)
           .arg(stats.totalTests > 0 ? 100.0 * stats.errorTests / stats.totalTests : 0.0);
    out << "\n";
    
    if (stats.totalTests > 0) {
        out << "TEST PERIOD:\n";
        out << "------------\n";
        out << QString("First Test: %1\n").arg(stats.firstTest.toString());
        out << QString("Last Test: %1\n").arg(stats.lastTest.toString());
        out << QString("Duration: %1 hours\n")
               .arg(stats.firstTest.secsTo(stats.lastTest) / 3600.0, 0, 'f', 2);
        out << "\n";
    }
    
    if (!stats.componentTypeCounts.isEmpty()) {
        out << "COMPONENT TYPE BREAKDOWN:\n";
        out << "-------------------------\n";
        auto it = stats.componentTypeCounts.constBegin();
        while (it != stats.componentTypeCounts.constEnd()) {
            out << QString("%1: %2 tests\n").arg(it.key()).arg(it.value());
            ++it;
        }
        out << "\n";
    }
    
    if (!stats.faultTypeCounts.isEmpty()) {
        out << "FAULT TYPE ANALYSIS:\n";
        out << "--------------------\n";
        auto it = stats.faultTypeCounts.constBegin();
        while (it != stats.faultTypeCounts.constEnd()) {
            out << QString("%1: %2 occurrences\n").arg(it.key()).arg(it.value());
            ++it;
        }
        out << "\n";
    }
    
    file.close();
    emit exportCompleted(filePath);
    qDebug() << "Statistics report completed:" << filePath;
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

QString ResultExporter::formatMeasurementDataForCSV(const MeasurementResult& data, 
                                                   const ExportOptions& options)
{
    QStringList values;
    
    if (data.voltage != 0.0) {
        values << QString("V=%1").arg(data.voltage, 0, 'g', 6);
    }
    if (data.current != 0.0) {
        values << QString("I=%1").arg(data.current, 0, 'g', 6);
    }    if (data.primary_value != 0.0) {
        values << QString("Primary=%1").arg(data.primary_value, 0, 'g', 6);
    }
    if (data.power != 0.0) {
        values << QString("P=%1W").arg(data.power, 0, 'g', 6);
    }
    if (data.esr != 0.0) {
        values << QString("ESR=%1").arg(data.esr, 0, 'g', 6);
    }
    if (data.leakage_current != 0.0) {
        values << QString("IL=%1A").arg(data.leakage_current, 0, 'g', 6);
    }
    if (data.temperature != 25.0) {
        values << QString("T=%1°C").arg(data.temperature, 0, 'f', 1);
    }
    
    return values.join("; ");
}

QJsonObject ResultExporter::resultToJson(const DiagnosticResult& result)
{
    QJsonObject json;
    
    json["test_id"] = result.testId;
    json["component_type"] = result.componentType;
    json["component_id"] = result.componentId;
    
    QString resultStr;
    switch (result.result) {
        case DiagnosticResult::PASS: resultStr = "PASS"; break;
        case DiagnosticResult::FAIL: resultStr = "FAIL"; break;
        case DiagnosticResult::ERROR: resultStr = "ERROR"; break;
    }
    json["result"] = resultStr;
    
    json["health_score"] = result.healthScore;
    json["confidence"] = result.confidence;
    json["timestamp"] = result.timestamp.toString(Qt::ISODate);
    json["test_equipment"] = result.testEquipment;
    json["expected_value"] = result.expectedValue;
    json["tolerance"] = result.tolerance;
    json["notes"] = result.notes;
    
    QJsonArray faultArray;
    for (const QString& fault : result.faultTypes) {
        faultArray.append(fault);
    }
    json["fault_types"] = faultArray;
    
    json["measurement_data"] = measurementDataToJson(result.measurementData);
    
    return json;
}

QJsonObject ResultExporter::measurementDataToJson(const MeasurementResult& data)
{
    QJsonObject json;
    
    json["primary_value"] = data.primary_value;
    json["voltage"] = data.voltage;
    json["current"] = data.current;
    json["power"] = data.power;
    json["temperature"] = data.temperature;
    json["esr"] = data.esr;
    json["leakage_current"] = data.leakage_current;
    json["valid"] = data.valid;
    json["error_message"] = data.error_message;
    
    return json;
}

QString ResultExporter::generateHTMLHeader(const QString& title)
{
    return QString(R"(<!DOCTYPE html>
<html>
<head>
    <title>%1</title>
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f4f4f4; padding: 10px; border-left: 4px solid #2196F3; }
        .summary { background-color: #e8f5e8; padding: 15px; margin: 20px 0; border-radius: 5px; }
        table { border-collapse: collapse; width: 100%%; margin-top: 20px; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; font-weight: bold; }
        .pass { background-color: #d4edda; }
        .fail { background-color: #f8d7da; }
        .error { background-color: #fff3cd; }
        .footer { margin-top: 30px; font-size: 12px; color: #666; }
    </style>
</head>
<body>
    <div class="header">
        <h1>%1</h1>
        <p>Generated on: %2</p>
    </div>
)").arg(title).arg(QDateTime::currentDateTime().toString());
}

QString ResultExporter::generateHTMLSummary(const QList<DiagnosticResult>& results)
{
    Statistics stats = calculateStatistics(results);
    
    return QString(R"(<div class="summary">
        <h2>Test Summary</h2>
        <p><strong>Total Tests:</strong> %1</p>
        <p><strong>Passed:</strong> %2 (%.1f%%)</p>
        <p><strong>Failed:</strong> %3</p>
        <p><strong>Errors:</strong> %4</p>
        <p><strong>Overall Pass Rate:</strong> %.1f%%</p>
    </div>
)").arg(stats.totalTests)
   .arg(stats.passedTests).arg(stats.passRate)
   .arg(stats.failedTests)
   .arg(stats.errorTests)
   .arg(stats.passRate);
}

QString ResultExporter::generateHTMLTable(const QList<DiagnosticResult>& results)
{
    QString html = R"(<table>
        <tr>
            <th>Test ID</th>
            <th>Component</th>
            <th>Result</th>
            <th>Health Score</th>
            <th>Faults</th>
            <th>Test Time</th>
        </tr>
)";
    
    for (const DiagnosticResult& result : results) {
        QString rowClass;
        QString resultText;
        
        switch (result.result) {
            case DiagnosticResult::PASS:
                rowClass = "pass";
                resultText = "PASS";
                break;
            case DiagnosticResult::FAIL:
                rowClass = "fail";
                resultText = "FAIL";
                break;
            case DiagnosticResult::ERROR:
                rowClass = "error";
                resultText = "ERROR";
                break;
        }
        
        html += QString(R"(        <tr class="%1">
            <td>%2</td>
            <td>%3 (%4)</td>
            <td>%5</td>
            <td>%.1f</td>
            <td>%6</td>
            <td>%7</td>
        </tr>
)").arg(rowClass)
   .arg(result.testId)
   .arg(result.componentType).arg(result.componentId)
   .arg(resultText)
   .arg(result.healthScore)
   .arg(result.faultTypes.join(", "))
   .arg(result.timestamp.toString());
    }
    
    html += "</table>";
    return html;
}

QString ResultExporter::generateHTMLFooter()
{
    return QString(R"(    <div class="footer">
        <p>Report generated by PCB Fault Detection System v%1</p>
    </div>
</body>
</html>
)").arg(QApplication::applicationVersion());
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
            case DiagnosticResult::PASS:
                stats.passedTests++;
                break;
            case DiagnosticResult::FAIL:
                stats.failedTests++;
                break;
            case DiagnosticResult::ERROR:
                stats.errorTests++;
                break;
        }
        
        // 统计组件类型
        stats.componentTypeCounts[result.componentType]++;
        
        // 统计故障类型
        for (const QString& fault : result.faultTypes) {
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
