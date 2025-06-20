#ifndef PCBCOMPONENTSDETECT_H
#define PCBCOMPONENTSDETECT_H

#include <QWidget>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <onnxruntime/onnxruntime_run_options_config_keys.h>
#include <string>
#include <vector>
#include "detectcamera.h"
#include "yolomodel.h"
#include "imageviewer.h"
#include <atomic>
#include <mutex>
#include <condition_variable>
using namespace std;
using namespace cv;

namespace Ui {
class PCBComponentsDetect;
}

class PCBComponentsDetect : public QWidget
{
    Q_OBJECT

public:
    static std::shared_ptr<PCBComponentsDetect> getInstance(QWidget *parent = nullptr) {
        static std::shared_ptr<PCBComponentsDetect> instance = std::make_shared<PCBComponentsDetect>(parent);
        return instance;
    }

    PCBComponentsDetect(QWidget *parent);
    ~PCBComponentsDetect();
    PCBComponentsDetect(const PCBComponentsDetect&) = delete;
    PCBComponentsDetect& operator=(const PCBComponentsDetect&) = delete;

    void startrecog();

public slots:
    void displayFrame(const cv::Mat& frame);
    void on_pbsave_clicked();
    void on_pbcancle_clicked();

signals:
    void signal_save_image_label(const cv::Mat& image, const std::vector<Label>& labels);

protected:
    void closeEvent(QCloseEvent *event) override;
    
private:
    Ui::PCBComponentsDetect *ui;
    std::atomic<bool> stop_recog = true;
    std::mutex mtx;
    std::shared_ptr<ImageViewer> imageViewer;
    cv::Mat recog_image;
    cv::Mat receive_image;
    bool is_display = false;
    bool is_recog = true;
    bool pause_recog = false;
    std::condition_variable cv;
    std::shared_ptr<YOLOModel>& yolomodel;
    std::vector<Label> labels;

    void recog();
    std::shared_ptr<std::thread> recogThread;
    std::vector<int> class_display = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    bool is_pressed[16] = { true }; // 记录每个按钮的状态
    std::vector<std::string> class_name = {"Capacitor", "IC", "LED", "Resistor", "battery", "buzzer", "clock", "connector", "diode", "display", "fuse", "inductor", "potentiometer", "relay", "switch", "transistor"};
    std::vector<cv::Scalar> color = {
        cv::Scalar(193, 182, 255),
        cv::Scalar(173, 216, 230),
        cv::Scalar(240, 230, 140),
        cv::Scalar(144, 238, 144),
        cv::Scalar(255, 160, 122),
        cv::Scalar(221, 160, 221),
        cv::Scalar(135, 206, 250),
        cv::Scalar(180, 105, 255),
        cv::Scalar(147, 20, 255),
        cv::Scalar(0, 255, 127),
        cv::Scalar(0, 165, 255),
        cv::Scalar(0, 215, 255),
        cv::Scalar(170, 178, 32),
        cv::Scalar(205, 90, 106),
        cv::Scalar(113, 179, 60),
        cv::Scalar(238, 130, 238)
    };
};

#endif // PCBCOMPONENTSDETECT_H
