#include "pcbdetectionhistorywidget.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QPixmap>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QInputDialog>

PCBDetectionHistoryWidget::PCBDetectionHistoryWidget(QWidget *parent)
    : QWidget(parent)
    , detection_manager_(nullptr)
    , current_selected_record_id_("")
    , statistics_timer_(new QTimer(this))
{
    setupUI();
    connectSignals();
    
    // 设置统计更新定时器
    statistics_timer_->setInterval(5000); // 5秒更新一次统计
    connect(statistics_timer_, &QTimer::timeout, this, &PCBDetectionHistoryWidget::onUpdateStatistics);
    statistics_timer_->start();
}

PCBDetectionHistoryWidget::~PCBDetectionHistoryWidget() {
    if (statistics_timer_) {
        statistics_timer_->stop();
    }
}

void PCBDetectionHistoryWidget::setDetectionManager(PCBDetectionManager* manager) {
    if (detection_manager_) {
        disconnect(detection_manager_, nullptr, this, nullptr);
    }
    
    detection_manager_ = manager;
    
    if (detection_manager_) {
        connect(detection_manager_, &PCBDetectionManager::recordAdded,
                this, &PCBDetectionHistoryWidget::onNewRecordAdded);
        connect(detection_manager_, &PCBDetectionManager::recordUpdated,
                this, &PCBDetectionHistoryWidget::onRecordUpdated);
        connect(detection_manager_, &PCBDetectionManager::recordDeleted,
                this, &PCBDetectionHistoryWidget::onRecordDeleted);
        
        refreshRecordsList();
        updateStatistics();
    }
}

void PCBDetectionHistoryWidget::refreshRecordsList() {
    if (!detection_manager_) {
        return;
    }
    
    populateTable();
    updateStatistics();
}

void PCBDetectionHistoryWidget::setupUI() {
    auto* main_layout = new QVBoxLayout(this);
    
    // 创建主分割器
    main_splitter_ = new QSplitter(Qt::Horizontal);
    
    // 左侧面板
    left_panel_ = new QWidget;
    auto* left_layout = new QVBoxLayout(left_panel_);
    
    setupFilterPanel();
    setupTableWidget();
    setupToolbar();
    
    left_layout->addWidget(filter_group_);
    left_layout->addWidget(records_table_);
    left_layout->addWidget(records_count_label_);
    left_layout->addLayout(new QHBoxLayout);
    
    // 工具栏布局
    auto* toolbar_layout = new QHBoxLayout;
    toolbar_layout->addWidget(select_all_button_);
    toolbar_layout->addWidget(delete_selected_button_);
    toolbar_layout->addWidget(export_selected_button_);
    toolbar_layout->addWidget(export_all_button_);
    toolbar_layout->addWidget(cleanup_button_);
    toolbar_layout->addStretch();
    
    left_layout->addLayout(toolbar_layout);
    
    // 右侧面板
    right_panel_ = new QWidget;
    auto* right_layout = new QVBoxLayout(right_panel_);
    
    setupDetailPanel();
    setupStatisticsPanel();
    
    right_layout->addWidget(detail_group_);
    right_layout->addWidget(statistics_group_);
    right_layout->setStretch(0, 2);
    right_layout->setStretch(1, 1);
    
    // 添加到分割器
    main_splitter_->addWidget(left_panel_);
    main_splitter_->addWidget(right_panel_);
    main_splitter_->setStretchFactor(0, 2);
    main_splitter_->setStretchFactor(1, 1);
    
    main_layout->addWidget(main_splitter_);
    
    setWindowTitle("PCB检测历史管理");
    resize(1200, 800);
}

