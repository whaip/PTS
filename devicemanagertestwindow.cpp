#include "devicemanagertestwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <algorithm>
#include <numeric>
#include <QEventLoop>
#include <QTimer>

// TaskExecutionThread 实现
TaskExecutionThread::TaskExecutionThread(DeviceManager* deviceManager, QObject* parent)
    : QThread(parent)
    , deviceManager_(deviceManager)
    , taskRunning_(false)
    , stopRequested_(false)
{
}

TaskExecutionThread::~TaskExecutionThread()
{
    stopTask();
    if (!wait(3000)) {
        terminate();
        wait(1000);
    }
}

void TaskExecutionThread::startMultiPointAcquisitionTask(const QString& deviceName, 
                                                       const QVector<int>& channels,
                                                       double sampleRate, 
                                                       int samplesPerChannel,
                                                       double rangeMin, 
                                                       double rangeMax)
{
    QMutexLocker locker(&taskMutex_);
    
    if (taskRunning_) {
        emit taskFailed("任务已在运行中，请先停止当前任务");
        return;
    }
    
    // 保存任务参数
    deviceName_ = deviceName;
    channels_ = channels;
    sampleRate_ = sampleRate;
    samplesPerChannel_ = samplesPerChannel;
    rangeMin_ = rangeMin;
    rangeMax_ = rangeMax;
    
    stopRequested_ = false;
    taskRunning_ = true;
    
    // 启动线程
    start();
}

void TaskExecutionThread::stopTask()
{
    QMutexLocker locker(&taskMutex_);
    stopRequested_ = true;
    
    if (isRunning()) {
        // 等待线程结束
        locker.unlock();
        if (!wait(5000)) {
            terminate();
            wait(1000);
        }
        locker.relock();
    }
    
    taskRunning_ = false;
}

bool TaskExecutionThread::isTaskRunning() const
{
    QMutexLocker locker(&taskMutex_);
    return taskRunning_;
}

void TaskExecutionThread::run()
{
    QString taskName = QString("多点采集测试 - %1").arg(deviceName_);
    emit taskStarted(taskName);
    
    try {
        TaskResult result = executeMultiPointAcquisition();
        
        QMutexLocker locker(&taskMutex_);
        if (!stopRequested_) {
            if (result.success) {
                calculateChannelStats(result);
                emit taskCompleted(result);
            } else {
                emit taskFailed(result.errorMessage);
            }
        }
        taskRunning_ = false;
        
    } catch (const std::exception& e) {
        QMutexLocker locker(&taskMutex_);
        taskRunning_ = false;
        emit taskFailed(QString("任务执行异常: %1").arg(e.what()));
    } catch (...) {
        QMutexLocker locker(&taskMutex_);
        taskRunning_ = false;
        emit taskFailed("任务执行发生未知异常");
    }
}

TaskResult TaskExecutionThread::executeMultiPointAcquisition()
{
    TaskResult result;
    result.deviceName = deviceName_;
    result.channels = channels_;
    result.sampleRate = sampleRate_;
    result.samplesPerChannel = samplesPerChannel_;
    
    QElapsedTimer totalTimer;
    totalTimer.start();
    
    emit taskProgress("检查设备状态...", 5);
    
    // 检查设备管理器状态
    if (!deviceManager_) {
        result.errorMessage = "DeviceManager 未初始化";
        return result;
    }
    
    // 检查停止请求
    {
        QMutexLocker locker(&taskMutex_);
        if (stopRequested_) {
            result.errorMessage = "任务被用户取消";
            return result;
        }
    }
    
    emit taskProgress("配置多点采集...", 15);
    
    // 1. 配置多点采集
    if (!deviceManager_->configureMultiPointAcquisition(deviceName_, channels_, 
                                                        sampleRate_, samplesPerChannel_,
                                                        rangeMin_, rangeMax_)) {
        result.errorMessage = "配置多点采集失败: " + deviceManager_->getLastError();
        return result;
    }
    
    // 检查停止请求
    {
        QMutexLocker locker(&taskMutex_);
        if (stopRequested_) {
            result.errorMessage = "任务被用户取消";
            return result;
        }
    }
    
    emit taskProgress("启动数据采集...", 25);
    
    // 2. 启动采集
    if (!deviceManager_->startMultiPointAcquisition(deviceName_)) {
        result.errorMessage = "启动多点采集失败: " + deviceManager_->getLastError();
        return result;
    }
    
    emit taskProgress("等待数据采集完成...", 35);
    
    // 3. 等待数据采集完成
    bool dataReady = false;
    int maxAttempts = 100;  // 最多尝试100次
    int attempt = 0;
    
    QVector<QVector<double>> channelData;
    
    while (!dataReady && attempt < maxAttempts) {
        // 检查停止请求
        {
            QMutexLocker locker(&taskMutex_);
            if (stopRequested_) {
                deviceManager_->stopMultiPointAcquisition(deviceName_);
                result.errorMessage = "任务被用户取消";
                return result;
            }
        }
        
        attempt++;
        
        // 更新进度 (35% 到 85% 之间)
        int progress = 35 + (attempt * 50) / maxAttempts;
        emit taskProgress(QString("等待数据... (%1/%2)").arg(attempt).arg(maxAttempts), progress);
        
        if (deviceManager_->readMultiPointData(deviceName_, channelData, 1000)) {
            if (!channelData.isEmpty() && !channelData[0].isEmpty()) {
                dataReady = true;
                emit taskProgress("数据采集成功！", 85);
                break;
            }
        }
        
        // 短暂等待
        msleep(50);  // 50ms间隔
    }
    
    emit taskProgress("停止采集...", 90);
    
    // 4. 停止采集
    if (!deviceManager_->stopMultiPointAcquisition(deviceName_)) {
        // 这里不作为致命错误，只记录警告
        qDebug() << "警告：停止采集失败";
    }
    
    emit taskProgress("处理结果...", 95);
    
    // 5. 处理结果
    if (dataReady && !channelData.isEmpty()) {
        result.success = true;
        result.channelData = channelData;
        result.executionTime = totalTimer.elapsed();
        
        emit taskProgress("任务完成！", 100);
    } else {
        result.errorMessage = QString("数据采集失败，尝试了%1次").arg(attempt);
    }
    
    return result;
}

