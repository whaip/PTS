# PCB故障检测系统 - 实现细节与代码结构

## 项目结构

```
d:\FaultDetect\Program\FaultDetect\PTS\
├── PTS.pro                 # QMake项目文件
├── main.cpp               # 应用程序入口
├── mainwindow.h/cpp       # 主窗口类
├── devicemanager.h/cpp    # 设备管理器
├── faultdiagnostic.h/cpp  # 故障诊断引擎
├── testsequencemanager.h/cpp # 测试序列管理
├── resultexporter.h/cpp   # 结果导出器
├── devicethread.h/cpp     # 设备线程基类
├── devices/               # 设备驱动层
│   ├── aodevice.h/cpp    # AO设备驱动
│   ├── daqdevice.h/cpp   # DAQ设备驱动
│   └── dmmdevice.h/cpp   # DMM设备驱动
├── ui/                   # UI相关文件
│   ├── mainwindow.ui     # 主界面UI文件
│   └── dialogs/          # 对话框
├── algorithms/           # 诊断算法
│   ├── resistor_diagnostic.cpp
│   ├── capacitor_diagnostic.cpp
│   ├── inductor_diagnostic.cpp
│   ├── diode_diagnostic.cpp
│   └── ic_diagnostic.cpp
├── utils/               # 工具类
│   ├── config.h/cpp     # 配置管理
│   ├── logger.h/cpp     # 日志系统
│   └── math_utils.h/cpp # 数学工具
└── resources/           # 资源文件
    ├── icons/          # 图标资源
    ├── config/         # 配置文件
    └── translations/   # 国际化文件
```

## 核心数据结构

### 1. ComponentSpec (组件规格)

```cpp
class ComponentSpec {
public:
    enum ComponentType {
        RESISTOR,
        CAPACITOR,
        INDUCTOR,
        DIODE,
        IC
    };

private:
    ComponentType m_type;           // 组件类型
    QString m_name;                 // 组件名称
    QString m_position;             // PCB位置
    QMap<QString, QVariant> m_parameters; // 参数集合

public:
    ComponentSpec(ComponentType type, const QString& name);
    
    // 参数设置
    void setParameter(const QString& key, const QVariant& value);
    QVariant getParameter(const QString& key) const;
    
    // 电阻器特有参数
    void setResistance(double resistance);
    void setTolerance(double tolerance);
    
    // 电容器特有参数
    void setCapacitance(double capacitance);
    void setESRThreshold(double esr);
    
    // 电感器特有参数
    void setInductance(double inductance);
    
    // 二极管特有参数
    void setForwardVoltage(double vf);
    void setReverseVoltage(double vr);
    
    // 通用方法
    ComponentType getType() const { return m_type; }
    QString getName() const { return m_name; }
    QString getPosition() const { return m_position; }
};
```

### 2. MeasurementResult (测量结果)

```cpp
class MeasurementResult {
private:
    QString m_componentName;        // 组件名称
    QDateTime m_timestamp;          // 测量时间戳
    QVector<double> m_voltageData;  // 电压数据
    QVector<double> m_currentData;  // 电流数据
    QVector<double> m_timeAxis;     // 时间轴
    double m_sampleRate;            // 采样率
    QMap<QString, double> m_calculatedValues; // 计算值

public:
    MeasurementResult(const QString& componentName);
    
    // 数据设置
    void setVoltageData(const QVector<double>& voltage);
    void setCurrentData(const QVector<double>& current);
    void setTimeAxis(const QVector<double>& time);
    void setSampleRate(double rate);
    
    // 计算值设置
    void setCalculatedValue(const QString& key, double value);
    double getCalculatedValue(const QString& key) const;
    
    // 数据获取
    const QVector<double>& getVoltageData() const { return m_voltageData; }
    const QVector<double>& getCurrentData() const { return m_currentData; }
    const QVector<double>& getTimeAxis() const { return m_timeAxis; }
    
    // 统计分析
    double getMeanVoltage() const;
    double getMeanCurrent() const;
    double getRMSVoltage() const;
    double getRMSCurrent() const;
    double getPeakToPeakVoltage() const;
    
    // 频域分析
    QVector<double> getFrequencySpectrum() const;
    double getDominantFrequency() const;
    double getTotalHarmonicDistortion() const;
};
```

