#include "pcbdetectionmanager.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QStringConverter>
#include <QUuid>
#include <QFileInfo>
#include <QTextStream>
#include <QMutexLocker>
#include <QTimer>
#include <QDebug>

// PCBDetectionRecord JSON序列化实现
QJsonObject PCBDetectionRecord::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["imagePath"] = imagePath;
    obj["annotatedImagePath"] = annotatedImagePath;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    obj["pcbModel"] = pcbModel;
    obj["pcbConfidence"] = pcbConfidence;
    obj["totalComponents"] = totalComponents;
    obj["analysisTime"] = analysisTime;
    obj["notes"] = notes;
    obj["isValid"] = isValid;

    // 序列化元件标签
    QJsonArray labelsArray;
    for (const auto& label : componentLabels) {
        QJsonObject labelObj;
        labelObj["id"] = label.id;
        labelObj["x"] = label.x;        
        labelObj["y"] = label.y;
        labelObj["w"] = label.w;
        labelObj["h"] = label.h;
        labelObj["cls"] = label.cls;
        labelObj["confidence"] = label.confidence;
        labelObj["label_string"] = label.label;
        labelsArray.append(labelObj);
    }
    obj["componentLabels"] = labelsArray;

    // 序列化元件统计
    QJsonObject countsObj;
    for (const auto& pair : componentCounts) {
        countsObj[QString::fromStdString(pair.first)] = pair.second;
    }
    obj["componentCounts"] = countsObj;

    return obj;
}

PCBDetectionRecord PCBDetectionRecord::fromJson(const QJsonObject& json) {
    PCBDetectionRecord record;
    record.id = json["id"].toString();
    record.imagePath = json["imagePath"].toString();
    record.annotatedImagePath = json["annotatedImagePath"].toString();
    record.timestamp = QDateTime::fromString(json["timestamp"].toString(), Qt::ISODate);
    record.pcbModel = json["pcbModel"].toString();
    record.pcbConfidence = json["pcbConfidence"].toDouble();
    record.totalComponents = json["totalComponents"].toInt();
    record.analysisTime = json["analysisTime"].toDouble();
    record.notes = json["notes"].toString();
    record.isValid = json["isValid"].toBool();

    // 反序列化元件标签
    QJsonArray labelsArray = json["componentLabels"].toArray();
    for (const auto& value : labelsArray) {
        QJsonObject labelObj = value.toObject();
        Label label;
        label.id = labelObj["id"].toInt();
        label.x = labelObj["x"].toInt();
        label.y = labelObj["y"].toInt();        label.w = labelObj["w"].toInt();
        label.h = labelObj["h"].toInt();
        label.cls = labelObj["cls"].toInt();
        label.confidence = labelObj["confidence"].toDouble();
        label.label = labelObj["label_string"].toString();
        record.componentLabels.push_back(label);
    }

    // 反序列化元件统计
    QJsonObject countsObj = json["componentCounts"].toObject();
    for (auto it = countsObj.begin(); it != countsObj.end(); ++it) {
        record.componentCounts[it.key().toStdString()] = it.value().toInt();
    }

    return record;
}

PCBDetectionManager::PCBDetectionManager(QObject *parent)
    : QObject(parent)
    , max_records_count_(1000)
    , database_timer_(new QTimer(this))
{
    // 设置默认路径
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    image_storage_path_ = documentsPath + "/PCB_Detection_Images";
    database_path_ = documentsPath + "/PCB_Detection_Database.json";

    // 确保目录存在
    ensureDirectoryExists(image_storage_path_);

    // 设置定时保存
    database_timer_->setInterval(30000); // 30秒保存一次
    connect(database_timer_, &QTimer::timeout, this, &PCBDetectionManager::onDatabaseTimer);
    database_timer_->start();

    // 初始化数据库
    initializeDatabase();
}

PCBDetectionManager::~PCBDetectionManager() {
    if (database_timer_) {
        database_timer_->stop();
    }
    saveRecordsToDatabase();
}

