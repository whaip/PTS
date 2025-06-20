#include "pcbidentificationdialog.h"
#include <QApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QDateTime>
#include <QInputDialog>
#include <QFormLayout>
#include <QIcon>

PCBIdentificationDialog::PCBIdentificationDialog(QWidget *parent)
    : QDialog(parent)
    , pcb_identifier_(nullptr)
{
    setupUI();
    connectSignals();
    
    // 设置窗口属性
    setWindowTitle("PCB板卡型号识别系统");
    setWindowIcon(QIcon(":/icons/pcb_icon.png"));
    resize(1200, 800);
    
    // 居中显示
    QScreen* screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
    
    // 初始化状态
    updateStatusMessage("PCB识别系统已启动，请加载识别器");
    addLogMessage("系统初始化完成");
}

PCBIdentificationDialog::~PCBIdentificationDialog()
{
    if (pcb_identifier_) {
        addLogMessage("正在保存配置...");
    }
}

void PCBIdentificationDialog::setPCBIdentifier(PCBIdentifier* identifier)
{
    if (pcb_identifier_) {
        // 断开旧连接
        disconnect(pcb_identifier_, nullptr, this, nullptr);
    }
    
    pcb_identifier_ = identifier;
    
    if (pcb_identifier_) {
        // 连接信号
        connect(pcb_identifier_, &PCBIdentifier::identificationCompleted,
                this, &PCBIdentificationDialog::onIdentificationCompleted);
        connect(pcb_identifier_, &PCBIdentifier::identificationProgress,
                this, &PCBIdentificationDialog::onIdentificationProgress);
        connect(pcb_identifier_, &PCBIdentifier::errorOccurred,
                this, &PCBIdentificationDialog::onErrorOccurred);
        connect(pcb_identifier_, &PCBIdentifier::databaseUpdated,
                this, &PCBIdentificationDialog::updateModelList);
        
        // 更新界面状态
        updateConfigurationDisplay();
        updateModelList();
        updateStatusMessage("PCB识别器已连接，系统就绪");
        addLogMessage("PCB识别器连接成功");
    }
}

void PCBIdentificationDialog::setupUI()
{
    // 创建主布局
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    
    // 创建标签页
    main_tabs_ = new QTabWidget();
    main_layout->addWidget(main_tabs_);
    
    // 设置页面
    setupIdentificationPage();
    setupTemplatePage();
    setupConfigPage();
    
    // 状态栏
    status_label_ = new QLabel("系统初始化中...");
    status_label_->setStyleSheet("QLabel { padding: 5px; border: 1px solid gray; }");
    main_layout->addWidget(status_label_);
}

void PCBIdentificationDialog::setupIdentificationPage()
{
    identification_page_ = new QWidget();
    main_tabs_->addTab(identification_page_, "PCB识别");
    
    QHBoxLayout* main_layout = new QHBoxLayout(identification_page_);
    
    // 左侧：控制面板
    QVBoxLayout* left_layout = new QVBoxLayout();
    main_layout->addLayout(left_layout, 1);
    
    // 图片选择组
    QGroupBox* image_group = new QGroupBox("图片选择");
    left_layout->addWidget(image_group);
    
    QVBoxLayout* image_layout = new QVBoxLayout(image_group);
    
    QHBoxLayout* path_layout = new QHBoxLayout();
    image_path_edit_ = new QLineEdit();
    image_path_edit_->setPlaceholderText("选择要识别的PCB图片...");
    browse_image_button_ = new QPushButton("浏览...");
    path_layout->addWidget(image_path_edit_);
    path_layout->addWidget(browse_image_button_);
    image_layout->addLayout(path_layout);
    
    // 操作按钮组
    QGroupBox* action_group = new QGroupBox("识别操作");
    left_layout->addWidget(action_group);
    
    QVBoxLayout* action_layout = new QVBoxLayout(action_group);
    
    identify_button_ = new QPushButton("开始识别");
    identify_button_->setStyleSheet("QPushButton { font-weight: bold; padding: 10px; }");
    batch_identify_button_ = new QPushButton("批量识别");
    
    action_layout->addWidget(identify_button_);
    action_layout->addWidget(batch_identify_button_);
    
    // 进度条
    progress_bar_ = new QProgressBar();
    progress_bar_->setVisible(false);
    action_layout->addWidget(progress_bar_);
    
    // 结果显示组
    QGroupBox* result_group = new QGroupBox("识别结果");
    left_layout->addWidget(result_group);
    
    QVBoxLayout* result_layout = new QVBoxLayout(result_group);
    
    result_display_ = new QTextEdit();
    result_display_->setMaximumHeight(200);
    result_display_->setReadOnly(true);
    result_layout->addWidget(result_display_);
    
    // 结果表格
    results_table_ = new QTableWidget();
    results_table_->setColumnCount(6);
    QStringList headers = {"时间", "模型名称", "置信度", "匹配分数", "匹配点数", "状态"};
    results_table_->setHorizontalHeaderLabels(headers);
    results_table_->horizontalHeader()->setStretchLastSection(true);
    result_layout->addWidget(results_table_);
    
    left_layout->addStretch();
    
    // 右侧：图片预览
    QVBoxLayout* right_layout = new QVBoxLayout();
    main_layout->addLayout(right_layout, 1);
    
    QGroupBox* preview_group = new QGroupBox("图片预览");
    right_layout->addWidget(preview_group);
    
    QVBoxLayout* preview_layout = new QVBoxLayout(preview_group);
    
    preview_scroll_ = new QScrollArea();
    preview_label_ = new QLabel();
    preview_label_->setAlignment(Qt::AlignCenter);
    preview_label_->setMinimumSize(400, 300);
    preview_label_->setStyleSheet("QLabel { border: 2px dashed gray; }");
    preview_label_->setText("在此显示图片预览");
    
    preview_scroll_->setWidget(preview_label_);
    preview_scroll_->setWidgetResizable(true);
    preview_layout->addWidget(preview_scroll_);
}

