#include "wiringguidedialog.h"
#include <QApplication>
#include <QScreen>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

WiringGuideDialog::WiringGuideDialog(const ComponentSpec& component, QWidget *parent)
    : QDialog(parent)
    , component_(component)
    , resource_manager_(new WiringResourceManager(this))
    , schematic_widget_(nullptr)
    , current_step_index_(0)
    , test_voltage_(0.0)
    , test_current_(0.0)
    , use_dmm_(true)
    , wiring_completed_(false)
{
    try {
        setWindowTitle(QString("接线引导 - %1").arg(component_.reference));
        setMinimumSize(1000, 700);
        resize(1200, 800);
        
        qDebug() << "开始设置UI...";
        setupUI();
        
        qDebug() << "开始连接信号...";
        connectSignals();
        
        // 初始化资源管理器
        qDebug() << "开始初始化资源管理器...";
        if (!resource_manager_->initializeResources()) {
            QMessageBox::warning(this, "警告", "无法初始化测试资源");
        }
        
        qDebug() << "开始更新可用资源...";
        updateAvailableResources();
        
        qDebug() << "WiringGuideDialog 构造完成";
    } catch (const std::exception& e) {
        qDebug() << "WiringGuideDialog 构造失败:" << e.what();
        QMessageBox::critical(this, "错误", QString("接线引导初始化失败: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "WiringGuideDialog 构造失败: 未知错误";
        QMessageBox::critical(this, "错误", "接线引导初始化失败: 未知错误");
    }
}

WiringGuideDialog::~WiringGuideDialog()
{
    if (resource_manager_) {
        resource_manager_->releaseAllResources();
    }
}

void WiringGuideDialog::setupUI()
{
    try {
        QVBoxLayout* main_layout = new QVBoxLayout(this);
        
        // 创建标题区域
        QLabel* title_label = new QLabel(QString("元件 %1 测试接线引导").arg(component_.reference));
        QFont title_font = title_label->font();
        title_font.setPointSize(16);
        title_font.setBold(true);
        title_label->setFont(title_font);
        title_label->setAlignment(Qt::AlignCenter);
        main_layout->addWidget(title_label);
        
        // 创建选项卡控件
        tab_widget_ = new QTabWidget();
        main_layout->addWidget(tab_widget_);
        
        qDebug() << "设置资源选择页面...";
        setupResourceSelectionPage();
        
        qDebug() << "设置接线引导页面...";
        setupWiringGuidePage();
        
        qDebug() << "设置验证页面...";
        setupVerificationPage();
        
        qDebug() << "设置原理图显示...";
        setupSchematicDisplay();
        
        // 创建按钮区域
        QHBoxLayout* button_layout = new QHBoxLayout();
        
        QPushButton* reset_btn = new QPushButton("重置");
        QPushButton* help_btn = new QPushButton("帮助");
        QPushButton* cancel_btn = new QPushButton("取消");
        QPushButton* ok_btn = new QPushButton("确定");
        
        button_layout->addWidget(reset_btn);
        button_layout->addStretch();
        button_layout->addWidget(help_btn);
        button_layout->addWidget(cancel_btn);
        button_layout->addWidget(ok_btn);
        
        main_layout->addLayout(button_layout);
        
        // 连接按钮信号
        connect(reset_btn, &QPushButton::clicked, this, &WiringGuideDialog::onResetWiring);
        connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
        connect(ok_btn, &QPushButton::clicked, this, &QDialog::accept);
        
        qDebug() << "WiringGuideDialog::setupUI 完成";
    } catch (const std::exception& e) {
        qDebug() << "WiringGuideDialog::setupUI 失败:" << e.what();
        throw;
    }
}

void WiringGuideDialog::setupResourceSelectionPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    
    // 测试参数显示
    QGroupBox* test_params_group = new QGroupBox("测试参数");
    QFormLayout* params_layout = new QFormLayout(test_params_group);
    
    QLabel* voltage_label = new QLabel(QString("%1 V").arg(test_voltage_));
    QLabel* current_label = new QLabel(QString("%1 A").arg(test_current_));
    QLabel* dmm_label = new QLabel(use_dmm_ ? "是" : "否");
    
    params_layout->addRow("测试电压:", voltage_label);
    params_layout->addRow("测试电流:", current_label);
    params_layout->addRow("使用万用表:", dmm_label);
    layout->addWidget(test_params_group);
    
    // 创建分割器
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    layout->addWidget(splitter);
    
    // 左侧：资源选择
    QWidget* left_widget = new QWidget();
    QVBoxLayout* left_layout = new QVBoxLayout(left_widget);
    
    // 电源输出选择
    power_selection_group_ = new QGroupBox("电源输出端口");
    QFormLayout* power_layout = new QFormLayout(power_selection_group_);
    
    voltage_source_combo_ = new QComboBox();
    current_source_combo_ = new QComboBox();
    
    power_layout->addRow("电压源:", voltage_source_combo_);
    power_layout->addRow("电流源:", current_source_combo_);
    left_layout->addWidget(power_selection_group_);
    
    // 万用表选择
    dmm_selection_group_ = new QGroupBox("数字万用表");
    QFormLayout* dmm_layout = new QFormLayout(dmm_selection_group_);
    
    dmm_channel_combo_ = new QComboBox();
    dmm_layout->addRow("万用表通道:", dmm_channel_combo_);
    left_layout->addWidget(dmm_selection_group_);
    
    splitter->addWidget(left_widget);
    
    // 右侧：I/O端口选择
    QWidget* right_widget = new QWidget();
    QVBoxLayout* right_layout = new QVBoxLayout(right_widget);
    
    // 数字I/O
    digital_io_group_ = new QGroupBox("数字I/O端口");
    QHBoxLayout* digital_layout = new QHBoxLayout(digital_io_group_);
    
    QVBoxLayout* do_layout = new QVBoxLayout();
    do_layout->addWidget(new QLabel("数字输出:"));
    digital_output_list_ = new QListWidget();
    digital_output_list_->setSelectionMode(QAbstractItemView::MultiSelection);
    do_layout->addWidget(digital_output_list_);
    digital_layout->addLayout(do_layout);
    
    QVBoxLayout* di_layout = new QVBoxLayout();
    di_layout->addWidget(new QLabel("数字输入:"));
    digital_input_list_ = new QListWidget();
    digital_input_list_->setSelectionMode(QAbstractItemView::MultiSelection);
    di_layout->addWidget(digital_input_list_);
    digital_layout->addLayout(di_layout);
    
    right_layout->addWidget(digital_io_group_);
    
    // 模拟I/O
    analog_io_group_ = new QGroupBox("模拟I/O端口");
    QHBoxLayout* analog_layout = new QHBoxLayout(analog_io_group_);
    
    QVBoxLayout* ao_layout = new QVBoxLayout();
    ao_layout->addWidget(new QLabel("模拟输出:"));
    analog_output_list_ = new QListWidget();
    analog_output_list_->setSelectionMode(QAbstractItemView::MultiSelection);
    ao_layout->addWidget(analog_output_list_);
    analog_layout->addLayout(ao_layout);
    
    QVBoxLayout* ai_layout = new QVBoxLayout();
    ai_layout->addWidget(new QLabel("模拟输入:"));
    analog_input_list_ = new QListWidget();
    analog_input_list_->setSelectionMode(QAbstractItemView::MultiSelection);
    ai_layout->addWidget(analog_input_list_);
    analog_layout->addLayout(ai_layout);
    
    right_layout->addWidget(analog_io_group_);
    
    splitter->addWidget(right_widget);
    splitter->setSizes({400, 600});
    
    tab_widget_->addTab(page, "资源选择");
}

void WiringGuideDialog::setupWiringGuidePage()
{
    QWidget* page = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(page);
    
    // 左侧：接线步骤列表
    QVBoxLayout* left_layout = new QVBoxLayout();
    
    QLabel* steps_label = new QLabel("接线步骤:");
    left_layout->addWidget(steps_label);
    
    wiring_steps_list_ = new QListWidget();
    left_layout->addWidget(wiring_steps_list_);
    
    // 步骤控制按钮
    QHBoxLayout* step_control_layout = new QHBoxLayout();
    previous_step_btn_ = new QPushButton("上一步");
    step_completed_btn_ = new QPushButton("完成此步");
    next_step_btn_ = new QPushButton("下一步");
    
    step_control_layout->addWidget(previous_step_btn_);
    step_control_layout->addWidget(step_completed_btn_);
    step_control_layout->addWidget(next_step_btn_);
    left_layout->addLayout(step_control_layout);
    
    // 进度条
    wiring_progress_ = new QProgressBar();
    left_layout->addWidget(wiring_progress_);
    
    QWidget* left_widget = new QWidget();
    left_widget->setLayout(left_layout);
    left_widget->setMaximumWidth(300);
    layout->addWidget(left_widget);
    
    // 右侧：当前步骤详情和图示
    QVBoxLayout* right_layout = new QVBoxLayout();
    
    QLabel* detail_label = new QLabel("当前步骤详情:");
    right_layout->addWidget(detail_label);
    
    current_step_detail_ = new QTextEdit();
    current_step_detail_->setMaximumHeight(150);
    right_layout->addWidget(current_step_detail_);
    
    QLabel* diagram_label = new QLabel("接线图示:");
    right_layout->addWidget(diagram_label);
    
    wiring_diagram_view_ = new QGraphicsView();
    wiring_diagram_scene_ = new QGraphicsScene();
    wiring_diagram_view_->setScene(wiring_diagram_scene_);
    right_layout->addWidget(wiring_diagram_view_);
    
    QWidget* right_widget = new QWidget();
    right_widget->setLayout(right_layout);
    layout->addWidget(right_widget);
    
    tab_widget_->addTab(page, "接线引导");
}

void WiringGuideDialog::setupVerificationPage()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    
    // 连接验证表格
    QLabel* table_label = new QLabel("连接验证:");
    layout->addWidget(table_label);
    
    connection_table_ = new QTableWidget();
    connection_table_->setColumnCount(5);
    QStringList headers = {"资源端口", "目标连接点", "导线颜色", "状态", "验证"};
    connection_table_->setHorizontalHeaderLabels(headers);
    connection_table_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(connection_table_);
    
    // 验证按钮
    QHBoxLayout* verify_layout = new QHBoxLayout();
    validate_btn_ = new QPushButton("验证连接");
    generate_task_btn_ = new QPushButton("生成测试任务");
    generate_task_btn_->setEnabled(false);
    
    verify_layout->addWidget(validate_btn_);
    verify_layout->addStretch();
    verify_layout->addWidget(generate_task_btn_);
    layout->addLayout(verify_layout);
    
    // 验证结果
    QLabel* result_label = new QLabel("验证结果:");
    layout->addWidget(result_label);
    
    validation_result_ = new QTextEdit();
    validation_result_->setMaximumHeight(150);
    layout->addWidget(validation_result_);
    
    tab_widget_->addTab(page, "连接验证");
}

