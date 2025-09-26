#ifndef YOLOMODEL_H
#define YOLOMODEL_H

#include <opencv2/opencv.hpp>
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <iostream>
#include <QString>
#include <QByteArray>
#include "ClassList.h"
using namespace std;

class YOLOModel
{

public:
    static std::shared_ptr<YOLOModel>& getInstance(){
        static std::shared_ptr<YOLOModel> instance = std::make_shared<YOLOModel>();
        return instance;
    }
    
    YOLOModel();
    ~YOLOModel();
    cv::Mat recognize(const cv::Mat& src_img);

    void set_class_display(const std::vector<int>& class_display);

    std::vector<Label> get_labels();

    const std::vector<std::string>& get_class_names() const { return class_name; }
    const std::vector<cv::Scalar>& get_colors() const { return color; }
    const std::vector<int>& get_class_display() const { return class_display; }

private:
    void loadModel();
    std::unique_ptr<Ort::Env> env;
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memory_info;
    std::wstring model_path = L"C:/Users/Administrator/FaultDetect_new/PCB_Components_Detect/model/best.onnx";
    std::vector<int> class_display = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    std::vector<Label> labels;

    std::vector<float> img2vector(const cv::Mat& img);
    void print_float_data(const float* const pdata, int data_num_per_line = 6, int data_num = 300);
    std::vector<std::vector<float>> float2vector(const float* const pdata, int data_num_per_line = 6, int data_num = 100, float conf = 0.6);
    void draw_box(cv::Mat& img, const std::vector<std::vector<float>>& info);
    // 在透视展开后的坐标系中得到的检测框，映射回原图并绘制
    void draw_box_mapped(cv::Mat& original,
                         const std::vector<std::vector<float>>& info,
                         const cv::Size& warped_size,
                         const cv::Mat& Hinv);
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
#endif // YOLOMODEL_H
