# DeviceManager 设备操作完整示例

## 概述

本文档提供了使用 `DeviceManager` 的 `submitOperation` 和 `waitForResult` 方法进行各种设备操作的完整示例。这些方法是系统与硬件设备通信的标准接口。

## 基础数据结构

### DeviceOperation 结构

```cpp
struct DeviceOperation {
    DeviceCommand command;              // 操作命令
    int channel = -1;                   // 通道号
    double value = 0.0;                 // 数值参数
    double sampleRate = 0.0;            // 采样率
    int timeout = 5000;                 // 超时时间(ms)
    bool blocking = true;               // 是否阻塞
    QString deviceName;                 // 设备名称
    QVariantMap parameters;             // 参数映射
    
    // 同步相关
    QString syncGroup;                  // 同步组名
    int syncDelay = 0;                  // 同步延迟(μs)
    
    // 数据采集相关
    int samplesPerChannel = 1000;       // 每通道采样点数
    QVector<int> channels;              // 通道列表
    QString acquisitionMode = "single"; // 采集模式
    double inputRangeMin = -10.0;       // 输入范围最小值
    double inputRangeMax = 10.0;        // 输入范围最大值
    bool useCallback = false;           // 是否使用回调
};
```

### DeviceResult 结构

```cpp
struct DeviceResult {
    bool success = false;               // 操作是否成功
    double value = 0.0;                 // 主要返回值
    QString error;                      // 错误信息
    QVariantMap data;                   // 附加数据
    QDateTime timestamp;                // 时间戳
    DeviceCommand command;              // 执行的命令
};
```

## 1. JY8902 DMM 设备操作示例

### 1.1 基础电阻测量

```cpp
// 配置DMM进行电阻测量
void configureDMMForResistance() {
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    configOp.parameters["range"] = "auto";                    // 自动量程 "auto", "100", "1k", "10k", "100k", "1M", "10M", "100M"
    configOp.parameters["samplesPerTrigger"] = 20;            // 每次触发采样20次
    configOp.parameters["sampleInterval"] = 0.02;             // 采样间隔20ms
    configOp.parameters["useNPLC"] = true;                    // 使用NPLC模式
    configOp.parameters["nplcValue"] = 3;                     // NPLC值
    configOp.parameters["triggerDelay"] = 10;                 // 触发延迟10ms
    configOp.parameters["bufferSize"] = 1000;                 // 缓冲区大小
    configOp.timeout = 10000;
    
    if (!deviceManager_->submitOperation("JY8902", configOp)) {
        qDebug() << "Failed to submit DMM configuration";
        return;
    }
    
    DeviceResult configResult = deviceManager_->waitForResult("JY8902", 10000);
    if (!configResult.success) {
        qDebug() << "DMM configuration failed:" << configResult.error;
        return;
    }
    
    qDebug() << "DMM configured successfully:" 
             << "Range:" << configResult.data["range"].toInt()
             << "SamplesPerTrigger:" << configResult.data["samplesPerTrigger"].toInt();
}

// 启动连续电阻测量
void startContinuousResistanceMeasurement() {
    DeviceOperation startOp(DeviceCommand::START_MEASUREMENT);
    startOp.timeout = 10000;
    
    if (!deviceManager_->submitOperation("JY8902", startOp)) {
        qDebug() << "Failed to submit DMM start operation";
        return;
    }
    
    DeviceResult startResult = deviceManager_->waitForResult("JY8902", 10000);
    if (!startResult.success) {
        qDebug() << "DMM start failed:" << startResult.error;
        return;
    }
    
    qDebug() << "DMM measurement started successfully";
}

// 读取电阻测量数据
void readResistanceData() {
    DeviceOperation readOp(DeviceCommand::READ_DATA);
    readOp.timeout = 5000;
    
    if (!deviceManager_->submitOperation("JY8902", readOp)) {
        qDebug() << "Failed to submit DMM read operation";
        return;
    }
    
    DeviceResult readResult = deviceManager_->waitForResult("JY8902", 5000);
    if (!readResult.success) {
        qDebug() << "DMM read failed:" << readResult.error;
        return;
    }
    
    // 解析测量结果
    QVector<double> measurements = readResult.data["measurements"].value<QVector<double>>();
    double averageValue = readResult.value;
    double minValue = readResult.data["minimum"].toDouble();
    double maxValue = readResult.data["maximum"].toDouble();
    
    qDebug() << "Resistance measurement results:"
             << "Average:" << averageValue << "Ω"
             << "Min:" << minValue << "Ω"
             << "Max:" << maxValue << "Ω"
             << "Sample count:" << measurements.size();
}

// 停止电阻测量
void stopResistanceMeasurement() {
    DeviceOperation stopOp(DeviceCommand::STOP_MEASUREMENT);
    stopOp.timeout = 5000;
    
    if (!deviceManager_->submitOperation("JY8902", stopOp)) {
        qDebug() << "Failed to submit DMM stop operation";
        return;
    }
    
    DeviceResult stopResult = deviceManager_->waitForResult("JY8902", 5000);
    if (stopResult.success) {
        qDebug() << "DMM measurement stopped successfully";
    } else {
        qDebug() << "DMM stop failed:" << stopResult.error;
    }
}
```