void PCBDetectionHistoryWidget::setupTableWidget() {
    records_table_ = new QTableWidget;
    records_table_->setColumnCount(7);
    
    QStringList headers = {"时间", "PCB模型", "置信度", "元件数量", "分析时间", "备注", "ID"};
    records_table_->setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    records_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    records_table_->setAlternatingRowColors(true);
    records_table_->setSortingEnabled(true);
    records_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 设置列宽
    records_table_->setColumnWidth(0, 150); // 时间
    records_table_->setColumnWidth(1, 120); // PCB模型
    records_table_->setColumnWidth(2, 80);  // 置信度
    records_table_->setColumnWidth(3, 80);  // 元件数量
    records_table_->setColumnWidth(4, 100); // 分析时间
    records_table_->setColumnWidth(5, 200); // 备注
    records_table_->setColumnWidth(6, 100); // ID
    
    records_table_->horizontalHeader()->setStretchLastSection(false);
    
    records_count_label_ = new QLabel("总记录数: 0");
    records_count_label_->setStyleSheet("QLabel { font-weight: bold; }");
}

void PCBDetectionHistoryWidget::setupFilterPanel() {
    filter_group_ = new QGroupBox("过滤条件");
    auto* layout = new QGridLayout(filter_group_);
    
    // 搜索框
    layout->addWidget(new QLabel("搜索:"), 0, 0);
    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("搜索PCB模型、备注等...");
    layout->addWidget(search_edit_, 0, 1, 1, 2);
    
    // PCB模型过滤
    layout->addWidget(new QLabel("PCB模型:"), 1, 0);
    pcb_model_combo_ = new QComboBox;
    pcb_model_combo_->addItem("全部");
    layout->addWidget(pcb_model_combo_, 1, 1, 1, 2);
    
    // 日期范围
    layout->addWidget(new QLabel("开始时间:"), 2, 0);
    start_date_edit_ = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-30));
    start_date_edit_->setCalendarPopup(true);
    layout->addWidget(start_date_edit_, 2, 1);
    
    layout->addWidget(new QLabel("结束时间:"), 2, 2);
    end_date_edit_ = new QDateTimeEdit(QDateTime::currentDateTime());
    end_date_edit_->setCalendarPopup(true);
    layout->addWidget(end_date_edit_, 2, 3);
    
    // 按钮
    refresh_button_ = new QPushButton("刷新");
    clear_filters_button_ = new QPushButton("清除过滤");
    layout->addWidget(refresh_button_, 3, 0);
    layout->addWidget(clear_filters_button_, 3, 1);
}

void PCBDetectionHistoryWidget::setupDetailPanel() {
    detail_group_ = new QGroupBox("记录详情");
    auto* layout = new QVBoxLayout(detail_group_);
    
    // 基本信息网格
    auto* info_layout = new QGridLayout;
    
    info_layout->addWidget(new QLabel("记录ID:"), 0, 0);
    detail_id_label_ = new QLabel("-");
    detail_id_label_->setStyleSheet("QLabel { font-family: monospace; }");
    info_layout->addWidget(detail_id_label_, 0, 1);
    
    info_layout->addWidget(new QLabel("检测时间:"), 1, 0);
    detail_timestamp_label_ = new QLabel("-");
    info_layout->addWidget(detail_timestamp_label_, 1, 1);
    
    info_layout->addWidget(new QLabel("PCB模型:"), 2, 0);
    detail_pcb_model_label_ = new QLabel("-");
    detail_pcb_model_label_->setStyleSheet("QLabel { font-weight: bold; }");
    info_layout->addWidget(detail_pcb_model_label_, 2, 1);
    
    info_layout->addWidget(new QLabel("PCB置信度:"), 3, 0);
    detail_pcb_confidence_label_ = new QLabel("-");
    info_layout->addWidget(detail_pcb_confidence_label_, 3, 1);
    
    info_layout->addWidget(new QLabel("元件总数:"), 4, 0);
    detail_total_components_label_ = new QLabel("-");
    info_layout->addWidget(detail_total_components_label_, 4, 1);
    
    info_layout->addWidget(new QLabel("分析时间:"), 5, 0);
    detail_analysis_time_label_ = new QLabel("-");
    info_layout->addWidget(detail_analysis_time_label_, 5, 1);
    
    layout->addLayout(info_layout);
    
    // 备注编辑
    layout->addWidget(new QLabel("备注:"));
    detail_notes_edit_ = new QTextEdit;
    detail_notes_edit_->setMaximumHeight(80);
    layout->addWidget(detail_notes_edit_);
    
    save_notes_button_ = new QPushButton("保存备注");
    save_notes_button_->setEnabled(false);
    layout->addWidget(save_notes_button_);
    
    // 操作按钮
    auto* button_layout = new QHBoxLayout;
    view_original_button_ = new QPushButton("查看原图");
    view_annotated_button_ = new QPushButton("查看标注图");
    delete_record_button_ = new QPushButton("删除记录");
    delete_record_button_->setStyleSheet("QPushButton { background-color: #ff4444; color: white; }");
    
    button_layout->addWidget(view_original_button_);
    button_layout->addWidget(view_annotated_button_);
    button_layout->addStretch();
    button_layout->addWidget(delete_record_button_);
    
    layout->addLayout(button_layout);
    
    // 图像预览
    image_preview_area_ = new QScrollArea;
    image_preview_label_ = new QLabel;
    image_preview_label_->setAlignment(Qt::AlignCenter);
    image_preview_label_->setMinimumSize(300, 200);
    image_preview_label_->setStyleSheet("QLabel { border: 1px solid #ccc; background-color: #f5f5f5; }");
    image_preview_area_->setWidget(image_preview_label_);
    image_preview_area_->setMaximumHeight(250);
    
    layout->addWidget(new QLabel("图像预览:"));
    layout->addWidget(image_preview_area_);
}

