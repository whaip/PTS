# FaultDiagnostic 模块化迁移指南

## 概述

本指南帮助开发者从旧版本的 FaultDiagnostic 系统迁移到新的模块化架构。新架构提供了更好的可扩展性、可配置性和用户体验。

## 主要变化

### 1. 架构变化

**旧版本 (单体架构):**
```cpp
FaultDiagnostic faultDiagnostic;
faultDiagnostic.diagnoseComponent(component);  // 一体化诊断
```

**新版本 (模块化架构):**
```cpp
FaultDiagnostic faultDiagnostic;
// 三个独立模块协同工作
SignalConfiguration* signalConfig = faultDiagnostic.getSignalConfiguration();
PortConfiguration* portConfig = faultDiagnostic.getPortConfiguration();
FaultAnalysis* faultAnalysis = faultDiagnostic.getFaultAnalysis();
```

### 2. 接口变化

#### 兼容的接口（无需修改）

以下接口保持不变，现有代码可以直接使用：

```cpp
// 基本诊断接口
DiagnosticResult result = faultDiagnostic.diagnoseComponent(component);

// 信号连接
connect(&faultDiagnostic, &FaultDiagnostic::diagnosticCompleted,
        this, &MyClass::onDiagnosticCompleted);

// 元件类型枚举
ComponentType::RESISTOR
ComponentType::CAPACITOR
// ... 其他类型保持不变
```

#### 新增的接口

```cpp
// 工作流程接口
bool success = faultDiagnostic.executeFullDiagnosticWorkflow("resistor");

// 分步执行
faultDiagnostic.step1_ConfigureSignals("resistor");
faultDiagnostic.step2_ConfigurePorts();
faultDiagnostic.step3_ExecuteTests();
faultDiagnostic.step4_AnalyzeResults();

// 配置接口
faultDiagnostic.setActiveTestScheme("电阻测试(标准)");
faultDiagnostic.showPortConfigurationDialog();
```

### 3. 信号变化

#### 保持的信号
```cpp
// 这些信号保持不变
diagnosticCompleted(const DiagnosticResult& result)
errorOccurred(const QString& error)
progressUpdated(int percentage)
```

#### 新增的信号
```cpp
// 工作流程信号
workflowStarted(const QString& workflowType)
workflowCompleted(bool success, const QString& message)

// 模块状态信号
signalSchemeChanged(const QString& schemeName)
portConfigurationCompleted(const TestConfiguration& config)
```

## 迁移步骤

### 步骤 1: 更新头文件包含

**旧版本:**
```cpp
#include "faultdiagnostic.h"
```

**新版本:**
```cpp
#include "faultdiagnostic.h"
// 如果需要直接访问模块，可以包含：
// #include "signalconfiguration.h"
// #include "portconfiguration.h"
// #include "faultanalysis.h"
```

### 步骤 2: 检查代码兼容性

大多数现有代码无需修改。检查以下几点：

1. **基本诊断调用** - 保持不变
2. **信号连接** - 大部分保持不变
3. **数据结构** - ComponentSpec、DiagnosticResult 等保持不变

### 步骤 3: 利用新功能

#### 使用工作流程接口

**旧方式:**
```cpp
// 手动管理每个步骤
setupSignals();
configurePort();
executeTest();
analyzeResult();
```

**新方式:**
```cpp
// 一键执行完整流程
bool success = faultDiagnostic.executeFullDiagnosticWorkflow("resistor");
```

#### 使用配置接口

**旧方式:**
```cpp
// 硬编码配置
ComponentSpec spec;
spec.test_voltage = 1.0;
spec.channel = 1;
```

**新方式:**
```cpp
// 灵活配置
faultDiagnostic.setActiveTestScheme("电阻测试(高精度)");
faultDiagnostic.showPortConfigurationDialog();
```

### 步骤 4: 更新信号处理

**新增有用的信号连接:**
```cpp
// 工作流程监控
connect(&faultDiagnostic, &FaultDiagnostic::workflowStarted,
        this, &MyClass::onWorkflowStarted);
connect(&faultDiagnostic, &FaultDiagnostic::workflowCompleted,
        this, &MyClass::onWorkflowCompleted);

// 接线引导
connect(&faultDiagnostic, &FaultDiagnostic::wiringGuideRequired,
        this, &MyClass::showWiringGuide);
```

## 具体迁移示例

### 示例 1: 简单诊断

