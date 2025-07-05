#include "cameramanager.h"
#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QTextStream>
#include <QFile>
#include <cmath>

// 包含红外摄像头SDK头文件
#ifdef _WIN32
#include "include/RtNet.h"
#include "include/ReturnCodeDef.h"
#include "include/RunParamDef.h"
#endif

CameraManager::CameraManager(QObject *parent)
    : QObject(parent)
{
    qDebug() << "CameraManager initialized";
}

CameraManager::~CameraManager()
{
    stopAllCameras();
}

bool CameraManager::initializeCamera(CameraType type)
{
    switch (type) {
    case CameraType::HD_CAMERA:
        return initializeHDCamera();
    case CameraType::IR_CAMERA:
        return initializeIRCamera();
    }
    return false;
}

bool CameraManager::startCamera(CameraType type)
{
    switch (type) {
    case CameraType::HD_CAMERA:
        return startHDCamera();
    case CameraType::IR_CAMERA:
        return startIRCamera();
    }
    return false;
}

bool CameraManager::stopCamera(CameraType type)
{
    switch (type) {
    case CameraType::HD_CAMERA:
        return stopHDCamera();
    case CameraType::IR_CAMERA:
        return stopIRCamera();
    }
    return false;
}

void CameraManager::stopAllCameras()
{
    stopCamera(CameraType::HD_CAMERA);
    stopCamera(CameraType::IR_CAMERA);
}

CameraStatus CameraManager::getCameraStatus(CameraType type) const
{
    switch (type) {
    case CameraType::HD_CAMERA:
        return hdCamera_.status;
    case CameraType::IR_CAMERA:
        return irCamera_.status;
    }
    return CameraStatus::ERROR;
}

bool CameraManager::isCameraAvailable(CameraType type) const
{
    CameraStatus status = getCameraStatus(type);
    return (status == CameraStatus::CONNECTED || status == CameraStatus::STREAMING);
}

QString CameraManager::getCameraDiagnosticInfo(CameraType type) const
{
    QString info;
    QTextStream stream(&info);
    
    switch (type) {
    case CameraType::HD_CAMERA:
        stream << "=== HD Camera Diagnostic Info ===\n";
        stream << "Status: " << static_cast<int>(hdCamera_.status) << "\n";
        stream << "Running: " << (hdCamera_.isRunning ? "Yes" : "No") << "\n";
        stream << "Capture Opened: " << (hdCamera_.capture.isOpened() ? "Yes" : "No") << "\n";
        
        if (hdCamera_.capture.isOpened()) {
            stream << "Width: " << hdCamera_.capture.get(cv::CAP_PROP_FRAME_WIDTH) << "\n";
            stream << "Height: " << hdCamera_.capture.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";
            stream << "FPS: " << hdCamera_.capture.get(cv::CAP_PROP_FPS) << "\n";
            stream << "Backend: " << hdCamera_.capture.get(cv::CAP_PROP_BACKEND) << "\n";
            stream << "Buffer Size: " << hdCamera_.capture.get(cv::CAP_PROP_BUFFERSIZE) << "\n";
            stream << "FOURCC: " << hdCamera_.capture.get(cv::CAP_PROP_FOURCC) << "\n";
        }
        
        stream << "Configured: " << hdCamera_.width << "x" << hdCamera_.height << "@" << hdCamera_.fps << "\n";
        stream << "Latest Image Valid: " << (hdCamera_.latestImage.isValid ? "Yes" : "No") << "\n";
        if (hdCamera_.latestImage.isValid) {
            stream << "Image Size: " << hdCamera_.latestImage.image.cols << "x" << hdCamera_.latestImage.image.rows << "\n";
            stream << "Image Type: " << hdCamera_.latestImage.image.type() << "\n";
            stream << "Timestamp: " << hdCamera_.latestImage.timestamp.toString() << "\n";
        }
        break;
        
    case CameraType::IR_CAMERA:
        stream << "=== IR Camera Diagnostic Info ===\n";
        stream << "Status: " << static_cast<int>(irCamera_.status) << "\n";
        stream << "Running: " << (irCamera_.isRunning ? "Yes" : "No") << "\n";
        stream << "IP Address: " << irCamera_.ipAddress << "\n";
        stream << "RGB Streamer: " << (irCamera_.rgbStreamer ? "Connected" : "Not Connected") << "\n";
        stream << "IR Streamer: " << (irCamera_.irStreamer ? "Connected" : "Not Connected") << "\n";
        stream << "Temp Streamer: " << (irCamera_.tempStreamer ? "Connected" : "Not Connected") << "\n";
        stream << "Latest Data Valid: " << (irCamera_.latestThermalData.isValid ? "Yes" : "No") << "\n";        if (irCamera_.latestThermalData.isValid) {
            stream << "IR Image Size: " << irCamera_.latestThermalData.thermalImage.cols << "x" << irCamera_.latestThermalData.thermalImage.rows << "\n";
            if (!irCamera_.latestThermalData.thermalColorMap.empty()) {
                stream << "Thermal ColorMap Size: " << irCamera_.latestThermalData.thermalColorMap.cols << "x" << irCamera_.latestThermalData.thermalColorMap.rows << "\n";
            }
            stream << "Temperature Range: " << irCamera_.latestThermalData.minTemp << "°C - " << irCamera_.latestThermalData.maxTemp << "°C\n";
            stream << "Average Temperature: " << irCamera_.latestThermalData.avgTemp << "°C\n";
            stream << "Timestamp: " << irCamera_.latestThermalData.timestamp.toString() << "\n";
        }
        break;
    }
    
    return info;
}