void TaskExecutionThread::calculateChannelStats(TaskResult& result)
{
    result.channelStats.clear();
    result.channelStats.resize(result.channelData.size());
    
    for (int ch = 0; ch < result.channelData.size(); ++ch) {
        if (!result.channelData[ch].isEmpty()) {
            TaskResult::ChannelStats& stats = result.channelStats[ch];
            
            const QVector<double>& chData = result.channelData[ch];
            
            // 计算统计值
            stats.minValue = *std::min_element(chData.begin(), chData.end());
            stats.maxValue = *std::max_element(chData.begin(), chData.end());
            stats.avgValue = std::accumulate(chData.begin(), chData.end(), 0.0) / chData.size();
            
            // 获取前5个样本
            for (int i = 0; i < qMin(5, chData.size()); ++i) {
                stats.firstSamples << QString::number(chData[i], 'f', 3);
            }
        }
    }
}

DeviceManagerTestWindow::DeviceManagerTestWindow(DeviceManager* deviceManager, QWidget *parent)
    : QDialog(parent), deviceManager_(deviceManager), taskThread_(nullptr)
{
    setWindowTitle("设备管理器测试窗口");
    resize(800, 600);
    
    setupUI();
    
    if (deviceManager_) {
        connect(deviceManager_, &DeviceManager::deviceStatusChanged,
                this, &DeviceManagerTestWindow::onDeviceStatusChanged);
        connect(deviceManager_, &DeviceManager::errorOccurred,
                this, &DeviceManagerTestWindow::onErrorOccurred);
        
        // 初始化任务执行线程
        taskThread_ = new TaskExecutionThread(deviceManager_, this);
        
        // 连接任务线程信号
        connect(taskThread_, &TaskExecutionThread::taskStarted,
                this, &DeviceManagerTestWindow::onTaskStarted);
        connect(taskThread_, &TaskExecutionThread::taskProgress,
                this, &DeviceManagerTestWindow::onTaskProgress);
        connect(taskThread_, &TaskExecutionThread::taskCompleted,
                this, &DeviceManagerTestWindow::onTaskCompleted);
        connect(taskThread_, &TaskExecutionThread::taskFailed,
                this, &DeviceManagerTestWindow::onTaskFailed);
    }
    
    updateDeviceStatusDisplay();
}

DeviceManagerTestWindow::~DeviceManagerTestWindow()
{
    if (taskThread_) {
        taskThread_->stopTask();
        taskThread_ = nullptr;
    }
}

