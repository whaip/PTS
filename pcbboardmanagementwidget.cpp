#include "pcbboardmanagementwidget.h"
#include "identificationworker.h"
#include <QApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QPixmap>
#include <QMouseEvent>
#include <QPainter>
#include <QColorDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QJsonDocument>
#include <QFormLayout>
#include <QCheckBox>
#include <QSlider>
#include <QProgressDialog>
#include <QTimer>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QFutureWatcher>

// PCBBoardManagementWidget 实现
PCBBoardManagementWidget::PCBBoardManagementWidget(QWidget *parent)
    : QWidget(parent)
    , board_manager_(nullptr)
    , camera_manager_(nullptr)
    , current_board_id_("")
    , zoom_factor_(1.0)
    , dragging_(false)
    , selected_component_(nullptr)
    , drawing_component_(false)
    , label_editing_(nullptr)
    , identification_worker_thread_(nullptr)
{
    setupUI();
    connectSignals();
    
    // 允许拖放
    setAcceptDrops(true);
}

PCBBoardManagementWidget::~PCBBoardManagementWidget()
{
    if (label_editing_) {
        delete label_editing_;
        label_editing_ = nullptr;
    }
    
    // 清理识别线程 - 使用deleteLater避免阻塞
    if (identification_worker_thread_) {
        identification_worker_thread_->quit();
        identification_worker_thread_->deleteLater();
        identification_worker_thread_ = nullptr;
    }
}

void PCBBoardManagementWidget::setBoardManager(PCBBoardManager* manager)
{
    board_manager_ = manager;
    if (board_manager_) {
        connect(board_manager_, &PCBBoardManager::boardAdded, this, &PCBBoardManagementWidget::onBoardAdded);
        connect(board_manager_, &PCBBoardManager::boardUpdated, this, &PCBBoardManagementWidget::onBoardUpdated);
        connect(board_manager_, &PCBBoardManager::boardDeleted, this, &PCBBoardManagementWidget::onBoardDeleted);
        
        // 刷新界面
        updateBoardList();
    }
}

void PCBBoardManagementWidget::setCameraManager(CameraManager* cameraManager)
{
    camera_manager_ = cameraManager;
}

void PCBBoardManagementWidget::setupUI()
{
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    
    // 创建标签页
    main_tabs_ = new QTabWidget(this);
    
    // 板卡管理标签页
    setupBoardManagementTab();
    
    // 板卡识别标签页
    setupIdentificationTab();
    
    mainLayout->addWidget(main_tabs_);
}

void PCBBoardManagementWidget::setupBoardManagementTab()
{
    board_management_tab_ = new QWidget();
    main_tabs_->addTab(board_management_tab_, "板卡管理");
    
    QHBoxLayout* layout = new QHBoxLayout(board_management_tab_);
    layout->setContentsMargins(5, 5, 5, 5);
    
    board_splitter_ = new QSplitter(Qt::Horizontal, board_management_tab_);
      // 设置各个面板
    setupBoardListPanel();
    setupImageDisplayPanel();
    
    board_splitter_->addWidget(board_list_panel_);
    board_splitter_->addWidget(image_display_panel_);
    
    // 设置初始比例
    board_splitter_->setSizes({400, 600});
    
    layout->addWidget(board_splitter_);
}

void PCBBoardManagementWidget::setupIdentificationTab()
{
    identification_tab_ = new QWidget();
    main_tabs_->addTab(identification_tab_, "板卡识别");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(identification_tab_);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // 顶部控制面板
    identification_group_ = new QGroupBox("识别控制");
    identification_group_->setMaximumHeight(80);
    QHBoxLayout* controlLayout = new QHBoxLayout(identification_group_);
    
    identify_file_button_ = new QPushButton("从文件识别");
    identify_file_button_->setMinimumHeight(35);
    identify_file_button_->setStyleSheet(
        "QPushButton { font-size: 12px; font-weight: bold; background-color: #4CAF50; color: white; border: none; border-radius: 5px; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:pressed { background-color: #3d8b40; }"
    );
    
    identify_camera_button_ = new QPushButton("摄像头识别");
    identify_camera_button_->setMinimumHeight(35);
    identify_camera_button_->setStyleSheet(
        "QPushButton { font-size: 12px; font-weight: bold; background-color: #2196F3; color: white; border: none; border-radius: 5px; }"
        "QPushButton:hover { background-color: #1976D2; }"
        "QPushButton:pressed { background-color: #1565C0; }"
    );
    
    controlLayout->addWidget(identify_file_button_);
    controlLayout->addWidget(identify_camera_button_);
    controlLayout->addStretch();
    
    mainLayout->addWidget(identification_group_);
    
    // 主要内容区域
    QWidget* contentWidget = new QWidget();
    QHBoxLayout* contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setSpacing(15);
    
    // 左侧：图像显示区域
    QWidget* imageWidget = new QWidget();
    imageWidget->setMinimumWidth(400);
    imageWidget->setMaximumWidth(500);
    QVBoxLayout* imageLayout = new QVBoxLayout(imageWidget);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* imageTitle = new QLabel("待识别图像");
    imageTitle->setAlignment(Qt::AlignCenter);
    imageTitle->setStyleSheet(
        "font-weight: bold; font-size: 14px; color: #333; "
        "background-color: #f0f0f0; padding: 8px; border-radius: 5px; margin-bottom: 5px;"
    );
    imageLayout->addWidget(imageTitle);
    
    identification_image_label_ = new QLabel();
    identification_image_label_->setMinimumSize(380, 300);
    identification_image_label_->setScaledContents(false);
    identification_image_label_->setAlignment(Qt::AlignCenter);
    identification_image_label_->setStyleSheet(
        "border: 2px dashed #ccc; background-color: #fafafa; "
        "font-size: 12px; color: #666; border-radius: 8px;"
    );
    identification_image_label_->setText("点击\"从文件识别\"选择图片\n或点击\"摄像头识别\"使用摄像头");
    imageLayout->addWidget(identification_image_label_);
    
    // 图像信息显示
    QWidget* imageInfoWidget = new QWidget();
    QGridLayout* imageInfoLayout = new QGridLayout(imageInfoWidget);
    imageInfoLayout->setContentsMargins(5, 5, 5, 5);
    
    QLabel* sizeLabel = new QLabel("图像尺寸:");
    sizeLabel->setStyleSheet("font-weight: bold; color: #555;");
    sizeValueLabel = new QLabel("未加载");
    sizeValueLabel->setObjectName("imageSizeLabel");
    sizeValueLabel->setStyleSheet("color: #333;");
    
    QLabel* formatLabel = new QLabel("图像格式:");
    formatLabel->setStyleSheet("font-weight: bold; color: #555;");
    formatValueLabel = new QLabel("未知");
    formatValueLabel->setObjectName("imageFormatLabel");
    formatValueLabel->setStyleSheet("color: #333;");
    
    imageInfoLayout->addWidget(sizeLabel, 0, 0);
    imageInfoLayout->addWidget(sizeValueLabel, 0, 1);
    imageInfoLayout->addWidget(formatLabel, 1, 0);
    imageInfoLayout->addWidget(formatValueLabel, 1, 1);
    
    imageLayout->addWidget(imageInfoWidget);
    imageLayout->addStretch();
    
    contentLayout->addWidget(imageWidget);
    
    // 右侧：识别结果区域
    QWidget* resultsWidget = new QWidget();
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsWidget);
    resultsLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* resultsTitle = new QLabel("识别结果");
    resultsTitle->setAlignment(Qt::AlignCenter);
    resultsTitle->setStyleSheet(
        "font-weight: bold; font-size: 14px; color: #333; "
        "background-color: #f0f0f0; padding: 8px; border-radius: 5px; margin-bottom: 5px;"
    );
    resultsLayout->addWidget(resultsTitle);
    
    // 识别统计信息
    QWidget* statsWidget = new QWidget();
    QHBoxLayout* statsLayout = new QHBoxLayout(statsWidget);
    statsLayout->setContentsMargins(5, 5, 5, 5);
    
    resultsLayout->addWidget(statsWidget);
    
    // 候选板卡表格
    candidates_table_ = new QTableWidget();
    candidates_table_->setColumnCount(4);
    candidates_table_->setHorizontalHeaderLabels({"板卡名称", "型号", "相似度", "描述"});
    candidates_table_->horizontalHeader()->setStretchLastSection(true);
    candidates_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    candidates_table_->setAlternatingRowColors(true);
    candidates_table_->setGridStyle(Qt::SolidLine);
    candidates_table_->setStyleSheet(
        "QTableWidget { "
        "   background-color: white; "
        "   border: 1px solid #ddd; "
        "   border-radius: 5px; "
        "   gridline-color: #e0e0e0; "
        "} "
        "QTableWidget::item { "
        "   padding: 8px; "
        "   border: none; "
        "} "
        "QTableWidget::item:selected { "
        "   background-color: #e3f2fd; "
        "   color: #1976d2; "
        "} "
        "QHeaderView::section { "
        "   background-color: #f5f5f5; "
        "   padding: 8px; "
        "   border: none; "
        "   border-right: 1px solid #ddd; "
        "   font-weight: bold; "
        "   color: #333; "
        "}"
    );
    
    // 设置表格列宽
    candidates_table_->setColumnWidth(0, 120);
    candidates_table_->setColumnWidth(1, 100);
    candidates_table_->setColumnWidth(2, 80);
    
    resultsLayout->addWidget(candidates_table_);
    
    // 底部按钮区域
    QWidget* buttonWidget = new QWidget();
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setContentsMargins(0, 10, 0, 0);
    
    confirm_identification_button_ = new QPushButton("确认选择");
    confirm_identification_button_->setEnabled(false);
    confirm_identification_button_->setMinimumHeight(35);
    confirm_identification_button_->setStyleSheet(
        "QPushButton { "
        "   font-size: 12px; font-weight: bold; "
        "   background-color: #FF9800; color: white; "
        "   border: none; border-radius: 5px; "
        "   padding: 8px 20px; "
        "} "
        "QPushButton:hover { background-color: #F57C00; } "
        "QPushButton:pressed { background-color: #E65100; } "
        "QPushButton:disabled { background-color: #ccc; color: #666; }"
    );
    
    QPushButton* clearButton = new QPushButton("清除结果");
    clearButton->setMinimumHeight(35);
    clearButton->setStyleSheet(
        "QPushButton { "
        "   font-size: 12px; font-weight: bold; "
        "   background-color: #9E9E9E; color: white; "
        "   border: none; border-radius: 5px; "
        "   padding: 8px 20px; "
        "} "
        "QPushButton:hover { background-color: #757575; } "
        "QPushButton:pressed { background-color: #616161; }"
    );
    
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        candidates_table_->setRowCount(0);
        identification_candidates_.clear();
        confirm_identification_button_->setEnabled(false);
        identification_image_label_->clear();
        identification_image_label_->setText("点击\"从文件识别\"选择图片\n或点击\"摄像头识别\"使用摄像头");
        sizeValueLabel->setText("未加载");
        formatValueLabel->setText("未知");
        
    });
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(clearButton);
    buttonLayout->addWidget(confirm_identification_button_);
    
    resultsLayout->addWidget(buttonWidget);
    
    contentLayout->addWidget(resultsWidget);
    
    // 设置左右比例 (40% : 60%)
    contentLayout->setStretchFactor(imageWidget, 2);
    contentLayout->setStretchFactor(resultsWidget, 3);
    
    mainLayout->addWidget(contentWidget, 1);
}

