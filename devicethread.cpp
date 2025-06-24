#include "devicethread.h"
#include <QDebug>
#include <QApplication>
#include <QThread>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QCoreApplication>
#include "include/JYDAQError.h"
#include "include/JYDMMError.h"

// DeviceSyncController 实现
DeviceSyncController* DeviceSyncController::instance_ = nullptr;

DeviceSyncController* DeviceSyncController::instance()
{
    if (!instance_) {
        instance_ = new DeviceSyncController(nullptr);
    }
    return instance_;
}

DeviceSyncController::DeviceSyncController(QObject* parent)
    : QObject(parent)
{
}

DeviceSyncController::~DeviceSyncController()
{    // Clean up all synchronization objects
    for (auto it = syncConditions_.begin(); it != syncConditions_.end(); ++it) {
        delete it.value();
    }
    for (auto it = syncMutexes_.begin(); it != syncMutexes_.end(); ++it) {
        delete it.value();
    }
}

void DeviceSyncController::createSyncGroup(const QString& groupName, const QStringList& deviceNames)
{    QMutexLocker locker(&groupsMutex_);
    
    SyncGroup group;
    group.deviceNames = deviceNames;
    syncGroups_[groupName] = group;
    
    // Create synchronization objects for this group
    syncConditions_[groupName] = new QWaitCondition();
    syncMutexes_[groupName] = new QMutex();
    
    qDebug() << "Created sync group:" << groupName << "with devices:" << deviceNames;
}

void DeviceSyncController::removeSyncGroup(const QString& groupName)
{
    QMutexLocker locker(&groupsMutex_);
    syncGroups_.remove(groupName);
    
    // Clean up synchronization objects
    if (syncConditions_.contains(groupName)) {
        delete syncConditions_[groupName];
        syncConditions_.remove(groupName);
    }
    if (syncMutexes_.contains(groupName)) {
        delete syncMutexes_[groupName];
        syncMutexes_.remove(groupName);
    }
    
    qDebug() << "Removed sync group:" << groupName;
}

bool DeviceSyncController::waitForSync(const QString& groupName, const QString& deviceName, int timeout)
{
    QMutexLocker locker(&groupsMutex_);
    
    if (!syncGroups_.contains(groupName) || !syncMutexes_.contains(groupName) || !syncConditions_.contains(groupName)) {
        return false;
    }
    
    SyncGroup& group = syncGroups_[groupName];
    QMutex* mutex = syncMutexes_[groupName];
    QWaitCondition* condition = syncConditions_[groupName];
    
    locker.unlock(); // Release the groups mutex
    QMutexLocker syncLocker(mutex); // Lock the specific group mutex
    
    // 标记设备就绪
    if (!group.readyDevices.contains(deviceName)) {
        group.readyDevices.append(deviceName);
    }
      // 如果所有设备都就绪，触发同步
    if (group.readyDevices.size() == group.deviceNames.size()) {
        group.triggered = true;
        condition->wakeAll();
        emit syncTriggered(groupName);
        return true;
    }
    
    // 等待其他设备就绪
    return condition->wait(mutex, timeout);
}

void DeviceSyncController::signalReady(const QString& groupName, const QString& deviceName)
{
    QMutexLocker locker(&groupsMutex_);
    
    if (!syncGroups_.contains(groupName) || !syncMutexes_.contains(groupName) || !syncConditions_.contains(groupName)) {
        return;
    }
    
    SyncGroup& group = syncGroups_[groupName];
    QMutex* mutex = syncMutexes_[groupName];
    QWaitCondition* condition = syncConditions_[groupName];
    
    locker.unlock(); // Release the groups mutex
    QMutexLocker syncLocker(mutex); // Lock the specific group mutex
    
    if (!group.readyDevices.contains(deviceName)) {
        group.readyDevices.append(deviceName);
    }
    
    if (group.readyDevices.size() == group.deviceNames.size()) {
        group.triggered = true;
        condition->wakeAll();
        emit syncTriggered(groupName);
    }
}

void DeviceSyncController::triggerSync(const QString& groupName)
{
    QMutexLocker locker(&groupsMutex_);
    
    if (syncGroups_.contains(groupName) && syncMutexes_.contains(groupName) && syncConditions_.contains(groupName)) {
        SyncGroup& group = syncGroups_[groupName];
        QMutex* mutex = syncMutexes_[groupName];
        QWaitCondition* condition = syncConditions_[groupName];
        
        locker.unlock(); // Release the groups mutex
        QMutexLocker syncLocker(mutex); // Lock the specific group mutex
        
        group.triggered = true;
        condition->wakeAll();
        emit syncTriggered(groupName);
    }
}

void DeviceSyncController::setGlobalTrigger(bool enabled)
{
    globalTriggerEnabled_ = enabled;
}

void DeviceSyncController::sendGlobalTrigger()
{
    if (globalTriggerEnabled_) {
        emit globalTriggerReceived();
    }
}

// BaseDeviceThread 实现
BaseDeviceThread::BaseDeviceThread(const QString& deviceName, QObject* parent)
    : QThread(parent)
    , deviceName_(deviceName)
    , deviceReady_(0)
    , stopRequested_(0)
    , syncController_(DeviceSyncController::instance())
{
    connect(syncController_, &DeviceSyncController::syncTriggered,
            this, &BaseDeviceThread::onSyncTriggered);
    connect(syncController_, &DeviceSyncController::globalTriggerReceived,
            this, &BaseDeviceThread::onGlobalTrigger);
}

BaseDeviceThread::~BaseDeviceThread()
{
    stopThread();
}

void BaseDeviceThread::submitOperation(const DeviceOperation& operation)
{
    QMutexLocker locker(&operationMutex_);
    operationQueue_.enqueue(operation);
    operationCondition_.wakeOne();
}

DeviceResult BaseDeviceThread::waitForResult(int timeout)
{
    QMutexLocker locker(&operationMutex_);
    
    if (resultQueue_.isEmpty()) {
        operationCondition_.wait(&operationMutex_, timeout);
    }
    
    if (!resultQueue_.isEmpty()) {
        return resultQueue_.dequeue();
    }
    
    DeviceResult result;
    result.success = false;
    result.error = "Timeout waiting for operation result";
    return result;
}

void BaseDeviceThread::joinSyncGroup(const QString& groupName)
{
    currentSyncGroup_ = groupName;
}

void BaseDeviceThread::leaveSyncGroup()
{
    currentSyncGroup_.clear();
}

void BaseDeviceThread::stopThread()
{
    stopRequested_ = 1;
    
    {
        QMutexLocker locker(&operationMutex_);
        operationCondition_.wakeAll();
    }
    
    if (!wait(3000)) {
        terminate();
        wait(1000);
    }
}

void BaseDeviceThread::run()
{
    qDebug() << "Starting device thread for:" << deviceName_;
      // 初始化设备
    if (initializeDevice()) {
        deviceReady_ = 1;
        emit deviceStatusChanged(deviceName_, true);
        qDebug() << "Device" << deviceName_ << "initialized successfully";
    } else {
        emit errorOccurred(deviceName_, "Failed to initialize device");
        return;
    }
      // 主循环
    while (!stopRequested_.loadRelaxed()) {
        DeviceOperation operation;
        bool hasOperation = false;
        
        {
            QMutexLocker locker(&operationMutex_);
            if (operationQueue_.isEmpty()) {
                operationCondition_.wait(&operationMutex_, 100);
            }
            
            if (!operationQueue_.isEmpty()) {
                operation = operationQueue_.dequeue();
                hasOperation = true;
            }
        }
        
        if (hasOperation) {
            // 处理同步操作
            if (!operation.syncGroup.isEmpty()) {
                if (syncController_->waitForSync(operation.syncGroup, deviceName_, operation.timeout)) {
                    // 同步成功，执行操作
                    if (operation.syncDelay > 0) {
                        QThread::usleep(operation.syncDelay);
                    }
                } else {
                    // 同步超时
                    DeviceResult result;
                    result.success = false;
                    result.error = QString("Sync timeout for group: %1").arg(operation.syncGroup);
                    
                    {
                        QMutexLocker locker(&operationMutex_);
                        resultQueue_.enqueue(result);
                        operationCondition_.wakeAll();
                    }
                    
                    emit operationCompleted(deviceName_, result);
                    continue;
                }
            }
            
            // 执行操作
            DeviceResult result = executeOperation(operation);
            
            {
                QMutexLocker locker(&operationMutex_);
                resultQueue_.enqueue(result);
                operationCondition_.wakeAll();
            }
            
            emit operationCompleted(deviceName_, result);
        }
        
        QThread::msleep(1);
    }
      // 关闭设备
    shutdownDevice();
    deviceReady_ = 0;
    emit deviceStatusChanged(deviceName_, false);
    
    qDebug() << "Device thread stopped for:" << deviceName_;
}

void BaseDeviceThread::onSyncTriggered(const QString& groupName)
{
    if (currentSyncGroup_ == groupName) {
        handleSyncTrigger();
    }
}

void BaseDeviceThread::onGlobalTrigger()
{
    handleSyncTrigger();
}

// AODeviceThread 实现
AODeviceThread::AODeviceThread(QObject* parent)
    : BaseDeviceThread("JY5711", parent)
    , deviceHandle_(nullptr)
{
}

AODeviceThread::~AODeviceThread()
{
    stopThread();
}

bool AODeviceThread::initializeDevice()
{
    int32_t result = JY5710_Open(0, &deviceHandle_);
    if (result != Success) {
        qDebug() << "Failed to open JY5711 AO device, error:" << result;
        return false;
    }

    return true;
}