void PCBIdentificationDialog::setupTemplatePage()
{
    template_page_ = new QWidget();
    main_tabs_->addTab(template_page_, "模板管理");
    
    QHBoxLayout* main_layout = new QHBoxLayout(template_page_);
    
    // 左侧：模型列表
    QVBoxLayout* left_layout = new QVBoxLayout();
    main_layout->addLayout(left_layout, 1);
    
    QGroupBox* model_list_group = new QGroupBox("PCB模型列表");
    left_layout->addWidget(model_list_group);
    
    QVBoxLayout* list_layout = new QVBoxLayout(model_list_group);
    
    model_list_ = new QListWidget();
    list_layout->addWidget(model_list_);
    
    // 模型操作按钮
    QHBoxLayout* model_buttons_layout = new QHBoxLayout();
    add_model_button_ = new QPushButton("添加模型");
    remove_model_button_ = new QPushButton("删除模型");
    update_model_button_ = new QPushButton("更新模型");
    
    model_buttons_layout->addWidget(add_model_button_);
    model_buttons_layout->addWidget(remove_model_button_);
    model_buttons_layout->addWidget(update_model_button_);
    list_layout->addLayout(model_buttons_layout);
    
    // 数据库操作
    QGroupBox* db_group = new QGroupBox("数据库操作");
    left_layout->addWidget(db_group);
    
    QVBoxLayout* db_layout = new QVBoxLayout(db_group);
    
    load_db_button_ = new QPushButton("加载数据库");
    save_db_button_ = new QPushButton("保存数据库");
    rebuild_db_button_ = new QPushButton("重建数据库");
    
    db_layout->addWidget(load_db_button_);
    db_layout->addWidget(save_db_button_);
    db_layout->addWidget(rebuild_db_button_);
    
    // 右侧：模型详情
    QVBoxLayout* right_layout = new QVBoxLayout();
    main_layout->addLayout(right_layout, 1);
    
    model_details_group_ = new QGroupBox("模型详情");
    model_details_group_->setEnabled(false);
    right_layout->addWidget(model_details_group_);
    
    QFormLayout* details_layout = new QFormLayout(model_details_group_);
    
    model_name_edit_ = new QLineEdit();
    model_code_edit_ = new QLineEdit();
    model_description_edit_ = new QTextEdit();
    model_description_edit_->setMaximumHeight(100);
    
    details_layout->addRow("模型名称:", model_name_edit_);
    details_layout->addRow("模型代码:", model_code_edit_);
    details_layout->addRow("描述:", model_description_edit_);
    
    // 模板路径列表
    QLabel* templates_label = new QLabel("模板图片:");
    details_layout->addRow(templates_label);
    
    template_paths_list_ = new QListWidget();
    template_paths_list_->setMaximumHeight(150);
    details_layout->addRow(template_paths_list_);
    
    QHBoxLayout* template_buttons_layout = new QHBoxLayout();
    add_template_button_ = new QPushButton("添加模板");
    remove_template_button_ = new QPushButton("删除模板");
    
    template_buttons_layout->addWidget(add_template_button_);
    template_buttons_layout->addWidget(remove_template_button_);
    details_layout->addRow(template_buttons_layout);
}