void DeviceManagerTestWindow::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    
    // 创建测试按钮组
    testButtonsGroup_ = new QGroupBox("测试功能", this);
    QGridLayout* buttonLayout = new QGridLayout(testButtonsGroup_);
    
    initDevicesBtn_ = new QPushButton("初始化设备", this);
    measureVoltageBtn_ = new QPushButton("测量电压", this);
    measureCurrentBtn_ = new QPushButton("测量电流", this);
    measureResistanceBtn_ = new QPushButton("多点采集测试(线程)", this);  // 修改按钮名称
    outputVoltageBtn_ = new QPushButton("输出电压", this);
    deviceStatusBtn_ = new QPushButton("检查设备状态", this);
    clearBtn_ = new QPushButton("清除结果", this);
    stopTaskBtn_ = new QPushButton("停止任务", this);  // 新增停止任务按钮
    
    buttonLayout->addWidget(initDevicesBtn_, 0, 0);
    buttonLayout->addWidget(measureVoltageBtn_, 0, 1);
    buttonLayout->addWidget(measureCurrentBtn_, 0, 2);
    buttonLayout->addWidget(measureResistanceBtn_, 1, 0);
    buttonLayout->addWidget(outputVoltageBtn_, 1, 1);
    buttonLayout->addWidget(deviceStatusBtn_, 1, 2);
    buttonLayout->addWidget(clearBtn_, 2, 0);
    buttonLayout->addWidget(stopTaskBtn_, 2, 1);
    
    // 停止任务按钮初始状态为禁用
    stopTaskBtn_->setEnabled(false);
    stopTaskBtn_->setStyleSheet("QPushButton:disabled { background-color: #cccccc; color: #666666; }");
    
    // 创建参数设置组
    parametersGroup_ = new QGroupBox("测试参数", this);
    QFormLayout* paramLayout = new QFormLayout(parametersGroup_);
    
    channelSpin_ = new QSpinBox(this);
    channelSpin_->setRange(0, 31);
    channelSpin_->setValue(0);
    
    voltageSpin_ = new QDoubleSpinBox(this);
    voltageSpin_->setRange(-10.0, 10.0);
    voltageSpin_->setSingleStep(0.1);
    voltageSpin_->setValue(1.0);
    voltageSpin_->setSuffix(" V");
    
    deviceCombo_ = new QComboBox(this);
    deviceCombo_->addItems({"JY5322", "JY5323", "JY8902", "JY5711"});
    
    timeoutSpin_ = new QSpinBox(this);
    timeoutSpin_->setRange(1000, 30000);
    timeoutSpin_->setValue(5000);
    timeoutSpin_->setSuffix(" ms");
    
    paramLayout->addRow("通道:", channelSpin_);
    paramLayout->addRow("电压值:", voltageSpin_);
    paramLayout->addRow("设备:", deviceCombo_);
    paramLayout->addRow("超时时间:", timeoutSpin_);
    
    // 创建设备状态组
    statusGroup_ = new QGroupBox("设备状态", this);
    QGridLayout* statusLayout = new QGridLayout(statusGroup_);
    
    aoStatusLabel_ = new QLabel("AO (JY5711): 未知", this);
    daq5322StatusLabel_ = new QLabel("DAQ5322: 未知", this);
    daq5323StatusLabel_ = new QLabel("DAQ5323: 未知", this);
    dmmStatusLabel_ = new QLabel("DMM (JY8902): 未知", this);
    
    testProgress_ = new QProgressBar(this);
    testProgress_->setVisible(false);
    testProgress_->setStyleSheet("QProgressBar { border: 1px solid grey; border-radius: 5px; text-align: center; } QProgressBar::chunk { background-color: #05B8CC; }");
    
    statusLayout->addWidget(aoStatusLabel_, 0, 0);
    statusLayout->addWidget(daq5322StatusLabel_, 0, 1);
    statusLayout->addWidget(daq5323StatusLabel_, 1, 0);
    statusLayout->addWidget(dmmStatusLabel_, 1, 1);
    statusLayout->addWidget(testProgress_, 2, 0, 1, 2);
    
    // 创建结果显示组
    resultsGroup_ = new QGroupBox("测试结果", this);
    QVBoxLayout* resultLayout = new QVBoxLayout(resultsGroup_);
    
    resultText_ = new QTextEdit(this);
    resultText_->setReadOnly(true);
    resultText_->setMaximumHeight(200);
    
    resultLayout->addWidget(resultText_);
    
    // 将所有组添加到主布局
    mainLayout_->addWidget(testButtonsGroup_);
    mainLayout_->addWidget(parametersGroup_);
    mainLayout_->addWidget(statusGroup_);
    mainLayout_->addWidget(resultsGroup_);
    
    // 连接按钮信号
    connect(initDevicesBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testInitializeDevices);
    connect(measureVoltageBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testMeasureVoltage);
    connect(measureCurrentBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testMeasureCurrent);
    connect(measureResistanceBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testMeasureResistance);
    connect(outputVoltageBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testOutputVoltage);
    connect(deviceStatusBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testDeviceStatus);
    connect(clearBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::clearResults);
    connect(stopTaskBtn_, &QPushButton::clicked, [this]() {
        if (taskThread_ && taskThread_->isTaskRunning()) {
            appendResult("用户请求停止任务...");
            taskThread_->stopTask();
        }
    });
}

void DeviceManagerTestWindow::setTestingState(bool testing)
{
    // 设置按钮状态
    initDevicesBtn_->setEnabled(!testing);
    measureVoltageBtn_->setEnabled(!testing);
    measureCurrentBtn_->setEnabled(!testing);
    measureResistanceBtn_->setEnabled(!testing);
    outputVoltageBtn_->setEnabled(!testing);
    deviceStatusBtn_->setEnabled(!testing);
    
    // 停止任务按钮状态
    stopTaskBtn_->setEnabled(testing);
    
    // 进度条状态
    testProgress_->setVisible(testing);
    if (!testing) {
        testProgress_->setValue(0);
    }
}

// 任务线程相关槽函数
void DeviceManagerTestWindow::onTaskStarted(const QString& taskName)
{
    appendResult(QString("=== 开始执行任务: %1 ===").arg(taskName));
    setTestingState(true);
    testProgress_->setValue(0);
}

