# PCB故障检测系统 - 硬件集成与设备管理

## 硬件设备概述

PCB故障检测系统集成了多种专业测试设备，通过协调这些设备的工作来实现对PCB上电子元件的全面检测。

### 支持的硬件设备

#### 1. JY5711 - 任意波形/函数发生器 (AO设备)
- **厂商**: 聚源科技
- **型号**: JY5711
- **功能**: 模拟输出，信号激励源
- **主要规格**:
  - 输出电压范围: ±10V
  - 最大采样率: 100MS/s
  - 输出阻抗: 50Ω
  - 波形内存: 8M采样点
  - 支持任意波形输出

#### 2. JY5322 - 高速数据采集卡 (主DAQ设备)
- **厂商**: 聚源科技
- **型号**: JY5322
- **功能**: 高精度数据采集
- **主要规格**:
  - 输入通道: 8路差分
  - 采样率: 10MS/s
  - 分辨率: 16位
  - 输入范围: ±10V
  - 输入阻抗: 1MΩ

#### 3. JY5323 - 高速数据采集卡 (辅助DAQ设备)
- **厂商**: 聚源科技
- **型号**: JY5323
- **功能**: 扩展数据采集通道
- **主要规格**:
  - 输入通道: 8路差分
  - 采样率: 10MS/s
  - 分辨率: 16位
  - 输入范围: ±10V
  - 同步触发支持

#### 4. JY8902 - 高精度数字万用表 (DMM设备)
- **厂商**: 聚源科技
- **型号**: JY8902
- **功能**: 精密测量
- **主要规格**:
  - 直流电压: 0.1mV - 1000V
  - 直流电流: 0.1μA - 10A
  - 电阻: 0.1Ω - 100MΩ
  - 电容: 1pF - 100mF
  - 频率: 0.1Hz - 1MHz

## 设备驱动架构

### 1. 设备抽象层

```cpp
// 设备基础接口
class IDevice {
public:
    virtual ~IDevice() = default;
    
    // 基本设备操作
    virtual bool initialize() = 0;
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual QString getDeviceInfo() const = 0;
    virtual QString getLastError() const = 0;
    
    // 设备配置
    virtual bool configure(const DeviceConfig& config) = 0;
    virtual DeviceConfig getCurrentConfig() const = 0;
    
    // 设备状态
    virtual DeviceStatus getStatus() const = 0;
    virtual bool selfTest() = 0;
    virtual void reset() = 0;
};

// 设备配置基类
struct DeviceConfig {
    QString deviceName;
    QString serialNumber;
    QMap<QString, QVariant> parameters;
    
    virtual bool isValid() const { return !deviceName.isEmpty(); }
    virtual QJsonObject toJson() const;
    virtual void fromJson(const QJsonObject& json);
};

// 设备状态枚举
enum class DeviceStatus {
    Unknown,
    Disconnected,
    Connected,
    Configured, 
    Ready,
    Running,
    Error,
    Busy
};
```

### 2. JY5711 AO设备驱动实现

