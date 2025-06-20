#include "staticlabelviewdialog.h"
#include <QDebug>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QScrollArea>
#include <QMessageBox>
#include <QGraphicsRectItem>

StaticLabelViewDialog::StaticLabelViewDialog(QWidget *parent, 
                                           const QImage& image,
                                           const std::vector<Label>& existingLabels)
    : QDialog(parent), originalImage(image), currentLabels(existingLabels)
{
    setupUI();
    
    // 转换图像格式
    if (!originalImage.isNull()) {
        cvImage = qImageToCvMat(originalImage);
        updateImageDisplay();
    }
}

StaticLabelViewDialog::~StaticLabelViewDialog()
{
}

void StaticLabelViewDialog::setupUI()
{
    setWindowTitle(tr("标签管理"));
    resize(1200, 800);

    auto mainLayout = new QHBoxLayout(this);
    
    // 左侧图像显示区域
    auto leftLayout = new QVBoxLayout();
    
    // 图像视图
    imageView = new QGraphicsView(this);
    scene = new QGraphicsScene(this);
    imageView->setScene(scene);
    imageView->setMinimumSize(800, 600);
    leftLayout->addWidget(imageView);
    
    // 按钮布局
    auto buttonLayout = new QHBoxLayout();
    detectButton = new QPushButton(tr("检测标签"), this);
    okButton = new QPushButton(tr("确定"), this);
    cancelButton = new QPushButton(tr("取消"), this);
    
    buttonLayout->addWidget(detectButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    leftLayout->addLayout(buttonLayout);
    
    mainLayout->addLayout(leftLayout);
    
    // 右侧标签面板
    setupLabelPanel();
    mainLayout->addWidget(labelPanel);
    
    // 连接按钮信号
    connect(detectButton, &QPushButton::clicked, this, &StaticLabelViewDialog::onDetectLabels);
    connect(okButton, &QPushButton::clicked, this, &StaticLabelViewDialog::onAccept);
    connect(cancelButton, &QPushButton::clicked, this, &StaticLabelViewDialog::onReject);
}

void StaticLabelViewDialog::setupLabelPanel()
{
    labelPanel = new QWidget(this);
    auto mainPanelLayout = new QVBoxLayout(labelPanel);
    mainPanelLayout->setSpacing(10);
    mainPanelLayout->setContentsMargins(0, 0, 0, 0);
    
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
        
        QFont checkBoxFont = checkBox->font();
        checkBoxFont.setPointSize(11);
        checkBox->setFont(checkBoxFont);
        
        checkBox->setStyleSheet(getStyleSheet(colors[i]));
        
        connect(checkBox, &QCheckBox::toggled, this, 
                [this, i](bool checked) { onLabelToggled(i, checked); });
        
        labelCheckBoxes.push_back(checkBox);
        scrollLayout->addWidget(checkBox);
    }
    
    scrollContent->setLayout(scrollLayout);
    scrollArea->setWidget(scrollContent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
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
    connect(selectAllButton, &QPushButton::clicked, this, &StaticLabelViewDialog::selectAllLabels);
    connect(deselectAllButton, &QPushButton::clicked, this, &StaticLabelViewDialog::deselectAllLabels);
    
    labelPanel->setMinimumWidth(250);
    labelPanel->setMaximumWidth(300);
}

QString StaticLabelViewDialog::getStyleSheet(const cv::Scalar& color)
{
    return QString(
        "QCheckBox {"
        "    color: rgb(%1, %2, %3);"
        "    font-weight: bold;"
        "    padding: 5px;"
        "    margin: 2px;"
        "}"
        "QCheckBox::indicator {"
        "    width: 16px;"
        "    height: 16px;"
        "}"
        "QCheckBox::indicator:checked {"
        "    background-color: rgb(%1, %2, %3);"
        "    border: 2px solid gray;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "    background-color: white;"
        "    border: 2px solid rgb(%1, %2, %3);"
        "}"
    ).arg(color[2]).arg(color[1]).arg(color[0]); // OpenCV使用BGR顺序
}

void StaticLabelViewDialog::onLabelToggled(int index, bool checked)
{
    // 更新显示的类别
    updateImageDisplay();
}

void StaticLabelViewDialog::onDetectLabels()
{
    if (cvImage.empty()) {
        QMessageBox::warning(this, "错误", "没有可用的图像进行检测");
        return;
    }
    
    try {
        // 使用YOLO模型检测
        cv::Mat processedImage = YOLOModel::getInstance()->recognize(cvImage);
        detectedLabels = YOLOModel::getInstance()->get_labels();
        
        // 合并现有标签和检测到的标签
        currentLabels.clear();
        currentLabels.insert(currentLabels.end(), detectedLabels.begin(), detectedLabels.end());
        
        // 更新显示
        updateImageDisplay();
        
        QMessageBox::information(this, "检测完成", 
                                QString("检测到 %1 个标签").arg(detectedLabels.size()));
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, "检测错误", 
                             QString("标签检测失败: %1").arg(e.what()));
    }
}