void PCBDetectionHistoryWidget::setupStatisticsPanel() {
    statistics_group_ = new QGroupBox("统计信息");
    auto* layout = new QGridLayout(statistics_group_);
    
    total_records_label_ = new QLabel("总记录数: 0");
    date_range_label_ = new QLabel("时间范围: -");
    most_common_pcb_label_ = new QLabel("最常见PCB: -");
    most_common_component_label_ = new QLabel("最常见元件: -");
    
    layout->addWidget(total_records_label_, 0, 0, 1, 2);
    layout->addWidget(date_range_label_, 1, 0, 1, 2);
    layout->addWidget(most_common_pcb_label_, 2, 0, 1, 2);
    layout->addWidget(most_common_component_label_, 3, 0, 1, 2);
    
    layout->addWidget(new QLabel("存储使用情况:"), 4, 0);
    storage_usage_bar_ = new QProgressBar;
    storage_usage_bar_->setFormat("%p% (%v MB)");
    layout->addWidget(storage_usage_bar_, 4, 1);
}

void PCBDetectionHistoryWidget::setupToolbar() {
    select_all_button_ = new QPushButton("全选");
    delete_selected_button_ = new QPushButton("删除选中");
    export_selected_button_ = new QPushButton("导出选中");
    export_all_button_ = new QPushButton("导出全部");
    cleanup_button_ = new QPushButton("清理旧记录");
    
    delete_selected_button_->setStyleSheet("QPushButton { background-color: #ff6666; color: white; }");
    cleanup_button_->setStyleSheet("QPushButton { background-color: #ff9900; color: white; }");
}