### 3. DiagnosticResult (诊断结果)

```cpp
class DiagnosticResult {
public:
    enum FaultType {
        NO_FAULT,
        OPEN_CIRCUIT,
        SHORT_CIRCUIT,
        OUT_OF_TOLERANCE,
        HIGH_ESR,
        LEAKAGE_CURRENT,
        FORWARD_VOLTAGE_ABNORMAL,
        REVERSE_LEAKAGE,
        POWER_PIN_FAULT,
        FUNCTIONAL_PIN_FAULT
    };
    
    enum Severity {
        INFO,
        WARNING,
        CRITICAL
    };

private:
    QString m_componentName;        // 组件名称
    FaultType m_faultType;         // 故障类型
    Severity m_severity;           // 严重程度
    QString m_description;         // 故障描述
    double m_confidence;           // 置信度 (0-1)
    QMap<QString, QVariant> m_additionalData; // 附加数据
    QDateTime m_timestamp;         // 诊断时间

public:
    DiagnosticResult(const QString& componentName);
    
    // 故障信息设置
    void setFaultType(FaultType type);
    void setSeverity(Severity severity);
    void setDescription(const QString& description);
    void setConfidence(double confidence);
    void setAdditionalData(const QString& key, const QVariant& value);
    
    // 故障信息获取
    bool hasFault() const { return m_faultType != NO_FAULT; }
    FaultType getFaultType() const { return m_faultType; }
    Severity getSeverity() const { return m_severity; }
    QString getDescription() const { return m_description; }
    double getConfidence() const { return m_confidence; }
    
    // 结果比较
    bool operator<(const DiagnosticResult& other) const;
    bool operator==(const DiagnosticResult& other) const;
    
    // 序列化
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
};
```

### 4. TestSequence (测试序列)

```cpp
class TestSequence : public QObject {
    Q_OBJECT

public:
    enum SequenceState {
        CREATED,
        RUNNING,
        PAUSED,
        COMPLETED,
        ABORTED
    };

private:
    QString m_name;                           // 序列名称
    QList<ComponentSpec> m_components;        // 组件列表
    QList<DiagnosticResult> m_results;       // 诊断结果
    SequenceState m_state;                   // 序列状态
    int m_currentIndex;                      // 当前测试索引
    QDateTime m_startTime;                   // 开始时间
    QDateTime m_endTime;                     // 结束时间
    QMap<QString, QVariant> m_globalSettings; // 全局设置

public:
    TestSequence(const QString& name, QObject* parent = nullptr);
    
    // 序列管理
    void addComponent(const ComponentSpec& spec);
    void removeComponent(int index);
    void clearComponents();
    
    // 执行控制
    void start();
    void pause();
    void resume();
    void stop();
    void reset();
    
    // 状态查询
    SequenceState getState() const { return m_state; }
    int getCurrentIndex() const { return m_currentIndex; }
    int getTotalCount() const { return m_components.size(); }
    int getProgress() const;
    
    // 结果管理
    void addResult(const DiagnosticResult& result);
    QList<DiagnosticResult> getResults() const { return m_results; }
    DiagnosticResult getResult(int index) const;
    
    // 设置管理
    void setGlobalSetting(const QString& key, const QVariant& value);
    QVariant getGlobalSetting(const QString& key) const;

signals:
    void stateChanged(SequenceState newState);
    void progressChanged(int progress);
    void componentTested(int index, const DiagnosticResult& result);
    void sequenceCompleted();
    void sequenceAborted();

private slots:
    void onComponentTestCompleted();
};
```

## 设备驱动架构

### 1. DeviceThread (设备线程基类)

