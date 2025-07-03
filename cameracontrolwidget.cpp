#include "cameracontrolwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QStandardPaths>
#include <QDateTime>
#include <vector>

CameraControlWidget::CameraControlWidget(QWidget *parent)
    : QWidget(parent)
    , cameraManager_(new CameraManager(this))
    , pcbIdentifier_(new PCBIdentifier(this))
    , statusUpdateTimer_(new QTimer(this))
    , isDetectionRunning_(false)
    , isRealtimeIdentificationRunning_(false)
{
    setupUI();
    connectSignals();
    setupTimer();
}

CameraControlWidget::~CameraControlWidget()
{
    // 断开 cameraManager_ 的所有信号，避免析构过程中收到 queued 信号导致崩溃
    disconnect(cameraManager_, nullptr, this, nullptr);
    // 停止所有摄像头，先停止红外再停止高清
    cameraManager_->stopCamera(CameraType::IR_CAMERA);
    cameraManager_->stopCamera(CameraType::HD_CAMERA);
}

CameraManager* CameraControlWidget::getCameraManager() const
{
    return cameraManager_;
}

void CameraControlWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    
    // 创建主要的标签页控件
    tabWidget_ = new QTabWidget;
    
    // 创建全局控制组
    setupGlobalControls();
    mainLayout->addWidget(globalControlGroup_);
    
    // 设置各个页面
    setupHDCameraPage();
    setupIRCameraPage();
    setupDetectionPage();
    setupStatusPage();
    
    // 添加标签页
    tabWidget_->addTab(hdCameraPage_, "高清摄像头");
    tabWidget_->addTab(irCameraPage_, "红外摄像头");
    tabWidget_->addTab(detectionPage_, "检测结果");
    tabWidget_->addTab(statusPage_, "状态监控");
    
    mainLayout->addWidget(tabWidget_);
    
    setLayout(mainLayout);
    setWindowTitle("相机控制台");
    resize(1200, 800);
}

void CameraControlWidget::setupGlobalControls()
{
    globalControlGroup_ = new QGroupBox("全局控制");
    auto* layout = new QHBoxLayout(globalControlGroup_);
    
    startAllButton_ = new QPushButton("启动所有摄像头");
    stopAllButton_ = new QPushButton("停止所有摄像头");
    
    layout->addWidget(startAllButton_);
    layout->addWidget(stopAllButton_);
    layout->addStretch();
}

void CameraControlWidget::setupHDCameraPage()
{
    hdCameraPage_ = new QWidget;
    auto* mainLayout = new QHBoxLayout(hdCameraPage_);
    
    // 左侧控制面板
    auto* leftPanel = new QWidget;
    leftPanel->setMaximumWidth(300);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    
    // 高清摄像头控制组
    hdControlGroup_ = new QGroupBox("摄像头控制");
    auto* controlLayout = new QVBoxLayout(hdControlGroup_);
      startHDButton_ = new QPushButton("启动高清摄像头");
    stopHDButton_ = new QPushButton("停止高清摄像头");
    saveHDImageButton_ = new QPushButton("保存图像");
    detectComponentsButton_ = new QPushButton("检测PCB元件");
    
    controlLayout->addWidget(startHDButton_);
    controlLayout->addWidget(stopHDButton_);
    controlLayout->addWidget(saveHDImageButton_);
    controlLayout->addWidget(detectComponentsButton_);
      // 参数设置组
    hdParamsGroup_ = new QGroupBox("参数设置");
    auto* paramsLayout = new QGridLayout(hdParamsGroup_);    // 分辨率选择器
    paramsLayout->addWidget(new QLabel("分辨率:"), 0, 0);
    hdResolutionCombo_ = new QComboBox;
    // 8K及高分辨率选项
    hdResolutionCombo_->addItem("8192×4608 (8K Max)", QVariantList({8192, 4608}));
    hdResolutionCombo_->addItem("7680×4320 (8K UHD)", QVariantList({7680, 4320}));
    hdResolutionCombo_->addItem("5120×2880 (5K)", QVariantList({5120, 2880}));
    hdResolutionCombo_->addItem("3840×2160 (4K UHD)", QVariantList({3840, 2160}));
    hdResolutionCombo_->addItem("2560×1440 (QHD)", QVariantList({2560, 1440}));
    hdResolutionCombo_->addItem("1920×1080 (Full HD)", QVariantList({1920, 1080}));
    hdResolutionCombo_->addItem("1600×1200 (UXGA)", QVariantList({1600, 1200}));
    hdResolutionCombo_->addItem("1280×720 (HD)", QVariantList({1280, 720}));
    hdResolutionCombo_->addItem("1024×768 (XGA)", QVariantList({1024, 768}));
    hdResolutionCombo_->addItem("800×600 (SVGA)", QVariantList({800, 600}));
    hdResolutionCombo_->addItem("640×480 (VGA)", QVariantList({640, 480}));
    hdResolutionCombo_->addItem("自定义", QVariantList({-1, -1}));
    hdResolutionCombo_->setCurrentIndex(5); // 默认选择1920x1080 (Full HD)
    paramsLayout->addWidget(hdResolutionCombo_, 0, 1);
      // 自定义分辨率输入（初始隐藏）
    paramsLayout->addWidget(new QLabel("宽度:"), 1, 0);
    hdWidthSpin_ = new QSpinBox;
    hdWidthSpin_->setRange(320, 8192);  // 支持最大8K宽度
    hdWidthSpin_->setValue(1920);
    hdWidthSpin_->setVisible(false);
    paramsLayout->addWidget(hdWidthSpin_, 1, 1);
    
    paramsLayout->addWidget(new QLabel("高度:"), 2, 0);
    hdHeightSpin_ = new QSpinBox;
    hdHeightSpin_->setRange(240, 4608);  // 支持最大8K高度
    hdHeightSpin_->setValue(1080);
    hdHeightSpin_->setVisible(false);
    paramsLayout->addWidget(hdHeightSpin_, 2, 1);
    
    paramsLayout->addWidget(new QLabel("帧率:"), 3, 0);
    hdFpsSpin_ = new QSpinBox;
    hdFpsSpin_->setRange(1, 60);
    hdFpsSpin_->setValue(30);
    paramsLayout->addWidget(hdFpsSpin_, 3, 1);
    
    applyHDParamsButton_ = new QPushButton("应用参数");
    paramsLayout->addWidget(applyHDParamsButton_, 4, 0, 1, 2);
      leftLayout->addWidget(hdControlGroup_);
    leftLayout->addWidget(hdParamsGroup_);
    
    // 实时PCB识别控制组
    realtimePCBGroup_ = new QGroupBox("实时PCB识别");
    auto* realtimeLayout = new QGridLayout(realtimePCBGroup_);
    
    startRealtimePCBButton_ = new QPushButton("开始实时识别");
    stopRealtimePCBButton_ = new QPushButton("停止实时识别");
    stopRealtimePCBButton_->setEnabled(false);
    
    startRealtimePCBButton_->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    stopRealtimePCBButton_->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    
    realtimeLayout->addWidget(startRealtimePCBButton_, 0, 0);
    realtimeLayout->addWidget(stopRealtimePCBButton_, 0, 1);
    
    realtimeLayout->addWidget(new QLabel("识别间隔:"), 1, 0);
    realtimeIntervalSpin_ = new QSpinBox;
    realtimeIntervalSpin_->setRange(100, 5000);
    realtimeIntervalSpin_->setValue(500);
    realtimeIntervalSpin_->setSuffix(" ms");
    realtimeLayout->addWidget(realtimeIntervalSpin_, 1, 1);
    
    realtimeLayout->addWidget(new QLabel("匹配阈值:"), 2, 0);
    realtimeThresholdSpin_ = new QDoubleSpinBox;
    realtimeThresholdSpin_->setRange(0.1, 1.0);
    realtimeThresholdSpin_->setValue(0.3);
    realtimeThresholdSpin_->setDecimals(2);
    realtimeThresholdSpin_->setSingleStep(0.05);
    realtimeLayout->addWidget(realtimeThresholdSpin_, 2, 1);
    
    realtimeResultLabel_ = new QLabel("识别结果: 等待中...");
    realtimeResultLabel_->setStyleSheet("QLabel { font-weight: bold; }");
    realtimeLayout->addWidget(realtimeResultLabel_, 3, 0, 1, 2);
    
    realtimeConfidenceLabel_ = new QLabel("置信度: 0%");
    realtimeLayout->addWidget(realtimeConfidenceLabel_, 4, 0, 1, 2);
    
    leftLayout->addWidget(realtimePCBGroup_);
    leftLayout->addStretch();// 右侧图像显示
    hdImageView_ = new QGraphicsView;
    hdImageScene_ = new QGraphicsScene;
    hdImageItem_ = new QGraphicsPixmapItem;
    hdImageScene_->addItem(hdImageItem_);
    hdImageView_->setScene(hdImageScene_);
    hdImageView_->setMinimumSize(640, 480);
    
    // 设置视图属性，支持自动调整大小
    hdImageView_->setRenderHint(QPainter::Antialiasing);
    hdImageView_->setDragMode(QGraphicsView::ScrollHandDrag);
    hdImageView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    hdImageView_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 设置调整大小模式，使图像能够自动适应窗口大小
    hdImageView_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    hdImageView_->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    
    // 设置背景色，帮助调试显示问题
    hdImageView_->setBackgroundBrush(QBrush(QColor(50, 50, 50))); // 深灰色背景
    hdImageScene_->setBackgroundBrush(QBrush(QColor(30, 30, 30))); // 更深的场景背景
    
    qDebug() << "HD ImageView initialized - Scene items count:" << hdImageScene_->items().count();
    
    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(hdImageView_, 1);
}

