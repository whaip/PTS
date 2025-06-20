#include "devicemanager.h"
#include <QDebug>
#include <QThread>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QCoreApplication>

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent), syncController_(DeviceSyncController::instance()),
      statusTimer_(new QTimer(this))
{
    // 初始化状态监控
    initializeStatusMonitoring();
}

DeviceManager::~DeviceManager()
{
    shutdownDeviceThreads();
}

bool DeviceManager::initializeDeviceThreads()
{
    qDebug() << "Initializing device threads...";
    
    // First check device availability
    qDebug() << "Checking device availability before initialization...";
    if (!checkDeviceAvailability()) {
        QStringList unavailable = getUnavailableDevices();
        QString errorMsg = QString("Cannot initialize - some devices are not available: %1")
                           .arg(unavailable.join(", "));
        setError(errorMsg);
        qDebug() << errorMsg;
        qDebug() << "Please check: 1) Hardware connections 2) Device drivers 3) Device power 4) No other software using devices";
        return false;
    }
    
    try {
        // 创建并启动AO设备线程 (JY5711)
        AODeviceThread* aoThread = new AODeviceThread(this);
        deviceThreads_["JY5711"] = aoThread;
        deviceStatus_["JY5711"] = DeviceStatus::DISCONNECTED;
        
        // 创建并启动DAQ设备线程 (JY5322 - slot 5)
        DAQDeviceThread* daqThread5322 = new DAQDeviceThread("JY5322", 5, this);
        deviceThreads_["JY5322"] = daqThread5322;
        deviceStatus_["JY5322"] = DeviceStatus::DISCONNECTED;
        
        // 创建并启动DAQ设备线程 (JY5323 - slot 3)
        DAQDeviceThread* daqThread5323 = new DAQDeviceThread("JY5323", 3, this);
        deviceThreads_["JY5323"] = daqThread5323;
        deviceStatus_["JY5323"] = DeviceStatus::DISCONNECTED;
        
        // 创建并启动DMM设备线程 (JY8902)
        DMMDeviceThread* dmmThread = new DMMDeviceThread(this);
        deviceThreads_["JY8902"] = dmmThread;
        deviceStatus_["JY8902"] = DeviceStatus::DISCONNECTED;
        
        // 连接信号槽
        for (auto it = deviceThreads_.begin(); it != deviceThreads_.end(); ++it) {
            BaseDeviceThread* thread = it.value();
            connect(thread, &BaseDeviceThread::deviceStatusChanged,
                    this, &DeviceManager::onDeviceStatusChanged);
            connect(thread, &BaseDeviceThread::operationCompleted,
                    this, &DeviceManager::onOperationCompleted);
            connect(thread, &BaseDeviceThread::errorOccurred,
                    this, &DeviceManager::onDeviceError);
        }
        
        // 启动所有设备线程
        for (auto it = deviceThreads_.begin(); it != deviceThreads_.end(); ++it) {
            BaseDeviceThread* thread = it.value();
            thread->start();
            qDebug() << "Started device thread for:" << it.key();
        }
          // 等待设备初始化完成
        bool allInitialized = false;
        QElapsedTimer timer;
        timer.start();
        const int timeout = 25000; // 增加到25秒超时 - 为DMM设备提供更多时间
        
        while (!allInitialized && timer.elapsed() < timeout) {
            QCoreApplication::processEvents();
            QThread::msleep(100);
            
            allInitialized = true;
            
            // 检查每个设备状态并输出详细信息
            for (auto it = deviceStatus_.begin(); it != deviceStatus_.end(); ++it) {
                if (it.value() == DeviceStatus::DISCONNECTED) {
                    allInitialized = false;
                    
                    // 每2秒输出一次等待信息
                    if (timer.elapsed() % 2000 < 100) {
                        qDebug() << "Still waiting for device:" << it.key() 
                                 << "- elapsed time:" << timer.elapsed() << "ms";
                    }
                    break;
                }
            }
        }
        
        if (!allInitialized) {
            // 输出具体哪些设备未连接
            QStringList disconnectedDevices;
            for (auto it = deviceStatus_.begin(); it != deviceStatus_.end(); ++it) {
                if (it.value() == DeviceStatus::DISCONNECTED) {
                    disconnectedDevices << it.key();
                }
            }
            
            QString errorMsg = QString("Device initialization timeout. Disconnected devices: %1")
                               .arg(disconnectedDevices.join(", "));
            setError(errorMsg);
            qDebug() << errorMsg;
            return false;
        }
        
        // 发送初始化命令给每个设备线程
        for (auto it = deviceThreads_.begin(); it != deviceThreads_.end(); ++it) {
            DeviceOperation initOp;
            initOp.command = DeviceCommand::INITIALIZE;
            it.value()->submitOperation(initOp);
        }
        
        qDebug() << "All device threads initialized successfully";
        return true;
        
    } catch (const std::exception& e) {
        setError(QString("Exception during device initialization: %1").arg(e.what()));
        return false;
    } catch (...) {
        setError("Unknown exception during device initialization");
        return false;
    }
}

