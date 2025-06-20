#ifndef PCBDETECTIONMANAGER_H
#define PCBDETECTIONMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QDir>
#include <QMutex>
#include <opencv2/opencv.hpp>
#include "PCB_Components_Detect/ClassList.h"

// PCB检测记录数据结构
struct PCBDetectionRecord {
    QString id;                    // 唯一标识符
    QString imagePath;             // 原始图像路径
    QString annotatedImagePath;    // 标注图像路径
    QDateTime timestamp;           // 检测时间
    QString pcbModel;              // PCB模型名称
    double pcbConfidence;          // PCB识别置信度
    std::vector<Label> componentLabels;  // 元件标签列表
    int totalComponents;           // 元件总数
    std::map<std::string, int> componentCounts;  // 各类元件数量统计
    double analysisTime;           // 分析用时（毫秒）
    QString notes;                 // 备注信息
    bool isValid;                  // 记录是否有效

    PCBDetectionRecord() : pcbConfidence(0.0), totalComponents(0), analysisTime(0.0), isValid(false) {}
    
    // JSON序列化
    QJsonObject toJson() const;
    static PCBDetectionRecord fromJson(const QJsonObject& json);
};

class PCBDetectionManager : public QObject
{
    Q_OBJECT

public:
    explicit PCBDetectionManager(QObject *parent = nullptr);
    ~PCBDetectionManager();

    // 数据库操作
    bool initializeDatabase(const QString& databasePath = "");
    bool saveDetectionRecord(const PCBDetectionRecord& record);
    bool updateDetectionRecord(const PCBDetectionRecord& record);
    bool deleteDetectionRecord(const QString& recordId);
    QList<PCBDetectionRecord> getAllRecords() const;
    QList<PCBDetectionRecord> getRecordsByDateRange(const QDateTime& startDate, const QDateTime& endDate) const;
    QList<PCBDetectionRecord> getRecordsByPCBModel(const QString& pcbModel) const;
    PCBDetectionRecord getRecordById(const QString& recordId) const;

    // 图像管理
    QString saveImage(const cv::Mat& image, const QString& prefix = "pcb_detection");
    QString saveAnnotatedImage(const cv::Mat& image, const std::vector<Label>& labels, 
                             const std::vector<std::string>& classNames,
                             const std::vector<cv::Scalar>& colors,
                             const QString& prefix = "pcb_annotated");
    bool deleteImageFiles(const QString& recordId);
    cv::Mat loadImage(const QString& imagePath) const;

    // 数据导出/导入
    bool exportToJson(const QString& filePath, const QList<QString>& recordIds = QList<QString>()) const;
    bool importFromJson(const QString& filePath);
    bool exportToCSV(const QString& filePath, const QList<QString>& recordIds = QList<QString>()) const;

    // 统计信息
    int getTotalRecordsCount() const;
    QMap<QString, int> getPCBModelStatistics() const;
    QMap<QString, int> getComponentTypeStatistics() const;
    QDateTime getOldestRecordDate() const;
    QDateTime getNewestRecordDate() const;

    // 数据维护
    bool cleanupOldRecords(int keepDays = 30);
    bool compactDatabase();
    qint64 getDatabaseSize() const;

    // 设置
    void setImageStoragePath(const QString& path);
    QString getImageStoragePath() const;
    void setMaxRecordsCount(int maxCount);
    int getMaxRecordsCount() const;

signals:
    void recordAdded(const QString& recordId);
    void recordUpdated(const QString& recordId);
    void recordDeleted(const QString& recordId);
    void databaseError(const QString& error);

private slots:
    void onDatabaseTimer();

private:
    // 私有方法
    QString generateUniqueId() const;
    QString createImageFileName(const QString& prefix, const QString& extension = ".jpg") const;
    bool ensureDirectoryExists(const QString& path) const;    
    void loadRecordsFromDatabase();
    bool saveRecordsToDatabase();
    cv::Mat createAnnotatedImage(const cv::Mat& originalImage,
                               const std::vector<Label>& labels,
                               const std::vector<std::string>& classNames,
                               const std::vector<cv::Scalar>& colors) const;

private:
    QList<PCBDetectionRecord> records_;
    QString database_path_;
    QString image_storage_path_;
    int max_records_count_;
    QTimer* database_timer_;
    mutable QMutex records_mutex_;
};

#endif // PCBDETECTIONMANAGER_H