```cpp
class DeviceThread : public QThread {
    Q_OBJECT

public:
    enum DeviceState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        CONFIGURED,
        RUNNING,
        ERROR
    };

    enum DeviceType {
        AO_DEVICE,
        DAQ_DEVICE,
        DMM_DEVICE
    };

protected:
    DeviceType m_deviceType;        // 设备类型
    DeviceState m_state;           // 设备状态
    QString m_deviceName;          // 设备名称
    QString m_serialNumber;        // 序列号
    QMutex m_stateMutex;          // 状态互斥锁
    QQueue<DeviceCommand> m_commandQueue; // 命令队列
    QMutex m_queueMutex;          // 队列互斥锁
    QWaitCondition m_queueCondition; // 队列条件变量

public:
    DeviceThread(DeviceType type, const QString& name, QObject* parent = nullptr);
    virtual ~DeviceThread();
    
    // 设备控制
    virtual bool connectDevice() = 0;
    virtual bool disconnectDevice() = 0;
    virtual bool configureDevice(const QMap<QString, QVariant>& config) = 0;
    virtual bool startOperation() = 0;
    virtual bool stopOperation() = 0;
    
    // 状态查询
    DeviceState getState() const;
    QString getDeviceName() const { return m_deviceName; }
    QString getSerialNumber() const { return m_serialNumber; }
    bool isConnected() const { return m_state >= CONNECTED; }
    bool isRunning() const { return m_state == RUNNING; }
    
    // 命令管理
    void addCommand(const DeviceCommand& command);
    void clearCommands();
    int getCommandQueueSize() const;

protected:
    // 线程主函数
    void run() override;
    
    // 命令处理
    virtual void processCommand(const DeviceCommand& command) = 0;
    
    // 状态管理
    void setState(DeviceState newState);
    
    // 错误处理
    virtual void handleError(const QString& errorMessage);

signals:
    void stateChanged(DeviceState newState);
    void errorOccurred(const QString& errorMessage);
    void dataReady(const QByteArray& data);
    void operationCompleted();

private slots:
    void onCommandQueueChanged();
};
```

### 2. JY5711 AO设备实现

```cpp
class AODevice : public DeviceThread {
    Q_OBJECT

private:
    struct AOConfiguration {
        double outputRange;         // 输出范围 (V)
        double sampleRate;         // 采样率 (Hz)
        int bufferSize;            // 缓冲区大小
        bool continuous;           // 是否连续输出
    };

    AOConfiguration m_config;      // 设备配置
    QVector<double> m_outputBuffer; // 输出缓冲区
    void* m_deviceHandle;          // 设备句柄

public:
    AODevice(const QString& name, QObject* parent = nullptr);
    ~AODevice();
    
    // 设备控制实现
    bool connectDevice() override;
    bool disconnectDevice() override;
    bool configureDevice(const QMap<QString, QVariant>& config) override;
    bool startOperation() override;
    bool stopOperation() override;
    
    // AO特有功能
    bool setOutputVoltage(double voltage);
    bool setOutputWaveform(const QVector<double>& waveform);
    bool enableOutput(bool enable);
    
    // 波形生成
    QVector<double> generateSineWave(double frequency, double amplitude, double duration);
    QVector<double> generateSquareWave(double frequency, double amplitude, double duration);
    QVector<double> generateTriangleWave(double frequency, double amplitude, double duration);

protected:
    void processCommand(const DeviceCommand& command) override;
    void handleError(const QString& errorMessage) override;

private:
    bool initializeHardware();
    void releaseHardware();
    bool configureOutputRange(double range);
    bool configureSampleRate(double rate);
};
```

### 3. JY5322/JY5323 DAQ设备实现

```cpp
class DAQDevice : public DeviceThread {
    Q_OBJECT

private:
    struct DAQConfiguration {
        QVector<int> channels;      // 采集通道
        double inputRange;          // 输入范围 (V)
        double sampleRate;          // 采样率 (Hz)
        int samplesPerChannel;      // 每通道采样点数
        QString triggerSource;      // 触发源
        double triggerLevel;        // 触发电平
    };

    DAQConfiguration m_config;     // 设备配置
    QVector<QVector<double>> m_inputBuffers; // 输入缓冲区
    void* m_deviceHandle;          // 设备句柄
    bool m_isJY5322;              // 是否为JY5322设备

public:
    DAQDevice(const QString& name, bool isJY5322 = true, QObject* parent = nullptr);
    ~DAQDevice();
    
    // 设备控制实现
    bool connectDevice() override;
    bool disconnectDevice() override;
    bool configureDevice(const QMap<QString, QVariant>& config) override;
    bool startOperation() override;
    bool stopOperation() override;
    
    // DAQ特有功能
    bool startAcquisition();
    bool stopAcquisition();
    QVector<double> getChannelData(int channel);
    QVector<QVector<double>> getAllChannelData();
    
    // 触发设置
    bool setTriggerSource(const QString& source);
    bool setTriggerLevel(double level);
    bool enableTrigger(bool enable);
    
    // 采样设置
    bool setSampleRate(double rate);
    bool setSamplesPerChannel(int samples);
    bool setInputRange(double range);

protected:
    void processCommand(const DeviceCommand& command) override;
    void handleError(const QString& errorMessage) override;

private:
    bool initializeHardware();
    void releaseHardware();
    bool configureChannels(const QVector<int>& channels);
    bool configureTiming(double sampleRate, int samples);
    void processAcquiredData();

signals:
    void dataAcquired(const QVector<QVector<double>>& data);
    void acquisitionCompleted();
};
```