bool AODeviceThread::initializeChannel()
{
    shutdownDevice();
    int32_t result = JY5710_Open(0, &deviceHandle_);
    if (result != Success) {
        qDebug() << "Failed to open JY5711 AO device, error:" << result;
        return false;
    }

    // 初始化后将所有端口输出设置为0V，防止保留上次的输出值
    qDebug() << "Resetting all AO channels to 0V...";
    
    // 设置所有32个通道为0V输出
    const int maxChannels = 32;
    std::vector<unsigned char> channels(maxChannels);
    std::vector<double> lowRanges(maxChannels, -10.0);
    std::vector<double> highRanges(maxChannels, 10.0);
    std::vector<double> zeroValues(maxChannels, 0.0);
    
    // 准备通道数组 (0-31)
    for (int i = 0; i < maxChannels; i++) {
        channels[i] = static_cast<unsigned char>(i);
    }
      // 启用所有通道
    result = JY5710_AO_EnableChannel(deviceHandle_, maxChannels,
                                   channels.data(), lowRanges.data(), highRanges.data());
    if (result != Success) {
        qDebug() << "Warning: Failed to enable all channels for reset, error:" << result;
        return false;
    }
    
    // 设置为单点输出模式
    result = JY5710_AO_SetMode(deviceHandle_, JY5710_AO_Single);
    if (result != Success) {
        qDebug() << "Warning: Failed to set single output mode, error:" << result;
        return false;
    }
    
    // 先启动任务以激活单点输出模式
    result = JY5710_AO_Start(deviceHandle_);
    if (result != Success) {
        qDebug() << "Warning: Failed to start AO task, error:" << result;
        return false;
    }
    
    // 写入0值到所有通道 - 使用单点写入接口
    for (int i = 0; i < maxChannels; i++) {
        double zeroValue = 0.0;
        result = JY5710_AO_WriteSinglePoint(deviceHandle_, &zeroValue, i);
        if (result != Success) {
            qDebug() << "Warning: Failed to write zero value to channel" << i << ", error:" << result;
        }
    }
    
    qDebug() << "Successfully reset all" << maxChannels << "channels to 0V using single-point output";
    
    // 清空通道状态记录
    channelStates_.clear();
    enabledChannels_.clear();
    
    qDebug() << "JY5711 AO device initialized successfully";
    return true;
}
void AODeviceThread::shutdownDevice()
{
    if (deviceHandle_) {
        JY5710_Close(deviceHandle_);
        deviceHandle_ = nullptr;
    }
    enabledChannels_.clear();
    channelStates_.clear();
}

DeviceResult AODeviceThread::executeOperation(const DeviceOperation& operation)
{
    DeviceResult result;
    
    if (!deviceHandle_) {
        result.error = "Device not initialized";
        return result;
    }
    
    int32_t apiResult = 0;
    
    switch (operation.command) {        
        case DeviceCommand::INITIALIZE:
            // Check if already initialized
            result.success = initializeChannel();
            if (!result.success) {
                result.error = "Failed to initialize AO device";
            }
            return result;
            
        case DeviceCommand::SHUTDOWN:
            shutdownDevice();
            result.success = true;
            return result;
            
        case DeviceCommand::CONFIGURE_CHANNEL:
            // 配置AO通道 - 每次配置都重启设备
            result = configureChannelWithRestart(operation);
            break;
            
        case DeviceCommand::WRITE_DATA:
            // 输出波形数据
            result = outputWaveform(operation);
            break;
            
        case DeviceCommand::START_MEASUREMENT:
            apiResult = JY5710_AO_Start(deviceHandle_);
            if (apiResult == Success) {
                result.success = true;
                qDebug() << "AO started";
            } else {
                result.error = QString("Failed to start AO, error: %1").arg(apiResult);
            }
            break;
            
        case DeviceCommand::STOP_MEASUREMENT:
            apiResult = JY5710_AO_Stop(deviceHandle_);
            if (apiResult == Success) {
                result.success = true;
                qDebug() << "AO stopped";
            } else {
                result.error = QString("Failed to stop AO, error: %1").arg(apiResult);
            }
            break;
            
        case DeviceCommand::SYNC_TRIGGER:
            // 发送软件触发
            apiResult = JY5710_AO_SendSoftTrigger(deviceHandle_);
            if (apiResult == Success) {
                result.success = true;
                qDebug() << "AO soft trigger sent";
            } else {
                result.error = QString("Failed to send AO soft trigger, error: %1").arg(apiResult);
            }
            break;
            
        default:
            result.error = "Unsupported operation for AO device";
            break;
    }
    
    return result;
}

DeviceResult AODeviceThread::configureChannelWithRestart(const DeviceOperation& operation)
{
    DeviceResult result;
    
    qDebug() << "Configuring AO channel with device restart...";
    
    initializeChannel();
    // 1. 关闭当前设备
    shutdownDevice();
    QThread::msleep(100);
    
    // 2. 重新初始化设备
    if (!initializeDevice()) {
        result.error = "Failed to reinitialize device during channel configuration";
        return result;
    }
    
    // 3. 解析波形配置参数
    QVariantMap params = operation.parameters;
    int channelCount = params.value("channelCount", 1).toInt();
    QVariantList waveformConfigs = params.value("waveforms").toList();
    
    if (waveformConfigs.isEmpty()) {
        result.error = "No waveform configurations provided";
        return result;
    }
    
    // 4. 准备通道配置
    std::vector<unsigned char> channels;
    std::vector<double> lowRanges;
    std::vector<double> highRanges;
    
    for (const QVariant& configVar : waveformConfigs) {
        QVariantMap config = configVar.toMap();
        int channel = config.value("channel").toInt();
        double lowRange = config.value("lowRange", -10.0).toDouble();
        double highRange = config.value("highRange", 10.0).toDouble();
        
        channels.push_back(static_cast<unsigned char>(channel));
        lowRanges.push_back(lowRange);
        highRanges.push_back(highRange);
        
        enabledChannels_.insert(channel);
    }
    
    // 5. 配置设备参数
    double sampleRate = params.value("sampleRate", 1000000.0).toDouble();
    double actualUpdateRate = 0;
    
    // 启用通道
    int32_t apiResult = JY5710_AO_EnableChannel(deviceHandle_, channelCount, 
                                               channels.data(), lowRanges.data(), highRanges.data());
    if (apiResult != Success) {
        result.error = QString("Failed to enable AO channels, error: %1").arg(apiResult);
        return result;
    }
    
    // 设置更新速率
    apiResult = JY5710_AO_SetUpdateRate(deviceHandle_, sampleRate, &actualUpdateRate);
    if (apiResult != Success) {
        result.error = QString("Failed to set AO update rate, error: %1").arg(apiResult);
        return result;
    }
    
    // 设置连续循环模式
    apiResult = JY5710_AO_SetMode(deviceHandle_, JY5710_AO_ContinuousWrapping);
    if (apiResult != Success) {
        result.error = QString("Failed to set AO mode, error: %1").arg(apiResult);
        return result;
    }
    
    // 设置软件触发
    apiResult = JY5710_AO_SetStartTriggerType(deviceHandle_, JY5710_AO_Soft);
    if (apiResult != Success) {
        result.error = QString("Failed to set AO trigger type, error: %1").arg(apiResult);
        return result;
    }
    
    result.success = true;
    result.data["actualSampleRate"] = actualUpdateRate;
    qDebug() << "AO channels configured successfully with sample rate:" << actualUpdateRate;
    
    return result;
}

DeviceResult AODeviceThread::outputWaveform(const DeviceOperation& operation)
{
    DeviceResult result;
    
    QVariantMap params = operation.parameters;
    QVariantList waveformConfigs = params.value("waveforms").toList();
    int sampleRate = params.value("sampleRate", 1000000).toInt();
    int samplesPerChannel = params.value("samplesPerChannel", sampleRate).toInt();
    
    if (waveformConfigs.isEmpty()) {
        result.error = "No waveform configurations provided";
        return result;
    }
    
    qDebug() << "Generating waveform data for" << waveformConfigs.size() << "channels";
    
    // 创建波形生成器映射
    std::map<int, std::unique_ptr<Waveform>> waveformGenerators;
    std::vector<int> channelOrder;
    
    for (const QVariant& configVar : waveformConfigs) {
        QVariantMap config = configVar.toMap();
        int channel = config.value("channel").toInt();
        PXIe5711_testtype waveformType = static_cast<PXIe5711_testtype>(config.value("type").toInt());
        double amplitude = config.value("amplitude").toDouble();
        double frequency = config.value("frequency", 1000.0).toDouble();
        double dutyCycle = config.value("dutyCycle", 0.5).toDouble();
        
        channelOrder.push_back(channel);
        
        // 根据波形类型创建相应的波形生成器
        switch (waveformType) {
            case PXIe5711_testtype::SineWave:
                waveformGenerators[channel] = std::make_unique<SineWave>(amplitude, frequency);
                qDebug() << "Created SineWave for channel" << channel << "- amp:" << amplitude << "freq:" << frequency;
                break;
                
            case PXIe5711_testtype::SquareWave:
                waveformGenerators[channel] = std::make_unique<SquareWave>(amplitude, frequency, dutyCycle);
                qDebug() << "Created SquareWave for channel" << channel << "- amp:" << amplitude << "freq:" << frequency << "duty:" << dutyCycle;
                break;
                
            case PXIe5711_testtype::TriangleWave:
                waveformGenerators[channel] = std::make_unique<TriangleWave>(amplitude, frequency);
                qDebug() << "Created TriangleWave for channel" << channel << "- amp:" << amplitude << "freq:" << frequency;
                break;
                
            case PXIe5711_testtype::StepWave:
                waveformGenerators[channel] = std::make_unique<StepWave>(amplitude);
                qDebug() << "Created StepWave for channel" << channel << "- amp:" << amplitude;
                break;
                
            case PXIe5711_testtype::HighLevelWave:
                waveformGenerators[channel] = std::make_unique<HighLevelWave>(amplitude);
                qDebug() << "Created HighLevelWave for channel" << channel << "- amp:" << amplitude;
                break;
                
            case PXIe5711_testtype::LowLevelWave:
                waveformGenerators[channel] = std::make_unique<LowLevelWave>(amplitude);
                qDebug() << "Created LowLevelWave for channel" << channel << "- amp:" << amplitude;
                break;
                
            case PXIe5711_testtype::PulseWave:
                waveformGenerators[channel] = std::make_unique<PulseWave>(amplitude, frequency);
                qDebug() << "Created PulseWave for channel" << channel << "- amp:" << amplitude << "freq:" << frequency;
                break;
                
            case PXIe5711_testtype::RampWave:
                waveformGenerators[channel] = std::make_unique<RampWave>(amplitude, frequency);
                qDebug() << "Created RampWave for channel" << channel << "- amp:" << amplitude << "freq:" << frequency;
                break;
                
            default:
                qDebug() << "Unknown waveform type for channel" << channel << ", using DC level";
                waveformGenerators[channel] = std::make_unique<StepWave>(amplitude);
                break;
        }
    }
    
    // 生成交错的波形数据
    int totalSamples = samplesPerChannel * channelOrder.size();
    std::vector<double> waveformData(totalSamples);
    
    for (int sampleIndex = 0; sampleIndex < samplesPerChannel; sampleIndex++) {
        for (int channelIndex = 0; channelIndex < channelOrder.size(); channelIndex++) {
            int channel = channelOrder[channelIndex];
            int dataIndex = sampleIndex * channelOrder.size() + channelIndex;
            
            if (waveformGenerators.find(channel) != waveformGenerators.end()) {
                waveformData[dataIndex] = waveformGenerators[channel]->generate(sampleIndex, sampleRate);
            } else {
                waveformData[dataIndex] = 0.0;
            }
        }
    }
    
    qDebug() << "Generated" << totalSamples << "samples of waveform data";
    
    // 写入波形数据到设备
    unsigned int actualWriteSamples = 0;
    int32_t apiResult = JY5710_AO_WriteData(deviceHandle_, waveformData.data(), 
                                           totalSamples, -1, &actualWriteSamples);
    
    if (apiResult != Success) {
        result.error = QString("Failed to write waveform data, error: %1").arg(apiResult);
        return result;
    }
    
    // 启动AO输出
    apiResult = JY5710_AO_Start(deviceHandle_);
    if (apiResult != Success) {
        result.error = QString("Failed to start AO output, error: %1").arg(apiResult);
        return result;
    }
    
    result.success = true;
    result.data["actualWriteSamples"] = static_cast<int>(actualWriteSamples);
    result.data["totalSamples"] = totalSamples;
    
    qDebug() << "Waveform output started successfully, wrote" << actualWriteSamples << "samples";
    
    return result;
}