void CameraControlWidget::setupIRCameraPage()
{
    irCameraPage_ = new QWidget;
    auto* mainLayout = new QHBoxLayout(irCameraPage_);
    
    // 左侧控制面板
    auto* leftPanel = new QWidget;
    leftPanel->setMaximumWidth(300);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    
    // 红外摄像头控制组
    irControlGroup_ = new QGroupBox("摄像头控制");
    auto* controlLayout = new QVBoxLayout(irControlGroup_);
      startIRButton_ = new QPushButton("启动红外摄像头");
    stopIRButton_ = new QPushButton("停止红外摄像头");
    saveThermalButton_ = new QPushButton("保存热图");
    measureTempButton_ = new QPushButton("测量温度");
    markMaxTempCheckBox_ = new QCheckBox("标记最高温度点");
    markMaxTempCheckBox_->setChecked(false);  // 默认不标记
    
    controlLayout->addWidget(startIRButton_);
    controlLayout->addWidget(stopIRButton_);
    controlLayout->addWidget(saveThermalButton_);
    controlLayout->addWidget(measureTempButton_);
    controlLayout->addWidget(markMaxTempCheckBox_);
    
    // 参数设置组
    irParamsGroup_ = new QGroupBox("参数设置");
    auto* paramsLayout = new QGridLayout(irParamsGroup_);
    
    paramsLayout->addWidget(new QLabel("IP地址:"), 0, 0);
    irIpEdit_ = new QLineEdit("192.168.0.200");
    paramsLayout->addWidget(irIpEdit_, 0, 1);
    
    applyIRParamsButton_ = new QPushButton("应用参数");
    paramsLayout->addWidget(applyIRParamsButton_, 1, 0, 1, 2);
    
    // 温度显示组
    temperatureGroup_ = new QGroupBox("温度监控");
    auto* tempLayout = new QGridLayout(temperatureGroup_);
    
    tempLayout->addWidget(new QLabel("当前温度:"), 0, 0);
    currentTempLabel_ = new QLabel("--°C");
    tempLayout->addWidget(currentTempLabel_, 0, 1);
    
    tempLayout->addWidget(new QLabel("最高温度:"), 1, 0);
    maxTempLabel_ = new QLabel("--°C");
    tempLayout->addWidget(maxTempLabel_, 1, 1);
    
    tempLayout->addWidget(new QLabel("最低温度:"), 2, 0);
    minTempLabel_ = new QLabel("--°C");
    tempLayout->addWidget(minTempLabel_, 2, 1);
    
    tempLayout->addWidget(new QLabel("平均温度:"), 3, 0);
    avgTempLabel_ = new QLabel("--°C");
    tempLayout->addWidget(avgTempLabel_, 3, 1);
    
    tempLayout->addWidget(new QLabel("温度阈值:"), 4, 0);
    thresholdSpin_ = new QDoubleSpinBox;
    thresholdSpin_->setRange(-50.0, 200.0);
    thresholdSpin_->setValue(60.0);
    thresholdSpin_->setSuffix("°C");
    tempLayout->addWidget(thresholdSpin_, 4, 1);
    
    setThresholdButton_ = new QPushButton("设置阈值");
    tempLayout->addWidget(setThresholdButton_, 5, 0, 1, 2);
      leftLayout->addWidget(irControlGroup_);
    leftLayout->addWidget(irParamsGroup_);
    leftLayout->addWidget(temperatureGroup_);
    leftLayout->addStretch();
    
    // 右侧图像显示 - 使用专门的红外图像显示控件
    irImageDisplay_ = new IRImageDisplay(this);
    irImageDisplay_->setMinimumSize(640, 480);
    
    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(irImageDisplay_, 1);
}