### 1.2 完整的电阻测量流程

```cpp
void performCompleteResistanceMeasurement() {
    // 1. 配置DMM
    configureDMMForResistance();
    
    // 2. 启动测量
    startContinuousResistanceMeasurement();
    
    // 3. 等待测量稳定
    QThread::msleep(100);
    
    // 4. 发送软件触发
    DeviceOperation triggerOp(DeviceCommand::SYNC_TRIGGER);
    triggerOp.timeout = 5000;
    
    if (deviceManager_->submitOperation("JY8902", triggerOp)) {
        DeviceResult triggerResult = deviceManager_->waitForResult("JY8902", 5000);
        if (triggerResult.success) {
            qDebug() << "DMM trigger sent successfully";
        }
    }
    
    // 5. 等待数据采集完成
    QThread::msleep(500);
    
    // 6. 读取数据
    readResistanceData();
    
    // 7. 停止测量
    stopResistanceMeasurement();
}
```

## 2. JY5322/JY5323 DAQ 设备操作示例

### 2.1 单点采集

```cpp
void performSinglePointAcquisition() {
    // 配置单点采集
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    configOp.parameters["mode"] = "single";
    configOp.channels = {0, 1, 2, 3};        // 采集通道0-3
    configOp.inputRangeMin = -10.0;           // 输入范围 -10V 到 +10V
    configOp.inputRangeMax = 10.0;
    configOp.timeout = 10000;
    
    if (!deviceManager_->submitOperation("JY5322", configOp)) {
        qDebug() << "Failed to submit DAQ configuration";
        return;
    }
    
    DeviceResult configResult = deviceManager_->waitForResult("JY5322", 10000);
    if (!configResult.success) {
        qDebug() << "DAQ configuration failed:" << configResult.error;
        return;
    }
    
    // 执行单点采集
    DeviceOperation readOp(DeviceCommand::READ_DATA);
    readOp.channel = 0;                       // 读取指定通道
    readOp.timeout = 5000;
    
    if (!deviceManager_->submitOperation("JY5322", readOp)) {
        qDebug() << "Failed to submit DAQ read operation";
        return;
    }
    
    DeviceResult readResult = deviceManager_->waitForResult("JY5322", 5000);
    if (readResult.success) {
        double voltage = readResult.value;
        qDebug() << "Single point acquisition result:"
                 << "Channel 0 voltage:" << voltage << "V";
    } else {
        qDebug() << "DAQ read failed:" << readResult.error;
    }
}

// 多通道单点采集
void performMultiChannelSinglePoint() {
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    configOp.parameters["mode"] = "single";
    configOp.channels = {0, 1, 2, 3};
    configOp.inputRangeMin = -10.0;
    configOp.inputRangeMax = 10.0;
    
    deviceManager_->submitOperation("JY5322", configOp);
    DeviceResult configResult = deviceManager_->waitForResult("JY5322", 10000);
    
    if (configResult.success) {
        DeviceOperation readOp(DeviceCommand::READ_DATA);
        readOp.timeout = 5000;
        
        deviceManager_->submitOperation("JY5322", readOp);
        DeviceResult readResult = deviceManager_->waitForResult("JY5322", 5000);
        
        if (readResult.success) {
            QVariantMap channelData = readResult.data;
            qDebug() << "Multi-channel results:";
            for (auto it = channelData.begin(); it != channelData.end(); ++it) {
                qDebug() << it.key() << ":" << it.value().toDouble() << "V";
            }
        }
    }
}
```

