#ifndef RESULTEXPORTER_H
#define RESULTEXPORTER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>
#include "faultdiagnostic.h"

struct ExportOptions {
    bool includeTimestamp = true;
    bool includeDeviceInfo = true;
    bool includeRawData = false;
    bool includeStatistics = true;
    QString delimiter = ",";
    QString encoding = "UTF-8";
    QString dateFormat = "yyyy-MM-dd hh:mm:ss";
};

class ResultExporter : public QObject
{
    Q_OBJECT

public:
    explicit ResultExporter(QObject *parent = nullptr);
    
    // 导出功能
    bool exportToCSV(const QList<DiagnosticResult>& results, 
                     const QString& filePath, 
                     const ExportOptions& options = ExportOptions());
    
    bool exportToJSON(const QList<DiagnosticResult>& results, 
                      const QString& filePath);

signals:
    void exportProgress(int percentage);
    void exportCompleted(const QString& filePath);
    void exportFailed(const QString& error);

private:    // CSV导出辅助函数
    QString escapeCSVField(const QString& field, const QString& delimiter);
    
    // JSON/XML辅助函数
    QJsonObject resultToJson(const DiagnosticResult& result);
    
    // 统计计算
    struct Statistics {
        int totalTests = 0;
        int passedTests = 0;
        int failedTests = 0;
        int errorTests = 0;
        double passRate = 0.0;
        QDateTime firstTest;
        QDateTime lastTest;
        QMap<QString, int> componentTypeCounts;
        QMap<QString, int> faultTypeCounts;
    };
    
    Statistics calculateStatistics(const QList<DiagnosticResult>& results);
};

#endif // RESULTEXPORTER_H
