# JY5320 DAQ设备增强功能文档

## 概述

本次更新为JY5320系列DAQ设备（JY5322、JY5323等）添加了完整的单点采集和多点采集功能，支持回调方式和直接读取方式获取数据。

## 主要功能特性

### 1. 采集模式
- **单点采集**: 适用于快速单次测量，自动配置和读取
- **多点有限采集**: 固定数量的样本采集，适用于波形捕获
- **多点连续采集**: 持续采集，适用于实时监控

### 2. 数据获取方式
- **直接读取**: 调用函数传入数据Buffer获取
- **回调方式**: 通过信号槽机制自动接收数据
- **定时器驱动**: 自动定期检查和处理缓冲区数据

### 3. 多通道支持
- 同时配置多个通道（最多32个）
- 每通道独立配置输入范围
- 数据按通道分离返回

## API接口说明

### DeviceManager高级接口

#### 1. 单点采集
```cpp
bool readDAQSinglePoint(const QString& deviceName, int channel, double& result, 
                        double sampleRate = 1000.0, double inputRangeMin = -10.0, 
                        double inputRangeMax = 10.0, int timeout_ms = 5000);
```

**参数说明:**
- `deviceName`: 设备名称 ("JY5322", "JY5323"等)
- `channel`: 通道号 (0-31)
- `result`: 输出结果(电压值)
- `sampleRate`: 采样率(Hz)
- `inputRangeMin/Max`: 输入电压范围
- `timeout_ms`: 超时时间

#### 2. 多点采集配置
```cpp
bool configureDAQMultiPoint(const QString& deviceName, const QVector<int>& channels, 
                            double sampleRate, int samplesPerChannel, 
                            double inputRangeMin = -10.0, double inputRangeMax = 10.0,
                            const QString& acquisitionMode = "multi", int timeout_ms = 5000);
```

**参数说明:**
- `channels`: 要启用的通道列表
- `samplesPerChannel`: 每通道采样点数
- `acquisitionMode`: "multi"(有限采样) 或 "continuous"(连续采样)

#### 3. 采集控制
```cpp
bool startDAQAcquisition(const QString& deviceName, bool useCallback = false, int timeout_ms = 5000);
bool stopDAQAcquisition(const QString& deviceName, int timeout_ms = 5000);
```

#### 4. 数据读取
```cpp
bool readDAQMultiPointData(const QString& deviceName, QVector<QVector<double>>& channelData, 
                          int samplesPerChannel = 0, int timeout_ms = 10000);
```

**返回数据格式:**
```cpp
// channelData[通道索引][样本索引] = 电压值
// 例如: channelData[0][100] 表示第0通道的第100个样本
```

### DAQDeviceThread新增信号

```cpp
// 多点数据就绪信号
void multiPointDataReady(const QString& deviceName, const QVector<QVector<double>>& channelData);

// 缓冲区状态更新信号
void dataBufferUpdated(const QString& deviceName, const QVariantMap& bufferInfo);
```

**bufferInfo包含:**
- `availableSamples`: 可用样本数
- `actualSamples`: 实际读取样本数
- `enabledChannels`: 启用的通道列表
- `overrun`: 是否发生数据溢出

## 使用示例

### 1. 简单单点测量
```cpp
DeviceManager manager;
manager.initializeDeviceThreads();

double voltage;
bool success = manager.readDAQSinglePoint("JY5322", 0, voltage, 10000.0);
if (success) {
    qDebug() << "通道0电压:" << voltage << "V";
}
```

### 2. 多通道有限采集
```cpp
// 配置4通道，每通道1000个样本，100KHz采样率
QVector<int> channels = {0, 1, 2, 3};
manager.configureDAQMultiPoint("JY5322", channels, 100000.0, 1000);

// 启动采集
manager.startDAQAcquisition("JY5322", false);

// 等待采集完成
QThread::msleep(100);

// 读取数据
QVector<QVector<double>> data;
manager.readDAQMultiPointData("JY5322", data);

// 处理数据
for (int ch = 0; ch < data.size(); ch++) {
    qDebug() << "通道" << channels[ch] << "数据点数:" << data[ch].size();
    // data[ch][i] 为第ch通道第i个样本
}

// 停止采集
manager.stopDAQAcquisition("JY5322");
```

