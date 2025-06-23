#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSplitter>
#include <QTabWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include "devicemanager.h"
#include "faultdiagnostic.h"
#include "testsequencemanager.h"
#include "resultexporter.h"
#include "pcbidentifier.h"
#include "pcbidentificationdialog.h"
#include "cameracontrolwidget.h"
#include "realtimepcbanalyzerwidget.h"
#include "pcbdetectionhistorywidget.h"
#include "pcbdetectionmanager.h"
#include "pcbboardmanagementwidget.h"
#include "pcbboardmanager.h"
#include "devicemanagertestwindow.h"
// 新增：接线引导相关包含
#include "WiringGuide/wiringguidedialog.h"
#include "WiringGuide/portmanager.h"
#include "WiringGuide/wiringresourcemanager.h"
#include "WiringGuide/wiringtaskgenerator.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void initializeSystem();
    void shutdownSystem();
    void startSingleTest();
    void startBatchTest();
    void onDeviceStatusChanged(const QString& device, DeviceStatus status);    
    void onDiagnosticCompleted(const DiagnosticResult& result);
    void onErrorOccurred(const QString& error);
    void addComponent();
    void removeComponent();
    void loadTestSequence();
    void saveTestSequence();
    void exportResults();
    void clearResults();
    void updateSystemStatus();
    void runNextTest();
    void finishBatchTest();    
    void openPCBIdentification();  // 新增：打开PCB识别界面
    void openCameraControl();      // 新增：打开相机控制界面
    void openPCBAnalyzer();         // 新增：打开PCB综合分析器
    void openDetectionHistory();    // 新增：打开检测历史管理    
    void openBoardManagement();
    void openDeviceManagerTest();   // 新增：打开设备管理器测试窗口
    void startWiringGuide();        // 开始接线引导
    void onWiringCompleted(const WiringScheme& scheme);  // 接线完成回调
    void showWiringGuideForComponent(const ComponentSpec& component);  // 为特定元件显示接线引导
    void showPortConfigurationForComponent(const ComponentSpec& component);  // 为特定元件显示端口配置

private:
    Ui::MainWindow *ui;    // 核心组件
    DeviceManager* device_manager_;
    FaultDiagnostic* fault_diagnostic_;
    TestSequenceManager* sequence_manager_;
    ResultExporter* result_exporter_;
    PCBIdentifier* pcb_identifier_;           // 新增：PCB识别器
    PCBIdentificationDialog* pcb_dialog_;     // 新增：PCB识别对话框
    CameraControlWidget* camera_control_;     // 新增：相机控制组件
    RealtimePCBAnalyzerWidget* pcb_analyzer_; // 新增：PCB综合分析器
    PCBDetectionHistoryWidget* history_widget_; // 新增：检测历史管理窗口
    PCBDetectionManager* detection_manager_;    // 新增：检测数据管理器    
    PCBBoardManagementWidget* board_management_widget_; // 新增：PCB板卡管理窗口
    PCBBoardManager* board_manager_;            // 新增：PCB板卡管理器
    DeviceManagerTestWindow* device_test_window_;  // 新增：设备管理器测试窗口
    QTimer* status_timer_;    // 新增：接线引导相关成员
    PortManager* port_manager_;
    WiringResourceManager* wiring_resource_manager_;
    WiringGuideDialog* current_wiring_dialog_;
    WiringTaskGenerator* task_generator_;
    QString current_task_id_;  // 保存当前执行的任务ID
    
    // UI组件
    QTabWidget* main_tabs_;
      // 设备状态页面
    QWidget* device_status_page_;
    QLabel* ao_status_label_;
    QLabel* daq_5322_status_label_;
    QLabel* daq_5323_status_label_;
    QLabel* dmm_status_label_;
    QPushButton* init_button_;
    QPushButton* shutdown_button_;
    
    // 单个测试页面
    QWidget* single_test_page_;
    QComboBox* component_type_combo_;
    QLineEdit* component_ref_edit_;
    QDoubleSpinBox* nominal_value_spin_;
    QDoubleSpinBox* tolerance_spin_;
    QSpinBox* channel_spin_;
    QPushButton* single_test_button_;
    QTextEdit* single_result_text_;
    
    // 批量测试页面
    QWidget* batch_test_page_;
    QTableWidget* component_table_;
    QPushButton* add_component_button_;
    QPushButton* remove_component_button_;
    QPushButton* load_sequence_button_;
    QPushButton* save_sequence_button_;
    QPushButton* batch_test_button_;
    QProgressBar* test_progress_;
    
    // 结果页面
    QWidget* results_page_;
    QTableWidget* results_table_;
    QPushButton* export_button_;
    QPushButton* clear_button_;
    QTextEdit* detail_text_;
    
    // 状态栏
    QLabel* system_status_label_;
    QLabel* test_count_label_;
    QProgressBar* status_progress_;
    // 数据
    QVector<ComponentSpec> test_components_;
    QList<DiagnosticResult> test_results_;
    TestSequence current_sequence_;
    int current_test_index_;
    bool batch_testing_active_;
    
    void setupUI();
    void setupDeviceStatusPage();
    void setupSingleTestPage();
    void setupBatchTestPage();
    void setupResultsPage();
    void setupStatusBar();
    void setupMenuBar();
    void updateDeviceStatus();
    void updateResultsTable();
    void updateDetailText(int currentRow, int currentColumn, int previousRow, int previousColumn);
    ComponentSpec createComponentFromUI();
    TestStep createTestStepFromComponent(const ComponentSpec& component);
    void populateComponentTable();
    QString formatResult(const DiagnosticResult& result);
    QString getStatusIcon(DeviceStatus status);
};

#endif // MAINWINDOW_H