ImageData CameraManager::getLatestImage(CameraType type) const
{
    switch (type) {
    case CameraType::HD_CAMERA: {
        QMutexLocker locker(&hdCamera_.imageMutex);
        return hdCamera_.latestImage;
    }
    case CameraType::IR_CAMERA: {
        QMutexLocker locker(&irCamera_.thermalMutex);
        ImageData data;
        data.image = irCamera_.latestThermalData.thermalImage;
        data.timestamp = irCamera_.latestThermalData.timestamp;
        data.isValid = irCamera_.latestThermalData.isValid;
        return data;
    }
    }
    return ImageData();
}

ThermalData CameraManager::getLatestThermalData() const
{
    QMutexLocker locker(&irCamera_.thermalMutex);
    return irCamera_.latestThermalData;
}

cv::Mat CameraManager::getThermalColorMap() const
{
    QMutexLocker locker(&irCamera_.thermalMutex);
    return irCamera_.latestThermalData.thermalColorMap.clone();
}

double CameraManager::getTemperatureAt(int x, int y) const
{
    QMutexLocker locker(&irCamera_.thermalMutex);
    
    const ThermalData& data = irCamera_.latestThermalData;
    if (!data.isValid || !data.rawTempData || 
        x < 0 || y < 0 || x >= static_cast<int>(data.width) || y >= static_cast<int>(data.height)) {
        return -273.15; // 返回绝对零度表示无效
    }
    
    uint16_t rawValue = data.rawTempData[y * data.width + x];
    return convertRawToTemperature(rawValue);
}

double CameraManager::getAverageTemperature(const cv::Rect& region) const
{
    QMutexLocker locker(&irCamera_.thermalMutex);
    
    const ThermalData& data = irCamera_.latestThermalData;
    if (!data.isValid || !data.rawTempData) {
        return -273.15;
    }
    
    // 确保区域在图像范围内
    cv::Rect validRegion = region & cv::Rect(0, 0, data.width, data.height);
    if (validRegion.area() <= 0) {
        return -273.15;
    }
    
    double totalTemp = 0.0;
    int count = 0;
    
    for (int y = validRegion.y; y < validRegion.y + validRegion.height; ++y) {
        for (int x = validRegion.x; x < validRegion.x + validRegion.width; ++x) {
            uint16_t rawValue = data.rawTempData[y * data.width + x];
            totalTemp += convertRawToTemperature(rawValue);
            count++;
        }
    }
    
    return count > 0 ? totalTemp / count : -273.15;
}

double CameraManager::getMaxTemperature(const cv::Rect& region) const
{
    QMutexLocker locker(&irCamera_.thermalMutex);
    
    const ThermalData& data = irCamera_.latestThermalData;
    if (!data.isValid || !data.rawTempData) {
        return -273.15;
    }
    
    cv::Rect validRegion = region & cv::Rect(0, 0, data.width, data.height);
    if (validRegion.area() <= 0) {
        return -273.15;
    }
    
    double maxTemp = -273.15;
    
    for (int y = validRegion.y; y < validRegion.y + validRegion.height; ++y) {
        for (int x = validRegion.x; x < validRegion.x + validRegion.width; ++x) {
            uint16_t rawValue = data.rawTempData[y * data.width + x];
            double temp = convertRawToTemperature(rawValue);
            maxTemp = std::max(maxTemp, temp);
        }
    }
    
    return maxTemp;
}

