#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "include/JY5320Core.h"
#include "include/JY5710.h"
#include "include/JY8902.h"
#include "devicethread.h"
#include "cameramanager.h"
#include <QString>
#include <QObject>
#include <QMap>
#include <QTimer>
#include <QMutex>
#include <vector>
#include <memory>
#include <QEventLoop>

// 设备状态枚举
enum class DeviceStatus {
    DISCONNECTED,
    INITIALIZING,
    CONNECTED,
    BUSY,
    ERROR
};

// 多线程设备管理器类
class DeviceManager : public QObject
{    Q_OBJECT

public:
    explicit DeviceManager(CameraManager* cameraManager = nullptr, QObject *parent = nullptr);
    ~DeviceManager();

    struct WaitResult {
        bool success = false;
        bool timeout = false;
        int attempts = 0;
        QString errorMessage;
    };

    // 设备线程管理
    bool initializeDeviceThreads();
    void shutdownDeviceThreads();
    bool isSystemReady() const;
    
    // 设备状态查询
    DeviceStatus getDeviceStatus(const QString& deviceName) const;
    QStringList getConnectedDevices() const;
    
    // 同步操作管理
    void createSyncGroup(const QString& groupName, const QStringList& deviceNames);
    void removeSyncGroup(const QString& groupName);
    bool executeSync(const QString& groupName, const QList<DeviceOperation>& operations, int timeout = 10000);
      // 异步操作接口
    bool submitOperation(const QString& deviceName, const DeviceOperation& operation);
    DeviceResult waitForResult(const QString& deviceName, int timeout = 5000);
    
    // 数据采集接口 - 单点采集
    bool singlePointAcquisition(const QString& deviceName, int channel, double& result, 
                               double rangeMin = -10.0, double rangeMax = 10.0, int timeout_ms = 5000);
    bool singlePointAcquisition(const QString& deviceName, const QVector<int>& channels, 
                               QVector<double>& results, double rangeMin = -10.0, double rangeMax = 10.0, int timeout_ms = 5000);
    
    // 数据采集接口 - 多点采集  
    bool configureMultiPointAcquisition(const QString& deviceName, const QVector<int>& channels,
                                       double sampleRate, int samplesPerChannel,
                                       double rangeMin = -10.0, double rangeMax = 10.0);
    bool startMultiPointAcquisition(const QString& deviceName);
    bool readMultiPointData(const QString& deviceName, QVector<QVector<double>>& channelData, int timeout_ms = 5000);
    bool stopMultiPointAcquisition(const QString& deviceName);
    
    // 数据采集接口 - 连续采集
    bool configureContinuousAcquisition(const QString& deviceName, const QVector<int>& channels,
                                       double sampleRate, int bufferSize = 10000,
                                       double rangeMin = -10.0, double rangeMax = 10.0);
    bool startContinuousAcquisition(const QString& deviceName);
    bool readContinuousData(const QString& deviceName, QVector<QVector<double>>& channelData, 
                           int keepSamples = 0, int timeout_ms = 1000);
    bool stopContinuousAcquisition(const QString& deviceName);
    
    // 数据采集状态查询
    bool isAcquisitionActive(const QString& deviceName);
    QVariantMap getAcquisitionStatus(const QString& deviceName);
    QVector<QString> getDAQDevices() const;
    
    // 采集数据导出
    bool exportAcquisitionData(const QVector<QVector<double>>& channelData, 
                              const QVector<int>& channels, const QString& fileName,
                              const QString& format = "csv");
    
    // 实用工具方法
    QString acquisitionModeToString(const QString& mode) const;
    QStringList getSupportedAcquisitionModes() const;

    // 同步测试方法（简化版本用于测试）
    bool testSynchronizedOutputAndAcquisition(double sineFreq = 1000.0, double sineAmplitude = 4.0, 
                                            double sampleRate = 10000.0, int samplesPerChannel = 1000,
                                            int timeout_ms = 15000);

    // 通用的事件循环等待方法
    WaitResult waitForDataWithEventLoop(const QString& deviceName, 
                                       QVector<QVector<double>>& channelData,
                                       int maxAttempts = 50,
                                       int checkInterval = 100,
                                       int timeout = 5000);

    QString getLastError() const { return last_error_; }
    ThermalData getLatestThermalData() const;
    CameraManager *getCameraManager() const { return cameraManager_; }

    // 通用测量接口
    QMap<QString, QVariant> measureVoltage(int channel, double range = 10.0);
    QMap<QString, QVariant> measureCurrent(int channel, double range = 1.0);
    QMap<QString, QVariant> measureResistance(double nominalValue);
    QMap<QString, QVariant> measureLCR(const QMap<QString, QVariant>& params);
    QMap<QString, QVariant> measureSMU(const QMap<QString, QVariant>& params);
    QMap<QString, QVariant> measurePulse(const QMap<QString, QVariant>& params);
    void setTemperatureThreshold(double threshold);

public slots:
    void onDeviceStatusChanged(const QString& deviceName, bool ready);
    void onOperationCompleted(const QString& deviceName, const DeviceResult& result);
    void onDeviceError(const QString& deviceName, const QString& error);
    void onTemperatureAlert(double temperature, const cv::Point& location);

signals:
    void deviceStatusChanged(const QString& device, DeviceStatus status);
    void errorOccurred(const QString& error);
    void operationCompleted(const QString& deviceName, const DeviceResult& result);

private:
    // 设备线程映射
    QMap<QString, BaseDeviceThread*> deviceThreads_;
    QMap<QString, DeviceStatus> deviceStatus_;
    
    // 同步控制
    DeviceSyncController* syncController_;
    CameraManager* cameraManager_;
    ThermalData latestThermalData_;
    mutable QMutex thermalDataMutex_;
    bool TemperatureAlerted_ = false;

    // 错误处理
    QString last_error_;
    mutable QMutex errorMutex_;
    
    // 状态监控
    QTimer* statusTimer_;
    
    void setError(const QString& error);
    void initializeStatusMonitoring();
    DeviceStatus convertToStatus(bool ready) const;
    
    // 设备可用性检查
    bool checkDeviceAvailability();
    QStringList getUnavailableDevices();
    
private slots:
    void checkDeviceStatus();
};

#endif // DEVICEMANAGER_H
