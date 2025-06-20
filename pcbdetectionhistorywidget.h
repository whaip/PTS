#ifndef PCBDETECTIONHISTORYWIDGET_H
#define PCBDETECTIONHISTORYWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QSplitter>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QScrollArea>
#include <QProgressBar>
#include <QTimer>
#include <QInputDialog>
#include "pcbdetectionmanager.h"

class PCBDetectionHistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PCBDetectionHistoryWidget(QWidget *parent = nullptr);
    ~PCBDetectionHistoryWidget();

    void setDetectionManager(PCBDetectionManager* manager);
    void refreshRecordsList();

public slots:
    void onNewRecordAdded(const QString& recordId);
    void onRecordUpdated(const QString& recordId);
    void onRecordDeleted(const QString& recordId);

private slots:
    // 界面操作
    void onSearchTextChanged();
    void onDateRangeChanged();
    void onPCBModelFilterChanged();
    void onRefreshClicked();
    void onClearFiltersClicked();
    
    // 记录操作
    void onRecordSelectionChanged();
    void onDeleteRecordClicked();
    void onExportSelectedClicked();
    void onViewImageClicked();
    void onEditNotesClicked();
    void onSaveNotesClicked();
    
    // 右键菜单
    void onShowContextMenu(const QPoint& pos);
    void onCopyRecordId();
    void onOpenImageLocation();
    void onExportSingleRecord();
    
    // 批量操作
    void onSelectAllClicked();
    void onDeleteSelectedClicked();
    void onExportAllClicked();
    void onCleanupOldRecordsClicked();
    
    // 统计刷新
    void onUpdateStatistics();

private:
    void setupUI();
    void setupTableWidget();
    void setupFilterPanel();
    void setupDetailPanel();
    void setupStatisticsPanel();
    void setupToolbar();
    void connectSignals();
    
    void populateTable();
    void populateTableWithRecords(const QList<PCBDetectionRecord>& records);
    void applyFilters();
    void updateRecordDetails(const PCBDetectionRecord& record);
    void clearRecordDetails();
    
    void showImagePreview(const QString& imagePath);
    void updateStatistics();
    
    QList<PCBDetectionRecord> getFilteredRecords() const;
    QStringList getSelectedRecordIds() const;
    
    void exportRecords(const QList<QString>& recordIds);
    bool deleteRecords(const QList<QString>& recordIds);

private:
    PCBDetectionManager* detection_manager_;
    
    // 主要布局
    QSplitter* main_splitter_;
    QWidget* left_panel_;
    QWidget* right_panel_;
    
    // 过滤面板
    QGroupBox* filter_group_;
    QLineEdit* search_edit_;
    QComboBox* pcb_model_combo_;
    QDateTimeEdit* start_date_edit_;
    QDateTimeEdit* end_date_edit_;
    QPushButton* refresh_button_;
    QPushButton* clear_filters_button_;
    
    // 记录表格
    QTableWidget* records_table_;
    QLabel* records_count_label_;
    
    // 工具栏
    QPushButton* select_all_button_;
    QPushButton* delete_selected_button_;
    QPushButton* export_selected_button_;
    QPushButton* export_all_button_;
    QPushButton* cleanup_button_;
    
    // 详情面板
    QGroupBox* detail_group_;
    QLabel* detail_id_label_;
    QLabel* detail_timestamp_label_;
    QLabel* detail_pcb_model_label_;
    QLabel* detail_pcb_confidence_label_;
    QLabel* detail_total_components_label_;
    QLabel* detail_analysis_time_label_;
    QTextEdit* detail_notes_edit_;
    QPushButton* save_notes_button_;
    QPushButton* view_original_button_;
    QPushButton* view_annotated_button_;
    QPushButton* delete_record_button_;
    
    // 图像预览
    QScrollArea* image_preview_area_;
    QLabel* image_preview_label_;
    
    // 统计面板
    QGroupBox* statistics_group_;
    QLabel* total_records_label_;
    QLabel* date_range_label_;
    QLabel* most_common_pcb_label_;
    QLabel* most_common_component_label_;
    QProgressBar* storage_usage_bar_;
    
    // 右键菜单
    QMenu* context_menu_;
    QAction* copy_id_action_;
    QAction* open_location_action_;
    QAction* export_single_action_;
    QAction* delete_action_;
    
    // 状态
    QString current_selected_record_id_;
    QTimer* statistics_timer_;
};

#endif // PCBDETECTIONHISTORYWIDGET_H
