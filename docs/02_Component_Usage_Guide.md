# PCB故障检测系统 - 组件使用指南

## 主要组件介绍

### 1. MainWindow (主窗口控制器)

`MainWindow` 是系统的主要用户交互界面，负责协调所有子系统的工作。

#### 主要功能
- 用户界面布局管理
- 菜单和工具栏控制
- 测试进度显示
- 实时状态更新
- 用户操作响应

#### 关键方法
```cpp
// 系统初始化
void MainWindow::initializeSystem();

// 开始测试
void MainWindow::startTesting();

// 停止测试
void MainWindow::stopTesting();

// 更新状态显示
void MainWindow::updateStatus(const QString& message);

// 处理测试结果
void MainWindow::handleTestResult(const DiagnosticResult& result);
```

#### 使用示例
```cpp
MainWindow window;
window.show();
window.initializeSystem();  // 初始化硬件设备
```

### 2. DeviceManager (设备管理器)

`DeviceManager` 负责管理所有硬件设备的生命周期和状态。

#### 支持的设备类型
- **JY5711 AO设备**: 模拟输出设备
- **JY5322 DAQ设备**: 数据采集设备（主）
- **JY5323 DAQ设备**: 数据采集设备（辅）
- **JY8902 DMM设备**: 数字万用表

#### 核心功能
```cpp
// 设备初始化
bool DeviceManager::initializeDevices();

// 设备检测
QStringList DeviceManager::detectAvailableDevices();

// 设备控制
bool DeviceManager::startDevice(DeviceType type);
bool DeviceManager::stopDevice(DeviceType type);

// 设备状态查询
DeviceStatus DeviceManager::getDeviceStatus(DeviceType type);

// 同步控制
bool DeviceManager::synchronizeDevices();
```

#### 设备配置
```cpp
// 配置AO设备
AOConfig aoConfig;
aoConfig.outputRange = 10.0;  // ±10V
aoConfig.sampleRate = 1000000; // 1MHz
deviceManager->configureAO(aoConfig);

// 配置DAQ设备
DAQConfig daqConfig;
daqConfig.channels = {0, 1, 2, 3};
daqConfig.sampleRate = 1000000;
daqConfig.inputRange = 10.0;
deviceManager->configureDAQ(daqConfig);
```

### 3. FaultDiagnostic (故障诊断器)

`FaultDiagnostic` 是系统的核心诊断引擎，包含各种元件的故障检测算法。

#### 支持的元件类型

##### 电阻器诊断
```cpp
DiagnosticResult FaultDiagnostic::diagnoseResistor(
    const ComponentSpec& spec,
    const MeasurementResult& measurement
);
```

**检测故障类型**:
- 开路 (Open Circuit)
- 短路 (Short Circuit)  
- 阻值偏差 (Out of Tolerance)

**诊断参数**:
```cpp
struct ResistorSpec {
    double nominalValue;     // 标称阻值
    double tolerance;        // 容差百分比
    double openThreshold;    // 开路判断阈值
    double shortThreshold;   // 短路判断阈值
};
```

##### 电容器诊断
```cpp
DiagnosticResult FaultDiagnostic::diagnoseCapacitor(
    const ComponentSpec& spec,
    const MeasurementResult& measurement
);
```

**检测故障类型**:
- 开路 (Open Circuit)
- 短路 (Short Circuit)
- 容值偏差 (Capacitance Deviation)
- 高ESR (High ESR)

**诊断参数**:
```cpp
struct CapacitorSpec {
    double nominalCapacitance;  // 标称容值
    double tolerance;           // 容差
    double esrThreshold;        // ESR阈值
    double leakageThreshold;    // 漏电流阈值
};
```

##### 电感器诊断
```cpp
DiagnosticResult FaultDiagnostic::diagnoseInductor(
    const ComponentSpec& spec,
    const MeasurementResult& measurement
);
```

**检测故障类型**:
- 开路 (Open Circuit)
- 短路 (Short Circuit)
- 感值偏差 (Inductance Deviation)

##### 二极管诊断
```cpp
DiagnosticResult FaultDiagnostic::diagnoseDiode(
    const ComponentSpec& spec,
    const MeasurementResult& measurement
);
```

**检测故障类型**:
- 开路 (Open Circuit)
- 短路 (Short Circuit)
- 正向压降异常 (Forward Voltage Drop)
- 反向漏电 (Reverse Leakage)

##### 集成电路诊断
```cpp
DiagnosticResult FaultDiagnostic::diagnoseIC(
    const ComponentSpec& spec,
    const MeasurementResult& measurement
);
```