void DeviceManager::shutdownDeviceThreads()
{
    qDebug() << "Shutting down device threads...";
    
    // 发送关闭命令给所有设备
    for (auto it = deviceThreads_.begin(); it != deviceThreads_.end(); ++it) {
        DeviceOperation shutdownOp;
        shutdownOp.command = DeviceCommand::SHUTDOWN;
        it.value()->submitOperation(shutdownOp);
    }
    
    // 等待所有线程关闭
    for (auto it = deviceThreads_.begin(); it != deviceThreads_.end(); ++it) {
        BaseDeviceThread* thread = it.value();
        thread->stopThread();
        delete thread;
    }
    
    deviceThreads_.clear();
    deviceStatus_.clear();
    
    qDebug() << "All device threads shut down";
}

bool DeviceManager::isSystemReady() const
{
    QMutexLocker locker(&errorMutex_);
    
    for (auto it = deviceStatus_.begin(); it != deviceStatus_.end(); ++it) {
        if (it.value() != DeviceStatus::CONNECTED) {
            return false;
        }
    }
    return !deviceStatus_.isEmpty();
}

DeviceStatus DeviceManager::getDeviceStatus(const QString& deviceName) const
{
    QMutexLocker locker(&errorMutex_);
    return deviceStatus_.value(deviceName, DeviceStatus::DISCONNECTED);
}

QStringList DeviceManager::getConnectedDevices() const
{
    QMutexLocker locker(&errorMutex_);
    QStringList connected;
    
    for (auto it = deviceStatus_.begin(); it != deviceStatus_.end(); ++it) {
        if (it.value() == DeviceStatus::CONNECTED) {
            connected.append(it.key());
        }
    }
    
    return connected;
}

void DeviceManager::createSyncGroup(const QString& groupName, const QStringList& deviceNames)
{
    syncController_->createSyncGroup(groupName, deviceNames);
    
    // 将设备加入同步组
    for (const QString& deviceName : deviceNames) {
        if (deviceThreads_.contains(deviceName)) {
            deviceThreads_[deviceName]->joinSyncGroup(groupName);
        }
    }
}

void DeviceManager::removeSyncGroup(const QString& groupName)
{
    // 移除设备的同步组关联
    for (auto it = deviceThreads_.begin(); it != deviceThreads_.end(); ++it) {
        it.value()->leaveSyncGroup();
    }
    
    syncController_->removeSyncGroup(groupName);
}

bool DeviceManager::executeSync(const QString& groupName, const QList<DeviceOperation>& operations, int timeout)
{
    if (operations.isEmpty()) {
        return false;
    }
    
    // 为每个操作设置同步组
    for (int i = 0; i < operations.size(); ++i) {
        const DeviceOperation& op = operations[i];
        if (deviceThreads_.contains(op.deviceName)) {
            DeviceOperation syncOp = op;
            syncOp.syncGroup = groupName;
            syncOp.timeout = timeout;
            deviceThreads_[op.deviceName]->submitOperation(syncOp);
        }
    }
    
    return true;
}

bool DeviceManager::submitOperation(const QString& deviceName, const DeviceOperation& operation)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    deviceThreads_[deviceName]->submitOperation(operation);
    return true;
}

DeviceResult DeviceManager::waitForResult(const QString& deviceName, int timeout)
{
    if (!deviceThreads_.contains(deviceName)) {
        DeviceResult result;
        result.success = false;
        result.error = QString("Device %1 not found").arg(deviceName);
        return result;
    }
    
    return deviceThreads_[deviceName]->waitForResult(timeout);
}

