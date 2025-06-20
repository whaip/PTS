#include "yolomodel.h"
#include <filesystem>
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <QDebug>

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
        // 你可以在这里 return 或者抛出自定义错误，避免程序继续往下跑
    }
}
cv::Mat YOLOModel::recognize(const cv::Mat& src_img)
{
    cv::Mat img;

    cv::Mat new_img = src_img.clone();
    cv::resize(src_img, img, cv::Size(640, 640));
    // cv::imshow("test", img);
    std::vector<float> img_vector = img2vector(img);
    std::vector<int64_t> dim = { 1, 3, 640, 640 };
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info, img_vector.data(), img_vector.size(), dim.data(), dim.size());

    std::vector<const char*> input_names = { "images" };
    std::vector<const char*> output_names = { "output0" };
    try {
        std::vector<Ort::Value> output_tensors = session->Run(Ort::RunOptions{ nullptr }, input_names.data(), &input_tensor, input_names.size(), output_names.data(), output_names.size());

        float* output = output_tensors[0].GetTensorMutableData<float>();
        auto info = float2vector(output);

        draw_box(new_img, info);

        // cout << "info.size() = " << info.size() << endl;
        return new_img;
    }
    catch (const Ort::Exception &e) {
        qDebug() << "ONNX Runtime 错误：" << e.what()
                 << "，错误码 =" << e.GetOrtErrorCode();
        return src_img;  // 或者其他降级处理
    }
    // cv::imshow("test", img);
    // cv::waitKey(0);
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