**检测故障类型**:
- 电源引脚故障 (Power Pin Fault)
- 功能引脚故障 (Functional Pin Fault)
- 内部短路 (Internal Short)

### 4. TestSequenceManager (测试序列管理器)

`TestSequenceManager` 负责管理测试序列的创建、执行和控制。

#### 核心功能
```cpp
// 创建测试序列
TestSequence* TestSequenceManager::createSequence(
    const QList<ComponentSpec>& components
);

// 执行测试序列
bool TestSequenceManager::executeSequence(TestSequence* sequence);

// 暂停/恢复测试
void TestSequenceManager::pauseSequence();
void TestSequenceManager::resumeSequence();

// 停止测试
void TestSequenceManager::stopSequence();

// 获取测试进度
int TestSequenceManager::getProgress() const;
```

#### 测试序列配置
```cpp
TestSequence sequence;
sequence.addComponent(ComponentSpec(RESISTOR, "R1", 1000.0, 5.0));
sequence.addComponent(ComponentSpec(CAPACITOR, "C1", 100e-9, 10.0));
sequence.addComponent(ComponentSpec(INDUCTOR, "L1", 100e-6, 20.0));

// 设置测试参数
sequence.setTestVoltage(5.0);
sequence.setTestCurrent(0.001);
sequence.setMeasurementTime(1.0);
```

### 5. ResultExporter (结果导出器)

`ResultExporter` 负责测试结果的导出和报告生成。

#### 支持的导出格式
- CSV格式
- JSON格式
- XML格式
- PDF报告

#### 使用方法
```cpp
// 导出CSV格式
bool ResultExporter::exportToCSV(
    const QList<DiagnosticResult>& results,
    const QString& filename
);

// 导出JSON格式
bool ResultExporter::exportToJSON(
    const QList<DiagnosticResult>& results,
    const QString& filename
);

// 生成PDF报告
bool ResultExporter::generatePDFReport(
    const QList<DiagnosticResult>& results,
    const QString& filename
);
```

#### 报告内容
- 测试概要信息
- 每个元件的详细结果
- 故障统计分析
- 图表和趋势分析
- 建议和结论

## 典型使用流程

### 1. 单元件测试
```cpp
// 初始化系统
MainWindow window;
DeviceManager* deviceManager = window.getDeviceManager();
FaultDiagnostic* diagnostic = window.getFaultDiagnostic();

// 配置测试参数
ComponentSpec resistorSpec(RESISTOR, "R1", 1000.0, 5.0);

// 执行测试
MeasurementResult measurement = deviceManager->measureComponent(resistorSpec);
DiagnosticResult result = diagnostic->diagnoseResistor(resistorSpec, measurement);

// 处理结果
if (result.hasFault()) {
    qDebug() << "故障类型:" << result.getFaultType();
    qDebug() << "故障描述:" << result.getDescription();
}
```

### 2. 批量测试
```cpp
// 创建测试序列
TestSequenceManager* manager = window.getTestSequenceManager();
TestSequence* sequence = manager->createSequence(componentList);

// 执行批量测试
connect(manager, &TestSequenceManager::progressChanged,
        [](int progress) {
            qDebug() << "测试进度:" << progress << "%";
        });

connect(manager, &TestSequenceManager::testCompleted,
        [](const QList<DiagnosticResult>& results) {
            qDebug() << "测试完成，共" << results.size() << "个结果";
        });

manager->executeSequence(sequence);
```

### 3. 结果导出
```cpp
// 获取测试结果
QList<DiagnosticResult> results = manager->getResults();

// 导出结果
ResultExporter* exporter = window.getResultExporter();
exporter->exportToCSV(results, "test_results.csv");
exporter->generatePDFReport(results, "test_report.pdf");
```

## 错误处理和异常

### 常见错误类型
- 设备连接失败
- 设备初始化错误
- 测量超时
- 数据处理异常
- 文件操作失败

### 错误处理策略
```cpp
try {
    bool success = deviceManager->initializeDevices();
    if (!success) {
        throw DeviceInitializationException("设备初始化失败");
    }
} catch (const DeviceException& e) {
    QMessageBox::critical(this, "错误", e.what());
    return false;
}
```

## 性能优化建议

### 1. 多线程使用
- 设备操作在独立线程中执行
- 避免阻塞主界面线程
- 合理设置线程优先级

### 2. 内存管理
- 及时释放测量数据
- 使用智能指针管理资源
- 避免内存泄漏

### 3. 设备同步
- 合理设置同步间隔
- 避免频繁状态查询
- 优化设备配置参数

---

*本文档详细介绍了系统各组件的使用方法，如需了解更多技术实现细节，请参考实现细节文档。*
