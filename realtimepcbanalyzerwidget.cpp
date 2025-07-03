#include "realtimepcbanalyzerwidget.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QSplitter>
#include <QHeaderView>
#include <QThread>
#include <QApplication>
#include <QMutexLocker>
#include <QTableWidgetItem>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QInputDialog>
#include <algorithm>
#include <chrono>

RealtimePCBAnalyzerWidget::RealtimePCBAnalyzerWidget(QWidget *parent)
    : QWidget(parent)
    , pcb_identifier_(nullptr)
    , camera_manager_(nullptr)
    , yolo_model_(YOLOModel::getInstance())
    , detection_manager_(new PCBDetectionManager(this))
    , is_running_(false)
    , stop_analysis_(false)
    , analysis_mode_(COMPREHENSIVE_ANALYSIS)
    , display_timer_(new QTimer(this))
    , analysis_timer_(new QTimer(this))
    , is_processing_(false)
    , show_annotations_(true)
    , auto_save_enabled_(false)
    , zoom_level_(DEFAULT_ZOOM)
    , display_mode_("综合显示")
    , total_analyses_(0)
    , successful_analyses_(0)
    , total_processing_time_(0.0)
{
    setupUI();
    connectSignals();
    
    // 初始化数据管理器
    if (!detection_manager_->initializeDatabase()) {
        addLogMessage("警告：数据库初始化失败，数据保存功能将不可用");
    } else {
        addLogMessage("数据管理系统初始化成功");
    }
    
    // 设置显示更新定时器
    display_timer_->setInterval(UPDATE_DISPLAY_INTERVAL);
    connect(display_timer_, &QTimer::timeout, this, &RealtimePCBAnalyzerWidget::updateCameraImage);
    
    // 设置分析定时器
    analysis_timer_->setInterval(DEFAULT_INTERVAL_MS);
    analysis_timer_->setSingleShot(false);
    connect(analysis_timer_, &QTimer::timeout, this, &RealtimePCBAnalyzerWidget::performComprehensiveAnalysis);
    
    // 初始化组件名称和颜色
    component_names_ = yolo_model_->get_class_names();
    component_colors_ = yolo_model_->get_colors();
    enabled_components_ = yolo_model_->get_class_display();
    
    // 初始化状态
    updateStatus("就绪 - 等待启动实时分析");
    session_start_time_ = QDateTime::currentDateTime();
}

RealtimePCBAnalyzerWidget::~RealtimePCBAnalyzerWidget()
{
    // 设置停止标志
    stop_analysis_ = true;
    
    // 停止所有定时器
    if (analysis_timer_) {
        analysis_timer_->stop();
        analysis_timer_ = nullptr;
    }
    
    if (display_timer_) {
        display_timer_->stop();
        display_timer_->deleteLater();
        display_timer_ = nullptr;
    }
    
    // 停止实时分析
    if (is_running_) {
        is_running_ = false;
        addLogMessage("组件正在关闭，停止实时分析...");
    }
    
    // 清理所有活跃的future watchers
    for (auto* watcher : active_watchers_) {
        if (watcher) {
            watcher->cancel();
            watcher->waitForFinished();
            watcher->deleteLater();
        }
    }
    active_watchers_.clear();
    
    // 等待当前分析完成（最多等待3秒）
    int wait_count = 0;
    while (is_processing_ && wait_count < 30) {
        QApplication::processEvents();
        QThread::msleep(100);
        wait_count++;
    }
      // 如果还有处理在进行，强制停止
    if (is_processing_) {
        is_processing_ = false;
        addLogMessage("警告：强制停止正在进行的分析");
    }
    
    // 停止并清理摄像头资源
    if (camera_manager_) {
        addLogMessage("正在关闭HD摄像头...");
        
        if (camera_manager_->isCameraAvailable(CameraType::HD_CAMERA)) {
            camera_manager_->stopCamera(CameraType::HD_CAMERA);
            addLogMessage("HD摄像头已停止");
        }
        
        QApplication::processEvents();
        QThread::msleep(200);
    }
      // 清理资源
    pcb_identifier_ = nullptr;
    camera_manager_ = nullptr;
    
    addLogMessage("PCB综合分析组件已完全关闭");
    
    // 强制处理所有待处理的事件
    QApplication::processEvents();
    
    addLogMessage("PCB综合分析组件已关闭");
}

void RealtimePCBAnalyzerWidget::closeEvent(QCloseEvent* event)
{
    // 停止分析
    stopRealtimeAnalysis();
    event->accept();
}

void RealtimePCBAnalyzerWidget::setupUI()
{
    auto* main_layout = new QVBoxLayout(this);
    
    // 创建主分割器
    auto* main_splitter = new QSplitter(Qt::Horizontal);
    
    // 设置控制和显示面板
    setupControlPanel();
    setupDisplayPanel();
    setupResultsPanel();
    setupComponentFilterPanel();
    
    // 创建左侧面板（控制 + 过滤 + 结果）
    auto* left_widget = new QWidget;
    auto* left_layout = new QVBoxLayout(left_widget);
    left_layout->addWidget(control_group_);
    left_layout->addWidget(filter_group_);
    left_layout->addWidget(results_group_);
    left_layout->setStretch(2, 1); // 结果面板可拉伸
    
    // 添加到分割器
    main_splitter->addWidget(left_widget);
    main_splitter->addWidget(display_group_);
    main_splitter->setStretchFactor(0, 0); // 左侧固定宽度
    main_splitter->setStretchFactor(1, 1); // 显示面板可拉伸
    
    // 状态栏
    status_label_ = new QLabel("就绪");
    status_label_->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 5px; border: 1px solid #ccc; }");
    
    // 添加到主布局
    main_layout->addWidget(main_splitter);
    main_layout->addWidget(status_label_);
}

