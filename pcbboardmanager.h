#ifndef PCBBOARDMANAGER_H
#define PCBBOARDMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QDir>
#include <QMutex>
#include <QThread>
#include <QRunnable>
#include <QThreadPool>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QDataStream>
#include <QFile>
#include <opencv2/opencv.hpp>
#include "PCB_Components_Detect/ClassList.h"

// 前向声明
class YOLOModel;

// 特征点数据结构
struct FeatureKeyPoint {
    float x, y;                   // 特征点坐标
    float angle;                  // 角度
    float response;               // 响应强度
    int octave;                   // 金字塔层级
    float size;                   // 特征点尺寸
    
    FeatureKeyPoint() : x(0), y(0), angle(0), response(0), octave(0), size(0) {}
    FeatureKeyPoint(const cv::KeyPoint& kp) 
        : x(kp.pt.x), y(kp.pt.y), angle(kp.angle), response(kp.response), 
          octave(kp.octave), size(kp.size) {}
    
    cv::KeyPoint toCvKeyPoint() const {
        return cv::KeyPoint(cv::Point2f(x, y), size, angle, response, octave);
    }
};

// 单张图像的特征信息
struct ImageFeatureInfo {
    QString imagePath;            // 图像路径
    int rotation;                 // 旋转角度 (0, 90, 180, 270)
    std::vector<FeatureKeyPoint> keypoints;  // 特征点
    std::vector<float> descriptors;          // 描述符(展平的数据)
    int descriptorRows;           // 描述符行数
    int descriptorCols;           // 描述符列数
    QDateTime extractTime;        // 提取时间
    
    ImageFeatureInfo() : rotation(0), descriptorRows(0), descriptorCols(0) {}
    
    // 转换为OpenCV格式
    std::vector<cv::KeyPoint> getCvKeypoints() const;
    cv::Mat getCvDescriptors() const;
    
    // 从OpenCV格式设置
    void setFromCv(const std::vector<cv::KeyPoint>& kps, const cv::Mat& desc);
};

// 板卡的完整特征信息
struct BoardFeatureInfo {
    QString boardId;              // 板卡ID
    std::vector<ImageFeatureInfo> imageFeatures;  // 4个旋转角度的特征
    QDateTime lastUpdate;         // 最后更新时间
    
    BoardFeatureInfo() {}
    explicit BoardFeatureInfo(const QString& id) : boardId(id) {}
};

// PCB板卡信息结构体
struct PCBBoardInfo {
    QString boardId;              // 板卡唯一ID
    QString boardName;            // 板卡名称
    QString boardModel;           // 板卡型号
    QString description;          // 描述信息
    QString imagePath;            // 板卡图片路径
    QString templatePath;         // 模板图片路径（用于识别）
    QString featureFilePath;      // 特征文件路径
    QDateTime createTime;         // 创建时间
    QDateTime updateTime;         // 更新时间
    std::vector<Label> components; // 元器件列表
    int componentCount;           // 元器件数量
    bool isActive;                // 是否激活
    bool hasFeatures;             // 是否已提取特征
    
    PCBBoardInfo() : componentCount(0), isActive(true), hasFeatures(false) {}
    
    // JSON序列化
    QJsonObject toJson() const;
    static PCBBoardInfo fromJson(const QJsonObject& json);
};

// 元器件信息结构体（扩展Label）
struct ComponentInfo {
    Label labelInfo;              // 基础标签信息
    QString componentName;        // 元器件名称
    QString componentType;        // 元器件类型
    QString componentValue;       // 元器件参数值
    QString componentPackage;     // 封装类型
    QString manufacturer;         // 制造商
    QString partNumber;           // 器件编号
    QString description;          // 描述
    bool isRequired;              // 是否必需
    
    ComponentInfo() : isRequired(true) {}
    ComponentInfo(const Label& label) : labelInfo(label), isRequired(true) {}
    
    // JSON序列化
    QJsonObject toJson() const;
    static ComponentInfo fromJson(const QJsonObject& json);
};

class PCBBoardManager : public QObject
{
    Q_OBJECT

public:
    explicit PCBBoardManager(QObject *parent = nullptr);
    ~PCBBoardManager();

    // 数据库初始化
    bool initializeDatabase(const QString& databasePath = "");
    
    // 板卡管理
    QString createBoard(const QString& boardName, const QString& boardModel, 
                       const cv::Mat& boardImage, const QString& description = "");
    bool updateBoard(const PCBBoardInfo& boardInfo);
    bool deleteBoard(const QString& boardId);
    PCBBoardInfo getBoardById(const QString& boardId) const;
    QList<PCBBoardInfo> getAllBoards() const;
    QList<PCBBoardInfo> getBoardsByModel(const QString& model) const;
    QStringList getAllBoardModels() const;
    
    // 元器件管理
    bool addComponent(const QString& boardId, const ComponentInfo& component);
    bool updateComponent(const QString& boardId, const ComponentInfo& component);
    bool removeComponent(const QString& boardId, int componentId);
    QList<ComponentInfo> getComponents(const QString& boardId) const;
    ComponentInfo getComponent(const QString& boardId, int componentId) const;
      // 板卡识别
    QList<PCBBoardInfo> identifyBoard(const cv::Mat& inputImage, double threshold = 0.3);
    PCBBoardInfo getBestMatch(const cv::Mat& inputImage, double threshold = 0.3);
    
