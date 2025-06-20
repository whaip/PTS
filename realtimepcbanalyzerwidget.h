#ifndef REALTIMEPCBANALYZERWIDGET_H
#define REALTIMEPCBANALYZERWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QTimer>
#include <QPixmap>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QListWidget>
#include <QSplitter>
#include <QCloseEvent>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <opencv2/opencv.hpp>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "pcbidentifier.h"
#include "cameramanager.h"
#include "PCB_Components_Detect/yolomodel.h"
#include "PCB_Components_Detect/ClassList.h"
#include "pcbdetectionmanager.h"
#include "pcbdetectionhistorywidget.h"

// PCB综合分析结果结构体
struct PCBAnalysisResult {
    // PCB板卡识别结果
    RealtimeIdentificationResult pcbIdentResult;
    
    // 元件检测结果
    std::vector<Label> componentLabels;
    cv::Mat annotatedImage;
    
    // 统计信息
    int totalComponents;
    std::map<std::string, int> componentCounts;
    double analysisTime;
    QDateTime timestamp;
    bool isValid;
    
    PCBAnalysisResult() : totalComponents(0), analysisTime(0.0), isValid(false) {}
};

class RealtimePCBAnalyzerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RealtimePCBAnalyzerWidget(QWidget *parent = nullptr);
    ~RealtimePCBAnalyzerWidget();

    // 设置组件
    void setPCBIdentifier(PCBIdentifier* identifier);
    void setCameraManager(CameraManager* cameraManager);

    // 分析模式枚举
    enum AnalysisMode {
        PCB_IDENTIFICATION_ONLY,    // 仅PCB识别
        COMPONENT_DETECTION_ONLY,   // 仅元件检测
        COMPREHENSIVE_ANALYSIS      // 综合分析
    };

public slots:
    // 控制功能
    void startRealtimeAnalysis();
    void stopRealtimeAnalysis();
    void onAnalysisIntervalChanged(int intervalMs);
    void onPCBThresholdChanged(double threshold);
    void onComponentConfidenceChanged(double confidence);
    void onAnalysisModeChanged(int mode);
    void onShowAnnotationsToggled(bool enabled);
    void onAutoSaveToggled(bool enabled);
      // 结果处理
    void onRealtimeAnalysisCompleted(const PCBAnalysisResult& result);
    void onAnalysisStarted();
    void onAnalysisStopped();
    void onErrorOccurred(const QString& error);
    
    // 图像显示
    void updateCameraImage();
    void saveCurrentResult();
    void clearResults();
    
    // 数据管理
    void showDetectionHistory();
    void onAutoSaveResult(const PCBAnalysisResult& result);
    void exportResults();
    void deleteOldRecords();
    
    // 组件筛选
    void onComponentTypeToggled(int componentType, bool enabled);
    void selectAllComponents();
    void deselectAllComponents();
protected:
    void closeEvent(QCloseEvent* event) override;
private slots:
    void onDisplayModeChanged();
    void onZoomChanged(int value);
    void onTabChanged(int index);

private:    // UI组件
    void setupUI();
    void setupControlPanel();
    void setupDisplayPanel();
    void setupPCBResultTab();
    void setupComponentResultTab();
    void setupResultsPanel();
    void setupComponentFilterPanel();
    void connectSignals();
    
    // 分析处理
    void performComprehensiveAnalysis();
    PCBAnalysisResult analyzeImage(const cv::Mat& image);
    void processComponentDetection(const cv::Mat& image, PCBAnalysisResult& result);
    void processPCBIdentification(const cv::Mat& image, PCBAnalysisResult& result);
    
    // 图像处理
    void updateImageDisplay();
    void updateResultDisplay();
    QPixmap matToQPixmap(const cv::Mat& mat);
    cv::Mat createCombinedAnnotation(const cv::Mat& originalImage, 
                                   const RealtimeIdentificationResult& pcbResult,
                                   const std::vector<Label>& componentLabels);
    
    // 结果显示
    void updateComponentStatistics(const PCBAnalysisResult& result);
    void updatePCBIdentificationDisplay(const RealtimeIdentificationResult& result);
    void updateComponentDetectionDisplay(const std::vector<Label>& labels);
    
    // 日志和状态
    void addLogMessage(const QString& message);
    void updateStatus(const QString& status);
    void updateAnalysisStatistics();