double CameraManager::getMinTemperature(const cv::Rect& region) const
{
    QMutexLocker locker(&irCamera_.thermalMutex);
    
    const ThermalData& data = irCamera_.latestThermalData;
    if (!data.isValid || !data.rawTempData) {
        return -273.15;
    }
    
    cv::Rect validRegion = region & cv::Rect(0, 0, data.width, data.height);
    if (validRegion.area() <= 0) {
        return -273.15;
    }
    
    double minTemp = 1000.0; // 初始化为一个很高的温度
    
    for (int y = validRegion.y; y < validRegion.y + validRegion.height; ++y) {
        for (int x = validRegion.x; x < validRegion.x + validRegion.width; ++x) {
            uint16_t rawValue = data.rawTempData[y * data.width + x];
            double temp = convertRawToTemperature(rawValue);
            minTemp = std::min(minTemp, temp);
        }
    }
    
    return minTemp < 1000.0 ? minTemp : -273.15;
}

void CameraManager::setTemperatureThreshold(double threshold)
{
    QMutexLocker locker(&irCamera_.thermalMutex);
    temperatureThreshold_ = threshold;
    qDebug() << "Temperature threshold set to:" << threshold << "°C";
}

PCBDetectionResult CameraManager::detectPCBComponents(const cv::Mat& image)
{
    PCBDetectionResult result;
    result.timestamp = QDateTime::currentDateTime();
    
    if (image.empty()) {
        qWarning() << "Empty image provided for PCB detection";
        return result;
    }
    
    try {
        // 预处理图像
        cv::Mat processedImage = preprocessImageForDetection(image);
        
        // 这里应该集成YOLO模型进行PCB元件检测
        // 目前使用简单的轮廓检测作为占位符
        QVector<cv::Rect> components = detectComponentRegions(processedImage);
        
        result.detectedImage = image.clone();
        result.componentBounds = components;
        
        // 为每个检测到的元件生成基本信息
        for (int i = 0; i < components.size(); ++i) {
            const cv::Rect& rect = components[i];
            
            // 根据大小和形状推测元件类型（简单启发式）
            QString componentType = "Unknown";
            double confidence = 0.5;
            
            double area = rect.area();
            double aspectRatio = static_cast<double>(rect.width) / rect.height;
            
            if (area < 100) {
                componentType = "Small Component (R/C)";
                confidence = 0.6;
            } else if (area < 500 && aspectRatio > 0.8 && aspectRatio < 1.2) {
                componentType = "IC Package";
                confidence = 0.7;
            } else if (aspectRatio > 2.0) {
                componentType = "Resistor";
                confidence = 0.65;
            } else {
                componentType = "Capacitor";
                confidence = 0.6;
            }
            
            result.detectedComponents.append(componentType);
            result.confidences.append(confidence);
            
            // 在图像上绘制检测框
            cv::rectangle(result.detectedImage, rect, cv::Scalar(0, 255, 0), 2);
            cv::putText(result.detectedImage, componentType.toStdString(), 
                       cv::Point(rect.x, rect.y - 5), cv::FONT_HERSHEY_SIMPLEX, 
                       0.5, cv::Scalar(0, 255, 0), 1);
        }
        
        result.isValid = true;
        qDebug() << "PCB detection completed. Found" << components.size() << "components";
        
        emit pcbDetectionCompleted(result);
        
    } catch (const cv::Exception& e) {
        qWarning() << "OpenCV error in PCB detection:" << e.what();
        result.isValid = false;
    }
    
    return result;
}

QString CameraManager::identifyPCBModel(const cv::Mat& image)
{
    if (image.empty()) {
        return "Unknown";
    }
    
    // 这里应该集成PCB型号识别算法
    // 目前返回占位符结果
    QString model = "PCB_MODEL_UNKNOWN";
    double confidence = 0.5;
    
    // 可以在这里调用现有的PCBIdentifier
    emit pcbModelIdentified(model, confidence);
    
    return model;
}

void CameraManager::setHDCameraParams(int width, int height, int fps)
{
    hdCamera_.width = width;
    hdCamera_.height = height;
    hdCamera_.fps = fps;
    
    if (hdCamera_.status == CameraStatus::STREAMING) {
        // 如果摄像头正在运行，重新配置
        hdCamera_.capture.set(cv::CAP_PROP_FRAME_WIDTH, width);
        hdCamera_.capture.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        hdCamera_.capture.set(cv::CAP_PROP_FPS, fps);
    }
}