    // 异步板卡识别
    void identifyBoardAsync(const cv::Mat& inputImage, double threshold = 0.3);
    void cancelBoardIdentification();
    
    // 元器件检测
    std::vector<Label> detectComponents(const cv::Mat& boardImage);
    cv::Mat createAnnotatedImage(const cv::Mat& originalImage, const QList<ComponentInfo>& components);
    
    // 异步元器件检测
    void detectComponentsAsync(const cv::Mat& boardImage, const QString& boardId = "");
    void cancelComponentDetection();
      // 异步板卡创建
    void createBoardAsync(const QString& boardName, const QString& boardModel, 
                         const cv::Mat& boardImage, const QString& description = "");
    void cancelBoardCreation();
    
    // 特征管理
    bool extractAndSaveFeatures(const QString& boardId);
    bool loadBoardFeatures(const QString& boardId, BoardFeatureInfo& features) const;
    bool deleteBoardFeatures(const QString& boardId);
    QString getFeatureFilePath(const QString& boardId) const;
    bool hasFeatureFile(const QString& boardId) const;
    
    // 文件管理
    QString saveImage(const cv::Mat& image, const QString& prefix = "pcb_board");
    bool deleteImageFiles(const QString& boardId);
    cv::Mat loadImage(const QString& imagePath) const;
    
    // 导入导出
    bool exportBoard(const QString& boardId, const QString& filePath);
    bool importBoard(const QString& filePath);
    bool exportAllBoards(const QString& filePath);
    
    // 统计信息
    int getTotalBoardsCount() const;
    int getTotalComponentsCount() const;
    QMap<QString, int> getComponentTypeStatistics() const;

signals:
    void boardAdded(const QString& boardId);
    void boardUpdated(const QString& boardId);
    void boardDeleted(const QString& boardId);
    void componentAdded(const QString& boardId, int componentId);
    void componentUpdated(const QString& boardId, int componentId);
    void componentRemoved(const QString& boardId, int componentId);
    void databaseError(const QString& error);
    
    // 异步操作信号
    void boardIdentificationStarted();
    void boardIdentificationProgress(int percentage, const QString& message);
    void boardIdentificationFinished(const QList<PCBBoardInfo>& candidates);
    void boardIdentificationError(const QString& errorMessage);
    
    void componentDetectionStarted();
    void componentDetectionProgress(int percentage, const QString& message);
    void componentDetectionFinished(const std::vector<Label>& components, const QString& boardId);
    void componentDetectionError(const QString& errorMessage);
    
    void boardCreationStarted();
    void boardCreationProgress(int percentage, const QString& message);
    void boardCreationFinished(const QString& boardId);
    void boardCreationError(const QString& errorMessage);

private:
    // 私有方法
    QString generateBoardId() const;
    int generateComponentId(const QString& boardId) const;
    bool ensureDirectoryExists(const QString& path) const;
    void loadBoardsFromDatabase();
    bool saveBoardsToDatabase();
    QString createImageFileName(const QString& prefix, const QString& extension = ".jpg") const;
      // 板卡识别辅助方法
    double calculateSimilarity(const cv::Mat& image1, const cv::Mat& image2);
    double calculateSimilarityWithFeatures(const cv::Mat& inputImage, const BoardFeatureInfo& boardFeatures);
    std::vector<cv::KeyPoint> extractKeypoints(const cv::Mat& image);
    cv::Mat extractDescriptors(const cv::Mat& image, std::vector<cv::KeyPoint>& keypoints);
      // 特征处理方法
    cv::Mat rotateImage(const cv::Mat& image, int angle);
    bool extractImageFeatures(const cv::Mat& image, ImageFeatureInfo& featureInfo);
    bool saveBoardFeaturesToFile(const BoardFeatureInfo& features, const QString& filePath);
    bool loadBoardFeaturesFromFile(const QString& filePath, BoardFeatureInfo& features) const;
    void writeFeatureKeyPoint(QDataStream& stream, const FeatureKeyPoint& kp) const;
    FeatureKeyPoint readFeatureKeyPoint(QDataStream& stream) const;
    
    // 多线程辅助方法
    void initializeWatchers();
    void cleanupWatchers();
    
private:
    QString database_path_;
    QString image_storage_path_;
    QString features_storage_path_;
    QList<PCBBoardInfo> boards_;
    mutable QMutex boards_mutex_;
    
    // OpenCV特征检测器
    cv::Ptr<cv::SIFT> sift_detector_;
    cv::Ptr<cv::BFMatcher> matcher_;
    
    // YOLO元件检测模型
    std::shared_ptr<YOLOModel> yolo_model_;
    
    // 多线程相关
    QFutureWatcher<QList<PCBBoardInfo>>* identification_watcher_;
    QFutureWatcher<std::vector<Label>>* detection_watcher_;
    QFutureWatcher<QString>* creation_watcher_;
    QMutex cancel_mutex_;
    bool identification_cancelled_;
    bool detection_cancelled_;
    bool creation_cancelled_;
};

#endif // PCBBOARDMANAGER_H