void WiringGuideDialog::setupSchematicDisplay()
{
    try {
        // 暂时使用简单的文本标签代替复杂的WiringSchematic来避免崩溃
        QWidget* placeholder = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(placeholder);
        
        QLabel* info_label = new QLabel("原理图显示功能");
        info_label->setAlignment(Qt::AlignCenter);
        info_label->setStyleSheet("font-size: 14px; color: #666; padding: 50px;");
        layout->addWidget(info_label);
        
        QLabel* status_label = new QLabel("此功能正在开发中...");
        status_label->setAlignment(Qt::AlignCenter);
        status_label->setStyleSheet("font-size: 12px; color: #999;");
        layout->addWidget(status_label);
        
        tab_widget_->addTab(placeholder, "原理图");
        
        qDebug() << "WiringGuideDialog::setupSchematicDisplay 使用占位符完成";
    } catch (const std::exception& e) {
        qDebug() << "WiringGuideDialog::setupSchematicDisplay 失败:" << e.what();
        
        // 创建最简单的占位符
        QWidget* simple_placeholder = new QWidget();
        QLabel* error_label = new QLabel("原理图功能暂时不可用");
        QVBoxLayout* error_layout = new QVBoxLayout(simple_placeholder);
        error_layout->addWidget(error_label);
        tab_widget_->addTab(simple_placeholder, "原理图");
    }
}