void PCBIdentificationDialog::setupConfigPage()
{
    config_page_ = new QWidget();
    main_tabs_->addTab(config_page_, "系统配置");
    
    QVBoxLayout* main_layout = new QVBoxLayout(config_page_);
    
    // 识别参数配置
    QGroupBox* params_group = new QGroupBox("识别参数");
    main_layout->addWidget(params_group);
    
    QFormLayout* params_layout = new QFormLayout(params_group);
    
    threshold_spin_ = new QDoubleSpinBox();
    threshold_spin_->setRange(0.0, 1.0);
    threshold_spin_->setSingleStep(0.01);
    threshold_spin_->setDecimals(3);
    threshold_spin_->setValue(0.3);
    params_layout->addRow("匹配阈值:", threshold_spin_);
    
    max_results_spin_ = new QSpinBox();
    max_results_spin_->setRange(1, 50);
    max_results_spin_->setValue(5);
    params_layout->addRow("最大结果数:", max_results_spin_);
    
    // GPU配置
    QGroupBox* gpu_group = new QGroupBox("GPU加速");
    main_layout->addWidget(gpu_group);
    
    QVBoxLayout* gpu_layout = new QVBoxLayout(gpu_group);
    
    gpu_checkbox_ = new QCheckBox("启用GPU加速");
    gpu_layout->addWidget(gpu_checkbox_);
    
    gpu_status_label_ = new QLabel("GPU状态: 检测中...");
    gpu_layout->addWidget(gpu_status_label_);
    
    // 日志显示
    QGroupBox* log_group = new QGroupBox("系统日志");
    main_layout->addWidget(log_group);
    
    QVBoxLayout* log_layout = new QVBoxLayout(log_group);
    
    log_display_ = new QTextEdit();
    log_display_->setReadOnly(true);
    log_display_->setMaximumHeight(200);
    log_layout->addWidget(log_display_);
    
    QPushButton* clear_log_button = new QPushButton("清空日志");
    connect(clear_log_button, &QPushButton::clicked, log_display_, &QTextEdit::clear);
    log_layout->addWidget(clear_log_button);
    
    main_layout->addStretch();
}

void PCBIdentificationDialog::connectSignals()
{
    // 识别页面信号连接
    connect(browse_image_button_, &QPushButton::clicked,
            this, &PCBIdentificationDialog::selectImageForIdentification);
    connect(identify_button_, &QPushButton::clicked,
            this, &PCBIdentificationDialog::startIdentification);
    connect(batch_identify_button_, &QPushButton::clicked,
            this, &PCBIdentificationDialog::startBatchIdentification);
    
    // 模板管理页面信号连接
    connect(add_model_button_, &QPushButton::clicked,
            this, &PCBIdentificationDialog::addNewModel);
    connect(remove_model_button_, &QPushButton::clicked,
            this, &PCBIdentificationDialog::removeSelectedModel);
    connect(update_model_button_, &QPushButton::clicked,
            this, &PCBIdentificationDialog::updateSelectedModel);
    connect(load_db_button_, &QPushButton::clicked,
            this, &PCBIdentificationDialog::loadTemplateDatabase);
    connect(save_db_button_, &QPushButton::clicked,
            this, &PCBIdentificationDialog::saveTemplateDatabase);
    connect(rebuild_db_button_, &QPushButton::clicked,
            this, &PCBIdentificationDialog::rebuildDatabase);
    
    connect(model_list_, &QListWidget::currentRowChanged,
            this, &PCBIdentificationDialog::onModelSelectionChanged);
    
    // 模板按钮连接
    connect(add_template_button_, &QPushButton::clicked, [this]() {
        QStringList files = selectTemplateFiles();
        for (const QString& file : files) {
            template_paths_list_->addItem(file);
        }
    });
    
    connect(remove_template_button_, &QPushButton::clicked, [this]() {
        int row = template_paths_list_->currentRow();
        if (row >= 0) {
            delete template_paths_list_->takeItem(row);
        }
    });
    
    // 配置页面信号连接
    connect(threshold_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PCBIdentificationDialog::onThresholdChanged);
    connect(max_results_spin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PCBIdentificationDialog::onMaxResultsChanged);
    connect(gpu_checkbox_, &QCheckBox::toggled,
            this, &PCBIdentificationDialog::onGPUToggled);
}