```cpp
class JY5711Driver : public IDevice {
private:
    // 设备句柄和配置
    void* m_deviceHandle;
    AODeviceConfig m_config;
    DeviceStatus m_status;
    QString m_lastError;
    
    // 输出缓冲区
    QVector<double> m_outputBuffer;
    QMutex m_bufferMutex;
    
    // 硬件相关函数指针
    typedef int (*AO_OpenDevice)(int deviceIndex);
    typedef int (*AO_CloseDevice)(int deviceHandle);
    typedef int (*AO_ConfigOutput)(int handle, const AOConfig* config);
    typedef int (*AO_StartOutput)(int handle);
    typedef int (*AO_StopOutput)(int handle);
    typedef int (*AO_WriteData)(int handle, double* data, int length);
    
    // 动态库函数指针
    AO_OpenDevice m_openDevice;
    AO_CloseDevice m_closeDevice;
    AO_ConfigOutput m_configOutput;
    AO_StartOutput m_startOutput;
    AO_StopOutput m_stopOutput;
    AO_WriteData m_writeData;

public:
    JY5711Driver();
    ~JY5711Driver();
    
    // IDevice接口实现
    bool initialize() override;
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override { return m_status >= DeviceStatus::Connected; }
    QString getDeviceInfo() const override;
    QString getLastError() const override { return m_lastError; }
    
    bool configure(const DeviceConfig& config) override;
    DeviceConfig getCurrentConfig() const override;
    DeviceStatus getStatus() const override { return m_status; }
    bool selfTest() override;
    void reset() override;
    
    // AO设备特有功能
    bool setOutputVoltage(double voltage);
    bool startContinuousOutput();
    bool stopOutput();
    bool writeWaveform(const QVector<double>& waveform);
    
    // 波形生成工具
    QVector<double> generateSine(double freq, double amp, int samples);
    QVector<double> generateSquare(double freq, double amp, int samples);
    QVector<double> generateTriangle(double freq, double amp, int samples);
    QVector<double> generateDC(double voltage, int samples);

private:
    bool loadDriverLibrary();
    void unloadDriverLibrary();
    bool validateConfig(const AODeviceConfig& config);
    void setError(const QString& error);
};

// AO设备配置结构
struct AODeviceConfig : public DeviceConfig {
    double outputRange;      // 输出范围 (V)
    double sampleRate;       // 采样率 (S/s)
    int bufferSize;          // 缓冲区大小
    bool enableTrigger;      // 是否启用触发
    QString triggerSource;   // 触发源
    
    bool isValid() const override {
        return DeviceConfig::isValid() && 
               outputRange > 0 && 
               sampleRate > 0 && 
               bufferSize > 0;
    }
};
```

### 3. JY5322/JY5323 DAQ设备驱动实现

```cpp
class JY532xDriver : public IDevice {
private:
    void* m_deviceHandle;
    DAQDeviceConfig m_config;
    DeviceStatus m_status;
    QString m_lastError;
    bool m_isJY5322;  // true for JY5322, false for JY5323
    
    // 数据缓冲区
    QVector<QVector<double>> m_inputBuffers;
    QMutex m_bufferMutex;
    QAtomicBool m_isAcquiring;
    
    // 触发设置
    QString m_triggerSource;
    double m_triggerLevel;
    bool m_triggerEnabled;
    
    // 硬件API函数指针
    typedef int (*DAQ_OpenDevice)(int deviceIndex);
    typedef int (*DAQ_CloseDevice)(int handle);
    typedef int (*DAQ_ConfigChannels)(int handle, const ChannelConfig* config);
    typedef int (*DAQ_ConfigTiming)(int handle, double sampleRate, int samplesPerChannel);
    typedef int (*DAQ_ConfigTrigger)(int handle, const TriggerConfig* config);
    typedef int (*DAQ_StartAcquisition)(int handle);
    typedef int (*DAQ_StopAcquisition)(int handle);
    typedef int (*DAQ_ReadData)(int handle, double* buffer, int bufferSize, int* samplesRead);

public:
    JY532xDriver(bool isJY5322 = true);
    ~JY532xDriver();
    
    // IDevice接口实现
    bool initialize() override;
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override;
    QString getDeviceInfo() const override;
    QString getLastError() const override;
    
    bool configure(const DeviceConfig& config) override;
    DeviceConfig getCurrentConfig() const override;
    DeviceStatus getStatus() const override;
    bool selfTest() override;
    void reset() override;
    
    // DAQ设备特有功能
    bool startAcquisition();
    bool stopAcquisition();
    bool isAcquiring() const { return m_isAcquiring.load(); }
    
    // 数据读取
    QVector<double> getChannelData(int channel);
    QVector<QVector<double>> getAllChannelData();
    bool readSingleSample(QVector<double>& samples);
    
    // 触发控制
    bool configureTrigger(const QString& source, double level);
    bool enableTrigger(bool enable);
    bool waitForTrigger(int timeoutMs = 5000);
    
    // 通道配置
    bool configureChannels(const QVector<int>& channels, double inputRange);
    bool enableChannel(int channel, bool enable);
    
    // 同步功能
    bool synchronizeWith(JY532xDriver* otherDevice);
    bool startSynchronizedAcquisition();

private:
    bool loadDriverLibrary();
    void processAcquiredData();
    bool validateChannelConfig(const QVector<int>& channels);
    void setError(const QString& error);
};

// DAQ设备配置结构
struct DAQDeviceConfig : public DeviceConfig {
    QVector<int> enabledChannels;   // 启用的通道
    double inputRange;              // 输入范围 (V)
    double sampleRate;              // 采样率 (S/s)
    int samplesPerChannel;          // 每通道采样数
    QString triggerSource;          // 触发源
    double triggerLevel;            // 触发电平
    bool continuousMode;            // 连续模式
    
    bool isValid() const override {
        return DeviceConfig::isValid() &&
               !enabledChannels.isEmpty() &&
               inputRange > 0 &&
               sampleRate > 0 &&
               samplesPerChannel > 0;
    }
};
```

