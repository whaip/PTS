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
#include <QScrollArea>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include "ComponentDiagnosticFramework/componentdiagnosticframework.h"
#include "ComponentDiagnosticFramework/componentdiagnosticmanager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , device_manager_(nullptr)
    , fault_diagnostic_(nullptr)
    , sequence_manager_(nullptr)
    , result_exporter_(nullptr)
    , pcb_identifier_(nullptr)
    , current_task_id_(QString())  // 初始化任务ID为空
    , camera_control_(nullptr)
    , pcb_analyzer_(nullptr)
    , detection_manager_(nullptr)
    , board_management_widget_(nullptr)
    , board_manager_(nullptr)
    , device_test_window_(nullptr)  // 新增：设备管理器测试窗口
    , status_timer_(new QTimer(this))
    , port_manager_(nullptr)
    , wiring_resource_manager_(nullptr)
    , current_wiring_dialog_(nullptr)
    , task_generator_(nullptr)
    , current_test_index_(0)
    , batch_testing_active_(false)
    , unified_wiring_prepared_(false)
{
    ui->setupUi(this);

    // 初始化核心组件
    camera_control_ = new CameraControlWidget();
    device_manager_ = new DeviceManager(camera_control_->getCameraManager(), this);
    diagnostic_manager = ComponentDiagnosticFramework::createDiagnosticManager(device_manager_, this);
    fault_diagnostic_ = new FaultDiagnostic(device_manager_, diagnostic_manager, this);
    sequence_manager_ = new TestSequenceManager(this);
    result_exporter_ = new ResultExporter(this);
    pcb_identifier_ = new PCBIdentifier(this);
      // 初始化PCB综合分析器 - 作为独立窗口
    pcb_analyzer_ = new RealtimePCBAnalyzerWidget();
    pcb_analyzer_->setPCBIdentifier(pcb_identifier_);
    pcb_analyzer_->setCameraManager(camera_control_->getCameraManager());
      // 新增：初始化端口管理器
    port_manager_ = new PortManager(device_manager_, this);
    if (!port_manager_->initializePorts()) {
        QMessageBox::warning(this, "警告", "端口系统初始化失败");
    }

    wiring_resource_manager_ = new WiringResourceManager(port_manager_, this);

    // 初始化任务生成器
    task_generator_ = new WiringTaskGenerator(wiring_resource_manager_, this);
    connect(task_generator_, &WiringTaskGenerator::taskGenerated,
            [this](const TestTask& task) {
                qDebug() << "测试任务已生成:" << task.taskId;
                statusBar()->showMessage(QString("测试任务已生成: %1").arg(task.taskName), 3000);
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
    board_management_widget_ = new PCBBoardManagementWidget(diagnostic_manager);
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

    connect(board_management_widget_, &PCBBoardManagementWidget::diagnoseComponents,
            this, &MainWindow::onDiagnoseComponents);

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
      if (current_wiring_dialog_) {
        disconnect(current_wiring_dialog_, nullptr, nullptr, nullptr);
        current_wiring_dialog_->close();
        // 改为同步删除，确保在port_manager_删除前完成
        delete current_wiring_dialog_;
        current_wiring_dialog_ = nullptr;
    }

    if (fault_diagnostic_) {
        disconnect(fault_diagnostic_, nullptr, nullptr, nullptr);

        // 强制删除，触发其析构函数
        delete fault_diagnostic_;
        fault_diagnostic_ = nullptr;

        qDebug() << "故障诊断模块清理完成";

        // 处理事件，确保所有析构操作完成
        QApplication::processEvents();
        QThread::msleep(200);
    }

    if (diagnostic_manager) {
        // 取消所有活动任务
        auto activeTasks = diagnostic_manager->getActiveTasks();
        for (const QString& taskId : activeTasks) {
            diagnostic_manager->cancelTask(taskId);
        }

        // 清理已完成的任务
        diagnostic_manager->cleanupCompletedTasks();

        delete diagnostic_manager;
        diagnostic_manager = nullptr;
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
    
    // 清理测量数据图表
    if (measurement_plot_) {
        measurement_plot_->deleteLater();
        measurement_plot_ = nullptr;
    }
    
    // 清理单元件测试图表
    if (single_measurement_plot_) {
        single_measurement_plot_->deleteLater();
        single_measurement_plot_ = nullptr;
    }
      // 清理单元件测试红外图像显示
    if (single_thermal_display_) {
        single_thermal_display_->clear();
        single_thermal_display_->deleteLater();
        single_thermal_display_ = nullptr;
    }

   // 清理批量测试红外图像显示
   if (batch_thermal_display_) {
       batch_thermal_display_->clear();
       batch_thermal_display_->deleteLater();
       batch_thermal_display_ = nullptr;
   }

    if (device_test_window_) {
        disconnect(device_test_window_, nullptr, this, nullptr);
        device_test_window_->close();
        device_test_window_->deleteLater();
        device_test_window_ = nullptr;
    }
      // 清理接线引导相关组件 - 先清理task_generator，再清理resource manager，最后清理port manager
    if (task_generator_) {
        disconnect(task_generator_, nullptr, this, nullptr);
        task_generator_->deleteLater();
        task_generator_ = nullptr;
    }

    // 先清理wiring_resource_manager_，它会调用releaseAllResources()
    if (wiring_resource_manager_) {
        disconnect(wiring_resource_manager_, nullptr, this, nullptr);
        wiring_resource_manager_->deleteLater();  // 不再手动调用releaseAllResources，让析构函数处理
        wiring_resource_manager_ = nullptr;
    }

    // 最后清理port_manager_，避免双重释放端口
    if (port_manager_) {
        disconnect(port_manager_, nullptr, this, nullptr);
        port_manager_->deleteLater();  // 不再手动调用releaseAllPorts，资源已经被上面释放了
        port_manager_ = nullptr;
    }// 关闭设备管理器
    if (device_manager_) {
        disconnect(device_manager_, nullptr, this, nullptr);
        device_manager_->shutdownDeviceThreads();
    }

    // 强制处理所有待处理的事件
    QApplication::processEvents();

    delete ui;

    qDebug() << "MainWindow 析构完成";
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
    param_group->setFixedWidth(400);  // 增加宽度以容纳更多控件
    QVBoxLayout* param_layout = new QVBoxLayout(param_group);

    // 基本信息区域
    QGroupBox* basic_group = new QGroupBox("基本信息");
    QFormLayout* basic_layout = new QFormLayout(basic_group);

    // 组件类型选择
    component_type_combo_ = new QComboBox();
    QVector<ComponentType> supportTypes = diagnostic_manager->getSupportedComponentTypes();
    for (auto t : supportTypes) {
        component_type_combo_->addItem(getComponentTypeDisplayName(t), QVariant::fromValue<int>(int(t)));
    }
    basic_layout->addRow("元件类型:", component_type_combo_);

    // 组件标识
    component_ref_edit_ = new QLineEdit();
    component_ref_edit_->setPlaceholderText("如: R1, C2, L3, D4, IC5");
    basic_layout->addRow("元件标识:", component_ref_edit_);

    param_layout->addWidget(basic_group);

    // 创建参数标签页控件
    single_param_tabs_ = new QTabWidget();
    param_layout->addWidget(single_param_tabs_);

    // 动态创建各类型参数页面并保存对应控件
    single_param_widgets_map_.clear();
    for (int i = 0; i < supportTypes.size(); ++i) {
        ComponentType t = supportTypes[i];
        QWidget* param_page = new QWidget();
        single_param_tabs_->addTab(param_page, getComponentTypeDisplayName(t) + " 参数");
        QFormLayout* param_form_layout = new QFormLayout(param_page);

        QList<QWidget*> widgetList;
        auto params = diagnostic_manager->getRequiredParameters(t);
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            QString paramName = it.key();
            QVariant defaultValue = it.value();
            QWidget* widget = nullptr;
            
            switch (defaultValue.type()) {
                case QMetaType::Bool: {
                    QCheckBox* cb = new QCheckBox();
                    cb->setChecked(defaultValue.toBool());
                    widget = cb;
                    break;
                }
                case QMetaType::Int: {
                    QSpinBox* sb = new QSpinBox();
                    sb->setRange(-999999, 999999);
                    sb->setValue(defaultValue.toInt());
                    widget = sb;
                    break;
                }
                case QMetaType::Double: {
                    QDoubleSpinBox* dsb = new QDoubleSpinBox();
                    dsb->setRange(0.001, 1000000);
                    dsb->setDecimals(6);
                    dsb->setValue(defaultValue.toDouble());
                    widget = dsb;
                    break;
                }
                case QMetaType::QStringList: {
                    QComboBox* cb = new QComboBox();
                    for (auto& opt : defaultValue.toStringList())
                        cb->addItem(opt);
                    widget = cb;
                    break;
                }
                default: {
                    QLineEdit* le = new QLineEdit();
                    le->setText(defaultValue.toString());
                    widget = le;
                    break;
                }
            }
            param_form_layout->addRow(paramName + ":", widget);
            widgetList.append(widget);
        }
        single_param_widgets_map_.insert(t, widgetList);
    }

    // 切换到对应类型的参数页
    connect(component_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int idx){
            single_param_tabs_->setCurrentIndex(idx);
        });

    // 初始化
    component_type_combo_->setCurrentIndex(0);
    single_param_tabs_->setCurrentIndex(0);

    // 开始测试按钮
    single_test_button_ = new QPushButton("开始测试");
    single_test_button_->setEnabled(false);
    param_layout->addWidget(single_test_button_);

    main_layout->addWidget(param_group);

    // 右侧结果显示区域 - 使用垂直分割器
    QSplitter* result_splitter = new QSplitter(Qt::Vertical);
    
    // 上部分：测量数据图表
    QGroupBox* chart_group = new QGroupBox("测量数据图表");
    QVBoxLayout* chart_layout = new QVBoxLayout(chart_group);
    
    single_measurement_plot_ = new UESTCQCustomPlot();
    single_measurement_plot_->setMinimumHeight(200);
    chart_layout->addWidget(single_measurement_plot_);
    
    result_splitter->addWidget(chart_group);

    // 下部分：使用水平分割器分为左右两部分
    QSplitter* bottom_splitter = new QSplitter(Qt::Horizontal);
    
    // 左侧：文本结果
    QGroupBox* text_group = new QGroupBox("测试结果");
    QVBoxLayout* text_layout = new QVBoxLayout(text_group);
    
    single_result_text_ = new QTextEdit();
    single_result_text_->setReadOnly(true);
    text_layout->addWidget(single_result_text_);
    
    bottom_splitter->addWidget(text_group);
    
    // 右侧：红外图像显示
    QGroupBox* thermal_group = new QGroupBox("红外热成像");
    QVBoxLayout* thermal_layout = new QVBoxLayout(thermal_group);
    
    single_thermal_display_ = new IRImageDisplay();
    single_thermal_display_->setMinimumSize(320, 240);
    single_thermal_display_->setMouseTracking(true);
    thermal_layout->addWidget(single_thermal_display_);
    
    bottom_splitter->addWidget(thermal_group);
    
    // 设置水平分割器的初始大小比例（文本占60%，图像占40%）
    bottom_splitter->setSizes({360, 240});
    
    result_splitter->addWidget(bottom_splitter);
    
    // 设置分割器的初始大小比例（图表占40%，文本占60%）
    result_splitter->setSizes({200, 300});

    main_layout->addWidget(result_splitter);

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
    remove_all_components_button_ = new QPushButton("删除所有元件");
    load_sequence_button_ = new QPushButton("加载序列");
    save_sequence_button_ = new QPushButton("保存序列");
    batch_test_button_ = new QPushButton("开始批量测试");
    batch_test_button_->setEnabled(false);

    toolbar_layout->addWidget(add_component_button_);
    toolbar_layout->addWidget(remove_component_button_);
    toolbar_layout->addWidget(remove_all_components_button_);
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
    connect(remove_all_components_button_, &QPushButton::clicked, this, &MainWindow::removeAllComponents);
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

    // 使用垂直分割器将空间分为上下两部分
    QSplitter* main_splitter = new QSplitter(Qt::Vertical);
    
    // 上部分：测量数据图表
    measurement_plot_ = new UESTCQCustomPlot();
    measurement_plot_->setMinimumHeight(300);
    main_splitter->addWidget(measurement_plot_);
    
    // 下部分：使用水平分割器放置三个部分：表格、详细信息、红外图像
    QSplitter* bottom_splitter = new QSplitter(Qt::Horizontal);

    // 左侧：结果表格
    results_table_ = new QTableWidget();
    results_table_->setColumnCount(8);
    QStringList headers = {"测试ID", "组件类型", "组件ID", "测试结果", "健康度", "置信度", "故障类型", "时间戳"};
    results_table_->setHorizontalHeaderLabels(headers);
    results_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_table_->horizontalHeader()->setStretchLastSection(true);

    bottom_splitter->addWidget(results_table_);

    // 中间：详细信息
    detail_text_ = new QTextEdit();
    detail_text_->setReadOnly(true);
    bottom_splitter->addWidget(detail_text_);
    
    // 右侧：红外图像显示
    QGroupBox* batch_thermal_group = new QGroupBox("红外热成像");
    QVBoxLayout* batch_thermal_layout = new QVBoxLayout(batch_thermal_group);
    
    batch_thermal_display_ = new IRImageDisplay();
    batch_thermal_display_->setMinimumSize(320, 240);
    batch_thermal_display_->setMouseTracking(true);
    batch_thermal_layout->addWidget(batch_thermal_display_);
    
    bottom_splitter->addWidget(batch_thermal_group);
    
    // 设置水平分割器的大小比例（表格50%，详细信息30%，红外图像20%）
    bottom_splitter->setSizes({500, 300, 200});
    
    main_splitter->addWidget(bottom_splitter);
    
    // 设置主分割器的初始大小比例（上部分占40%，下部分占60%）
    main_splitter->setSizes({400, 600});
    
    layout->addWidget(main_splitter);

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
    system_menu->addAction("设备管理器测试", this, &MainWindow::openDeviceManagerTest);
    system_menu->addSeparator();
    system_menu->addAction("接线引导", this, &MainWindow::startWiringGuide);

    // 工具菜单
    QMenu* tools_menu = menu_bar->addMenu("工具");
    tools_menu->addAction("相机控制", this, &MainWindow::openCameraControl);
    tools_menu->addAction("PCB板卡管理", this, &MainWindow::openBoardManagement);
    tools_menu->addAction("PCB综合分析", this, &MainWindow::openPCBAnalyzer);
    tools_menu->addSeparator();

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

    // 清除之前的任务ID
    current_task_id_.clear();

    // 检查系统是否就绪
    if (!device_manager_ || !device_manager_->isSystemReady()) {
        QMessageBox::warning(this, "系统未就绪", "请先初始化测试设备！");
        return;
    }

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
    // 如果当前正在进行批量测试，则停止测试
    if (batch_testing_active_) {
        stopBatchTest();
        return;
    }

    // Check if system is ready
    bool systemReady = device_manager_ ? device_manager_->isSystemReady() : false;

    if (!systemReady) {
        QMessageBox::warning(this, "系统未就绪", "请先初始化测试设备！");
        return;
    }

    // 创建批量测试的统一组件规格
    QVector<ComponentSpec> batchComponent = createBatchComponentSpec();

    // 使用单一的接线引导对话框进行批量测试接线
    startBatchTestWithUnifiedWiring(batchComponent);
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
        qDebug() << "跳过禁用的测试步骤：" << step.testName;
        current_test_index_++;
        test_progress_->setValue(current_test_index_);
        QTimer::singleShot(100, this, &MainWindow::runNextTest);
        return;
    }
      qDebug() << "开始测试步骤" << (current_test_index_ + 1) << "/" << current_sequence_.steps.size() << ":" << step.testName;

    // 创建ComponentSpec用于新框架
    ComponentSpec specs;
    specs.reference = step.component; // 使用testName作为reference
    specs.type = step.common_type;
    specs.params = step.specs;
    specs.allocatedPorts = step.allocatedPorts;
    // specs.parameters保持为空或根据需要设置

    test_count_label_->setText(QString("正在测试: %1 (%2/%3)")
                              .arg(step.testName)
                              .arg(current_test_index_ + 1)
                              .arg(current_sequence_.steps.size()));
    if (task_generator_ && unified_wiring_prepared_ && !main_batch_task_id_.isEmpty()) {
        // 设置当前步骤的任务ID为主任务ID（共享端口分配）
        current_task_id_ = main_batch_task_id_;

        qDebug() << "使用主任务端口分配执行测试步骤：" << specs.reference;

        // 直接执行故障诊断，使用已分配的端口
        QTimer::singleShot(100, [this, specs]() {
            // 执行故障诊断
            fault_diagnostic_->diagnoseComponent(specs);
        });
    } else {
        qDebug() << "错误：任务生成器未初始化或统一接线未准备";
        proceedToNextBatchTest();
    }
}