### 2.2 多点采集 (Finite Mode)

```cpp
void performMultiPointAcquisition() {
    // 配置多点采集
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    configOp.parameters["mode"] = "multi";
    configOp.channels = {0, 1};                 // 采集通道0和1
    configOp.sampleRate = 10000.0;              // 采样率10kHz
    configOp.samplesPerChannel = 1000;          // 每通道1000个样本
    configOp.inputRangeMin = -10.0;
    configOp.inputRangeMax = 10.0;
    configOp.timeout = 10000;
    
    if (!deviceManager_->submitOperation("JY5322", configOp)) {
        qDebug() << "Failed to submit DAQ multi-point configuration";
        return;
    }
    
    DeviceResult configResult = deviceManager_->waitForResult("JY5322", 10000);
    if (!configResult.success) {
        qDebug() << "DAQ multi-point configuration failed:" << configResult.error;
        return;
    }
    
    qDebug() << "Multi-point acquisition configured:"
             << "Actual sample rate:" << configResult.data["actualSampleRate"].toDouble()
             << "Total samples:" << configResult.data["totalSamples"].toInt();
    
    // 启动采集
    DeviceOperation startOp(DeviceCommand::START_MEASUREMENT);
    startOp.timeout = 10000;
    
    deviceManager_->submitOperation("JY5322", startOp);
    DeviceResult startResult = deviceManager_->waitForResult("JY5322", 10000);
    
    if (!startResult.success) {
        qDebug() << "Failed to start multi-point acquisition:" << startResult.error;
        return;
    }
    
    // 等待采集完成
    QThread::msleep(200);  // 等待足够的时间完成采集
    
    // 读取数据
    DeviceOperation readOp(DeviceCommand::READ_DATA);
    readOp.timeout = 10000;
    
    deviceManager_->submitOperation("JY5322", readOp);
    DeviceResult readResult = deviceManager_->waitForResult("JY5322", 10000);
    
    if (readResult.success) {
        QVector<QVector<double>> channelData = 
            readResult.data["channelData"].value<QVector<QVector<double>>>();
        
        qDebug() << "Multi-point acquisition completed:"
                 << "Channels:" << channelData.size()
                 << "Samples per channel:" << (channelData.isEmpty() ? 0 : channelData[0].size());
                 
        // 处理数据...
        for (int ch = 0; ch < channelData.size(); ++ch) {
            double sum = 0.0;
            for (double sample : channelData[ch]) {
                sum += sample;
            }
            double average = sum / channelData[ch].size();
            qDebug() << "Channel" << ch << "average:" << average << "V";
        }
    } else {
        qDebug() << "Failed to read multi-point data:" << readResult.error;
    }
    
    // 停止采集
    DeviceOperation stopOp(DeviceCommand::STOP_MEASUREMENT);
    deviceManager_->submitOperation("JY5322", stopOp);
    deviceManager_->waitForResult("JY5322", 5000);
}
```

### 2.3 连续采集