void StaticLabelViewDialog::updateImageDisplay()
{
    if (originalImage.isNull()) return;
    
    // 清除现有场景内容
    scene->clear();
    
    // 添加原始图像
    QPixmap pixmap = QPixmap::fromImage(originalImage);
    imageItem = scene->addPixmap(pixmap);
    
    // 绘制标签
    drawLabelsOnImage();
    
    // 调整视图
    imageView->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
}

void StaticLabelViewDialog::drawLabelsOnImage()
{
    if (!scene || currentLabels.empty()) return;
    
    const auto& classNames = YOLOModel::getInstance()->get_class_names();
    const auto& colors = YOLOModel::getInstance()->get_colors();
    
    for (const auto& label : currentLabels) {
        // 检查是否应该显示这个类别
        if (label.cls >= 0 && label.cls < labelCheckBoxes.size() && 
            !labelCheckBoxes[label.cls]->isChecked()) {
            continue;
        }
        
        // 获取颜色
        cv::Scalar color = (label.cls >= 0 && label.cls < colors.size()) ? 
                          colors[label.cls] : cv::Scalar(255, 255, 255);
        
        QColor qColor(color[2], color[1], color[0]); // BGR to RGB
        
        // 绘制矩形
        QGraphicsRectItem* rectItem = scene->addRect(
            label.x, label.y, label.w, label.h,
            QPen(qColor, 2), QBrush(qColor, Qt::NoBrush)
        );
        
        // 添加标签文本
        QString labelText = QString("%1 (%.2f)")
                           .arg(QString::fromStdString(
                               (label.cls >= 0 && label.cls < classNames.size()) ? 
                               classNames[label.cls] : "Unknown"))
                           .arg(label.confidence);
        
        QGraphicsTextItem* textItem = scene->addText(labelText);
        textItem->setPos(label.x, label.y - 20);
        textItem->setDefaultTextColor(qColor);
    }
}

QImage StaticLabelViewDialog::cvMatToQImage(const cv::Mat& mat)
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
    } else if (mat.type() == CV_8UC1) {
        return QImage(
            mat.data,
            mat.cols,
            mat.rows,
            mat.step,
            QImage::Format_Grayscale8
        ).copy();
    }
    return QImage();
}

cv::Mat StaticLabelViewDialog::qImageToCvMat(const QImage& qimg)
{
    if (qimg.isNull()) return cv::Mat();
    
    QImage swapped = qimg.rgbSwapped();
    return cv::Mat(swapped.height(), swapped.width(), CV_8UC3, 
                   (void*)swapped.constBits(), swapped.bytesPerLine()).clone();
}

void StaticLabelViewDialog::selectAllLabels()
{
    for(auto checkBox : labelCheckBoxes) {
        checkBox->setChecked(true);
    }
    updateImageDisplay();
}

void StaticLabelViewDialog::deselectAllLabels()
{
    for(auto checkBox : labelCheckBoxes) {
        checkBox->setChecked(false);
    }
    updateImageDisplay();
}

std::vector<Label> StaticLabelViewDialog::getLabels() const
{
    return currentLabels;
}

void StaticLabelViewDialog::onAccept()
{
    emit labelsUpdated(currentLabels);
    accept();
}

void StaticLabelViewDialog::onReject()
{
    reject();
}