### 4. JY8902 DMM设备实现

```cpp
class DMMDevice : public DeviceThread {
    Q_OBJECT

public:
    enum MeasurementType {
        DC_VOLTAGE,
        AC_VOLTAGE,
        DC_CURRENT,
        AC_CURRENT,
        RESISTANCE,
        CAPACITANCE,
        INDUCTANCE,
        FREQUENCY,
        DIODE_TEST,
        CONTINUITY_TEST
    };

private:
    struct DMMConfiguration {
        MeasurementType measurementType; // 测量类型
        double range;                    // 测量量程
        bool autoRange;                  // 自动量程
        int averageCount;                // 平均次数
        double integrationTime;          // 积分时间
    };

    DMMConfiguration m_config;         // 设备配置
    double m_lastMeasurement;          // 最后测量值
    void* m_deviceHandle;              // 设备句柄

public:
    DMMDevice(const QString& name, QObject* parent = nullptr);
    ~DMMDevice();
    
    // 设备控制实现
    bool connectDevice() override;
    bool disconnectDevice() override;
    bool configureDevice(const QMap<QString, QVariant>& config) override;
    bool startOperation() override;
    bool stopOperation() override;
    
    // DMM特有功能
    bool setMeasurementType(MeasurementType type);
    bool setRange(double range);
    bool setAutoRange(bool enable);
    double takeMeasurement();
    QVector<double> takeMeasurements(int count);
    
    // 特殊测量功能
    double measureResistance();
    double measureCapacitance();
    double measureInductance();
    bool testDiode(double& forwardVoltage);
    bool testContinuity();

protected:
    void processCommand(const DeviceCommand& command) override;
    void handleError(const QString& errorMessage) override;

private:
    bool initializeHardware();
    void releaseHardware();
    bool configureMeasurement(MeasurementType type, double range);
    double readMeasurementValue();

signals:
    void measurementReady(double value);
    void measurementCompleted(const QVector<double>& values);
};
```

## 故障诊断算法实现

### 1. 电阻器诊断算法

```cpp
DiagnosticResult FaultDiagnostic::diagnoseResistor(
    const ComponentSpec& spec,  
    const MeasurementResult& measurement) {
    
    DiagnosticResult result(spec.getName());
    
    // 获取测量数据
    double measuredResistance = measurement.getCalculatedValue("resistance");
    double nominalResistance = spec.getParameter("resistance").toDouble();
    double tolerance = spec.getParameter("tolerance").toDouble() / 100.0;
    
    // 计算允许范围
    double lowerBound = nominalResistance * (1.0 - tolerance);
    double upperBound = nominalResistance * (1.0 + tolerance);
    
    // 开路检测
    if (measuredResistance > OPEN_CIRCUIT_THRESHOLD) {
        result.setFaultType(DiagnosticResult::OPEN_CIRCUIT);
        result.setSeverity(DiagnosticResult::CRITICAL);
        result.setDescription("电阻器开路");
        result.setConfidence(0.95);
        return result;
    }
    
    // 短路检测
    if (measuredResistance < SHORT_CIRCUIT_THRESHOLD) {
        result.setFaultType(DiagnosticResult::SHORT_CIRCUIT);
        result.setSeverity(DiagnosticResult::CRITICAL);
        result.setDescription("电阻器短路");
        result.setConfidence(0.95);
        return result;
    }
    
    // 阻值偏差检测
    if (measuredResistance < lowerBound || measuredResistance > upperBound) {
        result.setFaultType(DiagnosticResult::OUT_OF_TOLERANCE);
        result.setSeverity(DiagnosticResult::WARNING);
        
        double deviation = abs(measuredResistance - nominalResistance) / nominalResistance * 100.0;
        result.setDescription(QString("阻值偏差 %1%").arg(deviation, 0, 'f', 2));
        result.setConfidence(calculateConfidence(deviation, tolerance * 100.0));
        result.setAdditionalData("measured_value", measuredResistance);
        result.setAdditionalData("deviation_percent", deviation);
        return result;
    }
    
    // 正常状态
    result.setFaultType(DiagnosticResult::NO_FAULT);
    result.setSeverity(DiagnosticResult::INFO);
    result.setDescription("电阻器正常");
    result.setConfidence(0.90);
    
    return result;
}

// 置信度计算辅助函数
double FaultDiagnostic::calculateConfidence(double deviation, double tolerance) {
    // 偏差越大，置信度越高
    double ratio = deviation / tolerance;
    if (ratio > 2.0) return 0.95;
    if (ratio > 1.5) return 0.85;
    if (ratio > 1.2) return 0.75;
    return 0.65;
}
```