void PCBDetectionHistoryWidget::connectSignals() {
    // 过滤信号
    connect(search_edit_, &QLineEdit::textChanged, this, &PCBDetectionHistoryWidget::onSearchTextChanged);
    connect(pcb_model_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PCBDetectionHistoryWidget::onPCBModelFilterChanged);
    connect(start_date_edit_, &QDateTimeEdit::dateTimeChanged, this, &PCBDetectionHistoryWidget::onDateRangeChanged);
    connect(end_date_edit_, &QDateTimeEdit::dateTimeChanged, this, &PCBDetectionHistoryWidget::onDateRangeChanged);
    connect(refresh_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onRefreshClicked);
    connect(clear_filters_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onClearFiltersClicked);
    
    // 表格信号
    connect(records_table_, &QTableWidget::itemSelectionChanged, this, &PCBDetectionHistoryWidget::onRecordSelectionChanged);
    connect(records_table_, &QTableWidget::customContextMenuRequested, this, &PCBDetectionHistoryWidget::onShowContextMenu);
    
    // 详情面板信号
    connect(save_notes_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onSaveNotesClicked);
    connect(view_original_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onViewImageClicked);
    connect(view_annotated_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onViewImageClicked);
    connect(delete_record_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onDeleteRecordClicked);
    
    // 工具栏信号
    connect(select_all_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onSelectAllClicked);
    connect(delete_selected_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onDeleteSelectedClicked);
    connect(export_selected_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onExportSelectedClicked);
    connect(export_all_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onExportAllClicked);
    connect(cleanup_button_, &QPushButton::clicked, this, &PCBDetectionHistoryWidget::onCleanupOldRecordsClicked);
    
    // 右键菜单
    context_menu_ = new QMenu(this);
    copy_id_action_ = context_menu_->addAction("复制记录ID");
    open_location_action_ = context_menu_->addAction("打开图像位置");
    export_single_action_ = context_menu_->addAction("导出此记录");
    context_menu_->addSeparator();
    delete_action_ = context_menu_->addAction("删除记录");
    
    connect(copy_id_action_, &QAction::triggered, this, &PCBDetectionHistoryWidget::onCopyRecordId);
    connect(open_location_action_, &QAction::triggered, this, &PCBDetectionHistoryWidget::onOpenImageLocation);
    connect(export_single_action_, &QAction::triggered, this, &PCBDetectionHistoryWidget::onExportSingleRecord);
    connect(delete_action_, &QAction::triggered, this, &PCBDetectionHistoryWidget::onDeleteRecordClicked);
}

void PCBDetectionHistoryWidget::populateTable() {
    if (!detection_manager_) {
        records_table_->setRowCount(0);
        records_count_label_->setText("总记录数: 0");
        return;
    }
    
    QList<PCBDetectionRecord> records = getFilteredRecords();
    populateTableWithRecords(records);
}

void PCBDetectionHistoryWidget::populateTableWithRecords(const QList<PCBDetectionRecord>& records) {
    records_table_->setRowCount(records.size());
    
    for (int i = 0; i < records.size(); ++i) {
        const PCBDetectionRecord& record = records[i];
        
        // 时间
        records_table_->setItem(i, 0, new QTableWidgetItem(record.timestamp.toString("yyyy-MM-dd hh:mm:ss")));
        
        // PCB模型
        records_table_->setItem(i, 1, new QTableWidgetItem(record.pcbModel));
        
        // 置信度
        auto* confidenceItem = new QTableWidgetItem(QString::number(record.pcbConfidence, 'f', 1) + "%");
        confidenceItem->setData(Qt::UserRole, record.pcbConfidence);
        records_table_->setItem(i, 2, confidenceItem);
        
        // 元件数量
        auto* componentsItem = new QTableWidgetItem(QString::number(record.totalComponents));
        componentsItem->setData(Qt::UserRole, record.totalComponents);
        records_table_->setItem(i, 3, componentsItem);
        
        // 分析时间
        auto* timeItem = new QTableWidgetItem(QString::number(record.analysisTime, 'f', 0) + " ms");
        timeItem->setData(Qt::UserRole, record.analysisTime);
        records_table_->setItem(i, 4, timeItem);
        
        // 备注
        QString notes = record.notes;
        if (notes.length() > 50) {
            notes = notes.left(47) + "...";
        }
        records_table_->setItem(i, 5, new QTableWidgetItem(notes));
        
        // ID (隐藏列，用于查找)
        auto* idItem = new QTableWidgetItem(record.id);
        records_table_->setItem(i, 6, idItem);
    }
    
    records_count_label_->setText(QString("总记录数: %1").arg(records.size()));
}

