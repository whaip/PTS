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
    
    DeviceOperation(DeviceCommand cmd = DeviceCommand::INITIALIZE) : command(cmd) {}
};

// 设备操作结果
struct DeviceResult {
    bool success = false;
    double value = 0.0;
    QString error;
    QVariantMap data;
    QDateTime timestamp;
    
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
    
    // 触发相关
    void setupTrigger();
    void startAcquisition();
    void stopAcquisition();
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