### 3. 连续采集+回调处理
```cpp
// 配置连续采集
QVector<int> channels = {0, 1};
manager.configureDAQMultiPoint("JY5322", channels, 50000.0, 500, -5.0, 5.0, "continuous");

// 连接回调信号 (需要获取DAQDeviceThread对象)
// connect(daqThread, &DAQDeviceThread::multiPointDataReady,
//         [](const QString& device, const QVector<QVector<double>>& data) {
//             // 处理实时数据
//             qDebug() << "接收到" << data.size() << "通道数据";
//         });

// 启动连续采集(回调模式)
manager.startDAQAcquisition("JY5322", true);

// 连续采集运行中...
QThread::msleep(5000);

// 停止连续采集
manager.stopDAQAcquisition("JY5322");
```

### 4. 低级精细控制
```cpp
// 使用DeviceOperation直接控制
DeviceOperation configOp;
configOp.command = DeviceCommand::CONFIGURE_CHANNEL;
configOp.channels = {0, 1, 2, 3};
configOp.sampleRate = 200000.0;
configOp.samplesPerChannel = 2000;
configOp.inputRangeMin = -10.0;
configOp.inputRangeMax = 10.0;
configOp.acquisitionMode = "multi";

manager.submitOperation("JY5322", configOp);
DeviceResult result = manager.waitForResult("JY5322", 10000);
```

## 技术实现细节

### 1. 线程安全设计
- 所有操作通过线程安全的队列机制
- 使用QMutex保护共享数据
- 支持多线程并发操作

### 2. 内存管理
- 自动管理数据缓冲区
- 防止内存泄漏
- 大数据块的高效传输

### 3. 错误处理
- 完整的错误代码映射
- 详细的错误信息
- 自动重试和恢复机制

### 4. 性能优化
- 最小化数据拷贝
- 缓冲区预分配
- 高效的数据重组算法

## 配置参数建议

### 采样率选择
- **单点测量**: 1KHz - 10KHz (响应速度优先)
- **波形捕获**: 10KHz - 1MHz (根据信号频率选择)
- **连续监控**: 1KHz - 100KHz (平衡性能和数据量)

### 缓冲区大小
- **有限采集**: 100 - 100,000样本/通道
- **连续采集**: 100 - 1,000样本/次读取

### 输入范围
- 根据实际信号幅度选择合适范围
- 过大的范围会降低精度
- 建议范围: ±0.5V, ±1V, ±2V, ±5V, ±10V

## 注意事项

1. **设备初始化**: 确保在使用前调用`initializeDeviceThreads()`
2. **资源释放**: 使用完毕后调用`shutdownDeviceThreads()`
3. **超时设置**: 根据采样数量和速率合理设置超时时间
4. **数据溢出**: 连续采集时注意监控`overrun`标志
5. **线程阻塞**: 大量数据读取可能阻塞调用线程

## 故障排除

### 常见错误
1. **设备未找到**: 检查设备连接和槽位号
2. **配置失败**: 验证参数范围和设备能力
3. **读取超时**: 增加超时时间或减少采样数量
4. **数据溢出**: 增加读取频率或减少采样率

### 调试信息
通过`qDebug()`输出详细的操作日志，包括:
- 配置参数确认
- 实际采样率
- 数据传输状态
- 错误代码和描述

## 兼容性

- **支持设备**: JY5320系列 (JY5322, JY5323, JY5321, JY5324)
- **Qt版本**: Qt 5.12+ 
- **编译器**: MSVC 2017+, GCC 7.0+
- **操作系统**: Windows 10+

## 更新历史

**v1.0 (当前版本)**
- 实现单点和多点采集功能
- 支持回调和直接读取两种方式
- 完整的多通道支持
- 线程安全的设计架构
