#ifndef WIRINGGUIDEDIALOG_H
#define WIRINGGUIDEDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QListWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QProgressBar>
#include <QCheckBox>
#include <QSplitter>
#include <QTabWidget>
#include <QPixmap>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QScrollArea>
#include <QFrame>
#include <QMessageBox>
#include <QTimer>
#include <QFormLayout>
#include "wiringresource.h"
#include "wiringschematic.h"
#include "../faultdiagnostic.h"

class WiringGuideDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WiringGuideDialog(const ComponentSpec& component, QWidget *parent = nullptr);
    ~WiringGuideDialog();

    // 获取用户配置的接线方案
    WiringConfiguration getWiringConfiguration() const;
    
    // 设置测试参数
    void setTestParameters(double voltage, double current, bool useDMM = true);

private slots:
    void onResourceSelectionChanged();
    void onPortSelectionChanged();
    void onWiringStepCompleted(int step);
    void onGenerateTestTask();
    void onResetWiring();
    void onShowSchematic();
    void onValidateConnections();

private:
    void setupUI();
    void setupResourceSelectionPage();
    void setupWiringGuidePage();
    void setupVerificationPage();
    void setupSchematicDisplay();
    void connectSignals();
    
    void updateAvailableResources();
    void updateWiringInstructions();
    void updateConnectionDiagram();
    void validateUserSelections();
    void generateWiringSteps();
    
    // UI组件
    QTabWidget* tab_widget_;
    
    // 资源选择页面
    QGroupBox* power_selection_group_;
    QGroupBox* dmm_selection_group_;
    QGroupBox* digital_io_group_;
    QGroupBox* analog_io_group_;
    
    QComboBox* voltage_source_combo_;
    QComboBox* current_source_combo_;
    QComboBox* dmm_channel_combo_;
    QListWidget* digital_output_list_;
    QListWidget* digital_input_list_;
    QListWidget* analog_output_list_;
    QListWidget* analog_input_list_;
    
    // 接线引导页面
    QListWidget* wiring_steps_list_;
    QTextEdit* current_step_detail_;
    QGraphicsView* wiring_diagram_view_;
    QGraphicsScene* wiring_diagram_scene_;
    QProgressBar* wiring_progress_;
    QPushButton* step_completed_btn_;
    QPushButton* previous_step_btn_;
    QPushButton* next_step_btn_;
    
    // 验证页面
    QTableWidget* connection_table_;
    QPushButton* validate_btn_;
    QPushButton* generate_task_btn_;
    QTextEdit* validation_result_;
    
    // 原理图显示
    WiringSchematic* schematic_widget_;
    
    // 数据成员
    ComponentSpec component_;
    WiringResourceManager* resource_manager_;
    WiringConfiguration current_config_;
    QList<WiringStep> wiring_steps_;
    int current_step_index_;
    double test_voltage_;
    double test_current_;
    bool use_dmm_;
    bool wiring_completed_;
    
    // 资源限制
    static const int MAX_DIGITAL_OUTPUTS = 12;
    static const int MAX_POWER_OUTPUTS = 4;
    static const int MAX_ANALOG_OUTPUTS = 16;
    static const int MAX_DIGITAL_INPUTS = 16;
    static const int MAX_ANALOG_INPUTS = 32;
    static const int DMM_CHANNELS = 1;
};

#endif // WIRINGGUIDEDIALOG_H