bool DeviceManager::measureVoltageAsync(const QString& deviceName, int channel, double& result, int timeout_ms)
{
    DeviceOperation operation;
    operation.command = DeviceCommand::READ_DATA;
    operation.channel = channel;
    operation.timeout = timeout_ms;
    
    if (!submitOperation(deviceName, operation)) {
        return false;
    }
    
    DeviceResult deviceResult = waitForResult(deviceName, timeout_ms);
    if (deviceResult.success) {
        result = deviceResult.value;
        return true;
    } else {
        setError(deviceResult.error);
        return false;
    }
}

bool DeviceManager::measureCurrentAsync(double& result, int timeout_ms)
{
    DeviceOperation operation;
    operation.command = DeviceCommand::READ_DATA;
    operation.timeout = timeout_ms;
    
    if (!submitOperation("JY8902", operation)) {
        return false;
    }
    
    DeviceResult deviceResult = waitForResult("JY8902", timeout_ms);
    if (deviceResult.success) {
        result = deviceResult.value;
        return true;
    } else {
        setError(deviceResult.error);
        return false;
    }
}

bool DeviceManager::measureResistanceAsync(double& result, int timeout_ms)
{    // 首先配置DMM为电阻测量模式
    DeviceOperation configOp;
    configOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    configOp.parameters["function"] = static_cast<int>(JY8902_2_Wire_Resistance);
    
    if (!submitOperation("JY8902", configOp)) {
        return false;
    }
    
    // 等待配置完成
    DeviceResult configResult = waitForResult("JY8902", timeout_ms);
    if (!configResult.success) {
        setError(configResult.error);
        return false;
    }
    
    // 执行测量
    DeviceOperation measureOp;
    measureOp.command = DeviceCommand::READ_DATA;
    measureOp.timeout = timeout_ms;
    
    if (!submitOperation("JY8902", measureOp)) {
        return false;
    }
    
    DeviceResult measureResult = waitForResult("JY8902", timeout_ms);
    if (measureResult.success) {
        result = measureResult.value;
        return true;
    } else {
        setError(measureResult.error);
        return false;
    }
}

bool DeviceManager::outputVoltageAsync(int channel, double voltage, int timeout_ms)
{
    // 验证输入参数
    if (channel < 0 || channel > 31) {
        setError(QString("Invalid channel number: %1. Valid range: 0-31").arg(channel));
        return false;
    }
    
    if (voltage < -10.0 || voltage > 10.0) {
        setError(QString("Invalid voltage: %1V. Valid range: ±10V").arg(voltage));
        return false;
    }
    
    DeviceOperation operation;
    operation.command = DeviceCommand::WRITE_DATA;
    operation.channel = channel;
    operation.value = voltage;
    operation.timeout = timeout_ms;

    if (!submitOperation("JY5711", operation)) {
        setError("Failed to submit voltage output operation to JY5711 device");
        return false;
    }
    
    DeviceResult deviceResult = waitForResult("JY5711", timeout_ms);
    if (deviceResult.success) {
        return true;
    } else {
        setError(QString("JY5711 voltage output failed: %1").arg(deviceResult.error));
        return false;
    }
}

// 同步接口（兼容现有代码）
bool DeviceManager::measureVoltage(const QString& deviceName, int channel, double& result, int timeout_ms)
{
    return measureVoltageAsync(deviceName, channel, result, timeout_ms);
}

bool DeviceManager::measureVoltage(int channel, double& result, int timeout_ms)
{
    return measureVoltageAsync("JY5322", channel, result, timeout_ms);
}

bool DeviceManager::measureCurrent(int channel, double& result, int timeout_ms)
{
    Q_UNUSED(channel)  // Channel parameter not used in current implementation
    return measureCurrentAsync(result, timeout_ms);
}

bool DeviceManager::measureResistance(double& result, int timeout_ms)
{
    return measureResistanceAsync(result, timeout_ms);
}

bool DeviceManager::outputVoltage(int channel, double voltage)
{
    return outputVoltageAsync(channel, voltage);
}

// DMM配置接口实现
bool DeviceManager::configureDMMForVoltage(int timeout_ms)
{
    qDebug() << "Configuring DMM for voltage measurement";
    
    DeviceOperation operation;
    operation.command = DeviceCommand::CONFIGURE_CHANNEL;
    operation.parameters["function"] = static_cast<int>(JY8902_DC_Volts);
    operation.timeout = timeout_ms;
    
    if (!submitOperation("JY8902", operation)) {
        setError("Failed to submit DMM voltage configuration");
        return false;
    }
    
    DeviceResult result = waitForResult("JY8902", timeout_ms);
    if (result.success) {
        qDebug() << "DMM voltage configuration successful";
        return true;
    } else {
        setError(QString("DMM voltage configuration failed: %1").arg(result.error));
        return false;
    }
}

