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
{
}

DAQDeviceThread::~DAQDeviceThread()
{
    stopThread();
}

bool DAQDeviceThread::initializeDevice()
{
    int32_t result = JY5320_Open(slotNumber_, &deviceHandle_);
    if (result != Success) {
        qDebug() << "Failed to open" << deviceName_ << "device, error:" << result;
        return false;
    }
    
    qDebug() << deviceName_ << "device initialized successfully";
    return true;
}

void DAQDeviceThread::shutdownDevice()
{
    if (deviceHandle_) {
        if (acquisitionActive_) {
            JY5320_AI_Stop(deviceHandle_);
            acquisitionActive_ = false;
        }
        JY5320_Close(deviceHandle_);
        deviceHandle_ = nullptr;
    }
}

DeviceResult DAQDeviceThread::executeOperation(const DeviceOperation& operation)
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
            if (deviceHandle_) {
                result.success = true;
                qDebug() << "DAQ device" << deviceName_ << "already initialized";
            } else {
                result.success = initializeDevice();
                if (!result.success) {
                    result.error = "Failed to initialize DAQ device";
                }
            }
            return result;
            
        case DeviceCommand::SHUTDOWN:
            shutdownDevice();
            result.success = true;
            return result;
            
        case DeviceCommand::CONFIGURE_CHANNEL:
            // 配置AI通道
            apiResult = JY5320_AI_SetSampleRate(deviceHandle_, operation.sampleRate, nullptr);
            if (apiResult == Success) {
                apiResult = JY5320_AI_SetMode(deviceHandle_, JY5320_AI_Single);
                if (apiResult == Success) {                    
                    unsigned char channels[] = {(unsigned char)operation.channel};
                    double lowRegion[] = {-10.0};
                    double highRegion[] = {10.0};
                    JY5320_AI_BandWidth bandwidth[] = {JY5320_AI_BandWidth_25K};
                    apiResult = JY5320_AI_EnableChannel(deviceHandle_, 1, channels, lowRegion, highRegion, bandwidth);
                    if (apiResult == Success) {
                        channelEnabled_[operation.channel] = true;
                        currentSampleRate_ = operation.sampleRate;
                        result.success = true;
                        qDebug() << deviceName_ << "channel" << operation.channel << "configured";
                    } else {
                        result.error = QString("Failed to enable AI channel %1, error: %2")
                                     .arg(operation.channel).arg(apiResult);
                    }
                } else {
                    result.error = QString("Failed to set AI mode, error: %1").arg(apiResult);
                }
            } else {
                result.error = QString("Failed to set AI sample rate, error: %1").arg(apiResult);
            }
            break;
            
        case DeviceCommand::START_MEASUREMENT:
            if (!acquisitionActive_) {
                setupTrigger();
                startAcquisition();
            }
            result.success = acquisitionActive_;
            break;
            
        case DeviceCommand::STOP_MEASUREMENT:
            if (acquisitionActive_) {
                stopAcquisition();
            }
            result.success = !acquisitionActive_;
            break;        
        case DeviceCommand::READ_DATA:
            // 读取数据 - 需要先启动采集
            {
                // 确保通道已配置
                if (!channelEnabled_[operation.channel]) {
                    // 如果通道未配置，使用默认配置
                    unsigned char channels[] = {(unsigned char)operation.channel};
                    double lowRegion[] = {-10.0};
                    double highRegion[] = {10.0};
                    JY5320_AI_BandWidth bandwidth[] = {JY5320_AI_BandWidth_25K};
                    
                    apiResult = JY5320_AI_SetMode(deviceHandle_, JY5320_AI_Single);
                    if (apiResult == Success) {
                        apiResult = JY5320_AI_EnableChannel(deviceHandle_, 1, channels, lowRegion, highRegion, bandwidth);
                        if (apiResult == Success) {
                            channelEnabled_[operation.channel] = true;
                        }
                    }
                }
                
                if (apiResult == Success || channelEnabled_[operation.channel]) {
                    // 启动AI采集
                    apiResult = JY5320_AI_Start(deviceHandle_);
                    if (apiResult == Success) {
                        // 读取单点数据
                        double voltage = 0.0;
                        apiResult = JY5320_AI_ReadSinglePoint(deviceHandle_, &voltage, operation.channel);
                        if (apiResult == Success) {
                            result.success = true;
                            result.value = voltage;
                            qDebug() << deviceName_ << "read voltage" << voltage << "V from channel" << operation.channel;
                        } else {
                            result.error = QString("Failed to read AI data from channel %1, error: %2")
                                         .arg(operation.channel).arg(apiResult);
                        }
                        
                        // 停止AI采集
                        JY5320_AI_Stop(deviceHandle_);
                    } else {
                        result.error = QString("Failed to start AI acquisition for %1, error: %2")
                                     .arg(deviceName_).arg(apiResult);
                    }
                } else {
                    result.error = QString("Failed to configure channel %1 for %2")
                                 .arg(operation.channel).arg(deviceName_);
                }
            }
            break;
            
        default:
            result.error = QString("Unsupported operation for %1 device").arg(deviceName_);
            break;
    }
    
    return result;
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

