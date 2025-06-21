#ifndef DEVICETHREAD_H
#define DEVICETHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QTimer>
#include <QAtomicInteger>
#include <QAtomicPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>
#include <QObject>
#include <QMap>
#include <QHash>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <memory>
#include <vector>
#include "include/JY5320Core.h"
#include "include/JY5710.h"
#include "include/JY8902.h"
#include "5711waveformconfig.h"

// 设备操作命令枚举
enum class DeviceCommand {
    INITIALIZE,
    SHUTDOWN,
    CONFIGURE_CHANNEL,
    START_MEASUREMENT,
    STOP_MEASUREMENT,
    READ_DATA,
    WRITE_DATA,
    SYNC_TRIGGER,
    CALIBRATE
};

// 设备操作参数结构
struct DeviceOperation {
    DeviceCommand command;
    int channel = -1;
    double value = 0.0;
    double sampleRate = 0.0;
    int timeout = 5000;
    bool blocking = true;
    QString deviceName;
    QVariantMap parameters;
    
    // 同步相关
    QString syncGroup;
    int syncDelay = 0;  // 微秒级延迟
    
    // 数据采集相关参数
    int samplesPerChannel = 1000;       // 每通道采样点数
    QVector<int> channels;              // 多通道采集的通道列表
    QString acquisitionMode = "single"; // "single", "multi", "continuous"
    double inputRangeMin = -10.0;       // 输入范围最小值
    double inputRangeMax = 10.0;        // 输入范围最大值
    bool useCallback = false;           // 是否使用回调方式获取数据
    
    DeviceOperation(DeviceCommand cmd = DeviceCommand::INITIALIZE) : command(cmd) {}
};

// 设备操作结果
struct DeviceResult {
    bool success = false;
    double value = 0.0;
    QString error;
    QVariantMap data;
    QDateTime timestamp;
    DeviceCommand command = DeviceCommand::INITIALIZE;  // 添加command成员
    
    DeviceResult(bool ok = false) : success(ok), timestamp(QDateTime::currentDateTime()) {}
};

// 设备同步控制器
class DeviceSyncController : public QObject
{
    Q_OBJECT
    
public:
    static DeviceSyncController* instance();
    ~DeviceSyncController();
    
    // 同步组管理
    void createSyncGroup(const QString& groupName, const QStringList& deviceNames);
    void removeSyncGroup(const QString& groupName);
    bool waitForSync(const QString& groupName, const QString& deviceName, int timeout = 5000);
    void signalReady(const QString& groupName, const QString& deviceName);
    void triggerSync(const QString& groupName);
    
    // 全局触发控制
    void setGlobalTrigger(bool enabled);
    void sendGlobalTrigger();
    
signals:
    void syncTriggered(const QString& groupName);
    void globalTriggerReceived();
    
private:
    explicit DeviceSyncController(QObject* parent = nullptr);
    static DeviceSyncController* instance_;    struct SyncGroup {
        QStringList deviceNames;
        QStringList readyDevices;
        bool triggered = false;
    };
      QHash<QString, SyncGroup> syncGroups_;
    QHash<QString, QWaitCondition*> syncConditions_;
    QHash<QString, QMutex*> syncMutexes_;
    QMutex groupsMutex_;
    bool globalTriggerEnabled_ = false;
};

// 基础设备线程类
class BaseDeviceThread : public QThread
{
    Q_OBJECT
    
public:
    explicit BaseDeviceThread(const QString& deviceName, QObject* parent = nullptr);
    virtual ~BaseDeviceThread();
      // 设备控制
    void submitOperation(const DeviceOperation& operation);
    DeviceResult waitForResult(int timeout = 5000);
    bool isDeviceReady() const { return deviceReady_.loadAcquire() != 0; }
    QString getDeviceName() const { return deviceName_; }
    