QList<PCBDetectionRecord> PCBDetectionHistoryWidget::getFilteredRecords() const {
    if (!detection_manager_) {
        return QList<PCBDetectionRecord>();
    }
    
    QList<PCBDetectionRecord> allRecords = detection_manager_->getAllRecords();
    QList<PCBDetectionRecord> filteredRecords;
    
    QString searchText = search_edit_->text().toLower();
    QString pcbModelFilter = pcb_model_combo_->currentText();
    QDateTime startDate = start_date_edit_->dateTime();
    QDateTime endDate = end_date_edit_->dateTime();
    
    for (const auto& record : allRecords) {
        // 文本搜索过滤
        if (!searchText.isEmpty()) {
            bool matches = record.pcbModel.toLower().contains(searchText) ||
                          record.notes.toLower().contains(searchText) ||
                          record.id.toLower().contains(searchText);
            if (!matches) continue;
        }
        
        // PCB模型过滤
        if (pcbModelFilter != "全部" && record.pcbModel != pcbModelFilter) {
            continue;
        }
        
        // 日期范围过滤
        if (record.timestamp < startDate || record.timestamp > endDate) {
            continue;
        }
        
        filteredRecords.append(record);
    }
    
    return filteredRecords;
}

// 实现所有槽函数...
void PCBDetectionHistoryWidget::onNewRecordAdded(const QString& recordId) {
    Q_UNUSED(recordId)
    refreshRecordsList();
}

void PCBDetectionHistoryWidget::onRecordUpdated(const QString& recordId) {
    Q_UNUSED(recordId)
    refreshRecordsList();
}

void PCBDetectionHistoryWidget::onRecordDeleted(const QString& recordId) {
    Q_UNUSED(recordId)
    refreshRecordsList();
    if (current_selected_record_id_ == recordId) {
        clearRecordDetails();
    }
}

void PCBDetectionHistoryWidget::onSearchTextChanged() {
    populateTable();
}

void PCBDetectionHistoryWidget::onDateRangeChanged() {
    populateTable();
}

void PCBDetectionHistoryWidget::onPCBModelFilterChanged() {
    populateTable();
}

void PCBDetectionHistoryWidget::onRefreshClicked() {
    // 更新PCB模型列表
    if (detection_manager_) {
        QMap<QString, int> pcbStats = detection_manager_->getPCBModelStatistics();
        pcb_model_combo_->clear();
        pcb_model_combo_->addItem("全部");
        for (auto it = pcbStats.begin(); it != pcbStats.end(); ++it) {
            pcb_model_combo_->addItem(it.key());
        }
    }
    
    refreshRecordsList();
}

void PCBDetectionHistoryWidget::onClearFiltersClicked() {
    search_edit_->clear();
    pcb_model_combo_->setCurrentText("全部");
    start_date_edit_->setDateTime(QDateTime::currentDateTime().addDays(-30));
    end_date_edit_->setDateTime(QDateTime::currentDateTime());
    populateTable();
}

void PCBDetectionHistoryWidget::onRecordSelectionChanged() {
    QList<QTableWidgetItem*> selectedItems = records_table_->selectedItems();
    if (selectedItems.isEmpty()) {
        clearRecordDetails();
        return;
    }
    
    int row = selectedItems.first()->row();
    QTableWidgetItem* idItem = records_table_->item(row, 6);
    if (!idItem) return;
    
    QString recordId = idItem->text();
    current_selected_record_id_ = recordId;
    
    if (detection_manager_) {
        PCBDetectionRecord record = detection_manager_->getRecordById(recordId);
        updateRecordDetails(record);
    }
}

