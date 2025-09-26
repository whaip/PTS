#include "yolomodel.h"
#include <filesystem>
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include "../PCB_Model_Identification/pcb_extract.h"

YOLOModel::YOLOModel()
    : env(std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "yolomodel")),
    session_options(),
    session(),
    memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU))
{
    loadModel();
}
YOLOModel::~YOLOModel(){
    env.reset();
    session.reset();
}

void YOLOModel::loadModel(){
    std::cout << "session_options init" << std::endl;
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    
    std::cout << "session_options set" << std::endl;
    OrtStatus* status = OrtSessionOptionsAppendExecutionProvider_CUDA(session_options, 0);
    if (status != nullptr) {
        std::cerr << "CUDA is not available." << std::endl;
    }

    else {
        std::cout << "Using CUDA execution provider." << std::endl;
    }

    try {
        session = std::make_unique<Ort::Session>(*env, model_path.c_str(), session_options);
        std::cout << "session init" << std::endl;
    }
    catch (const Ort::Exception &e) {
        qCritical() << "创建 Ort::Session 失败："
                    << e.what()
                    << "，错误码 =" << e.GetOrtErrorCode();
    }
}
cv::Mat YOLOModel::recognize(const cv::Mat& src_img)
{
    if (src_img.empty()) return src_img;

    cv::Mat working_img = src_img.clone();

    // 先尝试提取PCB并获得单应矩阵
    cv::Mat warped, H, Hinv;
    cv::Size warped_size;
    bool extracted = false;
    try {
        extracted = PCB_EXTRACT->extractWithHomography(working_img, warped, H, warped_size);
    } catch (...) {
        extracted = false;
    }

    cv::Mat inference_img;
    cv::Mat resize_ref_img; // 用于缩放比例参考
    if (extracted && !warped.empty() && H.cols == 3 && H.rows == 3) {
        // 使用透视展开图进行推理
        inference_img = warped;
        // 计算逆单应矩阵，用于将结果映射回原图
        cv::invert(H, Hinv);
        cv::imshow("warped", warped);
        cv::imshow("H", H);
        cv::imshow("Hinv", Hinv);
        cv::waitKey(0);
    } else {
        // 回退到原来的路径：在原图上直接推理
        inference_img = working_img;
    }

    // 按模型输入大小缩放到 640x640
    cv::Mat resized;
    cv::resize(inference_img, resized, cv::Size(640, 640));
    std::vector<float> img_vector = img2vector(resized);
    std::vector<int64_t> dim = { 1, 3, 640, 640 };
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info, img_vector.data(), img_vector.size(), dim.data(), dim.size());

    std::vector<const char*> input_names = { "images" };
    std::vector<const char*> output_names = { "output0" };
    try {
        std::vector<Ort::Value> output_tensors = session->Run(Ort::RunOptions{ nullptr }, input_names.data(), &input_tensor, input_names.size(), output_names.data(), output_names.size());

        float* output = output_tensors[0].GetTensorMutableData<float>();
        auto info = float2vector(output);

        if (extracted && !warped.empty() && Hinv.cols == 3 && Hinv.rows == 3) {
            // 将框映射回原图
            draw_box_mapped(working_img, info, inference_img.size(), Hinv);
            return working_img;
        } else {
            // 维持原有行为：在推理图上绘制
            draw_box(inference_img, info);
            return inference_img;
        }
    }
    catch (const Ort::Exception &e) {
        qDebug() << "ONNX Runtime 错误：" << e.what()
                 << "，错误码 =" << e.GetOrtErrorCode();
        return src_img;  // 降级
    }
}

void YOLOModel::draw_box(cv::Mat& img, const std::vector<std::vector<float>>& info)
{
    float w = img.cols;
    float h = img.rows;
    int line_thickness = std::max(1, static_cast<int>(std::min(w, h) / 300)); // 根据图像大小动态调整线条粗细
    labels.clear();
    for (int i = 0; i < info.size(); i++)
    {
        if(std::find(class_display.begin(), class_display.end(), info[i][5]) == class_display.end())
            continue;
        cv::Point p1(info[i][0] * w / 640.0, info[i][1] * h / 640.0);
        cv::Point p2(info[i][2] * w / 640.0, info[i][3] * h / 640.0);        cv::rectangle(img, p1, p2, color[info[i][5]], line_thickness);
        string label;
        label += class_name[info[i][5]];
        // 修正Label构造函数参数顺序: id, x, y, w, h, cls, confidence, label, position, notes
        labels.push_back(Label(i,                                    // id: 使用循环索引
                              p1.x,                                  // x: 左上角x坐标
                              p1.y,                                  // y: 左上角y坐标
                              abs(p2.x - p1.x),                      // w: 宽度
                              abs(p2.y - p1.y),                      // h: 高度
                              static_cast<int>(info[i][5]),          // cls: 类别ID
                              info[i][4],                            // confidence: 置信度
                              QString::fromStdString(label),         // label: 标签文本
                              "",                                    // position_number: 位置编号
                              QByteArray()));                        // notes: 备注
        label += "  ";
        std::stringstream oss;
        oss << info[i][4];
        label += oss.str();
        int font_scale = std::max(1, static_cast<int>(std::min(w, h) / 1500)); // 增加字体大小以提高可读性
        cv::putText(img, label, cv::Point(info[i][0] * w / 640.0, info[i][1] * h / 640.0 - 20), cv::FONT_HERSHEY_SIMPLEX, font_scale, color[info[i][5]], line_thickness);
    }
}