void DeviceManagerTestWindow::onTaskProgress(const QString& message, int percentage)
{
    appendResult(message);
    if (percentage >= 0) {
        testProgress_->setValue(percentage);
    }
}

void DeviceManagerTestWindow::onTaskCompleted(const TaskResult& result)
{
    setTestingState(false);
    
    appendResult("=== 任务执行完成 ===");
    appendResult(QString("✓ 多点采集测试成功！执行时间: %1ms").arg(result.executionTime));
    appendResult(QString("设备: %1").arg(result.deviceName));
    appendResult(QString("采集参数: 采样率=%1Hz, 每通道样本数=%2")
                .arg(result.sampleRate).arg(result.samplesPerChannel));
    
    // 验证数据完整性
    bool dataValid = true;
    for (int ch = 0; ch < result.channelData.size(); ++ch) {
        if (result.channelData[ch].size() != result.samplesPerChannel) {
            appendResult(QString("❌ 通道 %1 数据不完整: 期望 %2 样本，实际 %3 样本")
                       .arg(ch).arg(result.samplesPerChannel).arg(result.channelData[ch].size()));
            dataValid = false;
        }
    }
    
    if (dataValid) {
        appendResult("✓ 数据完整性验证通过");
        
        // 显示数据统计
        for (int ch = 0; ch < result.channelStats.size() && ch < result.channels.size(); ++ch) {
            const TaskResult::ChannelStats& stats = result.channelStats[ch];
            appendResult(QString("通道 %1: 样本数=%2, 范围=[%3, %4]V, 平均=%5V")
                       .arg(result.channels[ch])
                       .arg(result.channelData[ch].size())
                       .arg(stats.minValue, 0, 'f', 3)
                       .arg(stats.maxValue, 0, 'f', 3)
                       .arg(stats.avgValue, 0, 'f', 3));
                       
            // 显示前5个样本
            appendResult(QString("  前5个样本: [%1]").arg(stats.firstSamples.join(", ")));
        }
        
        // 可选：导出数据
        QString fileName = QString("multipoint_test_%1.csv")
                          .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        if (deviceManager_->exportAcquisitionData(result.channelData, result.channels, fileName)) {
            appendResult("数据已导出到: " + fileName);
        }
    }
}

void DeviceManagerTestWindow::onTaskFailed(const QString& error)
{
    setTestingState(false);
    appendResult("=== 任务执行失败 ===");
    appendResult(QString("❌ 错误: %1").arg(error));
}

void DeviceManagerTestWindow::testInitializeDevices()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    
    appendResult("开始初始化设备...");
    testProgress_->setVisible(true);
    testProgress_->setRange(0, 0); // 显示无限进度条
    
    bool success = deviceManager_->initializeDeviceThreads();
    
    testProgress_->setVisible(false);
    
    if (success) {
        appendResult("设备初始化成功!");
    } else {
        appendResult("设备初始化失败: " + deviceManager_->getLastError());
    }
}

