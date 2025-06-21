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
    return outputVoltageAsync(channel, voltage, 5000);
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

// 数据采集接口实现

bool DeviceManager::singlePointAcquisition(const QString& deviceName, int channel, double& result, 
                                          double rangeMin, double rangeMax, int timeout_ms)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    
    // 配置通道
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    configOp.channel = channel;
    configOp.inputRangeMin = rangeMin;
    configOp.inputRangeMax = rangeMax;
    configOp.parameters["mode"] = "single";
    
    device->submitOperation(configOp);
    DeviceResult configResult = device->waitForResult(timeout_ms);
    
    if (!configResult.success) {
        setError(QString("Failed to configure channel %1: %2").arg(channel).arg(configResult.error));
        return false;
    }
    
    // 读取数据
    DeviceOperation readOp(DeviceCommand::READ_DATA);
    readOp.channel = channel;
    readOp.timeout = timeout_ms;
    
    device->submitOperation(readOp);
    DeviceResult readResult = device->waitForResult(timeout_ms);
    
    if (readResult.success) {
        result = readResult.value;
        return true;
    } else {
        setError(QString("Failed to read from channel %1: %2").arg(channel).arg(readResult.error));
        return false;
    }
}

bool DeviceManager::singlePointAcquisition(const QString& deviceName, const QVector<int>& channels, 
                                          QVector<double>& results, double rangeMin, double rangeMax, int timeout_ms)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    results.clear();
    results.resize(channels.size());
    
    // 配置所有通道
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    configOp.channels = channels;
    configOp.inputRangeMin = rangeMin;
    configOp.inputRangeMax = rangeMax;
    configOp.parameters["mode"] = "single";
    
    device->submitOperation(configOp);
    DeviceResult configResult = device->waitForResult(timeout_ms);
    
    if (!configResult.success) {
        setError(QString("Failed to configure channels: %1").arg(configResult.error));
        return false;
    }
    
    // 读取所有通道数据
    DeviceOperation readOp(DeviceCommand::READ_DATA);
    readOp.channel = -1; // 读取所有通道
    readOp.timeout = timeout_ms;
    
    device->submitOperation(readOp);
    DeviceResult readResult = device->waitForResult(timeout_ms);
    
    if (readResult.success) {
        QVariantMap channelData = readResult.data;
        for (int i = 0; i < channels.size(); ++i) {
            QString key = QString("ch%1").arg(channels[i]);
            if (channelData.contains(key)) {
                results[i] = channelData[key].toDouble();
            } else {
                setError(QString("Missing data for channel %1").arg(channels[i]));
                return false;
            }
        }
        return true;
    } else {
        setError(QString("Failed to read channels: %1").arg(readResult.error));
        return false;
    }
}

bool DeviceManager::configureMultiPointAcquisition(const QString& deviceName, const QVector<int>& channels,
                                                  double sampleRate, int samplesPerChannel,
                                                  double rangeMin, double rangeMax)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    configOp.channels = channels;
    configOp.sampleRate = sampleRate;
    configOp.samplesPerChannel = samplesPerChannel;
    configOp.inputRangeMin = rangeMin;
    configOp.inputRangeMax = rangeMax;
    configOp.parameters["mode"] = "multi";
    
    device->submitOperation(configOp);
    DeviceResult result = device->waitForResult(5000);
    
    if (!result.success) {
        setError(QString("Failed to configure multi-point acquisition: %1").arg(result.error));
        return false;
    }
    
    return true;
}

bool DeviceManager::startMultiPointAcquisition(const QString& deviceName)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    
    DeviceOperation startOp(DeviceCommand::START_MEASUREMENT);
    device->submitOperation(startOp);
    DeviceResult result = device->waitForResult(5000);
    
    if (!result.success) {
        setError(QString("Failed to start multi-point acquisition: %1").arg(result.error));
        return false;
    }
    
    return true;
}