void YOLOModel::draw_box_mapped(cv::Mat& original,
                         const std::vector<std::vector<float>>& info,
                         const cv::Size& warped_size,
                         const cv::Mat& Hinv)
{
    if (original.empty()) return;
    labels.clear();
    const float w640 = 640.0f;
    const float h640 = 640.0f;
    for (int i = 0; i < static_cast<int>(info.size()); ++i)
    {
        if(std::find(class_display.begin(), class_display.end(), static_cast<int>(info[i][5])) == class_display.end())
            continue;
        int cls = static_cast<int>(info[i][5]);
        if (cls < 0 || cls >= static_cast<int>(class_name.size()) || cls >= static_cast<int>(color.size()))
            continue;

        // 1) 先把 640x640 坐标系的 box 缩放回到 warped 尺度
        float x1 = info[i][0] * static_cast<float>(warped_size.width)  / w640;
        float y1 = info[i][1] * static_cast<float>(warped_size.height) / h640;
        float x2 = info[i][2] * static_cast<float>(warped_size.width)  / w640;
        float y2 = info[i][3] * static_cast<float>(warped_size.height) / h640;

        // 2) 取矩形四个角点并通过逆单应映射回原图
        std::vector<cv::Point2f> warped_pts = {
            {x1, y1}, {x2, y1}, {x2, y2}, {x1, y2}
        };
        std::vector<cv::Point2f> orig_pts;
        cv::perspectiveTransform(warped_pts, orig_pts, Hinv);

        // 3) 使用四个点的外接轴对齐矩形进行绘制与标签生成
        float minx = std::min(std::min(orig_pts[0].x, orig_pts[1].x), std::min(orig_pts[2].x, orig_pts[3].x));
        float maxx = std::max(std::max(orig_pts[0].x, orig_pts[1].x), std::max(orig_pts[2].x, orig_pts[3].x));
        float miny = std::min(std::min(orig_pts[0].y, orig_pts[1].y), std::min(orig_pts[2].y, orig_pts[3].y));
        float maxy = std::max(std::max(orig_pts[0].y, orig_pts[1].y), std::max(orig_pts[2].y, orig_pts[3].y));

        cv::Point p1(std::clamp(static_cast<int>(std::round(minx)), 0, original.cols-1),
                     std::clamp(static_cast<int>(std::round(miny)), 0, original.rows-1));
        cv::Point p2(std::clamp(static_cast<int>(std::round(maxx)), 0, original.cols-1),
                     std::clamp(static_cast<int>(std::round(maxy)), 0, original.rows-1));

        if (p2.x <= p1.x || p2.y <= p1.y) continue;
        cv::rectangle(original, p1, p2, color[cls],
                      std::max(1, std::min(original.cols, original.rows) / 300));

        std::string labelStr = class_name[cls];
        std::stringstream oss; oss << "  " << info[i][4];
        labelStr += oss.str();

        int font_scale = std::max(1, std::min(std::max(original.cols, original.rows), 3000) / 1500);
        cv::putText(original, labelStr, cv::Point(p1.x, std::max(0, p1.y - 20)),
                    cv::FONT_HERSHEY_SIMPLEX, font_scale, color[cls],
                    std::max(1, std::min(original.cols, original.rows) / 300));

        // 同步 labels（使用原图坐标）
        QString qlabel = QString::fromStdString(class_name[cls]);
        labels.push_back(Label(i,
                              p1.x,
                              p1.y,
                              std::abs(p2.x - p1.x),
                              std::abs(p2.y - p1.y),
                              cls,
                              info[i][4],
                              qlabel,
                              "",
                              QByteArray()));
    }
}

std::vector<float> YOLOModel::img2vector(const cv::Mat& img)
{
    std::vector<float> B;

    vector<float> G;
    vector<float> R;
    B.reserve(640 * 640 * 3);
    G.reserve(640 * 640);
    R.reserve(640 * 640);
    const uchar* pdata = (uchar*)img.datastart;
    for (int i = 0; i < img.dataend - img.datastart; i += 3)
    {
        B.push_back((float)*(pdata + i) / 255.0);
        G.push_back((float)*(pdata + i + 1) / 255.0);
        R.push_back((float)*(pdata + i + 2) / 255.0);
    }
    B.insert(B.cend(), G.cbegin(), G.cend());
    B.insert(B.cend(), R.cbegin(), R.cend());
    return B;
}

void YOLOModel::print_float_data(const float* const pdata, int data_num_per_line, int data_num)
{
    for (int i = 0; i < data_num; i++)


    {
        for (int j = 0; j < data_num_per_line; j++)
        {
            cout << *(pdata + i * data_num_per_line + j) << " ";
        }
        cout << endl;
    }
}

std::vector<std::vector<float>> YOLOModel::float2vector(const float* const pdata, int data_num_per_line, int data_num, float conf)
{
    std::vector<std::vector<float>> info;


    vector<float> info_line;
    for (int i = 0; i < data_num; i++)
    {
        if (*(pdata + i * data_num_per_line + 4) < conf)
        {
            continue;
        }
        for (int j = 0; j < data_num_per_line; j++)
        {
            //cout << *(pdata + i * data_num_per_line + j) << " ";
            info_line.push_back(*(pdata + i * data_num_per_line + j));
        }
        info.push_back(info_line);
        info_line.clear();
        //cout << endl;
    }
    return info;
}

void YOLOModel::set_class_display(const std::vector<int>& class_display)
{
    this->class_display = class_display;
}

std::vector<Label> YOLOModel::get_labels(){
    return labels;
}