void DeviceManagerTestWindow::testMeasureVoltage()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    
    QVector<QString> daqDevices = deviceManager_->getDAQDevices();
    if (daqDevices.isEmpty()) {
        appendResult("未找到可用的DAQ设备");
        return;
    }

    appendResult("找到DAQ设备: " + daqDevices.join(", "));
    QString deviceName = daqDevices.first();

    // 使用更合理的采集参数
    QVector<int> channels = {0, 1};
    double sampleRate = 10000.0;      // 降低到10kHz
    int samplesPerChannel = 1000;     // 每通道1000个样本
    
    appendResult(QString("配置多点采集 - 设备: %1, 通道: %2, 采样率: %3Hz, 样本数: %4")
                .arg(deviceName)
                .arg(channels.size())
                .arg(sampleRate)
                .arg(samplesPerChannel));
    
    if (!deviceManager_->configureMultiPointAcquisition(deviceName, channels, sampleRate, samplesPerChannel)) {
        appendResult("多点采集配置失败: " + deviceManager_->getLastError());
        return;
    }

    if (!deviceManager_->startMultiPointAcquisition(deviceName)) {
        appendResult("多点采集启动失败: " + deviceManager_->getLastError());
        return;
    }

    appendResult("多点采集已启动，等待数据...");
    
    QVector<QVector<double>> channelData;
    int attempts = 0;
    const int maxAttempts = 50;  // 增加尝试次数
    
    while (attempts < maxAttempts) {
        QThread::msleep(100); // 减少等待时间到100ms
        
        if (deviceManager_->readMultiPointData(deviceName, channelData, 5000)) {
            appendResult("多点采集数据读取成功!");
            appendResult(QString("采集到 %1 个通道的数据").arg(channelData.size()));
            
            for (int ch = 0; ch < channelData.size() && ch < channels.size(); ++ch) {
                appendResult(QString("通道%1: %2 个样本").arg(channels[ch]).arg(channelData[ch].size()));
                
                // 计算统计信息
                if (!channelData[ch].isEmpty()) {
                    double sum = 0.0;
                    double minVal = channelData[ch][0];
                    double maxVal = channelData[ch][0];
                    
                    for (double value : channelData[ch]) {
                        sum += value;
                        minVal = qMin(minVal, value);
                        maxVal = qMax(maxVal, value);
                    }
                    
                    double avgVal = sum / channelData[ch].size();
                    appendResult(QString("  统计: 平均值=%.4f, 最小值=%.4f, 最大值=%.4f")
                               .arg(avgVal).arg(minVal).arg(maxVal));
                    
                    // 显示前5个样本
                    QString samples = "  前5个样本: ";
                    int showCount = qMin(5, channelData[ch].size());
                    for (int i = 0; i < showCount; ++i) {
                        samples += QString::number(channelData[ch][i], 'f', 4) + " ";
                    }
                    appendResult(samples);
                }
            }
            
            // 只有成功读取到数据才导出
            if (!channelData.isEmpty() && !channelData[0].isEmpty()) {
                QString fileName = QString("multipoint_data_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
                if (deviceManager_->exportAcquisitionData(channelData, channels, fileName)) {
                    appendResult("数据已导出到: " + fileName);
                } else {
                    appendResult("数据导出失败: " + deviceManager_->getLastError());
                }
            } else {
                appendResult("警告: 读取到的数据为空，跳过导出");
            }
            
            break;
        } else {
            attempts++;
            if (attempts % 10 == 0) {  // 每10次尝试输出一次状态
                appendResult(QString("等待数据... (%1/%2) - %3")
                           .arg(attempts).arg(maxAttempts)
                           .arg(deviceManager_->getLastError()));
            }
        }
    }
    
    if (attempts >= maxAttempts) {
        appendResult(QString("警告: 超时未读取到完整数据 (尝试了%1次)").arg(maxAttempts));
    }
    
    // 停止采集
    appendResult("正在停止多点采集...");
    if (!deviceManager_->stopMultiPointAcquisition(deviceName)) {
        appendResult("停止多点采集失败: " + deviceManager_->getLastError());
    } else {
        appendResult("多点采集已成功停止");
    }
}