void CameraManager::setIRCameraParams(const QString& ipAddress)
{
    irCamera_.ipAddress = ipAddress;
    irCamera_.ipAddressStd = ipAddress.toStdString();
}

bool CameraManager::saveImage(CameraType type, const QString& filePath)
{
    ImageData data = getLatestImage(type);
    if (!data.isValid || data.image.empty()) {
        return false;
    }
    
    try {
        return cv::imwrite(filePath.toStdString(), data.image);
    } catch (const cv::Exception& e) {
        qWarning() << "Failed to save image:" << e.what();
        return false;
    }
}

bool CameraManager::saveThermalData(const QString& filePath)
{
    ThermalData data = getLatestThermalData();
    if (!data.isValid) {
        return false;
    }
      try {
        // 保存原始红外图像和JET热图
        QString irImagePath = filePath + "_ir.png";
        QString thermalColorMapPath = filePath + "_thermal_colormap.png";
        
        if (!data.thermalImage.empty()) {
            cv::imwrite(irImagePath.toStdString(), data.thermalImage);
        }
        if (!data.thermalColorMap.empty()) {
            cv::imwrite(thermalColorMapPath.toStdString(), data.thermalColorMap);
        }
        
        // 保存温度数据到CSV文件
        QString csvPath = filePath + "_temperature.csv";
        QFile csvFile(csvPath);
        if (csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&csvFile);
            stream << "X,Y,Temperature\n";
            
            for (uint32_t y = 0; y < data.height; ++y) {
                for (uint32_t x = 0; x < data.width; ++x) {
                    if (data.rawTempData) {
                        uint16_t rawValue = data.rawTempData[y * data.width + x];
                        double temp = convertRawToTemperature(rawValue);
                        stream << x << "," << y << "," << temp << "\n";
                    }
                }
            }
            csvFile.close();
            return true;
        }
    } catch (const cv::Exception& e) {
        qWarning() << "Failed to save thermal data:" << e.what();
    }
    
    return false;
}

// 私有方法实现

bool CameraManager::initializeHDCamera()
{
    try {
        // 先释放之前的资源
        if (hdCamera_.capture.isOpened()) {
            hdCamera_.capture.release();
        }
        
        qDebug() << "Initializing HD camera...";
        
        // 优先使用DSHOW后端，这是Windows上最稳定的选择
        qDebug() << "Trying to open camera with DSHOW backend...";
        hdCamera_.capture.open(0, cv::CAP_DSHOW);
        
        if (!hdCamera_.capture.isOpened()) {
            qDebug() << "DSHOW backend failed, trying other camera indices...";
            
            // 尝试其他摄像头索引，但保持使用DSHOW后端
            for (int i = 1; i < 4; ++i) {
                qDebug() << "Trying camera index" << i << "with DSHOW...";
                hdCamera_.capture.open(i, cv::CAP_DSHOW);
                if (hdCamera_.capture.isOpened()) {
                    qDebug() << "Successfully opened camera" << i << "with DSHOW backend";
                    break;
                }
            }
        }
        
        if (!hdCamera_.capture.isOpened()) {
            qWarning() << "Failed to open HD camera with DSHOW backend";
            hdCamera_.status = CameraStatus::ERROR;
            emit cameraStatusChanged(CameraType::HD_CAMERA, hdCamera_.status);
            return false;
        }
        
        // 设置摄像头参数 - 按照DetectCamera的方式设置
        qDebug() << "Setting camera parameters: " << hdCamera_.width << "x" << hdCamera_.height << "@" << hdCamera_.fps;
        
        hdCamera_.capture.set(cv::CAP_PROP_FRAME_WIDTH, hdCamera_.width);
        hdCamera_.capture.set(cv::CAP_PROP_FRAME_HEIGHT, hdCamera_.height);
        hdCamera_.capture.set(cv::CAP_PROP_FPS, hdCamera_.fps);
        
        // 不设置缓冲区大小和FOURCC，让系统使用默认值
        
        // 验证参数是否设置成功
        double actualWidth = hdCamera_.capture.get(cv::CAP_PROP_FRAME_WIDTH);
        double actualHeight = hdCamera_.capture.get(cv::CAP_PROP_FRAME_HEIGHT);
        double actualFPS = hdCamera_.capture.get(cv::CAP_PROP_FPS);
        
        qDebug() << "Actual camera parameters: " << actualWidth << "x" << actualHeight << "@" << actualFPS;
        
        // 测试读取一帧 - 使用简单的>>操作符而不是grab/retrieve
        cv::Mat testFrame;
        hdCamera_.capture >> testFrame;
        if (!testFrame.empty()) {
            qDebug() << "Test frame captured successfully, size:" << testFrame.cols << "x" << testFrame.rows;
        } else {
            qWarning() << "Failed to capture test frame, but camera is opened";
        }
        
        hdCamera_.status = CameraStatus::CONNECTED;
        emit cameraStatusChanged(CameraType::HD_CAMERA, hdCamera_.status);
        
        qDebug() << "HD camera initialized successfully";
        return true;
        
    } catch (const cv::Exception& e) {
        qWarning() << "OpenCV error initializing HD camera:" << e.what();
        hdCamera_.status = CameraStatus::ERROR;
        emit cameraStatusChanged(CameraType::HD_CAMERA, hdCamera_.status);
        return false;
    } catch (const std::exception& e) {
        qWarning() << "Standard error initializing HD camera:" << e.what();
        hdCamera_.status = CameraStatus::ERROR;
        emit cameraStatusChanged(CameraType::HD_CAMERA, hdCamera_.status);
        return false;
    }
}