void RealtimePCBAnalyzerWidget::setupControlPanel()
{
    control_group_ = new QGroupBox("实时分析控制");
    auto* layout = new QGridLayout(control_group_);
    
    // 控制按钮
    start_button_ = new QPushButton("开始分析");
    stop_button_ = new QPushButton("停止分析");
    stop_button_->setEnabled(false);
    
    start_button_->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px; }");
    stop_button_->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; padding: 8px; }");
    
    // 分析模式选择
    analysis_mode_combo_ = new QComboBox;
    analysis_mode_combo_->addItems({"仅PCB识别", "仅元件检测", "综合分析"});
    analysis_mode_combo_->setCurrentIndex(2); // 默认综合分析
    analysis_mode_combo_->setToolTip("选择分析模式");
    
    // 参数设置
    interval_spinbox_ = new QSpinBox;
    interval_spinbox_->setRange(500, 10000);
    interval_spinbox_->setValue(DEFAULT_INTERVAL_MS);
    interval_spinbox_->setSuffix(" ms");
    interval_spinbox_->setToolTip("分析间隔时间（毫秒）");
    
    pcb_threshold_spinbox_ = new QDoubleSpinBox;
    pcb_threshold_spinbox_->setRange(0.1, 1.0);
    pcb_threshold_spinbox_->setValue(DEFAULT_PCB_THRESHOLD);
    pcb_threshold_spinbox_->setDecimals(2);
    pcb_threshold_spinbox_->setSingleStep(0.05);
    pcb_threshold_spinbox_->setToolTip("PCB识别匹配阈值");
    
    component_confidence_spinbox_ = new QDoubleSpinBox;
    component_confidence_spinbox_->setRange(0.1, 1.0);
    component_confidence_spinbox_->setValue(DEFAULT_COMPONENT_CONFIDENCE);
    component_confidence_spinbox_->setDecimals(2);
    component_confidence_spinbox_->setSingleStep(0.05);
    component_confidence_spinbox_->setToolTip("元件检测置信度阈值");
    
    // 显示选项
    show_annotations_checkbox_ = new QCheckBox("显示标注");
    show_annotations_checkbox_->setChecked(true);
    show_annotations_checkbox_->setToolTip("在图像上显示分析结果标注");
    
    auto_save_checkbox_ = new QCheckBox("自动保存");
    auto_save_checkbox_->setToolTip("自动保存分析结果");
    
    // 显示模式
    display_mode_combo_ = new QComboBox;
    display_mode_combo_->addItems({"原始图像", "PCB标注", "元件标注", "综合标注"});
    display_mode_combo_->setCurrentText("综合标注");
    
    // 缩放控制
    zoom_spinbox_ = new QSpinBox;
    zoom_spinbox_->setRange(25, 400);
    zoom_spinbox_->setValue(DEFAULT_ZOOM);
    zoom_spinbox_->setSuffix(" %");
    zoom_spinbox_->setToolTip("图像缩放比例");
    
    // 布局
    int row = 0;
    layout->addWidget(new QLabel("控制:"), row, 0);
    layout->addWidget(start_button_, row, 1);
    layout->addWidget(stop_button_, row, 2);
    
    row++;
    layout->addWidget(new QLabel("分析模式:"), row, 0);
    layout->addWidget(analysis_mode_combo_, row, 1, 1, 2);
    
    row++;
    layout->addWidget(new QLabel("分析间隔:"), row, 0);
    layout->addWidget(interval_spinbox_, row, 1, 1, 2);
    
    row++;
    layout->addWidget(new QLabel("PCB阈值:"), row, 0);
    layout->addWidget(pcb_threshold_spinbox_, row, 1, 1, 2);
    
    row++;
    layout->addWidget(new QLabel("元件置信度:"), row, 0);
    layout->addWidget(component_confidence_spinbox_, row, 1, 1, 2);
    
    row++;
    layout->addWidget(show_annotations_checkbox_, row, 0, 1, 3);
    
    row++;
    layout->addWidget(auto_save_checkbox_, row, 0, 1, 3);
    
    row++;
    layout->addWidget(new QLabel("显示模式:"), row, 0);
    layout->addWidget(display_mode_combo_, row, 1, 1, 2);
    
    row++;
    layout->addWidget(new QLabel("缩放:"), row, 0);
    layout->addWidget(zoom_spinbox_, row, 1, 1, 2);
}

void RealtimePCBAnalyzerWidget::setupDisplayPanel()
{
    display_group_ = new QGroupBox("实时图像显示与分析结果");
    auto* layout = new QVBoxLayout(display_group_);
    
    // 创建标签页
    display_tabs_ = new QTabWidget;
    
    // 图像显示标签页
    auto* image_tab = new QWidget;
    auto* image_layout = new QVBoxLayout(image_tab);
    
    // 图像显示区域
    image_label_ = new QLabel;
    image_label_->setAlignment(Qt::AlignCenter);
    image_label_->setMinimumSize(800, 600);
    image_label_->setStyleSheet("QLabel { background-color: #2b2b2b; border: 2px solid #555; }");
    image_label_->setText("等待摄像头图像...");
    image_label_->setStyleSheet("QLabel { background-color: #2b2b2b; color: white; border: 2px solid #555; }");
    
    image_scroll_area_ = new QScrollArea;
    image_scroll_area_->setWidget(image_label_);
    image_scroll_area_->setAlignment(Qt::AlignCenter);
    image_scroll_area_->setMinimumSize(820, 620);
    
    image_layout->addWidget(image_scroll_area_);
    display_tabs_->addTab(image_tab, "图像显示");
      // PCB识别结果标签页
    setupPCBResultTab();
    
    // 元件检测结果标签页
    setupComponentResultTab();
    
    layout->addWidget(display_tabs_);
}

void RealtimePCBAnalyzerWidget::setupPCBResultTab()
{
    pcb_tab_ = new QWidget;
    auto* layout = new QGridLayout(pcb_tab_);
    
    pcb_model_label_ = new QLabel("模型: 未识别");
    pcb_model_label_->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; }");
    
    pcb_confidence_bar_ = new QProgressBar;
    pcb_confidence_bar_->setRange(0, 100);
    pcb_confidence_bar_->setValue(0);
    pcb_confidence_bar_->setFormat("置信度: %p%");
    
    pcb_match_count_label_ = new QLabel("匹配点数: 0");
    pcb_processing_time_label_ = new QLabel("处理时间: 0 ms");
    
    layout->addWidget(pcb_model_label_, 0, 0, 1, 2);
    layout->addWidget(pcb_confidence_bar_, 1, 0, 1, 2);
    layout->addWidget(pcb_match_count_label_, 2, 0);
    layout->addWidget(pcb_processing_time_label_, 2, 1);
    
    display_tabs_->addTab(pcb_tab_, "PCB识别");
}

