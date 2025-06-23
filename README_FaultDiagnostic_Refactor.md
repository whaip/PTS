# FaultDiagnostic 模块化重构说明

## 重构概述

FaultDiagnostic 系统已经重构为三个独立但协调工作的模块，提高了系统的可扩展性和用户可配置性。

## 三大核心模块

### 1. 信号配置模块 (SignalConfiguration)
**文件:** `signalconfiguration.h/cpp`

**功能:**
- 管理测试信号类型和测试方案
- 提供预定义的测试方案（电阻、电容、电感、二极管、IC）
- 支持自定义信号配置

**主要类:**
- `SignalConfiguration`: 主配置类
- `TestSchemeSignals`: 测试方案结构
- `SignalDefinition`: 单个信号定义

**使用示例:**
```cpp
FaultDiagnostic faultDiagnostic(&deviceManager);
QStringList schemes = faultDiagnostic.getAvailableTestSchemes();
faultDiagnostic.setActiveTestScheme("电阻测试(标准)");
```

### 2. 端口配置和测试执行模块 (PortConfiguration)
**文件:** `portconfiguration.h/cpp`

**功能:**
- 配置测试端口和设备映射
- 提供图形化配置界面
- 执行测试序列
- 设备同步控制

**主要类:**
- `PortConfiguration`: 端口配置管理器
- `PortConfigurationDialog`: 配置对话框
- `TestExecutor`: 测试执行器

**使用示例:**
```cpp
bool configured = faultDiagnostic.showPortConfigurationDialog();
if (configured) {
    TestConfiguration config = faultDiagnostic.getCurrentTestConfiguration();
}
```

### 3. 故障分析模块 (FaultAnalysis)
**文件:** `faultanalysis.h/cpp`

**功能:**
- 测试数据分析和故障诊断
- 支持多种分析算法
- 异步分析处理
- 生成详细分析报告

**主要类:**
- `FaultAnalysis`: 主分析器
- `AnalysisAlgorithm`: 分析算法基类
- `ResistorAnalysisAlgorithm`: 电阻分析算法
- `CapacitorAnalysisAlgorithm`: 电容分析算法

**使用示例:**
```cpp
faultDiagnostic.startFaultAnalysisAsync(testId, testData);
AnalysisResult result = faultDiagnostic.getLastAnalysisResult();
```

## 新的模块化接口

### 高级工作流程接口

**完整诊断工作流程:**
```cpp
// 执行完整的自动化诊断流程
bool success = faultDiagnostic.executeFullDiagnosticWorkflow("resistor");
```

**分步骤工作流程:**
```cpp
// 分步执行，可以在每步之间进行自定义操作
faultDiagnostic.step1_ConfigureSignals("resistor");
faultDiagnostic.step2_ConfigurePorts();
faultDiagnostic.step3_ExecuteTests();
faultDiagnostic.step4_AnalyzeResults();
```

**自定义工作流程:**
```cpp
// 使用自定义方案和端口列表
TestSchemeSignals customScheme = createCustomScheme();
QStringList portList = {"Port1", "Port2"};
faultDiagnostic.executeCustomWorkflow(customScheme, portList);
```

### WiringGuide 集成

系统自动集成接线引导功能：

```cpp
// 启动接线引导
faultDiagnostic.startWiringGuide(config);

// 验证接线
bool wiringOK = faultDiagnostic.verifyWiring();

// 跳过接线引导（高级用户）
faultDiagnostic.skipWiringGuide();
```

## 向后兼容性

重构后的系统完全兼容现有的接口：

```cpp
// 传统的组件诊断接口仍然可用
ComponentSpec component;
component.type = ComponentType::RESISTOR;
component.reference = "R1";
component.nominal_value = 1000.0;

DiagnosticResult result = faultDiagnostic.diagnoseComponent(component);
```

## 信号系统

### 工作流程信号
- `workflowStarted(QString workflowType)`
- `workflowStepCompleted(int step, QString stepName)`
- `workflowCompleted(bool success, QString message)`
- `workflowError(QString error, int step)`

### 模块状态信号
- `signalSchemeChanged(QString schemeName)`
- `portConfigurationCompleted(TestConfiguration config)`
- `testExecutionCompleted(QString testId, TestData results)`
- `analysisCompleted(QString testId, AnalysisResult result)`

### 接线引导信号
- `wiringGuideRequired(TestConfiguration config)`
- `wiringVerificationResult(bool success, QString message)`

## 设备操作要求

所有设备操作使用 DeviceManager 的标准接口：
- `deviceManager_->submitOperation()`
- `deviceManager_->waitForResult()`
- `deviceManager_->getLastError()`
- `deviceManager_->waitForDataWithEventLoop()` (5320/8902数据采集)
- `deviceManager_->createSyncGroup()` / `removeSyncGroup()` (同步操作)

## 扩展性

### 添加新的信号类型
在 `SignalConfiguration` 中添加新的 `SignalType` 枚举值和相应的处理逻辑。

### 添加新的分析算法
继承 `AnalysisAlgorithm` 基类并实现特定的分析逻辑：

```cpp
class CustomAnalysisAlgorithm : public AnalysisAlgorithm
{
public:
    AnalysisResult analyze(const TestConfiguration& config,
                          const QVector<MeasurementData>& data,
                          const QMap<QString, QVariant>& specs) override;
    
    QString getAlgorithmName() const override { return "Custom Analysis"; }
    QString getComponentType() const override { return "CUSTOM"; }
};
```

### 添加新的测试设备
在 `PortConfiguration` 中的设备列表中添加新设备，并在 `TestExecutor` 中实现相应的控制逻辑。

## 配置文件

系统支持配置的保存和加载：
- 测试方案配置：自动保存用户自定义的测试方案
- 端口配置：保存常用的端口映射配置
- 分析参数：保存分析算法的参数设置

## 性能优化

- **异步处理**: 测试执行和分析都支持异步处理，不阻塞UI
- **数据缓存**: 测试结果和分析结果都有缓存机制
- **设备复用**: 智能的设备资源管理，避免重复初始化
- **并行测试**: 支持多设备并行测试以提高效率

## 错误处理

- 完善的错误传播机制
- 详细的错误信息和建议
- 自动恢复和重试机制
- 用户友好的错误提示

## 测试和验证

使用 `test_fault_diagnostic.cpp` 进行系统测试：

```bash
# 编译测试程序
qmake && make

# 运行测试
./test_fault_diagnostic
```

## 维护和调试

- 详细的日志输出（使用 qDebug）
- 模块状态监控
- 性能指标统计
- 内存使用监控