    // 同步控制
    void joinSyncGroup(const QString& groupName);
    void leaveSyncGroup();
      // 线程控制
    void stopThread();
    
signals:
    void deviceStatusChanged(const QString& deviceName, bool ready);
    void operationCompleted(const QString& deviceName, const DeviceResult& result);
    void errorOccurred(const QString& deviceName, const QString& error);
    
protected:
    void run() override;
    virtual bool initializeDevice() = 0;
    virtual void shutdownDevice() = 0;
    virtual DeviceResult executeOperation(const DeviceOperation& operation) = 0;
    virtual void handleSyncTrigger() = 0;
    
    QString deviceName_;
    QAtomicInteger<int> deviceReady_;
    QAtomicInteger<int> stopRequested_;
    
    mutable QMutex operationMutex_;
    QWaitCondition operationCondition_;
    QQueue<DeviceOperation> operationQueue_;
    QQueue<DeviceResult> resultQueue_;
    
    QString currentSyncGroup_;
    DeviceSyncController* syncController_;
    
private slots:
    void onSyncTriggered(const QString& groupName);
    void onGlobalTrigger();
};

// AO设备线程 (JY5711)
class AODeviceThread : public BaseDeviceThread
{
    Q_OBJECT
    
public:
    explicit AODeviceThread(QObject* parent = nullptr);
    ~AODeviceThread();
    
protected:
    bool initializeDevice() override;
    void shutdownDevice() override;
    DeviceResult executeOperation(const DeviceOperation& operation) override;
    void handleSyncTrigger() override;
    
private:
    JY5710_DeviceHandle deviceHandle_;
    QMap<int, double> channelStates_;  // 记录各通道状态
    QSet<int> enabledChannels_;        // 记录已启用的通道

    bool initializeChannel();
    
    // 新增的波形配置和输出方法
    DeviceResult configureChannelWithRestart(const DeviceOperation& operation);
    DeviceResult outputWaveform(const DeviceOperation& operation);
};

// DAQ设备线程 (JY5320)
class DAQDeviceThread : public BaseDeviceThread
{
    Q_OBJECT
    
public:
    explicit DAQDeviceThread(const QString& deviceName, int slot, QObject* parent = nullptr);
    ~DAQDeviceThread();
    
    // 数据采集模式枚举
    enum class AcquisitionMode {
        SINGLE_POINT,    // 单点采集
        MULTI_POINT,     // 多点采集 (有限采样)
        CONTINUOUS       // 连续采集
    };
    
    // 采集参数结构体
    struct AcquisitionParams {
        AcquisitionMode mode = AcquisitionMode::SINGLE_POINT;
        QVector<int> channels;              // 通道列表
        double sampleRate = 1000.0;         // 采样率
        int samplesPerChannel = 1000;       // 每通道采样点数（用于多点和连续模式）
        double inputRangeMin = -10.0;       // 输入范围最小值
        double inputRangeMax = 10.0;        // 输入范围最大值
        double rangeMin = -10.0;            // 输入范围最小值（兼容性）
        double rangeMax = 10.0;             // 输入范围最大值（兼容性）
        JY5320_AI_BandWidth bandwidth = JY5320_AI_BandWidth_25K;  // 带宽设置
        bool useBuffer = true;              // 是否使用缓冲区（连续模式）
        int bufferSize = 10000;             // 缓冲区大小
        int timeout = 5000;                 // 读取超时时间(ms)
    };
    
protected:
    bool initializeDevice() override;
    void shutdownDevice() override;
    DeviceResult executeOperation(const DeviceOperation& operation) override;
    void handleSyncTrigger() override;
    
private:
    JY5320_DeviceHandle deviceHandle_;
    int slotNumber_;
    QMap<int, bool> channelEnabled_;
    double currentSampleRate_;
    bool acquisitionActive_;
    