void PCBIdentificationDialog::selectImageForIdentification()
{
    QStringList files = selectImageFiles("选择要识别的PCB图片");
    if (!files.isEmpty()) {
        image_path_edit_->setText(files.first());
        updatePreviewImage(files.first());
        addLogMessage(QString("选择图片: %1").arg(files.first()));
    }
}

void PCBIdentificationDialog::startIdentification()
{
    if (!pcb_identifier_) {
        showErrorMessage("错误", "PCB识别器未初始化");
        return;
    }
    
    QString imagePath = image_path_edit_->text().trimmed();
    if (imagePath.isEmpty()) {
        showErrorMessage("错误", "请先选择要识别的图片");
        return;
    }
    
    if (!QFileInfo::exists(imagePath)) {
        showErrorMessage("错误", "选择的图片文件不存在");
        return;
    }
    
    // 禁用按钮并显示进度
    identify_button_->setEnabled(false);
    progress_bar_->setVisible(true);
    progress_bar_->setValue(0);
    
    updateStatusMessage("正在识别PCB型号...");
    addLogMessage(QString("开始识别: %1").arg(imagePath));
    
    // 启动异步识别
    pcb_identifier_->identifyPCBAsync(imagePath);
}

void PCBIdentificationDialog::startBatchIdentification()
{
    if (!pcb_identifier_) {
        showErrorMessage("错误", "PCB识别器未初始化");
        return;
    }
    
    QStringList imageFiles = selectImageFiles("选择要批量识别的PCB图片");
    if (imageFiles.isEmpty()) {
        return;
    }
    
    // 禁用按钮并显示进度
    batch_identify_button_->setEnabled(false);
    progress_bar_->setVisible(true);
    progress_bar_->setValue(0);
    
    updateStatusMessage(QString("正在批量识别 %1 张图片...").arg(imageFiles.size()));
    addLogMessage(QString("开始批量识别 %1 张图片").arg(imageFiles.size()));
    
    // 启动批量识别
    QVector<PCBIdentificationResult> results = pcb_identifier_->identifyBatch(imageFiles);
    
    // 处理结果
    for (const PCBIdentificationResult& result : results) {
        updateResultsTable(result);
    }
    
    // 恢复界面状态
    batch_identify_button_->setEnabled(true);
    progress_bar_->setVisible(false);
    updateStatusMessage(QString("批量识别完成，共处理 %1 张图片").arg(imageFiles.size()));
}

void PCBIdentificationDialog::onIdentificationCompleted(const PCBIdentificationResult& result)
{
    // 恢复界面状态
    identify_button_->setEnabled(true);
    progress_bar_->setVisible(false);
    
    // 更新结果显示
    updateResultsTable(result);
    
    QString resultText;
    if (result.isValid) {
        resultText = QString("识别成功!\n"
                           "模型名称: %1\n"
                           "置信度: %2\n"
                           "匹配分数: %3\n"
                           "匹配点数: %4")
                    .arg(result.modelName)
                    .arg(formatConfidence(result.confidence))
                    .arg(formatMatchScore(result.matchScore))
                    .arg(result.matchCount);
        
        updateStatusMessage(QString("识别完成: %1 (置信度: %2)")
                          .arg(result.modelName)
                          .arg(formatConfidence(result.confidence)));
        
        addLogMessage(QString("识别成功: %1").arg(result.modelName));
    } else {
        resultText = QString("识别失败\n错误信息: %1").arg(result.errorMessage);
        updateStatusMessage("识别失败");
        addLogMessage(QString("识别失败: %1").arg(result.errorMessage));
    }
    
    result_display_->setText(resultText);
}

void PCBIdentificationDialog::onIdentificationProgress(int percentage)
{
    progress_bar_->setValue(percentage);
}

void PCBIdentificationDialog::onErrorOccurred(const QString& error)
{
    showErrorMessage("识别错误", error);
    addLogMessage(QString("错误: %1").arg(error));
    
    // 恢复界面状态
    identify_button_->setEnabled(true);
    batch_identify_button_->setEnabled(true);
    progress_bar_->setVisible(false);
}

void PCBIdentificationDialog::addNewModel()
{
    // 实现添加新模型的逻辑
    QString modelName = QInputDialog::getText(this, "添加PCB模型", "请输入模型名称:");
    if (modelName.isEmpty()) {
        return;
    }
    
    QStringList templateFiles = selectTemplateFiles();
    if (templateFiles.isEmpty()) {
        showErrorMessage("错误", "请至少选择一个模板图片");
        return;
    }
    
    if (pcb_identifier_ && pcb_identifier_->addPCBModel(modelName, templateFiles)) {
        addLogMessage(QString("添加模型成功: %1").arg(modelName));
        updateModelList();
    }
}