bool DeviceManager::readMultiPointData(const QString& deviceName, QVector<QVector<double>>& channelData, int timeout_ms)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    
    DeviceOperation readOp(DeviceCommand::READ_DATA);
    readOp.timeout = timeout_ms;
    
    device->submitOperation(readOp);
    DeviceResult result = device->waitForResult(timeout_ms);
    
    qDebug() << "DeviceManager::readMultiPointData - Device:" << deviceName
             << "Success:" << result.success
             << "Error:" << result.error
             << "Data keys:" << result.data.keys();
    
    if (result.success) {
        // 检查数据是否存在
        if (result.data.contains("channelData")) {
            QVariant channelDataVariant = result.data["channelData"];
            qDebug() << "Channel data variant type:" << channelDataVariant.typeName()
                     << "isValid:" << channelDataVariant.isValid();
            
            channelData = channelDataVariant.value<QVector<QVector<double>>>();
            qDebug() << "Extracted channel data size:" << channelData.size();
            
            for (int ch = 0; ch < channelData.size(); ++ch) {
                qDebug() << "Channel" << ch << "has" << channelData[ch].size() << "samples";
            }
            
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

bool DeviceManager::stopMultiPointAcquisition(const QString& deviceName)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    
    DeviceOperation stopOp(DeviceCommand::STOP_MEASUREMENT);
    device->submitOperation(stopOp);
    DeviceResult result = device->waitForResult(5000);
    
    if (!result.success) {
        setError(QString("Failed to stop multi-point acquisition: %1").arg(result.error));
        return false;
    }
    
    return true;
}

bool DeviceManager::configureContinuousAcquisition(const QString& deviceName, const QVector<int>& channels,
                                                  double sampleRate, int bufferSize,
                                                  double rangeMin, double rangeMax)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    configOp.channels = channels;
    configOp.sampleRate = sampleRate;
    configOp.inputRangeMin = rangeMin;
    configOp.inputRangeMax = rangeMax;
    configOp.parameters["mode"] = "continuous";
    configOp.parameters["bufferSize"] = bufferSize;
    
    device->submitOperation(configOp);
    DeviceResult result = device->waitForResult(5000);
    
    if (!result.success) {
        setError(QString("Failed to configure continuous acquisition: %1").arg(result.error));
        return false;
    }
    
    return true;
}

bool DeviceManager::startContinuousAcquisition(const QString& deviceName)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    
    DeviceOperation startOp(DeviceCommand::START_MEASUREMENT);
    device->submitOperation(startOp);
    DeviceResult result = device->waitForResult(5000);
    
    if (!result.success) {
        setError(QString("Failed to start continuous acquisition: %1").arg(result.error));
        return false;
    }
    
    return true;
}

bool DeviceManager::readContinuousData(const QString& deviceName, QVector<QVector<double>>& channelData, 
                                      int keepSamples, int timeout_ms)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    
    DeviceOperation readOp(DeviceCommand::READ_DATA);
    readOp.timeout = timeout_ms;
    readOp.parameters["keepSamples"] = keepSamples;
    
    device->submitOperation(readOp);
    DeviceResult result = device->waitForResult(timeout_ms);
    
    if (result.success) {
        channelData = result.data["channelData"].value<QVector<QVector<double>>>();
        return true;
    } else {
        setError(QString("Failed to read continuous data: %1").arg(result.error));
        return false;
    }
}

bool DeviceManager::stopContinuousAcquisition(const QString& deviceName)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    
    DeviceOperation stopOp(DeviceCommand::STOP_MEASUREMENT);
    device->submitOperation(stopOp);
    DeviceResult result = device->waitForResult(5000);
    
    if (!result.success) {
        setError(QString("Failed to stop continuous acquisition: %1").arg(result.error));
        return false;
    }
    
    return true;
}

bool DeviceManager::configureAcquisition(const QString& deviceName, const QString& mode,
                                        const QVector<int>& channels, double sampleRate,
                                        int samplesPerChannel, double rangeMin, double rangeMax,
                                        int bufferSize)
{
    if (!deviceThreads_.contains(deviceName)) {
        setError(QString("Device %1 not found").arg(deviceName));
        return false;
    }
    
    BaseDeviceThread* device = deviceThreads_[deviceName];
    
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    configOp.channels = channels;
    configOp.sampleRate = sampleRate;
    configOp.samplesPerChannel = samplesPerChannel;
    configOp.inputRangeMin = rangeMin;
    configOp.inputRangeMax = rangeMax;
    configOp.parameters["mode"] = mode;
    configOp.parameters["bufferSize"] = bufferSize;
    
    device->submitOperation(configOp);
    DeviceResult result = device->waitForResult(5000);
    
    if (!result.success) {
        setError(QString("Failed to configure acquisition: %1").arg(result.error));
        return false;
    }
    
    return true;
}