void CameraControlWidget::setupDetectionPage()
{
    detectionPage_ = new QWidget;
    auto* mainLayout = new QHBoxLayout(detectionPage_);
    
    // 左侧检测结果表格
    auto* leftPanel = new QWidget;
    leftPanel->setMaximumWidth(400);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    
    auto* tableLabel = new QLabel("检测结果:");
    leftLayout->addWidget(tableLabel);
    
    detectionTable_ = new QTableWidget;
    detectionTable_->setColumnCount(4);
    QStringList headers;
    headers << "组件类型" << "位置(x,y)" << "大小" << "置信度";
    detectionTable_->setHorizontalHeaderLabels(headers);
    detectionTable_->horizontalHeader()->setStretchLastSection(true);
    leftLayout->addWidget(detectionTable_);
    
    auto* detailsLabel = new QLabel("详细信息:");
    leftLayout->addWidget(detailsLabel);
    
    detectionDetails_ = new QTextEdit;
    detectionDetails_->setMaximumHeight(150);
    leftLayout->addWidget(detectionDetails_);
    
    saveDetectionButton_ = new QPushButton("保存检测结果");
    leftLayout->addWidget(saveDetectionButton_);
    
    // 右侧检测结果图像
    detectionImageView_ = new QGraphicsView;
    detectionImageScene_ = new QGraphicsScene;
    detectionImageItem_ = new QGraphicsPixmapItem;
    detectionImageScene_->addItem(detectionImageItem_);
    detectionImageView_->setScene(detectionImageScene_);
    
    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(detectionImageView_, 1);
}

void CameraControlWidget::setupStatusPage()
{
    statusPage_ = new QWidget;
    auto* mainLayout = new QVBoxLayout(statusPage_);
    
    // 状态信息组
    auto* statusGroup = new QGroupBox("摄像头状态");
    auto* statusLayout = new QGridLayout(statusGroup);
    
    statusLayout->addWidget(new QLabel("高清摄像头:"), 0, 0);
    hdStatusLabel_ = new QLabel("未连接");
    statusLayout->addWidget(hdStatusLabel_, 0, 1);
    
    statusLayout->addWidget(new QLabel("红外摄像头:"), 1, 0);
    irStatusLabel_ = new QLabel("未连接");
    statusLayout->addWidget(irStatusLabel_, 1, 1);
    
    statusLayout->addWidget(new QLabel("处理进度:"), 2, 0);
    processingProgress_ = new QProgressBar;
    statusLayout->addWidget(processingProgress_, 2, 1);
    
    mainLayout->addWidget(statusGroup);
    
    // 日志信息
    auto* logLabel = new QLabel("系统日志:");
    mainLayout->addWidget(logLabel);
    
    logTextEdit_ = new QTextEdit;
    logTextEdit_->setReadOnly(true);
    mainLayout->addWidget(logTextEdit_);
}

void CameraControlWidget::connectSignals()
{
    // 全局控制信号
    connect(startAllButton_, &QPushButton::clicked, this, &CameraControlWidget::onStartAllCameras);
    connect(stopAllButton_, &QPushButton::clicked, this, &CameraControlWidget::onStopAllCameras);    // 高清摄像头信号
    connect(startHDButton_, &QPushButton::clicked, this, &CameraControlWidget::onStartHDCamera);
    connect(stopHDButton_, &QPushButton::clicked, this, &CameraControlWidget::onStopHDCamera);
    connect(saveHDImageButton_, &QPushButton::clicked, this, &CameraControlWidget::onSaveHDImage);
    connect(detectComponentsButton_, &QPushButton::clicked, this, &CameraControlWidget::onDetectPCBComponents);
    connect(hdResolutionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CameraControlWidget::onHDResolutionChanged);
    connect(applyHDParamsButton_, &QPushButton::clicked, this, &CameraControlWidget::onApplyHDParams);
    
    // 实时PCB识别信号
    connect(startRealtimePCBButton_, &QPushButton::clicked, this, &CameraControlWidget::onStartRealtimePCBIdentification);
    connect(stopRealtimePCBButton_, &QPushButton::clicked, this, &CameraControlWidget::onStopRealtimePCBIdentification);
    connect(realtimeIntervalSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &CameraControlWidget::onRealtimeIntervalChanged);
    connect(realtimeThresholdSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CameraControlWidget::onRealtimeThresholdChanged);
    
    // 红外摄像头信号
    connect(startIRButton_, &QPushButton::clicked, this, &CameraControlWidget::onStartIRCamera);
    connect(stopIRButton_, &QPushButton::clicked, this, &CameraControlWidget::onStopIRCamera);
    connect(saveThermalButton_, &QPushButton::clicked, this, &CameraControlWidget::onSaveThermalImage);
    connect(measureTempButton_, &QPushButton::clicked, this, &CameraControlWidget::onMeasureTemperature);
    connect(applyIRParamsButton_, &QPushButton::clicked, this, &CameraControlWidget::onApplyIRParams);
    connect(setThresholdButton_, &QPushButton::clicked, this, &CameraControlWidget::onSetTemperatureThreshold);
    connect(markMaxTempCheckBox_, &QCheckBox::toggled, this, &CameraControlWidget::onMarkMaxTempToggled);
    
    // 检测结果信号
    connect(saveDetectionButton_, &QPushButton::clicked, this, &CameraControlWidget::onSaveDetectionResults);
      // 摄像头管理器信号
    connect(cameraManager_, &CameraManager::cameraStatusChanged,
            this, &CameraControlWidget::onCameraStatusChanged);
    
    bool imageSignalConnected = connect(cameraManager_, &CameraManager::newImageAvailable,
            this, &CameraControlWidget::onHDImageReceived);
    qDebug() << "newImageAvailable signal connected:" << imageSignalConnected;
    
    connect(cameraManager_, &CameraManager::newThermalDataAvailable,
            this, &CameraControlWidget::onThermalDataReceived);
    connect(cameraManager_, &CameraManager::pcbDetectionCompleted,
            this, &CameraControlWidget::onPCBDetectionCompleted);
    connect(cameraManager_, &CameraManager::pcbModelIdentified,
            this, &CameraControlWidget::onPCBModelIdentified);    
            connect(cameraManager_, &CameraManager::temperatureAlert,
            this, &CameraControlWidget::onTemperatureAlert);
    connect(cameraManager_, &CameraManager::errorOccurred,
            this, &CameraControlWidget::onCameraError);
    
    // PCB识别器信号
    connect(pcbIdentifier_, &PCBIdentifier::realtimeIdentificationCompleted,
            this, &CameraControlWidget::onRealtimeIdentificationCompleted);
    connect(pcbIdentifier_, &PCBIdentifier::realtimeIdentificationStarted,
            this, &CameraControlWidget::onRealtimeIdentificationStarted);
    connect(pcbIdentifier_, &PCBIdentifier::realtimeIdentificationStopped,
            this, &CameraControlWidget::onRealtimeIdentificationStopped);
    connect(pcbIdentifier_, &PCBIdentifier::errorOccurred,
            this, &CameraControlWidget::onRealtimeIdentificationError);
}

