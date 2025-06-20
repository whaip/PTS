#include "labelimageviewdialog.h"
#include <QDebug>
#include <QFileDialog>
#include <QDateTime>
#include <QtConcurrent>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QScrollArea>

LabelImageViewDialog::LabelImageViewDialog(QWidget *parent, CameraManager* cameraManager)
    : QDialog(parent), cameraManager_(cameraManager)
{
    setupUI();
    
    // 连接相机信号
    if (cameraManager_) {
        connect(cameraManager_, &CameraManager::newImageAvailable, 
                this, &LabelImageViewDialog::onFrameReady);
        
        // 直接启动HD摄像头
        if (!cameraManager_->isCameraAvailable(CameraType::HD_CAMERA)) {
            cameraManager_->initializeCamera(CameraType::HD_CAMERA);
        }
        
        if (cameraManager_->isCameraAvailable(CameraType::HD_CAMERA)) {
            cameraManager_->setHDCameraParams(1920, 1080, 30);  // 设置合适的分辨率
            cameraManager_->startCamera(CameraType::HD_CAMERA);
        }
    }
}

LabelImageViewDialog::~LabelImageViewDialog()
{
    if (cameraManager_ && cameraManager_->isCameraAvailable(CameraType::HD_CAMERA)) {
        cameraManager_->stopCamera(CameraType::HD_CAMERA);
    }
}