### 4. JY8902 DMM设备驱动实现

```cpp
class JY8902Driver : public IDevice {
public:
    enum MeasureFunction {
        DC_VOLTAGE,
        AC_VOLTAGE, 
        DC_CURRENT,
        AC_CURRENT,
        RESISTANCE_2WIRE,
        RESISTANCE_4WIRE,
        CAPACITANCE,
        INDUCTANCE,
        FREQUENCY,
        PERIOD,
        DIODE_TEST,
        CONTINUITY
    };
    
    enum MeasureRange {
        AUTO_RANGE = -1,
        RANGE_100mV = 0,
        RANGE_1V = 1,
        RANGE_10V = 2,
        RANGE_100V = 3,
        RANGE_1000V = 4
    };

private:
    void* m_deviceHandle;
    DMMDeviceConfig m_config;
    DeviceStatus m_status;
    QString m_lastError;
    
    // 测量状态
    MeasureFunction m_currentFunction;
    MeasureRange m_currentRange;
    bool m_autoRangeEnabled;
    
    // 测量结果缓存
    double m_lastMeasurement;
    QDateTime m_lastMeasurementTime;
    QVector<double> m_measurementHistory;
    
    // 硬件API函数指针
    typedef int (*DMM_OpenDevice)(const char* deviceName);
    typedef int (*DMM_CloseDevice)(int handle);
    typedef int (*DMM_ConfigFunction)(int handle, int function);
    typedef int (*DMM_ConfigRange)(int handle, int range);
    typedef int (*DMM_SetAutoRange)(int handle, bool enable);
    typedef int (*DMM_TriggerMeasurement)(int handle);
    typedef int (*DMM_ReadMeasurement)(int handle, double* value);
    typedef int (*DMM_GetMeasurementStatus)(int handle, int* status);

public:
    JY8902Driver();
    ~JY8902Driver();
    
    // IDevice接口实现
    bool initialize() override;
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override;
    QString getDeviceInfo() const override;
    QString getLastError() const override;
    
    bool configure(const DeviceConfig& config) override;
    DeviceConfig getCurrentConfig() const override;
    DeviceStatus getStatus() const override;
    bool selfTest() override;
    void reset() override;
    
    // DMM设备特有功能
    bool setMeasureFunction(MeasureFunction function);
    bool setMeasureRange(MeasureRange range);
    bool setAutoRange(bool enable);
    
    // 测量操作
    double takeSingleMeasurement();
    QVector<double> takeMultipleMeasurements(int count);
    bool startContinuousMeasurement();
    bool stopContinuousMeasurement();
    
    // 特殊测量功能
    double measureResistance(bool fourWire = false);
    double measureCapacitance();
    double measureInductance(); 
    bool testDiode(double& forwardVoltage);
    bool testContinuity(double threshold = 100.0);
    
    // 统计功能
    double getLastMeasurement() const { return m_lastMeasurement; }
    QVector<double> getMeasurementHistory() const { return m_measurementHistory; }
    double getMean() const;
    double getStandardDeviation() const;
    double getMinValue() const;
    double getMaxValue() const;

private:
    bool loadDriverLibrary();
    bool waitForMeasurementComplete(int timeoutMs = 5000);
    QString functionToString(MeasureFunction function) const;
    QString rangeToString(MeasureRange range) const;
    void addToHistory(double value);
    void setError(const QString& error);
};

// DMM设备配置结构
struct DMMDeviceConfig : public DeviceConfig {
    MeasureFunction defaultFunction;
    MeasureRange defaultRange;
    bool autoRangeEnabled;
    int measurementAveraging;
    double integrationTime;
    bool highAccuracyMode;
    
    bool isValid() const override {
        return DeviceConfig::isValid() &&
               measurementAveraging > 0 &&
               integrationTime > 0;
    }
};
```