void WiringGuideDialog::connectSignals()
{
    // 资源选择信号
    connect(voltage_source_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WiringGuideDialog::onResourceSelectionChanged);
    connect(current_source_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WiringGuideDialog::onResourceSelectionChanged);
    connect(dmm_channel_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WiringGuideDialog::onResourceSelectionChanged);
    
    // 接线步骤信号
    connect(wiring_steps_list_, &QListWidget::currentRowChanged,
            this, &WiringGuideDialog::onPortSelectionChanged);
    connect(step_completed_btn_, &QPushButton::clicked,
            [this]() { onWiringStepCompleted(current_step_index_); });
    connect(previous_step_btn_, &QPushButton::clicked,
            [this]() { 
                if (current_step_index_ > 0) {
                    current_step_index_--;
                    updateWiringInstructions();
                }
            });
    connect(next_step_btn_, &QPushButton::clicked,
            [this]() {
                if (current_step_index_ < wiring_steps_.size() - 1) {
                    current_step_index_++;
                    updateWiringInstructions();
                }
            });
    
    // 验证信号
    connect(validate_btn_, &QPushButton::clicked, this, &WiringGuideDialog::onValidateConnections);
    connect(generate_task_btn_, &QPushButton::clicked, this, &WiringGuideDialog::onGenerateTestTask);
    
    // 资源管理器信号
    connect(resource_manager_, &WiringResourceManager::errorOccurred,
            [this](const QString& error) {
                QMessageBox::warning(this, "错误", error);
            });
}

