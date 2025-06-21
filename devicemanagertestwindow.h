#ifndef DEVICEMANAGERTESTWINDOW_H
#define DEVICEMANAGERTESTWINDOW_H

#include <QDialog>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QProgressBar>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QElapsedTimer>
#include <QDateTime>
#include "devicemanager.h"

// 任务执行结果结构
struct TaskResult {
    bool success = false;
    QString errorMessage;
    QVector<QVector<double>> channelData;
    QVector<int> channels;
    int samplesPerChannel = 0;
    double sampleRate = 0.0;
    qint64 executionTime = 0;  // 执行时间(ms)
    QString deviceName;
    
    // 数据统计信息
    struct ChannelStats {
        double minValue = 0.0;
        double maxValue = 0.0;
        double avgValue = 0.0;
        QStringList firstSamples;
    };
    QVector<ChannelStats> channelStats;
};

// 任务执行线程
class TaskExecutionThread : public QThread
{
    Q_OBJECT
    
public:
    explicit TaskExecutionThread(DeviceManager* deviceManager, QObject* parent = nullptr);
    ~TaskExecutionThread();
    
    // 启动多点采集任务
    void startMultiPointAcquisitionTask(const QString& deviceName, 
                                       const QVector<int>& channels,
                                       double sampleRate, 
                                       int samplesPerChannel,
                                       double rangeMin = -10.0, 
                                       double rangeMax = 10.0);
    
    // 停止任务执行
    void stopTask();
    
    // 检查是否正在执行任务
    bool isTaskRunning() const;

signals:
    void taskStarted(const QString& taskName);
    void taskProgress(const QString& message, int percentage = -1);
    void taskCompleted(const TaskResult& result);
    void taskFailed(const QString& error);

protected:
    void run() override;

private:
    DeviceManager* deviceManager_;
    mutable QMutex taskMutex_;
    bool taskRunning_;
    bool stopRequested_;
    
    // 任务参数
    QString deviceName_;
    QVector<int> channels_;
    double sampleRate_;
    int samplesPerChannel_;
    double rangeMin_;
    double rangeMax_;
    
    // 执行多点采集任务的具体实现
    TaskResult executeMultiPointAcquisition();
    
    // 计算数据统计信息
    void calculateChannelStats(TaskResult& result);
};

class DeviceManagerTestWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DeviceManagerTestWindow(DeviceManager* deviceManager, QWidget *parent = nullptr);
    ~DeviceManagerTestWindow();

private slots:
    void testInitializeDevices();
    void testMeasureVoltage();
    void testMeasureCurrent();
    void testMeasureResistance();
    void testOutputVoltage();
    void testDeviceStatus();
    void clearResults();
    void onDeviceStatusChanged(const QString& device, DeviceStatus status);
    void onErrorOccurred(const QString& error);
    
    // 任务线程相关槽函数
    void onTaskStarted(const QString& taskName);
    void onTaskProgress(const QString& message, int percentage);
    void onTaskCompleted(const TaskResult& result);
    void onTaskFailed(const QString& error);

private:
    void setupUI();
    void updateDeviceStatusDisplay();
    void appendResult(const QString& result);
    void setTestingState(bool testing);  // 设置测试状态
    
    DeviceManager* deviceManager_;
    TaskExecutionThread* taskThread_;  // 任务执行线程
    
    // UI组件
    QVBoxLayout* mainLayout_;
    QHBoxLayout* buttonLayout_;
    QGroupBox* testButtonsGroup_;
    QGroupBox* parametersGroup_;
    QGroupBox* statusGroup_;
    QGroupBox* resultsGroup_;
    
    // 测试按钮
    QPushButton* initDevicesBtn_;
    QPushButton* measureVoltageBtn_;
    QPushButton* measureCurrentBtn_;
    QPushButton* measureResistanceBtn_;
    QPushButton* outputVoltageBtn_;
    QPushButton* deviceStatusBtn_;
    QPushButton* clearBtn_;
    QPushButton* stopTaskBtn_;  // 停止任务按钮
    
    // 参数控件
    QSpinBox* channelSpin_;
    QDoubleSpinBox* voltageSpin_;
    QComboBox* deviceCombo_;
    QSpinBox* timeoutSpin_;
    
    // 状态显示
    QLabel* aoStatusLabel_;
    QLabel* daq5322StatusLabel_;
    QLabel* daq5323StatusLabel_;
    QLabel* dmmStatusLabel_;
    QProgressBar* testProgress_;
    
    // 结果显示
    QTextEdit* resultText_;
};

#endif // DEVICEMANAGERTESTWINDOW_H