void LabelImageViewDialog::setupUI()
{
    setWindowTitle(tr("实时标签检测"));
    resize(1000, 600);

    auto mainLayout = new QHBoxLayout(this);
    
    // 左侧预览和按钮区域
    auto leftLayout = new QVBoxLayout();
    
    // 预览标签
    previewLabel = new QLabel(this);
    previewLabel->setMinimumSize(640, 480);
    previewLabel->setAlignment(Qt::AlignCenter);
    leftLayout->addWidget(previewLabel);
    
    // 按钮布局
    auto buttonLayout = new QHBoxLayout();
    captureButton = new QPushButton(tr("拍摄"), this);
    cancelButton = new QPushButton(tr("取消"), this);
    
    buttonLayout->addWidget(captureButton);
    buttonLayout->addWidget(cancelButton);
    leftLayout->addLayout(buttonLayout);
    
    mainLayout->addLayout(leftLayout);
    
    // 右侧标签面板
    setupLabelPanel();
    mainLayout->addWidget(labelPanel);
    
    // 连接按钮信号
    connect(captureButton, &QPushButton::clicked, this, &LabelImageViewDialog::captureImage);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void LabelImageViewDialog::setupLabelPanel()
{
    labelPanel = new QWidget(this);
    auto mainPanelLayout = new QVBoxLayout(labelPanel);
    mainPanelLayout->setSpacing(10);
    mainPanelLayout->setContentsMargins(0, 0, 0, 0);  // 移除外边距
    
    // 标题
    auto titleLabel = new QLabel(tr("标签控制"), labelPanel);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainPanelLayout->addWidget(titleLabel);
    
    // 添加全选/取消按钮
    auto buttonLayout = new QHBoxLayout();
    auto selectAllButton = new QPushButton(tr("全选"), labelPanel);
    auto deselectAllButton = new QPushButton(tr("全部取消"), labelPanel);
    
    // 设置按钮样式
    QFont buttonFont;
    buttonFont.setPointSize(10);
    selectAllButton->setFont(buttonFont);
    deselectAllButton->setFont(buttonFont);
    
    selectAllButton->setMinimumHeight(30);
    deselectAllButton->setMinimumHeight(30);
    
    buttonLayout->addWidget(selectAllButton);
    buttonLayout->addWidget(deselectAllButton);
    mainPanelLayout->addLayout(buttonLayout);
    
    // 添加分隔线
    auto line = new QFrame(labelPanel);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainPanelLayout->addWidget(line);
    
    // 创建滚动区域和内容容器
    auto scrollArea = new QScrollArea(labelPanel);
    auto scrollContent = new QWidget(scrollArea);
    auto scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setAlignment(Qt::AlignTop);
    scrollLayout->setSpacing(5);
    
    // 获取YOLO模型的类别名称和颜色
    const auto& classNames = YOLOModel::getInstance()->get_class_names();
    const auto& colors = YOLOModel::getInstance()->get_colors();
    
    // 创建复选框
    for(size_t i = 0; i < classNames.size(); ++i) {
        auto checkBox = new QCheckBox(QString::fromStdString(classNames[i]), scrollContent);
        checkBox->setChecked(true);
        
        // 设置复选框字体
        QFont checkBoxFont = checkBox->font();
        checkBoxFont.setPointSize(11);
        checkBox->setFont(checkBoxFont);
        
        checkBox->setStyleSheet(getStyleSheet(colors[i]));
        
        connect(checkBox, &QCheckBox::toggled, this, 
                [this, i](bool checked) { onLabelToggled(i, checked); });
        
        labelCheckBoxes.push_back(checkBox);
        scrollLayout->addWidget(checkBox);
    }
    
    // 设置滚动区域属性
    scrollContent->setLayout(scrollLayout);
    scrollArea->setWidget(scrollContent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 设置滚动区域样式
    scrollArea->setStyleSheet(
        "QScrollArea {"
        "    border: none;"
        "}"
        "QScrollBar:vertical {"
        "    border: none;"
        "    background: #F0F0F0;"
        "    width: 10px;"
        "    margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #CCCCCC;"
        "    min-height: 20px;"
        "    border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "    background: none;"
        "}"
    );
    
    mainPanelLayout->addWidget(scrollArea);
    
    // 连接全选/取消按钮信号
    connect(selectAllButton, &QPushButton::clicked, this, &LabelImageViewDialog::selectAllLabels);
    connect(deselectAllButton, &QPushButton::clicked, this, &LabelImageViewDialog::deselectAllLabels);
    
    labelPanel->setMinimumWidth(250);
    labelPanel->setMaximumWidth(300);
}

QString LabelImageViewDialog::getStyleSheet(const cv::Scalar& color)
{
    return QString(
        "QCheckBox {"
        "    color: rgb(%1, %2, %3);"
        "    font-weight: bold;"
        "    padding: 5px;"          // 添加内边距
        "    margin: 2px;"           // 添加外边距
        "}"
        "QCheckBox::indicator {"
        "    width: 16px;"           // 增大指示器大小
        "    height: 16px;"
        "}"
        "QCheckBox::indicator:checked {"
        "    background-color: rgb(%1, %2, %3);"
        "    border: 2px solid gray;"  // 增加边框宽度
        "}"
        "QCheckBox::indicator:unchecked {"
        "    background-color: white;"
        "    border: 2px solid rgb(%1, %2, %3);"  // 增加边框宽度
        "}"
    ).arg(color[2]).arg(color[1]).arg(color[0]); // OpenCV使用BGR顺序
}

void LabelImageViewDialog::onLabelToggled(int index, bool checked)
{
    std::vector<int> displayClasses = YOLOModel::getInstance()->get_class_display();
    
    if(checked) {
        if(std::find(displayClasses.begin(), displayClasses.end(), index) == displayClasses.end()) {
            displayClasses.push_back(index);
        }
    } else {
        displayClasses.erase(
            std::remove(displayClasses.begin(), displayClasses.end(), index),
            displayClasses.end()
        );
    }
    
    YOLOModel::getInstance()->set_class_display(displayClasses);
}

void LabelImageViewDialog::onFrameReady(CameraType type, const ImageData& data)
{
    // 只处理HD摄像头的图像
    if (type != CameraType::HD_CAMERA || !data.isValid || data.image.empty()) {
        return;
    }
    
    // 如果正在处理上一帧，则跳过这一帧
    if(isProcessing.load()) {
        return;
    }
    
    isProcessing.store(true);
    
    // 在新线程中处理图像
    QtConcurrent::run([this, frame = data.image.clone()]() {
        // 保存原始帧
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            originalFrame = frame.clone();
        }
        
        // 使用YOLO模型处理帧
        cv::Mat processedFrame = YOLOModel::getInstance()->recognize(frame);
        
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            currentFrame = processedFrame.clone();
        }
        
        // 更新预览
        QMetaObject::invokeMethod(this, [this]() {
            if(!currentFrame.empty()) {
                QImage img = cvMatToQImage(currentFrame);
                previewLabel->setPixmap(QPixmap::fromImage(img).scaled(
                    previewLabel->size(), 
                    Qt::KeepAspectRatio, 
                    Qt::SmoothTransformation
                ));
            }
        }, Qt::QueuedConnection);
        
        isProcessing.store(false);
    });
}

void LabelImageViewDialog::captureImage()
{
    std::lock_guard<std::mutex> lock(frameMutex);
    if(!currentFrame.empty() && !originalFrame.empty()) {
        // 获取当前帧的标签
        auto labels = YOLOModel::getInstance()->get_labels();
        // 发送原始图像和标签
        emit imageCaptured(originalFrame.clone(), labels);
        accept(); // 关闭对话框
    }
}

QImage LabelImageViewDialog::cvMatToQImage(const cv::Mat& mat)
{
    if(mat.empty()) return QImage();
    
    if(mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(
            rgb.data, 
            rgb.cols, 
            rgb.rows, 
            rgb.step, 
            QImage::Format_RGB888
        ).copy();
    }
    return QImage();
}

void LabelImageViewDialog::selectAllLabels()
{
    for(auto checkBox : labelCheckBoxes) {
        checkBox->setChecked(true);
    }
}

void LabelImageViewDialog::deselectAllLabels()
{
    for(auto checkBox : labelCheckBoxes) {
        checkBox->setChecked(false);
    }
} 