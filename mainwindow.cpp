#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QHeaderView>
#include <QSplitter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFormLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QThread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , device_manager_(nullptr)
    , fault_diagnostic_(nullptr)
    , sequence_manager_(nullptr)
    , result_exporter_(nullptr)      
    , pcb_identifier_(nullptr)    
    , pcb_dialog_(nullptr)
    , camera_control_(nullptr)
    , pcb_analyzer_(nullptr)
    , history_widget_(nullptr)
    , detection_manager_(nullptr)
    , board_management_widget_(nullptr)
    , board_manager_(nullptr)
    , status_timer_(new QTimer(this))
    , wiring_resource_manager_(nullptr)
    , current_wiring_dialog_(nullptr)
    , task_generator_(nullptr)
    , current_test_index_(0)
    , batch_testing_active_(false)
{
    ui->setupUi(this);
    
    // 初始化核心组件
    device_manager_ = new DeviceManager(this);
    fault_diagnostic_ = new FaultDiagnostic(device_manager_, this);
    sequence_manager_ = new TestSequenceManager(this);
    result_exporter_ = new ResultExporter(this);
    pcb_identifier_ = new PCBIdentifier(this);
      // 初始化PCB识别对话框
    pcb_dialog_ = new PCBIdentificationDialog(this);
    pcb_dialog_->setPCBIdentifier(pcb_identifier_);    // 初始化相机控制组件 - 作为独立窗口
    camera_control_ = new CameraControlWidget();
      // 初始化PCB综合分析器 - 作为独立窗口
    pcb_analyzer_ = new RealtimePCBAnalyzerWidget();
    pcb_analyzer_->setPCBIdentifier(pcb_identifier_);
    pcb_analyzer_->setCameraManager(camera_control_->getCameraManager());
    
    // 新增：初始化接线引导资源管理器
    wiring_resource_manager_ = new WiringResourceManager(this);
    if (!wiring_resource_manager_->initializeResources()) {
        QMessageBox::warning(this, "警告", "接线引导资源初始化失败");
    }
    
    // 初始化任务生成器
    task_generator_ = new WiringTaskGenerator(this);
    connect(task_generator_, &WiringTaskGenerator::taskGenerated,
            [this](const TestTask& task) {
                qDebug() << "测试任务已生成:" << task.task_id;
                statusBar()->showMessage(QString("测试任务已生成: %1").arg(task.task_id), 3000);
            });
    connect(task_generator_, &WiringTaskGenerator::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    
    // 初始化检测数据管理器
    detection_manager_ = new PCBDetectionManager(this);
    if (!detection_manager_->initializeDatabase()) {
        qWarning() << "PCB检测数据库初始化失败";
    }
    
    // 初始化PCB板卡管理器
    board_manager_ = new PCBBoardManager(this);
    if (!board_manager_->initializeDatabase()) {
        qWarning() << "PCB板卡数据库初始化失败";
    }
    
    // 初始化PCB板卡管理窗口 - 作为独立窗口
    board_management_widget_ = new PCBBoardManagementWidget();
    board_management_widget_->setBoardManager(board_manager_);
    board_management_widget_->setCameraManager(camera_control_->getCameraManager());
    
    // 设置UI
    setupUI();
    
    // 连接信号槽
    connect(device_manager_, &DeviceManager::deviceStatusChanged,
            this, &MainWindow::onDeviceStatusChanged);
    connect(device_manager_, &DeviceManager::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    
    connect(fault_diagnostic_, &FaultDiagnostic::diagnosticCompleted,
            this, &MainWindow::onDiagnosticCompleted);
    connect(fault_diagnostic_, &FaultDiagnostic::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    // 新增：连接接线引导信号
    connect(fault_diagnostic_, &FaultDiagnostic::wiringRequired,
            this, &MainWindow::showWiringGuideForComponent);
    
    // 测试序列管理器信号
    connect(sequence_manager_, &TestSequenceManager::sequenceLoaded,
            this, [this](const TestSequence& sequence) {
                current_sequence_ = sequence;
                populateComponentTable();
                statusBar()->showMessage(QString("Loaded test sequence: %1").arg(sequence.name), 3000);
            });
    connect(sequence_manager_, &TestSequenceManager::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    
    // 结果导出器信号
    connect(result_exporter_, &ResultExporter::exportCompleted,
            this, [this](const QString& filePath) {
                QMessageBox::information(this, "Export Complete", 
                    QString("Results exported successfully to:\n%1").arg(filePath));
            });
    connect(result_exporter_, &ResultExporter::exportFailed,
            this, &MainWindow::onErrorOccurred);
    
    // 状态更新定时器
    connect(status_timer_, &QTimer::timeout, this, &MainWindow::updateSystemStatus);
    status_timer_->start(1000); // 每秒更新一次状态
    
    setWindowTitle("PCB元件故障诊断系统 v1.0");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    // 停止状态定时器
    if (status_timer_) {
        status_timer_->stop();
    }
    
    // 停止批量测试
    if (batch_testing_active_) {
        batch_testing_active_ = false;
    }
    
    // 清理PCB分析器 - 需要先断开信号连接
    if (pcb_analyzer_) {
        disconnect(pcb_analyzer_, nullptr, this, nullptr);
        pcb_analyzer_->close();
        pcb_analyzer_->deleteLater();
        pcb_analyzer_ = nullptr;
    }
      // 清理相机控制组件
    if (camera_control_) {
        disconnect(camera_control_, nullptr, this, nullptr);
        camera_control_->close();
        camera_control_->deleteLater();
        camera_control_ = nullptr;
    }
    
    // 清理PCB板卡管理窗口
    if (board_management_widget_) {
        disconnect(board_management_widget_, nullptr, this, nullptr);
        board_management_widget_->close();
        board_management_widget_->deleteLater();
        board_management_widget_ = nullptr;
    }
      // 清理PCB识别对话框
    if (pcb_dialog_) {
        disconnect(pcb_dialog_, nullptr, this, nullptr);
        pcb_dialog_->close();
        pcb_dialog_->deleteLater();
        pcb_dialog_ = nullptr;
    }
    
    // 清理检测历史管理窗口
    if (history_widget_) {
        disconnect(history_widget_, nullptr, this, nullptr);
        history_widget_->close();
        history_widget_->deleteLater();
        history_widget_ = nullptr;
    }
    
    // 关闭设备管理器
    if (device_manager_) {
        disconnect(device_manager_, nullptr, this, nullptr);
        device_manager_->shutdownDeviceThreads();
    }
    
    // 断开故障诊断的信号连接
    if (fault_diagnostic_) {
        disconnect(fault_diagnostic_, nullptr, this, nullptr);
    }
    
    // 其他组件的信号断开
    if (sequence_manager_) {
        disconnect(sequence_manager_, nullptr, this, nullptr);
    }
    
    if (result_exporter_) {
        disconnect(result_exporter_, nullptr, this, nullptr);
    }
    
    // 强制处理所有待处理的事件
    QApplication::processEvents();
    
    delete ui;
}

void MainWindow::setupUI()
{
    // 创建主标签页
    main_tabs_ = new QTabWidget(this);
    setCentralWidget(main_tabs_);
    
    setupDeviceStatusPage();
    setupSingleTestPage();
    setupBatchTestPage();
    setupResultsPage();
    setupStatusBar();
    setupMenuBar();
}

void MainWindow::setupDeviceStatusPage()
{
    device_status_page_ = new QWidget();
    main_tabs_->addTab(device_status_page_, "设备状态");
    
    QVBoxLayout* layout = new QVBoxLayout(device_status_page_);
    
    // 设备状态显示
    QGroupBox* status_group = new QGroupBox("设备状态");
    QGridLayout* status_layout = new QGridLayout(status_group);
      status_layout->addWidget(new QLabel("JY5711 (AO):"), 0, 0);
    ao_status_label_ = new QLabel("❌ 未连接");
    status_layout->addWidget(ao_status_label_, 0, 1);
    
    status_layout->addWidget(new QLabel("JY5322 (DAQ):"), 1, 0);
    daq_5322_status_label_ = new QLabel("❌ 未连接");
    status_layout->addWidget(daq_5322_status_label_, 1, 1);
    
    status_layout->addWidget(new QLabel("JY5323 (DAQ):"), 2, 0);
    daq_5323_status_label_ = new QLabel("❌ 未连接");
    status_layout->addWidget(daq_5323_status_label_, 2, 1);
    
    status_layout->addWidget(new QLabel("JY8902 (DMM):"), 3, 0);
    dmm_status_label_ = new QLabel("❌ 未连接");    status_layout->addWidget(dmm_status_label_, 3, 1);
    
    layout->addWidget(status_group);
    
    // 控制按钮
    QGroupBox* control_group = new QGroupBox("系统控制");
    QHBoxLayout* control_layout = new QHBoxLayout(control_group);
    
    init_button_ = new QPushButton("初始化系统");
    shutdown_button_ = new QPushButton("关闭系统");
    shutdown_button_->setEnabled(false);
    
    control_layout->addWidget(init_button_);
    control_layout->addWidget(shutdown_button_);
    control_layout->addStretch();
    
    layout->addWidget(control_group);
    layout->addStretch();
    
    // 连接信号
    connect(init_button_, &QPushButton::clicked, this, &MainWindow::initializeSystem);
    connect(shutdown_button_, &QPushButton::clicked, this, &MainWindow::shutdownSystem);
}

void MainWindow::setupSingleTestPage()
{
    single_test_page_ = new QWidget();
    main_tabs_->addTab(single_test_page_, "单元件测试");
    
    QHBoxLayout* main_layout = new QHBoxLayout(single_test_page_);
    
    // 左侧参数设置
    QGroupBox* param_group = new QGroupBox("测试参数");
    param_group->setFixedWidth(300);
    QFormLayout* param_layout = new QFormLayout(param_group);
    
    component_type_combo_ = new QComboBox();
    component_type_combo_->addItems({"电阻", "电容", "电感", "二极管", "集成电路"});
    param_layout->addRow("元件类型:", component_type_combo_);
    
    component_ref_edit_ = new QLineEdit();
    component_ref_edit_->setPlaceholderText("如: R1, C2, IC3");
    param_layout->addRow("元件标识:", component_ref_edit_);
    
    nominal_value_spin_ = new QDoubleSpinBox();
    nominal_value_spin_->setRange(0.001, 1000000);
    nominal_value_spin_->setDecimals(6);
    nominal_value_spin_->setSuffix(" Ω/F/H");
    param_layout->addRow("标称值:", nominal_value_spin_);
    
    tolerance_spin_ = new QDoubleSpinBox();
    tolerance_spin_->setRange(0.1, 50.0);
    tolerance_spin_->setValue(5.0);
    tolerance_spin_->setSuffix(" %");
    param_layout->addRow("容差:", tolerance_spin_);
    
    channel_spin_ = new QSpinBox();
    channel_spin_->setRange(0, 31);
    param_layout->addRow("测试通道:", channel_spin_);
    
    single_test_button_ = new QPushButton("开始测试");
    single_test_button_->setEnabled(false);
    param_layout->addRow(single_test_button_);
    
    main_layout->addWidget(param_group);
    
    // 右侧结果显示
    QGroupBox* result_group = new QGroupBox("测试结果");
    QVBoxLayout* result_layout = new QVBoxLayout(result_group);
    
    single_result_text_ = new QTextEdit();
    single_result_text_->setReadOnly(true);
    result_layout->addWidget(single_result_text_);
    
    main_layout->addWidget(result_group);
    
    connect(single_test_button_, &QPushButton::clicked, this, &MainWindow::startSingleTest);
}

void MainWindow::setupBatchTestPage()
{
    batch_test_page_ = new QWidget();
    main_tabs_->addTab(batch_test_page_, "批量测试");
    
    QVBoxLayout* layout = new QVBoxLayout(batch_test_page_);
    
    // 工具栏
    QHBoxLayout* toolbar_layout = new QHBoxLayout();
    
    add_component_button_ = new QPushButton("添加元件");
    remove_component_button_ = new QPushButton("删除元件");
    load_sequence_button_ = new QPushButton("加载序列");
    save_sequence_button_ = new QPushButton("保存序列");
    batch_test_button_ = new QPushButton("开始批量测试");
    batch_test_button_->setEnabled(false);
    
    toolbar_layout->addWidget(add_component_button_);
    toolbar_layout->addWidget(remove_component_button_);
    toolbar_layout->addWidget(load_sequence_button_);
    toolbar_layout->addWidget(save_sequence_button_);
    toolbar_layout->addStretch();
    toolbar_layout->addWidget(batch_test_button_);
    
    layout->addLayout(toolbar_layout);
      // 元件列表表格
    component_table_ = new QTableWidget();
    component_table_->setColumnCount(5);
    QStringList headers = {"测试名称", "组件类型", "标称值", "状态", "超时"};
    component_table_->setHorizontalHeaderLabels(headers);
    component_table_->horizontalHeader()->setStretchLastSection(true);
    
    layout->addWidget(component_table_);
    
    // 进度条
    test_progress_ = new QProgressBar();
    test_progress_->setVisible(false);
    layout->addWidget(test_progress_);
    
    // 连接信号
    connect(add_component_button_, &QPushButton::clicked, this, &MainWindow::addComponent);
    connect(remove_component_button_, &QPushButton::clicked, this, &MainWindow::removeComponent);
    connect(load_sequence_button_, &QPushButton::clicked, this, &MainWindow::loadTestSequence);
    connect(save_sequence_button_, &QPushButton::clicked, this, &MainWindow::saveTestSequence);
    connect(batch_test_button_, &QPushButton::clicked, this, &MainWindow::startBatchTest);
}

void MainWindow::setupResultsPage()
{
    results_page_ = new QWidget();
    main_tabs_->addTab(results_page_, "测试结果");
    
    QVBoxLayout* layout = new QVBoxLayout(results_page_);
    
    // 工具栏
    QHBoxLayout* toolbar_layout = new QHBoxLayout();
    
    export_button_ = new QPushButton("导出结果");
    clear_button_ = new QPushButton("清除结果");
    
    toolbar_layout->addWidget(export_button_);
    toolbar_layout->addWidget(clear_button_);
    toolbar_layout->addStretch();
    
    layout->addLayout(toolbar_layout);
    
    // 使用分割器
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
      // 结果表格
    results_table_ = new QTableWidget();
    results_table_->setColumnCount(8);
    QStringList headers = {"测试ID", "组件类型", "组件ID", "测试结果", "健康度", "置信度", "故障类型", "时间戳"};
    results_table_->setHorizontalHeaderLabels(headers);
    results_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_table_->horizontalHeader()->setStretchLastSection(true);
    
    splitter->addWidget(results_table_);
    
    // 详细信息
    detail_text_ = new QTextEdit();
    detail_text_->setReadOnly(true);
    detail_text_->setMaximumWidth(400);
    
    splitter->addWidget(detail_text_);
    splitter->setSizes({800, 400});
    
    layout->addWidget(splitter);
      // 连接信号
    connect(export_button_, &QPushButton::clicked, this, &MainWindow::exportResults);
    connect(clear_button_, &QPushButton::clicked, this, &MainWindow::clearResults);
    connect(results_table_, &QTableWidget::currentCellChanged, this, &MainWindow::updateDetailText);
}

void MainWindow::setupStatusBar()
{
    system_status_label_ = new QLabel("系统未就绪");
    test_count_label_ = new QLabel("测试: 0/0");
    status_progress_ = new QProgressBar();
    status_progress_->setVisible(false);
    status_progress_->setMaximumWidth(200);
    
    statusBar()->addWidget(system_status_label_);
    statusBar()->addPermanentWidget(test_count_label_);
    statusBar()->addPermanentWidget(status_progress_);
}

void MainWindow::setupMenuBar()
{
    QMenuBar* menu_bar = menuBar();
    
    // 文件菜单
    QMenu* file_menu = menu_bar->addMenu("文件");
    file_menu->addAction("加载测试序列", this, &MainWindow::loadTestSequence);
    file_menu->addAction("保存测试序列", this, &MainWindow::saveTestSequence);
    file_menu->addSeparator();
    file_menu->addAction("导出结果", this, &MainWindow::exportResults);
    file_menu->addSeparator();
    file_menu->addAction("退出", this, &QWidget::close);
      // 系统菜单
    QMenu* system_menu = menu_bar->addMenu("系统");
    system_menu->addAction("初始化设备", this, &MainWindow::initializeSystem);
    system_menu->addAction("关闭设备", this, &MainWindow::shutdownSystem);
    system_menu->addSeparator();
    system_menu->addAction("接线引导", this, &MainWindow::startWiringGuide);
    
    // 工具菜单
    QMenu* tools_menu = menu_bar->addMenu("工具");
    tools_menu->addAction("PCB板卡识别", this, &MainWindow::openPCBIdentification);
    tools_menu->addAction("相机控制", this, &MainWindow::openCameraControl);
    tools_menu->addAction("PCB综合分析器", this, &MainWindow::openPCBAnalyzer);
    tools_menu->addAction("检测历史管理", this, &MainWindow::openDetectionHistory);
    tools_menu->addAction("PCB板卡管理", this, &MainWindow::openBoardManagement);
    tools_menu->addAction("PCB综合分析", this, &MainWindow::openPCBAnalyzer);
    tools_menu->addSeparator();
    tools_menu->addAction("检测历史管理", this, &MainWindow::openDetectionHistory);
    
    // 帮助菜单
    QMenu* help_menu = menu_bar->addMenu("帮助");
    help_menu->addAction("关于", [this]() {
        QMessageBox::about(this, "关于", 
            "PCB元件故障诊断系统 v1.0\n\n"
            "基于JYTEK设备的自动化测试系统\n"
            "支持设备: JY5711, JY5320, JY8902\n\n"
            "Copyright © 2025");
    });
}

void MainWindow::initializeSystem()
{
    init_button_->setEnabled(false);
    init_button_->setText("初始化中...");
    
    QApplication::processEvents();
      bool success = device_manager_->initializeDeviceThreads();
    QString errorMessage;
    if (!success) {
        errorMessage = device_manager_->getLastError();
    }
    
    if (success) {
        init_button_->setText("初始化系统");
        shutdown_button_->setEnabled(true);
        single_test_button_->setEnabled(true);
        batch_test_button_->setEnabled(true);
        system_status_label_->setText("系统就绪");
        
        QMessageBox::information(this, "成功", "设备初始化成功！");
    } else {
        init_button_->setEnabled(true);
        init_button_->setText("初始化系统");
        QMessageBox::critical(this, "错误", "设备初始化失败！\n" + errorMessage);
    }
}

void MainWindow::shutdownSystem()
{
    device_manager_->shutdownDeviceThreads();
    
    init_button_->setEnabled(true);
    shutdown_button_->setEnabled(false);
    single_test_button_->setEnabled(false);
    batch_test_button_->setEnabled(false);
    system_status_label_->setText("系统未就绪");
    
    updateDeviceStatus();
}

void MainWindow::startSingleTest()
{
    ComponentSpec component = createComponentFromUI();
    
    // 检查系统是否就绪
    if (!device_manager_ || !device_manager_->isSystemReady()) {
        QMessageBox::warning(this, "系统未就绪", "请先初始化测试设备！");
        return;
    }
    
    // 设置测试参数
    component.test_voltage = 3.3;  // 默认3.3V测试电压
    component.test_current = 0.01; // 默认10mA测试电流
    component.requires_dmm = true; // 默认使用万用表
    
    single_test_button_->setEnabled(false);
    single_test_button_->setText("接线引导中...");
    single_result_text_->clear();
    single_result_text_->append("开始为 " + component.reference + " 进行接线引导...\n");
    
    QApplication::processEvents();
    
    // 显示接线引导对话框
    showWiringGuideForComponent(component);
}

void MainWindow::startBatchTest()
{
    if (current_sequence_.steps.isEmpty()) {
        // 如果没有测试序列，创建一个默认序列
        if (!sequence_manager_->createDefaultSequence(current_sequence_)) {
            QMessageBox::warning(this, "错误", "无法创建默认测试序列！");
            return;
        }
        populateComponentTable();
    }
    
    // Check if system is ready
    bool systemReady = device_manager_ ? device_manager_->isSystemReady() : false;
    
    if (!systemReady) {
        QMessageBox::warning(this, "系统未就绪", "请先初始化测试设备！");
        return;
    }
    
    batch_testing_active_ = true;
    current_test_index_ = 0;
    
    batch_test_button_->setText("停止测试");
    batch_test_button_->setEnabled(true);
    test_progress_->setVisible(true);
    test_progress_->setMaximum(current_sequence_.steps.size());
    test_progress_->setValue(0);
    
    test_results_.clear();
    updateResultsTable();
    
    // 开始第一个测试
    runNextTest();
}

void MainWindow::runNextTest()
{
    if (!batch_testing_active_ || current_test_index_ >= current_sequence_.steps.size()) {
        // 测试完成
        finishBatchTest();
        return;
    }
    
    const TestStep& step = current_sequence_.steps[current_test_index_];
    if (!step.enabled) {
        // 跳过禁用的测试
        current_test_index_++;
        test_progress_->setValue(current_test_index_);
        QTimer::singleShot(100, this, &MainWindow::runNextTest);
        return;
    }
    
    // 创建组件规格
    ComponentSpecs specs = step.specs;
    
    // 启动诊断
    fault_diagnostic_->diagnoseComponentAsync(step.componentType, 
                                             QString("Step_%1").arg(current_test_index_ + 1), 
                                             specs);
    
    test_count_label_->setText(QString("正在测试: %1 (%2/%3)")
                              .arg(step.testName)
                              .arg(current_test_index_ + 1)
                              .arg(current_sequence_.steps.size()));
}

void MainWindow::finishBatchTest()
{
    batch_testing_active_ = false;
    batch_test_button_->setText("开始批量测试");
    batch_test_button_->setEnabled(true);
    test_progress_->setVisible(false);
    
    // 统计结果
    int passed = 0, failed = 0, errors = 0;
    for (const auto& result : test_results_) {
        switch (result.result) {
            case DiagnosticResult::PASS: passed++; break;
            case DiagnosticResult::FAIL: failed++; break;
            case DiagnosticResult::ERROR: errors++; break;
        }
    }
    
    QString summary = QString("批量测试完成！\n通过: %1\n失败: %2\n错误: %3")
                     .arg(passed).arg(failed).arg(errors);
    
    test_count_label_->setText(QString("完成 %1 个测试").arg(test_results_.size()));
    updateResultsTable();
    main_tabs_->setCurrentWidget(results_page_);
    
    QMessageBox::information(this, "测试完成", summary);
}

void MainWindow::onDeviceStatusChanged(const QString& device, DeviceStatus status)
{
    Q_UNUSED(device)
    Q_UNUSED(status)
    updateDeviceStatus();
}

void MainWindow::onDiagnosticCompleted(const DiagnosticResult& result)
{
    // 添加结果到列表
    test_results_.append(result);
    
    // 在单元件测试时更新结果显示
    if (main_tabs_->currentWidget() == single_test_page_) {
        single_result_text_->append(formatResult(result));
        single_result_text_->append("\n测试完成。");
        
        // Re-enable the single test button
        single_test_button_->setEnabled(true);
        single_test_button_->setText("开始测试");
    }
    
    // 在批量测试时处理下一个测试
    if (batch_testing_active_) {
        current_test_index_++;
        test_progress_->setValue(current_test_index_);
        updateResultsTable();
        
        // 延迟启动下一个测试，给UI时间更新
        QTimer::singleShot(500, this, &MainWindow::runNextTest);
    }
}

void MainWindow::onErrorOccurred(const QString& error)
{
    QMessageBox::warning(this, "错误", error);
}

void MainWindow::addComponent()
{
    // 创建综合元件输入对话框
    QDialog dialog(this);
    dialog.setWindowTitle("添加测试元件");
    dialog.setModal(true);
    dialog.setMinimumSize(600, 500);
    
    QVBoxLayout* main_layout = new QVBoxLayout(&dialog);
    
    // 创建标签页控件
    QTabWidget* tabs = new QTabWidget();
    main_layout->addWidget(tabs);
    
    // 基本信息页面
    QWidget* basic_page = new QWidget();
    tabs->addTab(basic_page, "基本信息");
    
    QFormLayout* basic_layout = new QFormLayout(basic_page);
    
    // 元件类型选择
    QComboBox* type_combo = new QComboBox();
    type_combo->addItems({"电阻", "电容", "电感", "二极管", "集成电路"});
    basic_layout->addRow("元件类型:", type_combo);
    
    // 元件标识
    QLineEdit* ref_edit = new QLineEdit();
    ref_edit->setPlaceholderText("如: R1, C2, L3, D4, IC5");
    basic_layout->addRow("元件标识:", ref_edit);
    
    // 测试通道
    QSpinBox* channel_spin = new QSpinBox();
    channel_spin->setRange(0, 31);
    channel_spin->setValue(test_components_.size());
    basic_layout->addRow("测试通道:", channel_spin);
    
    // 描述信息
    QLineEdit* desc_edit = new QLineEdit();
    desc_edit->setPlaceholderText("可选的描述信息");
    basic_layout->addRow("描述:", desc_edit);
    
    // 电阻参数页面
    QWidget* resistor_page = new QWidget();
    tabs->addTab(resistor_page, "电阻参数");
    
    QFormLayout* resistor_layout = new QFormLayout(resistor_page);
    
    QDoubleSpinBox* resistance_spin = new QDoubleSpinBox();
    resistance_spin->setRange(0.001, 10000000.0);
    resistance_spin->setDecimals(6);
    resistance_spin->setValue(1000.0);
    resistance_spin->setSuffix(" Ω");
    resistor_layout->addRow("阻值:", resistance_spin);
    
    QDoubleSpinBox* resistor_tolerance_spin = new QDoubleSpinBox();
    resistor_tolerance_spin->setRange(0.1, 50.0);
    resistor_tolerance_spin->setValue(5.0);
    resistor_tolerance_spin->setSuffix(" %");
    resistor_layout->addRow("容差:", resistor_tolerance_spin);
    
    QDoubleSpinBox* temp_coeff_spin = new QDoubleSpinBox();
    temp_coeff_spin->setRange(-1000.0, 1000.0);
    temp_coeff_spin->setValue(0.0);
    temp_coeff_spin->setSuffix(" ppm/°C");
    resistor_layout->addRow("温度系数:", temp_coeff_spin);
    
    QDoubleSpinBox* max_power_spin = new QDoubleSpinBox();
    max_power_spin->setRange(0.001, 100.0);
    max_power_spin->setValue(0.25);
    max_power_spin->setSuffix(" W");
    resistor_layout->addRow("最大功率:", max_power_spin);
    
    // 电容参数页面
    QWidget* capacitor_page = new QWidget();
    tabs->addTab(capacitor_page, "电容参数");
    
    QFormLayout* capacitor_layout = new QFormLayout(capacitor_page);
    
    QDoubleSpinBox* capacitance_spin = new QDoubleSpinBox();
    capacitance_spin->setRange(1e-12, 1.0);
    capacitance_spin->setDecimals(12);
    capacitance_spin->setValue(100e-9);
    capacitance_spin->setSuffix(" F");
    capacitor_layout->addRow("容值:", capacitance_spin);
    
    QDoubleSpinBox* capacitor_tolerance_spin = new QDoubleSpinBox();
    capacitor_tolerance_spin->setRange(0.1, 50.0);
    capacitor_tolerance_spin->setValue(10.0);
    capacitor_tolerance_spin->setSuffix(" %");
    capacitor_layout->addRow("容差:", capacitor_tolerance_spin);
    
    QDoubleSpinBox* max_esr_spin = new QDoubleSpinBox();
    max_esr_spin->setRange(0.001, 1000.0);
    max_esr_spin->setValue(1.0);
    max_esr_spin->setSuffix(" Ω");
    capacitor_layout->addRow("最大ESR:", max_esr_spin);
    
    QDoubleSpinBox* max_leakage_spin = new QDoubleSpinBox();
    max_leakage_spin->setRange(1e-12, 1e-3);
    max_leakage_spin->setDecimals(12);
    max_leakage_spin->setValue(1e-6);
    max_leakage_spin->setSuffix(" A");
    capacitor_layout->addRow("最大漏电流:", max_leakage_spin);
    
    QDoubleSpinBox* cap_voltage_spin = new QDoubleSpinBox();
    cap_voltage_spin->setRange(1.0, 1000.0);
    cap_voltage_spin->setValue(50.0);
    cap_voltage_spin->setSuffix(" V");
    capacitor_layout->addRow("额定电压:", cap_voltage_spin);
    
    // 电感参数页面
    QWidget* inductor_page = new QWidget();
    tabs->addTab(inductor_page, "电感参数");
    
    QFormLayout* inductor_layout = new QFormLayout(inductor_page);
    
    QDoubleSpinBox* inductance_spin = new QDoubleSpinBox();
    inductance_spin->setRange(1e-9, 1.0);
    inductance_spin->setDecimals(12);
    inductance_spin->setValue(100e-6);
    inductance_spin->setSuffix(" H");
    inductor_layout->addRow("感值:", inductance_spin);
    
    QDoubleSpinBox* inductor_tolerance_spin = new QDoubleSpinBox();
    inductor_tolerance_spin->setRange(0.1, 50.0);
    inductor_tolerance_spin->setValue(20.0);
    inductor_tolerance_spin->setSuffix(" %");
    inductor_layout->addRow("容差:", inductor_tolerance_spin);
    
    QDoubleSpinBox* max_current_spin = new QDoubleSpinBox();
    max_current_spin->setRange(0.001, 100.0);
    max_current_spin->setValue(1.0);
    max_current_spin->setSuffix(" A");
    inductor_layout->addRow("最大电流:", max_current_spin);
    
    QDoubleSpinBox* dcr_spin = new QDoubleSpinBox();
    dcr_spin->setRange(0.001, 1000.0);
    dcr_spin->setValue(0.1);
    dcr_spin->setSuffix(" Ω");
    inductor_layout->addRow("直流电阻:", dcr_spin);
    
    // 二极管参数页面
    QWidget* diode_page = new QWidget();
    tabs->addTab(diode_page, "二极管参数");
    
    QFormLayout* diode_layout = new QFormLayout(diode_page);
    
    QDoubleSpinBox* forward_voltage_spin = new QDoubleSpinBox();
    forward_voltage_spin->setRange(0.1, 5.0);
    forward_voltage_spin->setDecimals(3);
    forward_voltage_spin->setValue(0.7);
    forward_voltage_spin->setSuffix(" V");
    diode_layout->addRow("正向压降:", forward_voltage_spin);
    
    QDoubleSpinBox* diode_tolerance_spin = new QDoubleSpinBox();
    diode_tolerance_spin->setRange(0.1, 50.0);
    diode_tolerance_spin->setValue(10.0);
    diode_tolerance_spin->setSuffix(" %");
    diode_layout->addRow("容差:", diode_tolerance_spin);
    
    QDoubleSpinBox* reverse_voltage_spin = new QDoubleSpinBox();
    reverse_voltage_spin->setRange(1.0, 1000.0);
    reverse_voltage_spin->setValue(50.0);
    reverse_voltage_spin->setSuffix(" V");
    diode_layout->addRow("反向耐压:", reverse_voltage_spin);
    
    QDoubleSpinBox* diode_leakage_spin = new QDoubleSpinBox();
    diode_leakage_spin->setRange(1e-12, 1e-3);
    diode_leakage_spin->setDecimals(12);
    diode_leakage_spin->setValue(1e-6);
    diode_leakage_spin->setSuffix(" A");
    diode_layout->addRow("最大漏电流:", diode_leakage_spin);
    
    // IC参数页面
    QWidget* ic_page = new QWidget();
    tabs->addTab(ic_page, "IC参数");
    
    QFormLayout* ic_layout = new QFormLayout(ic_page);
    
    QDoubleSpinBox* supply_voltage_spin = new QDoubleSpinBox();
    supply_voltage_spin->setRange(1.0, 50.0);
    supply_voltage_spin->setValue(5.0);
    supply_voltage_spin->setSuffix(" V");
    ic_layout->addRow("供电电压:", supply_voltage_spin);
    
    QDoubleSpinBox* ic_tolerance_spin = new QDoubleSpinBox();
    ic_tolerance_spin->setRange(0.1, 20.0);
    ic_tolerance_spin->setValue(5.0);
    ic_tolerance_spin->setSuffix(" %");
    ic_layout->addRow("容差:", ic_tolerance_spin);
    
    QDoubleSpinBox* ic_max_current_spin = new QDoubleSpinBox();
    ic_max_current_spin->setRange(0.001, 10.0);
    ic_max_current_spin->setValue(0.1);
    ic_max_current_spin->setSuffix(" A");
    ic_layout->addRow("最大电流:", ic_max_current_spin);
    
    QLineEdit* ic_type_edit = new QLineEdit();
    ic_type_edit->setPlaceholderText("如: Logic, Analog, Mixed-Signal");
    ic_layout->addRow("IC类型:", ic_type_edit);
    
    // 根据元件类型切换到相应页面
    auto switchToComponentPage = [=]() {
        int type_index = type_combo->currentIndex();
        tabs->setCurrentIndex(type_index + 1); // +1因为第一页是基本信息
    };
    
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), switchToComponentPage);
    
    // 自动生成元件标识
    auto generateReference = [=]() {
        int type_index = type_combo->currentIndex();
        QStringList prefixes = {"R", "C", "L", "D", "IC"};
        if (type_index >= 0 && type_index < prefixes.size()) {
            QString prefix = prefixes[type_index];
            int count = 0;
            for (const auto& comp : test_components_) {
                if (comp.reference.startsWith(prefix)) {
                    count++;
                }
            }
            ref_edit->setText(QString("%1%2").arg(prefix).arg(count + 1));
        }
    };
    
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), generateReference);
    
    // 初始化默认值
    generateReference();
    switchToComponentPage();
    
    // 对话框按钮
    QDialogButtonBox* button_box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    main_layout->addWidget(button_box);
    
    connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    // 执行对话框
    if (dialog.exec() == QDialog::Accepted) {
        ComponentSpec component;
        
        // 基本信息
        QString type_text = type_combo->currentText();
        if (type_text == "电阻") component.type = ComponentType::RESISTOR;
        else if (type_text == "电容") component.type = ComponentType::CAPACITOR;
        else if (type_text == "电感") component.type = ComponentType::INDUCTOR;
        else if (type_text == "二极管") component.type = ComponentType::DIODE;
        else if (type_text == "集成电路") component.type = ComponentType::IC;
        else component.type = ComponentType::RESISTOR;
        
        component.reference = ref_edit->text();
        component.channel = channel_spin->value();
        component.description = desc_edit->text();
        
        // 根据类型设置特定参数
        switch (component.type) {
            case ComponentType::RESISTOR:
                component.nominal_value = resistance_spin->value();
                component.tolerance = resistor_tolerance_spin->value() / 100.0;
                component.temp_coefficient = temp_coeff_spin->value();
                component.max_voltage = sqrt(max_power_spin->value() * resistance_spin->value()); // P = V²/R
                component.max_current = sqrt(max_power_spin->value() / resistance_spin->value()); // P = I²R
                break;
                
            case ComponentType::CAPACITOR:
                component.nominal_value = capacitance_spin->value();
                component.tolerance = capacitor_tolerance_spin->value() / 100.0;
                component.max_esr = max_esr_spin->value();
                component.max_leakage = max_leakage_spin->value();
                component.max_voltage = cap_voltage_spin->value();
                component.max_current = 0.1; // 默认最大电流
                break;
                
            case ComponentType::INDUCTOR:
                component.nominal_value = inductance_spin->value();
                component.tolerance = inductor_tolerance_spin->value() / 100.0;
                component.max_current = max_current_spin->value();
                component.max_voltage = 50.0; // 默认最大电压
                component.temp_coefficient = dcr_spin->value(); // 使用temp_coefficient字段存储DCR
                break;
                
            case ComponentType::DIODE:
                component.nominal_value = forward_voltage_spin->value();
                component.tolerance = diode_tolerance_spin->value() / 100.0;
                component.max_voltage = reverse_voltage_spin->value();
                component.max_leakage = diode_leakage_spin->value();
                component.max_current = 1.0; // 默认最大电流
                break;
                
            case ComponentType::IC:
                component.nominal_value = supply_voltage_spin->value();
                component.tolerance = ic_tolerance_spin->value() / 100.0;
                component.max_voltage = supply_voltage_spin->value();
                component.max_current = ic_max_current_spin->value();
                // 将IC类型存储在描述中
                if (!ic_type_edit->text().isEmpty()) {
                    component.description += QString(" [%1]").arg(ic_type_edit->text());
                }
                break;
                
            default:
                component.nominal_value = 1000.0;
                component.tolerance = 0.05;
                break;
        }
        
        // 验证输入
        if (component.reference.isEmpty()) {
            QMessageBox::warning(this, "输入错误", "请输入元件标识！");
            return;
        }
        
        // 检查元件标识是否重复
        for (const auto& existing : test_components_) {
            if (existing.reference == component.reference) {
                QMessageBox::warning(this, "输入错误", 
                    QString("元件标识 '%1' 已存在，请使用不同的标识！").arg(component.reference));
                return;
            }
        }
          // 添加到组件列表
        test_components_.append(component);
        
        // 转换为TestStep并添加到测试序列
        TestStep step = createTestStepFromComponent(component);
        sequence_manager_->addTestStep(current_sequence_, step);
        
        // 刷新表格显示
        populateComponentTable();
        
        // 显示成功消息
        QString type_name = type_combo->currentText();
        QMessageBox::information(this, "添加成功", 
            QString("已成功添加%1 '%2'").arg(type_name).arg(component.reference));
    }
}