void RealtimePCBAnalyzerWidget::setupComponentResultTab()
{
    component_tab_ = new QWidget;
    auto* layout = new QVBoxLayout(component_tab_);
    
    total_components_label_ = new QLabel("检测到的元件总数: 0");
    total_components_label_->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; }");
    layout->addWidget(total_components_label_);
    
    // 创建元件统计表格
    component_table_ = new QTableWidget;
    component_table_->setColumnCount(3);
    component_table_->setHorizontalHeaderLabels({"元件类型", "数量", "置信度"});
    component_table_->horizontalHeader()->setStretchLastSection(true);
    component_table_->setAlternatingRowColors(true);
    component_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    layout->addWidget(component_table_);
    display_tabs_->addTab(component_tab_, "元件检测");
}

void RealtimePCBAnalyzerWidget::setupComponentFilterPanel()
{
    filter_group_ = new QGroupBox("元件类型过滤");
    auto* layout = new QVBoxLayout(filter_group_);
    
    // 全选/取消全选按钮
    auto* button_layout = new QHBoxLayout;
    select_all_button_ = new QPushButton("全选");
    deselect_all_button_ = new QPushButton("取消全选");
    button_layout->addWidget(select_all_button_);
    button_layout->addWidget(deselect_all_button_);
    layout->addLayout(button_layout);
    
    // 创建组件复选框
    auto* checkbox_layout = new QGridLayout;
    component_checkboxes_.clear();
    
    for (size_t i = 0; i < component_names_.size(); ++i) {
        auto* checkbox = new QCheckBox(QString::fromStdString(component_names_[i]));
        checkbox->setChecked(true);
        
        // 设置背景颜色
        cv::Scalar color = component_colors_[i];
        QString styleSheet = QString("QCheckBox { background-color: rgb(%1, %2, %3); color: black; padding: 2px; }")
                           .arg(static_cast<int>(color[2]))
                           .arg(static_cast<int>(color[1]))
                           .arg(static_cast<int>(color[0]));
        checkbox->setStyleSheet(styleSheet);
        
        component_checkboxes_.push_back(checkbox);
        
        int row = i / 2;
        int col = i % 2;
        checkbox_layout->addWidget(checkbox, row, col);
    }
    
    layout->addLayout(checkbox_layout);
}

void RealtimePCBAnalyzerWidget::setupResultsPanel()
{
    results_group_ = new QGroupBox("分析结果与统计");
    auto* layout = new QVBoxLayout(results_group_);
    
    // 统计信息
    auto* stats_layout = new QGridLayout;
    
    total_analysis_label_ = new QLabel("总分析次数: 0");
    success_rate_label_ = new QLabel("成功率: 0%");
    average_processing_time_label_ = new QLabel("平均处理时间: 0 ms");
    
    stats_layout->addWidget(total_analysis_label_, 0, 0);
    stats_layout->addWidget(success_rate_label_, 0, 1);
    stats_layout->addWidget(average_processing_time_label_, 1, 0, 1, 2);
    
    // 日志区域
    log_text_ = new QTextEdit;
    log_text_->setMaximumHeight(200);
    log_text_->setFont(QFont("Consolas", 9));
      // 操作按钮
    auto* button_layout = new QHBoxLayout;
    save_result_button_ = new QPushButton("保存当前结果");
    clear_log_button_ = new QPushButton("清空日志");
    export_button_ = new QPushButton("导出数据");
    cleanup_button_ = new QPushButton("清理旧数据");
    
    button_layout->addWidget(save_result_button_);
    button_layout->addWidget(clear_log_button_);
    button_layout->addWidget(export_button_);
    button_layout->addWidget(cleanup_button_);
    button_layout->addStretch();
    
    layout->addLayout(stats_layout);
    layout->addWidget(new QLabel("分析日志:"));
    layout->addWidget(log_text_);
    layout->addLayout(button_layout);
}

void RealtimePCBAnalyzerWidget::connectSignals()
{
    // 控制信号
    connect(start_button_, &QPushButton::clicked, this, &RealtimePCBAnalyzerWidget::startRealtimeAnalysis);
    connect(stop_button_, &QPushButton::clicked, this, &RealtimePCBAnalyzerWidget::stopRealtimeAnalysis);
    
    // 参数变化信号
    connect(analysis_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RealtimePCBAnalyzerWidget::onAnalysisModeChanged);
    connect(interval_spinbox_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RealtimePCBAnalyzerWidget::onAnalysisIntervalChanged);
    connect(pcb_threshold_spinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RealtimePCBAnalyzerWidget::onPCBThresholdChanged);
    connect(component_confidence_spinbox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RealtimePCBAnalyzerWidget::onComponentConfidenceChanged);
    connect(show_annotations_checkbox_, &QCheckBox::toggled,
            this, &RealtimePCBAnalyzerWidget::onShowAnnotationsToggled);
    connect(auto_save_checkbox_, &QCheckBox::toggled,
            this, &RealtimePCBAnalyzerWidget::onAutoSaveToggled);
    
    // 显示控制信号
    connect(display_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RealtimePCBAnalyzerWidget::onDisplayModeChanged);
    connect(zoom_spinbox_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RealtimePCBAnalyzerWidget::onZoomChanged);
    connect(display_tabs_, &QTabWidget::currentChanged,
            this, &RealtimePCBAnalyzerWidget::onTabChanged);
    
    // 组件过滤信号
    connect(select_all_button_, &QPushButton::clicked, 
            this, &RealtimePCBAnalyzerWidget::selectAllComponents);
    connect(deselect_all_button_, &QPushButton::clicked,
            this, &RealtimePCBAnalyzerWidget::deselectAllComponents);
    
    // 为每个组件复选框连接信号
    for (size_t i = 0; i < component_checkboxes_.size(); ++i) {
        connect(component_checkboxes_[i], &QCheckBox::toggled,
                [this, i](bool enabled) {
                    onComponentTypeToggled(static_cast<int>(i), enabled);
                });
    }
      // 操作按钮信号
    connect(save_result_button_, &QPushButton::clicked, this, &RealtimePCBAnalyzerWidget::saveCurrentResult);
    connect(clear_log_button_, &QPushButton::clicked, this, &RealtimePCBAnalyzerWidget::clearResults);
    connect(export_button_, &QPushButton::clicked, this, &RealtimePCBAnalyzerWidget::exportResults);
    connect(cleanup_button_, &QPushButton::clicked, this, &RealtimePCBAnalyzerWidget::deleteOldRecords);
}

