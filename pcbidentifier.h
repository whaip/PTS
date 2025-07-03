#ifndef PCBIDENTIFIER_H
#define PCBIDENTIFIER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QDateTime>
#include <QPixmap>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <opencv2/opencv.hpp>
#include <memory>
#include "PCB_Model_Identification/siftmatcher.h"

// 前向声明
class CameraManager;

// 实时识别结果结构体
struct RealtimeIdentificationResult {
    QString modelName;          // 识别到的PCB型号
    double confidence;          // 识别置信度 (0-100)
    int matchCount;            // 匹配点数量
    double matchScore;         // 匹配分数
    bool isValid;              // 识别是否有效
    QDateTime timestamp;       // 识别时间戳
    cv::Mat annotatedImage;    // 带标注的图像
    bool isRealtime;           // 标记为实时识别结果
    
    RealtimeIdentificationResult() : confidence(0.0), matchCount(0), matchScore(0.0), 
                                   isValid(false), timestamp(QDateTime::currentDateTime()), 
                                   isRealtime(true) {}
};

// PCB识别结果结构体
struct PCBIdentificationResult {
    QString modelName;          // 识别到的PCB型号
    QString imagePath;          // 匹配的模板图片路径
    double confidence;          // 识别置信度 (0-100)
    int matchCount;            // 匹配点数量
    double matchScore;         // 匹配分数
    bool isValid;              // 识别是否有效
    QString errorMessage;      // 错误信息
    QDateTime timestamp;       // 识别时间戳
    
    PCBIdentificationResult() : confidence(0.0), matchCount(0), matchScore(0.0), 
                               isValid(false), timestamp(QDateTime::currentDateTime()) {}
};

// PCB模型信息结构体
struct PCBModelInfo {
    QString modelName;         // 模型名称
    QString modelCode;         // 模型代码
    QString description;       // 描述信息
    QStringList templatePaths; // 模板图片路径列表
    QString configPath;        // 配置文件路径
    QDateTime lastUpdated;     // 最后更新时间
    
    PCBModelInfo() : lastUpdated(QDateTime::currentDateTime()) {}
};

class PCBIdentifier : public QObject
{
    Q_OBJECT

public:
    explicit PCBIdentifier(QObject *parent = nullptr);
    ~PCBIdentifier();    // 基础识别功能
    
    // 实时摄像头识别功能
    RealtimeIdentificationResult identifyPCBFromImage(const cv::Mat& image);
    bool startRealtimeIdentification(CameraManager* cameraManager);
    void stopRealtimeIdentification();
    bool isRealtimeIdentificationRunning() const;
    void setRealtimeProcessingInterval(int intervalMs);
    
    // 摄像头集成
    void setCameraManager(CameraManager* cameraManager);
    CameraManager* getCameraManager() const;
    
    // 模板管理
    bool addPCBModel(const QString& modelName, const cv::Mat& imageData);
    bool removePCBModel(const QString& modelName);
    bool updatePCBModel(const QString& modelName, const cv::Mat& image);
    QVector<PCBModelInfo> getAvailableModels() const;
    
    // 数据库管理
    bool createDatabase(const QString& templateDirectory);
    bool loadDatabase(const QString& databasePath = "");
    bool saveDatabase(const QString& databasePath = "");
    bool rebuildDatabase();
    
    // 配置和参数
    void setMatchThreshold(double threshold);
    void setMaxResults(int maxResults);
    void setUseGPU(bool useGPU);
    double getMatchThreshold() const { return match_threshold_; }
    int getMaxResults() const { return max_results_; }
    bool isGPUEnabled() const;
    
    // 统计和诊断
    QString getLastError() const { return last_error_; }
    QStringList getSupportedImageFormats() const;
    bool validateImagePath(const QString& imagePath) const;
    
signals:
    void identificationCompleted(const PCBIdentificationResult& result);
    void realtimeIdentificationCompleted(const RealtimeIdentificationResult& result);
    void identificationProgress(int percentage);
    void databaseUpdated();
    void errorOccurred(const QString& error);
    void realtimeIdentificationStarted();
    void realtimeIdentificationStopped();

private slots:
    void onIdentificationFinished();
    void onCameraImageReceived();
    void processRealtimeIdentification();

private:
      // 摄像头管理
    CameraManager* camera_manager_;
    QTimer* realtime_timer_;
    bool is_realtime_running_;
    int realtime_interval_ms_;
    
    // 线程安全和异步处理
    mutable QMutex identification_mutex_;
    QFutureWatcher<RealtimeIdentificationResult>* future_watcher_;
    QFuture<RealtimeIdentificationResult> current_future_;
    bool is_processing_;
    cv::Mat current_image_;
    QMutex image_mutex_;
    
    // 配置参数
    double match_threshold_;
    int max_results_;
    QString database_path_;
    QString template_directory_;
    
    // 模型信息
    QVector<PCBModelInfo> pcb_models_;
    
    // 状态信息
    QString last_error_;
    bool is_initialized_;
      // 私有方法
    void setError(const QString& error);
    bool initializeMatcher();
    QString extractModelNameFromPath(const QString& imagePath) const;
    bool loadModelInfo(const QString& configPath);
    bool saveModelInfo(const QString& configPath) const;
    QStringList findTemplateImages(const QString& directory) const;    PCBIdentificationResult processMatchResults(const std::vector<SiftMatcher::MatchResult>& matches) const;
    RealtimeIdentificationResult processRealtimeMatchResults(const std::vector<SiftMatcher::MatchResult>& matches, const cv::Mat& originalImage) const;
    bool validateTemplate(const QString& templatePath) const;
    cv::Mat annotateIdentificationResult(const cv::Mat& image, const RealtimeIdentificationResult& result) const;
    
    // 线程相关的私有方法
    RealtimeIdentificationResult identifyPCBFromImageThreaded(const cv::Mat& image);
    void processImageInBackground(const cv::Mat& image);
    
    // 常量
    static const double DEFAULT_MATCH_THRESHOLD;
    static const int DEFAULT_MAX_RESULTS;
    static const QStringList SUPPORTED_FORMATS;
};

#endif // PCBIDENTIFIER_H