void DeviceManagerTestWindow::testMeasureCurrent()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    if (!deviceManager_->isSystemReady()) {
        appendResult("系统未就绪，请检查设备连接状态");
        return;
    }

    const QString syncGroupName = "sync_test_group";
    const QStringList deviceNames = {"JY5711", "JY5322"};

    deviceManager_->createSyncGroup(syncGroupName, deviceNames);

    DeviceOperation configOp;
    configOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    configOp.parameters["channelCount"] = 2;
    configOp.parameters["sampleRate"] = 1000000.0;
    // 配置波形数据
    QVariantList waveformConfigs;

    // 通道1: 4V高电平
    QVariantMap ch1Config;
    ch1Config["channel"] = 0;
    ch1Config["type"] = static_cast<int>(PXIe5711_testtype::HighLevelWave);
    ch1Config["amplitude"] = 4.0;
    ch1Config["frequency"] = 0.0;  // 直流
    ch1Config["lowRange"] = -10.0;
    ch1Config["highRange"] = 10.0;
    waveformConfigs.append(ch1Config);

    // 通道2: 正弦波
    QVariantMap ch2Config;
    ch2Config["channel"] = 1;
    ch2Config["type"] = static_cast<int>(PXIe5711_testtype::SineWave);
    ch2Config["amplitude"] = 4;
    ch2Config["dutyCycle"] = 0.5;
    ch2Config["frequency"] = 1000;
    ch2Config["lowRange"] = -10.0;
    ch2Config["highRange"] = 10.0;
    waveformConfigs.append(ch2Config);

    configOp.parameters["waveforms"] = waveformConfigs;

    if (!deviceManager_->submitOperation("JY5711", configOp)) {
        appendResult("提交JY5711配置操作失败");
        return;
    }
    DeviceResult aoConfigResult = deviceManager_->waitForResult("JY5711", 5000);
    if (aoConfigResult.success) {
        DeviceOperation outputOp;
        outputOp.command = DeviceCommand::WRITE_DATA;
        outputOp.parameters["waveforms"] = waveformConfigs;
        outputOp.parameters["sampleRate"] = 1000000;
        outputOp.parameters["samplesPerChannel"] = 1000000; // 1秒的数据
        deviceManager_->submitOperation("JY5711", outputOp);
        DeviceResult outputResult = deviceManager_->waitForResult("JY5711", 15000);
        if(!outputResult.success) {
            qDebug() << "JY5711数据写入失败";
        }
    }
    qDebug() << "JY5711配置成功，实际采样率:" << aoConfigResult.data["actualSampleRate"].toDouble();
    QVector<int> daqChannels = {0, 1};
    if (!deviceManager_->configureMultiPointAcquisition("JY5322", daqChannels, 1000000, 1000000, -10.0, 10.0)) {
        appendResult("JY5322多点采集配置失败");
        return;
    }
    QList<DeviceOperation> syncOperations;
    // AO输出波形操作
    DeviceOperation triggerOp;
    triggerOp.deviceName = "JY5711";
    triggerOp.command = DeviceCommand::SYNC_TRIGGER;
    syncOperations.append(triggerOp);

    // DAQ采集启动操作
    DeviceOperation daqStartOp;
    daqStartOp.command = DeviceCommand::START_MEASUREMENT;
    daqStartOp.deviceName = "JY5322";
    daqStartOp.syncGroup = syncGroupName;
    daqStartOp.syncDelay = 0;  // 无延迟
    daqStartOp.timeout = 5000;
    syncOperations.append(daqStartOp);
    
    // 6. 执行同步操作
    qDebug() << "执行同步操作...";
    if (!deviceManager_->executeSync(syncGroupName, syncOperations, 5000)) {
        appendResult("同步操作执行失败");
        deviceManager_->removeSyncGroup(syncGroupName);
        return;
    }
    
    // 7. 等待操作完成并检查结果
    qDebug() << "等待AO输出结果...";
    DeviceResult aoResult = deviceManager_->waitForResult("JY5711", 5000);
    if (!aoResult.success) {
        appendResult("JY5711输出失败: " + aoResult.error);
        deviceManager_->removeSyncGroup(syncGroupName);
        return;
    }
    
    qDebug() << "等待DAQ启动结果...";
    DeviceResult daqStartResult = deviceManager_->waitForResult("JY5322", 5000);
    if (!daqStartResult.success) {
        appendResult("JY5322采集启动失败: " + daqStartResult.error);
        deviceManager_->removeSyncGroup(syncGroupName);
        return;
    }
    
    // 8. 使用事件循环等待数据采集完成并读取数据
    qDebug() << "等待数据采集完成...";
    
    
    appendResult("开始读取采集数据...");
    
    // 使用通用的事件循环等待方法读取数据
    QVector<QVector<double>> finalChannelData;
    DeviceManager::WaitResult waitResult = deviceManager_->waitForDataWithEventLoop("JY5322", finalChannelData, 30, 200, 8000);
            
    // 9. 停止采集
    if (!deviceManager_->stopMultiPointAcquisition("JY5322")) {
        qDebug() << "警告：停止JY5322采集失败";
    }
    
    // 10. 分析结果
    if (waitResult.success && !finalChannelData.isEmpty()) {
    qDebug() << "同步测试完成！采集到" << finalChannelData.size() << "个通道的数据";
        appendResult(QString("✓ 同步采集成功！采集到 %1 个通道的数据").arg(finalChannelData.size()));
        
    for (int ch = 0; ch < finalChannelData.size(); ++ch) {
        if (!finalChannelData[ch].isEmpty()) {
            double avgValue = 0.0;
            double maxValue = *std::max_element(finalChannelData[ch].begin(), finalChannelData[ch].end());
            double minValue = *std::min_element(finalChannelData[ch].begin(), finalChannelData[ch].end());

            for (double val : finalChannelData[ch]) {
                avgValue += val;
            }
            avgValue /= finalChannelData[ch].size();

                QString channelResult = QString("通道%1: 样本数=%2, 平均值=%3V, 最大值=%4V, 最小值=%5V")
                        .arg(daqChannels[ch])
                        .arg(finalChannelData[ch].size())
                        .arg(avgValue, 0, 'f', 6)
                        .arg(maxValue, 0, 'f', 6)
                        .arg(minValue, 0, 'f', 6);
                            
                qDebug() << channelResult;
                appendResult(channelResult);
            }
        }
        
        // 导出数据
        QString fileName = QString("sync_test_data_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        if (deviceManager_->exportAcquisitionData(finalChannelData, daqChannels, fileName)) {
            appendResult("同步测试数据已导出到: " + fileName);
        }
        
        appendResult("✓ 同步输出和采集测试成功完成！");
    } else {
        appendResult("❌ 同步采集失败或超时");
        appendResult(QString("诊断信息 - 尝试次数: %1, 超时: %2")
                   .arg(waitResult.attempts)
                   .arg(waitResult.timeout ? "是" : "否"));
        if (!waitResult.errorMessage.isEmpty()) {
            appendResult("错误详情: " + waitResult.errorMessage);
        }
    }
    
    // 11. 清理同步组
    deviceManager_->removeSyncGroup(syncGroupName);
}