bool PCBDetectionManager::initializeDatabase(const QString& databasePath) {
    if (!databasePath.isEmpty()) {
        database_path_ = databasePath;
    }

    QFileInfo fileInfo(database_path_);
    if (!ensureDirectoryExists(fileInfo.absolutePath())) {
        emit databaseError("无法创建数据库目录");
        return false;
    }

    loadRecordsFromDatabase();
    return true;
}

bool PCBDetectionManager::saveDetectionRecord(const PCBDetectionRecord& record) {
    QMutexLocker locker(&records_mutex_);

    PCBDetectionRecord newRecord = record;
    if (newRecord.id.isEmpty()) {
        newRecord.id = generateUniqueId();
    }

    if (newRecord.timestamp.isNull()) {
        newRecord.timestamp = QDateTime::currentDateTime();
    }

    records_.append(newRecord);

    // 限制记录数量
    while (records_.size() > max_records_count_) {
        PCBDetectionRecord oldRecord = records_.takeFirst();
        deleteImageFiles(oldRecord.id);
    }

    emit recordAdded(newRecord.id);
    return true;
}

bool PCBDetectionManager::updateDetectionRecord(const PCBDetectionRecord& record) {
    QMutexLocker locker(&records_mutex_);

    for (int i = 0; i < records_.size(); ++i) {
        if (records_[i].id == record.id) {
            records_[i] = record;
            emit recordUpdated(record.id);
            return true;
        }
    }

    return false;
}

bool PCBDetectionManager::deleteDetectionRecord(const QString& recordId) {
    QMutexLocker locker(&records_mutex_);

    for (int i = 0; i < records_.size(); ++i) {
        if (records_[i].id == recordId) {
            deleteImageFiles(recordId);
            records_.removeAt(i);
            emit recordDeleted(recordId);
            return true;
        }
    }

    return false;
}

QList<PCBDetectionRecord> PCBDetectionManager::getAllRecords() const {
    QMutexLocker locker(&records_mutex_);
    return records_;
}

QList<PCBDetectionRecord> PCBDetectionManager::getRecordsByDateRange(const QDateTime& startDate, const QDateTime& endDate) const {
    QMutexLocker locker(&records_mutex_);

    QList<PCBDetectionRecord> filteredRecords;
    for (const auto& record : records_) {
        if (record.timestamp >= startDate && record.timestamp <= endDate) {
            filteredRecords.append(record);
        }
    }

    return filteredRecords;
}

QList<PCBDetectionRecord> PCBDetectionManager::getRecordsByPCBModel(const QString& pcbModel) const {
    QMutexLocker locker(&records_mutex_);

    QList<PCBDetectionRecord> filteredRecords;
    for (const auto& record : records_) {
        if (record.pcbModel == pcbModel) {
            filteredRecords.append(record);
        }
    }

    return filteredRecords;
}

PCBDetectionRecord PCBDetectionManager::getRecordById(const QString& recordId) const {
    QMutexLocker locker(&records_mutex_);

    for (const auto& record : records_) {
        if (record.id == recordId) {
            return record;
        }
    }

    return PCBDetectionRecord();
}

QString PCBDetectionManager::saveImage(const cv::Mat& image, const QString& prefix) {
    if (image.empty()) {
        return QString();
    }

    QString filename = createImageFileName(prefix);
    QString fullPath = image_storage_path_ + "/" + filename;

    if (cv::imwrite(fullPath.toStdString(), image)) {
        return fullPath;
    }

    return QString();
}

QString PCBDetectionManager::saveAnnotatedImage(const cv::Mat& image, const std::vector<Label>& labels, 
                                               const std::vector<std::string>& classNames,
                                               const std::vector<cv::Scalar>& colors,
                                               const QString& prefix) {
    if (image.empty()) {
        return QString();
    }

    cv::Mat annotatedImage = createAnnotatedImage(image, labels, classNames, colors);
    return saveImage(annotatedImage, prefix);
}