void DAQDeviceThread::setupTrigger()
{
    if (deviceHandle_) {
        // 设置触发模式
        JY5320_AI_SetStartTriggerType(deviceHandle_, JY5320_AI_Soft);
        JY5320_AI_SetDigitalStartTrigger(deviceHandle_, JY5320_PFI0, JY5320_Rising);
    }
}

void DAQDeviceThread::startAcquisition()
{
    if (deviceHandle_) {
        int32_t result = JY5320_AI_Start(deviceHandle_);
        if (result == Success) {
            acquisitionActive_ = true;
            qDebug() << deviceName_ << "acquisition started";
        } else {
            qDebug() << deviceName_ << "failed to start acquisition, error:" << result;
        }
    }
}

void DAQDeviceThread::stopAcquisition()
{
    if (deviceHandle_) {
        int32_t result = JY5320_AI_Stop(deviceHandle_);
        if (result == Success) {
            acquisitionActive_ = false;
            qDebug() << deviceName_ << "acquisition stopped";
        } else {
            qDebug() << deviceName_ << "failed to stop acquisition, error:" << result;
        }
    }
}

// DMMDeviceThread 实现
DMMDeviceThread::DMMDeviceThread(QObject* parent)
    : BaseDeviceThread("JY8902", parent)
    , deviceHandle_(nullptr)
    , currentFunction_(JY8902_DC_Volts)
    , measurementActive_(false)
{
}

DMMDeviceThread::~DMMDeviceThread()
{
    stopThread();
}

bool DMMDeviceThread::initializeDevice()
{
    qDebug() << "Attempting to initialize JY8902 DMM device...";
    
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
    
    // 基本配置（参考官方示例的简化流程）
    if (!configureBasicSettings()) {
        qDebug() << "Failed to configure basic settings";
        JY8902_Close(deviceHandle_);
        deviceHandle_ = nullptr;
        return false;
    }
    
    qDebug() << "JY8902 DMM device initialized successfully";
    return true;
}

void DMMDeviceThread::shutdownDevice()
{
    if (deviceHandle_) {
        if (measurementActive_) {
            JY8902_DMM_Stop(deviceHandle_);
            measurementActive_ = false;
        }
        JY8902_Close(deviceHandle_);
        deviceHandle_ = nullptr;
    }
}

DeviceResult DMMDeviceThread::executeOperation(const DeviceOperation& operation)
{
    DeviceResult result;
    
    if (!deviceHandle_) {
        result.error = "Device not initialized";
        return result;
    }
      switch (operation.command) {        case DeviceCommand::INITIALIZE:
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
            // 配置DMM测量功能
            {
                JY8902_DMM_MeasurementFunction function = 
                    static_cast<JY8902_DMM_MeasurementFunction>(operation.parameters.value("function").toInt());
                configureMeasurement(function);
                result.success = true;
                qDebug() << "DMM configured for function" << function;
            }
            break;        case DeviceCommand::START_MEASUREMENT:
            if (!measurementActive_) {
                int32_t apiResult = JY8902_DMM_Start(deviceHandle_);
                if (apiResult == 0) {
                    measurementActive_ = true;
                    result.success = true;
                    qDebug() << "DMM measurement started";
                } else {
                    result.error = QString("Failed to start DMM measurement, error: %1").arg(apiResult);
                }
            }
            break;        case DeviceCommand::STOP_MEASUREMENT:
            if (measurementActive_) {
                int32_t apiResult = JY8902_DMM_Stop(deviceHandle_);
                if (apiResult == 0) {
                    measurementActive_ = false;
                    result.success = true;
                    qDebug() << "DMM measurement stopped";
                } else {
                    result.error = QString("Failed to stop DMM measurement, error: %1").arg(apiResult);
                }
            }
            break;
            
        case DeviceCommand::READ_DATA:
            result = performMeasurement();
            break;
            
        default:
            result.error = "Unsupported operation for DMM device";
            break;
    }
    
    return result;
}