void MainWindow::finishBatchTest()
{
    batch_testing_active_ = false;
    batch_test_button_->setText("开始批量测试");
    batch_test_button_->setEnabled(true);
    test_progress_->setVisible(false);
      // 清理统一接线方案
    unified_wiring_prepared_ = false;

    // 完成主任务并释放端口
    if (task_generator_ && !main_batch_task_id_.isEmpty()) {
        task_generator_->finalizeTaskExecution(main_batch_task_id_, true, "BATCH_COMPLETED");
        main_batch_task_id_.clear();
    }
    current_task_id_.clear();
    camera_control_->getCameraManager()->stopCamera(CameraType::IR_CAMERA);

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

    // 在单元件测试时完成任务并释放端口，在批量测试时保持主任务不变
    if (task_generator_ && !current_task_id_.isEmpty() && !batch_testing_active_) {
        bool success = (result.result == DiagnosticResult::PASS);
        QString resultString = (result.result == DiagnosticResult::PASS) ? "PASS" :
                              (result.result == DiagnosticResult::FAIL) ? "FAIL" : "ERROR";
        task_generator_->finalizeTaskExecution(current_task_id_, success, resultString);
        current_task_id_.clear();  // 清除任务ID
    }

    // 在单元件测试时更新结果显示
    if (!batch_testing_active_) {
        single_result_text_->append(result.diagnosticSummary);
        updateSingleTestMeasurementPlot(result.metameasurementData);

        single_thermal_display_->clear();
        if (result.thermalData.isValid) {
            updateSingleTestThermalDisplay(result.thermalData);
        }
        camera_control_->getCameraManager()->stopCamera(CameraType::IR_CAMERA);
        single_result_text_->append("\n测试完成。");

        // Re-enable the single test button
        single_test_button_->setEnabled(true);
        single_test_button_->setText("开始测试");
    }
      // 在批量测试时处理下一个测试
    if (batch_testing_active_) {
        updateResultsTable();

        // 使用统一的下一个测试处理函数
        proceedToNextBatchTest();
    }
}