bool PCBDetectionManager::deleteImageFiles(const QString& recordId) {
    PCBDetectionRecord record = getRecordById(recordId);
    if (!record.isValid) {
        return false;
    }

    bool success = true;

    if (!record.imagePath.isEmpty() && QFile::exists(record.imagePath)) {
        success &= QFile::remove(record.imagePath);
    }

    if (!record.annotatedImagePath.isEmpty() && QFile::exists(record.annotatedImagePath)) {
        success &= QFile::remove(record.annotatedImagePath);
    }

    return success;
}

cv::Mat PCBDetectionManager::loadImage(const QString& imagePath) const {
    if (imagePath.isEmpty() || !QFile::exists(imagePath)) {
        return cv::Mat();
    }

    return cv::imread(imagePath.toStdString());
}

bool PCBDetectionManager::exportToJson(const QString& filePath, const QList<QString>& recordIds) const {
    QMutexLocker locker(&records_mutex_);

    QJsonArray recordsArray;
    
    if (recordIds.isEmpty()) {
        // 导出所有记录
        for (const auto& record : records_) {
            recordsArray.append(record.toJson());
        }
    } else {
        // 导出指定记录
        for (const QString& id : recordIds) {
            for (const auto& record : records_) {
                if (record.id == id) {
                    recordsArray.append(record.toJson());
                    break;
                }
            }
        }
    }

    QJsonObject rootObj;
    rootObj["version"] = "1.0";
    rootObj["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObj["recordCount"] = recordsArray.size();
    rootObj["records"] = recordsArray;

    QJsonDocument doc(rootObj);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(doc.toJson());
    return true;
}

bool PCBDetectionManager::importFromJson(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject rootObj = doc.object();

    QJsonArray recordsArray = rootObj["records"].toArray();
    
    QMutexLocker locker(&records_mutex_);
    
    for (const auto& value : recordsArray) {
        PCBDetectionRecord record = PCBDetectionRecord::fromJson(value.toObject());
        if (record.isValid) {
            records_.append(record);
            emit recordAdded(record.id);
        }
    }

    return true;
}

bool PCBDetectionManager::exportToCSV(const QString& filePath, const QList<QString>& recordIds) const {
    QMutexLocker locker(&records_mutex_);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 写入CSV头部
    out << "ID,时间戳,PCB模型,PCB置信度,元件总数,分析时间(ms),备注\n";

    QList<PCBDetectionRecord> recordsToExport;
    if (recordIds.isEmpty()) {
        recordsToExport = records_;
    } else {
        for (const QString& id : recordIds) {
            for (const auto& record : records_) {
                if (record.id == id) {
                    recordsToExport.append(record);
                    break;
                }
            }
        }
    }

    for (const auto& record : recordsToExport) {
        out << record.id << ","
            << record.timestamp.toString(Qt::ISODate) << ","
            << record.pcbModel << ","
            << record.pcbConfidence << ","
            << record.totalComponents << ","
            << record.analysisTime << ","
            << record.notes << "\n";
    }

    return true;
}

int PCBDetectionManager::getTotalRecordsCount() const {
    QMutexLocker locker(&records_mutex_);
    return records_.size();
}

QMap<QString, int> PCBDetectionManager::getPCBModelStatistics() const {
    QMutexLocker locker(&records_mutex_);

    QMap<QString, int> stats;
    for (const auto& record : records_) {
        if (!record.pcbModel.isEmpty()) {
            stats[record.pcbModel]++;
        }
    }

    return stats;
}

QMap<QString, int> PCBDetectionManager::getComponentTypeStatistics() const {
    QMutexLocker locker(&records_mutex_);

    QMap<QString, int> stats;
    for (const auto& record : records_) {
        for (const auto& pair : record.componentCounts) {
            QString componentName = QString::fromStdString(pair.first);
            stats[componentName] += pair.second;
        }
    }

    return stats;
}

QDateTime PCBDetectionManager::getOldestRecordDate() const {
    QMutexLocker locker(&records_mutex_);

    QDateTime oldest;
    for (const auto& record : records_) {
        if (!oldest.isValid() || record.timestamp < oldest) {
            oldest = record.timestamp;
        }
    }

    return oldest;
}

QDateTime PCBDetectionManager::getNewestRecordDate() const {
    QMutexLocker locker(&records_mutex_);

    QDateTime newest;
    for (const auto& record : records_) {
        if (!newest.isValid() || record.timestamp > newest) {
            newest = record.timestamp;
        }
    }

    return newest;
}

bool PCBDetectionManager::cleanupOldRecords(int keepDays) {
    QMutexLocker locker(&records_mutex_);

    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-keepDays);
    int removedCount = 0;

    for (int i = records_.size() - 1; i >= 0; --i) {
        if (records_[i].timestamp < cutoffDate) {
            deleteImageFiles(records_[i].id);
            records_.removeAt(i);
            removedCount++;
        }
    }

    return removedCount > 0;
}