bool CameraManager::initializeIRCamera()
{
#ifdef _WIN32
    try {
        if (RtNet_Init() != 0) {
            qWarning() << "Failed to initialize IR camera SDK";
            irCamera_.status = CameraStatus::ERROR;
            emit cameraStatusChanged(CameraType::IR_CAMERA, irCamera_.status);
            return false;
        }
        
        irCamera_.status = CameraStatus::CONNECTED;
        emit cameraStatusChanged(CameraType::IR_CAMERA, irCamera_.status);
        
        qDebug() << "IR camera initialized successfully";
        return true;
        
    } catch (const std::exception& e) {
        qWarning() << "Error initializing IR camera:" << e.what();
        irCamera_.status = CameraStatus::ERROR;
        emit cameraStatusChanged(CameraType::IR_CAMERA, irCamera_.status);
        return false;
    }
#else
    qWarning() << "IR camera is only supported on Windows platform";
    irCamera_.status = CameraStatus::ERROR;
    emit cameraStatusChanged(CameraType::IR_CAMERA, irCamera_.status);
    return false;
#endif
}

bool CameraManager::startHDCamera()
{
    if (hdCamera_.status != CameraStatus::CONNECTED) {
        if (!initializeHDCamera()) {
            return false;
        }
    }
    
    if (hdCamera_.isRunning) {
        return true; // 已经在运行
    }
    
    // 清空帧队列
    {
        std::lock_guard<std::mutex> lock(hdCamera_.frameMutex);
        while (!hdCamera_.frameQueue.empty()) {
            hdCamera_.frameQueue.pop();
        }
    }
    
    hdCamera_.isRunning = true;
    
    // 启动捕获线程和处理线程
    hdCamera_.captureThread = std::make_unique<std::thread>(&CameraManager::hdCaptureLoop, this);
    hdCamera_.processThread = std::make_unique<std::thread>(&CameraManager::hdProcessLoop, this);
    
    hdCamera_.status = CameraStatus::STREAMING;
    emit cameraStatusChanged(CameraType::HD_CAMERA, hdCamera_.status);
    
    qDebug() << "HD camera started with dual-thread mode";
    return true;
}

bool CameraManager::startIRCamera()
{
#ifdef _WIN32
    if (irCamera_.status != CameraStatus::CONNECTED) {
        if (!initializeIRCamera()) {
            return false;
        }
    }
    
    if (irCamera_.isRunning) {
        return true; // 已经在运行
    }
    
    try {
        const char* ip = irCamera_.ipAddressStd.c_str();
        
        irCamera_.rgbStreamer = RtNet_StartRgbJpegStream(ip, rgbJpegCallback, this);
        irCamera_.irStreamer = RtNet_StartIrJpegStream(ip, irJpegCallback, this);
        irCamera_.tempStreamer = RtNet_StartTemperatureStream(ip, temperatureCallback, this);
        
        irCamera_.isRunning = true;
        irCamera_.status = CameraStatus::STREAMING;
        emit cameraStatusChanged(CameraType::IR_CAMERA, irCamera_.status);
        
        qDebug() << "IR camera started";
        return true;
        
    } catch (const std::exception& e) {
        qWarning() << "Error starting IR camera:" << e.what();
        irCamera_.status = CameraStatus::ERROR;
        emit cameraStatusChanged(CameraType::IR_CAMERA, irCamera_.status);
        return false;
    }
#else
    return false;
#endif
}