### 2. 电容器诊断算法

```cpp
DiagnosticResult FaultDiagnostic::diagnoseCapacitor(
    const ComponentSpec& spec,
    const MeasurementResult& measurement) {
    
    DiagnosticResult result(spec.getName());
    
    // 获取测量数据
    double measuredCapacitance = measurement.getCalculatedValue("capacitance");
    double measuredESR = measurement.getCalculatedValue("esr");
    double leakageCurrent = measurement.getCalculatedValue("leakage_current");
    
    double nominalCapacitance = spec.getParameter("capacitance").toDouble();
    double tolerance = spec.getParameter("tolerance").toDouble() / 100.0;
    double esrThreshold = spec.getParameter("esr_threshold").toDouble();
    double leakageThreshold = spec.getParameter("leakage_threshold").toDouble();
    
    // 开路检测 (容值极小)
    if (measuredCapacitance < nominalCapacitance * 0.1) {
        result.setFaultType(DiagnosticResult::OPEN_CIRCUIT);
        result.setSeverity(DiagnosticResult::CRITICAL);
        result.setDescription("电容器开路");
        result.setConfidence(0.90);
        return result;
    }
    
    // 短路检测 (ESR极小且容值极大)
    if (measuredESR < 0.1 && measuredCapacitance > nominalCapacitance * 10) {
        result.setFaultType(DiagnosticResult::SHORT_CIRCUIT);
        result.setSeverity(DiagnosticResult::CRITICAL);
        result.setDescription("电容器短路");
        result.setConfidence(0.85);
        return result;
    }
    
    // 高ESR检测
    if (measuredESR > esrThreshold) {
        result.setFaultType(DiagnosticResult::HIGH_ESR);
        result.setSeverity(DiagnosticResult::WARNING);
        result.setDescription(QString("ESR过高: %1Ω").arg(measuredESR, 0, 'f', 3));
        result.setConfidence(0.80);
        result.setAdditionalData("measured_esr", measuredESR);
        result.setAdditionalData("esr_threshold", esrThreshold);
        return result;
    }
    
    // 漏电流检测
    if (leakageCurrent > leakageThreshold) {
        result.setFaultType(DiagnosticResult::LEAKAGE_CURRENT);
        result.setSeverity(DiagnosticResult::WARNING);
        result.setDescription(QString("漏电流过大: %1μA").arg(leakageCurrent * 1e6, 0, 'f', 2));
        result.setConfidence(0.75);
        result.setAdditionalData("measured_leakage", leakageCurrent);
        return result;
    }
    
    // 容值偏差检测
    double lowerBound = nominalCapacitance * (1.0 - tolerance);
    double upperBound = nominalCapacitance * (1.0 + tolerance);
    
    if (measuredCapacitance < lowerBound || measuredCapacitance > upperBound) {
        result.setFaultType(DiagnosticResult::OUT_OF_TOLERANCE);
        result.setSeverity(DiagnosticResult::WARNING);
        
        double deviation = abs(measuredCapacitance - nominalCapacitance) / nominalCapacitance * 100.0;
        result.setDescription(QString("容值偏差 %1%").arg(deviation, 0, 'f', 2));
        result.setConfidence(calculateConfidence(deviation, tolerance * 100.0));
        result.setAdditionalData("measured_capacitance", measuredCapacitance);
        result.setAdditionalData("deviation_percent", deviation);
        return result;
    }
    
    // 正常状态
    result.setFaultType(DiagnosticResult::NO_FAULT);
    result.setSeverity(DiagnosticResult::INFO);
    result.setDescription("电容器正常");
    result.setConfidence(0.85);
    
    return result;
}
```