void PCBBoardManagementWidget::setupBoardListPanel()
{
    board_list_panel_ = new QWidget();
    board_list_panel_->setMaximumWidth(400);
    board_list_panel_->setMinimumWidth(200);
    
    QVBoxLayout* layout = new QVBoxLayout(board_list_panel_);
      // 标题和工具栏
    setupToolbar();
    layout->addWidget(create_board_button_);
    
    // 第一行按钮：导入、导出
    QHBoxLayout* buttonLayout1 = new QHBoxLayout();
    buttonLayout1->addWidget(import_board_button_);
    buttonLayout1->addWidget(export_board_button_);
    layout->addLayout(buttonLayout1);
    
    // 第二行按钮：删除
    QHBoxLayout* buttonLayout2 = new QHBoxLayout();
    buttonLayout2->addWidget(delete_board_button_);
    layout->addLayout(buttonLayout2);
    
    // 搜索和过滤
    search_edit_ = new QLineEdit();
    search_edit_->setPlaceholderText("搜索板卡...");
    layout->addWidget(search_edit_);
    
    model_filter_combo_ = new QComboBox();
    model_filter_combo_->addItem("所有型号");
    layout->addWidget(model_filter_combo_);
    
    // 板卡表格
    board_table_ = new QTableWidget();
    board_table_->setColumnCount(3);
    board_table_->setHorizontalHeaderLabels({"名称", "型号", "元器件数"});
    board_table_->horizontalHeader()->setStretchLastSection(true);
    board_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    board_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(board_table_);
    
    // 统计信息
    setupStatisticsPanel();
    layout->addWidget(statistics_group_);
}

void PCBBoardManagementWidget::setupImageDisplayPanel()
{
    image_display_panel_ = new QWidget();
    
    QVBoxLayout* layout = new QVBoxLayout(image_display_panel_);
    
    // 图像工具栏
    QHBoxLayout* imageToolLayout = new QHBoxLayout();
    
    imageToolLayout->addStretch();
    
    layout->addLayout(imageToolLayout);
    
    // 创建水平分割器，左侧显示图像，右侧显示标签表格
    QSplitter* imageSplitter = new QSplitter(Qt::Horizontal);
    imageSplitter->setObjectName("imageSplitter");
    
    // 左侧：LabelEditing 图像编辑区域（初始为占位符）
    QWidget* imageWidget = new QWidget();
    imageWidget->setObjectName("imageEditingWidget");
    imageWidget->setMinimumSize(400, 300);
    imageWidget->setStyleSheet("border: 1px solid gray; background-color: #f0f0f0;");
    QVBoxLayout* imageLayout = new QVBoxLayout(imageWidget);
    QLabel* placeholderLabel = new QLabel("选择板卡以显示图片和标签编辑");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    imageLayout->addWidget(placeholderLabel);
    
    // 右侧：标签表格
    label_table_ = new QTableWidget();
    label_table_->setObjectName("label_table");
    label_table_->setMaximumWidth(500);
    label_table_->setMinimumWidth(200);
    
    imageSplitter->addWidget(imageWidget);
    imageSplitter->addWidget(label_table_);
    imageSplitter->setSizes({600, 300});
    
    layout->addWidget(imageSplitter);
}

void PCBBoardManagementWidget::setupToolbar()
{
    create_board_button_ = new QPushButton("新建板卡");
    import_board_button_ = new QPushButton("导入");
    export_board_button_ = new QPushButton("导出");
    delete_board_button_ = new QPushButton("删除");
    duplicate_board_button_ = new QPushButton("复制");
    export_all_button_ = new QPushButton("导出全部");
    
    // 为删除按钮设置红色样式，表示危险操作
    delete_board_button_->setStyleSheet(
        "QPushButton { "
        "   font-size: 12px; font-weight: bold; "
        "   background-color: #f44336; color: white; "
        "   border: none; border-radius: 4px; "
        "   padding: 6px 12px; "
        "} "
        "QPushButton:hover { background-color: #da190b; } "
        "QPushButton:pressed { background-color: #c62828; } "
        "QPushButton:disabled { background-color: #ccc; color: #666; }"
    );
}

void PCBBoardManagementWidget::setupStatisticsPanel()
{
    statistics_group_ = new QGroupBox("统计信息");
    QFormLayout* statsLayout = new QFormLayout(statistics_group_);
    
    total_boards_label_ = new QLabel("0");
    
    statsLayout->addRow("总板卡数:", total_boards_label_);
}

void PCBBoardManagementWidget::setupIdentificationPanel()
{
    // 在setupIdentificationTab中已实现
}

void PCBBoardManagementWidget::connectSignals()
{
    // 板卡管理按钮
    connect(create_board_button_, &QPushButton::clicked, this, &PCBBoardManagementWidget::onCreateBoard);
    connect(import_board_button_, &QPushButton::clicked, this, &PCBBoardManagementWidget::onImportBoard);
    connect(export_board_button_, &QPushButton::clicked, this, &PCBBoardManagementWidget::onExportBoard);
    connect(delete_board_button_, &QPushButton::clicked, this, &PCBBoardManagementWidget::onDeleteBoard);
    
    // 板卡识别按钮
    connect(identify_file_button_, &QPushButton::clicked, this, &PCBBoardManagementWidget::onIdentifyFromFile);
    connect(identify_camera_button_, &QPushButton::clicked, this, &PCBBoardManagementWidget::onIdentifyFromCamera);
    connect(confirm_identification_button_, &QPushButton::clicked, this, &PCBBoardManagementWidget::onConfirmIdentification);
      // 候选板卡表格选择事件
    connect(candidates_table_, &QTableWidget::currentCellChanged, 
            this, [this](int currentRow, int currentColumn, int previousRow, int previousColumn) {
                Q_UNUSED(currentColumn)
                Q_UNUSED(previousRow)
                Q_UNUSED(previousColumn)
                confirm_identification_button_->setEnabled(currentRow >= 0);
            });
    
    // 双击候选板卡直接确认
    connect(candidates_table_, &QTableWidget::itemDoubleClicked,
            this, &PCBBoardManagementWidget::onConfirmIdentification);
      // 表格选择
    connect(board_table_, &QTableWidget::currentItemChanged, this, &PCBBoardManagementWidget::onBoardSelectionChanged);
    
    // 搜索
    connect(search_edit_, &QLineEdit::textChanged, this, &PCBBoardManagementWidget::onSearchTextChanged);
    
      // 右键菜单
    connect(board_table_, &QTableWidget::customContextMenuRequested, this, &PCBBoardManagementWidget::onContextMenuRequested);
}