```cpp
void startContinuousAcquisition() {
    // 配置连续采集
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    configOp.parameters["mode"] = "continuous";
    configOp.channels = {0, 1, 2, 3};
    configOp.sampleRate = 1000.0;               // 采样率1kHz
    configOp.samplesPerChannel = 1000;          // 缓冲区大小
    configOp.inputRangeMin = -5.0;
    configOp.inputRangeMax = 5.0;
    configOp.timeout = 10000;
    
    deviceManager_->submitOperation("JY5322", configOp);
    DeviceResult configResult = deviceManager_->waitForResult("JY5322", 10000);
    
    if (!configResult.success) {
        qDebug() << "Continuous acquisition configuration failed:" << configResult.error;
        return;
    }
    
    // 启动连续采集
    DeviceOperation startOp(DeviceCommand::START_MEASUREMENT);
    startOp.timeout = 10000;
    
    deviceManager_->submitOperation("JY5322", startOp);
    DeviceResult startResult = deviceManager_->waitForResult("JY5322", 10000);
    
    if (startResult.success) {
        qDebug() << "Continuous acquisition started successfully";
    } else {
        qDebug() << "Failed to start continuous acquisition:" << startResult.error;
    }
}

void readContinuousData() {
    DeviceOperation readOp(DeviceCommand::READ_DATA);
    readOp.timeout = 2000;
    
    deviceManager_->submitOperation("JY5322", readOp);
    DeviceResult readResult = deviceManager_->waitForResult("JY5322", 2000);
    
    if (readResult.success) {
        QVector<QVector<double>> data = 
            readResult.data["channelData"].value<QVector<QVector<double>>>();
        
        qDebug() << "Continuous data read:"
                 << "Channels:" << data.size()
                 << "New samples:" << (data.isEmpty() ? 0 : data[0].size());
    } else {
        qDebug() << "Failed to read continuous data:" << readResult.error;
    }
}

void stopContinuousAcquisition() {
    DeviceOperation stopOp(DeviceCommand::STOP_MEASUREMENT);
    stopOp.timeout = 5000;
    
    deviceManager_->submitOperation("JY5322", stopOp);
    DeviceResult stopResult = deviceManager_->waitForResult("JY5322", 5000);
    
    if (stopResult.success) {
        qDebug() << "Continuous acquisition stopped successfully";
    } else {
        qDebug() << "Failed to stop continuous acquisition:" << stopResult.error;
    }
}
```

## 3. JY5711 AO 设备操作示例

### 3.1 波形输出配置

```cpp
void configureAOOutput() {
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    
    // 配置多通道波形输出
    QVariantList waveformConfigs;
    
    // 通道0 - 正弦波
    QVariantMap sineConfig;
    sineConfig["channel"] = 0;
    sineConfig["waveformType"] = static_cast<int>(PXIe5711_testtype::SineWave);
    sineConfig["amplitude"] = 5.0;
    sineConfig["frequency"] = 1000.0;
    sineConfig["lowRange"] = -10.0;
    sineConfig["highRange"] = 10.0;
    waveformConfigs.append(sineConfig);
    
    // 通道1 - 方波
    QVariantMap squareConfig;
    squareConfig["channel"] = 1;
    squareConfig["waveformType"] = static_cast<int>(PXIe5711_testtype::SquareWave);
    squareConfig["amplitude"] = 3.0;
    squareConfig["frequency"] = 500.0;
    squareConfig["dutyCycle"] = 50.0;
    squareConfig["lowRange"] = -5.0;
    squareConfig["highRange"] = 5.0;
    waveformConfigs.append(squareConfig);
    
    configOp.parameters["waveforms"] = waveformConfigs;
    configOp.parameters["sampleRate"] = 1000000.0;      // 1MHz采样率
    configOp.parameters["samplesPerChannel"] = 1000000; // 每通道100万样本
    configOp.timeout = 15000;
    
    if (!deviceManager_->submitOperation("JY5711", configOp)) {
        qDebug() << "Failed to submit AO configuration";
        return;
    }
    
    DeviceResult configResult = deviceManager_->waitForResult("JY5711", 15000);
    if (configResult.success) {
        double actualSampleRate = configResult.data["actualSampleRate"].toDouble();
        qDebug() << "AO configured successfully, actual sample rate:" << actualSampleRate;
    } else {
        qDebug() << "AO configuration failed:" << configResult.error;
    }
}

void outputWaveforms() {
    // 输出波形数据
    DeviceOperation outputOp(DeviceCommand::WRITE_DATA);
    
    // 参数已在配置阶段设置，这里只需要指定输出命令
    outputOp.timeout = 10000;
    
    if (!deviceManager_->submitOperation("JY5711", outputOp)) {
        qDebug() << "Failed to submit AO output operation";
        return;
    }
    
    DeviceResult outputResult = deviceManager_->waitForResult("JY5711", 10000);
    if (outputResult.success) {
        int writtenSamples = outputResult.data["actualWriteSamples"].toInt();
        qDebug() << "Waveform output started, written samples:" << writtenSamples;
    } else {
        qDebug() << "Waveform output failed:" << outputResult.error;
    }
}

void startAOOutput() {
    DeviceOperation startOp(DeviceCommand::START_MEASUREMENT);
    startOp.timeout = 5000;
    
    deviceManager_->submitOperation("JY5711", startOp);
    DeviceResult startResult = deviceManager_->waitForResult("JY5711", 5000);
    
    if (startResult.success) {
        qDebug() << "AO output started successfully";
    } else {
        qDebug() << "Failed to start AO output:" << startResult.error;
    }
}

void stopAOOutput() {
    DeviceOperation stopOp(DeviceCommand::STOP_MEASUREMENT);
    stopOp.timeout = 5000;
    
    deviceManager_->submitOperation("JY5711", stopOp);
    DeviceResult stopResult = deviceManager_->waitForResult("JY5711", 5000);
    
    if (stopResult.success) {
        qDebug() << "AO output stopped successfully";
    } else {
        qDebug() << "Failed to stop AO output:" << stopResult.error;
    }
}
```