void AODeviceThread::handleSyncTrigger()
{
    // 处理同步触发逻辑
    if (deviceHandle_ && !enabledChannels_.isEmpty()) {
        int32_t apiResult = JY5710_AO_SendSoftTrigger(deviceHandle_);
        if (apiResult == Success) {
            qDebug() << "AO soft trigger sent successfully";
        } else {
            qDebug() << "Failed to send AO soft trigger, error:" << apiResult;
        }
    } else {
        qDebug() << "AO device not ready or no channels enabled for sync trigger";
    }
}

// DAQDeviceThread 实现
DAQDeviceThread::DAQDeviceThread(const QString& deviceName, int slot, QObject* parent)
    : BaseDeviceThread(deviceName, parent)
    , deviceHandle_(nullptr)
    , slotNumber_(slot)
    , currentSampleRate_(1000.0)
    , acquisitionActive_(false)
    , currentMode_(AcquisitionMode::SINGLE_POINT)
    , samplesPerChannel_(1000)
    , enabledChannelCount_(0)
    , bufferWriteIndex_(0)
    , bufferOverrun_(false)
    , accumulatedSamples_(0)
{
    // 初始化数据获取定时器
    dataFetchTimer_ = new QTimer(this);
    connect(dataFetchTimer_, &QTimer::timeout, this, &DAQDeviceThread::onDataFetchTimer);
    dataFetchTimer_->setInterval(100); // 100ms间隔检查数据
}

DAQDeviceThread::~DAQDeviceThread()
{
    stopThread();
}

bool DAQDeviceThread::initializeDevice()
{
    int32_t result = JY5320_Open(slotNumber_, &deviceHandle_);
    if (result != 0) {
        qDebug() << "Failed to open" << deviceName_ << "device on slot" << slotNumber_ << ", error:" << result;
        return false;
    }
    
    // 设置设备属性
    JY5320_SetDeviceProperty(deviceHandle_, JY5320_Independent);
    
    qDebug() << deviceName_ << "device initialized successfully on slot" << slotNumber_;
    return true;
}

void DAQDeviceThread::shutdownDevice()
{
    if (deviceHandle_) {
        if (acquisitionActive_) {
            stopAcquisition();
        }
        
        if (dataFetchTimer_->isActive()) {
            dataFetchTimer_->stop();
        }
        
        JY5320_Close(deviceHandle_);
        deviceHandle_ = nullptr;
        qDebug() << deviceName_ << "device shutdown completed";
    }
}

DeviceResult DAQDeviceThread::executeOperation(const DeviceOperation& operation)
{
    DeviceResult result;
    
    if (!deviceHandle_ && operation.command != DeviceCommand::INITIALIZE) {
        result.error = "Device not initialized";
        return result;
    }
    
      switch (operation.command) {        
        case DeviceCommand::INITIALIZE:
            if (deviceHandle_) {
                result.success = true;
                qDebug() << "DAQ device" << deviceName_ << "already initialized";
            } else {
                result.success = initializeDevice();
                if (!result.success) {
                    result.error = "Failed to initialize DAQ device";
                }
            }
            break;
            
        case DeviceCommand::SHUTDOWN:
            shutdownDevice();
            result.success = true;
            break;
            
        case DeviceCommand::CONFIGURE_CHANNEL:
            // 根据采集模式配置通道
            {
                QString modeStr = operation.parameters.value("mode", "single").toString();
                if (modeStr == "single") {
                    currentMode_ = AcquisitionMode::SINGLE_POINT;
                } else if (modeStr == "multi" || modeStr == "finite") {
                    currentMode_ = AcquisitionMode::MULTI_POINT;
                } else if (modeStr == "continuous") {
                    currentMode_ = AcquisitionMode::CONTINUOUS;
                }
                
                QVector<int> channels;
                if (operation.channels.isEmpty()) {
                    if (operation.channel >= 0) {
                        channels.append(operation.channel);
                }
            } else {
                    channels = operation.channels;
                }
                
                qDebug() << deviceName_ << "CONFIGURE_CHANNEL:"
                         << "mode=" << modeStr
                         << "channels=" << channels
                         << "samplesPerChannel=" << operation.samplesPerChannel
                         << "sampleRate=" << operation.sampleRate;
                
                if (channels.isEmpty()) {
                    result.error = "No channels specified for configuration";
                    qDebug() << deviceName_ << result.error;
                    break;
                }
                
                result = configureChannels(channels, operation.inputRangeMin, operation.inputRangeMax);
                if (result.success) {
                    result = setSampleRate(operation.sampleRate);
                    if (result.success) {
                        result = setAcquisitionMode(currentMode_);
                        if (result.success) {
                            samplesPerChannel_ = operation.samplesPerChannel;
                            qDebug() << deviceName_ << "successfully configured for" << acquisitionModeToString(currentMode_)
                                   << "mode, channels:" << channels << ", sample rate:" << operation.sampleRate
                                   << ", samplesPerChannel:" << samplesPerChannel_;
                        }
                    }
                }
            }
            break;
            
        case DeviceCommand::START_MEASUREMENT:
            switch (currentMode_) {
                case AcquisitionMode::SINGLE_POINT:
                    // 单点模式不需要预先启动
                    result.success = true;
                    break;
                case AcquisitionMode::MULTI_POINT:
                    result = startMultiPointAcquisition(operation);
                    break;
                case AcquisitionMode::CONTINUOUS:
                    result = startContinuousAcquisition(operation);
                    break;
            }
            break;
            
        case DeviceCommand::STOP_MEASUREMENT:
            switch (currentMode_) {
                case AcquisitionMode::SINGLE_POINT:
                    result.success = true;
                    break;
                case AcquisitionMode::MULTI_POINT:
                    result = stopMultiPointAcquisition(operation);
                    break;
                case AcquisitionMode::CONTINUOUS:
                    result = stopContinuousAcquisition();
                    break;
            }
            break;        
            
        case DeviceCommand::READ_DATA:
            // 根据当前模式执行相应的读取操作
            switch (currentMode_) {
                case AcquisitionMode::SINGLE_POINT:
                    result = performSinglePointAcquisition(operation);
                    break;
                case AcquisitionMode::MULTI_POINT:
                    result = readMultiPointData(operation);
                    break;
                case AcquisitionMode::CONTINUOUS:
                    result = readContinuousData(operation);
                    break;
                default:
                    result.error = "Unknown acquisition mode";
                    break;
            }
            break;
            
        case DeviceCommand::SYNC_TRIGGER:
            // 发送软件触发
            if (acquisitionActive_) {
                int32_t apiResult = JY5320_AI_SendSoftTrigger(deviceHandle_, JY5320_StartTrigger);
                if (apiResult == 0) {
                    result.success = true;
                    qDebug() << "DAQ soft trigger sent successfully";
                } else {
                    result.error = QString("Failed to send DAQ soft trigger, error: %1").arg(apiResult);
                }
            } else {
                result.error = "DAQ measurement not active, cannot send trigger";
            }
            break;
            
        default:
            result.error = QString("Unsupported operation for %1 device").arg(deviceName_);
            break;
    }
    
    return result;
}

DeviceResult DAQDeviceThread::performSinglePointAcquisition(const DeviceOperation& operation)
{
    DeviceResult result;
    
    // 设置单点模式
    int32_t apiResult = JY5320_AI_SetMode(deviceHandle_, JY5320_AI_Single);
    if (apiResult != 0) {
        result.error = QString("Failed to set single point mode, error: %1").arg(apiResult);
        return result;
    }
    
    // 启动采集
    apiResult = JY5320_AI_Start(deviceHandle_);
    if (apiResult != 0) {
        result.error = QString("Failed to start single point acquisition, error: %1").arg(apiResult);
        return result;
    }
    
    // 读取数据
    if (operation.channel >= 0) {
        // 读取指定通道
                        double voltage = 0.0;
                        apiResult = JY5320_AI_ReadSinglePoint(deviceHandle_, &voltage, operation.channel);
        if (apiResult == 0) {
                            result.success = true;
                            result.value = voltage;
            result.data["channel"] = operation.channel;
            result.data["voltage"] = voltage;
                        } else {
            result.error = QString("Failed to read single point from channel %1, error: %2")
                                         .arg(operation.channel).arg(apiResult);
                        }
    } else {
        // 读取所有启用的通道
        QVariantMap channelData;
        bool allSuccess = true;
        
        for (int channel : enabledChannels_) {
            double voltage = 0.0;
            apiResult = JY5320_AI_ReadSinglePoint(deviceHandle_, &voltage, channel);
            if (apiResult == 0) {
                channelData[QString("ch%1").arg(channel)] = voltage;
                    } else {
                allSuccess = false;
                result.error += QString("Failed to read channel %1; ").arg(channel);
            }
        }
        
        if (allSuccess || !channelData.isEmpty()) {
            result.success = true;
            result.data = channelData;
        }
    }
    
    // 停止采集
    JY5320_AI_Stop(deviceHandle_);
    
    return result;
}