void PCBDetectionHistoryWidget::updateRecordDetails(const PCBDetectionRecord& record) {
    detail_id_label_->setText(record.id);
    detail_timestamp_label_->setText(record.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    detail_pcb_model_label_->setText(record.pcbModel.isEmpty() ? "未识别" : record.pcbModel);
    detail_pcb_confidence_label_->setText(QString::number(record.pcbConfidence, 'f', 1) + "%");
    detail_total_components_label_->setText(QString::number(record.totalComponents));
    detail_analysis_time_label_->setText(QString::number(record.analysisTime, 'f', 0) + " ms");
    detail_notes_edit_->setText(record.notes);
    
    save_notes_button_->setEnabled(true);
    view_original_button_->setEnabled(!record.imagePath.isEmpty());
    view_annotated_button_->setEnabled(!record.annotatedImagePath.isEmpty());
    delete_record_button_->setEnabled(true);
    
    // 显示图像预览
    if (!record.annotatedImagePath.isEmpty()) {
        showImagePreview(record.annotatedImagePath);
    } else if (!record.imagePath.isEmpty()) {
        showImagePreview(record.imagePath);
    } else {
        image_preview_label_->setText("无图像");
        image_preview_label_->setPixmap(QPixmap());
    }
}

void PCBDetectionHistoryWidget::clearRecordDetails() {
    current_selected_record_id_.clear();
    detail_id_label_->setText("-");
    detail_timestamp_label_->setText("-");
    detail_pcb_model_label_->setText("-");
    detail_pcb_confidence_label_->setText("-");
    detail_total_components_label_->setText("-");
    detail_analysis_time_label_->setText("-");
    detail_notes_edit_->clear();
    
    save_notes_button_->setEnabled(false);
    view_original_button_->setEnabled(false);
    view_annotated_button_->setEnabled(false);
    delete_record_button_->setEnabled(false);
    
    image_preview_label_->setText("请选择记录");
    image_preview_label_->setPixmap(QPixmap());
}

void PCBDetectionHistoryWidget::showImagePreview(const QString& imagePath) {
    if (imagePath.isEmpty() || !QFile::exists(imagePath)) {
        image_preview_label_->setText("图像文件不存在");
        image_preview_label_->setPixmap(QPixmap());
        return;
    }
    
    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        image_preview_label_->setText("无法加载图像");
        return;
    }
    
    // 缩放图像以适应预览区域
    QSize previewSize = image_preview_area_->size() - QSize(20, 20);
    QPixmap scaledPixmap = pixmap.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    image_preview_label_->setPixmap(scaledPixmap);
    image_preview_label_->resize(scaledPixmap.size());
}

void PCBDetectionHistoryWidget::updateStatistics() {
    if (!detection_manager_) {
        return;
    }
    
    int totalRecords = detection_manager_->getTotalRecordsCount();
    total_records_label_->setText(QString("总记录数: %1").arg(totalRecords));
    
    if (totalRecords > 0) {
        QDateTime oldest = detection_manager_->getOldestRecordDate();
        QDateTime newest = detection_manager_->getNewestRecordDate();
        date_range_label_->setText(QString("时间范围: %1 ~ %2")
                                  .arg(oldest.toString("yyyy-MM-dd"))
                                  .arg(newest.toString("yyyy-MM-dd")));
        
        // 最常见PCB
        QMap<QString, int> pcbStats = detection_manager_->getPCBModelStatistics();
        QString mostCommonPCB = "无";
        int maxPCBCount = 0;
        for (auto it = pcbStats.begin(); it != pcbStats.end(); ++it) {
            if (it.value() > maxPCBCount) {
                maxPCBCount = it.value();
                mostCommonPCB = it.key();
            }
        }
        most_common_pcb_label_->setText(QString("最常见PCB: %1 (%2次)").arg(mostCommonPCB).arg(maxPCBCount));
        
        // 最常见元件
        QMap<QString, int> componentStats = detection_manager_->getComponentTypeStatistics();
        QString mostCommonComponent = "无";
        int maxComponentCount = 0;
        for (auto it = componentStats.begin(); it != componentStats.end(); ++it) {
            if (it.value() > maxComponentCount) {
                maxComponentCount = it.value();
                mostCommonComponent = it.key();
            }
        }
        most_common_component_label_->setText(QString("最常见元件: %1 (%2个)").arg(mostCommonComponent).arg(maxComponentCount));
    } else {
        date_range_label_->setText("时间范围: -");
        most_common_pcb_label_->setText("最常见PCB: -");
        most_common_component_label_->setText("最常见元件: -");
    }
    
    // 存储使用情况
    qint64 dbSize = detection_manager_->getDatabaseSize();
    QDir imageDir(detection_manager_->getImageStoragePath());
    qint64 totalSize = dbSize;
    
    QFileInfoList files = imageDir.entryInfoList(QDir::Files);
    for (const QFileInfo& fileInfo : files) {
        totalSize += fileInfo.size();
    }
    
    int sizeMB = totalSize / (1024 * 1024);
    storage_usage_bar_->setValue(sizeMB);
    storage_usage_bar_->setMaximum(qMax(100, sizeMB + 50)); // 动态调整最大值
}