void DeviceManagerTestWindow::testMeasureResistance()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    
    appendResult("=== 开始DMM设备 (JY8902) 底层操作测试 ===");
    
    // 1. 检查DMM设备状态
    DeviceStatus dmmStatus = deviceManager_->getDeviceStatus("JY8902");
    if (dmmStatus != DeviceStatus::CONNECTED) {
        appendResult("❌ JY8902设备未连接，无法进行测试");
        return;
    }
    appendResult("✓ JY8902设备已连接");
    
    // 2. 使用DeviceOperation配置DMM连续电阻测量
    appendResult("步骤1: 使用DeviceOperation配置DMM连续电阻测量...");
    
    DeviceOperation configOp;
    configOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    configOp.parameters["range"] = "auto";
    configOp.parameters["samplesPerTrigger"] = 20;
    configOp.parameters["sampleInterval"] = 0.02;
    configOp.parameters["useNPLC"] = false;
    configOp.parameters["apertureTime"] = 0.02;
    configOp.parameters["nplcValue"] = 3;
    configOp.parameters["triggerDelay"] = 10;
    configOp.parameters["bufferSize"] = 1000;
    configOp.parameters["timeout"] = 10000;
    configOp.timeout = 10000;
    
    if (!deviceManager_->submitOperation("JY8902", configOp)) {
        appendResult("❌ DMM配置操作提交失败");
        return;
    }
    
    DeviceResult configResult = deviceManager_->waitForResult("JY8902", 10000);
    if (configResult.success) {
        appendResult("✓ DMM连续电阻测量配置成功");
        appendResult(QString("配置参数: %1").arg(configResult.data.keys().join(", ")));
    } else {
        appendResult("❌ DMM配置失败: " + configResult.error);
        return;
    }
    
    // 3. 使用DeviceOperation启动连续测量
    appendResult("步骤2: 启动DMM连续测量...");
    
    DeviceOperation startOp;
    startOp.command = DeviceCommand::START_MEASUREMENT;
    startOp.timeout = 10000;
    
    if (!deviceManager_->submitOperation("JY8902", startOp)) {
        appendResult("❌ DMM启动操作提交失败");
        return;
    }
    
    DeviceResult startResult = deviceManager_->waitForResult("JY8902", 10000);
    if (startResult.success) {
        appendResult("✓ DMM连续测量启动成功");
    } else {
        appendResult("❌ DMM启动失败: " + startResult.error);
        return;
    }
    
    // 4. 使用DeviceOperation发送软件触发
    appendResult("步骤3: 发送软件触发...");
    
    DeviceOperation triggerOp;
    triggerOp.command = DeviceCommand::SYNC_TRIGGER;
    triggerOp.timeout = 10000;
    
    if (!deviceManager_->submitOperation("JY8902", triggerOp)) {
        appendResult("❌ DMM软件触发操作提交失败");
        return;
    }
    
    DeviceResult triggerResult = deviceManager_->waitForResult("JY8902", 10000);
    if (triggerResult.success) {
        appendResult("✓ DMM软件触发发送成功");
    } else {
        appendResult("❌ DMM软件触发失败: " + triggerResult.error);
        return;
    }
    
    // 5. 使用DeviceOperation读取数据（事件循环方式）
    appendResult("步骤4: 使用事件循环读取DMM数据...");
    
    // 使用兼容的事件循环等待数据
    QVector<QVector<double>> channelData;
    DeviceManager::WaitResult waitResult = deviceManager_->waitForDataWithEventLoop(
        "JY8902", channelData, 50, 100, 15000);
    
    if (waitResult.success && !channelData.isEmpty() && !channelData[0].isEmpty()) {
        appendResult("✓ 事件循环数据读取成功！");
        
        // 分析电阻数据
        QVector<double> resistanceData = channelData[0];
        double avgResistance = 0.0;
        double maxResistance = *std::max_element(resistanceData.begin(), resistanceData.end());
        double minResistance = *std::min_element(resistanceData.begin(), resistanceData.end());
        
        for (double val : resistanceData) {
            avgResistance += val;
        }
        avgResistance /= resistanceData.size();
        
        appendResult(QString("数据分析结果:"));
        appendResult(QString("  样本数: %1").arg(resistanceData.size()));
        appendResult(QString("  平均值: %1 Ω").arg(avgResistance, 0, 'e', 3));
        appendResult(QString("  最大值: %1 Ω").arg(maxResistance, 0, 'e', 3));
        appendResult(QString("  最小值: %1 Ω").arg(minResistance, 0, 'e', 3));
        
    } else {
        appendResult("❌ 事件循环数据读取失败");
        appendResult(QString("尝试次数: %1, 超时: %2")
                   .arg(waitResult.attempts)
                   .arg(waitResult.timeout ? "是" : "否"));
        if (!waitResult.errorMessage.isEmpty()) {
            appendResult("错误详情: " + waitResult.errorMessage);
        }
    }
    
    appendResult("=== DMM设备底层操作测试完成 ===");
}

