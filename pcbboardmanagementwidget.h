#ifndef PCBBOARDMANAGEMENTWIDGET_H
#define PCBBOARDMANAGEMENTWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QProgressBar>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QThread>
#include <opencv2/opencv.hpp>
#include "pcbboardmanager.h"
#include "cameramanager.h"
#include "PCB_Components_Detect/labelediting.h"

class ComponentImageSelector;

class PCBBoardManagementWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PCBBoardManagementWidget(QWidget *parent = nullptr);
    ~PCBBoardManagementWidget();

    void setBoardManager(PCBBoardManager* manager);
    void setCameraManager(CameraManager* cameraManager);

public slots:
    // 板卡管理
    void onCreateBoard();
    void onImportBoard();
    void onEditBoard();
    void onDeleteBoard();
    void onDuplicateBoard();
    void onExportBoard();
    void onExportAllBoards();    // 板卡识别
    void onIdentifyFromFile();
    void onIdentifyFromCamera();
    void onConfirmIdentification();
    
    // 自动检测功能
    void onAutoDetectComponents();
    void onDetectLabels();
    
    // 界面更新
    void onBoardSelectionChanged(QTableWidgetItem* current, QTableWidgetItem* previous);
    void onImageZoomChanged(int value);
    void onShowComponentsToggled(bool enabled);
      // 数据更新
    void onBoardAdded(const QString& boardId);
    void onBoardUpdated(const QString& boardId);
    void onBoardDeleted(const QString& boardId);
    
    // 标签操作同步槽函数
    void onLabelAdded(const Label& label);
    void onLabelUpdated(const Label& label);
    void onLabelDeleted(int labelId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onContextMenuRequested(const QPoint& pos);
    void onImageAreaContextMenu(const QPoint& pos);
    void onRefreshBoards();
    void onSearchTextChanged();

private:    
    void setupUI();
    void setupBoardManagementTab();
    void setupIdentificationTab();
    void setupBoardListPanel();
    void setupImageDisplayPanel();
    void setupToolbar();
    void setupIdentificationPanel();
    void setupStatisticsPanel();
    void connectSignals();
      // 数据更新
    void updateBoardList();
    void updateImageDisplay();
    void updateBoardDetails();
    void updateStatistics();
    
    // 图像处理
    void displayBoardImage(const QString& boardId);
    void displayIdentificationResults(const QList<PCBBoardInfo>& candidates);
    void createOrUpdateLabelEditing();
    void clearLabelEditing();
    QPixmap matToQPixmap(const cv::Mat& mat);    
    cv::Mat qPixmapToMat(const QPixmap& pixmap);
    
    // 静态工具方法：标准化颜色转换
    static QPixmap matToQPixmapStandard(const cv::Mat& mat, bool convertBGRtoRGB = true);
    static cv::Mat qPixmapToMatStandard(const QPixmap& pixmap);
    
    // 坐标转换
    QPoint imageToWidget(const QPoint& imagePos);
    QPoint widgetToImage(const QPoint& widgetPos);
    QRect imageToWidget(const QRect& imageRect);
    QRect widgetToImage(const QRect& widgetRect);
    
    // 元器件可视化
    void drawComponents(QPainter& painter);
    void drawComponent(QPainter& painter, const ComponentInfo& component, bool selected = false);
    ComponentInfo* getComponentAtPosition(const QPoint& pos);

private:
    PCBBoardManager* board_manager_;
    CameraManager* camera_manager_;
    
    // 主界面布局
    QSplitter* main_splitter_;
    QTabWidget* main_tabs_;
    
    // 板卡管理标签页
    QWidget* board_management_tab_;
    QSplitter* board_splitter_;
    
    // 板卡列表面板
    QWidget* board_list_panel_;
    QLineEdit* search_edit_;
    QComboBox* model_filter_combo_;
    QTableWidget* board_table_;    QPushButton* create_board_button_;
    QPushButton* import_board_button_;
    QPushButton* edit_board_button_;
    QPushButton* delete_board_button_;
    QPushButton* duplicate_board_button_;
    QPushButton* export_board_button_;
    QPushButton* export_all_button_;
    
    // 图像显示面板
    QWidget* image_display_panel_;
    QScrollArea* image_scroll_area_;
    QLabel* image_label_;
    LabelEditing* label_editing_;
    QTableWidget* label_table_;
    QSlider* zoom_slider_;
    QCheckBox* show_components_checkbox_;
    QPushButton* auto_detect_button_;
    QPushButton* detect_labels_button_;
      // 删除了元器件列表面板相关成员变量
    
    // 板卡识别标签页
    QWidget* identification_tab_;
    QGroupBox* identification_group_;
    QPushButton* identify_file_button_;
    QPushButton* identify_camera_button_;
    QLabel* identification_image_label_;
    QTableWidget* candidates_table_;
    QPushButton* confirm_identification_button_;
    
    // 统计信息面板
    QGroupBox* statistics_group_;
    QLabel* total_boards_label_;
    QLabel* total_components_label_;
    QLabel* active_boards_label_;
      // 状态信息
    QString current_board_id_;
    QList<ComponentInfo> current_components_;
    cv::Mat current_board_image_;
    QList<PCBBoardInfo> identification_candidates_;
    QString selected_candidate_id_;
    
    // 异步识别
    QThread* identification_worker_thread_;
    
    // 图像显示状态
    double zoom_factor_;
    QPoint image_offset_;
    bool dragging_;
    QPoint last_drag_pos_;
    ComponentInfo* selected_component_;
    bool drawing_component_;
    QRect drawing_rect_;// 右键菜单
    QMenu* board_context_menu_;
    QMenu* image_context_menu_;
};

#endif // PCBBOARDMANAGEMENTWIDGET_H