void DMMDeviceThread::handleSyncTrigger()
{    // DMM设备的同步触发处理
    if (deviceHandle_ && measurementActive_) {
        // 发送软件触发开始测量
        JY8902_DMM_SendSoftTrigger(deviceHandle_);
        qDebug() << "DMM received sync trigger and sent soft trigger";
    }
}

void DMMDeviceThread::configureMeasurement(JY8902_DMM_MeasurementFunction function)
{
    if (!deviceHandle_) {
        qDebug() << "Cannot configure measurement - device handle is null";
        return;
    }
    
    qDebug() << "Configuring DMM measurement function:" << function;
    currentFunction_ = function;
    
    int32_t result = 0;
    
    // 完整的DMM配置序列 - with error checking
    result = JY8902_DMM_SetSampleMode(deviceHandle_, JY8902_SingleSample);
    if (result != 0) {
        qDebug() << "Failed to set sample mode, error:" << result;
        return;
    }
    
    result = JY8902_DMM_PowerLineFrequency(deviceHandle_, JY8902_50_Hz);
    if (result != 0) {
        qDebug() << "Failed to set power line frequency, error:" << result;
        return;
    }
    
    result = JY8902_DMM_SetMeasurementFunction(deviceHandle_, function);
    if (result != 0) {
        qDebug() << "Failed to set measurement function, error:" << result;
        return;
    }
    
    // Configure based on function type with optimized settings
    if (function == JY8902_2_Wire_Resistance) {
        result = JY8902_DMM_Set2WireResistance(deviceHandle_, JY8902_2_Wire_Resistance_Auto);
        if (result != 0) {
            qDebug() << "Failed to set 2-wire resistance range, error:" << result;
        }
    } else if (function == JY8902_DC_Volts) {
        result = JY8902_DMM_SetDCVolt(deviceHandle_, JY8902_DC_Volt_Auto);
        if (result != 0) {
            qDebug() << "Failed to set DC voltage range, error:" << result;
        }
    } else if (function == JY8902_DC_Current) {
        result = JY8902_DMM_SetDCCurrent(deviceHandle_, JY8902_DC_Current_Auto);
        if (result != 0) {
            qDebug() << "Failed to set DC current range, error:" << result;
        }
    }
    
    // Set aperture and timing - optimized for diode testing with higher NPLC
    result = JY8902_DMM_SetApertureUnit(deviceHandle_, JY8902_NPLC);
    if (result != 0) {
        qDebug() << "Failed to set aperture unit, error:" << result;
    }
    
    // Use higher NPLC values for better accuracy and noise reduction in diode testing
    double nplcValue = 1.0;  // Default
    if (function == JY8902_DC_Current) {
        nplcValue = 10.0;  // Higher NPLC for current measurements (more stable for diode testing)
        qDebug() << "Using high NPLC value for current measurement (diode testing optimization)";
    } else if (function == JY8902_DC_Volts) {
        nplcValue = 5.0;   // Medium NPLC for voltage measurements
        qDebug() << "Using medium NPLC value for voltage measurement";
    }
    
    result = JY8902_DMM_SetNPLC(deviceHandle_, nplcValue);
    if (result != 0) {
        qDebug() << "Failed to set NPLC to" << nplcValue << ", error:" << result;
    } else {
        qDebug() << "Successfully set NPLC to" << nplcValue;
    }
    
    // Enable auto-zero for better accuracy (especially important for diode measurements)
    result = JY8902_DMM_DisableCalibration(deviceHandle_, false);  // Enable calibration
    if (result != 0) {
        qDebug() << "Failed to enable auto-zero calibration, error:" << result;
    } else {
        qDebug() << "Auto-zero calibration enabled for improved accuracy";
    }
    
    // Set longer trigger delays for diode testing (allows for proper settling)
    double triggerDelay = 0.001;  // Default 1ms
    if (function == JY8902_DC_Current) {
        triggerDelay = 0.1;  // 100ms for current measurements (diode forward current settling)
        qDebug() << "Using extended trigger delay for current measurement stability";
    } else if (function == JY8902_DC_Volts) {
        triggerDelay = 0.05; // 50ms for voltage measurements
        qDebug() << "Using moderate trigger delay for voltage measurement";
    }
    
    result = JY8902_DMM_SetTriggerDelay(deviceHandle_, triggerDelay);
    if (result != 0) {
        qDebug() << "Failed to set trigger delay to" << triggerDelay << "s, error:" << result;
    } else {
        qDebug() << "Successfully set trigger delay to" << triggerDelay << "s";
    }
    
    // Use immediate trigger for simplicity and reliability
    result = JY8902_DMM_SetTriggerType(deviceHandle_, JY8902_Immediately);
    if (result != 0) {
        qDebug() << "Failed to set trigger type, error:" << result;
    }
    
    qDebug() << "DMM measurement configuration completed for function:" << function 
             << "with optimized settings for diode testing";
}