DeviceResult DAQDeviceThread::configureMultiPointAcquisition(const DeviceOperation& operation)
{
    DeviceResult result;
    result.command = operation.command;
    
    if (!deviceHandle_) {
        result.success = false;
        result.error = "Device not initialized";
        return result;
    }
    
    // 解析参数
    const QVariantMap& params = operation.parameters;
    QVector<int> channels = params["channels"].value<QVector<int>>();
    double sampleRate = params["sampleRate"].toDouble();
    int samplesPerChannel = params["samplesPerChannel"].toInt();
    double rangeMin = params.value("rangeMin", -10.0).toDouble();
    double rangeMax = params.value("rangeMax", 10.0).toDouble();
    
    if (channels.isEmpty()) {
        result.success = false;
        result.error = "No channels specified";
        return result;
    }
    
    qDebug() << deviceName_ << "Configuring multi-point acquisition (based on official example):"
             << "channels:" << channels
             << "sampleRate:" << sampleRate
             << "samplesPerChannel:" << samplesPerChannel
             << "range:" << rangeMin << "to" << rangeMax;
    
    // 停止当前任何正在进行的采集
    if (acquisitionActive_) {
        JY5320_AI_Stop(deviceHandle_);
        acquisitionActive_ = false;
    }
    
    // 准备通道配置数组
    enabledChannelCount_ = channels.size();
    std::vector<unsigned char> channelArray(enabledChannelCount_);
    std::vector<double> rangeLowArray(enabledChannelCount_);
    std::vector<double> rangeHighArray(enabledChannelCount_);
    std::vector<JY5320_AI_BandWidth> bandWidthArray(enabledChannelCount_);
    
    for (int i = 0; i < enabledChannelCount_; ++i) {
        channelArray[i] = static_cast<unsigned char>(channels[i]);
        rangeLowArray[i] = rangeMin;
        rangeHighArray[i] = rangeMax;
        bandWidthArray[i] = JY5320_AI_BandWidth_25K; // 默认带宽
    }
    
    // 1. 启用通道 - 按照官方示例
    int32_t apiResult = JY5320_AI_EnableChannel(deviceHandle_, enabledChannelCount_,
                                               channelArray.data(), rangeLowArray.data(),
                                               rangeHighArray.data(), bandWidthArray.data());
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to enable channels, error: %1").arg(apiResult);
        return result;
    }
    
    // 2. 设置模式为有限模式（多点采集）- 按照官方示例
    apiResult = JY5320_AI_SetMode(deviceHandle_, JY5320_AI_Finite);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to set finite mode, error: %1").arg(apiResult);
        return result;
    }
    
    // 3. 设置采样数量 - 按照官方示例
    apiResult = JY5320_AI_SetSamplesToAcquire(deviceHandle_, samplesPerChannel);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to set samples to acquire, error: %1").arg(apiResult);
        return result;
    }
    
    // 4. 设置采样率 - 按照官方示例
    double actualSampleRate = 0.0;
    apiResult = JY5320_AI_SetSampleRate(deviceHandle_, sampleRate, &actualSampleRate);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to set sample rate, error: %1").arg(apiResult);
        return result;
    }
    
    // 注意：按照官方示例，Finite模式下不设置触发类型，使用默认的立即触发
    // 这样Start后会自动开始采集，不需要发送软件触发
    
    // 保存配置参数
    currentParams_.sampleRate = actualSampleRate;
    currentParams_.samplesPerChannel = samplesPerChannel;
    currentParams_.channels = channels;
    currentParams_.rangeMin = rangeMin;
    currentParams_.rangeMax = rangeMax;
    currentMode_ = AcquisitionMode::MULTI_POINT;
    
    qDebug() << deviceName_ << "Multi-point acquisition configured successfully (official example style)."
             << "Actual sample rate:" << actualSampleRate;
    
    result.success = true;
    result.data["actualSampleRate"] = actualSampleRate;
    result.data["totalSamples"] = samplesPerChannel;
    result.data["channels"] = QVariant::fromValue(channels);
    
    return result;
}

DeviceResult DAQDeviceThread::startMultiPointAcquisition(const DeviceOperation& operation)
{
    DeviceResult result;
    result.command = operation.command;
    
    if (!deviceHandle_) {
        result.success = false;
        result.error = "Device not initialized";
        return result;
    }
    
    if (acquisitionActive_) {
        result.success = false;
        result.error = "Acquisition already active";
        return result;
    }
    
    qDebug() << deviceName_ << "Starting multi-point acquisition (official example style)...";
    
    // 按照官方示例，先分配数据缓冲区
    int totalSamples = currentParams_.samplesPerChannel * enabledChannelCount_;
    dataBuffer_.resize(totalSamples);
    
    // 1. 启动AI采集 - 按照官方示例，Finite模式下Start后自动开始采集
    int32_t apiResult = JY5320_AI_Start(deviceHandle_);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to start AI, error: %1").arg(apiResult);
        return result;
    }
    
    // 重置采集状态和数据缓存
    {
        QMutexLocker locker(&dataMutex_);
        dataReadyFlag_ = false;
        lastMultiPointData_.clear();
        tempMultiPointData_.clear();
    }
    
    acquisitionActive_ = true;
    currentMode_ = AcquisitionMode::MULTI_POINT;
    
    qDebug() << deviceName_ << "Multi-point acquisition started (no trigger needed in Finite mode)";
    
    // 启动数据读取定时器 - 确保在正确的线程中启动
    QMetaObject::invokeMethod(dataFetchTimer_, [this]() {
        dataFetchTimer_->start(50); // 50ms间隔，与官方示例一致
    }, Qt::QueuedConnection);
    
    result.success = true;
    result.data["mode"] = "multi-point";
    result.data["samplesPerChannel"] = currentParams_.samplesPerChannel;
    result.data["channels"] = QVariant::fromValue(currentParams_.channels);
    
    return result;
}

DeviceResult DAQDeviceThread::readMultiPointData(const DeviceOperation& operation)
{
    DeviceResult result;
    result.command = operation.command;
    
    if (!deviceHandle_) {
        result.success = false;
        result.error = "Device not initialized";
        return result;
    }
    
    if (currentMode_ != AcquisitionMode::MULTI_POINT) {
        result.success = false;
        result.error = "Not in multi-point acquisition mode";
        return result;
    }
    
    // 检查定时器是否已经读取了数据
    {
        QMutexLocker locker(&dataMutex_);
        if (dataReadyFlag_ && !lastMultiPointData_.isEmpty()) {
            // 验证数据有效性
            bool hasValidData = false;
            for (int ch = 0; ch < lastMultiPointData_.size(); ch++) {
                if (!lastMultiPointData_[ch].isEmpty()) {
                    hasValidData = true;
                    break;
                }
            }
            
            if (hasValidData) {
                // 数据已准备好且有效，返回数据
                result.success = true;
                result.data["channelData"] = QVariant::fromValue(lastMultiPointData_);  // 修复：使用channelData键名
                result.data["channels"] = QVariant::fromValue(lastMultiPointData_);     // 保持兼容性
                result.data["samplesPerChannel"] = lastMultiPointData_.isEmpty() ? 0 : lastMultiPointData_[0].size();
                result.data["channelCount"] = lastMultiPointData_.size();
                
                qDebug() << deviceName_ << "Returning cached multi-point data -" 
                         << "Channels:" << lastMultiPointData_.size()
                         << "Samples per channel:" << (lastMultiPointData_.isEmpty() ? 0 : lastMultiPointData_[0].size());
                
                return result;
            } else {
                qDebug() << deviceName_ << "数据标记为准备好但实际为空，重置标志继续等待";
                dataReadyFlag_ = false;  // 重置标志
            }
        }
    }
    
    // 数据还没准备好，检查采集状态
    if (!acquisitionActive_) {
        result.success = false;
        result.error = "Device not active or acquisition not started";
        return result;
    }
    
    // 采集还在进行中，检查当前缓冲区状态
    unsigned long long availableSamples = 0;
    unsigned long long transferredSamples = 0;
    bool overRun = false;
    
    result.success = false;
    
    return result;
}

DeviceResult DAQDeviceThread::stopMultiPointAcquisition(const DeviceOperation& operation)
{
    DeviceResult result;
    result.command = operation.command;
    
    if (!deviceHandle_) {
        result.success = false;
        result.error = "Device not initialized";
        return result;
    }
    
    qDebug() << deviceName_ << "Stopping multi-point acquisition...";
    
    // 停止数据读取定时器
    if (dataFetchTimer_->isActive()) {
        dataFetchTimer_->stop();
    }
    
    // 停止AI采集
    if (acquisitionActive_) {
        int32_t apiResult = JY5320_AI_Stop(deviceHandle_);
        if (apiResult != 0) {
            qDebug() << deviceName_ << "Warning: Failed to stop AI, error:" << apiResult;
            // 继续执行，不要因为停止失败而报错
        }
        acquisitionActive_ = false;
    }
    
    qDebug() << deviceName_ << "Multi-point acquisition stopped";
    
    result.success = true;
    result.data["stopped"] = true;
    
    return result;
}