// 核心实现：板卡选择改变时创建LabelEditing
void PCBBoardManagementWidget::onBoardSelectionChanged(QTableWidgetItem* current, QTableWidgetItem* previous)
{
    Q_UNUSED(previous)
    
    qDebug() << "PCBBoardManagementWidget::onBoardSelectionChanged - 板卡选择改变";
    
    try {
        if (!current) {
            qDebug() << "PCBBoardManagementWidget::onBoardSelectionChanged - 当前选择为空";            
            current_board_id_.clear();
            updateImageDisplay();
            return;
        }
        
        QString boardId = current->data(Qt::UserRole).toString();
        qDebug() << "PCBBoardManagementWidget::onBoardSelectionChanged - 选中板卡ID:" << boardId;
        qDebug() << "PCBBoardManagementWidget::onBoardSelectionChanged - 当前行:" << current->row();
        qDebug() << "PCBBoardManagementWidget::onBoardSelectionChanged - 板卡名称:" << current->text();
        
        if (boardId.isEmpty()) {
            qDebug() << "PCBBoardManagementWidget::onBoardSelectionChanged - 板卡ID为空，尝试使用行索引";
            // 如果boardId为空，尝试从行数据获取
            if (board_table_ && current->row() >= 0) {
                QTableWidgetItem* nameItem = board_table_->item(current->row(), 0);
                if (nameItem) {
                    boardId = nameItem->data(Qt::UserRole).toString();
                    qDebug() << "PCBBoardManagementWidget::onBoardSelectionChanged - 从名称项获取ID:" << boardId;
                }
            }
        }
        
        if (boardId != current_board_id_) {
            current_board_id_ = boardId;
            qDebug() << "PCBBoardManagementWidget::onBoardSelectionChanged - 更新当前板卡ID为:" << current_board_id_;
            displayBoardImage(boardId);
        }
    } catch (const std::exception& e) {
        qDebug() << "PCBBoardManagementWidget::onBoardSelectionChanged - 异常:" << e.what();
        QMessageBox::warning(this, "错误", QString("选择板卡时发生错误: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "PCBBoardManagementWidget::onBoardSelectionChanged - 未知异常";
        QMessageBox::warning(this, "错误", "选择板卡时发生未知错误");
    }
}

void PCBBoardManagementWidget::displayBoardImage(const QString& boardId)
{
    qDebug() << "PCBBoardManagementWidget::displayBoardImage - 开始显示板卡图片，ID:" << boardId;
    
    if (!board_manager_ || boardId.isEmpty()) {
        qDebug() << "PCBBoardManagementWidget::displayBoardImage - 板卡管理器未初始化或ID为空";
        current_board_image_ = cv::Mat();
        current_components_.clear();
        updateImageDisplay();
        return;
    }
    
    PCBBoardInfo board = board_manager_->getBoardById(boardId);
    if (board.boardId.isEmpty()) {
        current_board_image_ = cv::Mat();
        current_components_.clear();
        updateImageDisplay();
        return;
    }
    
    current_board_image_ = board_manager_->loadImage(board.imagePath);
    
    if (current_board_image_.empty()) {
        current_components_.clear();
    } else {
        
        // 加载组件数据
        current_components_ = board_manager_->getComponents(boardId);
    }
    
    updateImageDisplay();
}

void PCBBoardManagementWidget::updateImageDisplay()
{
    qDebug() << "PCBBoardManagementWidget::updateImageDisplay - 更新图片显示";
    
    if (current_board_image_.empty()) {
        qDebug() << "PCBBoardManagementWidget::updateImageDisplay - 当前图片为空";
        // 清除现有的LabelEditing，显示占位符
        clearLabelEditing();
        return;
    }
    
    qDebug() << "PCBBoardManagementWidget::updateImageDisplay - 当前图片尺寸:" 
             << current_board_image_.cols << "x" << current_board_image_.rows;
    
    // 创建或更新LabelEditing
    createOrUpdateLabelEditing();
}

void PCBBoardManagementWidget::createOrUpdateLabelEditing()
{
    if (current_board_image_.empty()) {
        return;
    }
      // 转换Mat为QImage - 使用标准化的颜色转换
    QPixmap pixmap = matToQPixmapStandard(current_board_image_, false);
    if (pixmap.isNull()) {
        qDebug() << "PCBBoardManagementWidget::createOrUpdateLabelEditing - 图像转换失败";
        return;
    }
    QImage qimage = pixmap.toImage();
    
    // 准备标签数据
    std::vector<Label> existingLabels;
    std::vector<Label> newLabels;
    std::vector<int> deleteIds;
    
    // 将ComponentInfo转换为Label
    for (const ComponentInfo& component : current_components_) {
        Label label;
        label.id = component.labelInfo.id;
        label.label = component.componentName;
        label.x = component.labelInfo.x;
        label.y = component.labelInfo.y;
        label.w = component.labelInfo.w;
        label.h = component.labelInfo.h;
        label.cls = component.labelInfo.cls;
        label.confidence = component.labelInfo.confidence;
        label.position_number = component.componentName;
        label.notes = component.componentValue.toUtf8();
        label.syncFields(); // 同步兼容性字段
        
        existingLabels.push_back(label);
    }
    
    // 查找分割器
    QSplitter* imageSplitter = image_display_panel_->findChild<QSplitter*>("imageSplitter");
    if (!imageSplitter) {
        qDebug() << "PCBBoardManagementWidget::createOrUpdateLabelEditing - 找不到分割器";
        return;
    }    // 地清除旧的LabelEditing
    if (label_editing_) {
        label_editing_->setParent(nullptr);
        label_editing_->deleteLater();
        label_editing_ = nullptr;
    }
    
      try {        
        label_editing_ = new LabelEditing(this, qimage, existingLabels, 
                                        newLabels, deleteIds, label_table_);
        
        connect(label_editing_, &LabelEditing::labelAdded, 
                this, &PCBBoardManagementWidget::onLabelAdded);
        connect(label_editing_, &LabelEditing::labelUpdated, 
                this, &PCBBoardManagementWidget::onLabelUpdated);
        connect(label_editing_, &LabelEditing::labelDeleted, 
                this, &PCBBoardManagementWidget::onLabelDeleted);
    
        
        if (imageSplitter->count() >= 2) {
            QWidget* oldWidget = imageSplitter->widget(0);
            imageSplitter->replaceWidget(0, label_editing_);
            if (oldWidget && oldWidget->objectName() == "imageEditingWidget") {
                oldWidget->deleteLater();
            }
        } else if (imageSplitter->count() == 1) {
            imageSplitter->insertWidget(0, label_editing_);
        } else {
            imageSplitter->addWidget(label_editing_);
            if (imageSplitter->indexOf(label_table_) == -1) {
                imageSplitter->addWidget(label_table_);
            }
        }
        
        label_editing_->setMinimumSize(400, 300);
        
        if (imageSplitter->indexOf(label_table_) != 1) {
            label_table_->setParent(nullptr);
            imageSplitter->addWidget(label_table_);
        }
        
        imageSplitter->setSizes({600, 300});
        
    } catch (const std::exception& e) {
        qDebug() << "PCBBoardManagementWidget::createOrUpdateLabelEditing - 创建LabelEditing时出错:" << e.what();
    } catch (...) {
        qDebug() << "PCBBoardManagementWidget::createOrUpdateLabelEditing - 创建LabelEditing时出现未知错误";
    }
}

void PCBBoardManagementWidget::clearLabelEditing()
{
    // 查找分割器
    QSplitter* imageSplitter = image_display_panel_->findChild<QSplitter*>("imageSplitter");
    if (!imageSplitter) {
        return;
    }
    
    // 安全地删除现有的LabelEditing
    if (label_editing_) {
        qDebug() << "PCBBoardManagementWidget::clearLabelEditing - 删除LabelEditing";
        label_editing_->setParent(nullptr);
        label_editing_->deleteLater();
        label_editing_ = nullptr;
    }
    
    // 创建新的占位符widget
    QWidget* placeholderWidget = new QWidget();
    placeholderWidget->setObjectName("imageEditingWidget");
    placeholderWidget->setMinimumSize(400, 300);
    placeholderWidget->setStyleSheet("border: 1px solid gray; background-color: #f0f0f0;");
    QVBoxLayout* layout = new QVBoxLayout(placeholderWidget);
    QLabel* placeholderLabel = new QLabel("选择板卡以显示图片和标签编辑");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholderLabel);
    
    // 确保分割器中有正确的widget数量和顺序
    if (imageSplitter->count() >= 2) {
        // 替换第一个widget（保持label_table_在第二个位置）
        QWidget* oldWidget = imageSplitter->widget(0);
        imageSplitter->replaceWidget(0, placeholderWidget);
        if (oldWidget) {
            oldWidget->deleteLater();
        }
    } else if (imageSplitter->count() == 1) {
        // 如果只有一个widget，在前面插入占位符
        imageSplitter->insertWidget(0, placeholderWidget);
    } else {
        // 如果没有widget，先添加占位符，再确保label_table_在第二个位置
        imageSplitter->addWidget(placeholderWidget);
        if (imageSplitter->indexOf(label_table_) == -1) {
            imageSplitter->addWidget(label_table_);
        }
    }
    
    // 确保label_table_在正确的位置
    if (imageSplitter->indexOf(label_table_) != 1) {
        // 如果label_table_不在第二个位置，移动它
        label_table_->setParent(nullptr);
        imageSplitter->addWidget(label_table_);
    }
    
    // 重新设置分割器比例
    imageSplitter->setSizes({600, 300});
    
    qDebug() << "PCBBoardManagementWidget::clearLabelEditing - 完成，分割器widget数量:" << imageSplitter->count();
}

// 板卡创建方法实现
void PCBBoardManagementWidget::onCreateBoard()
{
    if (!board_manager_) {
        QMessageBox::warning(this, "错误", "板卡管理器未初始化！");
        return;
    }
    
    // 创建新建板卡对话框
    QDialog* createDialog = new QDialog(this);
    createDialog->setWindowTitle("新建板卡");
    createDialog->setModal(true);
    createDialog->resize(600, 700);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(createDialog);
    
    // 基本信息输入区域
    QGroupBox* infoGroup = new QGroupBox("板卡基本信息");
    QFormLayout* infoLayout = new QFormLayout(infoGroup);
    
    QLineEdit* nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("请输入板卡名称");
    QLineEdit* modelEdit = new QLineEdit();
    modelEdit->setPlaceholderText("请输入板卡型号");
    QTextEdit* descEdit = new QTextEdit();
    descEdit->setPlaceholderText("请输入板卡描述（可选）");
    descEdit->setMaximumHeight(80);
    
    infoLayout->addRow("板卡名称*:", nameEdit);
    infoLayout->addRow("板卡型号*:", modelEdit);
    infoLayout->addRow("描述:", descEdit);
    
    mainLayout->addWidget(infoGroup);
    
    // 图像获取区域
    QGroupBox* imageGroup = new QGroupBox("板卡图像");
    QVBoxLayout* imageLayout = new QVBoxLayout(imageGroup);
    
    // 图像获取按钮
    QHBoxLayout* imageButtonLayout = new QHBoxLayout();
    QPushButton* fromFileButton = new QPushButton("从文件选择");
    fromFileButton->setStyleSheet(
        "QPushButton { font-size: 12px; font-weight: bold; background-color: #4CAF50; color: white; "
        "border: none; border-radius: 5px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:pressed { background-color: #3d8b40; }"
    );
    
    QPushButton* fromCameraButton = new QPushButton("摄像头拍摄");
    fromCameraButton->setStyleSheet(
        "QPushButton { font-size: 12px; font-weight: bold; background-color: #2196F3; color: white; "
        "border: none; border-radius: 5px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #1976D2; }"
        "QPushButton:pressed { background-color: #1565C0; }"
    );
    
    imageButtonLayout->addWidget(fromFileButton);
    imageButtonLayout->addWidget(fromCameraButton);
    imageButtonLayout->addStretch();
    
    imageLayout->addLayout(imageButtonLayout);
    
    // 图像预览区域
    QLabel* imagePreview = new QLabel();
    imagePreview->setMinimumSize(500, 300);
    imagePreview->setMaximumSize(500, 300);
    imagePreview->setScaledContents(false);
    imagePreview->setAlignment(Qt::AlignCenter);
    imagePreview->setStyleSheet(
        "border: 2px dashed #ccc; background-color: #fafafa; "
        "font-size: 14px; color: #666; border-radius: 8px;"
    );
    imagePreview->setText("请选择或拍摄板卡图像\n支持格式: PNG, JPG, BMP, TIFF");
    
    imageLayout->addWidget(imagePreview);
    
    // 图像信息显示
    QHBoxLayout* imageInfoLayout = new QHBoxLayout();
    QLabel* imageSizeLabel = new QLabel("图像尺寸: 未加载");
    QLabel* imageFormatLabel = new QLabel("格式: 未知");
    imageSizeLabel->setStyleSheet("color: #666;");
    imageFormatLabel->setStyleSheet("color: #666;");
    
    imageInfoLayout->addWidget(imageSizeLabel);
    imageInfoLayout->addStretch();
    imageInfoLayout->addWidget(imageFormatLabel);
    
    imageLayout->addLayout(imageInfoLayout);
    
    mainLayout->addWidget(imageGroup);
    
    // 对话框按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* createButton = new QPushButton("创建板卡");
    createButton->setEnabled(false); // 初始状态禁用
    createButton->setStyleSheet(
        "QPushButton { font-size: 12px; font-weight: bold; background-color: #FF9800; color: white; "
        "border: none; border-radius: 5px; padding: 10px 20px; }"
        "QPushButton:hover { background-color: #F57C00; }"
        "QPushButton:pressed { background-color: #E65100; }"
        "QPushButton:disabled { background-color: #ccc; color: #666; }"
    );
    
    QPushButton* cancelButton = new QPushButton("取消");
    cancelButton->setStyleSheet(
        "QPushButton { font-size: 12px; font-weight: bold; background-color: #9E9E9E; color: white; "
        "border: none; border-radius: 5px; padding: 10px 20px; }"
        "QPushButton:hover { background-color: #757575; }"
        "QPushButton:pressed { background-color: #616161; }"
    );
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(createButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // 存储选择的图像
    cv::Mat selectedImage;
    QString imageFilePath;
    
    // 验证输入完整性的函数
    auto validateInput = [&]() {
        bool hasName = !nameEdit->text().trimmed().isEmpty();
        bool hasModel = !modelEdit->text().trimmed().isEmpty();
        bool hasImage = !selectedImage.empty();
        
        createButton->setEnabled(hasName && hasModel && hasImage);
        
        if (hasName && hasModel && hasImage) {
            createButton->setToolTip("点击创建板卡");
        } else {
            QStringList missing;
            if (!hasName) missing << "板卡名称";
            if (!hasModel) missing << "板卡型号";
            if (!hasImage) missing << "板卡图像";
            createButton->setToolTip(QString("请补充: %1").arg(missing.join(", ")));
        }
    };
    
    // 连接输入验证
    connect(nameEdit, &QLineEdit::textChanged, validateInput);
    connect(modelEdit, &QLineEdit::textChanged, validateInput);
    
    // 从文件选择图像
    connect(fromFileButton, &QPushButton::clicked, [&]() {
        QString fileName = QFileDialog::getOpenFileName(createDialog,
            "选择板卡图像",
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
            "图片文件 (*.png *.jpg *.jpeg *.bmp *.tiff *.tif);;所有文件 (*.*)");
        
        if (!fileName.isEmpty()) {
            cv::Mat loadedImage = cv::imread(fileName.toStdString());
            if (!loadedImage.empty()) {
                selectedImage = loadedImage.clone();
                imageFilePath = fileName;
                
                // 更新预览
                cv::Mat rgbMat;
                cv::cvtColor(selectedImage, rgbMat, cv::COLOR_BGR2RGB);
                selectedImage = rgbMat;
                QPixmap pixmap = matToQPixmapStandard(selectedImage, false);
                if (!pixmap.isNull()) {
                    QSize previewSize = imagePreview->size();
                    pixmap = pixmap.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    imagePreview->setPixmap(pixmap);
                }
                
                // 更新图像信息
                imageSizeLabel->setText(QString("图像尺寸: %1 x %2")
                    .arg(selectedImage.cols).arg(selectedImage.rows));
                
                QFileInfo fileInfo(fileName);
                imageFormatLabel->setText(QString("格式: %1").arg(fileInfo.suffix().toUpper()));
                
                qDebug() << "从文件加载图像成功:" << fileName;
                qDebug() << "图像尺寸:" << selectedImage.cols << "x" << selectedImage.rows;
                
                validateInput();
            } else {
                QMessageBox::warning(createDialog, "错误", "无法加载图像文件！\n请检查文件格式是否支持。");
            }
        }
    });
    
    // 从摄像头拍摄图像
    connect(fromCameraButton, &QPushButton::clicked, [&]() {
        if (!camera_manager_) {
            QMessageBox::warning(createDialog, "错误", "摄像头管理器未初始化！");
            return;
        }
        
        // 检查摄像头是否可用
        if (!camera_manager_->isCameraAvailable(CameraType::HD_CAMERA)) {
            int ret = QMessageBox::question(createDialog, "摄像头未连接",
                "高清摄像头尚未连接，是否现在初始化？",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                
            if (ret == QMessageBox::Yes) {
                if (!camera_manager_->initializeCamera(CameraType::HD_CAMERA)) {
                    QMessageBox::warning(createDialog, "错误", "摄像头初始化失败！\n请检查摄像头连接。");
                    return;
                }
            } else {
                return;
            }
        }
        
        // 创建摄像头捕获对话框
        QDialog* captureDialog = new QDialog(createDialog);
        captureDialog->setWindowTitle("摄像头拍摄");
        captureDialog->setModal(true);
        captureDialog->resize(680, 580);
        
        QVBoxLayout* captureLayout = new QVBoxLayout(captureDialog);
        
        // 摄像头预览标签
        QLabel* cameraPreview = new QLabel();
        cameraPreview->setMinimumSize(640, 480);
        cameraPreview->setStyleSheet("border: 1px solid gray; background-color: black;");
        cameraPreview->setAlignment(Qt::AlignCenter);
        cameraPreview->setText("正在启动摄像头...");
        captureLayout->addWidget(cameraPreview);
        
        // 拍摄按钮布局
        QHBoxLayout* captureButtonLayout = new QHBoxLayout();
        QPushButton* takePhotoButton = new QPushButton("拍摄");
        takePhotoButton->setStyleSheet(
            "QPushButton { font-size: 14px; font-weight: bold; background-color: #4CAF50; color: white; "
            "border: none; border-radius: 6px; padding: 10px 25px; }"
            "QPushButton:hover { background-color: #45a049; }"
            "QPushButton:pressed { background-color: #3d8b40; }"
        );
        
        QPushButton* retakeButton = new QPushButton("重新拍摄");
        retakeButton->setEnabled(false);
        retakeButton->setStyleSheet(
            "QPushButton { font-size: 14px; font-weight: bold; background-color: #FF9800; color: white; "
            "border: none; border-radius: 6px; padding: 10px 25px; }"
            "QPushButton:hover { background-color: #F57C00; }"
            "QPushButton:pressed { background-color: #E65100; }"
            "QPushButton:disabled { background-color: #ccc; color: #666; }"
        );
        
        QPushButton* confirmButton = new QPushButton("确认使用");
        confirmButton->setEnabled(false);
        confirmButton->setStyleSheet(
            "QPushButton { font-size: 14px; font-weight: bold; background-color: #2196F3; color: white; "
            "border: none; border-radius: 6px; padding: 10px 25px; }"
            "QPushButton:hover { background-color: #1976D2; }"
            "QPushButton:pressed { background-color: #1565C0; }"
            "QPushButton:disabled { background-color: #ccc; color: #666; }"
        );
        
        QPushButton* closeCameraButton = new QPushButton("取消");
        closeCameraButton->setStyleSheet(
            "QPushButton { font-size: 14px; font-weight: bold; background-color: #9E9E9E; color: white; "
            "border: none; border-radius: 6px; padding: 10px 25px; }"
            "QPushButton:hover { background-color: #757575; }"
            "QPushButton:pressed { background-color: #616161; }"
        );
        
        captureButtonLayout->addStretch();
        captureButtonLayout->addWidget(takePhotoButton);
        captureButtonLayout->addWidget(retakeButton);
        captureButtonLayout->addWidget(confirmButton);
        captureButtonLayout->addWidget(closeCameraButton);
        captureLayout->addLayout(captureButtonLayout);
        
        // 存储拍摄的图像
        cv::Mat capturedImage;
        bool photoTaken = false;
        
        // 创建定时器来更新预览
        QTimer* cameraTimer = new QTimer(captureDialog);
        connect(cameraTimer, &QTimer::timeout, [this, cameraPreview, &photoTaken]() {
            if (!photoTaken) {
                try {
                    ImageData imageData = camera_manager_->getLatestImage(CameraType::HD_CAMERA);
                    if (imageData.isValid && !imageData.image.empty()) {
                        QPixmap pixmap = matToQPixmapStandard(imageData.image, false);
                        if (!pixmap.isNull()) {
                            QSize previewSize = cameraPreview->size();
                            pixmap = pixmap.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                            cameraPreview->setPixmap(pixmap);
                        }
                    }
                } catch (const std::exception& e) {
                    qDebug() << "摄像头预览更新错误:" << e.what();
                }
            }
        });
        
        // 拍摄按钮事件
        connect(takePhotoButton, &QPushButton::clicked, [&]() {
            try {
                ImageData imageData = camera_manager_->getLatestImage(CameraType::HD_CAMERA);
                if (imageData.isValid && !imageData.image.empty()) {
                    capturedImage = imageData.image.clone();
                    photoTaken = true;
                    
                    // 显示拍摄的图像
                    QPixmap pixmap = matToQPixmapStandard(capturedImage, false);
                    if (!pixmap.isNull()) {
                        QSize previewSize = cameraPreview->size();
                        pixmap = pixmap.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        cameraPreview->setPixmap(pixmap);
                    }
                    
                    // 更新按钮状态
                    takePhotoButton->setEnabled(false);
                    retakeButton->setEnabled(true);
                    confirmButton->setEnabled(true);
                    
                    qDebug() << "拍摄成功，图像尺寸:" << capturedImage.cols << "x" << capturedImage.rows;
                } else {
                    QMessageBox::warning(captureDialog, "错误", "无法获取摄像头图像！");
                }
            } catch (const std::exception& e) {
                QMessageBox::warning(captureDialog, "错误",
                    QString("拍摄图像时发生错误：\n%1").arg(e.what()));
            }
        });
        
        // 重新拍摄按钮事件
        connect(retakeButton, &QPushButton::clicked, [&]() {
            photoTaken = false;
            capturedImage = cv::Mat();
            
            // 恢复按钮状态
            takePhotoButton->setEnabled(true);
            retakeButton->setEnabled(false);
            confirmButton->setEnabled(false);
            
            cameraPreview->setText("正在启动摄像头...");
        });
        
        // 确认使用按钮事件
        connect(confirmButton, &QPushButton::clicked, [&]() {
            if (!capturedImage.empty()) {
                captureDialog->accept();
            }
        });
        
        // 取消按钮事件
        connect(closeCameraButton, &QPushButton::clicked, captureDialog, &QDialog::reject);
        
        // 启动摄像头预览
        try {
            if (camera_manager_->startCamera(CameraType::HD_CAMERA)) {
                cameraTimer->start(33); // ~30 FPS
                cameraPreview->setText("");
            } else {
                cameraPreview->setText("无法启动摄像头预览");
                takePhotoButton->setEnabled(false);
            }
        } catch (const std::exception& e) {
            cameraPreview->setText("摄像头启动失败");
            takePhotoButton->setEnabled(false);
            qDebug() << "摄像头启动错误:" << e.what();
        }
        
        // 显示摄像头对话框
        int cameraResult = captureDialog->exec();
        
        // 停止预览和摄像头
        cameraTimer->stop();
        camera_manager_->stopCamera(CameraType::HD_CAMERA);
        
        // 处理拍摄结果
        if (cameraResult == QDialog::Accepted && !capturedImage.empty()) {
            selectedImage = capturedImage.clone();
            imageFilePath.clear(); // 摄像头拍摄的图像没有文件路径
            
            // 更新预览
            QPixmap pixmap = matToQPixmapStandard(selectedImage, false);
            if (!pixmap.isNull()) {
                QSize previewSize = imagePreview->size();
                pixmap = pixmap.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imagePreview->setPixmap(pixmap);
            }
            
            // 更新图像信息
            imageSizeLabel->setText(QString("图像尺寸: %1 x %2")
                .arg(selectedImage.cols).arg(selectedImage.rows));
            imageFormatLabel->setText("格式: 摄像头拍摄 (BGR)");
            
            qDebug() << "摄像头拍摄图像成功，尺寸:" << selectedImage.cols << "x" << selectedImage.rows;
            
            validateInput();
        }
        
        // 清理摄像头对话框
        captureDialog->deleteLater();
    });
    
    connect(createButton, &QPushButton::clicked, [&]() {
        QString boardName = nameEdit->text().trimmed();
        QString boardModel = modelEdit->text().trimmed();
        QString description = descEdit->toPlainText().trimmed();

        if (boardName.isEmpty() || boardModel.isEmpty() || selectedImage.empty()) {
            QMessageBox::warning(createDialog, "错误", "请填写完整的板卡信息并选择图像！");
            return;
        }

        // 创建并显示进度对话框
        QProgressDialog* progressDialog = new QProgressDialog("正在创建板卡...", nullptr, 0, 100, createDialog);
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setValue(20);
        progressDialog->show();
        createButton->setEnabled(false);

        // 连接进度信号
        QMetaObject::Connection connProgress = connect(board_manager_, &PCBBoardManager::boardCreationProgress,
            progressDialog, [progressDialog](int pct, const QString& msg) {
                progressDialog->setValue(pct);
                progressDialog->setLabelText(msg);
            });

        // 连接完成和错误信号
        QMetaObject::Connection connFinished, connError;
        connFinished = connect(board_manager_, &PCBBoardManager::boardCreationFinished,
            this, [progressDialog, createDialog, createButton, boardName, boardModel, this](const QString& createdBoardId) {
                // 关闭进度对话框并显示结果
                progressDialog->close();
                QMessageBox::information(createDialog, "成功",
                    QString("板卡 \"%1\" 创建成功！\n板卡ID: %2").arg(boardName).arg(createdBoardId));
                qDebug() << "板卡创建成功 - 名称:" << boardName << "型号:" << boardModel << "ID:" << createdBoardId;
                updateBoardList();
                createDialog->accept();
            });
        connError = connect(board_manager_, &PCBBoardManager::boardCreationError,
            this, [progressDialog, createDialog, createButton, boardName, boardModel, this](const QString& errorMsg) {
                // 关闭进度对话框并提示错误
                progressDialog->close();
                QMessageBox::warning(createDialog, "错误",
                    QString("板卡创建失败：%1").arg(errorMsg));
                createButton->setEnabled(true);
            });

        // 发起异步创建
        board_manager_->createBoardAsync(boardName, boardModel, selectedImage, description);
    });
    
    // 取消按钮事件
    connect(cancelButton, &QPushButton::clicked, createDialog, &QDialog::reject);
    
    // 显示对话框
    int result = createDialog->exec();
    
    // 清理对话框
    createDialog->deleteLater();
    
    if (result == QDialog::Accepted) {
        qDebug() << "板卡创建对话框已确认";
    } else {
        qDebug() << "板卡创建已取消";
    }
}

void PCBBoardManagementWidget::onImportBoard()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
        "导入板卡配置", "", "JSON Files (*.json)");
    
    if (!fileName.isEmpty() && board_manager_) {
        if (board_manager_->importBoard(fileName)) {
            QMessageBox::information(this, "成功", "板卡导入成功！");
            updateBoardList();
        } else {
            QMessageBox::warning(this, "错误", "板卡导入失败！");
        }
    }
}

void PCBBoardManagementWidget::onExportBoard()
{
    if (current_board_id_.isEmpty()) {
        QMessageBox::warning(this, "错误", "请先选择要导出的板卡！");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, 
        "导出板卡配置", "", "JSON Files (*.json)");
    
    if (!fileName.isEmpty() && board_manager_) {
        if (board_manager_->exportBoard(current_board_id_, fileName)) {
            QMessageBox::information(this, "成功", "板卡导出成功！");
        } else {
            QMessageBox::warning(this, "错误", "板卡导出失败！");
        }
    }
}

void PCBBoardManagementWidget::onDeleteBoard()
{
    if (current_board_id_.isEmpty()) {
        QMessageBox::warning(this, "错误", "请先选择要删除的板卡！");
        return;
    }
    
    int ret = QMessageBox::question(this, "确认删除", 
        "确定要删除选中的板卡吗？\n此操作不可撤销。",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (ret == QMessageBox::Yes && board_manager_) {
        if (board_manager_->deleteBoard(current_board_id_)) {            
            QMessageBox::information(this, "成功", "板卡删除成功！");
            current_board_id_.clear();
            updateImageDisplay();
            updateBoardList();  // 刷新板卡列表
        } else {
            QMessageBox::warning(this, "错误", "板卡删除失败！");
        }
    }
}

void PCBBoardManagementWidget::onDuplicateBoard()
{
    QMessageBox::information(this, "提示", "复制板卡功能开发中...");
}

void PCBBoardManagementWidget::onExportAllBoards()
{
    QMessageBox::information(this, "提示", "导出全部功能开发中...");
}

void PCBBoardManagementWidget::onIdentifyFromFile()
{
    if (!board_manager_) {
        QMessageBox::warning(this, "错误", "板卡管理器未初始化！");
        return;
    }
    
    // 打开文件选择对话框
    QString fileName = QFileDialog::getOpenFileName(this, 
        "选择要识别的PCB图片", 
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.tiff *.tif);;所有文件 (*.*)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // 快速加载图片进行预览
    cv::Mat inputImage = cv::imread(fileName.toStdString());
    if (inputImage.empty()) {
        QMessageBox::warning(this, "错误", "无法加载图片文件！\n请检查文件格式是否支持。");
        return;
    }
    
    // 立即显示图片预览
    QPixmap pixmap = matToQPixmap(inputImage);
    if (!pixmap.isNull()) {
        QSize labelSize = identification_image_label_->size();
        if (labelSize.width() > 50 && labelSize.height() > 50) {
            pixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } else {
            pixmap = pixmap.scaled(QSize(400, 300), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        identification_image_label_->setPixmap(pixmap);
        sizeValueLabel->setText(QString("图像尺寸: %1x%2").arg(pixmap.width()).arg(pixmap.height()));
        formatValueLabel->setText(QString("格式: %1").arg(fileName.split('.').last()));
    }
    
    // 切换到识别标签页
    main_tabs_->setCurrentWidget(identification_tab_);
    
    // 创建进度对话框
    QProgressDialog* progressDialog = new QProgressDialog("正在识别板卡...", "取消", 0, 100, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setMinimumDuration(0);
    progressDialog->setValue(20);
    progressDialog->show();
      // 清理旧的识别线程
    if (identification_worker_thread_) {
        identification_worker_thread_->quit();
        identification_worker_thread_->deleteLater();
        identification_worker_thread_ = nullptr;
    }
    
    // 创建新的工作线程
    identification_worker_thread_ = new QThread(this);
    IdentificationWorker* worker = new IdentificationWorker(board_manager_, inputImage, 0.7);
    worker->moveToThread(identification_worker_thread_);
      // 连接信号槽
    connect(identification_worker_thread_, &QThread::started, worker, &IdentificationWorker::doWork);
    connect(worker, &IdentificationWorker::finished, this, [this, progressDialog](const QList<PCBBoardInfo>& candidates) {
        progressDialog->setValue(100);
        progressDialog->close();
        progressDialog->deleteLater();
        
        // 显示识别结果
        displayIdentificationResults(candidates);
        
        qDebug() << "板卡识别完成，找到" << candidates.size() << "个候选板卡";
        
        if (candidates.isEmpty()) {
            QMessageBox::information(this, "识别结果", 
                "未找到匹配的板卡。\n\n建议：\n"
                "1. 检查图片质量是否清晰\n"
                "2. 确认板卡是否已在数据库中\n"
                "3. 尝试调整图片角度或光照条件");
        } else {
            QMessageBox::information(this, "识别成功", 
                QString("识别完成！找到 %1 个候选板卡。\n请在候选列表中选择最合适的板卡。")
                .arg(candidates.size()));
        }
        
        // 异步清理线程 - 不要在信号槽中调用wait()
        if (identification_worker_thread_) {
            identification_worker_thread_->quit();
            identification_worker_thread_->deleteLater();
            identification_worker_thread_ = nullptr;
        }
    });
    
    connect(worker, &IdentificationWorker::error, this, [this, progressDialog](const QString& errorMsg) {
        progressDialog->close();
        progressDialog->deleteLater();
        
        QMessageBox::critical(this, "识别错误", 
            QString("识别过程中发生错误：\n%1").arg(errorMsg));
        
        // 异步清理线程 - 不要在信号槽中调用wait()
        if (identification_worker_thread_) {
            identification_worker_thread_->quit();
            identification_worker_thread_->deleteLater();
            identification_worker_thread_ = nullptr;
        }
    });
      connect(progressDialog, &QProgressDialog::canceled, this, [this, progressDialog]() {
        if (identification_worker_thread_) {
            identification_worker_thread_->quit();
            identification_worker_thread_->deleteLater();
            identification_worker_thread_ = nullptr;
        }
        progressDialog->close();
        qDebug() << "用户取消了板卡识别";
    });
    
    // 设置进度更新定时器
    QTimer* progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, [progressDialog, progressTimer]() {
        int currentValue = progressDialog->value();
        if (currentValue < 90) {
            progressDialog->setValue(currentValue + 5);
        }
        if (progressDialog->wasCanceled()) {
            progressTimer->stop();
            progressTimer->deleteLater();
        }
    });
    
    connect(worker, &IdentificationWorker::finished, progressTimer, [progressTimer]() {
        progressTimer->stop();
        progressTimer->deleteLater();
    });
    
    connect(worker, &IdentificationWorker::error, progressTimer, [progressTimer]() {
        progressTimer->stop();
        progressTimer->deleteLater();
    });
      connect(worker, &IdentificationWorker::finished, worker, &IdentificationWorker::deleteLater);
    connect(worker, &IdentificationWorker::error, worker, &IdentificationWorker::deleteLater);
    
    // 确保线程在工作完成后自动清理
    connect(identification_worker_thread_, &QThread::finished, identification_worker_thread_, &QThread::deleteLater);
    
    progressTimer->start(200); // 每200ms更新一次进度
    
    // 启动线程
    identification_worker_thread_->start();
}

void PCBBoardManagementWidget::onIdentifyFromCamera()
{
    if (!board_manager_) {
        QMessageBox::warning(this, "错误", "板卡管理器未初始化！");
        return;
    }
    
    if (!camera_manager_) {
        QMessageBox::warning(this, "错误", "摄像头管理器未初始化！");
        return;
    }
    
    // 检查摄像头是否可用
    if (!camera_manager_->isCameraAvailable(CameraType::HD_CAMERA)) {
        int ret = QMessageBox::question(this, "摄像头未连接", 
            "高清摄像头尚未连接，是否现在初始化？",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            
        if (ret == QMessageBox::Yes) {
            if (!camera_manager_->initializeCamera(CameraType::HD_CAMERA)) {
                QMessageBox::warning(this, "错误", "摄像头初始化失败！\n请检查摄像头连接。");
                return;
            }
        } else {
            return;
        }
    }
    
    // 创建摄像头捕获对话框
    QDialog* captureDialog = new QDialog(this);
    captureDialog->setWindowTitle("摄像头捕获");
    captureDialog->setModal(true);
    captureDialog->resize(640, 520);
    
    QVBoxLayout* layout = new QVBoxLayout(captureDialog);
    
    // 摄像头预览标签
    QLabel* previewLabel = new QLabel();
    previewLabel->setMinimumSize(640, 480);
    previewLabel->setStyleSheet("border: 1px solid gray; background-color: black;");
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setText("正在启动摄像头...");
    layout->addWidget(previewLabel);
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* captureButton = new QPushButton("捕获图像");
    QPushButton* cancelButton = new QPushButton("取消");
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(captureButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    // 存储捕获的图像
    cv::Mat capturedImage;
    bool imageCaptured = false;
      // 创建定时器来更新预览
    QTimer* previewTimer = new QTimer(captureDialog);
    connect(previewTimer, &QTimer::timeout, [this, previewLabel]() {
        try {
            ImageData imageData = camera_manager_->getLatestImage(CameraType::HD_CAMERA);
            if (imageData.isValid && !imageData.image.empty()) {
                QPixmap pixmap = matToQPixmapStandard(imageData.image, false);
                if (!pixmap.isNull()) {
                    // 缩放图像以适应标签大小
                    QSize labelSize = previewLabel->size();
                    pixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    previewLabel->setPixmap(pixmap);
                }
            }
        } catch (const std::exception& e) {
            qDebug() << "摄像头预览更新错误:" << e.what();
        }
    });
      // 连接按钮事件
    connect(captureButton, &QPushButton::clicked, [&capturedImage, &imageCaptured, this, captureDialog]() {
        try {
            ImageData imageData = camera_manager_->getLatestImage(CameraType::HD_CAMERA);
            if (imageData.isValid && !imageData.image.empty()) {
                capturedImage = imageData.image.clone();
                imageCaptured = true;
                captureDialog->accept();
            } else {
                QMessageBox::warning(captureDialog, "错误", "无法获取摄像头图像！");
            }
        } catch (const std::exception& e) {
            QMessageBox::warning(captureDialog, "错误", 
                QString("捕获图像时发生错误：\n%1").arg(e.what()));
        }
    });
    
    connect(cancelButton, &QPushButton::clicked, captureDialog, &QDialog::reject);
      // 启动摄像头预览
    try {
        if (camera_manager_->startCamera(CameraType::HD_CAMERA)) {
            previewTimer->start(33); // ~30 FPS
            previewLabel->setText("");
        } else {
            previewLabel->setText("无法启动摄像头预览");
            captureButton->setEnabled(false);
        }
    } catch (const std::exception& e) {
        previewLabel->setText("摄像头启动失败");
        captureButton->setEnabled(false);
        qDebug() << "摄像头启动错误:" << e.what();
    }
    
    // 显示对话框
    int result = captureDialog->exec();
    
    // 停止预览
    previewTimer->stop();
    camera_manager_->stopCamera(CameraType::HD_CAMERA);
    
    // 清理对话框
    captureDialog->deleteLater();
    
    // 如果用户取消或没有捕获图像，退出
    if (result != QDialog::Accepted || !imageCaptured || capturedImage.empty()) {
        return;
    }
    
    // 显示捕获的图像预览
    QPixmap pixmap = matToQPixmap(capturedImage);
    if (!pixmap.isNull()) {
        QSize labelSize = identification_image_label_->size();
        if (labelSize.width() > 50 && labelSize.height() > 50) {
            pixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } else {
            pixmap = pixmap.scaled(QSize(400, 300), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        identification_image_label_->setPixmap(pixmap);
        sizeValueLabel->setText(QString("图像尺寸: %1x%2").arg(pixmap.width()).arg(pixmap.height()));
        formatValueLabel->setText(QString("格式: %1").arg("CV::Mat"));
    }
    
    // 切换到识别标签页
    main_tabs_->setCurrentWidget(identification_tab_);
    
    // 创建进度对话框
    QProgressDialog* progressDialog = new QProgressDialog("正在识别板卡...", "取消", 0, 100, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setMinimumDuration(0);
    progressDialog->setValue(20);
    progressDialog->show();
    
    // 清理旧的识别线程
    if (identification_worker_thread_) {
        identification_worker_thread_->quit();
        identification_worker_thread_->deleteLater();
        identification_worker_thread_ = nullptr;
    }
    
    // 创建新的工作线程
    identification_worker_thread_ = new QThread(this);
    IdentificationWorker* worker = new IdentificationWorker(board_manager_, capturedImage, 5);
    worker->moveToThread(identification_worker_thread_);
    
    // 连接信号槽
    connect(identification_worker_thread_, &QThread::started, worker, &IdentificationWorker::doWork);
    connect(worker, &IdentificationWorker::finished, this, [this, progressDialog](const QList<PCBBoardInfo>& candidates) {
        progressDialog->setValue(100);
        progressDialog->close();
        progressDialog->deleteLater();
        
        // 显示识别结果
        displayIdentificationResults(candidates);
        
        qDebug() << "摄像头板卡识别完成，找到" << candidates.size() << "个候选板卡";
        
        if (candidates.isEmpty()) {
            QMessageBox::information(this, "识别结果", 
                "未找到匹配的板卡。\n\n建议：\n"
                "1. 调整摄像头角度和距离\n"
                "2. 改善光照条件\n"
                "3. 确认板卡是否已在数据库中\n"
                "4. 尝试重新捕获图像");
        } else {
            QMessageBox::information(this, "识别成功", 
                QString("识别完成！找到 %1 个候选板卡。\n请在候选列表中选择最合适的板卡。")
                .arg(candidates.size()));
        }
        
        // 异步清理线程
        if (identification_worker_thread_) {
            identification_worker_thread_->quit();
            identification_worker_thread_->deleteLater();
            identification_worker_thread_ = nullptr;
        }
    });
    
    connect(worker, &IdentificationWorker::error, this, [this, progressDialog](const QString& errorMsg) {
        progressDialog->close();
        progressDialog->deleteLater();
        
        QMessageBox::critical(this, "识别错误", 
            QString("识别过程中发生错误：\n%1").arg(errorMsg));
        
        // 异步清理线程
        if (identification_worker_thread_) {
            identification_worker_thread_->quit();
            identification_worker_thread_->deleteLater();
            identification_worker_thread_ = nullptr;
        }
    });
    
    connect(progressDialog, &QProgressDialog::canceled, this, [this, progressDialog]() {
        if (identification_worker_thread_) {
            identification_worker_thread_->quit();
            identification_worker_thread_->deleteLater();
            identification_worker_thread_ = nullptr;
        }
        progressDialog->close();
        qDebug() << "用户取消了摄像头板卡识别";
    });
    
    // 设置进度更新定时器
    QTimer* progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, [progressDialog, progressTimer]() {
        int currentValue = progressDialog->value();
        if (currentValue < 90) {
            progressDialog->setValue(currentValue + 5);
        }
        if (progressDialog->wasCanceled()) {
            progressTimer->stop();
            progressTimer->deleteLater();
        }
    });
    
    connect(worker, &IdentificationWorker::finished, progressTimer, [progressTimer]() {
        progressTimer->stop();
        progressTimer->deleteLater();
    });
    
    connect(worker, &IdentificationWorker::error, progressTimer, [progressTimer]() {
        progressTimer->stop();
        progressTimer->deleteLater();
    });
    
    connect(worker, &IdentificationWorker::finished, worker, &IdentificationWorker::deleteLater);
    connect(worker, &IdentificationWorker::error, worker, &IdentificationWorker::deleteLater);
    
    // 确保线程在工作完成后自动清理
    connect(identification_worker_thread_, &QThread::finished, identification_worker_thread_, &QThread::deleteLater);
    
    progressTimer->start(200); // 每200ms更新一次进度
    
    // 启动线程
    identification_worker_thread_->start();
}

void PCBBoardManagementWidget::onConfirmIdentification()
{
    int currentRow = candidates_table_->currentRow();
    if (currentRow < 0 || currentRow >= identification_candidates_.size()) {
        QMessageBox::warning(this, "错误", "请先选择一个候选板卡！");
        return;
    }
    
    // 获取选中的板卡信息
    const PCBBoardInfo& selectedBoard = identification_candidates_[currentRow];
    
    int ret = QMessageBox::question(this, "确认选择", 
        QString("确认选择板卡：%1 (%2)？\n\n"
                "选择后将切换到板卡管理标签页显示该板卡的详细信息。")
        .arg(selectedBoard.boardName)
        .arg(selectedBoard.boardModel),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    
    if (ret == QMessageBox::Yes) {
        // 切换到板卡管理标签页
        main_tabs_->setCurrentWidget(board_management_tab_);
        
        // 在板卡列表中选中该板卡
        for (int i = 0; i < board_table_->rowCount(); ++i) {
            QTableWidgetItem* item = board_table_->item(i, 0);
            if (item && item->data(Qt::UserRole).toString() == selectedBoard.boardId) {
                board_table_->selectRow(i);
                board_table_->setCurrentItem(item);
                break;
            }
        }
        
        // 清空识别结果
        candidates_table_->setRowCount(0);
        identification_candidates_.clear();
        confirm_identification_button_->setEnabled(false);
        identification_image_label_->clear();
        identification_image_label_->setText("选择图片或使用摄像头进行识别");
        sizeValueLabel->setText("未加载");
        formatValueLabel->setText("未知");
        

        QMessageBox::information(this, "成功", 
            QString("已选择板卡：%1\n现在可以查看和编辑该板卡的详细信息。")
            .arg(selectedBoard.boardName));
    }
}

void PCBBoardManagementWidget::onAutoDetectComponents()
{
    if (current_board_image_.empty()) {
        QMessageBox::warning(this, "错误", "请先选择一个板卡！");
        return;
    }
    
    QMessageBox::information(this, "提示", "自动检测功能开发中...");
}

void PCBBoardManagementWidget::onDetectLabels()
{
    if (current_board_image_.empty()) {
        QMessageBox::warning(this, "错误", "请先选择一个板卡！");
        return;
    }
    
    QMessageBox::information(this, "提示", "标签检测功能开发中...");
}

void PCBBoardManagementWidget::onImageZoomChanged(int value)
{
    zoom_factor_ = value / 100.0;
    // LabelEditing有自己的缩放控制
}

void PCBBoardManagementWidget::onBoardAdded(const QString& boardId)
{
    Q_UNUSED(boardId)
    updateBoardList();
}

void PCBBoardManagementWidget::onBoardUpdated(const QString& boardId)
{
    Q_UNUSED(boardId)
    updateBoardList();
    if (boardId == current_board_id_) {
        displayBoardImage(boardId);
    }
}

void PCBBoardManagementWidget::onBoardDeleted(const QString& boardId)
{
    Q_UNUSED(boardId)
    updateBoardList();    
    if (boardId == current_board_id_) {
        current_board_id_.clear();
        updateImageDisplay();
    }
}

// 数据更新方法
void PCBBoardManagementWidget::updateBoardList()
{
    if (!board_manager_) return;
    
    board_table_->setRowCount(0);
    QList<PCBBoardInfo> boards = board_manager_->getAllBoards();
    
    board_table_->setRowCount(boards.size());
    for (int i = 0; i < boards.size(); ++i) {
        const PCBBoardInfo& board = boards[i];
        
        QTableWidgetItem* nameItem = new QTableWidgetItem(board.boardName);
        nameItem->setData(Qt::UserRole, board.boardId);
        
        QTableWidgetItem* modelItem = new QTableWidgetItem(board.boardModel);
        QTableWidgetItem* countItem = new QTableWidgetItem(QString::number(board.componentCount));
        
        board_table_->setItem(i, 0, nameItem);
        board_table_->setItem(i, 1, modelItem);
        board_table_->setItem(i, 2, countItem);
    }
    total_boards_label_->setText(QString::number(boards.size()));
}

void PCBBoardManagementWidget::updateBoardDetails()
{
    // TODO: 更新板卡详细信息
}

void PCBBoardManagementWidget::updateStatistics()
{
    if (!board_manager_) return;
    
    QList<PCBBoardInfo> boards = board_manager_->getAllBoards();
    int totalComponents = 0;
    
    for (const PCBBoardInfo& board : boards) {
        totalComponents += board.componentCount;
    }
    
    total_boards_label_->setText(QString::number(boards.size()));
}

void PCBBoardManagementWidget::displayIdentificationResults(const QList<PCBBoardInfo>& candidates)
{
    identification_candidates_ = candidates;
    candidates_table_->setRowCount(candidates.size());
    
    QStringList imagePaths;
    QStringList board_name;
    for (int i = 0; i < candidates.size(); ++i) {
        const PCBBoardInfo& candidate = candidates[i];
        imagePaths << candidate.imagePath;
        board_name << candidate.boardName;
        QTableWidgetItem* nameItem = new QTableWidgetItem(candidate.boardName);
        nameItem->setData(Qt::UserRole, candidate.boardId);
        
        QTableWidgetItem* modelItem = new QTableWidgetItem(candidate.boardModel);
        QTableWidgetItem* similarityItem = new QTableWidgetItem(QString::number(candidate.matchScore));
        QTableWidgetItem* descItem = new QTableWidgetItem(candidate.description);
        
        candidates_table_->setItem(i, 0, nameItem);
        candidates_table_->setItem(i, 1, modelItem);
        candidates_table_->setItem(i, 2, similarityItem);
        candidates_table_->setItem(i, 3, descItem);
    }
    
    confirm_identification_button_->setEnabled(!candidates.isEmpty());
    if(imagePaths.size() > 0){
        ImageFlowDialog *imageFlowDialog = new ImageFlowDialog(this);
        imageFlowDialog->loadImages(imagePaths, board_name);
        connect(imageFlowDialog, &ImageFlowDialog::imageSelected, this, [this](const QString& board_name) {
                int selectrow = 0;
                for(int i = 0; i < identification_candidates_.size(); i++){
                    if(identification_candidates_[i].boardName == board_name){
                        selectrow = i;
                        break;
                    }
                }
                const PCBBoardInfo& selectedBoard = identification_candidates_[selectrow];
                main_tabs_->setCurrentWidget(board_management_tab_);
                
                // 在板卡列表中选中该板卡
                for (int i = 0; i < board_table_->rowCount(); ++i) {
                    QTableWidgetItem* item = board_table_->item(i, 0);
                    qDebug() << "板卡：" << item->data(Qt::UserRole).toString() << "行" << i;
                    qDebug() << "选中板卡：" << selectedBoard.boardId << "行" << i;
                    if (item && item->data(Qt::UserRole).toString() == selectedBoard.boardId) {
                        board_table_->selectRow(i);
                        qDebug() << "选中板卡：" << selectedBoard.boardName << "行" << i;
                        board_table_->setCurrentItem(item);
                        break;
                    }
                }
                
                // 清空识别结果
                candidates_table_->setRowCount(0);
                identification_candidates_.clear();
                confirm_identification_button_->setEnabled(false);
                identification_image_label_->clear();
                identification_image_label_->setText("选择图片或使用摄像头进行识别");
                sizeValueLabel->setText("未加载");
                formatValueLabel->setText("未知");
                

        });
        imageFlowDialog->exec();
        imageFlowDialog->deleteLater();
    }
}

QPixmap PCBBoardManagementWidget::matToQPixmap(const cv::Mat& mat)
{
    // 使用标准化的颜色转换方法
    return matToQPixmapStandard(mat, false);
}

cv::Mat PCBBoardManagementWidget::qPixmapToMat(const QPixmap& pixmap)
{
    QImage qimg = pixmap.toImage();
    if (qimg.isNull()) return cv::Mat();
    
    QImage swapped = qimg.rgbSwapped();
    return cv::Mat(swapped.height(), swapped.width(), CV_8UC3, 
                   (void*)swapped.constBits(), swapped.bytesPerLine()).clone();
}

// 静态工具方法：标准化颜色转换
QPixmap PCBBoardManagementWidget::matToQPixmapStandard(const cv::Mat& mat, bool convertBGRtoRGB)
{
    if (mat.empty()) {
        return QPixmap();
    }
    
    try {
        switch (mat.type()) {
            case CV_8UC1: {
                // 灰度图像
                QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
                qDebug() << "Created grayscale QImage - Size:" << image.width() << "x" << image.height();
                return QPixmap::fromImage(image);
            }
            case CV_8UC3: {
                // 3通道图像处理
                if (convertBGRtoRGB) {
                    // BGR图像 - 转换为RGB
                    cv::Mat rgbMat;
                    cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
                    QImage image(rgbMat.data, rgbMat.cols, rgbMat.rows, rgbMat.step, QImage::Format_RGB888);
                    qDebug() << "Created RGB QImage (with BGR->RGB conversion) - Size:" << image.width() << "x" << image.height();
                    return QPixmap::fromImage(image);
                } else {
                    // 已经是RGB格式或不需要转换
                    QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
                    qDebug() << "Created RGB QImage (no conversion) - Size:" << image.width() << "x" << image.height();
                    return QPixmap::fromImage(image);
                }
            }
            case CV_8UC4: {
                // BGRA图像
                QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
                qDebug() << "Created ARGB QImage - Size:" << image.width() << "x" << image.height();
                return QPixmap::fromImage(image);
            }
            case CV_16UC1: {
                // 16位灰度图像，转换为8位
                cv::Mat mat8;
                mat.convertTo(mat8, CV_8UC1, 1.0/256.0);
                QImage image(mat8.data, mat8.cols, mat8.rows, mat8.step, QImage::Format_Grayscale8);
                return QPixmap::fromImage(image);
            }
            case CV_32FC1: {
                // 32位浮点灰度图像，转换为8位
                cv::Mat mat8;
                mat.convertTo(mat8, CV_8UC1, 255.0);
                QImage image(mat8.data, mat8.cols, mat8.rows, mat8.step, QImage::Format_Grayscale8);
                return QPixmap::fromImage(image);
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
                    return QPixmap::fromImage(image);
                }
                
                return  QPixmap();
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "matToQImage error:" << e.what();
        return QPixmap();
    }
    return QPixmap();
}

cv::Mat PCBBoardManagementWidget::qPixmapToMatStandard(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return cv::Mat();
    }
    
    try {
        QImage qimg = pixmap.toImage();
        if (qimg.isNull()) {
            return cv::Mat();
        }
        
        // 确保图像格式为RGB888
        if (qimg.format() != QImage::Format_RGB888) {
            qimg = qimg.convertToFormat(QImage::Format_RGB888);
        }
        
        // 创建OpenCV Mat，注意QImage是RGB格式，需要转换为BGR
        cv::Mat mat(qimg.height(), qimg.width(), CV_8UC3, (void*)qimg.constBits(), qimg.bytesPerLine());
        cv::Mat result;
        cv::cvtColor(mat, result, cv::COLOR_RGB2BGR);
        
        return result.clone(); // 返回深拷贝
        
    } catch (const cv::Exception& e) {
        qDebug() << "PCBBoardManagementWidget::qPixmapToMatStandard - OpenCV异常:" << e.what();
        return cv::Mat();
    } catch (const std::exception& e) {
        qDebug() << "PCBBoardManagementWidget::qPixmapToMatStandard - 标准异常:" << e.what();
        return cv::Mat();
    }
}

// 简化的事件处理和其他方法
void PCBBoardManagementWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
}

void PCBBoardManagementWidget::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
}

void PCBBoardManagementWidget::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);
}

void PCBBoardManagementWidget::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
}

void PCBBoardManagementWidget::wheelEvent(QWheelEvent* event)
{
    QWidget::wheelEvent(event);
}

void PCBBoardManagementWidget::onContextMenuRequested(const QPoint& pos)
{
    Q_UNUSED(pos)
}

void PCBBoardManagementWidget::onImageAreaContextMenu(const QPoint& pos)
{
    Q_UNUSED(pos)
}

void PCBBoardManagementWidget::onRefreshBoards()
{
    updateBoardList();
}

void PCBBoardManagementWidget::onSearchTextChanged()
{
    // TODO: 实现搜索功能
}

QPoint PCBBoardManagementWidget::imageToWidget(const QPoint& imagePos)
{
    return imagePos;
}

QPoint PCBBoardManagementWidget::widgetToImage(const QPoint& widgetPos)
{
    return widgetPos;
}

QRect PCBBoardManagementWidget::imageToWidget(const QRect& imageRect)
{
    return imageRect;
}

QRect PCBBoardManagementWidget::widgetToImage(const QRect& widgetRect)
{
    return widgetRect;
}

void PCBBoardManagementWidget::drawComponents(QPainter& painter)
{
    Q_UNUSED(painter)
}

void PCBBoardManagementWidget::drawComponent(QPainter& painter, const ComponentInfo& component, bool selected)
{
    Q_UNUSED(painter)
    Q_UNUSED(component)
    Q_UNUSED(selected)
}

ComponentInfo* PCBBoardManagementWidget::getComponentAtPosition(const QPoint& pos)
{
    Q_UNUSED(pos)
    return nullptr;
}

// 标签操作同步槽函数实现
void PCBBoardManagementWidget::onLabelAdded(const Label& label)
{
    qDebug() << "PCBBoardManagementWidget::onLabelAdded - 标签添加:";
    
    if (!board_manager_ || current_board_id_.isEmpty()) {
        qWarning() << "PCBBoardManagementWidget::onLabelAdded - 板卡管理器未初始化或无当前板卡";
        return;
    }
    
    // 将Label转换为ComponentInfo
    ComponentInfo component;
    component.labelInfo = label;
    component.componentName = label.label;
    component.componentType = "Unknown"; // 默认类型
    component.componentValue = QString::fromUtf8(label.notes);
    component.description = QString("通过标签编辑添加的元器件");
    component.isRequired = true;
    
    // 添加到数据库
    if (board_manager_->addComponent(current_board_id_, component)) {
        qDebug() << "PCBBoardManagementWidget::onLabelAdded - 标签同步到数据库成功";
        
        // 更新本地组件列表
        current_components_.append(component);
        
        // 更新统计信息
        // updateStatistics();
    } else {
        qWarning() << "PCBBoardManagementWidget::onLabelAdded - 标签同步到数据库失败";
    }
}

void PCBBoardManagementWidget::onLabelUpdated(const Label& label)
{
    qDebug() << "PCBBoardManagementWidget::onLabelUpdated - 标签更新:" << label.label;
    
    if (!board_manager_ || current_board_id_.isEmpty()) {
        qWarning() << "PCBBoardManagementWidget::onLabelUpdated - 板卡管理器未初始化或无当前板卡";
        return;
    }
    
    // 在当前组件列表中查找对应的组件
    for (int i = 0; i < current_components_.size(); ++i) {
        if (current_components_[i].labelInfo.id == label.id) {
            // 更新组件信息
            ComponentInfo& component = current_components_[i];
            component.labelInfo = label;
            component.componentName = label.label;
            component.componentValue = QString::fromUtf8(label.notes);
            
            // 更新数据库
            if (board_manager_->updateComponent(current_board_id_, component)) {
                qDebug() << "PCBBoardManagementWidget::onLabelUpdated - 标签更新同步到数据库成功";
            } else {
                qWarning() << "PCBBoardManagementWidget::onLabelUpdated - 标签更新同步到数据库失败";
            }
            break;
        }
    }
}

void PCBBoardManagementWidget::onLabelDeleted(int labelId)
{
    qDebug() << "PCBBoardManagementWidget::onLabelDeleted - 标签删除ID:" << labelId;
    
    if (!board_manager_ || current_board_id_.isEmpty()) {
        qWarning() << "PCBBoardManagementWidget::onLabelDeleted - 板卡管理器未初始化或无当前板卡";
        return;
    }
    
    // 在当前组件列表中查找对应的组件
    for (int i = 0; i < current_components_.size(); ++i) {
        if (current_components_[i].labelInfo.id == labelId) {
            // 从数据库删除
            if (board_manager_->removeComponent(current_board_id_, labelId)) {
                qDebug() << "PCBBoardManagementWidget::onLabelDeleted - 标签删除同步到数据库成功";
                
                // 从本地组件列表中移除
                current_components_.removeAt(i);
                
                // 更新统计信息
                // updateStatistics();
            } else {
        qWarning() << "PCBBoardManagementWidget::onLabelDeleted - 标签删除同步到数据库失败";
            }
            break;
        }
    }
}