bool DMMDeviceThread::configureBasicSettings()
{
    if (!deviceHandle_) {
        qDebug() << "Cannot configure basic settings - device handle is null";
        return false;
    }
    
    qDebug() << "Configuring basic DMM settings based on official examples...";
    
    int32_t result = 0;
    
    // 基本配置序列（根据官方示例）
    
    // 1. 设置采样模式为单点测量
    result = JY8902_DMM_SetSampleMode(deviceHandle_, JY8902_SingleSample);
    if (result != 0) {
        qDebug() << "Failed to set sample mode, error:" << result;
        return false;
    }
    
    // 2. 设置电源线频率（50Hz）
    result = JY8902_DMM_PowerLineFrequency(deviceHandle_, JY8902_50_Hz);
    if (result != 0) {
        qDebug() << "Failed to set power line frequency, error:" << result;
        return false;
    }
    
    // 3. 设置默认测量功能（DC电压）
    result = JY8902_DMM_SetMeasurementFunction(deviceHandle_, JY8902_DC_Volts);
    if (result != 0) {
        qDebug() << "Failed to set measurement function, error:" << result;
        return false;
    }
    
    // 4. 设置DC电压量程为自动
    result = JY8902_DMM_SetDCVolt(deviceHandle_, JY8902_DC_Volt_Auto);
    if (result != 0) {
        qDebug() << "Failed to set DC voltage range, error:" << result;
        return false;
    }
    
    // 5. 设置孔径单位为NPLC
    result = JY8902_DMM_SetApertureUnit(deviceHandle_, JY8902_NPLC);
    if (result != 0) {
        qDebug() << "Failed to set aperture unit, error:" << result;
        return false;
    }
    
    // 6. 设置NPLC为1（快速测量）
    result = JY8902_DMM_SetNPLC(deviceHandle_, 1);
    if (result != 0) {
        qDebug() << "Failed to set NPLC, error:" << result;
        return false;
    }
    
    // 7. 禁用校准（加快初始化）
    result = JY8902_DMM_DisableCalibration(deviceHandle_, true);
    if (result != 0) {
        qDebug() << "Failed to disable calibration, error:" << result;
        return false;
    }
    
    // 8. 设置触发延迟
    result = JY8902_DMM_SetTriggerDelay(deviceHandle_, 10);
    if (result != 0) {
        qDebug() << "Failed to set trigger delay, error:" << result;
        return false;
    }
    
    // 9. 设置触发类型为立即触发
    result = JY8902_DMM_SetTriggerType(deviceHandle_, JY8902_Immediately);
    if (result != 0) {
        qDebug() << "Failed to set trigger type, error:" << result;
        return false;
    }
    
    qDebug() << "Basic DMM settings configured successfully";
    currentFunction_ = JY8902_DC_Volts;
    return true;
}