    // 数据采集相关
    AcquisitionMode currentMode_;
    int samplesPerChannel_;
    QVector<double> dataBuffer_;
    QTimer* dataFetchTimer_;
    int enabledChannelCount_;
    QVector<int> enabledChannels_;
    AcquisitionParams currentParams_;
    
    // 连续采集缓冲区管理
    QVector<QVector<double>> continuousDataBuffer_;  // 每个通道一个缓冲区
    QMutex bufferMutex_;
    int bufferWriteIndex_;
    bool bufferOverrun_;
    
    // 渐进式数据累积 (用于多点采集)
    QVector<QVector<double>> accumulatedData_;  // 累积的数据缓冲区
    int accumulatedSamples_;                    // 当前累积的样本数
    const int BATCH_READ_SIZE = 20;             // 每次批量读取的样本数
    const int BATCH_FETCH_INTERVAL = 50;        // 批次读取时间间隔(ms)
    
    // 数据缓存 (用于定时器读取的数据)
    QVector<QVector<double>> lastMultiPointData_;  // 最后读取的多点采集数据
    QVector<double> tempMultiPointData_;  // 临时缓存的多点采集数据
    bool dataReadyFlag_ = false;                   // 数据准备标志
    QMutex dataMutex_;                             // 数据访问互斥锁
    
    // 回调和数据处理
    void processMultiPointData();
    void processContinuousData();
    bool checkBufferStatus(unsigned long long& availableSamples, bool& overRun);
    
    // 触发相关
    void setupTrigger();
    void startAcquisition();
    void stopAcquisition();
    
    // 数据采集方法
    DeviceResult performSinglePointAcquisition(const DeviceOperation& operation);
    DeviceResult configureMultiPointAcquisition(const DeviceOperation& operation);
    DeviceResult startMultiPointAcquisition(const DeviceOperation& operation);
    DeviceResult stopMultiPointAcquisition(const DeviceOperation& operation = DeviceOperation());
    DeviceResult readMultiPointData(const DeviceOperation& operation);
    
    DeviceResult configureContinuousAcquisition(const DeviceOperation& operation);
    DeviceResult startContinuousAcquisition(const DeviceOperation& operation);
    DeviceResult stopContinuousAcquisition();
    DeviceResult readContinuousData(const DeviceOperation& operation);
    
    // 通道配置
    DeviceResult configureChannels(const QVector<int>& channels, double rangeMin = -10.0, double rangeMax = 10.0);
    DeviceResult setSampleRate(double sampleRate);
    DeviceResult setAcquisitionMode(AcquisitionMode mode);
    
    // 辅助方法
    QString acquisitionModeToString(AcquisitionMode mode) const;
    JY5320_AI_SampleMode convertToJY5320Mode(AcquisitionMode mode) const;
    
private slots:
    void onDataFetchTimer();
    
signals:
    void multiPointDataReady(const QString& deviceName, const QVector<QVector<double>>& channelData);
    void continuousDataReady(const QString& deviceName, const QVector<QVector<double>>& channelData);
    void dataBufferUpdated(const QString& deviceName, const QVariantMap& bufferInfo);
    void acquisitionCompleted(const QString& deviceName, AcquisitionMode mode);
};

// DMM设备线程 (JY8902)
class DMMDeviceThread : public BaseDeviceThread
{
    Q_OBJECT
    
public:
    explicit DMMDeviceThread(QObject* parent = nullptr);
    ~DMMDeviceThread();
    
protected:
    bool initializeDevice() override;
    void shutdownDevice() override;
    DeviceResult executeOperation(const DeviceOperation& operation) override;
    void handleSyncTrigger() override;
    
private:
    JY8902_DeviceHandle deviceHandle_;
    JY8902_DMM_MeasurementFunction currentFunction_;
    bool measurementActive_;    
    void configureMeasurement(JY8902_DMM_MeasurementFunction function);
    bool configureBasicSettings();
    DeviceResult performMeasurement();
};

#endif // DEVICETHREAD_H