void WiringGuideDialog::setTestParameters(double voltage, double current, bool useDMM)
{
    try {
        qDebug() << "setTestParameters 被调用: voltage=" << voltage << "current=" << current << "useDMM=" << useDMM;
        
        test_voltage_ = voltage;
        test_current_ = current;
        use_dmm_ = useDMM;
        
        // 检查resource_manager_是否可用
        if (!resource_manager_) {
            qDebug() << "setTestParameters: resource_manager_ 为空";
            return;
        }
        
        // 生成接线配置
        current_config_ = resource_manager_->generateWiringConfig(
            component_.reference, voltage, current, useDMM);
        
        // 生成接线步骤
        wiring_steps_ = resource_manager_->generateWiringSteps(current_config_);
        
        qDebug() << "setTestParameters: 生成了" << wiring_steps_.size() << "个接线步骤";
        
        // 只有在步骤不为空时才更新UI
        if (!wiring_steps_.isEmpty()) {
            updateAvailableResources();
            updateWiringInstructions();
        } else {
            qDebug() << "setTestParameters: 接线步骤为空，跳过UI更新";
        }
        
        qDebug() << "setTestParameters 完成";
    } catch (const std::exception& e) {
        qDebug() << "setTestParameters 失败:" << e.what();
    } catch (...) {
        qDebug() << "setTestParameters 失败: 未知错误";
    }
}

WiringConfiguration WiringGuideDialog::getWiringConfiguration() const
{
    return current_config_;
}

