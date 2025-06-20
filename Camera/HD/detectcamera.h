#ifndef DETECTCAMERA_H
#define DETECTCAMERA_H

#include <QObject>
#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

// 定义访问宏
#define CAMERA DetectCamera::getInstance()

class DetectCamera : public QObject
{
    Q_OBJECT

public:
    // 获取单例实例
    static DetectCamera* getInstance() {
        static DetectCamera instance;
        return &instance;
    }
    
    // 删除拷贝构造和赋值操作
    DetectCamera(const DetectCamera&) = delete;
    DetectCamera& operator=(const DetectCamera&) = delete;
    
    // 启动相机
    void start();
    // 停止相机
    void stop();
    // 获取当前帧
    cv::Mat getCurrentFrame();
    // 检查相机是否运行
    bool isRunning() const;

    // 设置相机参数
    void setParams(int width = 640, int height = 480, int fps = 30);

signals:
    // 新帧可用信号
    void frameReady(const cv::Mat& frame);

private:
    // 私有构造函数
    explicit DetectCamera(QObject *parent = nullptr);
    ~DetectCamera();

    void captureFrame();  // 捕获帧的线程函数
    void processFrame();  // 处理帧的线程函数

    int m_width{640};
    int m_height{480};
    int m_fps{30};
    int m_delay{33};  // 帧延迟时间(ms)

    cv::VideoCapture m_cap;
    cv::Mat m_currentFrame;  // 当前帧缓存

    std::mutex m_mutex;
    std::queue<cv::Mat> m_frames;
    std::atomic<bool> m_running{false};
    
    std::thread m_captureThread;
    std::thread m_processThread;
};

#endif // DETECTCAMERA_H 