## 设备管理器实现

### 1. 设备管理器核心类

```cpp
class DeviceManager : public QObject {
    Q_OBJECT

public:
    enum DeviceType {
        AO_DEVICE = 0,
        DAQ_DEVICE_PRIMARY = 1,
        DAQ_DEVICE_SECONDARY = 2,
        DMM_DEVICE = 3
    };

private:
    // 设备实例
    QMap<DeviceType, std::unique_ptr<IDevice>> m_devices;
    QMap<DeviceType, DeviceThread*> m_deviceThreads;
    
    // 设备状态
    QMap<DeviceType, DeviceStatus> m_deviceStatus;
    QMutex m_statusMutex;
    
    // 同步控制
    DeviceSynchronizer* m_synchronizer;
    QAtomicBool m_syncMode;
    
    // 配置管理
    ConfigManager* m_configManager;
    QMap<DeviceType, DeviceConfig> m_deviceConfigs;

public:
    DeviceManager(QObject* parent = nullptr);
    ~DeviceManager();
    
    // 设备生命周期管理
    bool initializeAllDevices();
    bool connectAllDevices();
    bool disconnectAllDevices();
    void shutdownAllDevices();
    
    // 单设备操作
    bool initializeDevice(DeviceType type);
    bool connectDevice(DeviceType type);
    bool disconnectDevice(DeviceType type);
    bool configureDevice(DeviceType type, const DeviceConfig& config);
    
    // 设备状态查询
    DeviceStatus getDeviceStatus(DeviceType type) const;
    bool isDeviceConnected(DeviceType type) const;
    bool areAllDevicesReady() const;
    QString getDeviceInfo(DeviceType type) const;
    QString getDeviceError(DeviceType type) const;
    
    // 设备访问
    template<typename T>
    T* getDevice(DeviceType type) {
        auto it = m_devices.find(type);
        if (it != m_devices.end()) {
            return dynamic_cast<T*>(it->get());
        }
        return nullptr;
    }
    
    // 同步控制
    void enableSynchronization(bool enable);
    bool isSynchronizationEnabled() const;
    bool synchronizeDevices();
    
    // 测试操作
    bool startMeasurement(const MeasurementConfig& config);
    bool stopMeasurement();
    MeasurementResults getMeasurementResults();
    
    // 设备发现
    QStringList scanAvailableDevices();
    bool validateDeviceConfiguration();

public slots:
    void onDeviceStatusChanged(DeviceType type, DeviceStatus status);
    void onDeviceError(DeviceType type, const QString& error);
    void onMeasurementComplete(const MeasurementResults& results);

signals:
    void deviceStatusChanged(DeviceType type, DeviceStatus status);
    void deviceError(DeviceType type, const QString& error);
    void allDevicesReady();
    void measurementComplete(const MeasurementResults& results);
    void synchronizationStatusChanged(bool enabled);

private:
    bool createDeviceInstances();
    bool loadDeviceConfigurations();
    void setupDeviceConnections();
    void cleanupDevices();
};
```

### 2. 设备线程管理

```cpp
class DeviceThread : public QThread {
    Q_OBJECT

protected:
    IDevice* m_device;                  // 设备实例
    DeviceManager::DeviceType m_type;  // 设备类型
    ThreadSafeQueue<DeviceCommand> m_commandQueue; // 命令队列
    QAtomicBool m_running;             // 运行标志
    QMutex m_deviceMutex;              // 设备访问互斥锁

public:
    DeviceThread(IDevice* device, DeviceManager::DeviceType type, QObject* parent = nullptr);
    ~DeviceThread();
    
    // 线程控制
    void start() override;
    void stop();
    bool isRunning() const { return m_running.load(); }
    
    // 命令管理
    void addCommand(const DeviceCommand& command);
    void clearCommands();
    int getCommandQueueSize() const;
    
    // 设备访问
    IDevice* getDevice() const { return m_device; }
    DeviceManager::DeviceType getDeviceType() const { return m_type; }

protected:
    void run() override;
    virtual void processCommand(const DeviceCommand& command);
    virtual void handleDeviceError(const QString& error);

signals:
    void commandCompleted(const DeviceCommand& command, bool success);
    void deviceStatusChanged(DeviceManager::DeviceType type, DeviceStatus status);
    void deviceError(DeviceManager::DeviceType type, const QString& error);
    void measurementDataReady(const QByteArray& data);

private slots:
    void onDeviceStatusChanged();
};

// 设备命令结构
struct DeviceCommand {
    enum CommandType {
        CONNECT,
        DISCONNECT,
        CONFIGURE,
        START_OPERATION,
        STOP_OPERATION,
        READ_DATA,
        WRITE_DATA,
        RESET,
        SELF_TEST
    };
    
    CommandType type;
    QVariantMap parameters;
    QString commandId;
    QDateTime timestamp;
    
    DeviceCommand(CommandType t = CONNECT) : type(t), timestamp(QDateTime::currentDateTime()) {
        commandId = QUuid::createUuid().toString();
    }
};
```