void WiringGuideDialog::updateAvailableResources()
{
    try {
        qDebug() << "开始更新可用资源...";
        
        // 检查控件是否已初始化
        if (!voltage_source_combo_ || !current_source_combo_ || !dmm_channel_combo_ ||
            !digital_output_list_ || !digital_input_list_ || 
            !analog_output_list_ || !analog_input_list_) {
            qDebug() << "updateAvailableResources: 控件未初始化";
            return;
        }
        
        if (!resource_manager_) {
            qDebug() << "updateAvailableResources: resource_manager_ 为空";
            return;
        }
        
        qDebug() << "清空控件...";
        // 更新电源端口
        voltage_source_combo_->clear();
        current_source_combo_->clear();
        
        qDebug() << "获取电源端口...";
        auto power_ports = resource_manager_->getAvailableResources(ResourceType::POWER_OUTPUT);
        qDebug() << "找到" << power_ports.size() << "个电源端口";
        
        for (const auto& port : power_ports) {
            if (port.max_voltage >= test_voltage_) {
                voltage_source_combo_->addItem(port.name, port.port_id);
            }
            if (port.max_current >= test_current_) {
                current_source_combo_->addItem(port.name, port.port_id);
            }
        }        
        qDebug() << "获取万用表端口...";
        // 更新万用表通道
        dmm_channel_combo_->clear();
        auto dmm_ports = resource_manager_->getAvailableResources(ResourceType::DMM);
        qDebug() << "找到" << dmm_ports.size() << "个万用表端口";
        
        for (const auto& port : dmm_ports) {
            dmm_channel_combo_->addItem(port.name, port.port_id);
        }
        
        qDebug() << "获取数字输出端口...";
        // 更新数字I/O
        digital_output_list_->clear();
        auto digital_outputs = resource_manager_->getAvailableResources(ResourceType::DIGITAL_OUTPUT);
        qDebug() << "找到" << digital_outputs.size() << "个数字输出端口";
        
        for (const auto& port : digital_outputs) {
            QListWidgetItem* item = new QListWidgetItem(port.name);
            item->setData(Qt::UserRole, port.port_id);
            digital_output_list_->addItem(item);
        }
        
        qDebug() << "获取数字输入端口...";
        digital_input_list_->clear();
        auto digital_inputs = resource_manager_->getAvailableResources(ResourceType::DIGITAL_INPUT);
        qDebug() << "找到" << digital_inputs.size() << "个数字输入端口";
        
        for (const auto& port : digital_inputs) {
            QListWidgetItem* item = new QListWidgetItem(port.name);
            item->setData(Qt::UserRole, port.port_id);
            digital_input_list_->addItem(item);
        }
        
        qDebug() << "获取模拟输出端口...";
        // 更新模拟I/O
        analog_output_list_->clear();
        auto analog_outputs = resource_manager_->getAvailableResources(ResourceType::ANALOG_OUTPUT);
        qDebug() << "找到" << analog_outputs.size() << "个模拟输出端口";
        
        for (const auto& port : analog_outputs) {
            QListWidgetItem* item = new QListWidgetItem(port.name);
            item->setData(Qt::UserRole, port.port_id);
            analog_output_list_->addItem(item);
        }
        
        qDebug() << "获取模拟输入端口...";
        analog_input_list_->clear();
        auto analog_inputs = resource_manager_->getAvailableResources(ResourceType::ANALOG_INPUT);
        qDebug() << "找到" << analog_inputs.size() << "个模拟输入端口";
        
        for (const auto& port : analog_inputs) {
            QListWidgetItem* item = new QListWidgetItem(port.name);
            item->setData(Qt::UserRole, port.port_id);
            analog_input_list_->addItem(item);
        }
        
        qDebug() << "updateAvailableResources 完成";
    } catch (const std::exception& e) {
        qDebug() << "updateAvailableResources 失败:" << e.what();
        QMessageBox::warning(this, "错误", QString("更新可用资源失败: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "updateAvailableResources 失败: 未知错误";
        QMessageBox::warning(this, "错误", "更新可用资源失败: 未知错误");
    }
}

void WiringGuideDialog::updateWiringInstructions()
{
    try {
        static bool updating = false;  // 防止递归调用
        if (updating) {
            qDebug() << "updateWiringInstructions: 已经在更新中，跳过";
            return;
        }
        updating = true;
        
        if (wiring_steps_.isEmpty() || current_step_index_ >= wiring_steps_.size()) {
            qDebug() << "updateWiringInstructions: 无效的步骤索引或空步骤列表";
            updating = false;
            return;
        }
        
        // 检查控件
        if (!wiring_steps_list_ || !current_step_detail_ || !wiring_progress_ ||
            !previous_step_btn_ || !next_step_btn_) {
            qDebug() << "updateWiringInstructions: 控件未初始化";
            updating = false;
            return;
        }
        
        qDebug() << "更新接线指令，当前步骤:" << current_step_index_;
        
        // 临时断开信号以防止循环
        disconnect(wiring_steps_list_, &QListWidget::currentRowChanged,
                   this, &WiringGuideDialog::onPortSelectionChanged);
        
        // 更新步骤列表
        wiring_steps_list_->clear();
        for (int i = 0; i < wiring_steps_.size(); ++i) {
            const auto& step = wiring_steps_[i];
            QListWidgetItem* item = new QListWidgetItem(
                QString("步骤 %1: %2").arg(i + 1).arg(step.title));
            
            if (i < current_step_index_) {
                item->setBackground(QBrush(QColor(144, 238, 144))); // 已完成 - 浅绿色
            } else if (i == current_step_index_) {
                item->setBackground(QBrush(QColor(255, 255, 224))); // 当前步骤 - 浅黄色
            }
            
            wiring_steps_list_->addItem(item);
        }
        wiring_steps_list_->setCurrentRow(current_step_index_);
        
        // 重新连接信号
        connect(wiring_steps_list_, &QListWidget::currentRowChanged,
                this, &WiringGuideDialog::onPortSelectionChanged);
        
        // 更新当前步骤详情
        const auto& current_step = wiring_steps_[current_step_index_];
        QString detail_text = QString("<b>%1</b><br><br>%2")
                             .arg(current_step.title)
                             .arg(current_step.description);
        
        if (!current_step.warning.isEmpty()) {
            detail_text += QString("<br><br><font color='red'><b>⚠️ 警告:</b> %1</font>")
                          .arg(current_step.warning);
        }
        
        current_step_detail_->setHtml(detail_text);
        
        // 更新进度条
        wiring_progress_->setMaximum(wiring_steps_.size());
        wiring_progress_->setValue(current_step_index_ + 1);
        
        // 更新按钮状态
        previous_step_btn_->setEnabled(current_step_index_ > 0);
        next_step_btn_->setEnabled(current_step_index_ < wiring_steps_.size() - 1);
        
        // 更新连接图表（不调用updateConnectionDiagram，因为它也可能触发信号）
        if (wiring_diagram_scene_ && !current_step.connections.isEmpty()) {
            try {
                wiring_diagram_scene_->clear();
                
                // 绘制简化的连接图
                double y_offset = 0;
                const double line_height = 60;
                const double margin = 20;
                
                for (const auto& connection : current_step.connections) {
                    // 绘制资源端口
                    QRectF source_rect(margin, y_offset, 150, 40);
                    wiring_diagram_scene_->addRect(source_rect, 
                        QPen(Qt::black, 2), QBrush(QColor(173, 216, 230)));
                    
                    QGraphicsTextItem* source_text = wiring_diagram_scene_->addText(
                        connection.resource_port.name, QFont("Arial", 10));
                    source_text->setPos(source_rect.center().x() - source_text->boundingRect().width()/2,
                                       source_rect.center().y() - source_text->boundingRect().height()/2);
                    
                    y_offset += line_height + margin;
                }
                
                if (wiring_diagram_view_) {
                    wiring_diagram_view_->fitInView(wiring_diagram_scene_->itemsBoundingRect(), Qt::KeepAspectRatio);
                }
            } catch (...) {
                // 忽略图形更新错误
            }
        }
        
        updating = false;
        qDebug() << "updateWiringInstructions 完成";
    } catch (const std::exception& e) {
        qDebug() << "updateWiringInstructions 失败:" << e.what();
    } catch (...) {
        qDebug() << "updateWiringInstructions 失败: 未知错误";
    }
}

void WiringGuideDialog::updateConnectionDiagram()
{
    try {
        if (!wiring_diagram_scene_ || wiring_steps_.isEmpty() || 
            current_step_index_ >= wiring_steps_.size()) {
            qDebug() << "updateConnectionDiagram: 场景或步骤无效";
            return;
        }
        
        qDebug() << "更新连接图表...";
        wiring_diagram_scene_->clear();
        
        const auto& current_step = wiring_steps_[current_step_index_];
        
        // 绘制简化的连接图
        double y_offset = 0;
        const double line_height = 60;
        const double margin = 20;
        
        for (const auto& connection : current_step.connections) {
            // 绘制资源端口
            QRectF source_rect(margin, y_offset, 150, 40);
            QGraphicsRectItem* source_item = wiring_diagram_scene_->addRect(source_rect, 
                QPen(Qt::black, 2), QBrush(QColor(173, 216, 230)));
            
            QGraphicsTextItem* source_text = wiring_diagram_scene_->addText(
                connection.resource_port.name, QFont("Arial", 10));
            source_text->setPos(source_rect.center().x() - source_text->boundingRect().width()/2,
                               source_rect.center().y() - source_text->boundingRect().height()/2);
            
            // 绘制连接线
            QLineF wire_line(source_rect.right(), source_rect.center().y(),
                            source_rect.right() + 100, source_rect.center().y());
            QGraphicsLineItem* wire_item = wiring_diagram_scene_->addLine(wire_line,
                QPen(QColor(connection.wire_color), 3));
            
            // 绘制目标连接点
            QRectF target_rect(source_rect.right() + 120, y_offset, 150, 40);
            QGraphicsRectItem* target_item = wiring_diagram_scene_->addRect(target_rect,
                QPen(Qt::black, 2), QBrush(QColor(255, 182, 193)));
            
            QGraphicsTextItem* target_text = wiring_diagram_scene_->addText(
                connection.target_point.label, QFont("Arial", 10));
            target_text->setPos(target_rect.center().x() - target_text->boundingRect().width()/2,
                               target_rect.center().y() - target_text->boundingRect().height()/2);
            
            // 添加连接说明
            QGraphicsTextItem* instruction_text = wiring_diagram_scene_->addText(
                connection.instruction, QFont("Arial", 9));
            instruction_text->setPos(margin, y_offset + 45);
            
            y_offset += line_height + margin;
        }
        
        if (wiring_diagram_view_) {
            wiring_diagram_view_->fitInView(wiring_diagram_scene_->itemsBoundingRect(), Qt::KeepAspectRatio);
        }
        
        qDebug() << "updateConnectionDiagram 完成";
    } catch (const std::exception& e) {
        qDebug() << "updateConnectionDiagram 失败:" << e.what();
    } catch (...) {
        qDebug() << "updateConnectionDiagram 失败: 未知错误";
    }
}

void WiringGuideDialog::onResourceSelectionChanged()
{
    validateUserSelections();
}

void WiringGuideDialog::onPortSelectionChanged()
{
    try {
        static bool handling = false;  // 防止递归调用
        if (handling) {
            qDebug() << "onPortSelectionChanged: 已经在处理中，跳过";
            return;
        }
        handling = true;
        
        int selected_step = wiring_steps_list_->currentRow();
        qDebug() << "onPortSelectionChanged: 选中步骤=" << selected_step << "当前步骤=" << current_step_index_;
        
        if (selected_step >= 0 && selected_step < wiring_steps_.size() && selected_step != current_step_index_) {
            current_step_index_ = selected_step;
            updateWiringInstructions();
        }
        
        handling = false;
        qDebug() << "onPortSelectionChanged 完成";
    } catch (const std::exception& e) {
        qDebug() << "onPortSelectionChanged 失败:" << e.what();
    } catch (...) {
        qDebug() << "onPortSelectionChanged 失败: 未知错误";
    }
}

void WiringGuideDialog::onWiringStepCompleted(int step)
{
    if (step >= 0 && step < wiring_steps_.size()) {
        // 标记步骤为已完成
        // 这里可以添加步骤完成的验证逻辑
        
        if (step < wiring_steps_.size() - 1) {
            current_step_index_++;
            updateWiringInstructions();
        } else {
            // 所有步骤完成
            wiring_completed_ = true;
            tab_widget_->setCurrentIndex(2); // 切换到验证页面
            onValidateConnections();
        }
    }
}

void WiringGuideDialog::onValidateConnections()
{
    // 填充连接验证表格
    connection_table_->setRowCount(0);
    
    for (const auto& step : wiring_steps_) {
        for (const auto& connection : step.connections) {
            int row = connection_table_->rowCount();
            connection_table_->insertRow(row);
            
            connection_table_->setItem(row, 0, 
                new QTableWidgetItem(connection.resource_port.name));
            connection_table_->setItem(row, 1, 
                new QTableWidgetItem(connection.target_point.label));
            connection_table_->setItem(row, 2, 
                new QTableWidgetItem(connection.wire_color));
            
            QString status = connection.is_completed ? "已完成" : "未完成";
            QTableWidgetItem* status_item = new QTableWidgetItem(status);
            if (connection.is_completed) {
                status_item->setBackground(QBrush(QColor(144, 238, 144)));
            } else {
                status_item->setBackground(QBrush(QColor(255, 192, 203)));
            }
            connection_table_->setItem(row, 3, status_item);
            
            QPushButton* verify_btn = new QPushButton("验证");
            connection_table_->setCellWidget(row, 4, verify_btn);
        }
    }
    
    // 执行验证逻辑
    QStringList errors;
    bool is_valid = resource_manager_->validateWiringConfiguration(current_config_, errors);
    
    QString result_text;
    if (is_valid) {
        result_text = "<font color='green'><b>✓ 连接验证通过</b></font><br>";
        result_text += "所有连接都符合要求，可以开始测试。";
        generate_task_btn_->setEnabled(true);
    } else {
        result_text = "<font color='red'><b>✗ 连接验证失败</b></font><br>";
        result_text += "发现以下问题:<br>";
        for (const QString& error : errors) {
            result_text += QString("• %1<br>").arg(error);
        }
        generate_task_btn_->setEnabled(false);
    }
    
    validation_result_->setHtml(result_text);
}

void WiringGuideDialog::onGenerateTestTask()
{
    // 生成最终的测试任务配置
    QString config_id = QUuid::createUuid().toString();
    current_config_.config_id = config_id;
    current_config_.created_time = QDateTime::currentDateTime();
    current_config_.notes = QString("为元件 %1 生成的测试接线配置").arg(component_.reference);
    
    // 分配选定的资源
    QList<ResourcePort> selected_resources;
    
    // 添加电源资源
    if (voltage_source_combo_->currentIndex() >= 0) {
        int port_id = voltage_source_combo_->currentData().toInt();
        ResourcePort port = resource_manager_->getResourcePort(port_id);
        selected_resources.append(port);
        resource_manager_->allocateResource(port);
    }
    
    if (current_source_combo_->currentIndex() >= 0) {
        int port_id = current_source_combo_->currentData().toInt();
        ResourcePort port = resource_manager_->getResourcePort(port_id);
        selected_resources.append(port);
        resource_manager_->allocateResource(port);
    }
    
    // 添加万用表资源
    if (dmm_channel_combo_->currentIndex() >= 0) {
        int port_id = dmm_channel_combo_->currentData().toInt();
        ResourcePort port = resource_manager_->getResourcePort(port_id);
        selected_resources.append(port);
        resource_manager_->allocateResource(port);
    }
    
    current_config_.used_resources = selected_resources;
    
    // 保存配置
    QString save_path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) 
                       + "/FaultDetect/WiringConfigs/";
    QDir().mkpath(save_path);
    
    QString file_path = save_path + QString("%1_%2.json")
                       .arg(component_.reference)
                       .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    
    if (resource_manager_->saveConfiguration(current_config_, file_path)) {
        validation_result_->append(QString("<br><font color='blue'><b>测试任务已生成</b></font><br>"
                                          "配置文件已保存至: %1").arg(file_path));
        
        QMessageBox::information(this, "成功", 
            QString("测试任务生成成功!\n配置ID: %1\n文件路径: %2")
            .arg(config_id).arg(file_path));
    } else {
        QMessageBox::warning(this, "错误", "无法保存配置文件");
    }
}

void WiringGuideDialog::onResetWiring()
{
    if (QMessageBox::question(this, "确认重置", 
                             "确定要重置所有接线配置吗？这将清除当前的所有设置。",
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        
        // 释放所有已分配的资源
        resource_manager_->releaseAllResources();
        
        // 重置状态
        current_step_index_ = 0;
        wiring_completed_ = false;
        current_config_ = WiringConfiguration();
        wiring_steps_.clear();
        
        // 重新初始化
        updateAvailableResources();
        tab_widget_->setCurrentIndex(0);
        
        // 清空界面
        wiring_steps_list_->clear();
        current_step_detail_->clear();
        wiring_diagram_scene_->clear();
        connection_table_->setRowCount(0);
        validation_result_->clear();
        
        generate_task_btn_->setEnabled(false);
    }
}

void WiringGuideDialog::onShowSchematic()
{
    tab_widget_->setCurrentIndex(3); // 切换到原理图页面
}

void WiringGuideDialog::validateUserSelections()
{
    // 验证用户的资源选择是否合理
    bool voltage_selected = voltage_source_combo_->currentIndex() >= 0;
    bool current_selected = current_source_combo_->currentIndex() >= 0;
    bool dmm_selected = !use_dmm_ || dmm_channel_combo_->currentIndex() >= 0;
    
    // 根据验证结果启用/禁用相关控件
    // 这里可以添加更详细的验证逻辑
}