void RealtimePCBAnalyzerWidget::setPCBIdentifier(PCBIdentifier* identifier)
{
    if (pcb_identifier_) {
        disconnect(pcb_identifier_, nullptr, this, nullptr);
    }
    
    pcb_identifier_ = identifier;
    
    if (pcb_identifier_) {
        addLogMessage("PCB识别器已连接");
    }
}

void RealtimePCBAnalyzerWidget::setCameraManager(CameraManager* cameraManager)
{
    camera_manager_ = cameraManager;
    
    if (camera_manager_) {
        addLogMessage("摄像头管理器已连接");
    }
}

void RealtimePCBAnalyzerWidget::startRealtimeAnalysis()
{
    if (is_running_) {
        return;
    }
    
    if (!camera_manager_) {
        QMessageBox::warning(this, "错误", "摄像头管理器未设置");
        return;
    }
    
    // 根据分析模式检查所需组件
    if ((analysis_mode_ == PCB_IDENTIFICATION_ONLY || analysis_mode_ == COMPREHENSIVE_ANALYSIS) && !pcb_identifier_) {
        QMessageBox::warning(this, "错误", "PCB识别器未设置");
        return;
    }
    
    // 确保HD摄像头已经初始化和启动
    if (!camera_manager_->isCameraAvailable(CameraType::HD_CAMERA)) {
        addLogMessage("正在初始化HD摄像头...");
        
        if (!camera_manager_->initializeCamera(CameraType::HD_CAMERA)) {
            QMessageBox::critical(this, "错误", "HD摄像头初始化失败");
            addLogMessage("HD摄像头初始化失败");
            return;
        }
        
        addLogMessage("HD摄像头初始化成功，正在启动...");
        
        if (!camera_manager_->startCamera(CameraType::HD_CAMERA)) {
            QMessageBox::critical(this, "错误", "HD摄像头启动失败");
            addLogMessage("HD摄像头启动失败");
            return;
        }
        
        addLogMessage("HD摄像头启动成功");
        QApplication::processEvents();
        QThread::msleep(500);
    }
    
    // 设置参数
    if (pcb_identifier_) {
        pcb_identifier_->setMatchThreshold(pcb_threshold_spinbox_->value());
    }
    
    // 启动分析
    analysis_timer_->setInterval(interval_spinbox_->value());
    analysis_timer_->start();
    display_timer_->start();
    
    is_running_ = true;
    stop_analysis_ = false;
    
    addLogMessage("实时分析已启动");
    onAnalysisStarted();
    
    // 重置统计
    total_analyses_ = 0;
    successful_analyses_ = 0;
    total_processing_time_ = 0.0;
    session_start_time_ = QDateTime::currentDateTime();
    updateAnalysisStatistics();
}

void RealtimePCBAnalyzerWidget::stopRealtimeAnalysis()
{
    if (!is_running_) {
        return;
    }
    
    addLogMessage("正在停止实时分析...");
    
    // 设置停止标志
    stop_analysis_ = true;
    is_running_ = false;
    
    // 停止定时器
    if (analysis_timer_) {
        analysis_timer_->stop();
    }
    if (display_timer_) {
        display_timer_->stop();
    }
    
    // 取消所有活跃的future watchers
    for (auto* watcher : active_watchers_) {
        if (watcher && !watcher->isFinished()) {
            watcher->cancel();
        }
    }
    
    // 等待当前分析完成（最多等待2秒）
    int wait_count = 0;
    while (is_processing_ && wait_count < 20) {
        QApplication::processEvents();
        QThread::msleep(100);
        wait_count++;
    }
    
    if (camera_manager_) {
        addLogMessage("正在关闭HD摄像头...");
        
        if (camera_manager_->isCameraAvailable(CameraType::HD_CAMERA)) {
            camera_manager_->stopCamera(CameraType::HD_CAMERA);
            addLogMessage("HD摄像头已停止");
        }
        
        QApplication::processEvents();
        QThread::msleep(200);
    }

    if (is_processing_) {
        addLogMessage("警告：强制停止分析进程");
        is_processing_ = false;
    }
    
    addLogMessage("实时分析已停止");
    onAnalysisStopped();
}

void RealtimePCBAnalyzerWidget::performComprehensiveAnalysis()
{
    if (!camera_manager_ || stop_analysis_ || is_processing_) {
        return;
    }
    
    // 获取最新的高清摄像头图像
    ImageData imageData = camera_manager_->getLatestImage(CameraType::HD_CAMERA);
    
    if (!imageData.isValid || imageData.image.empty()) {
        return;
    }
    
    // 避免重复处理相同的图像
    static QDateTime lastProcessedImageTime;
    if (lastProcessedImageTime.isValid() && lastProcessedImageTime == imageData.timestamp) {
        return;
    }
    
    is_processing_ = true;
    
    // 在后台线程中进行分析
    auto future = QtConcurrent::run([this, imageData]() -> PCBAnalysisResult {
        return analyzeImage(imageData.image);
    });
      // 设置分析完成的回调
    auto* watcher = new QFutureWatcher<PCBAnalysisResult>(this);
    active_watchers_.append(watcher);
    
    connect(watcher, &QFutureWatcher<PCBAnalysisResult>::finished, [this, watcher]() {
        if (!stop_analysis_) {
            PCBAnalysisResult result = watcher->result();
            onRealtimeAnalysisCompleted(result);
        }
        is_processing_ = false;
        active_watchers_.removeAll(watcher);
        watcher->deleteLater();
    });
    
    watcher->setFuture(future);
    lastProcessedImageTime = imageData.timestamp;
}