DeviceResult DAQDeviceThread::startContinuousAcquisition(const DeviceOperation& operation)
{
    DeviceResult result;
    
    // 设置连续模式
    int32_t apiResult = JY5320_AI_SetMode(deviceHandle_, JY5320_AI_Continuous);
    if (apiResult != 0) {
        result.error = QString("Failed to set continuous mode, error: %1").arg(apiResult);
        return result;
    }
    
    // 设置触发
    setupTrigger();
    
    // 初始化连续采集缓冲区
    QMutexLocker locker(&bufferMutex_);
    continuousDataBuffer_.clear();
    continuousDataBuffer_.resize(enabledChannelCount_);
    for (int ch = 0; ch < enabledChannelCount_; ++ch) {
        continuousDataBuffer_[ch].reserve(currentParams_.bufferSize);
    }
    bufferWriteIndex_ = 0;
    bufferOverrun_ = false;
    
    // 启动采集
    apiResult = JY5320_AI_Start(deviceHandle_);
    if (apiResult == 0) {
        acquisitionActive_ = true;
        // 确保在正确的线程中启动定时器
        QMetaObject::invokeMethod(dataFetchTimer_, [this]() {
            dataFetchTimer_->start(1000); // 1000ms间隔
        }, Qt::QueuedConnection);
        result.success = true;
        qDebug() << deviceName_ << "continuous acquisition started";
    } else {
        result.error = QString("Failed to start continuous acquisition, error: %1").arg(apiResult);
    }
    
    return result;
}

DeviceResult DAQDeviceThread::readContinuousData(const DeviceOperation& operation)
{
    DeviceResult result;
    
    if (!acquisitionActive_) {
        result.error = "Continuous acquisition not active";
        return result;
    }
    
    QMutexLocker locker(&bufferMutex_);
    
    // 检查是否有数据可读
    bool hasData = false;
    for (int ch = 0; ch < enabledChannelCount_; ++ch) {
        if (!continuousDataBuffer_[ch].isEmpty()) {
            hasData = true;
            break;
        }
    }
    
    if (hasData) {
        // 返回当前缓冲区中的数据
        QVector<QVector<double>> channelData = continuousDataBuffer_;
        
        result.success = true;
        result.data["channelData"] = QVariant::fromValue(channelData);
        result.data["channels"] = QVariant::fromValue(enabledChannels_);
        result.data["bufferOverrun"] = bufferOverrun_;
        
        // 清空缓冲区或保留部分数据
        int keepSamples = operation.parameters.value("keepSamples", 0).toInt();
        if (keepSamples > 0) {
            for (int ch = 0; ch < enabledChannelCount_; ++ch) {
                if (continuousDataBuffer_[ch].size() > keepSamples) {
                    continuousDataBuffer_[ch] = continuousDataBuffer_[ch].mid(
                        continuousDataBuffer_[ch].size() - keepSamples);
                }
            }
        } else {
            // 清空所有缓冲区
            for (int ch = 0; ch < enabledChannelCount_; ++ch) {
                continuousDataBuffer_[ch].clear();
            }
        }
        
        bufferOverrun_ = false;
        
        emit continuousDataReady(deviceName_, channelData);
    } else {
        result.success = false;
        result.error = "No continuous data available";
    }
    
    return result;
}

DeviceResult DAQDeviceThread::stopContinuousAcquisition()
{
    DeviceResult result;
    
    if (acquisitionActive_) {
        dataFetchTimer_->stop();
        
        int32_t apiResult = JY5320_AI_Stop(deviceHandle_);
        if (apiResult == 0) {
            acquisitionActive_ = false;
            result.success = true;
            qDebug() << deviceName_ << "continuous acquisition stopped";
            emit acquisitionCompleted(deviceName_, AcquisitionMode::CONTINUOUS);
        } else {
            result.error = QString("Failed to stop continuous acquisition, error: %1").arg(apiResult);
        }
    } else {
        result.success = true; // 已经停止
    }
    
    return result;
}

DeviceResult DAQDeviceThread::configureChannels(const QVector<int>& channels, double rangeMin, double rangeMax)
{
    DeviceResult result;
    
    qDebug() << deviceName_ << "configureChannels called with channels:" << channels
             << "range:" << rangeMin << "to" << rangeMax;
    
    if (channels.isEmpty()) {
        result.error = "No channels specified";
        qDebug() << deviceName_ << result.error;
        return result;
    }
    
    // 准备通道配置数组
    std::vector<unsigned char> channelIds(channels.size());
    std::vector<double> lowRegion(channels.size(), rangeMin);
    std::vector<double> highRegion(channels.size(), rangeMax);
    std::vector<JY5320_AI_BandWidth> bandwidths(channels.size(), JY5320_AI_BandWidth_25K);
    
    for (int i = 0; i < channels.size(); ++i) {
        channelIds[i] = static_cast<unsigned char>(channels[i]);
        qDebug() << deviceName_ << "Channel" << i << ":" << channels[i] << "-> channelId" << channelIds[i];
    }
    
    // 启用通道
    qDebug() << deviceName_ << "Calling JY5320_AI_EnableChannel with" << channels.size() << "channels";
    int32_t apiResult = JY5320_AI_EnableChannel(deviceHandle_, channels.size(),
                                               channelIds.data(), lowRegion.data(), 
                                               highRegion.data(), bandwidths.data());
    
    qDebug() << deviceName_ << "JY5320_AI_EnableChannel result:" << apiResult;
    
    if (apiResult == 0) {
        // 更新内部状态
        channelEnabled_.clear();
        enabledChannels_ = channels;
        enabledChannelCount_ = channels.size();
        
        for (int channel : channels) {
            channelEnabled_[channel] = true;
        }
        
        result.success = true;
        qDebug() << deviceName_ << "configured channels successfully:"
                 << "enabledChannels_=" << enabledChannels_
                 << "enabledChannelCount_=" << enabledChannelCount_
                 << "range:" << rangeMin << "to" << rangeMax;
    } else {
        result.error = QString("Failed to configure channels, error: %1").arg(apiResult);
        qDebug() << deviceName_ << result.error;
    }
    
    return result;
}

DeviceResult DAQDeviceThread::setSampleRate(double sampleRate)
{
    DeviceResult result;
    
    double actualSampleRate = 0.0;
    int32_t apiResult = JY5320_AI_SetSampleRate(deviceHandle_, sampleRate, &actualSampleRate);
    
    if (apiResult == 0) {
        currentSampleRate_ = actualSampleRate;
        result.success = true;
        result.value = actualSampleRate;
        qDebug() << deviceName_ << "sample rate set to" << actualSampleRate << "Hz (requested:" << sampleRate << "Hz)";
        } else {
        result.error = QString("Failed to set sample rate, error: %1").arg(apiResult);
    }
    
    return result;
}

DeviceResult DAQDeviceThread::setAcquisitionMode(AcquisitionMode mode)
{
    DeviceResult result;
    
    JY5320_AI_SampleMode jyMode = convertToJY5320Mode(mode);
    int32_t apiResult = JY5320_AI_SetMode(deviceHandle_, jyMode);
    
    if (apiResult == 0) {
        currentMode_ = mode;
        result.success = true;
        qDebug() << deviceName_ << "acquisition mode set to" << acquisitionModeToString(mode);
    } else {
        result.error = QString("Failed to set acquisition mode, error: %1").arg(apiResult);
    }
    
    return result;
}

void DAQDeviceThread::onDataFetchTimer()
{
    if (!acquisitionActive_ || !deviceHandle_) {
        return;
    }
    
    if (currentMode_ == AcquisitionMode::CONTINUOUS) {
        processContinuousData();
    } else if (currentMode_ == AcquisitionMode::MULTI_POINT) {
        processMultiPointData();
    }
}

void DAQDeviceThread::processMultiPointData()
{
    if (!deviceHandle_ || !acquisitionActive_ || currentMode_ != AcquisitionMode::MULTI_POINT) {
        return;
    }
    
    // 先检查缓冲区状态，确保数据确实可用
    unsigned long long currentAvailable = 0;
    unsigned long long currentTransferred = 0;
    unsigned long long currentTransferred_1 = 0;
    unsigned int actualReadLength = 0;
    bool currentOverrun = false;
    
    unsigned long long totalSamplesToRead = currentParams_.samplesPerChannel * enabledChannelCount_;
    int samplesPerChannelToRead = currentParams_.samplesPerChannel;
    int32_t statusResult = JY5320_AI_CheckBufferStatus(deviceHandle_, &currentAvailable,
                                                      &currentTransferred, &currentOverrun);
    if (statusResult == 0) {
        qDebug() << deviceName_ << "读取前缓冲区状态 - Available:" << currentAvailable 
                 << "Transferred:" << currentTransferred << "Overrun:" << currentOverrun;
    }
    
    // 修复：正确计算要读取的每通道样本数
    // ReadLength应该是每通道的样本数，而不是总样本数
    if(currentAvailable < totalSamplesToRead - currentTransferred) return;

    int actualSamplesToRead = totalSamplesToRead - currentTransferred;
    int mod = actualSamplesToRead % enabledChannelCount_;
    if (mod != 0) {
        if(actualSamplesToRead - mod > 0)
        actualSamplesToRead -= mod; // 确保读取的样本数是每通道样本数的整数倍
    }
    
    qDebug() << deviceName_ << "修正后的读取参数 - 每通道可用样本:" << currentAvailable
             << "实际读样本:" << totalSamplesToRead;
    
    std::unique_ptr<double[]> pDataBuf = std::make_unique<double[]>(actualSamplesToRead * enabledChannelCount_);
    // 按照官方示例：JY5320_AI_ReadData的dataLength参数是每通道样本数
    statusResult = JY5320_AI_ReadData(deviceHandle_, pDataBuf.get(), 
                                   qMin(actualSamplesToRead, samplesPerChannelToRead),
                                   -1, &actualReadLength);                           
    if (statusResult != 0) {
        qDebug() << deviceName_ << "数据读取错误:" << statusResult;
        return;
    }

    statusResult = JY5320_AI_CheckBufferStatus(deviceHandle_, &currentAvailable,
                                                      &currentTransferred_1, &currentOverrun);
    actualReadLength = currentTransferred_1 - currentTransferred;
    for(int i = 0; i < actualReadLength; ++i) {
        tempMultiPointData_.append(pDataBuf[i]);
    }

    if(currentTransferred_1 < totalSamplesToRead) return;


    // 保存读取的数据
    {
        QMutexLocker locker(&dataMutex_);
        QVector<QVector<double>> DataPoints(enabledChannelCount_);
        for(int ch = 0; ch < enabledChannelCount_; ++ch) {
            for (int sample = 0; sample < samplesPerChannelToRead; ++sample) {
                int index = sample * enabledChannelCount_ + ch;
                DataPoints[ch].append(tempMultiPointData_[index]);
            }
        }
        lastMultiPointData_ = DataPoints;
        dataReadyFlag_ = true;
        qDebug() << deviceName_ << "数据已保存到lastMultiPointData_，通道数：" << lastMultiPointData_.size()
                 << "每通道样本数：" << (lastMultiPointData_.empty() ? 0 : lastMultiPointData_[0].size());
    }
    
    qDebug() << deviceName_ << "Data parsed and saved successfully";

    if (dataFetchTimer_->isActive()) {
        dataFetchTimer_->stop();
    }
    
    acquisitionActive_ = false;
}