private:    // 核心组件
    PCBIdentifier* pcb_identifier_;
    CameraManager* camera_manager_;
    std::shared_ptr<YOLOModel> yolo_model_;
    PCBDetectionManager* detection_manager_;
    PCBDetectionHistoryWidget* history_widget_;
    
    // 分析状态
    std::atomic<bool> is_running_;
    std::atomic<bool> stop_analysis_;
    AnalysisMode analysis_mode_;
    QTimer* display_timer_;
    QTimer* analysis_timer_;
      // 线程安全
    mutable std::mutex analysis_mutex_;
    std::condition_variable analysis_cv_;
    std::atomic<bool> is_processing_;
    QList<QFutureWatcher<PCBAnalysisResult>*> active_watchers_;
    
    // 图像数据
    cv::Mat current_display_image_;
    PCBAnalysisResult current_result_;
    
    // 控制面板组件
    QGroupBox* control_group_;
    QPushButton* start_button_;
    QPushButton* stop_button_;
    QSpinBox* interval_spinbox_;
    QDoubleSpinBox* pcb_threshold_spinbox_;
    QDoubleSpinBox* component_confidence_spinbox_;
    QComboBox* analysis_mode_combo_;
    QCheckBox* show_annotations_checkbox_;
    QCheckBox* auto_save_checkbox_;
    QComboBox* display_mode_combo_;
    QSpinBox* zoom_spinbox_;
    
    // 显示面板组件
    QTabWidget* display_tabs_;
    QGroupBox* display_group_;
    QLabel* image_label_;
    QScrollArea* image_scroll_area_;
    
    // PCB识别显示
    QWidget* pcb_tab_;
    QLabel* pcb_model_label_;
    QProgressBar* pcb_confidence_bar_;
    QLabel* pcb_match_count_label_;
    QLabel* pcb_processing_time_label_;
    
    // 元件检测显示
    QWidget* component_tab_;
    QTableWidget* component_table_;
    QLabel* total_components_label_;
    
    // 组件过滤面板
    QGroupBox* filter_group_;
    std::vector<QCheckBox*> component_checkboxes_;
    QPushButton* select_all_button_;
    QPushButton* deselect_all_button_;
      // 结果面板组件
    QGroupBox* results_group_;
    QTextEdit* log_text_;
    QPushButton* save_result_button_;
    QPushButton* clear_log_button_;
    QPushButton* history_button_;
    QPushButton* export_button_;
    QPushButton* cleanup_button_;
    QLabel* status_label_;
    
    // 统计信息
    QLabel* total_analysis_label_;
    QLabel* success_rate_label_;
    QLabel* average_processing_time_label_;
      // 配置参数
    static const int DEFAULT_INTERVAL_MS = 1000;
    static inline const double DEFAULT_PCB_THRESHOLD = 0.3;
    static inline const double DEFAULT_COMPONENT_CONFIDENCE = 0.6;
    static const int DEFAULT_ZOOM = 100;
    static const int UPDATE_DISPLAY_INTERVAL = 100;
    
    // 统计数据
    int total_analyses_;
    int successful_analyses_;
    double total_processing_time_;
    QDateTime session_start_time_;
    
    // 元件类型和颜色
    std::vector<std::string> component_names_;
    std::vector<cv::Scalar> component_colors_;
    std::vector<int> enabled_components_;
    
    bool show_annotations_;
    bool auto_save_enabled_;
    int zoom_level_;
    QString display_mode_;
};

#endif // REALTIMEPCBANALYZERWIDGET_H