void CameraControlWidget::setupTimer()
{
    // 设置状态更新定时器
    connect(statusUpdateTimer_, &QTimer::timeout,
            this, &CameraControlWidget::updateStatus);
    statusUpdateTimer_->start(STATUS_UPDATE_INTERVAL);
}

// 摄像头控制槽函数
void CameraControlWidget::onStartHDCamera()
{
    addLogMessage("正在启动高清摄像头...");
    
    bool success = cameraManager_->startCamera(CameraType::HD_CAMERA);
    if (success) {
        addLogMessage("高清摄像头启动成功");
        startHDButton_->setEnabled(false);
        stopHDButton_->setEnabled(true);
        
        // 检查相机状态
        CameraStatus status = cameraManager_->getCameraStatus(CameraType::HD_CAMERA);
        addLogMessage(QString("HD相机状态: %1").arg(static_cast<int>(status)));
        
    } else {
        QString errorMsg = "高清摄像头启动失败，请检查：\n1. 相机是否正确连接\n2. 相机驱动是否安装\n3. 是否被其他程序占用";
        QMessageBox::warning(this, "错误", errorMsg);
        addLogMessage("高清摄像头启动失败");
    }
}

void CameraControlWidget::onStopHDCamera()
{
    cameraManager_->stopCamera(CameraType::HD_CAMERA);
    addLogMessage("高清摄像头已停止");
    startHDButton_->setEnabled(true);
    stopHDButton_->setEnabled(false);
}

void CameraControlWidget::onStartIRCamera()
{
    addLogMessage("正在启动红外摄像头...");
    
    bool success = cameraManager_->startCamera(CameraType::IR_CAMERA);
    if (success) {
        addLogMessage("红外摄像头启动成功");
        startIRButton_->setEnabled(false);
        stopIRButton_->setEnabled(true);
        
        // 检查相机状态
        CameraStatus status = cameraManager_->getCameraStatus(CameraType::IR_CAMERA);
        addLogMessage(QString("红外相机状态: %1").arg(static_cast<int>(status)));
        
    } else {
        QString errorMsg = "红外摄像头启动失败，请检查：\n1. 网络连接是否正常\n2. IP地址是否正确\n3. 红外相机是否在线";
        QMessageBox::warning(this, "错误", errorMsg);
        addLogMessage("红外摄像头启动失败");
    }
}

void CameraControlWidget::onStopIRCamera()
{
    cameraManager_->stopCamera(CameraType::IR_CAMERA);
    addLogMessage("红外摄像头已停止");
    startIRButton_->setEnabled(true);
    stopIRButton_->setEnabled(false);
}

void CameraControlWidget::onStartAllCameras()
{
    onStartHDCamera();
    onStartIRCamera();
}

void CameraControlWidget::onStopAllCameras()
{
    onStopHDCamera();
    onStopIRCamera();
}

// 诊断功能
void CameraControlWidget::onDiagnoseHDCamera()
{
    QString diagnosticInfo = cameraManager_->getCameraDiagnosticInfo(CameraType::HD_CAMERA);
    
    QDialog dialog(this);
    dialog.setWindowTitle("HD相机诊断信息");
    dialog.resize(500, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QTextEdit* textEdit = new QTextEdit(&dialog);
    textEdit->setFont(QFont("Consolas", 9));
    textEdit->setPlainText(diagnosticInfo);
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);
    
    QPushButton* closeButton = new QPushButton("关闭", &dialog);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton);
    
    dialog.exec();
    
    addLogMessage("HD相机诊断信息已显示");
}

void CameraControlWidget::onDiagnoseIRCamera()
{
    QString diagnosticInfo = cameraManager_->getCameraDiagnosticInfo(CameraType::IR_CAMERA);
    
    QDialog dialog(this);
    dialog.setWindowTitle("红外相机诊断信息");
    dialog.resize(500, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QTextEdit* textEdit = new QTextEdit(&dialog);
    textEdit->setFont(QFont("Consolas", 9));
    textEdit->setPlainText(diagnosticInfo);
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);
    
    QPushButton* closeButton = new QPushButton("关闭", &dialog);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton);
    
    dialog.exec();
    
    addLogMessage("红外相机诊断信息已显示");
}

// 图像处理槽函数
void CameraControlWidget::onHDImageReceived(CameraType type, const ImageData& data)
{
    qDebug() << "onHDImageReceived called - Type:" << static_cast<int>(type) 
             << "Valid:" << data.isValid 
             << "Image empty:" << data.image.empty();
    
    if (type == CameraType::HD_CAMERA && data.isValid) {
        try {
            if (data.image.empty()) {
                addLogMessage("警告: 收到空的HD图像数据");
                return;
            }
            
            qDebug() << "Processing HD image - Size:" << data.image.cols << "x" << data.image.rows
                     << "Channels:" << data.image.channels();            // 转换图像格式（来自CameraManager的图像已经是RGB格式，不需要BGR转换）
            QImage qImage = matToQImage(data.image, false);
            if (qImage.isNull()) {
                addLogMessage("错误: 图像格式转换失败");
                return;
            }
              // 创建像素图并缩放
            QPixmap pixmap = QPixmap::fromImage(qImage);
            if (pixmap.isNull()) {
                addLogMessage("错误: 创建像素图失败");
                return;
            }
              qDebug() << "QImage created successfully - Size:" << qImage.width() << "x" << qImage.height()
                     << "Format:" << qImage.format();
            
              // 显示图像 - 直接设置像素图，不进行预缩放
            hdImageItem_->setPixmap(pixmap);
            
            // 确保QGraphicsPixmapItem可见并正确配置
            hdImageItem_->setVisible(true);
            hdImageItem_->setOpacity(1.0);
            hdImageItem_->setPos(0, 0);  // 确保位置为原点
            
            qDebug() << "Pixmap set to graphics item";
            qDebug() << "Graphics item visible:" << hdImageItem_->isVisible()
                     << "Opacity:" << hdImageItem_->opacity()
                     << "Pos:" << hdImageItem_->pos();
            
            // 确保场景大小适合图像
            hdImageScene_->setSceneRect(hdImageItem_->boundingRect());
            qDebug() << "Scene rect set to:" << hdImageScene_->sceneRect();
            
            // 自动适应图像大小到视图
            hdImageView_->fitInView(hdImageItem_, Qt::KeepAspectRatio);
            qDebug() << "fitInView called";
            
            // 强制更新视图
            hdImageView_->update();
            hdImageView_->viewport()->update();
            hdImageView_->repaint();  // 强制重绘
            
            qDebug() << "HD image displayed - Original size:" << pixmap.width() << "x" << pixmap.height();
            qDebug() << "QGraphicsView size:" << hdImageView_->size() 
                     << "Scene rect:" << hdImageView_->sceneRect()
                     << "Item rect:" << hdImageItem_->boundingRect();
            
            // 更新状态
            static int frameCount = 0;
            frameCount++;
            if (frameCount % 30 == 0) { // 每30帧输出一次状态
                addLogMessage(QString("HD图像更新 - 帧数: %1, 尺寸: %2x%3")
                              .arg(frameCount).arg(data.image.cols).arg(data.image.rows));
            }
            
        } catch (const std::exception& e) {
            addLogMessage(QString("HD图像处理错误: %1").arg(e.what()));
        }
    }
}