### 3. 设备同步器

```cpp
class DeviceSynchronizer : public QObject {
    Q_OBJECT

private:
    QList<IDevice*> m_syncDevices;      // 同步设备列表
    QMutex m_syncMutex;                 // 同步互斥锁
    QWaitCondition m_syncCondition;     // 同步条件变量
    QAtomicInt m_readyCount;            // 就绪设备计数
    QAtomicBool m_syncEnabled;          // 同步启用标志
    int m_totalDevices;                 // 总设备数
    int m_syncTimeout;                  // 同步超时时间

public:
    DeviceSynchronizer(QObject* parent = nullptr);
    
    // 设备管理
    void addDevice(IDevice* device);
    void removeDevice(IDevice* device);
    void clearDevices();
    int getDeviceCount() const { return m_syncDevices.size(); }
    
    // 同步控制  
    void enableSync(bool enable);
    bool isSyncEnabled() const { return m_syncEnabled.load(); }
    void setSyncTimeout(int timeoutMs) { m_syncTimeout = timeoutMs; }
    
    // 同步操作
    bool synchronizeDevices();
    bool waitForAllReady(int timeoutMs = -1);
    void resetSync();
    
    // 状态查询
    int getReadyDeviceCount() const { return m_readyCount.load(); }
    bool areAllDevicesReady() const { return m_readyCount.load() == m_totalDevices; }

public slots:
    void onDeviceReady();
    void onDeviceNotReady();

signals:
    void allDevicesReady();
    void syncTimeout();
    void syncStatusChanged(bool enabled);

private:
    void updateReadyCount();
    void checkSyncCondition();
};
```

## 测量配置和结果处理

### 1. 测量配置

```cpp
struct MeasurementConfig {
    // 基本测量参数
    ComponentSpec::ComponentType componentType;
    QString componentName;
    QString testPosition;
    
    // 信号参数
    double testVoltage;         // 测试电压 (V)
    double testCurrent;         // 测试电流 (A)  
    double testFrequency;       // 测试频率 (Hz)
    QString signalType;         // 信号类型 (sine, square, triangle, dc)
    
    // 采样参数
    double sampleRate;          // 采样率 (S/s)
    int samplesPerChannel;      // 每通道采样数
    double measurementTime;     // 测量时间 (s)
    int averageCount;           // 平均次数
    
    // 触发设置
    bool triggerEnabled;        // 触发使能
    QString triggerSource;      // 触发源
    double triggerLevel;        // 触发电平
    
    // 通道映射
    QMap<QString, int> channelMapping; // 通道映射表
    
    bool isValid() const {
        return !componentName.isEmpty() &&
               testVoltage >= 0 &&
               sampleRate > 0 &&
               samplesPerChannel > 0 &&
               measurementTime > 0;
    }
    
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
};
```

### 2. 测量结果