PCBAnalysisResult RealtimePCBAnalyzerWidget::analyzeImage(const cv::Mat& image)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PCBAnalysisResult result;
    result.timestamp = QDateTime::currentDateTime();
    
    // 检查是否应该停止分析
    if (stop_analysis_) {
        result.isValid = false;
        return result;
    }
    
    try {
        // 根据分析模式进行处理
        switch (analysis_mode_) {
        case PCB_IDENTIFICATION_ONLY:
            processPCBIdentification(image, result);
            break;
        case COMPONENT_DETECTION_ONLY:
            processComponentDetection(image, result);
            break;
        case COMPREHENSIVE_ANALYSIS:
            processPCBIdentification(image, result);
            processComponentDetection(image, result);
            break;
        }
        
        // 创建综合标注图像
        if (show_annotations_) {
            result.annotatedImage = createCombinedAnnotation(image, result.pcbIdentResult, result.componentLabels);
        } else {
            result.annotatedImage = image.clone();
        }
        
        result.isValid = true;
        
    } catch (const std::exception& e) {
        qWarning() << "Analysis error:" << e.what();
        result.isValid = false;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.analysisTime = duration.count();
    
    return result;
}

void RealtimePCBAnalyzerWidget::processPCBIdentification(const cv::Mat& image, PCBAnalysisResult& result)
{
    if (!pcb_identifier_) {
        return;
    }
    
    result.pcbIdentResult = pcb_identifier_->identifyPCBFromImage(image);
}

void RealtimePCBAnalyzerWidget::processComponentDetection(const cv::Mat& image, PCBAnalysisResult& result)
{
    if (!yolo_model_) {
        return;
    }
    
    // 更新YOLO模型的显示类别
    yolo_model_->set_class_display(enabled_components_);
    
    // 进行元件检测
    cv::Mat annotated_image = yolo_model_->recognize(image);
    result.componentLabels = yolo_model_->get_labels();
    result.totalComponents = result.componentLabels.size();
    
    // 统计各类元件数量
    result.componentCounts.clear();
    for (const auto& label : result.componentLabels) {
        std::string component_name = component_names_[label.cls];
        result.componentCounts[component_name]++;
    }
}