void CameraControlWidget::onThermalDataReceived(const ThermalData& data)
{
    if (data.isValid) {
        try {
            if (data.thermalImage.empty()) {
                addLogMessage("警告: 收到空的红外图像数据");
                return;
            }            // 转换OpenCV Mat到QImage（红外图像保持原有转换逻辑）
            QImage qImage = matToQImage(data.thermalImage, true);
            if (qImage.isNull()) {
                addLogMessage("错误: 红外图像格式转换失败");
                return;
            }
            
            // 准备温度数据：优先使用 temperatureMap 拷贝，避免野指针
            std::vector<uint16_t> tempData;
            int w = 0, h = 0;
            if (!data.temperatureMap.empty()) {
                h = data.temperatureMap.rows;
                w = data.temperatureMap.cols;
                const uint16_t* p = data.temperatureMap.ptr<uint16_t>();
                tempData.assign(p, p + static_cast<size_t>(w) * h);
            } else if (data.rawTempData && data.width > 0 && data.height > 0) {
                // 兼容旧接口
                w = data.width;
                h = data.height;
                size_t dataSize = static_cast<size_t>(w) * h;
                tempData.assign(data.rawTempData, data.rawTempData + dataSize);
            }
            // 使用IRImageDisplay显示图像和温度数据
            irImageDisplay_->setImage(qImage, tempData, w, h);
            
            // 更新温度显示标签
            currentTempLabel_->setText(QString::number(data.avgTemp, 'f', 1) + "°C");
            maxTempLabel_->setText(QString::number(data.maxTemp, 'f', 1) + "°C");
            minTempLabel_->setText(QString::number(data.minTemp, 'f', 1) + "°C");
            avgTempLabel_->setText(QString::number(data.avgTemp, 'f', 1) + "°C");
            
            // 更新状态
            static int thermalFrameCount = 0;
            thermalFrameCount++;
            if (thermalFrameCount % 30 == 0) { // 每30帧输出一次状态
                addLogMessage(QString("红外图像更新 - 帧数: %1, 平均温度: %2°C")
                              .arg(thermalFrameCount).arg(data.avgTemp, 0, 'f', 1));
            }
            
        } catch (const std::exception& e) {
            addLogMessage(QString("红外图像处理错误: %1").arg(e.what()));
        }
    }
}

void CameraControlWidget::onDetectPCBComponents()
{
    if (!isDetectionRunning_) {
        isDetectionRunning_ = true;
        detectComponentsButton_->setEnabled(false);
        processingProgress_->setValue(0);        addLogMessage("开始PCB元件检测...");
        
        // 获取最新的HD图像
        ImageData imageData = cameraManager_->getLatestImage(CameraType::HD_CAMERA);
        if (imageData.isValid && !imageData.image.empty()) {
            // 启动检测
            PCBDetectionResult result = cameraManager_->detectPCBComponents(imageData.image);
            onPCBDetectionCompleted(result);
        } else {
            addLogMessage("错误: 没有可用的图像进行检测");
            isDetectionRunning_ = false;
        }
    }
}

void CameraControlWidget::onPCBDetectionCompleted(const PCBDetectionResult& result)
{
    isDetectionRunning_ = false;
    detectComponentsButton_->setEnabled(true);
    processingProgress_->setValue(100);
    lastDetectionResult_ = result;
    
    // 更新检测结果表格
    updateDetectionTable(result);    // 显示检测结果图像（检测结果图像也已经是RGB格式）
    if (!result.detectedImage.empty()) {
        QImage qImage = matToQImage(result.detectedImage, false);
        QPixmap pixmap = QPixmap::fromImage(qImage);
        detectionImageItem_->setPixmap(pixmap);
        detectionImageScene_->setSceneRect(detectionImageItem_->boundingRect());
        detectionImageView_->fitInView(detectionImageItem_, Qt::KeepAspectRatio);
    }
    
    addLogMessage(QString("检测完成，发现 %1 个元件").arg(result.detectedComponents.size()));
}

// 参数设置槽函数
void CameraControlWidget::onHDResolutionChanged()
{
    QVariantList resolution = hdResolutionCombo_->currentData().toList();
    
    if (resolution.size() >= 2) {
        int width = resolution[0].toInt();
        int height = resolution[1].toInt();
        
        // 如果选择了自定义分辨率（-1, -1），显示自定义输入框
        bool isCustom = (width == -1 && height == -1);
        
        // 更新自定义输入框的可见性
        hdWidthSpin_->setVisible(isCustom);
        hdHeightSpin_->setVisible(isCustom);
        
        // 查找包含宽度和高度标签的父布局
        QGridLayout* layout = qobject_cast<QGridLayout*>(hdParamsGroup_->layout());
        if (layout) {
            // 更新宽度和高度标签的可见性
            for (int i = 0; i < layout->count(); ++i) {
                QLayoutItem* item = layout->itemAt(i);
                if (item && item->widget()) {
                    QLabel* label = qobject_cast<QLabel*>(item->widget());
                    if (label && (label->text() == "宽度:" || label->text() == "高度:")) {
                        label->setVisible(isCustom);
                    }
                }
            }
        }
        
        if (!isCustom) {
            // 如果不是自定义，直接设置分辨率值并应用
            hdWidthSpin_->setValue(width);
            hdHeightSpin_->setValue(height);
              // 对于高分辨率提供性能警告
            if (width >= 3840 || height >= 2160) {
                QString warningMsg = QString("警告：选择的分辨率 %1x%2 较高，可能会影响系统性能。\n"
                                           "建议在性能较低的设备上使用较低分辨率，或确保系统具有足够的处理能力。")
                                           .arg(width).arg(height);
                addLogMessage(warningMsg);
                
                // 8K分辨率的特殊警告
                if (width >= 7680 || height >= 4320) {
                    addLogMessage("注意：8K分辨率需要高性能硬件支持（建议RTX 3060或更高级显卡），建议先测试系统兼容性。");
                    addLogMessage("提示：如遇到性能问题，建议降低帧率或使用4K UHD分辨率。");
                }
                
                // 8K Max分辨率的额外警告
                if (width >= 8192 || height >= 4608) {
                    addLogMessage("警告：8K Max分辨率(8192×4608)是最高支持分辨率，需要顶级硬件配置。");
                    addLogMessage("建议仅在有高性能工作站的情况下使用，否则可能导致系统卡顿或无响应。");
                }
            }
            
            // 自动应用预设分辨率
            int fps = hdFpsSpin_->value();
            cameraManager_->setHDCameraParams(width, height, fps);
            addLogMessage(QString("HD摄像头分辨率已更新: %1x%2@%3fps")
                        .arg(width).arg(height).arg(fps));
        } else {
            addLogMessage("已切换到自定义分辨率模式，请手动设置宽度和高度");
        }
    }
}