bool CameraManager::stopHDCamera()
{
    if (!hdCamera_.isRunning) {
        qDebug() << "HD camera is already stopped";
        return true;
    }
    
    qDebug() << "Stopping HD camera...";
    hdCamera_.isRunning = false;
    
    // 等待线程结束，参考DetectCamera的方式
    if (hdCamera_.captureThread && hdCamera_.captureThread->joinable()) {
        qDebug() << "Waiting for capture thread to finish...";
        hdCamera_.captureThread->join();
        hdCamera_.captureThread.reset();
    }
    
    if (hdCamera_.processThread && hdCamera_.processThread->joinable()) {
        qDebug() << "Waiting for process thread to finish...";
        hdCamera_.processThread->join();
        hdCamera_.processThread.reset();
    }
    
    // 释放摄像头资源
    if (hdCamera_.capture.isOpened()) {
        hdCamera_.capture.release();
        qDebug() << "HD camera capture released";
    }
    
    // 清空队列 - 参考DetectCamera的清空方式
    {
        std::lock_guard<std::mutex> lock(hdCamera_.frameMutex);
        std::queue<cv::Mat> emptyQueue;
        std::swap(hdCamera_.frameQueue, emptyQueue);
        hdCamera_.currentFrame = cv::Mat();
    }
    
    hdCamera_.status = CameraStatus::DISCONNECTED;
    emit cameraStatusChanged(CameraType::HD_CAMERA, hdCamera_.status);
    
    qDebug() << "HD camera stopped successfully";
    return true;
}

bool CameraManager::stopIRCamera()
{
#ifdef _WIN32
    if (!irCamera_.isRunning) {
        return true;
    }
    
    try {
        RtNet_StopRgbJpegStream(irCamera_.rgbStreamer);
        RtNet_StopIrJpegStream(irCamera_.irStreamer);
        RtNet_StopTemperatureData(irCamera_.tempStreamer);
        RtNet_Exit();
        
        irCamera_.isRunning = false;
        irCamera_.status = CameraStatus::DISCONNECTED;
        emit cameraStatusChanged(CameraType::IR_CAMERA, irCamera_.status);
        
        qDebug() << "IR camera stopped";
        return true;
        
    } catch (const std::exception& e) {
        qWarning() << "Error stopping IR camera:" << e.what();
        return false;
    }
#else
    return false;
#endif
}

void CameraManager::hdCaptureLoop()
{
    qDebug() << "HD camera capture thread started";
    
    int captureCount = 0;
    while (hdCamera_.isRunning) {
        try {
            cv::Mat frame;
            
            // 使用简单的>>操作符，参考DetectCamera的成功实现
            hdCamera_.capture >> frame;
            
            if (!frame.empty()) {
                captureCount++;
                if (captureCount % 30 == 0) { // 每30帧输出一次调试信息
                    qDebug() << "HD camera captured frame" << captureCount 
                             << "- Size:" << frame.cols << "x" << frame.rows;
                }
                // 添加图像翻转（如果需要）- 参考DetectCamera的实现
                cv::rotate(frame, frame, cv::ROTATE_180);
                
                // 检查队列大小，防止内存溢出
                {
                    std::lock_guard<std::mutex> lock(hdCamera_.frameMutex);
                    
                    // 如果队列满了，移除最旧的帧
                    while (hdCamera_.frameQueue.size() >= hdCamera_.maxQueueSize) {
                        hdCamera_.frameQueue.pop();
                    }
                    
                    // 添加新帧到队列
                    hdCamera_.frameQueue.push(frame.clone());
                    hdCamera_.currentFrame = frame.clone();
                }
            } else {
                qDebug() << "Empty frame captured from HD camera";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
        } catch (const cv::Exception& e) {
            qWarning() << "OpenCV exception in HD capture:" << e.what();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } catch (const std::exception& e) {
            qWarning() << "Standard exception in HD capture:" << e.what();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // 控制帧率 - 使用DetectCamera的延迟方式
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / hdCamera_.fps));
    }
    
    qDebug() << "HD camera capture thread ended";
}