## 多线程同步机制

### 1. 设备同步控制

```cpp
class DeviceSynchronizer : public QObject {
    Q_OBJECT

private:
    QList<DeviceThread*> m_devices;        // 设备列表
    QMutex m_syncMutex;                    // 同步互斥锁
    QWaitCondition m_syncCondition;        // 同步条件变量
    QAtomicInt m_readyDeviceCount;         // 就绪设备计数
    bool m_syncEnabled;                    // 是否启用同步

public:
    DeviceSynchronizer(QObject* parent = nullptr);
    
    // 设备管理
    void addDevice(DeviceThread* device);
    void removeDevice(DeviceThread* device);
    void clearDevices();
    
    // 同步控制
    void enableSynchronization(bool enable);
    bool isSynchronizationEnabled() const { return m_syncEnabled; }
    
    // 同步操作
    bool synchronizeDevices();
    bool waitForAllDevicesReady(int timeout = 5000);
    void resetSynchronization();

public slots:
    void onDeviceReady();
    void onDeviceError(const QString& error);

signals:
    void allDevicesReady();
    void synchronizationFailed(const QString& reason);

private:
    void checkSyncCondition();
};

// 使用示例
DeviceSynchronizer synchronizer;
synchronizer.addDevice(aoDevice);
synchronizer.addDevice(daqDevice1);
synchronizer.addDevice(daqDevice2);
synchronizer.addDevice(dmmDevice);

synchronizer.enableSynchronization(true);
bool success = synchronizer.synchronizeDevices();
```

### 2. 线程安全的数据传递

```cpp
template<typename T>
class ThreadSafeQueue {
private:
    mutable QMutex m_mutex;
    QQueue<T> m_queue;
    QWaitCondition m_condition;

public:
    void enqueue(const T& item) {
        QMutexLocker locker(&m_mutex);
        m_queue.enqueue(item);
        m_condition.wakeOne();
    }
    
    T dequeue() {
        QMutexLocker locker(&m_mutex);
        while (m_queue.isEmpty()) {
            m_condition.wait(&m_mutex);
        }
        return m_queue.dequeue();
    }
    
    bool tryDequeue(T& item, int timeout = 0) {
        QMutexLocker locker(&m_mutex);
        if (m_queue.isEmpty()) {
            if (timeout <= 0) return false;
            if (!m_condition.wait(&m_mutex, timeout)) return false;
        }
        if (!m_queue.isEmpty()) {
            item = m_queue.dequeue();
            return true;
        }
        return false;
    }
    
    int size() const {
        QMutexLocker locker(&m_mutex);
        return m_queue.size();
    }
    
    bool isEmpty() const {
        QMutexLocker locker(&m_mutex);
        return m_queue.isEmpty();
    }
};
```

## 配置管理系统

### 1. 配置文件结构

```cpp
class ConfigManager : public QObject {
    Q_OBJECT

private:
    QSettings* m_settings;              // Qt设置对象
    QMap<QString, QVariant> m_cache;    // 配置缓存
    QString m_configFilePath;           // 配置文件路径
    mutable QMutex m_mutex;            // 线程安全互斥锁

public:
    ConfigManager(const QString& configFile, QObject* parent = nullptr);
    ~ConfigManager();
    
    // 配置读写
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void setValue(const QString& key, const QVariant& value);
    
    // 组配置
    void beginGroup(const QString& group);
    void endGroup();
    
    // 设备配置
    void setDeviceConfig(const QString& deviceName, const QMap<QString, QVariant>& config);
    QMap<QString, QVariant> getDeviceConfig(const QString& deviceName) const;
    
    // 诊断配置
    void setDiagnosticConfig(ComponentSpec::ComponentType type, const QMap<QString, QVariant>& config);
    QMap<QString, QVariant> getDiagnosticConfig(ComponentSpec::ComponentType type) const;
    
    // 配置管理
    bool loadConfig();
    bool saveConfig();
    void resetToDefaults();
    
    // 配置验证
    bool validateConfig() const;
    QStringList getConfigErrors() const;

signals:
    void configChanged(const QString& key, const QVariant& value);
    void configLoaded();
    void configSaved();

private:
    void loadDefaultConfig();
    bool isValidKey(const QString& key) const;
};
```