void PCBIdentificationDialog::removeSelectedModel()
{
    int row = model_list_->currentRow();
    if (row < 0) {
        showErrorMessage("错误", "请先选择要删除的模型");
        return;
    }
    
    QString modelName = model_list_->item(row)->text();
    
    int ret = QMessageBox::question(this, "确认删除", 
                                   QString("确定要删除模型 '%1' 吗？").arg(modelName),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        if (pcb_identifier_ && pcb_identifier_->removePCBModel(modelName)) {
            addLogMessage(QString("删除模型成功: %1").arg(modelName));
            updateModelList();
        }
    }
}

void PCBIdentificationDialog::updateSelectedModel()
{
    // 实现更新模型的逻辑
    QString modelName = model_name_edit_->text().trimmed();
    if (modelName.isEmpty()) {
        showErrorMessage("错误", "模型名称不能为空");
        return;
    }
    
    QStringList templatePaths;
    for (int i = 0; i < template_paths_list_->count(); ++i) {
        templatePaths.append(template_paths_list_->item(i)->text());
    }
    
    if (templatePaths.isEmpty()) {
        showErrorMessage("错误", "请至少添加一个模板图片");
        return;
    }
    
    if (pcb_identifier_ && pcb_identifier_->updatePCBModel(modelName, templatePaths)) {
        addLogMessage(QString("更新模型成功: %1").arg(modelName));
        updateModelList();
    }
}

void PCBIdentificationDialog::loadTemplateDatabase()
{
    QString dbPath = QFileDialog::getOpenFileName(this, "选择数据库文件", "", "JSON Files (*.json)");
    if (!dbPath.isEmpty()) {
        if (pcb_identifier_ && pcb_identifier_->loadDatabase(dbPath)) {
            addLogMessage(QString("加载数据库成功: %1").arg(dbPath));
            updateModelList();
        }
    }
}

void PCBIdentificationDialog::saveTemplateDatabase()
{
    QString dbPath = QFileDialog::getSaveFileName(this, "保存数据库文件", "", "JSON Files (*.json)");
    if (!dbPath.isEmpty()) {
        if (pcb_identifier_ && pcb_identifier_->saveDatabase(dbPath)) {
            addLogMessage(QString("保存数据库成功: %1").arg(dbPath));
        }
    }
}

void PCBIdentificationDialog::rebuildDatabase()
{
    QString templateDir = selectDirectory("选择模板图片目录");
    if (!templateDir.isEmpty()) {
        if (pcb_identifier_ && pcb_identifier_->createDatabase(templateDir)) {
            addLogMessage(QString("重建数据库成功: %1").arg(templateDir));
            updateModelList();
        }
    }
}

void PCBIdentificationDialog::updateModelList()
{
    model_list_->clear();
    
    if (!pcb_identifier_) {
        return;
    }
    
    QVector<PCBModelInfo> models = pcb_identifier_->getAvailableModels();
    for (const PCBModelInfo& model : models) {
        model_list_->addItem(model.modelName);
    }
    
    addLogMessage(QString("模型列表已更新，共 %1 个模型").arg(models.size()));
}

void PCBIdentificationDialog::updateConfigurationDisplay()
{
    if (!pcb_identifier_) {
        return;
    }
    
    threshold_spin_->setValue(pcb_identifier_->getMatchThreshold());
    max_results_spin_->setValue(pcb_identifier_->getMaxResults());
    gpu_checkbox_->setChecked(pcb_identifier_->isGPUEnabled());
    
    QString gpuStatus = pcb_identifier_->isGPUEnabled() ? "GPU可用" : "GPU不可用，使用CPU";
    gpu_status_label_->setText(QString("GPU状态: %1").arg(gpuStatus));
}

void PCBIdentificationDialog::onModelSelectionChanged()
{
    int row = model_list_->currentRow();
    if (row < 0 || !pcb_identifier_) {
        model_details_group_->setEnabled(false);
        return;
    }
    
    QVector<PCBModelInfo> models = pcb_identifier_->getAvailableModels();
    if (row >= models.size()) {
        return;
    }
    
    const PCBModelInfo& model = models[row];
    
    model_name_edit_->setText(model.modelName);
    model_code_edit_->setText(model.modelCode);
    model_description_edit_->setText(model.description);
    
    template_paths_list_->clear();
    for (const QString& path : model.templatePaths) {
        template_paths_list_->addItem(path);
    }
    
    model_details_group_->setEnabled(true);
}