bool DeviceManager::isAcquisitionActive(const QString& deviceName)
{
    if (!deviceThreads_.contains(deviceName)) {
        return false;
    }
    
    // 这里可以通过查询设备状态来确定采集是否活动
    // 简化实现，实际应该查询设备内部状态
    return true; // 临时实现
}

QVariantMap DeviceManager::getAcquisitionStatus(const QString& deviceName)
{
    QVariantMap status;
    
    if (!deviceThreads_.contains(deviceName)) {
        status["error"] = QString("Device %1 not found").arg(deviceName);
        return status;
    }
    
    // 简化实现，实际应该查询设备详细状态
    status["deviceName"] = deviceName;
    status["isActive"] = isAcquisitionActive(deviceName);
    status["deviceType"] = "JY5320";
    
    return status;
}

QVector<QString> DeviceManager::getDAQDevices() const
{
    QVector<QString> daqDevices;
    
    for (auto it = deviceThreads_.begin(); it != deviceThreads_.end(); ++it) {
        const QString& deviceName = it.key();
        if (deviceName.contains("JY5320") || deviceName.contains("JY5322")) {
            daqDevices.append(deviceName);
        }
    }
    
    return daqDevices;
}

bool DeviceManager::exportAcquisitionData(const QVector<QVector<double>>& channelData, 
                                         const QVector<int>& channels, const QString& fileName,
                                         const QString& format)
{
    if (channelData.isEmpty() || channels.isEmpty()) {
        setError("No data to export");
        return false;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(QString("Failed to open file %1 for writing").arg(fileName));
        return false;
    }
    
    QTextStream out(&file);
    
    if (format.toLower() == "csv") {
        // CSV格式导出
        
        // 写入标题行
        out << "Sample";
        for (int ch : channels) {
            out << ",Channel_" << ch;
        }
        out << "\n";
        
        // 写入数据
        int maxSamples = 0;
        for (const auto& chData : channelData) {
            maxSamples = qMax(maxSamples, chData.size());
        }
        
        for (int sample = 0; sample < maxSamples; ++sample) {
            out << sample;
            for (int ch = 0; ch < channelData.size(); ++ch) {
                if (sample < channelData[ch].size()) {
                    out << "," << QString::number(channelData[ch][sample], 'f', 6);
                } else {
                    out << ",";
                }
            }
            out << "\n";
        }
    } else {
        setError(QString("Unsupported export format: %1").arg(format));
        return false;
    }
    
    file.close();
    return true;
}

QString DeviceManager::acquisitionModeToString(const QString& mode) const
{
    if (mode == "single") return "Single Point";
    if (mode == "multi" || mode == "finite") return "Multi Point";
    if (mode == "continuous") return "Continuous";
    return "Unknown";
}

QStringList DeviceManager::getSupportedAcquisitionModes() const
{
    return QStringList() << "single" << "multi" << "continuous";
}

