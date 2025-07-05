#include "detectcamera.h"
#include <iostream>

DetectCamera::DetectCamera(QObject *parent)
    : QObject(parent)
{
    m_delay = 1000 / m_fps;
}

DetectCamera::~DetectCamera()
{
    stop();
}

void DetectCamera::setParams(int width, int height, int fps)
{
    if (!m_running) {
        m_width = width;
        m_height = height;
        m_fps = fps;
        m_delay = 1000 / fps;
    } else {
        std::cout << "Cannot change parameters while camera is running" << std::endl;
    }
}

void DetectCamera::start()
{
    std::cout << "Opening camera..." << std::endl;

    // 使用DirectShow后端打开摄像头
    m_cap.open(0 + cv::CAP_DSHOW);
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH, m_width);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, m_height);
    m_cap.set(cv::CAP_PROP_FPS, m_fps);
    
    if (!m_cap.isOpened()) {
        std::cout << "Failed to open camera!" << std::endl;
        return;
    }

    std::cout << "Camera opened successfully." << std::endl;

    m_running = true;
    m_captureThread = std::thread(&DetectCamera::captureFrame, this);
    m_processThread = std::thread(&DetectCamera::processFrame, this);
}

void DetectCamera::stop()
{
    m_running = false;

    if (m_captureThread.joinable())
        m_captureThread.join();
    if (m_processThread.joinable())
        m_processThread.join();

    if (m_cap.isOpened()) {
        m_cap.release();
    }

    // 清空帧队列
    std::queue<cv::Mat> empty;
    std::swap(m_frames, empty);
}

void DetectCamera::captureFrame()
{
    while (m_running) {
        cv::Mat frame;
        m_cap >> frame;
        
        cv::flip(frame, frame, 0);
        
        if (!frame.empty()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_frames.push(frame.clone());
            m_currentFrame = frame.clone();  // 更新当前帧
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(m_delay));
    }
}

void DetectCamera::processFrame()
{
    while (m_running) {
        cv::Mat frame;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_frames.empty()) {
                frame = m_frames.front();
                m_frames.pop();
            }
        }
        
        if (!frame.empty()) {
            emit frameReady(frame);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(m_delay));
    }
}

cv::Mat DetectCamera::getCurrentFrame()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentFrame.clone();
}

bool DetectCamera::isRunning() const
{
    return m_running;
} 