void CameraManager::hdProcessLoop()
{
    qDebug() << "HD camera process thread started";
    
    while (hdCamera_.isRunning) {
        cv::Mat frame;
        
        // 从队列中获取帧
        {
            std::lock_guard<std::mutex> lock(hdCamera_.frameMutex);
            if (!hdCamera_.frameQueue.empty()) {
                frame = hdCamera_.frameQueue.front();
                hdCamera_.frameQueue.pop();
            }
        }
        
        if (!frame.empty()) {
            processHDFrame(frame);
        } else {
            // 如果没有帧可处理，短暂休眠
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    qDebug() << "HD camera process thread ended";
}

void CameraManager::processHDFrame(const cv::Mat& frame)
{
    qDebug() << "Processing HD frame - Size:" << frame.cols << "x" << frame.rows 
             << "Channels:" << frame.channels() << "Type:" << frame.type();
    
    if (frame.empty()) {
        qWarning() << "Received empty frame from HD camera";
        return;
    }
    
    try {
        QMutexLocker locker(&hdCamera_.imageMutex);
        
        // 确保图像格式正确
        cv::Mat processedFrame;
        if (frame.channels() == 3) {
            // BGR转RGB
            cv::cvtColor(frame, processedFrame, cv::COLOR_BGR2RGB);
        } else if (frame.channels() == 1) {
            // 灰度图转RGB
            cv::cvtColor(frame, processedFrame, cv::COLOR_GRAY2RGB);
        } else {
            processedFrame = frame.clone();
        }
        
        // 确保图像数据连续
        if (!processedFrame.isContinuous()) {
            processedFrame = processedFrame.clone();
        }
          hdCamera_.latestImage.image = processedFrame.clone();
        hdCamera_.latestImage.timestamp = QDateTime::currentDateTime();
        hdCamera_.latestImage.isValid = true;
        
        qDebug() << "Emitting newImageAvailable signal for HD camera - Image size:" 
                 << hdCamera_.latestImage.image.cols << "x" << hdCamera_.latestImage.image.rows
                 << "Valid:" << hdCamera_.latestImage.isValid;
        
        // 发射信号到UI线程
        emit newImageAvailable(CameraType::HD_CAMERA, hdCamera_.latestImage);
        
    } catch (const cv::Exception& e) {
        qWarning() << "Error processing HD frame:" << e.what();
    } catch (const std::exception& e) {
        qWarning() << "Standard error processing HD frame:" << e.what();
    }
}

void CameraManager::processThermalFrame(const ThermalData& data)
{
    // 检查温度阈值
    if (data.maxTemp > temperatureThreshold_) {
        // 找到最高温度的位置
        cv::Point maxLoc;
        double maxVal;
        cv::minMaxLoc(data.temperatureMap, nullptr, &maxVal, nullptr, &maxLoc);
        
        emit temperatureAlert(data.maxTemp, maxLoc);
    }
    
    emit newThermalDataAvailable(data);
}

double CameraManager::convertRawToTemperature(uint16_t rawValue) const
{
    // IR温度原始数据单位已经是摄氏度，只需要进行数值转换
    return rawValue / 10.0; // 直接转换为摄氏度（数据已经是摄氏度单位）
}

void CameraManager::calculateTemperatureStats(ThermalData& data) const
{
    if (!data.rawTempData || data.width == 0 || data.height == 0) {
        return;
    }
    
    double minTemp = 1000.0;
    double maxTemp = -273.15;
    double totalTemp = 0.0;
    uint32_t validPixels = 0;
    
    for (uint32_t i = 0; i < data.width * data.height; ++i) {
        double temp = convertRawToTemperature(data.rawTempData[i]);
        
        if (temp > -273.0 && temp < 500.0) { // 合理的温度范围
            minTemp = std::min(minTemp, temp);
            maxTemp = std::max(maxTemp, temp);
            totalTemp += temp;
            validPixels++;
        }
    }
    
    if (validPixels > 0) {
        data.minTemp = minTemp;
        data.maxTemp = maxTemp;
        data.avgTemp = totalTemp / validPixels;
    }
}

cv::Mat CameraManager::preprocessImageForDetection(const cv::Mat& image)
{
    cv::Mat processed;
    
    // 转换为灰度图
    if (image.channels() == 3) {
        cv::cvtColor(image, processed, cv::COLOR_BGR2GRAY);
    } else {
        processed = image.clone();
    }
    
    // 高斯模糊降噪
    cv::GaussianBlur(processed, processed, cv::Size(5, 5), 1.0);
    
    // 直方图均衡化增强对比度
    cv::equalizeHist(processed, processed);
    
    return processed;
}

QVector<cv::Rect> CameraManager::detectComponentRegions(const cv::Mat& image)
{
    QVector<cv::Rect> components;
    
    try {
        cv::Mat binary;
        
        // 自适应二值化
        cv::adaptiveThreshold(image, binary, 255, 
                             cv::ADAPTIVE_THRESH_GAUSSIAN_C, 
                             cv::THRESH_BINARY, 11, 2);
        
        // 形态学操作
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);
        
        // 查找轮廓
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        // 过滤轮廓
        for (const auto& contour : contours) {
            cv::Rect boundingRect = cv::boundingRect(contour);
            double area = cv::contourArea(contour);
            
            // 过滤太小或太大的区域
            if (area > 50 && area < 10000 && 
                boundingRect.width > 5 && boundingRect.height > 5) {
                components.append(boundingRect);
            }
        }
        
    } catch (const cv::Exception& e) {
        qWarning() << "Error in component detection:" << e.what();
    }
    
    return components;
}

// 静态回调函数实现

void CameraManager::rgbJpegCallback(uint8_t *data, uint32_t dataLen, 
                                   uint32_t width, uint32_t height, 
                                   uint64_t pts, void *arg)
{
    CameraManager* manager = static_cast<CameraManager*>(arg);
    
    try {
        cv::Mat image = cv::imdecode(cv::Mat(1, dataLen, CV_8UC1, data), cv::IMREAD_COLOR);
        if (!image.empty()) {
            QMutexLocker locker(&manager->irCamera_.thermalMutex);
            
            ImageData imageData;
            imageData.image = image;
            imageData.timestamp = QDateTime::currentDateTime();
            imageData.isValid = true;
            
            emit manager->newImageAvailable(CameraType::IR_CAMERA, imageData);
        }
    } catch (const cv::Exception& e) {
        qWarning() << "Error processing RGB JPEG:" << e.what();
    }
}

void CameraManager::irJpegCallback(uint8_t *data, uint32_t dataLen, 
                                  uint32_t width, uint32_t height, 
                                  uint64_t pts, void *arg)
{
    CameraManager* manager = static_cast<CameraManager*>(arg);
    
    try {
        cv::Mat image = cv::imdecode(cv::Mat(1, dataLen, CV_8UC1, data), cv::IMREAD_COLOR);
        if (!image.empty()) {
            QMutexLocker locker(&manager->irCamera_.thermalMutex);
            manager->irCamera_.latestThermalData.thermalImage = image;
            manager->irCamera_.latestThermalData.timestamp = QDateTime::currentDateTime();
        }
    } catch (const cv::Exception& e) {
        qWarning() << "Error processing IR JPEG:" << e.what();
    }
}

void CameraManager::temperatureCallback(uint16_t *paru16Data, uint32_t u32Width, 
                                       uint32_t u32Height, uint64_t u64Pts, void *pArg)
{
    CameraManager* manager = static_cast<CameraManager*>(pArg);
    
    QMutexLocker locker(&manager->irCamera_.thermalMutex);
    
    ThermalData& data = manager->irCamera_.latestThermalData;
    data.rawTempData = paru16Data;
    data.width = u32Width;
    data.height = u32Height;
    data.timestamp = QDateTime::currentDateTime();
    data.isValid = true;
    
    // 创建温度映射图像
    if (paru16Data && u32Width > 0 && u32Height > 0) {
        data.temperatureMap = cv::Mat(u32Height, u32Width, CV_16UC1, paru16Data).clone();
        
        // 计算温度统计信息
        manager->calculateTemperatureStats(data);
          // 创建可视化的热图
        cv::Mat normalizedTemp;
        data.temperatureMap.convertTo(normalizedTemp, CV_8UC1, 1.0 / 60.0);
        cv::applyColorMap(normalizedTemp, data.thermalColorMap, cv::COLORMAP_JET);
        
        // 处理温度数据
        manager->processThermalFrame(data);
    }
}

void CameraManager::onHDCameraFrame()
{
    // This slot can be used to trigger HD camera frame processing
    // Currently the frame processing is handled in the capture thread
    // This can be used for external frame triggers if needed
    qDebug() << "HD camera frame trigger received";
}

void CameraManager::onIRCameraFrame()
{
    // This slot can be used to trigger IR camera frame processing
    // Currently the frame processing is handled in the callbacks
    // This can be used for external frame triggers if needed
    qDebug() << "IR camera frame trigger received";
}

void CameraManager::processTemperatureData()
{
    // Process the latest temperature data
    ThermalData data = getLatestThermalData();
    if (data.isValid) {
        processThermalFrame(data);
        qDebug() << "Temperature data processed - Avg:" << data.avgTemp 
                 << "Min:" << data.minTemp << "Max:" << data.maxTemp;
    }
}