void MainWindow::onErrorOccurred(const QString& error)
{
    qDebug() << "错误发生：" << error;

    // 在批量测试中，记录错误但继续下一个测试
    if (batch_testing_active_) {
        // 创建错误结果记录
        DiagnosticResult errorResult;
        errorResult.testId = QString("batch_test_%1").arg(current_test_index_ + 1);
        errorResult.componentType = "未知";
        errorResult.componentId = QString("Step_%1").arg(current_test_index_ + 1);
        errorResult.result = DiagnosticResult::ERROR;
        errorResult.healthScore = 0.0;
        errorResult.confidence = 0.0;
        errorResult.notes = QString("测试错误: %1").arg(error);
        errorResult.timestamp = QDateTime::currentDateTime();

        test_results_.append(errorResult);

        // 继续下一个测试
        proceedToNextBatchTest();
    } else {
        // 在单元件测试中，显示错误对话框
        QMessageBox::warning(this, "错误", error);

        // 恢复单元件测试按钮状态
        if (single_test_button_) {
            single_test_button_->setEnabled(true);
            single_test_button_->setText("开始测试");
        }
    }
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
    QVector<ComponentType> supportTypes = diagnostic_manager->getSupportedComponentTypes();
    for (auto t : supportTypes) {
        type_combo->addItem(getComponentTypeDisplayName(t), QVariant::fromValue<int>(int(t)));
    }
    basic_layout->addRow("元件类型:", type_combo);

    // 元件标识
    QLineEdit* ref_edit = new QLineEdit();
    ref_edit->setPlaceholderText("如: R1, C2, L3, D4, IC5");
    basic_layout->addRow("元件标识:", ref_edit);

    // 描述信息
    QLineEdit* desc_edit = new QLineEdit();
    desc_edit->setPlaceholderText("可选的描述信息");
    basic_layout->addRow("描述:", desc_edit);

    // 动态创建各类型参数页面并保存对应控件
    QMap<ComponentType, QList<QWidget*>> paramWidgetsMap;
    for (int i = 0; i < supportTypes.size(); ++i) {
        ComponentType t = supportTypes[i];
        QWidget* param_page = new QWidget();
        tabs->addTab(param_page, getComponentTypeDisplayName(t) + " 参数");
        QFormLayout* param_layout = new QFormLayout(param_page);

        QList<QWidget*> widgetList;
        auto params = diagnostic_manager->getRequiredParameters(t);
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            QString paramName = it.key();
            QVariant defaultValue = it.value();
            QWidget* widget = nullptr;
            switch (defaultValue.type()) {
                case QMetaType::Bool: {
                    QCheckBox* cb = new QCheckBox();
                    cb->setChecked(defaultValue.toBool());
                    widget = cb;
                    break;
                }
                case QMetaType::Int: {
                    QSpinBox* sb = new QSpinBox();
                    sb->setValue(defaultValue.toInt());
                    widget = sb;
                    break;
                }
                case QMetaType::Double: {
                    QDoubleSpinBox* dsb = new QDoubleSpinBox();
                    dsb->setDecimals(6);
                    dsb->setValue(defaultValue.toDouble());
                    widget = dsb;
                    break;
                }
                case QMetaType::QStringList: {
                    QComboBox* cb = new QComboBox();
                    for (auto& opt : defaultValue.toStringList())
                        cb->addItem(opt);
                    widget = cb;
                    break;
                }
                default: {
                    QLineEdit* le = new QLineEdit();
                    le->setText(defaultValue.toString());
                    widget = le;
                    break;
                }
            }
            param_layout->addRow(paramName + ":", widget);
            widgetList.append(widget);
        }
        paramWidgetsMap.insert(t, widgetList);
    }

    // 切换到对应类型的参数页
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [tabs](int idx){
            tabs->setCurrentIndex(idx + 1);
        });

    // 初始化
    type_combo->setCurrentIndex(0);
    tabs->setCurrentIndex(1);

    // 对话框按钮
    QDialogButtonBox* button_box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    main_layout->addWidget(button_box);
    connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // 执行对话框
    if (dialog.exec() != QDialog::Accepted) return;

    // 构建ComponentSpec
    ComponentSpec component;
    component.type = supportTypes[type_combo->currentIndex()];
    component.reference = ref_edit->text().trimmed();
    component.description = desc_edit->text().trimmed();

    // 读取参数页面的值
    auto paramKeys = diagnostic_manager->getRequiredParameters(component.type).keys();
    auto widgets   = paramWidgetsMap.value(component.type);
    for (int i = 0; i < widgets.size() && i < paramKeys.size(); ++i) {
        QWidget* w = widgets[i];
        QVariant v;
        if (auto cb = qobject_cast<QCheckBox*>(w))        v = cb->isChecked();
        else if (auto sb = qobject_cast<QSpinBox*>(w))    v = sb->value();
        else if (auto dsb = qobject_cast<QDoubleSpinBox*>(w)) v = dsb->value();
        else if (auto cbx = qobject_cast<QComboBox*>(w))  v = cbx->currentText();
        else if (auto le = qobject_cast<QLineEdit*>(w))   v = le->text();
        component.params.insert(paramKeys[i], v);
    }

    // 验证输入
    if (component.reference.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入元件标识！");
        return;
    }
    for (auto& ex : current_sequence_.steps) {
        if (ex.component == component.reference) {
            QMessageBox::warning(this, "输入错误",
                QString("元件标识 '%1' 已存在，请使用不同的标识！").arg(component.reference));
            return;
        }
    }

    TestStep step = createTestStepFromComponent(component);
    // 添加重复检查，避免相同组件的测试步骤被重复加入
    for (const TestStep &existingStep : current_sequence_.steps) {
        if (existingStep.component == step.component) {
            QMessageBox::warning(this, "添加失败",
                                 QString("测试步骤 '%1' 已存在！").arg(step.component));
            return;
        }
    }
    sequence_manager_->addTestStep(current_sequence_, step);
    populateComponentTable();

    QMessageBox::information(this, "添加成功",
        QString("已成功添加 %1 '%2'")
            .arg(getComponentTypeDisplayName(component.type))
            .arg(component.reference));
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

    // 刷新表格显示
    populateComponentTable();

    // 显示成功消息
    statusBar()->showMessage(QString("已删除测试步骤: %1").arg(componentRef), 3000);
}

