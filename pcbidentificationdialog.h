#ifndef PCBIDENTIFICATIONDIALOG_H
#define PCBIDENTIFICATIONDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QScrollArea>
#include <QPixmap>
#include <QTimer>
#include "pcbidentifier.h"

class PCBIdentificationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PCBIdentificationDialog(QWidget *parent = nullptr);
    ~PCBIdentificationDialog();

    // 设置PCB识别器
    void setPCBIdentifier(PCBIdentifier* identifier);

private slots:
    // 识别操作
    void selectImageForIdentification();
    void startIdentification();
    void startBatchIdentification();
    void onIdentificationCompleted(const PCBIdentificationResult& result);
    void onIdentificationProgress(int percentage);
    
    // 模板管理
    void addNewModel();
    void removeSelectedModel();
    void updateSelectedModel();
    void loadTemplateDatabase();
    void saveTemplateDatabase();
    void rebuildDatabase();
    
    // 界面更新
    void updateModelList();
    void updateConfigurationDisplay();
    void onModelSelectionChanged();
    void onErrorOccurred(const QString& error);
    
    // 配置参数
    void onThresholdChanged(double value);
    void onMaxResultsChanged(int value);
    void onGPUToggled(bool enabled);

private:
    // UI组件
    QTabWidget* main_tabs_;
    
    // 识别页面
    QWidget* identification_page_;
    QLineEdit* image_path_edit_;
    QPushButton* browse_image_button_;
    QPushButton* identify_button_;
    QPushButton* batch_identify_button_;
    QProgressBar* progress_bar_;
    QTextEdit* result_display_;
    QLabel* preview_label_;
    QScrollArea* preview_scroll_;
    
    // 结果表格
    QTableWidget* results_table_;
    
    // 模板管理页面
    QWidget* template_page_;
    QListWidget* model_list_;
    QPushButton* add_model_button_;
    QPushButton* remove_model_button_;
    QPushButton* update_model_button_;
    QPushButton* load_db_button_;
    QPushButton* save_db_button_;
    QPushButton* rebuild_db_button_;
    
    // 模型详情
    QGroupBox* model_details_group_;
    QLineEdit* model_name_edit_;
    QLineEdit* model_code_edit_;
    QTextEdit* model_description_edit_;
    QListWidget* template_paths_list_;
    QPushButton* add_template_button_;
    QPushButton* remove_template_button_;
    
    // 配置页面
    QWidget* config_page_;
    QDoubleSpinBox* threshold_spin_;
    QSpinBox* max_results_spin_;
    QCheckBox* gpu_checkbox_;
    QLabel* gpu_status_label_;
    QTextEdit* log_display_;
    
    // 状态栏
    QLabel* status_label_;
    
    // 核心组件
    PCBIdentifier* pcb_identifier_;
    
    // 私有方法
    void setupUI();
    void setupIdentificationPage();
    void setupTemplatePage();
    void setupConfigPage();
    void connectSignals();
    void updateResultsTable(const PCBIdentificationResult& result);
    void updatePreviewImage(const QString& imagePath);
    void clearResults();
    void showErrorMessage(const QString& title, const QString& message);
    void showInfoMessage(const QString& title, const QString& message);
    QStringList selectImageFiles(const QString& title = "选择图片文件");
    QStringList selectTemplateFiles();
    QString selectDirectory(const QString& title = "选择目录");
    void updateStatusMessage(const QString& message);
    void addLogMessage(const QString& message);
    QString formatConfidence(double confidence) const;
    QString formatMatchScore(double score) const;
};

#endif // PCBIDENTIFICATIONDIALOG_H