void PCBDetectionHistoryWidget::onSaveNotesClicked() {
    if (current_selected_record_id_.isEmpty() || !detection_manager_) {
        return;
    }
    
    PCBDetectionRecord record = detection_manager_->getRecordById(current_selected_record_id_);
    if (!record.isValid) {
        return;
    }
    
    record.notes = detail_notes_edit_->toPlainText();
    
    if (detection_manager_->updateDetectionRecord(record)) {
        QMessageBox::information(this, "保存成功", "备注已保存");
        populateTable(); // 刷新表格显示
    } else {
        QMessageBox::warning(this, "保存失败", "无法保存备注");
    }
}

void PCBDetectionHistoryWidget::onViewImageClicked() {
    if (current_selected_record_id_.isEmpty() || !detection_manager_) {
        return;
    }
    
    PCBDetectionRecord record = detection_manager_->getRecordById(current_selected_record_id_);
    if (!record.isValid) {
        return;
    }
    
    QString imagePath;
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (button == view_original_button_) {
        imagePath = record.imagePath;
    } else if (button == view_annotated_button_) {
        imagePath = record.annotatedImagePath;
    }
    
    if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(imagePath));
    } else {
        QMessageBox::warning(this, "文件不存在", "图像文件不存在或已被删除");
    }
}

void PCBDetectionHistoryWidget::onDeleteRecordClicked() {
    if (current_selected_record_id_.isEmpty() || !detection_manager_) {
        return;
    }
    
    int result = QMessageBox::question(this, "确认删除", 
                                      "确定要删除这个检测记录吗？\n这将同时删除相关的图像文件。",
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        if (detection_manager_->deleteDetectionRecord(current_selected_record_id_)) {
            QMessageBox::information(this, "删除成功", "记录已删除");
        } else {
            QMessageBox::warning(this, "删除失败", "无法删除记录");
        }
    }
}

void PCBDetectionHistoryWidget::onEditNotesClicked() {
    if (current_selected_record_id_.isEmpty() || !detection_manager_) {
        return;
    }
    
    PCBDetectionRecord record = detection_manager_->getRecordById(current_selected_record_id_);
    if (!record.isValid) {
        QMessageBox::warning(this, "错误", "无法找到选中的记录");
        return;
    }
    
    // 创建编辑对话框
    bool ok;
    QString newNotes = QInputDialog::getMultiLineText(this, "编辑备注", 
                                                     "请输入备注信息:", 
                                                     record.notes, &ok);
    
    if (ok) {
        record.notes = newNotes;        
        if (detection_manager_->updateDetectionRecord(record)) {
            // 刷新显示
            refreshRecordsList();
            updateRecordDetails(record);
            QMessageBox::information(this, "成功", "备注信息已更新");
        } else {
            QMessageBox::warning(this, "错误", "更新备注信息失败");
        }
    }
}

// 其他槽函数的简化实现
void PCBDetectionHistoryWidget::onSelectAllClicked() {
    records_table_->selectAll();
}

void PCBDetectionHistoryWidget::onDeleteSelectedClicked() {
    QStringList selectedIds = getSelectedRecordIds();
    if (selectedIds.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要删除的记录");
        return;
    }
    
    int result = QMessageBox::question(this, "确认批量删除", 
                                      QString("确定要删除 %1 个记录吗？").arg(selectedIds.size()),
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        deleteRecords(selectedIds);
    }
}

void PCBDetectionHistoryWidget::onExportSelectedClicked() {
    QStringList selectedIds = getSelectedRecordIds();
    if (selectedIds.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要导出的记录");
        return;
    }
    
    exportRecords(selectedIds);
}

void PCBDetectionHistoryWidget::onExportAllClicked() {
    if (!detection_manager_) {
        return;
    }
    
    exportRecords(QStringList());
}