bool DeviceManager::configureDMMForCurrent(int timeout_ms)
{
    qDebug() << "Configuring DMM for current measurement";
    
    DeviceOperation operation;
    operation.command = DeviceCommand::CONFIGURE_CHANNEL;
    operation.parameters["function"] = static_cast<int>(JY8902_DC_Current);
    operation.timeout = timeout_ms;
    
    if (!submitOperation("JY8902", operation)) {
        setError("Failed to submit DMM current configuration");
        return false;
    }
    
    DeviceResult result = waitForResult("JY8902", timeout_ms);
    if (result.success) {
        qDebug() << "DMM current configuration successful";
        return true;
    } else {
        setError(QString("DMM current configuration failed: %1").arg(result.error));
        return false;
    }
}

bool DeviceManager::configureDMMForResistance(int timeout_ms)
{
    qDebug() << "Configuring DMM for resistance measurement";
    
    DeviceOperation operation;
    operation.command = DeviceCommand::CONFIGURE_CHANNEL;
    operation.parameters["function"] = static_cast<int>(JY8902_2_Wire_Resistance);
    operation.timeout = timeout_ms;
    
    if (!submitOperation("JY8902", operation)) {
        setError("Failed to submit DMM resistance configuration");
        return false;
    }
    
    DeviceResult result = waitForResult("JY8902", timeout_ms);
    if (result.success) {
        qDebug() << "DMM resistance configuration successful";
        return true;
    } else {
        setError(QString("DMM resistance configuration failed: %1").arg(result.error));
        return false;
    }
}

bool DeviceManager::configureDMMForDiodeTest(int timeout_ms)
{
    qDebug() << "Configuring DMM for diode test mode";
    
    // 二极管测试通常使用DC电压测量模式，配合特定的测试条件
    DeviceOperation operation;
    operation.command = DeviceCommand::CONFIGURE_CHANNEL;
    operation.parameters["function"] = static_cast<int>(JY8902_DC_Volts);
    operation.parameters["diode_test_mode"] = true;  // 标记为二极管测试模式
    operation.timeout = timeout_ms;
    
    if (!submitOperation("JY8902", operation)) {
        setError("Failed to submit DMM diode test configuration");
        return false;
    }
    
    DeviceResult result = waitForResult("JY8902", timeout_ms);
    if (result.success) {
        qDebug() << "DMM diode test configuration successful";
        return true;
    } else {
        setError(QString("DMM diode test configuration failed: %1").arg(result.error));
        return false;
    }
}

void DeviceManager::onDeviceStatusChanged(const QString& deviceName, bool ready)
{
    QMutexLocker locker(&errorMutex_);
    
    DeviceStatus newStatus = convertToStatus(ready);
    DeviceStatus oldStatus = deviceStatus_.value(deviceName, DeviceStatus::DISCONNECTED);
    
    if (newStatus != oldStatus) {
        deviceStatus_[deviceName] = newStatus;
        locker.unlock();
        
        qDebug() << "Threaded device status changed:" << deviceName << "status:" << static_cast<int>(newStatus);
        emit deviceStatusChanged(deviceName, newStatus);
    }
}

void DeviceManager::onOperationCompleted(const QString& deviceName, const DeviceResult& result)
{
    qDebug() << "Operation completed on device:" << deviceName 
             << "success:" << result.success 
             << "value:" << result.value;
             
    emit operationCompleted(deviceName, result);
}

void DeviceManager::onDeviceError(const QString& deviceName, const QString& error)
{
    QString fullError = QString("Device %1 error: %2").arg(deviceName, error);
    setError(fullError);
    
    QMutexLocker locker(&errorMutex_);
    deviceStatus_[deviceName] = DeviceStatus::ERROR;
    locker.unlock();
    
    emit deviceStatusChanged(deviceName, DeviceStatus::ERROR);
}

void DeviceManager::setError(const QString& error)
{
    QMutexLocker locker(&errorMutex_);
    last_error_ = error;
    locker.unlock();
    
    qDebug() << "ThreadedDeviceManager Error:" << error;
    emit errorOccurred(error);
}

void DeviceManager::initializeStatusMonitoring()
{
    // 设置状态监控定时器
    statusTimer_->setInterval(1000); // 每秒检查一次
    connect(statusTimer_, &QTimer::timeout, this, &DeviceManager::checkDeviceStatus);
    statusTimer_->start();
}