void PCBIdentificationDialog::onThresholdChanged(double value)
{
    if (pcb_identifier_) {
        pcb_identifier_->setMatchThreshold(value);
        addLogMessage(QString("匹配阈值已更新: %1").arg(value));
    }
}

void PCBIdentificationDialog::onMaxResultsChanged(int value)
{
    if (pcb_identifier_) {
        pcb_identifier_->setMaxResults(value);
        addLogMessage(QString("最大结果数已更新: %1").arg(value));
    }
}

void PCBIdentificationDialog::onGPUToggled(bool enabled)
{
    if (pcb_identifier_) {
        pcb_identifier_->setUseGPU(enabled);
        QString message = enabled ? "GPU加速已启用" : "GPU加速已禁用";
        addLogMessage(message);
    }
}

void PCBIdentificationDialog::updateResultsTable(const PCBIdentificationResult& result)
{
    int row = results_table_->rowCount();
    results_table_->insertRow(row);
    
    results_table_->setItem(row, 0, new QTableWidgetItem(result.timestamp.toString("yyyy-MM-dd hh:mm:ss")));
    results_table_->setItem(row, 1, new QTableWidgetItem(result.modelName));
    results_table_->setItem(row, 2, new QTableWidgetItem(formatConfidence(result.confidence)));
    results_table_->setItem(row, 3, new QTableWidgetItem(formatMatchScore(result.matchScore)));
    results_table_->setItem(row, 4, new QTableWidgetItem(QString::number(result.matchCount)));
    
    QString status = result.isValid ? "成功" : "失败";
    QTableWidgetItem* statusItem = new QTableWidgetItem(status);
    if (result.isValid) {
        statusItem->setBackground(QBrush(QColor(144, 238, 144))); // 浅绿色
    } else {
        statusItem->setBackground(QBrush(QColor(255, 182, 193))); // 浅红色
    }
    results_table_->setItem(row, 5, statusItem);
    
    results_table_->scrollToBottom();
}

void PCBIdentificationDialog::updatePreviewImage(const QString& imagePath)
{
    QPixmap pixmap(imagePath);
    if (!pixmap.isNull()) {
        // 缩放图片以适应预览区域
        int maxSize = 400;
        if (pixmap.width() > maxSize || pixmap.height() > maxSize) {
            pixmap = pixmap.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        preview_label_->setPixmap(pixmap);
        preview_label_->resize(pixmap.size());
    } else {
        preview_label_->setText("无法加载图片预览");
    }
}

void PCBIdentificationDialog::clearResults()
{
    results_table_->setRowCount(0);
    result_display_->clear();
}

void PCBIdentificationDialog::showErrorMessage(const QString& title, const QString& message)
{
    QMessageBox::critical(this, title, message);
}

void PCBIdentificationDialog::showInfoMessage(const QString& title, const QString& message)
{
    QMessageBox::information(this, title, message);
}

QStringList PCBIdentificationDialog::selectImageFiles(const QString& title)
{
    QStringList filters;
    if (pcb_identifier_) {
        QStringList formats = pcb_identifier_->getSupportedImageFormats();
        QStringList filterParts;
        for (const QString& format : formats) {
            filterParts.append("*." + format);
        }
        filters << QString("图片文件 (%1)").arg(filterParts.join(" "));
    }
    filters << "所有文件 (*.*)";
    
    return QFileDialog::getOpenFileNames(this, title, "", filters.join(";;"));
}

QStringList PCBIdentificationDialog::selectTemplateFiles()
{
    return selectImageFiles("选择模板图片");
}

QString PCBIdentificationDialog::selectDirectory(const QString& title)
{
    return QFileDialog::getExistingDirectory(this, title);
}

void PCBIdentificationDialog::updateStatusMessage(const QString& message)
{
    status_label_->setText(message);
}

void PCBIdentificationDialog::addLogMessage(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    log_display_->append(QString("[%1] %2").arg(timestamp, message));
}

QString PCBIdentificationDialog::formatConfidence(double confidence) const
{
    return QString("%1%").arg(confidence, 0, 'f', 1);
}

QString PCBIdentificationDialog::formatMatchScore(double score) const
{
    return QString("%1").arg(score, 0, 'f', 2);
}