cv::Mat RealtimePCBAnalyzerWidget::createCombinedAnnotation(const cv::Mat& originalImage,
                                                          const RealtimeIdentificationResult& pcbResult,
                                                          const std::vector<Label>& componentLabels)
{
    cv::Mat result = originalImage.clone();
    
    // 绘制元件检测结果
    for (const auto& label : componentLabels) {
        if (label.cls < 0 || label.cls >= static_cast<int>(component_colors_.size())) {
            continue;
        }
        
        cv::Scalar color = component_colors_[label.cls];
        std::string class_name = component_names_[label.cls];
        
        // 绘制边界框
        cv::rectangle(result, cv::Point(label.x, label.y), 
                     cv::Point(label.x + label.w, label.y + label.h), color, 2);
        
        // 绘制标签
        std::string label_text = class_name + " " + std::to_string(static_cast<int>(label.confidence * 100)) + "%";
        cv::putText(result, label_text, cv::Point(label.x, label.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
    }
    
    // 绘制PCB识别结果（如果有效）
    if (pcbResult.isValid && !pcbResult.annotatedImage.empty()) {
        // 在图像顶部添加PCB识别信息
        std::string pcb_info = "PCB: " + pcbResult.modelName.toStdString() + 
                              " (" + std::to_string(static_cast<int>(pcbResult.confidence)) + "%)";
        cv::putText(result, pcb_info, cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
    }
    
    return result;
}

void RealtimePCBAnalyzerWidget::onRealtimeAnalysisCompleted(const PCBAnalysisResult& result)
{
    current_result_ = result;
    total_analyses_++;
    
    if (result.isValid) {
        successful_analyses_++;
        total_processing_time_ += result.analysisTime;
        
        // 更新显示
        updatePCBIdentificationDisplay(result.pcbIdentResult);
        updateComponentDetectionDisplay(result.componentLabels);
        updateComponentStatistics(result);
          addLogMessage(QString("[%1] 分析完成 - PCB: %2, 元件: %3个, 用时: %4ms")
                     .arg(QTime::currentTime().toString("hh:mm:ss"))
                     .arg(result.pcbIdentResult.isValid ? result.pcbIdentResult.modelName : "未识别")
                     .arg(result.totalComponents)
                     .arg(static_cast<int>(result.analysisTime)));
        
        // 自动保存功能
        if (auto_save_enabled_) {
            onAutoSaveResult(result);
        }
    } else {
        addLogMessage(QString("[%1] 分析失败")
                     .arg(QTime::currentTime().toString("hh:mm:ss")));
    }
    
    updateAnalysisStatistics();
    updateImageDisplay();
}

void RealtimePCBAnalyzerWidget::updatePCBIdentificationDisplay(const RealtimeIdentificationResult& result)
{
    if (result.isValid) {
        pcb_model_label_->setText(QString("模型: %1").arg(result.modelName));
        pcb_confidence_bar_->setValue(static_cast<int>(result.confidence));
        pcb_match_count_label_->setText(QString("匹配点数: %1").arg(result.matchCount));
        
        // 设置置信度条颜色
        QString color = result.confidence > 70 ? "#4CAF50" :   // 绿色
                       result.confidence > 40 ? "#FF9800" :   // 橙色  
                                               "#F44336";     // 红色
        pcb_confidence_bar_->setStyleSheet(QString("QProgressBar::chunk { background-color: %1; }").arg(color));
    } else {
        pcb_model_label_->setText("模型: 未识别");
        pcb_confidence_bar_->setValue(0);
        pcb_match_count_label_->setText("匹配点数: 0");
    }
}

void RealtimePCBAnalyzerWidget::updateComponentDetectionDisplay(const std::vector<Label>& labels)
{
    total_components_label_->setText(QString("检测到的元件总数: %1").arg(labels.size()));
    
    // 统计各类元件数量
    std::map<std::string, int> counts;
    std::map<std::string, double> max_confidences;
    
    for (const auto& label : labels) {
        if (label.cls >= 0 && label.cls < static_cast<int>(component_names_.size())) {
            std::string name = component_names_[label.cls];
            counts[name]++;
            if (max_confidences.find(name) == max_confidences.end() || 
                label.confidence > max_confidences[name]) {
                max_confidences[name] = label.confidence;
            }
        }
    }
    
    // 更新表格
    component_table_->setRowCount(counts.size());
    int row = 0;
    for (const auto& pair : counts) {
        component_table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(pair.first)));
        component_table_->setItem(row, 1, new QTableWidgetItem(QString::number(pair.second)));
        component_table_->setItem(row, 2, new QTableWidgetItem(
            QString::number(max_confidences[pair.first] * 100, 'f', 1) + "%"));
        row++;
    }
}

void RealtimePCBAnalyzerWidget::updateComponentStatistics(const PCBAnalysisResult& result)
{
    // 更新统计标签或其他统计显示
    // 这里可以添加更详细的统计信息显示
}

void RealtimePCBAnalyzerWidget::updateCameraImage()
{
    if (!camera_manager_) {
        return;
    }
    
    static QDateTime lastUpdateTime;
    QDateTime currentTime = QDateTime::currentDateTime();
    
    // 避免过于频繁的更新（至少间隔50ms）
    if (lastUpdateTime.isValid() && lastUpdateTime.msecsTo(currentTime) < 50) {
        return;
    }
    
    ImageData imageData = camera_manager_->getLatestImage(CameraType::HD_CAMERA);
    if (imageData.isValid && !imageData.image.empty()) {
        // 检查是否是新的图像（通过时间戳）
        static QDateTime lastImageTime;
        if (lastImageTime.isValid() && lastImageTime == imageData.timestamp) {
            return;
        }
        
        current_display_image_ = imageData.image.clone();
        updateImageDisplay();
        
        lastImageTime = imageData.timestamp;
        lastUpdateTime = currentTime;
    }
}

void RealtimePCBAnalyzerWidget::updateImageDisplay()
{
    cv::Mat displayImage;
    
    if (current_display_image_.empty()) {
        return;
    }
    
    // 根据显示模式选择图像
    QString mode = display_mode_combo_->currentText();
    if (mode == "原始图像") {
        displayImage = current_display_image_;
    } else if (mode == "PCB标注" && current_result_.isValid && current_result_.pcbIdentResult.isValid && 
               !current_result_.pcbIdentResult.annotatedImage.empty()) {
        displayImage = current_result_.pcbIdentResult.annotatedImage;
    } else if (mode == "元件标注" && current_result_.isValid && !current_result_.componentLabels.empty()) {
        // 创建仅包含元件标注的图像
        displayImage = current_display_image_.clone();
        for (const auto& label : current_result_.componentLabels) {
            if (label.cls >= 0 && label.cls < static_cast<int>(component_colors_.size())) {
                cv::Scalar color = component_colors_[label.cls];
                std::string class_name = component_names_[label.cls];
                
                cv::rectangle(displayImage, cv::Point(label.x, label.y), 
                             cv::Point(label.x + label.w, label.y + label.h), color, 2);
                
                std::string label_text = class_name + " " + std::to_string(static_cast<int>(label.confidence * 100)) + "%";
                cv::putText(displayImage, label_text, cv::Point(label.x, label.y - 10),
                           cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
            }
        }
    } else if (mode == "综合标注" && current_result_.isValid && !current_result_.annotatedImage.empty()) {
        displayImage = current_result_.annotatedImage;
    } else {
        displayImage = current_display_image_;
    }
    
    if (!displayImage.empty()) {
        try {
            QPixmap pixmap = matToQPixmap(displayImage);
            
            if (pixmap.isNull()) {
                qWarning() << "Failed to convert cv::Mat to QPixmap";
                return;
            }
            
            // 应用缩放
            if (zoom_level_ != 100 && zoom_level_ > 0) {
                int w = pixmap.width() * zoom_level_ / 100;
                int h = pixmap.height() * zoom_level_ / 100;
                if (w > 0 && h > 0) {
                    pixmap = pixmap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
            }
            
            image_label_->setPixmap(pixmap);
            image_label_->resize(pixmap.size());
            
        } catch (const std::exception& e) {
            qWarning() << "Exception in updateImageDisplay:" << e.what();
        }
    }
}

QPixmap RealtimePCBAnalyzerWidget::matToQPixmap(const cv::Mat& mat)
{
    static QMutex imageMutex;
    QMutexLocker locker(&imageMutex);
    
    if (mat.empty()) {
        return QPixmap();
    }
    
    QImage qimg;
    try {
        if (mat.channels() == 3) {
            cv::Mat rgb = mat;
            
            if (!rgb.isContinuous()) {
                rgb = rgb.clone();
            }
            
            qimg = QImage(rgb.data, rgb.cols, rgb.rows, rgb.step[0], QImage::Format_RGB888).copy();
            
        } else if (mat.channels() == 1) {
            cv::Mat gray = mat;
            
            if (!gray.isContinuous()) {
                gray = gray.clone();
            }
            
            qimg = QImage(gray.data, gray.cols, gray.rows, gray.step[0], QImage::Format_Grayscale8).copy();
            
        } else {
            qDebug() << "Unsupported image format: channels =" << mat.channels();
            return QPixmap();
        }
        
        if (qimg.isNull()) {
            qDebug() << "Failed to create QImage from cv::Mat";
            return QPixmap();
        }
        
        return QPixmap::fromImage(qimg);
        
    } catch (const cv::Exception& e) {
        qWarning() << "OpenCV exception in matToQPixmap:" << e.what();
        return QPixmap();
    } catch (const std::exception& e) {
        qWarning() << "Standard exception in matToQPixmap:" << e.what();
        return QPixmap();
    }
}

void RealtimePCBAnalyzerWidget::saveCurrentResult()
{
    if (!current_result_.isValid) {
        QMessageBox::information(this, "提示", "当前没有有效的分析结果可保存");
        return;
    }
    
    try {
        // 创建检测记录
        PCBDetectionRecord record;
        record.id = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
        record.timestamp = current_result_.timestamp;        record.pcbModel = QString::fromStdString(current_result_.pcbIdentResult.modelName.toStdString());
        record.pcbConfidence = current_result_.pcbIdentResult.confidence;
        record.componentLabels = current_result_.componentLabels;
        record.totalComponents = current_result_.totalComponents;
        record.componentCounts = current_result_.componentCounts;
        record.analysisTime = current_result_.analysisTime;
        record.isValid = true;
          // 获取当前显示的图像
        cv::Mat currentImage;
        if (camera_manager_ && camera_manager_->isCameraAvailable(CameraType::HD_CAMERA)) {
            ImageData imageData = camera_manager_->getLatestImage(CameraType::HD_CAMERA);
            currentImage = imageData.image;
        }
        
        // 保存原始图像
        if (!currentImage.empty()) {
            record.imagePath = detection_manager_->saveImage(currentImage, "pcb_original");
        }
        
        // 保存标注图像
        if (!current_result_.annotatedImage.empty()) {
            record.annotatedImagePath = detection_manager_->saveAnnotatedImage(
                current_result_.annotatedImage, 
                current_result_.componentLabels,
                component_names_,
                component_colors_,
                "pcb_annotated"
            );
        }
        
        // 保存到数据库
        if (detection_manager_->saveDetectionRecord(record)) {
            addLogMessage(QString("检测结果已保存到数据库 (ID: %1)").arg(record.id));
            
            // 同时保存传统格式的图像文件（用户友好）
            QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
            QString filename = QString("PCB_分析结果_%1.jpg").arg(timestamp);
            QString filepath = defaultPath + "/" + filename;
            
            if (cv::imwrite(filepath.toStdString(), current_result_.annotatedImage)) {
                addLogMessage(QString("分析结果图像已保存: %1").arg(filepath));
            }
        } else {
            addLogMessage("保存检测结果到数据库失败");
        }
        
    } catch (const std::exception& e) {
        addLogMessage(QString("保存结果时发生异常: %1").arg(e.what()));
    }
}

void RealtimePCBAnalyzerWidget::clearResults()
{
    log_text_->clear();
    total_analyses_ = 0;
    successful_analyses_ = 0;
    total_processing_time_ = 0.0;
    session_start_time_ = QDateTime::currentDateTime();
    updateAnalysisStatistics();
    addLogMessage("日志和统计已清空");
}

void RealtimePCBAnalyzerWidget::onAutoSaveResult(const PCBAnalysisResult& result)
{
    if (!auto_save_enabled_ || !result.isValid) {
        return;
    }
    
    try {
        // 创建检测记录
        PCBDetectionRecord record;
        record.id = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
        record.timestamp = result.timestamp;        record.pcbModel = QString::fromStdString(result.pcbIdentResult.modelName.toStdString());
        record.pcbConfidence = result.pcbIdentResult.confidence;
        record.componentLabels = result.componentLabels;
        record.totalComponents = result.totalComponents;
        record.componentCounts = result.componentCounts;
        record.analysisTime = result.analysisTime;
        record.notes = "自动保存";
        record.isValid = true;
          // 获取当前图像
        cv::Mat currentImage;
        if (camera_manager_ && camera_manager_->isCameraAvailable(CameraType::HD_CAMERA)) {
            ImageData imageData = camera_manager_->getLatestImage(CameraType::HD_CAMERA);
            currentImage = imageData.image;
        }
        
        // 保存图像
        if (!currentImage.empty()) {
            record.imagePath = detection_manager_->saveImage(currentImage, "auto_pcb_original");
        }
        
        if (!result.annotatedImage.empty()) {
            record.annotatedImagePath = detection_manager_->saveAnnotatedImage(
                result.annotatedImage, 
                result.componentLabels,
                component_names_,
                component_colors_,
                "auto_pcb_annotated"
            );
        }
        
        // 保存到数据库
        if (detection_manager_->saveDetectionRecord(record)) {
            addLogMessage(QString("检测结果已自动保存 (ID: %1)").arg(record.id));
        }
        
    } catch (const std::exception& e) {
        addLogMessage(QString("自动保存时发生异常: %1").arg(e.what()));
    }
}

void RealtimePCBAnalyzerWidget::exportResults()
{
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    
    QStringList options;
    options << "JSON格式 (*.json)" << "CSV格式 (*.csv)";
    
    bool ok;
    QString selectedFormat = QInputDialog::getItem(this, "选择导出格式", 
                                                  "请选择要导出的数据格式:", 
                                                  options, 0, false, &ok);
    
    if (!ok) {
        return;
    }
    
    QString filename, filter;
    if (selectedFormat.startsWith("JSON")) {
        filename = QString("PCB_检测数据_%1.json").arg(timestamp);
        filter = "JSON Files (*.json)";
    } else {
        filename = QString("PCB_检测数据_%1.csv").arg(timestamp);
        filter = "CSV Files (*.csv)";
    }
    
    QString filepath = QFileDialog::getSaveFileName(this, "导出检测数据", 
                                                   defaultPath + "/" + filename,
                                                   filter);
    
    if (filepath.isEmpty()) {
        return;
    }
    
    try {
        bool success = false;
        if (selectedFormat.startsWith("JSON")) {
            success = detection_manager_->exportToJson(filepath);
        } else {
            success = detection_manager_->exportToCSV(filepath);
        }
        
        if (success) {
            addLogMessage(QString("检测数据已导出: %1").arg(filepath));
            QMessageBox::information(this, "导出成功", 
                                   QString("检测数据已成功导出到:\n%1").arg(filepath));
        } else {
            addLogMessage("导出检测数据失败");
            QMessageBox::warning(this, "导出失败", "导出检测数据失败，请检查文件路径和权限");
        }
        
    } catch (const std::exception& e) {
        QString error = QString("导出数据时发生异常: %1").arg(e.what());
        addLogMessage(error);
        QMessageBox::critical(this, "导出错误", error);
    }
}

void RealtimePCBAnalyzerWidget::deleteOldRecords()
{
    bool ok;
    int keepDays = QInputDialog::getInt(this, "清理旧数据", 
                                       "请输入要保留的天数（超过此天数的数据将被删除）:",
                                       30, 1, 365, 1, &ok);
    
    if (!ok) {
        return;
    }
    
    int reply = QMessageBox::question(this, "确认清理", 
                                     QString("确定要删除 %1 天前的检测数据吗？\n"
                                            "此操作不可撤销！").arg(keepDays),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    try {
        if (detection_manager_->cleanupOldRecords(keepDays)) {
            addLogMessage(QString("已清理 %1 天前的旧检测数据").arg(keepDays));
            QMessageBox::information(this, "清理完成", 
                                   QString("已成功清理 %1 天前的旧检测数据").arg(keepDays));
        } else {
            addLogMessage("清理旧数据失败");
            QMessageBox::warning(this, "清理失败", "清理旧数据失败");
        }
        
    } catch (const std::exception& e) {
        QString error = QString("清理数据时发生异常: %1").arg(e.what());
        addLogMessage(error);
        QMessageBox::critical(this, "清理错误", error);
    }
}

// 事件处理槽函数实现
void RealtimePCBAnalyzerWidget::onAnalysisIntervalChanged(int intervalMs)
{
    if (is_running_) {
        analysis_timer_->setInterval(intervalMs);
        addLogMessage(QString("分析间隔已更新为 %1 ms").arg(intervalMs));
    }
}

void RealtimePCBAnalyzerWidget::onPCBThresholdChanged(double threshold)
{
    if (pcb_identifier_) {
        // 这里应该设置PCB识别器的阈值
        // pcb_identifier_->setThreshold(threshold);
        addLogMessage(QString("PCB识别阈值已更新为 %1").arg(threshold));
    }
}

void RealtimePCBAnalyzerWidget::onComponentConfidenceChanged(double confidence)
{
    if (yolo_model_) {
        // 这里应该设置YOLO模型的置信度阈值
        // yolo_model_->setConfidenceThreshold(confidence);
        addLogMessage(QString("元件检测置信度已更新为 %1").arg(confidence));
    }
}

void RealtimePCBAnalyzerWidget::onAnalysisModeChanged(int mode)
{
    analysis_mode_ = static_cast<AnalysisMode>(mode);
    QString modeText;
    switch (analysis_mode_) {
        case PCB_IDENTIFICATION_ONLY:
            modeText = "仅PCB识别";
            break;
        case COMPONENT_DETECTION_ONLY:
            modeText = "仅元件检测";
            break;
        case COMPREHENSIVE_ANALYSIS:
            modeText = "综合分析";
            break;
    }
    addLogMessage(QString("分析模式已切换为: %1").arg(modeText));
}

void RealtimePCBAnalyzerWidget::onShowAnnotationsToggled(bool enabled)
{
    show_annotations_ = enabled;
    addLogMessage(QString("标注显示已%1").arg(enabled ? "启用" : "禁用"));
    updateImageDisplay();
}

void RealtimePCBAnalyzerWidget::onAutoSaveToggled(bool enabled)
{
    auto_save_enabled_ = enabled;
    addLogMessage(QString("自动保存已%1").arg(enabled ? "启用" : "禁用"));
}

void RealtimePCBAnalyzerWidget::onAnalysisStarted()
{
    addLogMessage("实时分析已启动");
    updateStatus("分析中");
}

void RealtimePCBAnalyzerWidget::onAnalysisStopped()
{
    addLogMessage("实时分析已停止");
    updateStatus("已停止");
}

void RealtimePCBAnalyzerWidget::onErrorOccurred(const QString& error)
{
    addLogMessage(QString("错误: %1").arg(error));
    updateStatus("错误");
}

void RealtimePCBAnalyzerWidget::onComponentTypeToggled(int componentType, bool enabled)
{
    if (componentType >= 0 && componentType < static_cast<int>(enabled_components_.size())) {
        enabled_components_[componentType] = enabled ? 1 : 0;
        
        if (yolo_model_) {
            yolo_model_->set_class_display(enabled_components_);
        }
        
        QString componentName = QString::fromStdString(component_names_[componentType]);
        addLogMessage(QString("元件类型 '%1' 已%2").arg(componentName).arg(enabled ? "启用" : "禁用"));
    }
}

void RealtimePCBAnalyzerWidget::selectAllComponents()
{
    for (auto* checkbox : component_checkboxes_) {
        checkbox->setChecked(true);
    }
    
    std::fill(enabled_components_.begin(), enabled_components_.end(), 1);
    
    if (yolo_model_) {
        yolo_model_->set_class_display(enabled_components_);
    }
    
    addLogMessage("已启用所有元件类型检测");
}

void RealtimePCBAnalyzerWidget::deselectAllComponents()
{
    for (auto* checkbox : component_checkboxes_) {
        checkbox->setChecked(false);
    }
    
    std::fill(enabled_components_.begin(), enabled_components_.end(), 0);
    
    if (yolo_model_) {
        yolo_model_->set_class_display(enabled_components_);
    }
    
    addLogMessage("已禁用所有元件类型检测");
}

void RealtimePCBAnalyzerWidget::onDisplayModeChanged()
{
    QString mode = display_mode_combo_->currentText();
    display_mode_ = mode;
    addLogMessage(QString("显示模式已切换为: %1").arg(mode));
    updateImageDisplay();
}

void RealtimePCBAnalyzerWidget::onZoomChanged(int value)
{
    zoom_level_ = value;
    addLogMessage(QString("缩放比例已调整为: %1%").arg(value));
    updateImageDisplay();
}

void RealtimePCBAnalyzerWidget::onTabChanged(int index)
{
    Q_UNUSED(index)
    // 标签页切换时的处理逻辑
    // 可以根据需要添加特定的处理
}

void RealtimePCBAnalyzerWidget::addLogMessage(const QString& message)
{
    if (!log_text_) {
        return;
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss] ");
    log_text_->append(timestamp + message);
    
    // 自动滚动到底部
    QTextCursor cursor = log_text_->textCursor();
    cursor.movePosition(QTextCursor::End);
    log_text_->setTextCursor(cursor);
}

void RealtimePCBAnalyzerWidget::updateStatus(const QString& status)
{
    if (!status_label_) {
        return;
    }
    
    status_label_->setText(QString("状态: %1 | 运行时间: %2秒")
                          .arg(status)
                          .arg(session_start_time_.secsTo(QDateTime::currentDateTime())));
}

void RealtimePCBAnalyzerWidget::updateAnalysisStatistics()
{
    if (!total_analysis_label_ || !success_rate_label_ || !average_processing_time_label_) {
        return;
    }
    
    total_analysis_label_->setText(QString("总分析次数: %1").arg(total_analyses_));
    
    double success_rate = total_analyses_ > 0 ? 
                         (successful_analyses_ * 100.0 / total_analyses_) : 0.0;
    success_rate_label_->setText(QString("成功率: %1%").arg(success_rate, 0, 'f', 1));
    
    double avg_time = successful_analyses_ > 0 ? 
                     (total_processing_time_ / successful_analyses_) : 0.0;
    average_processing_time_label_->setText(QString("平均处理时间: %1 ms").arg(avg_time, 0, 'f', 1));
}