bool PCBDetectionManager::compactDatabase() {
    return saveRecordsToDatabase();
}

qint64 PCBDetectionManager::getDatabaseSize() const {
    QFileInfo fileInfo(database_path_);
    return fileInfo.size();
}

void PCBDetectionManager::setImageStoragePath(const QString& path) {
    image_storage_path_ = path;
    ensureDirectoryExists(image_storage_path_);
}

QString PCBDetectionManager::getImageStoragePath() const {
    return image_storage_path_;
}

void PCBDetectionManager::setMaxRecordsCount(int maxCount) {
    max_records_count_ = maxCount;
}

int PCBDetectionManager::getMaxRecordsCount() const {
    return max_records_count_;
}

void PCBDetectionManager::onDatabaseTimer() {
    saveRecordsToDatabase();
}

QString PCBDetectionManager::generateUniqueId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString PCBDetectionManager::createImageFileName(const QString& prefix, const QString& extension) const {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    return QString("%1_%2%3").arg(prefix).arg(timestamp).arg(extension);
}

bool PCBDetectionManager::ensureDirectoryExists(const QString& path) const {
    QDir dir;
    return dir.mkpath(path);
}

void PCBDetectionManager::loadRecordsFromDatabase() {
    QFile file(database_path_);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject rootObj = doc.object();

    QJsonArray recordsArray = rootObj["records"].toArray();
    
    QMutexLocker locker(&records_mutex_);
    records_.clear();
    
    for (const auto& value : recordsArray) {
        PCBDetectionRecord record = PCBDetectionRecord::fromJson(value.toObject());
        if (record.isValid) {
            records_.append(record);
        }
    }
}

bool PCBDetectionManager::saveRecordsToDatabase() {
    QMutexLocker locker(&records_mutex_);

    QJsonArray recordsArray;
    for (const auto& record : records_) {
        recordsArray.append(record.toJson());
    }

    QJsonObject rootObj;
    rootObj["version"] = "1.0";
    rootObj["saveTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObj["recordCount"] = recordsArray.size();
    rootObj["records"] = recordsArray;

    QJsonDocument doc(rootObj);

    QFile file(database_path_);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        return true;
    }
    
    return false;
}

cv::Mat PCBDetectionManager::createAnnotatedImage(const cv::Mat& originalImage, 
                                                const std::vector<Label>& labels,
                                                const std::vector<std::string>& classNames,
                                                const std::vector<cv::Scalar>& colors) const {
    cv::Mat result = originalImage.clone();
    
    for (const auto& label : labels) {
        if (label.cls < 0 || label.cls >= static_cast<int>(colors.size()) || 
            label.cls >= static_cast<int>(classNames.size())) {
            continue;
        }
        
        cv::Scalar color = colors[label.cls];
        std::string class_name = classNames[label.cls];
        
        // 绘制边界框
        cv::rectangle(result, cv::Point(label.x, label.y), 
                     cv::Point(label.x + label.w, label.y + label.h), color, 2);
        
        // 绘制标签
        std::string label_text = class_name + " " + std::to_string(static_cast<int>(label.confidence * 100)) + "%";
        cv::putText(result, label_text, cv::Point(label.x, label.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
    }
    
    return result;
}