**旧代码:**
```cpp
void MyClass::diagnoseResistor()
{
    ComponentSpec spec;
    spec.type = ComponentType::RESISTOR;
    spec.reference = "R1";
    spec.nominal_value = 1000.0;
    spec.tolerance = 0.05;
    spec.channel = 1;
    
    DiagnosticResult result = faultDiagnostic_.diagnoseComponent(spec);
    
    if (result.result == DiagnosticResult::PASS) {
        qDebug() << "测试通过";
    }
}
```

**新代码 (兼容版本):**
```cpp
void MyClass::diagnoseResistor()
{
    // 完全相同的代码，无需修改
    ComponentSpec spec;
    spec.type = ComponentType::RESISTOR;
    spec.reference = "R1";
    spec.nominal_value = 1000.0;
    spec.tolerance = 0.05;
    spec.channel = 1;
    
    DiagnosticResult result = faultDiagnostic_.diagnoseComponent(spec);
    
    if (result.result == DiagnosticResult::PASS) {
        qDebug() << "测试通过";
    }
}
```

**新代码 (利用新功能):**
```cpp
void MyClass::diagnoseResistorWithWorkflow()
{
    // 使用新的工作流程接口
    connect(&faultDiagnostic_, &FaultDiagnostic::workflowCompleted,
            this, [this](bool success, const QString& message) {
                if (success) {
                    qDebug() << "工作流程完成:" << message;
                    AnalysisResult result = faultDiagnostic_.getLastAnalysisResult();
                    qDebug() << "健康度:" << result.healthScore;
                }
            });
    
    faultDiagnostic_.executeFullDiagnosticWorkflow("resistor");
}
```

### 示例 2: 批量测试

**旧代码:**
```cpp
void MyClass::batchTest(const QVector<ComponentSpec>& components)
{
    for (const auto& component : components) {
        DiagnosticResult result = faultDiagnostic_.diagnoseComponent(component);
        results_.append(result);
    }
}
```

**新代码 (利用异步处理):**
```cpp
void MyClass::batchTestAsync(const QVector<ComponentSpec>& components)
{
    connect(&faultDiagnostic_, &FaultDiagnostic::analysisCompleted,
            this, &MyClass::onAnalysisCompleted);
    
    for (const auto& component : components) {
        // 使用异步诊断，不阻塞UI
        QString testId = QString("batch_%1").arg(component.reference);
        
        // 先设置合适的测试方案
        QString componentType = getComponentTypeString(component.type);
        faultDiagnostic_.step1_ConfigureSignals(componentType);
        
        // 然后执行诊断
        DiagnosticResult result = faultDiagnostic_.diagnoseComponent(component);
        results_.append(result);
    }
}
```

## 性能优化建议

### 1. 使用异步接口
```cpp
// 避免阻塞UI
faultDiagnostic_.startFaultAnalysisAsync(testId, testData);
```

### 2. 重用配置
```cpp
// 设置一次，多次使用
faultDiagnostic_.setActiveTestScheme("电阻测试(标准)");
// 多个电阻测试会重用这个配置
```

### 3. 利用缓存
```cpp
// 系统会自动缓存分析结果
AnalysisResult cachedResult = faultDiagnostic_.getLastAnalysisResult();
```

## 故障排除

### 常见问题

1. **编译错误: 找不到新的信号**
   - 确保包含了正确的头文件
   - 检查信号名称拼写

2. **运行时错误: 模块未初始化**
   - 确保 DeviceManager 已正确初始化
   - 检查设备连接状态

3. **配置对话框不显示**
   - 确保先设置了测试方案
   - 检查GUI环境是否正常

### 调试技巧

```cpp
// 启用详细日志
qDebug() << "Available schemes:" << faultDiagnostic_.getAvailableTestSchemes();
qDebug() << "Port configured:" << faultDiagnostic_.isPortConfigured();
qDebug() << "Active scheme:" << faultDiagnostic_.getActiveTestScheme();
```

## 最佳实践

1. **优先使用工作流程接口** - 更简单，更可靠
2. **连接相关信号** - 获得更好的用户反馈
3. **处理错误情况** - 使用新的错误处理机制
4. **利用配置保存** - 提高用户体验
5. **测试兼容性** - 在部署前进行充分测试

## 支持和帮助

如果在迁移过程中遇到问题：

1. 查看 `README_FaultDiagnostic_Refactor.md` 了解详细架构
2. 运行 `test_fault_diagnostic.cpp` 进行功能验证
3. 检查日志输出了解运行状态
4. 参考现有的 `mainwindow.cpp` 中的使用示例