DeviceStatus DeviceManager::convertToStatus(bool ready) const
{
    return ready ? DeviceStatus::CONNECTED : DeviceStatus::DISCONNECTED;
}

bool DeviceManager::checkDeviceAvailability()
{
    qDebug() << "Checking device availability...";
    
    QStringList unavailable;
      // Check JY5711 AO device
    JY5710_DeviceHandle aoHandle = nullptr;
    if (JY5710_Open(0, &aoHandle) == 0) {
        JY5710_Close(aoHandle);
        qDebug() << "JY5711 AO device: Available";
    } else {
        unavailable << "JY5711 (AO Device)";
        qDebug() << "JY5711 AO device: Not available";
    }
    
    // Check JY5322 DAQ device (slot 5)
    JY5320_DeviceHandle daqHandle5322 = nullptr;
    if (JY5320_Open(5, &daqHandle5322) == 0) {
        JY5320_Close(daqHandle5322);
        qDebug() << "JY5322 DAQ device (slot 5): Available";
    } else {
        unavailable << "JY5322 (DAQ Device, slot 5)";
        qDebug() << "JY5322 DAQ device (slot 5): Not available";
    }
    
    // Check JY5323 DAQ device (slot 3)
    JY5320_DeviceHandle daqHandle5323 = nullptr;
    if (JY5320_Open(3, &daqHandle5323) == 0) {
        JY5320_Close(daqHandle5323);
        qDebug() << "JY5323 DAQ device (slot 3): Available";
    } else {
        unavailable << "JY5323 (DAQ Device, slot 3)";
        qDebug() << "JY5323 DAQ device (slot 3): Not available";
    }
    
    // Check JY8902 DMM device
    JY8902_DeviceHandle dmmHandle = nullptr;
    int32_t result = JY8902_Open(0, &dmmHandle);
    if (result == 0) {
        JY8902_Close(dmmHandle);
        qDebug() << "JY8902 DMM device: Available";
    } else {
        unavailable << QString("JY8902 (DMM Device, error: %1)").arg(result);
        qDebug() << "JY8902 DMM device: Not available, error:" << result;
    }
    
    if (unavailable.isEmpty()) {
        qDebug() << "All devices are available";
        return true;
    } else {
        qDebug() << "Unavailable devices:" << unavailable.join(", ");
        return false;
    }
}

QStringList DeviceManager::getUnavailableDevices()
{
    QStringList unavailable;
      // Quick check without opening/closing devices
    JY5710_DeviceHandle aoHandle = nullptr;
    if (JY5710_Open(0, &aoHandle) != 0) {
        unavailable << "JY5711";
    } else {
        JY5710_Close(aoHandle);
    }
    
    JY5320_DeviceHandle daqHandle5322 = nullptr;
    if (JY5320_Open(5, &daqHandle5322) != 0) {
        unavailable << "JY5322";
    } else {
        JY5320_Close(daqHandle5322);
    }
    
    JY5320_DeviceHandle daqHandle5323 = nullptr;
    if (JY5320_Open(3, &daqHandle5323) != 0) {
        unavailable << "JY5323";
    } else {
        JY5320_Close(daqHandle5323);
    }
    
    JY8902_DeviceHandle dmmHandle = nullptr;
    if (JY8902_Open(0, &dmmHandle) != 0) {
        unavailable << "JY8902";
    } else {
        JY8902_Close(dmmHandle);
    }
    
    return unavailable;
}

void DeviceManager::checkDeviceStatus()
{
    // 定期检查设备状态
    QMutexLocker locker(&errorMutex_);
    
    for (auto it = deviceThreads_.begin(); it != deviceThreads_.end(); ++it) {
        const QString& deviceName = it.key();
        BaseDeviceThread* thread = it.value();
        
        bool isReady = thread->isDeviceReady();
        DeviceStatus currentStatus = deviceStatus_.value(deviceName, DeviceStatus::DISCONNECTED);
        DeviceStatus newStatus = convertToStatus(isReady);
        
        if (newStatus != currentStatus) {
            deviceStatus_[deviceName] = newStatus;
            locker.unlock();
            
            qDebug() << "Device status changed:" << deviceName 
                     << "from" << static_cast<int>(currentStatus) 
                     << "to" << static_cast<int>(newStatus);
            emit deviceStatusChanged(deviceName, newStatus);
            
            locker.relock();
        }
    }
}