void DAQDeviceThread::processContinuousData()
{
    // 检查是否有新数据可用
    unsigned long long availableSamples = 0;
    bool overrun = false;
    
    if (!checkBufferStatus(availableSamples, overrun)) {
        return;
    }
    
    if (overrun) {
        QMutexLocker locker(&bufferMutex_);
        bufferOverrun_ = true;
        qDebug() << deviceName_ << "data overrun detected in continuous mode";
    }
    
    // 读取可用数据
    if (availableSamples > 0) {
        // 确保不超过可用样本数，并限制单次读取量
        unsigned long long maxRead = qMin(availableSamples, static_cast<unsigned long long>(1000));
        int samplesToRead = static_cast<int>(maxRead); // 一次最多读1000个样本
        QVector<double> tempBuffer(samplesToRead * enabledChannelCount_);
        
        unsigned int actualReadLength = 0;
        int32_t apiResult = JY5320_AI_ReadData(deviceHandle_, tempBuffer.data(),
                                               samplesToRead, 100, &actualReadLength);
        
        if (apiResult == 0 && actualReadLength > 0) {
            // 将数据添加到连续缓冲区
            QMutexLocker locker(&bufferMutex_);
            
            for (int sample = 0; sample < actualReadLength; ++sample) {
                for (int ch = 0; ch < enabledChannelCount_; ++ch) {
                    double value = tempBuffer[sample * enabledChannelCount_ + ch];
                    
                    // 检查缓冲区大小
                    if (continuousDataBuffer_[ch].size() >= currentParams_.bufferSize) {
                        // 移除最旧的数据
                        continuousDataBuffer_[ch].removeFirst();
                        bufferOverrun_ = true;
                    }
                    
                    continuousDataBuffer_[ch].append(value);
                }
            }
            
            // 发送缓冲区状态更新
            QVariantMap bufferInfo;
            bufferInfo["availableSamples"] = static_cast<int>(availableSamples);
            bufferInfo["samplesRead"] = actualReadLength;
            bufferInfo["bufferOverrun"] = bufferOverrun_;
            bufferInfo["bufferFillLevel"] = continuousDataBuffer_.isEmpty() ? 0 : continuousDataBuffer_[0].size();
            
            emit dataBufferUpdated(deviceName_, bufferInfo);
        }
    }
}

bool DAQDeviceThread::checkBufferStatus(unsigned long long& availableSamples, bool& overRun)
{
    unsigned long long transferredSamples = 0;
    int32_t apiResult = JY5320_AI_CheckBufferStatus(deviceHandle_, &availableSamples, 
                                                   &transferredSamples, &overRun);
    return (apiResult == 0);
}

void DAQDeviceThread::setupTrigger()
{
    if (deviceHandle_) {
        // 设置立即触发模式
        JY5320_AI_SetStartTriggerType(deviceHandle_, JY5320_AI_Immediately);
    }
}

void DAQDeviceThread::startAcquisition()
{
    // 这个方法被具体的采集模式方法替代
}

void DAQDeviceThread::stopAcquisition()
{
    if (deviceHandle_ && acquisitionActive_) {
        dataFetchTimer_->stop();
        JY5320_AI_Stop(deviceHandle_);
            acquisitionActive_ = false;
    }
}

QString DAQDeviceThread::acquisitionModeToString(AcquisitionMode mode) const
{
    switch (mode) {
        case AcquisitionMode::SINGLE_POINT: return "Single Point";
        case AcquisitionMode::MULTI_POINT: return "Multi Point";
        case AcquisitionMode::CONTINUOUS: return "Continuous";
        default: return "Unknown";
    }
}

JY5320_AI_SampleMode DAQDeviceThread::convertToJY5320Mode(AcquisitionMode mode) const
{
    switch (mode) {
        case AcquisitionMode::SINGLE_POINT: return JY5320_AI_Single;
        case AcquisitionMode::MULTI_POINT: return JY5320_AI_Finite;
        case AcquisitionMode::CONTINUOUS: return JY5320_AI_Continuous;
        default: return JY5320_AI_Single;
    }
}

void DAQDeviceThread::handleSyncTrigger()
{
    // DAQ设备的同步触发处理
    if (deviceHandle_ && acquisitionActive_) {
        // 发送软件触发
        JY5320_AI_SendSoftTrigger(deviceHandle_, JY5320_StartTrigger);
        qDebug() << deviceName_ << "received sync trigger and sent soft trigger";
    }
}

// DMMDeviceThread 实现 - 仅支持连续电阻测量（参考DAQDeviceThread结构）
DMMDeviceThread::DMMDeviceThread(QObject* parent)
    : BaseDeviceThread("JY8902", parent)
    , deviceHandle_(nullptr)
    , measurementActive_(false)
    , currentMode_(ResistanceMeasurementMode::CONTINUOUS)
    , samplesPerTrigger_(20)
    , bufferWriteIndex_(0)
    , bufferOverrun_(false)
    , accumulatedSamples_(0)
{
    // 初始化数据获取定时器（参考DAQDeviceThread）
    dataFetchTimer_ = new QTimer(this);
    connect(dataFetchTimer_, &QTimer::timeout, this, &DMMDeviceThread::onDataFetchTimer);
    dataFetchTimer_->setInterval(50); // 50ms间隔检查数据，与官方示例一致
    
    // 初始化默认参数（参考DAQDeviceThread的AcquisitionParams）
    currentParams_.mode = ResistanceMeasurementMode::CONTINUOUS;
    currentParams_.range = JY8902_2_Wire_Resistance_Auto;
    currentParams_.samplesPerTrigger = 20;
    currentParams_.sampleInterval = 0.02;
    currentParams_.useNPLC = false;
    currentParams_.apertureTime = 0.02;
    currentParams_.nplcValue = 3;
    currentParams_.triggerDelay = 10;
    currentParams_.useBuffer = true;
    currentParams_.bufferSize = 1000;
    currentParams_.timeout = 10000;
}

DMMDeviceThread::~DMMDeviceThread()
{
    stopThread();
}

bool DMMDeviceThread::initializeDevice()
{
    qDebug() << "Attempting to initialize JY8902 DMM device for continuous resistance measurement...";
    
    int32_t result = JY8902_Open(0, &deviceHandle_);
    if (result != 0) {
        QString errorMsg;
        
        // Map common error codes to user-friendly messages
        switch (result) {
            case -10001: // Error_DMM_OpenDeviceFailed
                errorMsg = "Device not found or already in use";
                break;
            case -10004: // Error_DMM_InitalDeviceFailed  
                errorMsg = "Device initialization failed - hardware issue";
                break;
            case -10005: // Error_DMM_ActiveDeviceFailed
                errorMsg = "Device activation failed - driver issue";
                break;
            case -10006: // Error_DMM_DeviceSlotNumberInvalid
                errorMsg = "Invalid device slot number";
                break;
            case -10007: // Error_DMM_DeviceNumberInvalid
                errorMsg = "Invalid device number";
                break;
            case -10009: // Error_DMM_DeviceHandleInvalid
                errorMsg = "Device handle invalid";
                break;
            default:
                errorMsg = QString("Unknown error code: %1").arg(result);
                break;
        }
        
        qDebug() << "Failed to open JY8902 DMM device, error:" << result << "-" << errorMsg;
        qDebug() << "Check: 1) Device connected 2) Drivers installed 3) Device not in use 4) Proper slot number";
        return false;
    }
      qDebug() << "JY8902 device opened successfully, handle:" << deviceHandle_;
    
    return true;
}

void DMMDeviceThread::shutdownDevice()
{
    if (deviceHandle_) {
        if (measurementActive_) {
            stopMeasurement();
        }
        
        if (dataFetchTimer_->isActive()) {
            dataFetchTimer_->stop();
        }
        
        JY8902_Close(deviceHandle_);
        deviceHandle_ = nullptr;
        qDebug() << "JY8902 DMM device shutdown completed";
    }
}

DeviceResult DMMDeviceThread::executeOperation(const DeviceOperation& operation)
{
    DeviceResult result;
    
    if (!deviceHandle_) {
        result.error = "Device not initialized";
        return result;
    }
      switch (operation.command) {        
        case DeviceCommand::INITIALIZE:
            // Check if already initialized
            if (deviceHandle_) {
                result.success = true;
                qDebug() << "DMM device already initialized";
            } else {
                result.success = initializeDevice();
                if (!result.success) {
                    result.error = "Failed to initialize DMM device";
                }
            }
            return result;
            
        case DeviceCommand::SHUTDOWN:
            shutdownDevice();
            result.success = true;
            return result;
            
        case DeviceCommand::CONFIGURE_CHANNEL:
            // 配置DMM连续电阻测量（参考DAQDeviceThread的CONFIGURE_CHANNEL）
            result = configureContinuousResistanceMeasurement(operation);
            break;

        case DeviceCommand::START_MEASUREMENT:
            result = startContinuousResistanceMeasurement(operation);
            break;
            
        case DeviceCommand::STOP_MEASUREMENT:
            result = stopContinuousResistanceMeasurement(operation);
            break;
            
        case DeviceCommand::READ_DATA:
            // 根据当前模式执行电阻测量（参考DAQDeviceThread的READ_DATA）
            result = readContinuousResistanceData(operation);
            break;
            
        case DeviceCommand::SYNC_TRIGGER:
            // 发送软件触发（参考DAQDeviceThread的SYNC_TRIGGER）
            if (measurementActive_) {
                int32_t apiResult = JY8902_DMM_SendSoftTrigger(deviceHandle_);
                if (apiResult == 0) {
                    result.success = true;
                    qDebug() << "DMM continuous resistance measurement soft trigger sent successfully";
                } else {
                    result.error = QString("Failed to send DMM soft trigger, error: %1").arg(apiResult);
                }
            } else {
                result.error = "DMM continuous measurement not active";
            }
            break;
            
        default:
            result.error = "Unsupported operation for DMM device";
            break;
    }
    
    return result;
}