### 3.2 简单的电压输出

```cpp
void outputConstantVoltage(double voltage) {
    DeviceOperation configOp(DeviceCommand::CONFIGURE_CHANNEL);
    
    QVariantList waveformConfigs;
    QVariantMap dcConfig;
    dcConfig["channel"] = 0;
    dcConfig["waveformType"] = static_cast<int>(PXIe5711_testtype::StepWave);
    dcConfig["amplitude"] = voltage;
    dcConfig["lowRange"] = -10.0;
    dcConfig["highRange"] = 10.0;
    waveformConfigs.append(dcConfig);
    
    configOp.parameters["waveforms"] = waveformConfigs;
    configOp.parameters["sampleRate"] = 1000.0;
    configOp.parameters["samplesPerChannel"] = 1000;
    configOp.timeout = 10000;
    
    deviceManager_->submitOperation("JY5711", configOp);
    DeviceResult configResult = deviceManager_->waitForResult("JY5711", 10000);
    
    if (configResult.success) {
        // 输出波形
        outputWaveforms();
        
        // 启动输出
        startAOOutput();
        
        qDebug() << "Constant voltage" << voltage << "V output started";
    } else {
        qDebug() << "Failed to configure constant voltage output:" << configResult.error;
    }
}
```

## 4. 同步操作示例

### 4.1 创建同步组进行同步测量

```cpp
void performSynchronizedMeasurement() {
    // 创建同步组
    QStringList devices = {"JY5711", "JY5322", "JY8902"};
    deviceManager_->createSyncGroup("TestGroup", devices);
    
    // 配置AO输出
    DeviceOperation aoConfigOp(DeviceCommand::CONFIGURE_CHANNEL);
    aoConfigOp.syncGroup = "TestGroup";
    // ... 配置AO参数 ...
    
    deviceManager_->submitOperation("JY5711", aoConfigOp);
    deviceManager_->waitForResult("JY5711", 10000);
    
    // 配置DAQ采集
    DeviceOperation daqConfigOp(DeviceCommand::CONFIGURE_CHANNEL);
    daqConfigOp.syncGroup = "TestGroup";
    daqConfigOp.parameters["mode"] = "multi";
    daqConfigOp.channels = {0, 1};
    daqConfigOp.sampleRate = 10000.0;
    daqConfigOp.samplesPerChannel = 1000;
    // ... 其他配置 ...
    
    deviceManager_->submitOperation("JY5322", daqConfigOp);
    deviceManager_->waitForResult("JY5322", 10000);
    
    // 配置DMM
    DeviceOperation dmmConfigOp(DeviceCommand::CONFIGURE_CHANNEL);
    dmmConfigOp.syncGroup = "TestGroup";
    // ... 配置DMM参数 ...
    
    deviceManager_->submitOperation("JY8902", dmmConfigOp);
    deviceManager_->waitForResult("JY8902", 10000);
    
    // 同步启动所有设备
    QList<DeviceOperation> syncOperations;
    
    DeviceOperation aoStart(DeviceCommand::START_MEASUREMENT);
    aoStart.syncGroup = "TestGroup";
    syncOperations.append(aoStart);
    
    DeviceOperation daqStart(DeviceCommand::START_MEASUREMENT);
    daqStart.syncGroup = "TestGroup";
    syncOperations.append(daqStart);
    
    DeviceOperation dmmStart(DeviceCommand::START_MEASUREMENT);
    dmmStart.syncGroup = "TestGroup";
    syncOperations.append(dmmStart);
    
    // 执行同步操作
    bool success = deviceManager_->executeSync("TestGroup", syncOperations, 15000);
    
    if (success) {
        qDebug() << "Synchronized measurement started successfully";
        
        // 等待测量完成
        QThread::msleep(500);
        
        // 读取结果
        // ... 读取各设备数据 ...
        
    } else {
        qDebug() << "Synchronized measurement failed";
    }
    
    // 清理同步组
    deviceManager_->removeSyncGroup("TestGroup");
}
```

