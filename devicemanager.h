#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "include/JY5320Core.h"
#include "include/JY5710.h"
#include "include/JY8902.h"
#include "devicethread.h"
#include <QString>
#include <QObject>
#include <QMap>
#include <QTimer>
#include <QMutex>
#include <vector>
#include <memory>

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
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager();

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
    
    // 便捷测量接口
    bool measureVoltage(const QString& deviceName, int channel, double& result, int timeout_ms = 5000);
    bool measureVoltage(int channel, double& result, int timeout_ms = 5000);
    bool measureCurrent(int channel, double& result, int timeout_ms = 5000);
    bool measureResistance(double& result, int timeout_ms = 5000);
    bool outputVoltage(int channel, double voltage);
      // 异步测量接口
    bool measureVoltageAsync(const QString& deviceName, int channel, double& result, int timeout_ms = 5000);
    bool measureCurrentAsync(double& result, int timeout_ms = 5000);
    bool measureResistanceAsync(double& result, int timeout_ms = 5000);
    bool outputVoltageAsync(int channel, double voltage, int timeout_ms = 5000);
    
    // DMM配置接口
    bool configureDMMForVoltage(int timeout_ms = 5000);
    bool configureDMMForCurrent(int timeout_ms = 5000);
    bool configureDMMForResistance(int timeout_ms = 5000);
    bool configureDMMForDiodeTest(int timeout_ms = 5000);
    
    // 设备可用性检查
    bool checkDeviceAvailability();
    QStringList getUnavailableDevices();

    QString getLastError() const { return last_error_; }

public slots:
    void onDeviceStatusChanged(const QString& deviceName, bool ready);
    void onOperationCompleted(const QString& deviceName, const DeviceResult& result);
    void onDeviceError(const QString& deviceName, const QString& error);

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
    
    // 错误处理
    QString last_error_;
    mutable QMutex errorMutex_;
    
    // 状态监控
    QTimer* statusTimer_;
    
    void setError(const QString& error);
    void initializeStatusMonitoring();
    DeviceStatus convertToStatus(bool ready) const;

private slots:
    void checkDeviceStatus();
};

#endif // DEVICEMANAGER_H