void PCBDetectionHistoryWidget::onCleanupOldRecordsClicked() {
    if (!detection_manager_) {
        return;
    }
    
    bool ok;
    int days = QInputDialog::getInt(this, "清理旧记录", 
                                   "删除多少天前的记录？", 30, 1, 365, 1, &ok);
    
    if (ok) {
        int result = QMessageBox::question(this, "确认清理", 
                                          QString("确定要删除 %1 天前的所有记录吗？").arg(days),
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
        
        if (result == QMessageBox::Yes) {
            if (detection_manager_->cleanupOldRecords(days)) {
                QMessageBox::information(this, "清理完成", "旧记录已清理");
                refreshRecordsList();
            } else {
                QMessageBox::information(this, "清理完成", "没有找到需要清理的记录");
            }
        }
    }
}

QStringList PCBDetectionHistoryWidget::getSelectedRecordIds() const {
    QStringList ids;
    QList<QTableWidgetItem*> selectedItems = records_table_->selectedItems();
    
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }
    
    for (int row : selectedRows) {
        QTableWidgetItem* idItem = records_table_->item(row, 6);
        if (idItem) {
            ids.append(idItem->text());
        }
    }
    
    return ids;
}

void PCBDetectionHistoryWidget::exportRecords(const QList<QString>& recordIds) {
    if (!detection_manager_) {
        return;
    }
    
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString defaultFileName = QString("PCB检测记录_%1.json").arg(timestamp);
    
    QString fileName = QFileDialog::getSaveFileName(this, "导出记录",
                                                   defaultPath + "/" + defaultFileName,
                                                   "JSON文件 (*.json);;CSV文件 (*.csv)");
    
    if (!fileName.isEmpty()) {
        bool success = false;
        if (fileName.endsWith(".csv")) {
            success = detection_manager_->exportToCSV(fileName, recordIds);
        } else {
            success = detection_manager_->exportToJson(fileName, recordIds);
        }
        
        if (success) {
            QMessageBox::information(this, "导出成功", QString("记录已导出到：\n%1").arg(fileName));
        } else {
            QMessageBox::warning(this, "导出失败", "无法导出记录");
        }
    }
}

bool PCBDetectionHistoryWidget::deleteRecords(const QList<QString>& recordIds) {
    if (!detection_manager_) {
        return false;
    }
    
    int successCount = 0;
    for (const QString& id : recordIds) {
        if (detection_manager_->deleteDetectionRecord(id)) {
            successCount++;
        }
    }
    
    if (successCount > 0) {
        QMessageBox::information(this, "删除完成", 
                                QString("成功删除 %1 个记录").arg(successCount));
        return true;
    } else {
        QMessageBox::warning(this, "删除失败", "无法删除记录");
        return false;
    }
}

void PCBDetectionHistoryWidget::onShowContextMenu(const QPoint& pos) {
    QTableWidgetItem* item = records_table_->itemAt(pos);
    if (item) {
        context_menu_->exec(records_table_->mapToGlobal(pos));
    }
}

void PCBDetectionHistoryWidget::onCopyRecordId() {
    if (!current_selected_record_id_.isEmpty()) {
        QApplication::clipboard()->setText(current_selected_record_id_);
        QMessageBox::information(this, "复制成功", "记录ID已复制到剪贴板");
    }
}

void PCBDetectionHistoryWidget::onOpenImageLocation() {
    if (current_selected_record_id_.isEmpty() || !detection_manager_) {
        return;
    }
    
    PCBDetectionRecord record = detection_manager_->getRecordById(current_selected_record_id_);
    if (!record.isValid) {
        return;
    }
    
    QString imagePath = record.annotatedImagePath;
    if (imagePath.isEmpty()) {
        imagePath = record.imagePath;
    }
    
    if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
        QFileInfo fileInfo(imagePath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
    }
}

void PCBDetectionHistoryWidget::onExportSingleRecord() {
    if (!current_selected_record_id_.isEmpty()) {
        exportRecords(QStringList() << current_selected_record_id_);
    }
}

void PCBDetectionHistoryWidget::onUpdateStatistics() {
    updateStatistics();
}