void CameraControlWidget::onApplyHDParams()
{
    int width = hdWidthSpin_->value();
    int height = hdHeightSpin_->value();
    int fps = hdFpsSpin_->value();
      // 对于高分辨率提供性能警告
    if (width >= 3840 || height >= 2160) {
        QString warningMsg = QString("警告：自定义分辨率 %1x%2 较高，可能会影响系统性能。")
                                   .arg(width).arg(height);
        addLogMessage(warningMsg);
        
        // 8K分辨率的警告
        if (width >= 7680 || height >= 4320) {
            addLogMessage("注意：8K级别分辨率需要高性能硬件支持（建议RTX 3060或更高级显卡）。");
        }
        
        // 8K Max分辨率的特殊警告
        if (width >= 8192 || height >= 4608) {
            addLogMessage("警告：8K Max级别分辨率需要顶级硬件配置，可能导致性能问题。");
        }
    }
    
    cameraManager_->setHDCameraParams(width, height, fps);
    logTextEdit_->append(QString("HD摄像头参数已更新: %1x%2@%3fps")
                        .arg(width).arg(height).arg(fps));
}

void CameraControlWidget::onApplyIRParams()
{
    QString ipAddress = irIpEdit_->text();
    cameraManager_->setIRCameraParams(ipAddress);
    logTextEdit_->append("红外摄像头IP地址已更新: " + ipAddress);
}

// 保存功能槽函数
void CameraControlWidget::onSaveHDImage()
{
    QString fileName = QFileDialog::getSaveFileName(this, "保存高清图像", 
        QString("HD_Image_%1.jpg").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss")),
        "Images (*.png *.jpg *.bmp)");
    
    if (!fileName.isEmpty()) {
        bool success = cameraManager_->saveImage(CameraType::HD_CAMERA, fileName);
        if (success) {
            addLogMessage("高清图像已保存: " + fileName);
        } else {
            QMessageBox::warning(this, "错误", "图像保存失败");
        }
    }
}

void CameraControlWidget::onSaveThermalImage()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "保存红外图像", 
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "图像文件 (*.png *.jpg *.bmp)");
        
    if (!fileName.isEmpty()) {
        if (cameraManager_->saveImage(CameraType::IR_CAMERA, fileName)) {
            logTextEdit_->append("红外图像已保存: " + fileName);
        } else {
            QMessageBox::warning(this, "错误", "保存红外图像失败");
        }
    }
}

void CameraControlWidget::onSaveDetectionResults()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "保存检测结果", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "文本文件 (*.txt)");
        
    if (!fileName.isEmpty()) {
        // 这里应该保存检测结果到文件
        logTextEdit_->append("检测结果已保存: " + fileName);
    }
}

// 温度测量槽函数
void CameraControlWidget::onMeasureTemperature()
{
    if (lastClickPosition_.isNull()) {
        QMessageBox::information(this, "提示", "请先在红外图像上点击要测量的位置");
        return;
    }        double temperature = cameraManager_->getTemperatureAt(lastClickPosition_.x(), lastClickPosition_.y());
    if (temperature != -273.15) { // 有效温度值
        currentTempLabel_->setText(QString::number(temperature, 'f', 1) + "°C");
        addLogMessage(QString("点 (%1, %2) 的温度: %3°C").arg(lastClickPosition_.x()).arg(lastClickPosition_.y()).arg(temperature, 0, 'f', 1));
    } else {
        QMessageBox::warning(this, "错误", "无法获取该位置的温度");
    }
}

// 状态更新和事件处理
void CameraControlWidget::onTemperatureAlert(double temperature, const cv::Point& location)
{
    QString message = QString("温度警报！位置(%1,%2)的温度 %3°C 超过阈值")
                        .arg(location.x).arg(location.y).arg(temperature, 0, 'f', 1);
    addLogMessage(message);
}

void CameraControlWidget::onCameraStatusChanged(CameraType type, CameraStatus status)
{
    QString statusText;
    switch (status) {
        case CameraStatus::DISCONNECTED: statusText = "未连接"; break;
        case CameraStatus::CONNECTED: statusText = "已连接"; break;
        case CameraStatus::STREAMING: statusText = "正在流传输"; break;
        case CameraStatus::ERROR: statusText = "错误"; break;
    }
    
    if (type == CameraType::HD_CAMERA) {
        hdStatusLabel_->setText(statusText);
    } else if (type == CameraType::IR_CAMERA) {
        irStatusLabel_->setText(statusText);
    }
}

void CameraControlWidget::onCameraError(CameraType type, const QString& error)
{
    QString cameraName = (type == CameraType::HD_CAMERA) ? "高清摄像头" : "红外摄像头";
    QString message = QString("%1 错误: %2").arg(cameraName).arg(error);
    QMessageBox::critical(this, "摄像头错误", message);
    addLogMessage(message);
}

void CameraControlWidget::onHDImageClicked(QPoint position)
{
    addLogMessage(QString("HD图像点击位置: (%1, %2)").arg(position.x()).arg(position.y()));
    
    // 可以在此添加点击处理逻辑
    // 比如在该位置进行特定分析或标记
    // 或者获取该点的图像信息
    
    // 保存点击位置以供其他操作使用
    lastClickPosition_ = position;
}

void CameraControlWidget::onThermalImageClicked(QPoint position)
{
    lastClickPosition_ = position;
    addLogMessage(QString("红外图像点击位置: (%1, %2)").arg(position.x()).arg(position.y()));
    
    // 自动获取该位置的温度
    double temperature = cameraManager_->getTemperatureAt(position.x(), position.y());
    if (temperature != -273.15) { // 有效温度值
        currentTempLabel_->setText(QString::number(temperature, 'f', 1) + "°C");
        addLogMessage(QString("点击位置温度: %1°C").arg(temperature, 0, 'f', 1));
        
        // 检查是否超过阈值
        if (temperature > thresholdSpin_->value()) {
            QString alertMsg = QString("警告：点击位置温度 %1°C 超过阈值 %2°C")
                                .arg(temperature, 0, 'f', 1)
                                .arg(thresholdSpin_->value(), 0, 'f', 1);
            addLogMessage(alertMsg);
        }
    } else {
        addLogMessage("无法获取该位置的温度数据");
    }
}

void CameraControlWidget::updateStatus()
{
    // 更新处理进度
    if (isDetectionRunning_) {
        int currentValue = processingProgress_->value();
        if (currentValue < 90) {
            processingProgress_->setValue(currentValue + 10);
        }
    }
}