```cpp
class MeasurementResults {
private:
    QString m_componentName;
    ComponentSpec::ComponentType m_componentType;
    QDateTime m_timestamp;
    
    // 原始数据
    QMap<QString, QVector<double>> m_rawData;  // 原始采样数据
    QVector<double> m_timeAxis;                // 时间轴
    double m_sampleRate;                       // 采样率
    
    // 计算结果
    QMap<QString, double> m_measuredValues;    // 测量值
    QMap<QString, double> m_statistics;        // 统计值
    
    // 频域分析结果
    QMap<QString, QVector<double>> m_frequencyData; // 频域数据
    QMap<QString, double> m_frequencyStats;         // 频域统计

public:
    MeasurementResults(const QString& componentName, ComponentSpec::ComponentType type);
    
    // 数据设置
    void setRawData(const QString& channel, const QVector<double>& data);
    void setTimeAxis(const QVector<double>& timeAxis);
    void setSampleRate(double rate);
    
    // 计算值设置
    void setMeasuredValue(const QString& parameter, double value);
    void setStatistic(const QString& stat, double value);
    
    // 数据获取
    QVector<double> getRawData(const QString& channel) const;
    QVector<double> getTimeAxis() const { return m_timeAxis; }
    double getSampleRate() const { return m_sampleRate; }
    
    double getMeasuredValue(const QString& parameter) const;
    double getStatistic(const QString& stat) const;
    QMap<QString, double> getAllMeasuredValues() const { return m_measuredValues; }
    
    // 数据分析
    void performBasicAnalysis();          // 基本统计分析
    void performFrequencyAnalysis();      // 频域分析
    void performComponentAnalysis();      // 元件特征分析
    
    // 结果验证
    bool isValid() const;
    QStringList getValidationErrors() const;
    
    // 序列化
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
    
    // 导出功能
    bool exportToCSV(const QString& filename) const;
    bool exportToMatlab(const QString& filename) const;

private:
    void calculateStatistics(const QString& channel);
    void performFFT(const QString& channel);
    double calculateRMS(const QVector<double>& data) const;
    double calculateMean(const QVector<double>& data) const;
    double calculateStdDev(const QVector<double>& data) const;
};
```

## 设备校准和验证

### 1. 设备校准

```cpp
class DeviceCalibrator : public QObject {
    Q_OBJECT

public:
    enum CalibrationType {
        VOLTAGE_CALIBRATION,
        CURRENT_CALIBRATION,
        FREQUENCY_CALIBRATION,
        PHASE_CALIBRATION,
        FULL_CALIBRATION
    };
    
    struct CalibrationPoint {
        double referenceValue;  // 参考值
        double measuredValue;   // 测量值
        double error;          // 误差
        double uncertainty;    // 不确定度
        QDateTime timestamp;   // 时间戳
    };
    
    struct CalibrationResult {
        CalibrationType type;
        QVector<CalibrationPoint> points;
        double linearityError;
        double accuracy;
        bool passed;
        QString report;
    };

private:
    DeviceManager* m_deviceManager;
    QMap<DeviceManager::DeviceType, CalibrationResult> m_calibrationResults;

public:
    DeviceCalibrator(DeviceManager* manager, QObject* parent = nullptr);
    
    // 校准操作
    bool calibrateDevice(DeviceManager::DeviceType type, CalibrationType calType);
    bool calibrateAllDevices();
    
    // 验证操作
    bool verifyDeviceAccuracy(DeviceManager::DeviceType type);
    bool verifySystemAccuracy();
    
    // 校准结果
    CalibrationResult getCalibrationResult(DeviceManager::DeviceType type) const;
    bool isDeviceCalibrated(DeviceManager::DeviceType type) const;
    QDateTime getLastCalibrationDate(DeviceManager::DeviceType type) const;
    
    // 校准数据管理
    bool saveCalibrationData(const QString& filename);
    bool loadCalibrationData(const QString& filename);
    void clearCalibrationData();

signals:
    void calibrationStarted(DeviceManager::DeviceType type);
    void calibrationProgress(int percentage);
    void calibrationCompleted(DeviceManager::DeviceType type, bool success);
    void calibrationFailed(DeviceManager::DeviceType type, const QString& error);

private:
    bool performVoltageCalibration(IDevice* device);
    bool performCurrentCalibration(IDevice* device);
    bool performFrequencyCalibration(IDevice* device);
    double calculateLinearity(const QVector<CalibrationPoint>& points);
    double calculateAccuracy(const QVector<CalibrationPoint>& points);
};
```

### 2. 系统诊断