void DMMDeviceThread::handleSyncTrigger()
{    // DMM设备的同步触发处理（参考DAQDeviceThread）
    if (deviceHandle_ && measurementActive_) {
        // 发送软件触发开始电阻测量
        JY8902_DMM_SendSoftTrigger(deviceHandle_);
        qDebug() << "DMM received sync trigger and sent soft trigger for continuous resistance measurement";
    }
}

DeviceResult DMMDeviceThread::configureContinuousResistanceMeasurement(const DeviceOperation& operation)
{
    DeviceResult result;
    result.command = operation.command;
    
    if (!deviceHandle_) {
        result.success = false;
        result.error = "Device not initialized";
        return result;
    }
    
    // 解析参数（参考DAQDeviceThread的configureMultiPointAcquisition）
    const QVariantMap& params = operation.parameters;
    QString rangeStr = params.value("range", "auto").toString();
    JY8902_DMM_2_Wire_ResistanceRange range = parseResistanceRange(rangeStr);
    int samplesPerTrigger = params.value("samplesPerTrigger", 20).toInt();
    double sampleInterval = params.value("sampleInterval", 0.02).toDouble();
    bool useNPLC = params.value("useNPLC", false).toBool();
    int nplcValue = params.value("nplcValue", 3).toInt();
    double apertureTime = params.value("apertureTime", 0.02).toDouble();
    int triggerDelay = params.value("triggerDelay", 10).toInt();
    int bufferSize = params.value("bufferSize", 1000).toInt();
    
    // 根据官方示例重新计算采样间隔
    if (useNPLC) {
        sampleInterval = nplcValue * 0.02;
        qDebug() << deviceName_ << "Using NPLC mode, recalculated sampleInterval:" << sampleInterval << "= nplcValue(" << nplcValue << ") * 0.02";
    }
    
    qDebug() << deviceName_ << "Configuring continuous resistance measurement (based on official example):"
             << "range:" << static_cast<int>(range)
             << "samplesPerTrigger:" << samplesPerTrigger
             << "sampleInterval:" << sampleInterval
             << "useNPLC:" << useNPLC
             << "nplcValue:" << nplcValue;
    
    // 停止当前任何正在进行的测量
    if (measurementActive_) {
        stopMeasurement();
    }
    
    // 配置参数（按照官方示例）
    int32_t apiResult = 0;
    
    // 1. 设置采样模式为连续多点模式（按照官方示例）
    apiResult = JY8902_DMM_SetSampleMode(deviceHandle_, JY8902_ContinuousMultiPoint);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to set continuous multi-point mode, error: %1").arg(apiResult);
        return result;
    }
    
    // 2. 设置电源线频率
    apiResult = JY8902_DMM_PowerLineFrequency(deviceHandle_, JY8902_50_Hz);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to set power line frequency, error: %1").arg(apiResult);
        return result;
    }
    
    // 3. 设置测量功能为2线电阻测量
    apiResult = JY8902_DMM_SetMeasurementFunction(deviceHandle_, JY8902_2_Wire_Resistance);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to set 2-wire resistance measurement function, error: %1").arg(apiResult);
        return result;
    }
    
    // 4. 设置电阻量程
    apiResult = JY8902_DMM_Set2WireResistance(deviceHandle_, range);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to set 2-wire resistance range, error: %1").arg(apiResult);
        return result;
    }
    
    // 5. 设置孔径单位和值（按照官方示例）
    if (useNPLC) {
        apiResult = JY8902_DMM_SetApertureUnit(deviceHandle_, JY8902_NPLC);
        if (apiResult != 0) {
            result.success = false;
            result.error = QString("Failed to set aperture unit to NPLC, error: %1").arg(apiResult);
            return result;
        }
        
        apiResult = JY8902_DMM_SetNPLC(deviceHandle_, nplcValue);
        if (apiResult != 0) {
            result.success = false;
            result.error = QString("Failed to set NPLC to %1, error: %2").arg(nplcValue).arg(apiResult);
            return result;
        }
    } else {
        apiResult = JY8902_DMM_SetApertureUnit(deviceHandle_, JY8902_Second);
        if (apiResult != 0) {
            result.success = false;
            result.error = QString("Failed to set aperture unit to Second, error: %1").arg(apiResult);
            return result;
        }
        
        apiResult = JY8902_DMM_SetApertureTime(deviceHandle_, apertureTime);
        if (apiResult != 0) {
            result.success = false;
            result.error = QString("Failed to set aperture time to %1, error: %2").arg(apertureTime).arg(apiResult);
            return result;
        }
    }
    
    // 6. 设置多点采样参数（按照官方示例）
    apiResult = JY8902_DMM_SetMultiSample(deviceHandle_, samplesPerTrigger, 
                                         JY8902_Sample_Immediately, sampleInterval);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to set multi-sample parameters, error: %1").arg(apiResult);
        return result;
    }
    
    // 7. 禁用校准（按照官方示例）
    apiResult = JY8902_DMM_DisableCalibration(deviceHandle_, true);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to disable calibration, error: %1").arg(apiResult);
        return result;
    }
    
    // 8. 设置触发延迟
    apiResult = JY8902_DMM_SetTriggerDelay(deviceHandle_, triggerDelay);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to set trigger delay, error: %1").arg(apiResult);
        return result;
    }
    
    // 9. 设置软件触发类型（连续测量使用软件触发）
    apiResult = JY8902_DMM_SetTriggerType(deviceHandle_, JY8902_Soft);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to set software trigger type, error: %1").arg(apiResult);
        return result;
    }
    
    // 保存配置参数
    currentParams_.range = range;
    currentParams_.samplesPerTrigger = samplesPerTrigger;
    currentParams_.sampleInterval = sampleInterval;
    currentParams_.useNPLC = useNPLC;
    currentParams_.nplcValue = nplcValue;
    currentParams_.apertureTime = apertureTime;
    currentParams_.triggerDelay = triggerDelay;
    currentParams_.bufferSize = bufferSize;
    currentMode_ = ResistanceMeasurementMode::CONTINUOUS;
    samplesPerTrigger_ = samplesPerTrigger;
    
    // 初始化数据缓冲区（参考DAQDeviceThread）
    {
        QMutexLocker locker(&bufferMutex_);
        continuousDataBuffer_.clear();
        continuousDataBuffer_.reserve(bufferSize);
        bufferOverrun_ = false;
        accumulatedSamples_ = 0;
        bufferWriteIndex_ = 0;
    }
    
    qDebug() << deviceName_ << "Continuous resistance measurement configured successfully (official example style)."
             << "Range:" << static_cast<int>(range) << "SamplesPerTrigger:" << samplesPerTrigger;
    
    result.success = true;
    result.data["range"] = static_cast<int>(range);
    result.data["samplesPerTrigger"] = samplesPerTrigger;
    result.data["sampleInterval"] = sampleInterval;
    
    return result;
}

DeviceResult DMMDeviceThread::startContinuousResistanceMeasurement(const DeviceOperation& operation)
{
    DeviceResult result;
    result.command = operation.command;
    
    if (!deviceHandle_) {
        result.success = false;
        result.error = "Device not initialized";
        return result;
    }
    
    if (measurementActive_) {
        result.success = false;
        result.error = "Measurement already active";
        return result;
    }
    
    qDebug() << deviceName_ << "Starting continuous resistance measurement (official example style)...";
    
    // 按照官方示例，先分配数据缓冲区
    int totalSamples = currentParams_.samplesPerTrigger;
    dataBuffer_.resize(totalSamples);
    
    // 1. 启动DMM测量 - 按照官方示例，连续模式下Start后等待软件触发
    int32_t apiResult = JY8902_DMM_Start(deviceHandle_);
    if (apiResult != 0) {
        result.success = false;
        result.error = QString("Failed to start DMM, error: %1").arg(apiResult);
        return result;
    }
    
    // 重置测量状态和数据缓存
    {
        QMutexLocker locker(&dataMutex_);
        dataReadyFlag_ = false;
        lastTriggerData_.clear();
        tempTriggerData_.clear();
    }
    
    measurementActive_ = true;
    currentMode_ = ResistanceMeasurementMode::CONTINUOUS;
    
    qDebug() << deviceName_ << "Continuous resistance measurement started (waiting for software trigger)";
    
    // 启动数据读取定时器 - 确保在正确的线程中启动
    QMetaObject::invokeMethod(dataFetchTimer_, [this]() {
        dataFetchTimer_->start(50); // 50ms间隔，与官方示例一致
    }, Qt::QueuedConnection);
    
    result.success = true;
    result.data["mode"] = "continuous";
    result.data["samplesPerTrigger"] = currentParams_.samplesPerTrigger;
    
    return result;
}