### 2. 默认配置文件 (config.ini)

```ini
[General]
language=zh_CN
theme=default
auto_save=true
backup_count=5

[Devices]
ao_device_name=JY5711
daq_device1_name=JY5322
daq_device2_name=JY5323
dmm_device_name=JY8902
sync_timeout=5000
retry_count=3

[AO_Device]
output_range=10.0
sample_rate=1000000
buffer_size=8192
continuous_output=false

[DAQ_Device]
input_range=10.0
sample_rate=1000000
samples_per_channel=1000
trigger_source=immediate
trigger_level=0.0

[DMM_Device]
auto_range=true
average_count=10
integration_time=0.1

[Diagnostics/Resistor]
open_threshold=1e9
short_threshold=1.0
tolerance_factor=1.2

[Diagnostics/Capacitor]
esr_threshold_factor=2.0
leakage_threshold=1e-6
capacitance_min_factor=0.1

[Diagnostics/Inductor]
q_factor_threshold=10.0
tolerance_factor=1.5

[Diagnostics/Diode]
forward_voltage_min=0.5
forward_voltage_max=1.0
reverse_leakage_threshold=1e-6

[Diagnostics/IC]
power_pin_threshold=0.1
functional_pin_threshold=0.2
```

## 日志系统

### 1. 日志管理器

```cpp
class Logger : public QObject {
    Q_OBJECT

public:
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };

private:
    static Logger* s_instance;          // 单例实例
    QFile* m_logFile;                  // 日志文件
    QTextStream* m_logStream;          // 日志流
    LogLevel m_minLevel;               // 最小日志级别
    QString m_logFormat;               // 日志格式
    QMutex m_mutex;                    // 线程安全互斥锁
    bool m_enableFileLogging;          // 是否启用文件日志
    bool m_enableConsoleOutput;        // 是否启用控制台输出

public:
    static Logger* instance();
    static void destroy();
    
    // 日志设置
    void setLogLevel(LogLevel level);
    void setLogFormat(const QString& format);
    void enableFileLogging(bool enable);
    void enableConsoleOutput(bool enable);
    bool openLogFile(const QString& filename);
    void closeLogFile();
    
    // 日志记录
    void log(LogLevel level, const QString& message, const QString& category = QString());
    void debug(const QString& message, const QString& category = QString());
    void info(const QString& message, const QString& category = QString());
    void warning(const QString& message, const QString& category = QString());
    void error(const QString& message, const QString& category = QString());
    void critical(const QString& message, const QString& category = QString());

private:
    Logger(QObject* parent = nullptr);
    ~Logger();
    
    QString formatMessage(LogLevel level, const QString& message, const QString& category) const;
    QString levelToString(LogLevel level) const;

signals:
    void messageLogged(LogLevel level, const QString& message, const QString& category);
};

// 便捷宏定义
#define LOG_DEBUG(msg) Logger::instance()->debug(msg, Q_FUNC_INFO)
#define LOG_INFO(msg) Logger::instance()->info(msg, Q_FUNC_INFO)
#define LOG_WARNING(msg) Logger::instance()->warning(msg, Q_FUNC_INFO)
#define LOG_ERROR(msg) Logger::instance()->error(msg, Q_FUNC_INFO)
#define LOG_CRITICAL(msg) Logger::instance()->critical(msg, Q_FUNC_INFO)
```

---

*本文档详细介绍了PCB故障检测系统的技术实现细节，包括核心数据结构、设备驱动架构、诊断算法和系统组件的具体实现。*