void MainWindow::removeAllComponents()
{
    if (current_sequence_.steps.isEmpty()) {
        QMessageBox::information(this, "提示", "没有元件可以删除");
        return;
    }

    int result = QMessageBox::question(this, "确认删除",
        "确定要删除所有测试步骤吗？",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result == QMessageBox::Yes) {
        sequence_manager_->removeAllTestStep(current_sequence_);
        populateComponentTable();
        statusBar()->showMessage("已清除所有测试步骤", 3000);
    }
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
        return;
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
        "CSV文件 (*.csv);;JSON文件 (*.json);;所有文件 (*.*)");

    if (!filename.isEmpty()) {
        QFileInfo fileInfo(filename);
        QString extension = fileInfo.suffix().toLower();

        bool success = false;
        if (extension == "csv") {
            success = result_exporter_->exportToCSV(test_results_, filename);
        } else if (extension == "json") {
            success = result_exporter_->exportToJSON(test_results_, filename);
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

void MainWindow::openBoardManagement()
{
    if (!board_management_widget_) {
        return;
    }
      board_management_widget_->show();
    board_management_widget_->raise();
    board_management_widget_->activateWindow();
}

void MainWindow::openDeviceManagerTest()
{
    if (!device_test_window_) {
        device_test_window_ = new DeviceManagerTestWindow(device_manager_, this);
        device_test_window_->setAttribute(Qt::WA_DeleteOnClose, false);

        // 连接信号，当测试窗口关闭时重置指针
        connect(device_test_window_, &QWidget::destroyed, this, [this]() {
            device_test_window_ = nullptr;
        });
    }

    // 设置为独立窗口并显示
    device_test_window_->setWindowTitle("设备管理器测试");
    device_test_window_->setMinimumSize(800, 600);
    device_test_window_->show();
    device_test_window_->raise();
    device_test_window_->activateWindow();
}

// 新增：接线引导相关槽函数实现
void MainWindow::startWiringGuide()
{
    // 获取当前选中的元件
    // 这里需要从UI中获取当前要测试的元件信息
    ComponentSpec component;
    component.reference = component_ref_edit_->text().isEmpty() ? "R1" : component_ref_edit_->text();
    component.type = static_cast<ComponentType>(component_type_combo_->currentIndex());
    component.nominal_value = nominal_value_spin_->value();
    component.tolerance_percent = tolerance_spin_->value();

    showWiringGuideForComponent(component);
}

void MainWindow::showWiringGuideForComponent(const ComponentSpec& component)
{    // 如果已有接线对话框在显示，先关闭
    if (current_wiring_dialog_) {
        current_wiring_dialog_->close();
        // 改为同步删除，确保立即释放资源
        delete current_wiring_dialog_;
        current_wiring_dialog_ = nullptr;
    }

    if (!port_manager_) {
        QMessageBox::warning(this, "错误", "端口管理器未初始化");
        return;
    }

    // 创建接线引导对话框
    current_wiring_dialog_ = new WiringGuideDialog(component, port_manager_, diagnostic_manager, this);

    // 连接信号
    connect(current_wiring_dialog_, &WiringGuideDialog::wiringCompleted,
            this, &MainWindow::onWiringCompleted);
    connect(current_wiring_dialog_, &WiringGuideDialog::wiringCancelled,
            [this]() {
                // 用户取消了接线引导，恢复按钮状态
                single_test_button_->setEnabled(true);
                single_test_button_->setText("开始测试");
                current_wiring_dialog_ = nullptr;
            });
      // 显示对话框
    current_wiring_dialog_->show();
}

void MainWindow::onWiringCompleted(const WiringScheme& scheme, const QMap<QString, QVector<PortInfo>>& allocatedPorts)
{
    // 接线完成后的处理
    statusBar()->showMessage(QString("接线配置完成 - %1").arg(scheme.schemeName), 3000);

    // 清理接线对话框（先断开信号连接）
    if (current_wiring_dialog_) {
        disconnect(current_wiring_dialog_, &WiringGuideDialog::wiringCancelled, nullptr, nullptr);
        current_wiring_dialog_->close();
        current_wiring_dialog_ = nullptr;
    }

    // 生成测试任务
    if (task_generator_) {
        ComponentSpec component = createComponentFromUI();
        component.allocatedPorts = allocatedPorts[component.reference];
        QString taskId = task_generator_->generateTaskFromScheme(scheme, component);
          if (!taskId.isEmpty()) {
            current_task_id_ = taskId;  // 保存任务ID以便后续清理
            single_result_text_->append(QString("接线方案应用成功: %1\n").arg(scheme.schemeName));
            single_result_text_->append(QString("测试任务已生成: %1\n").arg(taskId));

            // 准备执行测试
            if (task_generator_->prepareTaskExecution(taskId)) {
                single_result_text_->append("测试任务准备完成，开始执行测试...\n");

                // 开始故障诊断
                QTimer::singleShot(1000, [this, component]() {
                    single_test_button_->setText("故障诊断中...");
                    fault_diagnostic_->diagnoseComponent(component);
                });
            } else {
                single_result_text_->append("测试任务准备失败\n");
                current_task_id_.clear();  // 清除任务ID
                single_test_button_->setEnabled(true);
                single_test_button_->setText("开始测试");
            }
        } else {
            QMessageBox::warning(this, "错误", "测试任务生成失败");
            single_test_button_->setEnabled(true);
            single_test_button_->setText("开始测试");
        }
    } else {
        QMessageBox::warning(this, "错误", "任务生成器未初始化");
        single_test_button_->setEnabled(true);
        single_test_button_->setText("开始测试");
    }
}

QVector<ComponentSpec> MainWindow::createBatchComponentSpec()
{
    QVector<ComponentSpec> batchComponent;

    for (const auto& step : current_sequence_.steps) {
        if (step.enabled) {
            ComponentSpec component;
            component.reference = step.parameters.value("reference").toString();
            component.type = step.common_type;

            batchComponent.append(component);
        }
    }

    return batchComponent;
}

void MainWindow::startBatchTestWithUnifiedWiring(const QVector<ComponentSpec>& batchComponent)
{
    if (!port_manager_) {
        QMessageBox::warning(this, "错误", "端口管理器未初始化");
        return;
    }

    // 关闭任何现有的接线对话框
    if (current_wiring_dialog_) {
        current_wiring_dialog_->close();
        delete current_wiring_dialog_;
        current_wiring_dialog_ = nullptr;
    }
    if(batchComponent.isEmpty()) {
        QMessageBox::warning(this, "错误", "批量测试组件规格为空");
        return;
    }
    // 创建统一的接线引导对话框
    current_wiring_dialog_ = new WiringGuideDialog(batchComponent[0], port_manager_, diagnostic_manager, this);
    current_wiring_dialog_->setBatchWiring(batchComponent);
    current_wiring_dialog_->setWindowTitle("批量测试 - 统一接线引导");

    // 连接信号 - 使用批量测试专用的槽函数
    connect(current_wiring_dialog_, &WiringGuideDialog::wiringCompleted,
            this, &MainWindow::onBatchWiringCompleted);
    connect(current_wiring_dialog_, &WiringGuideDialog::wiringCancelled,
            [this]() {
                // 用户取消了接线引导，停止批量测试
                batch_testing_active_ = false;
                batch_test_button_->setText("开始批量测试");
                batch_test_button_->setEnabled(true);
                test_progress_->setVisible(false);
                current_wiring_dialog_ = nullptr;
                QMessageBox::information(this, "测试取消", "批量测试已取消");
            });

    // 显示对话框
    current_wiring_dialog_->show();
}

void MainWindow::onBatchWiringCompleted(const WiringScheme& scheme, const QMap<QString, QVector<PortInfo>>& allocatedPorts)
{
    // 批量接线完成后的处理
    statusBar()->showMessage(QString("批量测试接线配置完成 - %1").arg(scheme.schemeId), 3000);
    for(auto& step : current_sequence_.steps) {
        step.allocatedPorts = allocatedPorts[step.component];
    }
      // 生成批量测试任务
    if (task_generator_) {

        ComponentSpec spec;
        spec.reference = "Batch Test";
        QString taskId = task_generator_->generateTaskFromScheme(scheme, spec);

        if (!taskId.isEmpty()) {
            main_batch_task_id_ = taskId;  // 保存主任务ID
            current_task_id_ = taskId;     // 当前任务ID也设置为主任务ID
            qDebug() << "批量测试主任务已生成：" << taskId;

            // 标记统一接线已准备好
            unified_wiring_prepared_ = true;

            // 开始批量测试
            batch_testing_active_ = true;
            current_test_index_ = 0;

            batch_test_button_->setText("停止测试");
            batch_test_button_->setEnabled(true);
            test_progress_->setVisible(true);
            test_progress_->setMaximum(current_sequence_.steps.size());
            test_progress_->setValue(0);
            test_results_.clear();
            updateResultsTable();

            qDebug() << "开始批量测试，共" << current_sequence_.steps.size() << "个测试步骤";

            // 开始第一个测试
            QTimer::singleShot(500, this, &MainWindow::runNextTest);

        } else {
            QMessageBox::warning(this, "错误", "无法生成批量测试任务");
            unified_wiring_prepared_ = false;
        }
    } else {
        QMessageBox::warning(this, "错误", "任务生成器未初始化");
        unified_wiring_prepared_ = false;
    }
      // 清理接线对话框
    if (current_wiring_dialog_) {
        // 先断开信号连接，避免在关闭时触发 wiringCancelled 信号
        disconnect(current_wiring_dialog_, &WiringGuideDialog::wiringCancelled, nullptr, nullptr);
        current_wiring_dialog_->close();
        current_wiring_dialog_ = nullptr;
    }
}

void MainWindow::executeBatchTestWithPreAllocatedWiring(const ComponentSpec& specs)
{
    // 这个方法现在已经不需要了，因为runNextTest已经简化
    // 但为了兼容性保留它，直接调用故障诊断
    qDebug() << "使用预分配接线执行测试：" << specs.reference;

    // 使用延迟执行，避免阻塞UI
    QTimer::singleShot(100, [this, specs]() {
        fault_diagnostic_->diagnoseComponent(specs);
    });
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
        detail_text_->setText(result.diagnosticSummary);
        updateMeasurementPlot(result.metameasurementData);

        batch_thermal_display_->clear();
       if (batch_thermal_display_ && result.thermalData.isValid) {
           updateBatchTestThermalDisplay(result.thermalData);
       }
    }
}

TestStep MainWindow::createTestStepFromComponent(const ComponentSpec& component)
{
    TestStep step;

    step.componentType = getComponentTypeDisplayName(component.type);
    step.common_type = component.type;
    step.testName = component.name + QString("测试 - %1").arg(component.reference);
    step.component = component.reference;
    step.specs = component.params;

    // 通用设置
    step.enabled = true;
    step.timeoutMs = 5000; // 5秒超时

    // 设置测试参数
    step.parameters["reference"] = component.reference;
    step.parameters["description"] = component.description;

    return step;
}


ComponentSpec MainWindow::createComponentFromUI()
{
    ComponentSpec component;
    
    // 获取当前选中的组件类型
    QVector<ComponentType> supportTypes = diagnostic_manager->getSupportedComponentTypes();
    int currentIndex = component_type_combo_->currentIndex();
    if (currentIndex >= 0 && currentIndex < supportTypes.size()) {
        component.type = supportTypes[currentIndex];
    } else {
        QMessageBox::warning(this, "错误", "组件类型不支持");
        return component;
    }
    
    // 获取组件标识
    component.reference = component_ref_edit_->text().trimmed();
    if (component.reference.isEmpty()) {
        QMessageBox::warning(this, "错误", "组件标识不能为空");
        return component;
    }
    
    // 从动态参数控件中读取参数
    auto paramKeys = diagnostic_manager->getRequiredParameters(component.type).keys();
    auto widgets = single_param_widgets_map_.value(component.type);
    
    for (int i = 0; i < widgets.size() && i < paramKeys.size(); ++i) {
        QWidget* w = widgets[i];
        QVariant v;
        
        // 根据控件类型读取值
        if (auto cb = qobject_cast<QCheckBox*>(w)) {
            v = cb->isChecked();
        } else if (auto sb = qobject_cast<QSpinBox*>(w)) {
            v = sb->value();
        } else if (auto dsb = qobject_cast<QDoubleSpinBox*>(w)) {
            v = dsb->value();
        } else if (auto cbx = qobject_cast<QComboBox*>(w)) {
            v = cbx->currentText();
        } else if (auto le = qobject_cast<QLineEdit*>(w)) {
            v = le->text();
        }
        
        component.params.insert(paramKeys[i], v);
    }
    
    // 生成描述
    component.description = QString("%1测试 - %2")
                           .arg(getComponentTypeDisplayName(component.type))
                           .arg(component.reference);
    
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

        QString displayText;
        QTextStream ts(&displayText);
        for (auto it = step.specs.constBegin(); it != step.specs.constEnd(); ++it) {
            ts << it.key() << ": " << it.value().toString() << "\n";
        }
        component_table_->setItem(i, 2, new QTableWidgetItem(displayText));

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


QString MainWindow::getStatusIcon(DeviceStatus status)
{
    switch (status) {
    case DeviceStatus::CONNECTED: return "✅";
    case DeviceStatus::ERROR: return "❌";
    default: return "⚪";
    }
}

void MainWindow::proceedToNextBatchTest()
{
    if (!batch_testing_active_) {
        return;
    }

    // 移动到下一个测试
    current_test_index_++;
    test_progress_->setValue(current_test_index_);

    // 延迟执行下一个测试，给UI时间更新
    QTimer::singleShot(500, this, &MainWindow::runNextTest);
}

void MainWindow::stopBatchTest()
{
    batch_testing_active_ = false;

    // 清理主任务
    if (task_generator_ && !main_batch_task_id_.isEmpty()) {
        task_generator_->finalizeTaskExecution(main_batch_task_id_, false, "STOPPED");
        main_batch_task_id_.clear();
    }
    current_task_id_.clear();

    // 释放所有端口
    if (port_manager_) {
        port_manager_->releaseAllPorts();
    }

    // 清理统一接线方案
    unified_wiring_prepared_ = false;

    batch_test_button_->setText("开始批量测试");
    batch_test_button_->setEnabled(true);
    test_progress_->setVisible(false);

    test_count_label_->setText(QString("测试已停止 (%1/%2)")
                              .arg(current_test_index_)
                              .arg(current_sequence_.steps.size()));

    QMessageBox::information(this, "测试停止",
        QString("批量测试已停止。\n已完成 %1 个测试，剩余 %2 个。")
        .arg(current_test_index_)
        .arg(current_sequence_.steps.size() - current_test_index_));
}

void MainWindow::updateMeasurementPlot(const QMap<QString, QVariant>& measurementData)
{
    if (!measurement_plot_ || measurementData.isEmpty()) {
        return;
    }

    // 清除现有的图表数据
    measurement_plot_->removeAllLines();

    // 遍历测量数据并创建图表线条
    for (auto it = measurementData.constBegin(); it != measurementData.constEnd(); ++it) {
        const QString& dataName = it.key();
        const QVariant& dataValue = it.value();

        QVector<double> yData;

        if (dataValue.canConvert<QVector<double>>()) {
            yData = dataValue.value<QVector<double>>();
        } else if (dataValue.canConvert<QList<double>>()) {
            QList<double> listData = dataValue.value<QList<double>>();
            yData = QVector<double>(listData.begin(), listData.end());
        } else if (dataValue.canConvert<QVector<float>>()) {
            QVector<float> floatData = dataValue.value<QVector<float>>();
            yData.reserve(floatData.size());
            for (float v : floatData) {
                yData.append(static_cast<double>(v));
            }
        } else if (dataValue.canConvert<double>()) {
            yData.append(dataValue.toDouble());
        } else {
            // 尝试解析字符串格式的数据
            const QStringList parts = dataValue.toString().split(',', Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                bool ok = false;
                double v = part.trimmed().toDouble(&ok);
                if (ok) {
                    yData.append(v);
                }
            }
        }

        if (!yData.isEmpty()) {
            // 添加图表线条并更新数据
            QCPGraph* graph = measurement_plot_->addRealTimeLine(dataName);
            if (graph) {
                QVector<QCPGraph*> graphs = { graph };
                QVector<QVector<double>> dataVector = { yData };
                measurement_plot_->updateRealTimeLines(graphs, dataVector);
                qDebug() << "已添加测量数据到图表:" << dataName << "数据点数量:" << yData.size();
            }
        } else {
            qWarning() << "无法解析测量数据:" << dataName << "原始数据:" << dataValue.toString();
        }
    }

    if (measurementData.isEmpty()) {
        qDebug() << "测量数据为空，图表将保持清空状态";
    }

    // 自动调整坐标轴范围
    measurement_plot_->xAxis->rescale();
    measurement_plot_->yAxis->rescale();
    measurement_plot_->replot();
}

void MainWindow::updateSingleTestMeasurementPlot(const QMap<QString, QVariant>& measurementData)
{
    if (!single_measurement_plot_ || measurementData.isEmpty()) {
        return;
    }

    // 清除现有的图表数据
    single_measurement_plot_->removeAllLines();

    // 遍历测量数据并创建图表线条
    for (auto it = measurementData.constBegin(); it != measurementData.constEnd(); ++it) {
        const QString& dataName = it.key();
        const QVariant& dataValue = it.value();

        QVector<double> yData;

        if (dataValue.canConvert<QVector<double>>()) {
            yData = dataValue.value<QVector<double>>();
        } else if (dataValue.canConvert<QList<double>>()) {
            QList<double> listData = dataValue.value<QList<double>>();
            yData = QVector<double>(listData.begin(), listData.end());
        } else if (dataValue.canConvert<QVector<float>>()) {
            QVector<float> floatData = dataValue.value<QVector<float>>();
            yData.reserve(floatData.size());
            for (float v : floatData) {
                yData.append(static_cast<double>(v));
            }
        } else if (dataValue.canConvert<double>()) {
            yData.append(dataValue.toDouble());
        } else {
            // 尝试解析字符串格式的数据
            const QStringList parts = dataValue.toString().split(',', Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                bool ok = false;
                double v = part.trimmed().toDouble(&ok);
                if (ok) {
                    yData.append(v);
                }
            }
        }

        if (!yData.isEmpty()) {
            // 添加图表线条并更新数据
            QCPGraph* graph = single_measurement_plot_->addRealTimeLine(dataName);
            if (graph) {
                QVector<QCPGraph*> graphs = { graph };
                QVector<QVector<double>> dataVector = { yData };
                single_measurement_plot_->updateRealTimeLines(graphs, dataVector);
                qDebug() << "已添加测量数据到图表:" << dataName << "数据点数量:" << yData.size();
            }
        } else {
            qWarning() << "无法解析测量数据:" << dataName << "原始数据:" << dataValue.toString();
        }
    }

    if (measurementData.isEmpty()) {
        qDebug() << "测量数据为空，图表将保持清空状态";
    }

    // 自动调整坐标轴范围
    single_measurement_plot_->xAxis->rescale();
    single_measurement_plot_->yAxis->rescale();
    single_measurement_plot_->replot();
}

void MainWindow::updateSingleTestThermalDisplay(const ThermalData& thermalData)
{
    if (!single_thermal_display_ || !thermalData.isValid) {
        qWarning() << "红外图像显示组件无效或热数据无效";
        return;
    }

    // 数据转换：从ThermalData转换为IRImageDisplay需要的格式
    // 检查图像和原始温度数据
    if (thermalData.thermalImage.empty() || !thermalData.rawTempData
        || thermalData.width == 0 || thermalData.height == 0) {
        qWarning() << "无效的红外图像或温度数据";
        return;
    }

    // 将 cv::Mat 热图转换为 QImage（BGR/GRAY -> RGB）
    cv::Mat rgbMat;
    if (thermalData.thermalImage.channels() == 3) {
        cv::cvtColor(thermalData.thermalImage, rgbMat, cv::COLOR_BGR2RGB);
    } else if (thermalData.thermalImage.channels() == 1) {
        cv::cvtColor(thermalData.thermalImage, rgbMat, cv::COLOR_GRAY2RGB);
    } else {
        rgbMat = thermalData.thermalImage;
    }
    QImage thermalImage(
        rgbMat.data,
        rgbMat.cols,
        rgbMat.rows,
        static_cast<int>(rgbMat.step),
        QImage::Format_RGB888
    );
    thermalImage = thermalImage.copy();  // 拷贝数据以防止悬空

    // 原始温度数据直接拷贝到 vector<uint16_t>
    uint32_t width  = thermalData.width;
    uint32_t height = thermalData.height;
    
    if (!thermalData.rawTempData) {
        qWarning() << "rawTempData指针为空，无法转换温度数据";
        return;
    }
    
    // 准备温度数据：优先使用 temperatureMap 拷贝，避免野指针
    std::vector<uint16_t> tempData;
    int w = 0, h = 0;
    if (!thermalData.temperatureMap.empty()) {
        h = thermalData.temperatureMap.rows;
        w = thermalData.temperatureMap.cols;
        const uint16_t* p = thermalData.temperatureMap.ptr<uint16_t>();
        tempData.assign(p, p + static_cast<size_t>(w) * h);
    } else if (thermalData.rawTempData && thermalData.width > 0 && thermalData.height > 0) {
        // 兼容旧接口
        w = thermalData.width;
        h = thermalData.height;
        size_t dataSize = static_cast<size_t>(w) * h;
        tempData.assign(thermalData.rawTempData, thermalData.rawTempData + dataSize);
    }
    
    // 检查数据大小是否合理（防止过大的内存分配）
    size_t count = static_cast<size_t>(width) * height;
    if (count > 10000000) { // 10M像素限制
        qWarning() << "温度数据大小异常:" << count << "像素，可能存在数据错误";
        return;
    }

    // 验证转换后的数据
    if (tempData.empty()) {
        qWarning() << "温度数据转换后为空";
        return;
    }
    
    // 更新显示组件
    single_thermal_display_->setImage(thermalImage, tempData, width, height);
    batch_thermal_display_->MarkMaxTemp(true);
        
}

void MainWindow::updateBatchTestThermalDisplay(const ThermalData& thermalData)
{
    if (!batch_thermal_display_ || !thermalData.isValid) {
        qWarning() << "批量测试红外图像显示组件无效或热数据无效";
        return;
    }

    if (thermalData.thermalImage.empty() || !thermalData.rawTempData
        || thermalData.width == 0 || thermalData.height == 0) {
        qWarning() << "无效的红外图像或温度数据";
        return;
    }

    // 将 cv::Mat 热图转换为 QImage（BGR/GRAY -> RGB）
    cv::Mat rgbMat;
    if (thermalData.thermalImage.channels() == 3) {
        cv::cvtColor(thermalData.thermalImage, rgbMat, cv::COLOR_BGR2RGB);
    } else if (thermalData.thermalImage.channels() == 1) {
        cv::cvtColor(thermalData.thermalImage, rgbMat, cv::COLOR_GRAY2RGB);
    } else {
        rgbMat = thermalData.thermalImage;
    }
    QImage thermalImage(
        rgbMat.data,
        rgbMat.cols,
        rgbMat.rows,
        static_cast<int>(rgbMat.step),
        QImage::Format_RGB888
    );
    thermalImage = thermalImage.copy();  // 拷贝数据以防止悬空

    // 原始温度数据直接拷贝到 vector<uint16_t>
    uint32_t width  = thermalData.width;
    uint32_t height = thermalData.height;
    
    if (!thermalData.rawTempData) {
        qWarning() << "rawTempData指针为空，无法转换温度数据";
        return;
    }
    
    // 准备温度数据：优先使用 temperatureMap 拷贝，避免野指针
    std::vector<uint16_t> tempData;
    int w = 0, h = 0;
    if (!thermalData.temperatureMap.empty()) {
        h = thermalData.temperatureMap.rows;
        w = thermalData.temperatureMap.cols;
        const uint16_t* p = thermalData.temperatureMap.ptr<uint16_t>();
        tempData.assign(p, p + static_cast<size_t>(w) * h);
    } else if (thermalData.rawTempData && thermalData.width > 0 && thermalData.height > 0) {
        // 兼容旧接口
        w = thermalData.width;
        h = thermalData.height;
        size_t dataSize = static_cast<size_t>(w) * h;
        tempData.assign(thermalData.rawTempData, thermalData.rawTempData + dataSize);
    }
    
    // 检查数据大小是否合理（防止过大的内存分配）
    size_t count = static_cast<size_t>(width) * height;
    if (count > 10000000) { // 10M像素限制
        qWarning() << "温度数据大小异常:" << count << "像素，可能存在数据错误";
        return;
    }

    // 验证转换后的数据
    if (tempData.empty()) {
        qWarning() << "温度数据转换后为空";
        return;
    }
        
    batch_thermal_display_->setImage(thermalImage, tempData, width, height);
    batch_thermal_display_->MarkMaxTemp(true);
}

void MainWindow::onDiagnoseComponents(const QList<ComponentSpec>& specs)
{
    if(specs.isEmpty()) return;

    for(const auto& spec : specs)
    {
        TestStep step = createTestStepFromComponent(spec);
        bool isExist = false;
        for (const TestStep &existingStep : current_sequence_.steps) {
            if (existingStep.component == step.component) {
                QMessageBox::warning(this, "添加失败",
                                    QString("测试步骤 '%1' 已存在！").arg(step.component));
                isExist = true;
                break;
            }
        }
        if(!isExist)
        {
            sequence_manager_->addTestStep(current_sequence_, step);
        }
    }
    populateComponentTable();
    main_tabs_->setCurrentWidget(batch_test_page_);
}
