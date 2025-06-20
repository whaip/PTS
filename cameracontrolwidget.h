#ifndef CAMERACONTROLWIDGET_H
#define CAMERACONTROLWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QTableWidget>
#include <QProgressBar>
#include <QTimer>
#include <QCheckBox>
#include <QSlider>
#include <QComboBox>
#include <QTabWidget>
#include <QSplitter>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QFileDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include "cameramanager.h"
#include "pcbidentifier.h"
#include "Camera/Infrared/irimagedisplay.h"

class CameraControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraControlWidget(QWidget *parent = nullptr);
    ~CameraControlWidget();
    
    // 获取相机管理器
    CameraManager* getCameraManager() const;

private slots:
    // 摄像头控制
    void onStartHDCamera();
    void onStopHDCamera();
    void onStartIRCamera();
    void onStopIRCamera();
    void onStartAllCameras();
    void onStopAllCameras();
    
    // 诊断功能
    void onDiagnoseHDCamera();
    void onDiagnoseIRCamera();    // 图像处理
    void onHDImageReceived(CameraType type, const ImageData& data);
    void onThermalDataReceived(const ThermalData& data);
    void onDetectPCBComponents();
    void onPCBDetectionCompleted(const PCBDetectionResult& result);
    void onPCBModelIdentified(const QString& model, double confidence);
    
    // 实时PCB识别
    void onStartRealtimePCBIdentification();
    void onStopRealtimePCBIdentification();
    void onRealtimeIdentificationCompleted(const RealtimeIdentificationResult& result);
    void onRealtimeIdentificationStarted();
    void onRealtimeIdentificationStopped();
    void onRealtimeIdentificationError(const QString& error);
    void onRealtimeIntervalChanged(int intervalMs);
    void onRealtimeThresholdChanged(double threshold);// 温度测量
    void onTemperatureAlert(double temperature, const cv::Point& location);
    void onMeasureTemperature();
    void onSetTemperatureThreshold();
    void onMarkMaxTempToggled(bool enabled);  // 新增：最高温度标记开关
    
    // 摄像头状态
    void onCameraStatusChanged(CameraType type, CameraStatus status);    void onCameraError(CameraType type, const QString& error);
      // 参数设置
    void onHDResolutionChanged();
    void onHDCameraParamsChanged();
    void onIRCameraParamsChanged();
    void onApplyHDParams();
    void onApplyIRParams();
    
    // 图像保存
    void onSaveHDImage();
    void onSaveThermalData();
    void onSaveThermalImage();
    void onSaveDetectionResult();
    void onSaveDetectionResults();
    
    // 界面更新
    void updateStatus();
    void updateTemperatureDisplay();
      // 鼠标事件处理
    void onHDImageClicked(QPoint position);
    void onThermalImageClicked(QPoint position);

protected:
    // 重写事件处理函数
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUI();
    void setupGlobalControls();
    void setupHDCameraPage();
    void setupIRCameraPage();    
    void setupDetectionPage();
    void setupStatusPage();
    void connectSignals();
    void setupTimer();
    
    // Helper methods    
    void addLogMessage(const QString& message);
    cv::Mat qImageToCvMat(const QImage& qImage);
    QImage cvMatToQImage(const cv::Mat& mat);
    QImage matToQImage(const cv::Mat& mat, bool convertBGRtoRGB = false);
    void updateDetectionTable(const PCBDetectionResult& result);
    bool saveDetectionResultsToFile(const QString& fileName, const PCBDetectionResult& result);
    void setupHDCameraControls();
    void setupIRCameraControls();
    void setupImageDisplays();
    void setupDetectionControls();
    void setupTemperatureControls();
    void setupStatusDisplay();
      void updateHDImageDisplay(const cv::Mat& image);
    void updateThermalImageDisplay(const cv::Mat& image);
    void updateDetectionResultDisplay(const PCBDetectionResult& result);
    
    // UI组件
    QTabWidget* tabWidget_;
      // 高清摄像头页面
    QWidget* hdCameraPage_;    QGroupBox* hdControlGroup_;
    QPushButton* startHDButton_;
    QPushButton* stopHDButton_;
    QPushButton* saveHDImageButton_;
    QPushButton* detectComponentsButton_;
    
    // 实时PCB识别控件
    QGroupBox* realtimePCBGroup_;
    QPushButton* startRealtimePCBButton_;
    QPushButton* stopRealtimePCBButton_;
    QSpinBox* realtimeIntervalSpin_;
    QDoubleSpinBox* realtimeThresholdSpin_;
    QLabel* realtimeResultLabel_;
    QLabel* realtimeConfidenceLabel_;
      QGroupBox* hdParamsGroup_;
    QComboBox* hdResolutionCombo_;
    QSpinBox* hdWidthSpin_;
    QSpinBox* hdHeightSpin_;
    QSpinBox* hdFpsSpin_;
    QPushButton* applyHDParamsButton_;
    
    QGraphicsView* hdImageView_;
    QGraphicsScene* hdImageScene_;
    QGraphicsPixmapItem* hdImageItem_;    // 红外摄像头页面
    QWidget* irCameraPage_;
    QGroupBox* irControlGroup_;
    QPushButton* startIRButton_;
    QPushButton* stopIRButton_;
    QPushButton* saveThermalButton_;
    QPushButton* measureTempButton_;
    QCheckBox* markMaxTempCheckBox_;  // 标记最高温度点复选框
    
    QGroupBox* irParamsGroup_;
    QLineEdit* irIpEdit_;
    QPushButton* applyIRParamsButton_;
    
    IRImageDisplay* irImageDisplay_;  // 替换为专门的红外图像显示控件
    // QGraphicsView* irImageView_;  // 注释掉原来的
    // QGraphicsScene* irImageScene_;
    // QGraphicsPixmapItem* irImageItem_;
    
    // 温度显示控件
    QGroupBox* temperatureGroup_;
    QLabel* currentTempLabel_;
    QLabel* maxTempLabel_;
    QLabel* minTempLabel_;
    QLabel* avgTempLabel_;
    QDoubleSpinBox* thresholdSpin_;
    QPushButton* setThresholdButton_;
    
    // 检测结果页面
    QWidget* detectionPage_;
    QTableWidget* detectionTable_;
    QTextEdit* detectionDetails_;
    QGraphicsView* detectionImageView_;
    QGraphicsScene* detectionImageScene_;
    QGraphicsPixmapItem* detectionImageItem_;
    QPushButton* saveDetectionButton_;
    
    // 状态显示
    QWidget* statusPage_;
    QLabel* hdStatusLabel_;
    QLabel* irStatusLabel_;
    QTextEdit* logTextEdit_;
    QProgressBar* processingProgress_;
    
    // 全局控制
    QGroupBox* globalControlGroup_;
    QPushButton* startAllButton_;
    QPushButton* stopAllButton_;
      // 核心组件
    CameraManager* cameraManager_;
    PCBIdentifier* pcbIdentifier_;
    QTimer* statusUpdateTimer_;
      // 状态变量
    QPoint lastClickPosition_;
    bool isDetectionRunning_;
    bool isRealtimeIdentificationRunning_;
    PCBDetectionResult lastDetectionResult_;
    RealtimeIdentificationResult lastRealtimeResult_;
    
    // 常量
    static const int STATUS_UPDATE_INTERVAL = 1000; // 1秒
    static const int IMAGE_DISPLAY_WIDTH = 640;
    static const int IMAGE_DISPLAY_HEIGHT = 480;
};

#endif // CAMERACONTROLWIDGET_H