```cpp
class SystemDiagnostic : public QObject {
    Q_OBJECT

public:
    enum DiagnosticTest {
        COMMUNICATION_TEST,     // 通信测试
        FUNCTIONALITY_TEST,     // 功能测试
        ACCURACY_TEST,          // 精度测试
        STABILITY_TEST,         // 稳定性测试
        SYNCHRONIZATION_TEST,   // 同步测试
        FULL_DIAGNOSTIC        // 完整诊断
    };
    
    struct DiagnosticResult {
        DiagnosticTest testType;
        bool passed;
        double score;           // 评分 (0-100)
        QString description;
        QStringList issues;
        QStringList recommendations;
        QDateTime timestamp;
    };

private:
    DeviceManager* m_deviceManager;
    QMap<DiagnosticTest, DiagnosticResult> m_diagnosticResults;

public:
    SystemDiagnostic(DeviceManager* manager, QObject* parent = nullptr);
    
    // 诊断操作
    bool runDiagnostic(DiagnosticTest test);
    bool runFullDiagnostic();
    
    // 结果查询
    DiagnosticResult getDiagnosticResult(DiagnosticTest test) const;
    QMap<DiagnosticTest, DiagnosticResult> getAllResults() const;
    double getOverallScore() const;
    bool isSystemHealthy() const;
    
    // 报告生成
    QString generateDiagnosticReport() const;
    bool saveDiagnosticReport(const QString& filename) const;

signals:
    void diagnosticStarted(DiagnosticTest test);
    void diagnosticProgress(int percentage);
    void diagnosticCompleted(DiagnosticTest test, bool passed);
    void diagnosticFailed(DiagnosticTest test, const QString& error);

private:
    bool testCommunication();
    bool testFunctionality();
    bool testAccuracy();
    bool testStability();
    bool testSynchronization();
    double calculateScore(const DiagnosticResult& result);
};
```

## 错误处理和恢复机制

### 1. 设备错误处理

```cpp
class DeviceErrorHandler : public QObject {
    Q_OBJECT

public:
    enum ErrorType {
        COMMUNICATION_ERROR,
        HARDWARE_ERROR,
        CONFIGURATION_ERROR,
        TIMEOUT_ERROR,
        CALIBRATION_ERROR,
        SYNCHRONIZATION_ERROR
    };
    
    enum ErrorSeverity {
        INFO,
        WARNING, 
        ERROR,
        CRITICAL
    };
    
    struct ErrorInfo {
        ErrorType type;
        ErrorSeverity severity;
        QString message;
        QString device;
        QDateTime timestamp;
        QStringList possibleCauses;
        QStringList suggestedActions;
    };

private:
    QVector<ErrorInfo> m_errorHistory;
    QMap<QString, int> m_errorCounts;
    int m_maxHistorySize;

public:
    DeviceErrorHandler(QObject* parent = nullptr);
    
    // 错误记录
    void reportError(const ErrorInfo& error);
    void reportError(ErrorType type, ErrorSeverity severity, 
                    const QString& message, const QString& device = QString());
    
    // 错误查询
    QVector<ErrorInfo> getRecentErrors(int count = 10) const;
    QVector<ErrorInfo> getErrorsByDevice(const QString& device) const;
    QVector<ErrorInfo> getErrorsByType(ErrorType type) const;
    int getErrorCount(const QString& device = QString()) const;
    
    // 错误分析
    QString getMostCommonError() const;
    QString getMostProblematicDevice() const;
    QStringList getErrorTrends() const;
    
    // 恢复策略
    bool attemptRecovery(const ErrorInfo& error);
    QStringList getSuggestedActions(const ErrorInfo& error) const;
    
    // 错误管理
    void clearErrorHistory();
    void setMaxHistorySize(int size) { m_maxHistorySize = size; }

signals:
    void errorReported(const ErrorInfo& error);
    void criticalErrorOccurred(const ErrorInfo& error);
    void recoveryAttempted(const ErrorInfo& error, bool success);

private:
    QStringList getStandardCauses(ErrorType type) const;
    QStringList getStandardActions(ErrorType type) const;
    bool performAutomaticRecovery(const ErrorInfo& error);
};
```

---

*本文档详细介绍了PCB故障检测系统的硬件集成架构，包括各设备驱动的实现细节、设备管理机制、同步控制方法以及错误处理和恢复策略。*