DeviceResult DMMDeviceThread::readContinuousResistanceData(const DeviceOperation& operation)
{
    DeviceResult result;
    result.command = operation.command;
    
    if (!deviceHandle_) {
        result.success = false;
        result.error = "Device not initialized";
        return result;
    }
    
    if (currentMode_ != ResistanceMeasurementMode::CONTINUOUS) {
        result.success = false;
        result.error = "Not in continuous resistance measurement mode";
        return result;
    }
    
    // 检查定时器是否已经读取了数据（参考DAQDeviceThread的readMultiPointData）
    {
        QMutexLocker locker(&dataMutex_);
        if (dataReadyFlag_ && !lastTriggerData_.isEmpty()) {
            // 验证数据有效性
            bool hasValidData = !lastTriggerData_.isEmpty();
            
            if (hasValidData) {
                // 数据已准备好且有效，返回数据
                result.success = true;
                result.data["resistanceData"] = QVariant::fromValue(lastTriggerData_);
                result.data["samplesPerTrigger"] = lastTriggerData_.size();
                result.data["unit"] = "Ω";
                result.data["range"] = static_cast<int>(currentParams_.range);
                
                // 计算统计信息
                if (!lastTriggerData_.isEmpty()) {
                    double sum = 0.0;
                    double minVal = lastTriggerData_[0];
                    double maxVal = lastTriggerData_[0];
                    
                    for (double val : lastTriggerData_) {
                        sum += val;
                        minVal = qMin(minVal, val);
                        maxVal = qMax(maxVal, val);
                    }
                    
                    double avgVal = sum / lastTriggerData_.size();
                    result.value = avgVal;  // 返回平均值作为主要结果
                    result.data["average"] = avgVal;
                    result.data["minimum"] = minVal;
                    result.data["maximum"] = maxVal;
                }
                
                qDebug() << deviceName_ << "Returning cached continuous resistance data -" 
                         << "Samples:" << lastTriggerData_.size();
                
                return result;
            } else {
                qDebug() << deviceName_ << "数据标记为准备好但实际为空，重置标志继续等待";
                dataReadyFlag_ = false;  // 重置标志
            }
        }
    }
    
    // 数据还没准备好，检查测量状态
    if (!measurementActive_) {
        result.success = false;
        result.error = "Device not active or measurement not started";
        return result;
    }
    
    // 测量还在进行中，返回当前状态信息
    result.success = false;
    result.error = QString("Data not ready, measurement in progress");
    
    return result;
}

DeviceResult DMMDeviceThread::stopContinuousResistanceMeasurement(const DeviceOperation& operation)
{
    DeviceResult result;
    result.command = operation.command;
    
    if (!deviceHandle_) {
        result.success = false;
        result.error = "Device not initialized";
        return result;
    }
    
    qDebug() << deviceName_ << "Stopping continuous resistance measurement...";
    
    // 停止数据读取定时器
    if (dataFetchTimer_->isActive()) {
        dataFetchTimer_->stop();
    }
    
    // 停止DMM测量
    if (measurementActive_) {
        int32_t apiResult = JY8902_DMM_Stop(deviceHandle_);
        if (apiResult != 0) {
            qDebug() << deviceName_ << "Warning: Failed to stop DMM, error:" << apiResult;
            // 继续执行，不要因为停止失败而报错
        }
        measurementActive_ = false;
    }
    
    qDebug() << deviceName_ << "Continuous resistance measurement stopped";
    
    result.success = true;
    result.data["stopped"] = true;
    
    emit measurementCompleted(deviceName_, currentMode_);
    
    return result;
}

void DMMDeviceThread::onDataFetchTimer()
{
    if (!measurementActive_ || !deviceHandle_) {
        return;
    }
    
    if (currentMode_ == ResistanceMeasurementMode::CONTINUOUS) {
        processContinuousResistanceData();
    }
}

void DMMDeviceThread::processContinuousResistanceData()
{
    if (!deviceHandle_ || !measurementActive_ || currentMode_ != ResistanceMeasurementMode::CONTINUOUS) {
        return;
    }
    
    // 先检查缓冲区状态，确保数据确实可用（参考DAQDeviceThread的processMultiPointData）
    unsigned long long currentAvailable = 0;
    unsigned long long currentTransferred = 0;
    bool currentOverrun = false;
    
    unsigned long long totalSamplesToRead = currentParams_.samplesPerTrigger;
    int32_t statusResult = JY8902_DMM_CheckBufferStatus(deviceHandle_, &currentAvailable,
                                                      &currentTransferred, &currentOverrun);
    if (statusResult != 0) {
        return;
    }
    
    // 检查是否有足够的数据可读取
    if(currentAvailable < totalSamplesToRead) {
        return;
    }

    qDebug() << deviceName_ << "读取前缓冲区状态 - Available:" << currentAvailable 
             << "Transferred:" << currentTransferred << "Overrun:" << currentOverrun;
    
    // 读取数据（完全按照官方示例方式）
    // 官方示例：pDataBuf = new double[20]; 然后 JY8902_DMM_ReadMultiPoint(hDevice, pDataBuf, samplesToAcq, -1, &actualSample)
    double* pDataBuf = new double[currentParams_.samplesPerTrigger];
    int actualSample = 0;
    
    statusResult = JY8902_DMM_ReadMultiPoint(deviceHandle_, pDataBuf, 
                                           currentParams_.samplesPerTrigger, -1, &actualSample);                           
    if (statusResult != 0) {
        qDebug() << deviceName_ << "数据读取错误:" << statusResult;
        delete[] pDataBuf;  // 释放内存
        return;
    }

    if (actualSample <= 0) {
        qDebug() << deviceName_ << "无效的样本数:" << actualSample << "期望最大:" << currentParams_.samplesPerTrigger;
        delete[] pDataBuf;  // 释放内存
        return;
    }

    // 保存读取的数据（参考DAQDeviceThread）
    {
        QMutexLocker locker(&dataMutex_);
        QVector<double> resistanceData;
        
        // 调试：输出前几个原始数据值
        qDebug() << deviceName_ << "原始数据调试 - actualSample:" << actualSample;
        qDebug() << deviceName_ << "完整原始数据输出：";
        for (int i = 0; i < actualSample; ++i) {
            qDebug() << deviceName_ << QString("原始数据[%1]: %2").arg(i).arg(pDataBuf[i], 0, 'e', 6);
        }
        
        // 按照官方示例，单通道数据直接使用索引 i（没有通道交替）
        for (int i = 0; i < actualSample; ++i) {
            double value = pDataBuf[i];
            
            // 数据合理性检查，过滤异常值
            if (!std::isnan(value) && !std::isinf(value) && qAbs(value) < 1e15) {
                resistanceData.append(value);
            } else {
                qDebug() << deviceName_ << "过滤异常数据点[" << i << "]:" << value;
            }
        }
        
        // 只有当有有效数据时才保存
        if (!resistanceData.isEmpty()) {
            lastTriggerData_ = resistanceData;
            dataReadyFlag_ = true;
            qDebug() << deviceName_ << "数据已保存到lastTriggerData_，样本数：" << lastTriggerData_.size();
        } else {
            qDebug() << deviceName_ << "所有数据点都是异常值，丢弃此次读取";
        }
    }
    
    // 释放缓冲区内存（按照官方示例方式）
    delete[] pDataBuf;
    
    qDebug() << deviceName_ << "Resistance data parsed and saved successfully";

    // 发送数据就绪信号
    QVector<double> dataToEmit = lastTriggerData_;
    emit continuousResistanceDataReady(deviceName_, dataToEmit);
    
    // 暂时停止定时器，等待下次触发
    if (dataFetchTimer_->isActive()) {
        dataFetchTimer_->stop();
    }
}

bool DMMDeviceThread::checkBufferStatus(unsigned long long& availableSamples, bool& overRun)
{
    unsigned long long transferredSamples = 0;
    int32_t apiResult = JY8902_DMM_CheckBufferStatus(deviceHandle_, &availableSamples, 
                                                    &transferredSamples, &overRun);
    return (apiResult == 0);
}

void DMMDeviceThread::setupTrigger()
{
    if (deviceHandle_) {
        // 设置软件触发模式
        JY8902_DMM_SetTriggerType(deviceHandle_, JY8902_Soft);
    }
}

void DMMDeviceThread::startMeasurement()
{
    // 这个方法被具体的测量模式方法替代
}

void DMMDeviceThread::stopMeasurement()
{
    if (deviceHandle_ && measurementActive_) {
        dataFetchTimer_->stop();
        JY8902_DMM_Stop(deviceHandle_);
        measurementActive_ = false;
    }
}

DeviceResult DMMDeviceThread::configureResistanceMeasurement(const ResistanceMeasurementParams& params)
{
    DeviceResult result;
    
    // 这个方法可以用于未来扩展其他配置
    currentParams_ = params;
    result.success = true;
    
    return result;
}

DeviceResult DMMDeviceThread::setResistanceRange(JY8902_DMM_2_Wire_ResistanceRange range)
{
    DeviceResult result;
    
    if (deviceHandle_) {
        int32_t apiResult = JY8902_DMM_Set2WireResistance(deviceHandle_, range);
        if (apiResult == 0) {
            currentParams_.range = range;
            result.success = true;
        } else {
            result.error = QString("Failed to set resistance range, error: %1").arg(apiResult);
        }
    } else {
        result.error = "Device not initialized";
    }
    
    return result;
}

DeviceResult DMMDeviceThread::setMeasurementMode(ResistanceMeasurementMode mode)
{
    DeviceResult result;
    
    // 目前只支持连续模式
    if (mode == ResistanceMeasurementMode::CONTINUOUS) {
        currentMode_ = mode;
        result.success = true;
    } else {
        result.error = "Unsupported measurement mode";
    }
    
    return result;
}

QString DMMDeviceThread::resistanceModeToString(ResistanceMeasurementMode mode) const
{
    switch (mode) {
        case ResistanceMeasurementMode::CONTINUOUS: return "Continuous";
        default: return "Unknown";
    }
}

JY8902_DMM_2_Wire_ResistanceRange DMMDeviceThread::parseResistanceRange(const QString& rangeStr) const
{
    QString range = rangeStr.toLower();
    
    if (range == "auto") return JY8902_2_Wire_Resistance_Auto;
    if (range == "100" || range == "100r") return JY8902_2_Wire_Resistance_100;
    if (range == "1k" || range == "1000") return JY8902_2_Wire_Resistance_1K;
    if (range == "10k" || range == "10000") return JY8902_2_Wire_Resistance_10K;
    if (range == "100k" || range == "100000") return JY8902_2_Wire_Resistance_100K;
    if (range == "1m" || range == "1000000") return JY8902_2_Wire_Resistance_1M;
    if (range == "10m" || range == "10000000") return JY8902_2_Wire_Resistance_10M;
    if (range == "100m" || range == "100000000") return JY8902_2_Wire_Resistance_100M;
    
    return JY8902_2_Wire_Resistance_Auto;  // 默认自动量程
}
