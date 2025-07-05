#ifndef CLASSLIST_H
#define CLASSLIST_H

#include <QString>
#include <QByteArray>
#include <QStringList>
#include <algorithm>  // for std::max, std::min
#include <QMap>
#include <QVariant>


// PCB元件标签结构体
struct Label {
    int id;                      // 标签ID
    double x;                       // 边界框左上角X坐标
    double y;                       // 边界框左上角Y坐标
    double w;                       // 边界框宽度
    double h;                       // 边界框高度
    int cls;                     // 类别ID
    double confidence;           // 置信度 [0.0, 1.0]
    QString label;               // 标签文本
    QString position_number;     // 位置编号
    QByteArray notes;           // 备注信息
    QMap<QString, QVariant> parameters; // 参数
    
    // 兼容性字段 (为了兼容旧代码)
    int point_x;                 // 与 x 相同
    int point_y;                 // 与 y 相同
    int width;                   // 与 w 相同
    int height;                  // 与 h 相同
    
    // 构造函数
    Label() : id(-1), x(0), y(0), w(0), h(0), cls(-1), confidence(0.0)
        , point_x(0), point_y(0), width(0), height(0) {}
    
    Label(int id_, double x_, double y_, double w_, double h_, int cls_, double confidence_,
          const QString& label_ = "", const QString& position_ = "", 
          const QByteArray& notes_ = QByteArray())
        : id(id_), x(x_), y(y_), w(w_), h(h_), cls(cls_), confidence(confidence_)
        , label(label_), position_number(position_), notes(notes_)
        , point_x(x_), point_y(y_), width(w_), height(h_) {}
    
    Label(int id_, const QString& label_, double x_, double y_, int w_, int h_, 
          const QString& cls_ = "", const QString& pos_ = "")
        : id(id_), x(static_cast<double>(x_)), y(static_cast<double>(y_)), w(w_), h(h_)
        , cls(-1), confidence(0.0), label(label_), position_number(pos_)
        , point_x(static_cast<double>(x_)), point_y(static_cast<double>(y_)), width(w_), height(h_) {}
    
    // 同步函数：确保兼容性字段与主字段同步
    void syncFields() {
        point_x = x;
        point_y = y;
        width = w;
        height = h;
    }
    
    // 从兼容性字段更新主字段
    void updateFromCompat() {
        x = point_x;
        y = point_y;
        w = width;
        h = height;
    }
    
    // 获取中心点坐标
    double centerX() const { return x + w / 2; }
    double centerY() const { return y + h / 2; }
    
    // 获取右下角坐标
    double right() const { return x + w; }
    double bottom() const { return y + h; }
    
    // 检查标签是否有效
    bool isValid() const { 
        return w > 0 && h > 0 && cls >= 0 && confidence > 0.0; 
    }
    
    // 计算面积
    int area() const { return w * h; }
    
    // 计算与另一个标签的IoU (Intersection over Union)
    double iou(const Label& other) const {
        double left = std::max(x, other.x);
        double top = std::max(y, other.y);
        double right = std::min(x + w, other.x + other.w);
        double bottom = std::min(y + h, other.y + other.h);
        
        if (left >= right || top >= bottom) {
            return 0.0; // 没有重叠
        }
        
        double intersection_area = (right - left) * (bottom - top);
        double union_area = area() + other.area() - intersection_area;
        
        return union_area > 0 ? static_cast<double>(intersection_area) / union_area : 0.0;
    }
    
    // 比较操作符
    bool operator==(const Label& other) const {
        return id == other.id && x == other.x && y == other.y && 
               w == other.w && h == other.h && cls == other.cls;
    }
    
    bool operator!=(const Label& other) const {
        return !(*this == other);
    }
};

// PCB元件类别枚举
enum PCBComponentType {
    CAPACITOR = 0,      // 电容
    IC = 1,             // 集成电路
    LED = 2,            // 发光二极管
    RESISTOR = 3,       // 电阻
    BATTERY = 4,        // 电池
    BUZZER = 5,         // 蜂鸣器
    CLOCK = 6,          // 时钟
    CONNECTOR = 7,      // 连接器
    DIODE = 8,          // 二极管
    DISPLAY = 9,        // 显示器
    FUSE = 10,          // 保险丝
    INDUCTOR = 11,      // 电感器
    POTENTIOMETER = 12, // 电位器
    RELAY = 13,         // 继电器
    SWITCH = 14,        // 开关
    TRANSISTOR = 15     // 晶体管
};

// 获取组件类型名称
inline QString getComponentTypeName(int cls) {
    static const QString component_names[] = {
        "Capacitor", "IC", "LED", "Resistor", "Battery", "Buzzer",
        "Clock", "Connector", "Diode", "Display", "Fuse", "Inductor",
        "Potentiometer", "Relay", "Switch", "Transistor"
    };
    
    if (cls >= 0 && cls < 16) {
        return component_names[cls];
    }
    return "Unknown";
}

// 获取组件类型的中文名称
inline QString getComponentTypeNameCN(int cls) {
    static const QString component_names_cn[] = {
        "电容", "集成电路", "发光二极管", "电阻", "电池", "蜂鸣器",
        "时钟", "连接器", "二极管", "显示器", "保险丝", "电感器",
        "电位器", "继电器", "开关", "晶体管"
    };
    
    if (cls >= 0 && cls < 16) {
        return component_names_cn[cls];
    }
    return "未知";
}

#endif // CLASSLIST_H