// 辅助函数
void CameraControlWidget::addLogMessage(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp).arg(message);
    logTextEdit_->append(logEntry);
    
    // 保持日志在合理长度
    if (logTextEdit_->document()->blockCount() > 1000) {
        QTextCursor cursor = logTextEdit_->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 100);
        cursor.removeSelectedText();
    }
}

QImage CameraControlWidget::matToQImage(const cv::Mat& mat, bool convertBGRtoRGB)
{
    if (mat.empty()) {
        qWarning() << "matToQImage: Input Mat is empty";
        return QImage();
    }
    
    qDebug() << "matToQImage called - Mat size:" << mat.cols << "x" << mat.rows 
             << "Type:" << mat.type() << "Channels:" << mat.channels()
             << "Step:" << mat.step << "Continuous:" << mat.isContinuous()
             << "Convert BGR:" << convertBGRtoRGB;
    
    try {
        switch (mat.type()) {
            case CV_8UC1: {
                // 灰度图像
                QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
                qDebug() << "Created grayscale QImage - Size:" << image.width() << "x" << image.height();
                return image.copy();
            }            case CV_8UC3: {
                // 3通道图像处理
                if (convertBGRtoRGB) {
                    // BGR图像 - 转换为RGB
                    cv::Mat rgbMat;
                    cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
                    QImage image(rgbMat.data, rgbMat.cols, rgbMat.rows, rgbMat.step, QImage::Format_RGB888);
                    qDebug() << "Created RGB QImage (with BGR->RGB conversion) - Size:" << image.width() << "x" << image.height();
                    return image.copy();
                } else {
                    // 已经是RGB格式或不需要转换
                    QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
                    qDebug() << "Created RGB QImage (no conversion) - Size:" << image.width() << "x" << image.height();
                    return image.copy();
                }
            }
            case CV_8UC4: {
                // BGRA图像
                QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
                qDebug() << "Created ARGB QImage - Size:" << image.width() << "x" << image.height();
                return image.copy();
            }
            case CV_16UC1: {
                // 16位灰度图像，转换为8位
                cv::Mat mat8;
                mat.convertTo(mat8, CV_8UC1, 1.0/256.0);
                QImage image(mat8.data, mat8.cols, mat8.rows, mat8.step, QImage::Format_Grayscale8);
                return image.copy();
            }
            case CV_32FC1: {
                // 32位浮点灰度图像，转换为8位
                cv::Mat mat8;
                mat.convertTo(mat8, CV_8UC1, 255.0);
                QImage image(mat8.data, mat8.cols, mat8.rows, mat8.step, QImage::Format_Grayscale8);
                return image.copy();
            }
            default: {
                qWarning() << "matToQImage: Unsupported Mat type:" << mat.type();
                
                // 尝试转换为RGB格式
                cv::Mat rgbMat;
                if (mat.channels() == 1) {
                    cv::cvtColor(mat, rgbMat, cv::COLOR_GRAY2RGB);
                } else if (mat.channels() == 3) {
                    cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
                } else {
                    // 不支持的格式，创建一个错误图像
                    rgbMat = cv::Mat::zeros(100, 200, CV_8UC3);
                    cv::putText(rgbMat, "Unsupported Format", cv::Point(10, 50), 
                               cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
                }
                
                if (rgbMat.type() == CV_8UC3) {
                    QImage image(rgbMat.data, rgbMat.cols, rgbMat.rows, rgbMat.step, QImage::Format_RGB888);
                    return image.copy();
                }
                
                return QImage();
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "matToQImage error:" << e.what();
        return QImage();
    }
}

void CameraControlWidget::updateDetectionTable(const PCBDetectionResult& result)
{
    detectionTable_->setRowCount(result.detectedComponents.size());
    
    for (int i = 0; i < result.detectedComponents.size(); ++i) {
        const QString& componentType = result.detectedComponents[i];
        
        // Get component bounds and confidence if available
        cv::Rect componentBounds;
        double confidence = 0.0;
        if (i < result.componentBounds.size()) {
            componentBounds = result.componentBounds[i];
        }
        if (i < result.confidences.size()) {
            confidence = result.confidences[i];
        }        
        detectionTable_->setItem(i, 0, new QTableWidgetItem(componentType));
        detectionTable_->setItem(i, 1, new QTableWidgetItem(QString("(%1,%2)").arg(componentBounds.x).arg(componentBounds.y)));
        detectionTable_->setItem(i, 2, new QTableWidgetItem(QString("%1x%2").arg(componentBounds.width).arg(componentBounds.height)));
        detectionTable_->setItem(i, 3, new QTableWidgetItem(QString::number(confidence, 'f', 2)));
    }
    
    // 更新详细信息
    QString details = QString("检测结果摘要:\n");
    details += QString("总计发现 %1 个组件\n").arg(result.detectedComponents.size());
    details += QString("PCB型号: %1\n").arg(result.pcbModel.isEmpty() ? "未知" : result.pcbModel);
    details += QString("检测时间: %1ms\n").arg(result.processingTime);
    details += QString("平均置信度: %1\n").arg(result.averageConfidence, 0, 'f', 2);
    
    detectionDetails_->setText(details);
}

bool CameraControlWidget::saveDetectionResultsToFile(const QString& fileName, const PCBDetectionResult& result)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << "PCB元件检测结果报告\n";
    out << "==================\n\n";
    out << "检测时间: " << QDateTime::currentDateTime().toString() << "\n";
    out << "PCB型号: " << (result.pcbModel.isEmpty() ? "未知" : result.pcbModel) << "\n";
    out << "处理时间: " << result.processingTime << "ms\n";
    out << "平均置信度: " << QString::number(result.averageConfidence, 'f', 2) << "\n\n";    
    out << "检测到的组件:\n";
    out << "-------------\n";
    for (int i = 0; i < result.detectedComponents.size(); ++i) {
        const QString& componentType = result.detectedComponents[i];
        cv::Rect componentBounds;
        double confidence = 0.0;
        
        if (i < result.componentBounds.size()) {
            componentBounds = result.componentBounds[i];
        }
        if (i < result.confidences.size()) {
            confidence = result.confidences[i];
        }
        
        out << "类型: " << componentType << "\n";
        out << "位置: (" << componentBounds.x << ", " << componentBounds.y << ")\n";
        out << "大小: " << componentBounds.width << "x" << componentBounds.height << "\n";
        out << "置信度: " << QString::number(confidence, 'f', 2) << "\n\n";
    }    
    return true;
}

// Additional missing function implementations

void CameraControlWidget::onPCBModelIdentified(const QString& model, double confidence)
{
    QString message = QString("PCB型号识别完成: %1 (置信度: %2)")
                        .arg(model).arg(confidence, 0, 'f', 2);
    addLogMessage(message);
    
    // 更新检测详情显示
    QString details = detectionDetails_->toPlainText();
    details += QString("\n模型识别结果: %1\n置信度: %2\n")
                .arg(model).arg(confidence, 0, 'f', 2);
    detectionDetails_->setText(details);
}

void CameraControlWidget::onSetTemperatureThreshold()
{
    double threshold = thresholdSpin_->value();
    cameraManager_->setTemperatureThreshold(threshold);
    addLogMessage(QString("温度阈值已设置为: %1°C").arg(threshold, 0, 'f', 1));
}

void CameraControlWidget::onMarkMaxTempToggled(bool enabled)
{
    // 设置是否标记最高温度点
    if (irImageDisplay_) {
        irImageDisplay_->MarkMaxTemp(enabled);
        addLogMessage(enabled ? "已启用最高温度点标记" : "已禁用最高温度点标记");
    }
}

void CameraControlWidget::onIRCameraParamsChanged()
{
    // 当红外摄像头参数变化时的处理
    addLogMessage("红外摄像头参数已更新");
}

void CameraControlWidget::onSaveDetectionResult()
{
    onSaveDetectionResults(); // 调用现有的保存函数
}

void CameraControlWidget::updateTemperatureDisplay()
{
    // 更新温度显示信息
    ThermalData data = cameraManager_->getLatestThermalData();
    if (data.isValid) {
        currentTempLabel_->setText(QString::number(data.avgTemp, 'f', 1) + "°C");
        maxTempLabel_->setText(QString::number(data.maxTemp, 'f', 1) + "°C");
        minTempLabel_->setText(QString::number(data.minTemp, 'f', 1) + "°C");
        avgTempLabel_->setText(QString::number(data.avgTemp, 'f', 1) + "°C");
    }
}

void CameraControlWidget::onHDCameraParamsChanged()
{
    // 当HD摄像头参数变化时的处理
    addLogMessage("HD摄像头参数已更新");
    
    // 可以在此添加参数变化后的其他处理逻辑
    // 比如更新UI显示或重新配置摄像头
}

void CameraControlWidget::onSaveThermalData()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "保存热成像数据", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "热成像数据 (*.csv *.png)");
        
    if (!fileName.isEmpty()) {        if (cameraManager_->saveThermalData(fileName)) {
            addLogMessage("热成像数据已保存: " + fileName);
        } else {
            QMessageBox::warning(this, "错误", "保存热成像数据失败");
        }
    }
}

void CameraControlWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // 当窗口大小变化时，重新调整图像显示
    if (hdImageItem_ && !hdImageItem_->pixmap().isNull()) {
        // 重新适应HD图像
        hdImageView_->fitInView(hdImageItem_, Qt::KeepAspectRatio);
    }
    
    // IRImageDisplay 会自动处理自己的 resize 事件，无需手动调整
    
    if (detectionImageItem_ && !detectionImageItem_->pixmap().isNull()) {
        // 重新适应检测结果图像
        detectionImageView_->fitInView(detectionImageItem_, Qt::KeepAspectRatio);
    }
}

// 实时PCB识别槽函数实现

void CameraControlWidget::onStartRealtimePCBIdentification()
{
    if (isRealtimeIdentificationRunning_) {
        return;
    }
    
    addLogMessage("正在启动实时PCB识别...");
    
    // 确保HD摄像头可用
    if (!cameraManager_->isCameraAvailable(CameraType::HD_CAMERA)) {
        QMessageBox::warning(this, "错误", "HD摄像头不可用，请先启动HD摄像头");
        addLogMessage("启动实时识别失败：HD摄像头不可用");
        return;
    }
    
    // 设置参数
    pcbIdentifier_->setRealtimeProcessingInterval(realtimeIntervalSpin_->value());
    pcbIdentifier_->setMatchThreshold(realtimeThresholdSpin_->value());
    
    // 启动识别
    if (pcbIdentifier_->startRealtimeIdentification(cameraManager_)) {
        addLogMessage("实时PCB识别已启动");
        realtimeResultLabel_->setText("识别结果: 识别中...");
        realtimeConfidenceLabel_->setText("置信度: 0%");
    } else {
        QString error = QString("启动实时识别失败: %1").arg(pcbIdentifier_->getLastError());
        QMessageBox::critical(this, "错误", error);
        addLogMessage(error);
    }
}

void CameraControlWidget::onStopRealtimePCBIdentification()
{
    if (!isRealtimeIdentificationRunning_) {
        return;
    }
    
    pcbIdentifier_->stopRealtimeIdentification();
    addLogMessage("实时PCB识别已停止");
}

void CameraControlWidget::onRealtimeIdentificationCompleted(const RealtimeIdentificationResult& result)
{
    lastRealtimeResult_ = result;
    
    if (result.isValid) {
        realtimeResultLabel_->setText(QString("识别结果: %1").arg(result.modelName));
        realtimeConfidenceLabel_->setText(QString("置信度: %1%").arg(result.confidence, 0, 'f', 1));
        
        addLogMessage(QString("实时识别: %1 (置信度: %2%)")
                     .arg(result.modelName)
                     .arg(result.confidence, 0, 'f', 1));
    } else {
        realtimeResultLabel_->setText("识别结果: 未识别");
        realtimeConfidenceLabel_->setText("置信度: 0%");
    }
}

void CameraControlWidget::onRealtimeIdentificationStarted()
{
    isRealtimeIdentificationRunning_ = true;
    startRealtimePCBButton_->setEnabled(false);
    stopRealtimePCBButton_->setEnabled(true);
    addLogMessage("实时PCB识别已开始");
}

void CameraControlWidget::onRealtimeIdentificationStopped()
{
    isRealtimeIdentificationRunning_ = false;
    startRealtimePCBButton_->setEnabled(true);
    stopRealtimePCBButton_->setEnabled(false);
    realtimeResultLabel_->setText("识别结果: 已停止");
    realtimeConfidenceLabel_->setText("置信度: 0%");
    addLogMessage("实时PCB识别已停止");
}

void CameraControlWidget::onRealtimeIdentificationError(const QString& error)
{
    addLogMessage(QString("实时识别错误: %1").arg(error));
    QMessageBox::warning(this, "实时识别错误", error);
}

void CameraControlWidget::onRealtimeIntervalChanged(int intervalMs)
{
    if (pcbIdentifier_ && isRealtimeIdentificationRunning_) {
        pcbIdentifier_->setRealtimeProcessingInterval(intervalMs);
        addLogMessage(QString("识别间隔已更新为 %1 ms").arg(intervalMs));
    }
}

void CameraControlWidget::onRealtimeThresholdChanged(double threshold)
{
    if (pcbIdentifier_) {
        pcbIdentifier_->setMatchThreshold(threshold);
        addLogMessage(QString("匹配阈值已更新为 %1").arg(threshold, 0, 'f', 2));
    }
}
