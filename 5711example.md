```cpp
// 使用示例
DeviceManager* deviceManager = new DeviceManager();
deviceManager->initializeDeviceThreads();

// 配置多通道波形输出
DeviceOperation configOp;
configOp.command = DeviceCommand::CONFIGURE_CHANNEL;
configOp.parameters["channelCount"] = 3;
configOp.parameters["sampleRate"] = 1000000.0;

QVariantList waveforms;
// 通道0 - 正弦波
QVariantMap sine;
sine["channel"] = 0;
sine["type"] = static_cast<int>(PXIe5711_testtype::SineWave);
sine["amplitude"] = 5.0;
sine["frequency"] = 1000.0;
sine["lowRange"] = -10.0;
sine["highRange"] = 10.0;
waveforms.append(sine);

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

// 提交配置操作
deviceManager->submitOperation("JY5711", configOp);
DeviceResult configResult = deviceManager->waitForResult("JY5711", 10000);

if (configResult.success) {
    // 配置成功，现在输出波形
    DeviceOperation outputOp;
    outputOp.command = DeviceCommand::WRITE_DATA;
    outputOp.parameters["waveforms"] = waveforms;
    outputOp.parameters["sampleRate"] = 1000000;
    outputOp.parameters["samplesPerChannel"] = 1000000; // 1秒的数据
    
    deviceManager->submitOperation("JY5711", outputOp);
    DeviceResult outputResult = deviceManager->waitForResult("JY5711", 15000);
    
    if (outputResult.success) {
        qDebug() << "Waveform output started successfully";
        
        // 发送软件触发开始输出
        DeviceOperation triggerOp;
        triggerOp.command = DeviceCommand::SYNC_TRIGGER;
        deviceManager->submitOperation("JY5711", triggerOp);
    }
}
```