// 简化的同步测试实现（用于验证修复效果）
bool DeviceManager::testSynchronizedOutputAndAcquisition(double sineFreq, double sineAmplitude, 
                                                        double sampleRate, int samplesPerChannel,
                                                        int timeout_ms)
{
    qDebug() << "=== 开始简化同步测试 ===";
    qDebug() << "参数 - 正弦波频率:" << sineFreq << "Hz, 幅度:" << sineAmplitude << "V";
    qDebug() << "采样率:" << sampleRate << "Hz, 每通道采样数:" << samplesPerChannel;
    
    // 1. 检查设备状态
    if (!isSystemReady()) {
        setError("系统未就绪，请检查设备连接状态");
        return false;
    }
    
    try {
        // 2. 先进行简单的多点采集测试（不使用同步）
        qDebug() << "步骤1: 测试JY5322多点采集...";
        
        QVector<int> daqChannels = {0, 1};
        if (!configureMultiPointAcquisition("JY5322", daqChannels, sampleRate, samplesPerChannel, -10.0, 10.0)) {
            setError("JY5322多点采集配置失败: " + getLastError());
            return false;
        }
        
        if (!startMultiPointAcquisition("JY5322")) {
            setError("JY5322采集启动失败: " + getLastError());
            return false;
        }
        
        qDebug() << "JY5322采集已启动，等待数据采集完成...";
        
        // 3. 等待数据采集完成
        bool dataReady = false;
        int maxAttempts = 100;  // 增加尝试次数
        int attempt = 0;
        
        QVector<QVector<double>> channelData;
        
        while (!dataReady && attempt < maxAttempts) {
            attempt++;
            
            if (readMultiPointData("JY5322", channelData, 1000)) {
                if (!channelData.isEmpty() && !channelData[0].isEmpty()) {
                    dataReady = true;
                    qDebug() << "数据采集成功！尝试次数:" << attempt;
                    break;
                }
            }
            
            if (attempt % 20 == 0) {
                qDebug() << "等待数据... 尝试" << attempt << "/" << maxAttempts;
            }
            
            QThread::msleep(50);  // 50ms间隔
        }
        
        // 4. 停止采集
        if (!stopMultiPointAcquisition("JY5322")) {
            qDebug() << "警告：停止采集失败";
        }
        
        // 5. 分析结果
        if (dataReady && !channelData.isEmpty()) {
            qDebug() << "✓ 多点采集测试成功！";
            qDebug() << "采集到" << channelData.size() << "个通道的数据";
            
            for (int ch = 0; ch < channelData.size(); ++ch) {
                if (!channelData[ch].isEmpty()) {
                    double avgValue = 0.0;
                    double maxValue = *std::max_element(channelData[ch].begin(), channelData[ch].end());
                    double minValue = *std::min_element(channelData[ch].begin(), channelData[ch].end());
                    
                    for (double val : channelData[ch]) {
                        avgValue += val;
                    }
                    avgValue /= channelData[ch].size();
                    
                    qDebug() << QString("通道%1: 样本数=%2, 平均值=%3V, 最大值=%4V, 最小值=%5V")
                                .arg(daqChannels[ch])
                                .arg(channelData[ch].size())
                                .arg(avgValue, 0, 'f', 6)
                                .arg(maxValue, 0, 'f', 6)
                                .arg(minValue, 0, 'f', 6);
                }
            }
            
            // 现在测试AO输出
            qDebug() << "步骤2: 测试JY5711输出...";
            
            // 简单输出测试
            if (outputVoltageAsync(1, 4.0, timeout_ms)) {
                qDebug() << "✓ JY5711 端口1输出4V成功";
            } else {
                qDebug() << "❌ JY5711输出失败:" << getLastError();
            }
            
            // 恢复0V输出
            outputVoltageAsync(1, 0.0, timeout_ms);
            
            qDebug() << "=== 简化同步测试完成 ===";
            return true;
            
        } else {
            setError(QString("数据采集失败，尝试了%1次").arg(attempt));
            qDebug() << "❌ 数据采集失败，尝试了" << attempt << "次";
            return false;
        }
        
    } catch (const std::exception& e) {
        setError(QString("测试异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        setError("测试发生未知异常");
        return false;
    }
}

DeviceManager::WaitResult DeviceManager::waitForDataWithEventLoop(
    const QString& deviceName, 
    QVector<QVector<double>>& channelData,
    int maxAttempts,
    int checkInterval,
    int timeout)
{
    WaitResult result;
    
    // 创建事件循环和定时器
    QEventLoop eventLoop;
    QTimer timeoutTimer;
    QTimer checkTimer;
    
    // 设置超时定时器
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(timeout);
    
    // 设置检查定时器
    checkTimer.setInterval(checkInterval);
    
    // 连接超时信号
    connect(&timeoutTimer, &QTimer::timeout, [&]() {
        result.timeout = true;
        result.errorMessage = QString("等待数据超时 (%1ms)").arg(timeout);
        eventLoop.quit();
    });
    
    // 连接检查定时器信号
    connect(&checkTimer, &QTimer::timeout, [&]() {
        result.attempts++;
        
        // 尝试读取数据
        if (readMultiPointData(deviceName, channelData, 1000)) {
            if (!channelData.isEmpty() && !channelData[0].isEmpty()) {
                result.success = true;
                eventLoop.quit();
                return;
            }
        }
        
        // 检查是否达到最大尝试次数
        if (result.attempts >= maxAttempts) {
            result.errorMessage = QString("达到最大尝试次数 (%1)").arg(maxAttempts);
            eventLoop.quit();
        }
    });
    
    // 启动定时器
    timeoutTimer.start();
    checkTimer.start();
    
    // 运行事件循环 - 不会阻塞Qt的事件系统
    eventLoop.exec();
    
    // 停止定时器
    timeoutTimer.stop();
    checkTimer.stop();
    
    return result;
}