void DeviceManagerTestWindow::testOutputVoltage()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    
    DeviceOperation configOp;
    configOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    configOp.parameters["channelCount"] = 3;
    configOp.parameters["sampleRate"] = 1000000.0;
    
    QVariantList waveforms;
    // 通道0 - 高电平
    QVariantMap HighLevel;
    HighLevel["channel"] = 3;
    HighLevel["type"] = static_cast<int>(PXIe5711_testtype::HighLevelWave);
    HighLevel["amplitude"] = 5.0;
    HighLevel["frequency"] = 1000.0;
    HighLevel["lowRange"] = -10.0;
    HighLevel["highRange"] = 10.0;
    waveforms.append(HighLevel);

    // 通道1 - 方波
    QVariantMap square;
    square["channel"] = 1;
    square["type"] = static_cast<int>(PXIe5711_testtype::SquareWave);
    square["amplitude"] = 3.0;
    square["frequency"] = 500.0;
    square["dutyCycle"] = 0.3;
    square["lowRange"] = -10.0;
    square["highRange"] = 10.0;
    waveforms.append(square);

    // 通道2 - 三角波
    QVariantMap triangle;
    triangle["channel"] = 2;
    triangle["type"] = static_cast<int>(PXIe5711_testtype::TriangleWave);
    triangle["amplitude"] = 2.0;
    triangle["frequency"] = 2000.0;
    triangle["lowRange"] = -10.0;
    triangle["highRange"] = 10.0;
    waveforms.append(triangle);

    configOp.parameters["waveforms"] = waveforms;
    
    deviceManager_->submitOperation("JY5711", configOp);
    DeviceResult configResult = deviceManager_->waitForResult("JY5711", 10000);

    if (configResult.success) {
        DeviceOperation outputOp;
        outputOp.command = DeviceCommand::WRITE_DATA;
        outputOp.parameters["waveforms"] = waveforms;
        outputOp.parameters["sampleRate"] = 1000000;
        outputOp.parameters["samplesPerChannel"] = 1000000; // 1秒的数据
        
        deviceManager_->submitOperation("JY5711", outputOp);
        DeviceResult outputResult = deviceManager_->waitForResult("JY5711", 15000);
        
        if (outputResult.success) {
            qDebug() << "Waveform output started successfully";
            
            // 发送软件触发开始输出
            DeviceOperation triggerOp;
            triggerOp.command = DeviceCommand::SYNC_TRIGGER;
            deviceManager_->submitOperation("JY5711", triggerOp);
        }
    } else {
        appendResult("电压输出失败: " + deviceManager_->getLastError());
    }
}

void DeviceManagerTestWindow::testDeviceStatus()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    
    appendResult("检查设备状态...");
    
    QStringList deviceNames = {"JY5711", "JY5322", "JY5323", "JY8902"};
    
    for (const QString& deviceName : deviceNames) {
        DeviceStatus status = deviceManager_->getDeviceStatus(deviceName);
        QString statusText;
        
        switch (status) {
            case DeviceStatus::CONNECTED:
                statusText = "已连接";
                break;
            case DeviceStatus::DISCONNECTED:
                statusText = "未连接";
                break;
            case DeviceStatus::INITIALIZING:
                statusText = "初始化中";
                break;
            case DeviceStatus::BUSY:
                statusText = "忙碌";
                break;
            case DeviceStatus::ERROR:
                statusText = "错误";
                break;
            default:
                statusText = "未知";
                break;
        }
        
        appendResult(QString("%1: %2").arg(deviceName, statusText));
    }
    
    bool systemReady = deviceManager_->isSystemReady();
    appendResult(QString("系统状态: %1").arg(systemReady ? "就绪" : "未就绪"));
}

void DeviceManagerTestWindow::clearResults()
{
    resultText_->clear();
}

void DeviceManagerTestWindow::updateDeviceStatusDisplay()
{
    if (!deviceManager_) {
        return;
    }
    
    auto getStatusText = [](DeviceStatus status) -> QString {
        switch (status) {
            case DeviceStatus::CONNECTED: return "✅ 已连接";
            case DeviceStatus::DISCONNECTED: return "❌ 未连接";
            case DeviceStatus::INITIALIZING: return "🔄 初始化中";
            case DeviceStatus::BUSY: return "⚡ 忙碌";
            case DeviceStatus::ERROR: return "⚠️ 错误";
            default: return "❓ 未知";
        }
    };
    
    aoStatusLabel_->setText("AO (JY5711): " + getStatusText(deviceManager_->getDeviceStatus("JY5711")));
    daq5322StatusLabel_->setText("DAQ5322: " + getStatusText(deviceManager_->getDeviceStatus("JY5322")));
    daq5323StatusLabel_->setText("DAQ5323: " + getStatusText(deviceManager_->getDeviceStatus("JY5323")));
    dmmStatusLabel_->setText("DMM (JY8902): " + getStatusText(deviceManager_->getDeviceStatus("JY8902")));
}

void DeviceManagerTestWindow::appendResult(const QString& result)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    resultText_->append(QString("[%1] %2").arg(timestamp, result));
    
    // 自动滚动到底部
    QTextCursor cursor = resultText_->textCursor();
    cursor.movePosition(QTextCursor::End);
    resultText_->setTextCursor(cursor);
}

void DeviceManagerTestWindow::onDeviceStatusChanged(const QString& device, DeviceStatus status)
{
    Q_UNUSED(status)
    appendResult(QString("设备状态变化: %1").arg(device));
    updateDeviceStatusDisplay();
}

void DeviceManagerTestWindow::onErrorOccurred(const QString& error)
{
    appendResult("设备错误: " + error);
}