void MainWindow::removeComponent()
{
    int row = component_table_->currentRow();
    
    // 验证选中行的有效性
    if (row < 0 || row >= current_sequence_.steps.size()) {
        QMessageBox::warning(this, "删除失败", "请先选择要删除的元件！");
        return;
    }
    
    // 获取要删除的元件信息
    const TestStep& stepToRemove = current_sequence_.steps[row];
    QString componentRef = stepToRemove.testName;
    
    // 确认删除操作
    int result = QMessageBox::question(this, "确认删除", 
        QString("确定要删除测试步骤 '%1' 吗？").arg(componentRef),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (result != QMessageBox::Yes) {
        return;
    }
    
    // 同时从两个数据结构中删除
    // 1. 从测试序列中删除
    sequence_manager_->removeTestStep(current_sequence_, row);
    
    // 2. 从组件列表中删除（如果存在对应的项）
    // 通过匹配reference找到对应的ComponentSpec
    for (int i = 0; i < test_components_.size(); ++i) {
        // 创建期望的测试名称来匹配
        QString expectedTestName;
        switch (test_components_[i].type) {
            case ComponentType::RESISTOR:
                expectedTestName = QString("电阻测试 - %1").arg(test_components_[i].reference);
                break;
            case ComponentType::CAPACITOR:
                expectedTestName = QString("电容测试 - %1").arg(test_components_[i].reference);
                break;
            case ComponentType::INDUCTOR:
                expectedTestName = QString("电感测试 - %1").arg(test_components_[i].reference);
                break;
            case ComponentType::DIODE:
                expectedTestName = QString("二极管测试 - %1").arg(test_components_[i].reference);
                break;
            case ComponentType::IC:
                expectedTestName = QString("集成电路测试 - %1").arg(test_components_[i].reference);
                break;            default:
                expectedTestName = QString("未知组件测试 - %1").arg(test_components_[i].reference);
                break;
        }
        
        if (expectedTestName == stepToRemove.testName) {
            test_components_.removeAt(i);
            break;
        }
    }
    
    // 刷新表格显示
    populateComponentTable();
    
    // 显示成功消息
    statusBar()->showMessage(QString("已删除测试步骤: %1").arg(componentRef), 3000);
}

void MainWindow::loadTestSequence()
{
    QString filename = QFileDialog::getOpenFileName(this, 
        "加载测试序列", 
        "", 
        "JSON文件 (*.json);;所有文件 (*.*)");
    
    if (!filename.isEmpty()) {
        TestSequence sequence;
        if (sequence_manager_->loadSequence(filename, sequence)) {
            current_sequence_ = sequence;
            populateComponentTable();
            statusBar()->showMessage(QString("成功加载测试序列: %1").arg(sequence.name), 5000);
        }
    }
}

void MainWindow::saveTestSequence()
{
    if (current_sequence_.steps.isEmpty()) {
        // 如果当前序列为空，创建一个默认序列
        sequence_manager_->createDefaultSequence(current_sequence_);
    }
    
    QString filename = QFileDialog::getSaveFileName(this, 
        "保存测试序列", 
        current_sequence_.name + ".json",
        "JSON文件 (*.json);;所有文件 (*.*)");
    
    if (!filename.isEmpty()) {
        if (sequence_manager_->saveSequence(filename, current_sequence_)) {
            statusBar()->showMessage(QString("成功保存测试序列到: %1").arg(filename), 5000);
        }
    }
}

void MainWindow::exportResults()
{
    if (test_results_.isEmpty()) {
        QMessageBox::information(this, "提示", "没有测试结果可导出");
        return;
    }
    
    QString filename = QFileDialog::getSaveFileName(this, 
        "导出测试结果", 
        QString("test_results_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "CSV文件 (*.csv);;JSON文件 (*.json);;HTML报告 (*.html);;所有文件 (*.*)");
    
    if (!filename.isEmpty()) {
        QFileInfo fileInfo(filename);
        QString extension = fileInfo.suffix().toLower();
        
        bool success = false;
        if (extension == "csv") {
            success = result_exporter_->exportToCSV(test_results_, filename);
        } else if (extension == "json") {
            success = result_exporter_->exportToJSON(test_results_, filename);
        } else if (extension == "html") {
            success = result_exporter_->exportToHTML(test_results_, filename, "PCB故障诊断测试报告");
        } else {
            // 默认导出为CSV
            success = result_exporter_->exportToCSV(test_results_, filename);
        }
        
        if (success) {
            statusBar()->showMessage(QString("成功导出 %1 个测试结果").arg(test_results_.size()), 5000);
        }
    }
}

void MainWindow::clearResults()
{
    test_results_.clear();
    updateResultsTable();
    detail_text_->clear();
}

void MainWindow::openPCBIdentification()
{
    if (!pcb_dialog_) {
        QMessageBox::warning(this, "错误", "PCB识别对话框未初始化");
        return;
    }
    
    // 显示PCB识别对话框
    pcb_dialog_->show();
    pcb_dialog_->raise();
    pcb_dialog_->activateWindow();
}

void MainWindow::openCameraControl()
{
    if (!camera_control_) {
        QMessageBox::warning(this, "错误", "相机控制组件未初始化");
        return;
    }
    
    // 设置为独立窗口并显示
    camera_control_->setWindowTitle("摄像头控制系统");
    camera_control_->setMinimumSize(1200, 800);
    camera_control_->show();
    camera_control_->raise();
    camera_control_->activateWindow();
}

void MainWindow::openPCBAnalyzer()
{
    if (!pcb_analyzer_) {
        QMessageBox::warning(this, "错误", "PCB分析器组件未初始化");
        return;
    }
    
    // 设置为独立窗口并显示
    pcb_analyzer_->setWindowTitle("PCB板卡元件综合分析系统");
    pcb_analyzer_->setMinimumSize(1400, 900);
    pcb_analyzer_->show();
    pcb_analyzer_->raise();
    pcb_analyzer_->activateWindow();
}

void MainWindow::openDetectionHistory()
{
    if (!history_widget_) {
        history_widget_ = new PCBDetectionHistoryWidget();
        history_widget_->setDetectionManager(detection_manager_);
        history_widget_->setAttribute(Qt::WA_DeleteOnClose, false);
        
        // 连接信号，当历史窗口关闭时重置指针
        connect(history_widget_, &QWidget::destroyed, this, [this]() {
            history_widget_ = nullptr;
        });
    }
    
    // 设置为独立窗口并显示
    history_widget_->setWindowTitle("PCB检测历史管理");
    history_widget_->setMinimumSize(1200, 800);    history_widget_->show();
    history_widget_->raise();
    history_widget_->activateWindow();    history_widget_->refreshRecordsList();
}

void MainWindow::openBoardManagement()
{
    if (!board_management_widget_) {
        return;
    }
    
    board_management_widget_->show();
    board_management_widget_->raise();
    board_management_widget_->activateWindow();
}

// 新增：接线引导相关槽函数实现
void MainWindow::startWiringGuide()
{
    // 获取当前选中的元件
    // 这里需要从UI中获取当前要测试的元件信息
    ComponentSpec component;
    component.reference = "R1";  // 示例，实际应从UI获取
    component.type = ComponentType::RESISTOR;
    component.nominal_value = 1000.0;  // 1KΩ
    component.tolerance = 0.05;  // 5%
    component.test_voltage = 3.3;  // 3.3V
    component.test_current = 0.01;  // 10mA
    component.requires_dmm = true;
    component.description = "测试电阻";
    
    showWiringGuideForComponent(component);
}

void MainWindow::showWiringGuideForComponent(const ComponentSpec& component)
{
    // 如果已有接线对话框在显示，先关闭
    if (current_wiring_dialog_) {
        current_wiring_dialog_->close();
        current_wiring_dialog_->deleteLater();
        current_wiring_dialog_ = nullptr;
    }
    
    // 创建接线引导对话框
    current_wiring_dialog_ = new WiringGuideDialog(component, this);
    current_wiring_dialog_->setTestParameters(component.test_voltage, 
                                              component.test_current, 
                                              component.requires_dmm);
      // 连接信号
    connect(current_wiring_dialog_, &QDialog::accepted, 
            [this, component]() {
                if (current_wiring_dialog_) {
                    WiringConfiguration config = current_wiring_dialog_->getWiringConfiguration();
                    onWiringCompleted(config);
                    
                    // 保存配置并生成测试任务
                    current_wiring_config_ = config;
                    
                    // 生成测试任务
                    TestTask task = task_generator_->generateTestTask(config, component);
                    
                    // 执行测试任务
                    single_test_button_->setText("执行测试中...");
                    single_result_text_->append("接线完成，开始执行测试任务...\n");
                    
                     fault_diagnostic_->diagnoseComponent(component);
                    // if (task_generator_->executeTestTask(task, device_manager_)) {
                    //     // 任务执行成功，开始故障诊断
                    //     QTimer::singleShot(1000, [this, component]() {
                    //         single_test_button_->setText("故障诊断中...");
                    //         single_result_text_->append("开始故障诊断分析...\n");
                    //         fault_diagnostic_->diagnoseComponent(component);
                    //     });
                    // } else {
                    //     // 任务执行失败
                    //     single_result_text_->append("测试任务执行失败！\n");
                    //     single_test_button_->setEnabled(true);
                    //     single_test_button_->setText("开始测试");
                    // }
                }
                
                current_wiring_dialog_ = nullptr;
            });
      connect(current_wiring_dialog_, &QDialog::rejected, 
            [this]() {
                // 用户取消了接线引导，恢复按钮状态
                single_test_button_->setEnabled(true);
                single_test_button_->setText("开始测试");
                single_result_text_->append("接线引导已取消。\n");
                current_wiring_dialog_ = nullptr;
            });
    
    // 显示对话框
    current_wiring_dialog_->show();
}

void MainWindow::onWiringCompleted(const WiringConfiguration& config)
{
    // 接线完成后的处理
    statusBar()->showMessage(QString("接线配置完成 - 配置ID: %1").arg(config.config_id), 3000);
    
    // 更新UI状态
    // 这里可以更新状态栏、进度条等
    
    // 记录接线配置
    current_wiring_config_ = config;
    
    // 可以在这里添加接线验证逻辑
    QStringList errors;
    if (wiring_resource_manager_->validateWiringConfiguration(config, errors)) {
        statusBar()->showMessage("接线验证通过，准备开始测试", 2000);
    } else {
        QString error_msg = "接线验证失败:\n" + errors.join("\n");
        QMessageBox::warning(this, "接线验证失败", error_msg);
    }
}

//=============================================================================
// 缺失的函数实现 - 修复链接器错误
//=============================================================================

void MainWindow::updateSystemStatus()
{
    bool systemReady = device_manager_ ? device_manager_->isSystemReady() : false;

    if (systemReady) {
        system_status_label_->setText("系统就绪");
    } else {
        system_status_label_->setText("系统未就绪");
    }
}

void MainWindow::updateDeviceStatus()
{
    if (!device_manager_) return;

    DeviceStatus aoStatus = device_manager_->getDeviceStatus("JY5711");
    ao_status_label_->setText(getStatusIcon(aoStatus) +
                              (aoStatus == DeviceStatus::CONNECTED ? " 已连接" : " 未连接"));

    DeviceStatus daq5322Status = device_manager_->getDeviceStatus("JY5322");
    daq_5322_status_label_->setText(getStatusIcon(daq5322Status) +
                                    (daq5322Status == DeviceStatus::CONNECTED ? " 已连接" : " 未连接"));

    DeviceStatus daq5323Status = device_manager_->getDeviceStatus("JY5323");
    daq_5323_status_label_->setText(getStatusIcon(daq5323Status) +
                                    (daq5323Status == DeviceStatus::CONNECTED ? " 已连接" : " 未连接"));

    DeviceStatus dmmStatus = device_manager_->getDeviceStatus("JY8902");
    dmm_status_label_->setText(getStatusIcon(dmmStatus) +
                               (dmmStatus == DeviceStatus::CONNECTED ? " 已连接" : " 未连接"));
}

void MainWindow::updateResultsTable()
{
    if (!results_table_) return;

    results_table_->setRowCount(test_results_.size());

    for (int i = 0; i < test_results_.size(); ++i) {
        const DiagnosticResult& result = test_results_[i];

        results_table_->setItem(i, 0, new QTableWidgetItem(result.testId));
        results_table_->setItem(i, 1, new QTableWidgetItem(result.componentType));
        results_table_->setItem(i, 2, new QTableWidgetItem(result.componentId));

        // 测试结果
        QString resultText;
        QColor bgColor;
        switch (result.result) {
        case DiagnosticResult::PASS:
            resultText = "通过";
            bgColor = QColor(144, 238, 144); // 浅绿色
            break;
        case DiagnosticResult::FAIL:
            resultText = "失败";
            bgColor = QColor(255, 182, 193); // 浅红色
            break;
        case DiagnosticResult::ERROR:
            resultText = "错误";
            bgColor = QColor(255, 255, 224); // 浅黄色
            break;
        }

        QTableWidgetItem* resultItem = new QTableWidgetItem(resultText);
        resultItem->setBackground(bgColor);
        results_table_->setItem(i, 3, resultItem);

        results_table_->setItem(i, 4, new QTableWidgetItem(QString::number(result.healthScore, 'f', 1) + "%"));
        results_table_->setItem(i, 5, new QTableWidgetItem(QString::number(result.confidence, 'f', 1) + "%"));
        results_table_->setItem(i, 6, new QTableWidgetItem(result.faultTypes.join(", ")));
        results_table_->setItem(i, 7, new QTableWidgetItem(result.timestamp.toString("hh:mm:ss")));
    }
}


void MainWindow::updateDetailText(int currentRow, int currentColumn, int previousRow, int previousColumn)
{
    Q_UNUSED(currentColumn)
    Q_UNUSED(previousRow)
    Q_UNUSED(previousColumn)

    if (currentRow >= 0 && currentRow < test_results_.size()) {
        const DiagnosticResult& result = test_results_[currentRow];
        detail_text_->setText(formatResult(result));
    }
}

TestStep MainWindow::createTestStepFromComponent(const ComponentSpec& component)
{
    TestStep step;

    // 基本信息
    switch (component.type) {
    case ComponentType::RESISTOR:
        step.componentType = "resistor";
        step.testName = QString("电阻测试 - %1").arg(component.reference);
        step.specs.resistance.nominal = component.nominal_value;
        step.specs.resistance.tolerance = component.tolerance;
        step.specs.resistance.tempCoefficient = component.temp_coefficient;
        break;

    case ComponentType::CAPACITOR:
        step.componentType = "capacitor";
        step.testName = QString("电容测试 - %1").arg(component.reference);
        step.specs.capacitance.nominal = component.nominal_value;
        step.specs.capacitance.tolerance = component.tolerance;
        step.specs.capacitance.esr = component.max_esr;
        step.specs.capacitance.leakageCurrent = component.max_leakage;
        break;
    case ComponentType::INDUCTOR:
        step.componentType = "inductor";
        step.testName = QString("电感测试 - %1").arg(component.reference);
        step.specs.inductance.nominal = component.nominal_value;
        step.specs.inductance.tolerance = component.tolerance;
        step.specs.inductance.dcResistance = 0.1; // 默认直流电阻
        step.specs.inductance.qFactory = 50.0; // 默认品质因数
        break;
    case ComponentType::DIODE:
        step.componentType = "diode";
        step.testName = QString("二极管测试 - %1").arg(component.reference);
        step.specs.diode.forwardVoltage = component.nominal_value;
        step.specs.diode.reverseLeakage = component.max_leakage;
        step.specs.diode.breakdownVoltage = component.max_voltage * 1.2; // 击穿电压比最大工作电压高20%
        break;
    case ComponentType::IC:
        step.componentType = "ic";
        step.testName = QString("集成电路测试 - %1").arg(component.reference);
        step.specs.ic.supplyVoltage = component.nominal_value;
        step.specs.ic.supplyCurrent = component.max_current;
        step.specs.ic.inputLevels.high = component.nominal_value * 0.7; // 70% Vcc 为高电平
        step.specs.ic.inputLevels.low = component.nominal_value * 0.3;  // 30% Vcc 为低电平
        step.specs.ic.outputLevels.high = component.nominal_value * 0.8; // 80% Vcc 为输出高电平
        step.specs.ic.outputLevels.low = component.nominal_value * 0.2;  // 20% Vcc 为输出低电平
        break;

    default:
        step.componentType = "unknown";
        step.testName = QString("未知组件测试 - %1").arg(component.reference);
        break;
    }

    // 通用设置
    step.enabled = true;
    step.timeoutMs = 5000; // 5秒超时

    // 设置测试参数
    step.parameters["channel"] = component.channel;
    step.parameters["reference"] = component.reference;
    step.parameters["description"] = component.description;
    step.parameters["max_voltage"] = component.max_voltage;
    step.parameters["max_current"] = component.max_current;

    return step;
}


ComponentSpec MainWindow::createComponentFromUI()
{
    ComponentSpec component;
    QString type_text = component_type_combo_->currentText();
    if (type_text == "电阻") component.type = ComponentType::RESISTOR;
    else if (type_text == "电容") component.type = ComponentType::CAPACITOR;
    else if (type_text == "电感") component.type = ComponentType::INDUCTOR;
    else if (type_text == "二极管") component.type = ComponentType::DIODE;
    else if (type_text == "集成电路") component.type = ComponentType::IC;
    else component.type = ComponentType::RESISTOR; // defaultcomponent.type = ComponentType::IC;

    component.reference = component_ref_edit_->text();
    component.nominal_value = nominal_value_spin_->value();
    component.tolerance = tolerance_spin_->value() / 100.0;
    component.channel = channel_spin_->value();

    return component;
}


void MainWindow::populateComponentTable()
{
    if (!component_table_) return;

    component_table_->setRowCount(current_sequence_.steps.size());

    for (int i = 0; i < current_sequence_.steps.size(); ++i) {
        const TestStep& step = current_sequence_.steps[i];

        component_table_->setItem(i, 0, new QTableWidgetItem(step.testName));
        component_table_->setItem(i, 1, new QTableWidgetItem(step.componentType));

        // 根据组件类型显示标称值
        QString nominalValue;
        if (step.componentType == "resistor") {
            nominalValue = QString::number(step.specs.resistance.nominal, 'g', 6) + "Ω";
        } else if (step.componentType == "capacitor") {
            nominalValue = QString::number(step.specs.capacitance.nominal * 1e6, 'g', 6) + "μF";
        } else if (step.componentType == "inductor") {
            nominalValue = QString::number(step.specs.inductance.nominal * 1e6, 'g', 6) + "μH";
        } else if (step.componentType == "diode") {
            nominalValue = QString::number(step.specs.diode.forwardVoltage, 'g', 3) + "V";
        } else if (step.componentType == "ic") {
            nominalValue = QString::number(step.specs.ic.supplyVoltage, 'g', 3) + "V";
        }

        component_table_->setItem(i, 2, new QTableWidgetItem(nominalValue));
        component_table_->setItem(i, 3, new QTableWidgetItem(step.enabled ? "启用" : "禁用"));
        component_table_->setItem(i, 4, new QTableWidgetItem(QString::number(step.timeoutMs / 1000.0, 'f', 1) + "s"));

        // 设置行颜色
        QColor bgColor = step.enabled ? QColor(240, 255, 240) : QColor(255, 240, 240);
        for (int col = 0; col < component_table_->columnCount(); ++col) {
            if (component_table_->item(i, col)) {
                component_table_->item(i, col)->setBackground(bgColor);
            }
        }
    }
}


QString MainWindow::formatResult(const DiagnosticResult& result)
{
    QString text;
    text += "=== 测试结果 ===\n";
    text += QString("测试ID: %1\n").arg(result.testId);
    text += QString("组件类型: %1\n").arg(result.componentType);
    text += QString("组件标识: %1\n").arg(result.componentId);

    QString resultText;
    switch (result.result) {
    case DiagnosticResult::PASS: resultText = "通过"; break;
    case DiagnosticResult::FAIL: resultText = "失败"; break;
    case DiagnosticResult::ERROR: resultText = "错误"; break;
    }
    text += QString("测试结果: %1\n").arg(resultText);

    text += QString("健康度: %1%\n").arg(result.healthScore, 0, 'f', 1);
    text += QString("置信度: %1%\n").arg(result.confidence, 0, 'f', 1);

    if (!result.faultTypes.isEmpty()) {
        text += QString("故障类型: %1\n").arg(result.faultTypes.join(", "));
    }

    text += QString("期望值: %1\n").arg(result.expectedValue, 0, 'g', 6);
    text += QString("容差: ±%1%\n").arg(result.tolerance * 100, 0, 'f', 2);
        // 测量数据
    text += "\n=== 测量数据 ===\n";
    const MeasurementResult& data = result.measurementData;

    // 根据组件类型显示相应的测量值
    if (result.componentType == "电阻" && data.primary_value != 0.0) {
        text += QString("电阻值: %1 Ω\n").arg(data.primary_value, 0, 'g', 6);
    } else if (result.componentType == "电容" && data.primary_value != 0.0) {
        text += QString("电容值: %1 F\n").arg(data.primary_value, 0, 'g', 6);
    } else if (result.componentType == "电感" && data.primary_value != 0.0) {
        text += QString("电感值: %1 H\n").arg(data.primary_value, 0, 'g', 6);
    } else if (data.primary_value != 0.0) {
        text += QString("主要测量值: %1\n").arg(data.primary_value, 0, 'g', 6);
    }

    if (data.voltage != 0.0) text += QString("电压: %1 V\n").arg(data.voltage, 0, 'g', 6);
    if (data.current != 0.0) text += QString("电流: %1 A\n").arg(data.current, 0, 'g', 6);
    if (data.power != 0.0) text += QString("功率: %1 W\n").arg(data.power, 0, 'g', 6);
    if (data.esr != 0.0) text += QString("等效串联电阻: %1 Ω\n").arg(data.esr, 0, 'g', 6);
    if (data.leakage_current != 0.0) text += QString("漏电流: %1 A\n").arg(data.leakage_current, 0, 'g', 6);
    if (data.temperature != 25.0) text += QString("温度: %1 °C\n").arg(data.temperature, 0, 'f', 1);

    text += QString("\n测试时间: %1\n").arg(result.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    text += QString("测试设备: %1\n").arg(result.testEquipment);

    if (!result.notes.isEmpty()) {
        text += QString("备注: %1\n").arg(result.notes);
    }

    return text;
}


QString MainWindow::getStatusIcon(DeviceStatus status)
{
    switch (status) {
    case DeviceStatus::CONNECTED: return "✅";
    case DeviceStatus::ERROR: return "❌";
    default: return "⚪";
    }
}

