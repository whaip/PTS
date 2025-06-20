#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QStringList>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <queue>

// 摄像头类型枚举
enum class CameraType {
    HD_CAMERA,      // 高清摄像头
    IR_CAMERA       // 红外温度摄像头
};

// 摄像头状态枚举
enum class CameraStatus {
    DISCONNECTED,   // 未连接
    CONNECTED,      // 已连接
    STREAMING,      // 正在流传输
    ERROR          // 错误状态
};

// 图像数据结构
struct ImageData {
    cv::Mat image;
    QDateTime timestamp;
    bool isValid;
    
    ImageData() : isValid(false) {}
};

// 红外温度数据结构
struct ThermalData {
    cv::Mat thermalImage;        // 原始红外图像 (for display)
    cv::Mat thermalColorMap;     // JET热图 (for thermal analysis)
    cv::Mat temperatureMap;      // 温度映射数据
    uint16_t* rawTempData;       // 原始温度数据
    uint32_t width;
    uint32_t height;
    double minTemp;              // 最低温度
    double maxTemp;              // 最高温度
    double avgTemp;              // 平均温度
    QDateTime timestamp;
    bool isValid;
    
    ThermalData() : rawTempData(nullptr), width(0), height(0), 
                   minTemp(0), maxTemp(0), avgTemp(0), isValid(false) {}
};

// PCB检测结果结构
struct PCBDetectionResult {
    cv::Mat detectedImage;       // 检测结果图像
    QStringList detectedComponents; // 检测到的元件列表
    QVector<cv::Rect> componentBounds; // 元件边界框
    QVector<double> confidences; // 置信度
    QString pcbModel;            // PCB型号
    double pcbConfidence;        // PCB识别置信度
    QDateTime timestamp;
    bool isValid;
    
    // 添加缺失的字段
    double processingTime{0.0};  // 处理时间（毫秒）
    double averageConfidence{0.0}; // 平均置信度
    
    PCBDetectionResult() : pcbConfidence(0.0), isValid(false) {}
};

class CameraManager : public QObject
{
    Q_OBJECT

public:
    explicit CameraManager(QObject *parent = nullptr);
    ~CameraManager();

    // 摄像头控制
    bool initializeCamera(CameraType type);
    bool startCamera(CameraType type);
    bool stopCamera(CameraType type);
    void stopAllCameras();
    
    // 状态查询
    CameraStatus getCameraStatus(CameraType type) const;
    bool isCameraAvailable(CameraType type) const;
    
    // 相机诊断
    QString getCameraDiagnosticInfo(CameraType type) const;
      // 图像获取
    ImageData getLatestImage(CameraType type) const;
    ThermalData getLatestThermalData() const;
    cv::Mat getThermalColorMap() const;  // 获取JET热图
      // 温度测量
    double getTemperatureAt(int x, int y) const;
    double getAverageTemperature(const cv::Rect& region) const;
    double getMaxTemperature(const cv::Rect& region) const;
    double getMinTemperature(const cv::Rect& region) const;
    void setTemperatureThreshold(double threshold);
    
    // PCB检测相关
    PCBDetectionResult detectPCBComponents(const cv::Mat& image);
    QString identifyPCBModel(const cv::Mat& image);
    
    // 摄像头参数设置
    void setHDCameraParams(int width, int height, int fps);
    void setIRCameraParams(const QString& ipAddress);
    
    // 图像保存
    bool saveImage(CameraType type, const QString& filePath);
    bool saveThermalData(const QString& filePath);

signals:
    // 摄像头状态变化
    void cameraStatusChanged(CameraType type, CameraStatus status);
    
    // 新图像数据
    void newImageAvailable(CameraType type, const ImageData& data);
    void newThermalDataAvailable(const ThermalData& data);
    
    // PCB检测结果
    void pcbDetectionCompleted(const PCBDetectionResult& result);
    void pcbModelIdentified(const QString& model, double confidence);
    
    // 温度报警
    void temperatureAlert(double temperature, const cv::Point& location);
    
    // 错误信号
    void errorOccurred(CameraType type, const QString& error);

private slots:
    void onHDCameraFrame();
    void onIRCameraFrame();
    void processTemperatureData();

private:
    // 高清摄像头相关
    struct HDCameraData {
        cv::VideoCapture capture;
        std::atomic<bool> isRunning{false};
        std::unique_ptr<std::thread> captureThread;
        std::unique_ptr<std::thread> processThread;
        
        // 帧缓冲队列
        std::mutex frameMutex;
        std::queue<cv::Mat> frameQueue;
        cv::Mat currentFrame;
        
        ImageData latestImage;
        mutable QMutex imageMutex;
        
        // 参数设置
        int width{1920};
        int height{1080};
        int fps{30};
        int maxQueueSize{5};  // 最大队列大小
        
        CameraStatus status{CameraStatus::DISCONNECTED};
    };
    
    // 红外摄像头相关
    struct IRCameraData {
        std::atomic<bool> isRunning{false};
        ThermalData latestThermalData;
        mutable QMutex thermalMutex;
        
        // 网络参数
        QString ipAddress{"192.168.0.200"};
        std::string ipAddressStd{"192.168.0.200"};
        void* rgbStreamer{nullptr};
        void* irStreamer{nullptr};
        void* tempStreamer{nullptr};
        
        CameraStatus status{CameraStatus::DISCONNECTED};
    };
    
    HDCameraData hdCamera_;
    IRCameraData irCamera_;
    
    // 温度阈值设置
    double temperatureThreshold_{80.0}; // 温度报警阈值
      // 私有方法
    bool initializeHDCamera();
    bool initializeIRCamera();
    bool startHDCamera();
    bool startIRCamera();
    bool stopHDCamera();
    bool stopIRCamera();
      void hdCaptureLoop();
    void hdProcessLoop();  // 新增的处理线程
    void processHDFrame(const cv::Mat& frame);
    void processThermalFrame(const ThermalData& data);
    
    // 温度计算辅助函数
    double convertRawToTemperature(uint16_t rawValue) const;
    void calculateTemperatureStats(ThermalData& data) const;
    
    // PCB检测相关私有方法
    cv::Mat preprocessImageForDetection(const cv::Mat& image);
    QVector<cv::Rect> detectComponentRegions(const cv::Mat& image);
    
    // 静态回调函数（用于红外摄像头）
    static void rgbJpegCallback(uint8_t *data, uint32_t dataLen, 
                               uint32_t width, uint32_t height, 
                               uint64_t pts, void *arg);
    static void irJpegCallback(uint8_t *data, uint32_t dataLen, 
                              uint32_t width, uint32_t height, 
                              uint64_t pts, void *arg);
    static void temperatureCallback(uint16_t *paru16Data, uint32_t u32Width, 
                                   uint32_t u32Height, uint64_t u64Pts, void *pArg);
};

#endif // CAMERAMANAGER_H