## 5. 错误处理和调试

### 5.1 错误处理模式

```cpp
bool performOperationWithErrorHandling(const QString& deviceName, 
                                     const DeviceOperation& operation) {
    // 检查设备状态
    DeviceStatus status = deviceManager_->getDeviceStatus(deviceName);
    if (status != DeviceStatus::CONNECTED) {
        qDebug() << "Device" << deviceName << "not ready, status:" << static_cast<int>(status);
        return false;
    }
    
    // 提交操作
    if (!deviceManager_->submitOperation(deviceName, operation)) {
        qDebug() << "Failed to submit operation to" << deviceName;
        QString lastError = deviceManager_->getLastError();
        if (!lastError.isEmpty()) {
            qDebug() << "Last error:" << lastError;
        }
        return false;
    }
    
    // 等待结果
    DeviceResult result = deviceManager_->waitForResult(deviceName, operation.timeout);
    
    if (result.success) {
        qDebug() << "Operation completed successfully on" << deviceName;
        return true;
    } else {
        qDebug() << "Operation failed on" << deviceName << ":" << result.error;
        
        // 检查是否是超时错误
        if (result.error.contains("timeout", Qt::CaseInsensitive)) {
            qDebug() << "Operation timed out, device may be busy";
        }
        
        return false;
    }
}
```

### 5.2 设备状态监控

```cpp
void monitorDeviceStatus() {
    QStringList connectedDevices = deviceManager_->getConnectedDevices();
    qDebug() << "Connected devices:" << connectedDevices;
    
    for (const QString& device : connectedDevices) {
        DeviceStatus status = deviceManager_->getDeviceStatus(device);
        qDebug() << device << "status:" << static_cast<int>(status);
    }
    
    // 检查系统整体状态
    bool systemReady = deviceManager_->isSystemReady();
    qDebug() << "System ready:" << systemReady;
}
```

## 6. 高级功能示例

### 6.1 使用事件循环等待数据

```cpp
void waitForDataWithEventLoop() {
    // 启动采集
    DeviceOperation startOp(DeviceCommand::START_MEASUREMENT);
    deviceManager_->submitOperation("JY5322", startOp);
    deviceManager_->waitForResult("JY5322", 10000);
    
    // 使用事件循环等待数据
    QVector<QVector<double>> channelData;
    DeviceManager::WaitResult waitResult = deviceManager_->waitForDataWithEventLoop(
        "JY5322", channelData, 50, 100, 5000);
    
    if (waitResult.success) {
        qDebug() << "Data received after" << waitResult.attempts << "attempts";
        qDebug() << "Channels:" << channelData.size();
    } else {
        if (waitResult.timeout) {
            qDebug() << "Timeout waiting for data";
        } else {
            qDebug() << "Error waiting for data:" << waitResult.errorMessage;
        }
    }
}
```

### 6.2 批量操作示例

```cpp
void performBatchOperations() {
    QStringList devices = {"JY5322", "JY8902"};
    
    // 批量初始化设备
    for (const QString& device : devices) {
        DeviceOperation initOp(DeviceCommand::INITIALIZE);
        initOp.timeout = 10000;
        
        if (deviceManager_->submitOperation(device, initOp)) {
            DeviceResult result = deviceManager_->waitForResult(device, 10000);
            qDebug() << device << "initialization:" << result.success;
        }
    }
    
    // 批量配置
    // ...
    
    // 批量启动
    // ...
}
```

## 总结

本文档展示了如何使用 `DeviceManager` 的 `submitOperation` 和 `waitForResult` 方法进行各种设备操作：

1. **DMM设备**: 电阻、电容、电感测量
2. **DAQ设备**: 单点、多点、连续数据采集
3. **AO设备**: 波形输出、电压输出
4. **同步操作**: 多设备协调工作
5. **错误处理**: 状态检查、错误恢复
6. **高级功能**: 事件循环、批量操作

这些示例涵盖了故障检测系统中所有常见的设备操作场景，为开发者提供了完整的参考实现。