DeviceResult DMMDeviceThread::performMeasurement()
{
    DeviceResult result;
    
    if (!deviceHandle_) {
        result.error = "Device not initialized";
        return result;
    }
    
    qDebug() << "DMM performing measurement with function:" << currentFunction_;
    
    // 在开始测量前，先验证设备状态并重新配置
    int32_t apiResult = 0;
    
    // 1. 停止任何正在进行的测量
    JY8902_DMM_Stop(deviceHandle_);
    QThread::msleep(100);
    
    // 2. 重新配置当前测量功能以确保状态正确
    qDebug() << "Re-configuring measurement function before read:" << currentFunction_;
    apiResult = JY8902_DMM_SetMeasurementFunction(deviceHandle_, currentFunction_);
    if (apiResult != 0) {
        result.error = QString("Failed to re-configure measurement function, error: %1").arg(apiResult);
        return result;
    }
    
    // 3. 配置适当的量程
    if (currentFunction_ == JY8902_2_Wire_Resistance) {
        apiResult = JY8902_DMM_Set2WireResistance(deviceHandle_, JY8902_2_Wire_Resistance_Auto);
        if (apiResult != 0) {
            qDebug() << "Warning: Failed to set auto resistance range, error:" << apiResult;
        }
    } else if (currentFunction_ == JY8902_DC_Volts) {
        apiResult = JY8902_DMM_SetDCVolt(deviceHandle_, JY8902_DC_Volt_Auto);
        if (apiResult != 0) {
            qDebug() << "Warning: Failed to set auto voltage range, error:" << apiResult;
        }
    } else if (currentFunction_ == JY8902_DC_Current) {
        apiResult = JY8902_DMM_SetDCCurrent(deviceHandle_, JY8902_DC_Current_Auto);
        if (apiResult != 0) {
            qDebug() << "Warning: Failed to set auto current range, error:" << apiResult;
        }
    }
    
    // 4. 设置触发为立即触发
    apiResult = JY8902_DMM_SetTriggerType(deviceHandle_, JY8902_Immediately);
    if (apiResult != 0) {
        qDebug() << "Warning: Failed to set immediate trigger, error:" << apiResult;
    }
    
    // 5. 等待设备稳定
    QThread::msleep(200);
      // 6. 使用正确的测量序列：Start -> Read -> Stop
    qDebug() << "Starting proper measurement sequence...";
    double value = 0.0;
    int maxRetries = 3;
    
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        qDebug() << "DMM measurement attempt" << attempt;
        
        // Step 1: Start measurement
        qDebug() << "Starting DMM measurement...";
        apiResult = JY8902_DMM_Start(deviceHandle_);
        if (apiResult != 0) {
            qDebug() << "Failed to start DMM measurement, error:" << apiResult;
            if (attempt < maxRetries) {
                QThread::msleep(1000);
                continue;
            } else {
                break;
            }
        }
        
        // Step 2: Wait for measurement to stabilize
        QThread::msleep(500);
        
        // Step 3: Read measurement with timeout
        int timeout = 10000;  // 10秒超时
        qDebug() << "Reading DMM value with timeout:" << timeout << "ms";
        apiResult = JY8902_DMM_Read(deviceHandle_, &value, timeout);        
        // Step 4: Stop measurement (always stop, regardless of read result)
        int stopResult = JY8902_DMM_Stop(deviceHandle_);
        if (stopResult != 0) {
            qDebug() << "Warning: Failed to stop DMM measurement, error:" << stopResult;
        }
        
        if (apiResult == 0) {
            result.success = true;
            result.value = value;
            qDebug() << "DMM measurement successful, value:" << value;
            return result;
        } else {
            qDebug() << "DMM read attempt" << attempt << "failed with error:" << apiResult;
              // 分析错误类型并采取不同的恢复策略
            if (apiResult == -10133) {
                qDebug() << "Error -10133 detected (DMM Not Started), attempting recovery...";
                // 确保停止任何残留的测量
                JY8902_DMM_Stop(deviceHandle_);
                QThread::msleep(300);
                
                // 重新配置设备
                if (!configureBasicSettings()) {
                    qDebug() << "Failed to reconfigure basic settings during -10133 recovery";
                }
                configureMeasurement(currentFunction_);
                QThread::msleep(500);
                
            } else if (apiResult == -9002) {
                qDebug() << "Error -9002 detected, attempting device reset...";
                
                // 尝试重置设备状态
                JY8902_DMM_Stop(deviceHandle_);
                QThread::msleep(500);
                
                // 重新配置基本设置
                if (!configureBasicSettings()) {
                    qDebug() << "Failed to reconfigure basic settings during recovery";
                }
                
                // 重新配置测量功能
                configureMeasurement(currentFunction_);
                QThread::msleep(300);
                
            } else if (attempt < maxRetries) {
                qDebug() << "Retrying DMM read after delay...";
                QThread::msleep(1000);  // 等待1秒后重试
            }
        }
    }
    
    // 所有尝试都失败了
    QString errorMsg = QString("Failed to read DMM value after %1 attempts, final error: %2").arg(maxRetries).arg(apiResult);
    
    // 提供详细的错误诊断信息
    switch (apiResult) {
        case -10134:  // Error_DMM_ReadDataTimeOut
            errorMsg += "\n诊断建议: 测量超时，检查信号连接和信号幅度";
            break;
        case -10138:  // Error_DMM_TimeOut
            errorMsg += "\n诊断建议: 设备超时，检查设备状态和通信连接";
            break;
        case -9002:
            errorMsg += "\n诊断建议: 尝试重启设备，检查驱动程序和设备连接";
            break;
        case -10133:
            errorMsg += "\n诊断建议: 检查设备是否被其他程序占用，尝试重新插拔设备";
            break;
        default:
            errorMsg += "\n诊断建议: 请参考用户手册中的错误代码表进行故障排除";
            break;
    }
    
    result.success = false;
    result.error = errorMsg;
    qDebug() << errorMsg;
    
    return result;
}
