# FaultDiagnostic 重构版本使用指南

## 概述

FaultDiagnostic已经重构为基于ComponentDiagnosticFramework的新架构，提供了更强大、更灵活的组件诊断能力。

## 主要改进

### 1. 框架架构
- **基于插件的设计**：每种组件类型都有专门的诊断器
- **统一的接口**：所有诊断器都继承自BaseComponentDiagnostic
- **管理器模式**：ComponentDiagnosticManager负责管理所有诊断器

### 2. 异步能力
- **异步诊断**：支持单个组件的异步诊断
- **批量异步**：支持多个组件的并发诊断
- **任务管理**：可以查询、取消、监控诊断任务

### 3. 向后兼容
- **保持原有API**：原有的diagnoseComponent等方法仍然可用
- **兼容性包装**：旧的测量方法自动转换为新的框架调用

## 基本使用

### 创建诊断器
```cpp
#include "faultdiagnostic.h"

DeviceManager* deviceManager = new DeviceManager();
FaultDiagnostic* diagnostic = new FaultDiagnostic(deviceManager);
```

### 同步诊断单个组件
```cpp
ComponentSpec resistor;
resistor.reference = "R1";
resistor.type = ComponentType::RESISTOR;
resistor.nominal_value = 1000.0;  // 1kΩ
resistor.tolerance_percent = 5.0;
resistor.channel = 0;

DiagnosticResult result = diagnostic->diagnoseComponent(resistor);

if (result.result == DiagnosticResult::PASS) {
    qDebug() << "组件正常，健康评分:" << result.healthScore;
} else {
    qDebug() << "组件故障:" << result.notes;
}
```

### 异步诊断
```cpp
// 连接信号
connect(diagnostic, &FaultDiagnostic::componentDiagnosisCompleted,
        [](const QString& taskId, const QString& componentId, const DiagnosticResult& result) {
    qDebug() << "异步诊断完成:" << componentId;
});

// 启动异步诊断
QString taskId = diagnostic->diagnoseComponentAsync(resistor);

// 查询任务状态
QMap<QString, QVariant> status = diagnostic->getTaskStatus(taskId);
```

### 批量诊断
```cpp
QVector<ComponentSpec> components;
components.append(resistor);
components.append(capacitor);
components.append(inductor);

// 同步批量诊断
QVector<DiagnosticResult> results = diagnostic->diagnoseBatch(components);

// 或异步批量诊断
QString batchTaskId = diagnostic->diagnoseBatchAsync(components);
```

## 支持的组件类型

### 电阻器 (RESISTOR)
- **测试方法**：I-V特性测量
- **故障类型**：开路、短路、阻值偏差
- **参数**：标称值、容差、测试电流

### 电容器 (CAPACITOR)
- **测试方法**：阻抗特性测量
- **故障类型**：开路、短路、容值偏差、高ESR、高漏电流
- **参数**：标称值、容差、最大ESR、最大漏电流

### 电感器 (INDUCTOR)
- **测试方法**：阻抗特性测量
- **故障类型**：开路、短路、感值偏差、DCR过高
- **参数**：标称值、容差、最大DCR

### 二极管 (DIODE)
- **测试方法**：正向/反向特性测量
- **故障类型**：短路、开路、漏电
- **参数**：正向压降、反向漏电流

### 集成电路 (IC)
- **测试方法**：供电电流、功能测试
- **故障类型**：过流、逻辑错误
- **参数**：供电电压、最大电流

## 配置参数

### ComponentSpec结构
```cpp
struct ComponentSpec {
    QString reference;          // 组件标识 (如"R1", "C2")
    ComponentType type;         // 组件类型
    double nominal_value;       // 标称值
    double tolerance_percent;   // 容差百分比
    int channel;               // 测试通道
    double test_voltage;       // 测试电压
    double test_current;       // 测试电流
    double max_esr;            // 最大ESR (电容器)
    double max_leakage;        // 最大漏电流
    // ... 其他参数
};
```

### 诊断结果结构
```cpp
struct DiagnosticResult {
    QString componentId;        // 组件ID
    QString componentType;      // 组件类型
    TestResult result;          // 测试结果 (PASS/FAIL/ERROR)
    double healthScore;         // 健康评分 (0-100)
    double confidence;          // 置信度 (0-100)
    QStringList faultTypes;     // 故障类型列表
    MeasurementResult measurementData; // 测量数据
    QString notes;              // 备注信息
    QDateTime timestamp;        // 时间戳
};
```

## 信号和槽

### 诊断过程信号
```cpp
// 单个组件诊断
void componentDiagnosisStarted(const QString& taskId, const QString& componentId);
void componentDiagnosisProgress(const QString& taskId, const QString& componentId, int percentage);
void componentDiagnosisCompleted(const QString& taskId, const QString& componentId, const DiagnosticResult& result);
void componentDiagnosisError(const QString& taskId, const QString& componentId, const QString& error);

// 批量诊断
void batchDiagnosisStarted(const QString& taskId, int totalComponents);
void batchDiagnosisProgress(const QString& taskId, int completedComponents, int totalComponents);
void batchDiagnosisCompleted(const QString& taskId, const QVector<DiagnosticResult>& results);
void batchDiagnosisError(const QString& taskId, const QString& error);

// 向后兼容信号
void diagnosisStarted(const ComponentSpec& component);
void diagnosisCompleted(const DiagnosticResult& result);
void diagnosticProgress(int percentage);
void errorOccurred(const QString& error);
```

## 高级功能

### 任务管理
```cpp
// 获取所有活动任务
QStringList activeTasks = diagnostic->getActiveTasks();

// 取消任务
bool cancelled = diagnostic->cancelTask(taskId);

// 获取诊断统计
QMap<QString, QVariant> stats = diagnostic->getDiagnosticStatistics();
```

### 配置选项
```cpp
// 设置全局超时时间
diagnostic->setGlobalTimeout(30000);  // 30秒

// 检查组件类型支持
bool supported = diagnostic->isComponentTypeSupported(ComponentType::RESISTOR);

// 获取支持的组件类型列表
QStringList types = diagnostic->getSupportedComponentTypes();
```

## 错误处理

### 异常类型
```cpp
try {
    auto result = diagnostic->diagnoseComponent(component);
} catch (const ComponentDiagnosticFramework::DiagnosticException& e) {
    qDebug() << "诊断异常:" << e.message();
    qDebug() << "错误代码:" << static_cast<int>(e.code());
}
```

### 错误代码
- `INVALID_COMPONENT_SPEC`: 无效的组件规格
- `UNSUPPORTED_COMPONENT_TYPE`: 不支持的组件类型
- `DEVICE_NOT_READY`: 设备未就绪
- `PORT_ALLOCATION_FAILED`: 端口分配失败
- `DATA_ACQUISITION_FAILED`: 数据采集失败
- `ANALYSIS_FAILED`: 分析失败
- `TIMEOUT`: 超时

## 性能优化

### 并发诊断
```cpp
// 设置最大并发数
diagnostic->setMaxConcurrency(4);

// 并发诊断多个组件
QString concurrentTaskId = diagnostic->diagnoseConcurrently(components, 4);
```

### 资源管理
- 诊断器会自动管理设备资源
- 任务完成后会自动清理资源
- 可以手动调用cleanup方法

## 迁移指南

### 从旧版本迁移
1. **包含头文件**：只需包含`faultdiagnostic.h`
2. **创建方式不变**：构造函数参数相同
3. **主要API兼容**：`diagnoseComponent`等方法保持不变
4. **新功能**：可以选择性使用异步和批量功能

### 代码示例对比
```cpp
// 旧版本
DiagnosticResult result = diagnostic->diagnoseComponent(component);

// 新版本 - 完全兼容
DiagnosticResult result = diagnostic->diagnoseComponent(component);

// 新版本 - 新功能
QString taskId = diagnostic->diagnoseComponentAsync(component);
```

## 调试和日志

重构版本提供了详细的调试信息：
```cpp
// 启用详细日志
QLoggingCategory::setFilterRules("ComponentDiagnosticFramework.debug=true");
```

## 总结

重构后的FaultDiagnostic提供了：
- **更好的架构**：模块化、可扩展
- **更强的功能**：异步、批量、并发
- **更好的兼容性**：保持原有API
- **更高的可靠性**：错误处理、资源管理
- **更好的性能**：并发诊断、优化算法

新的架构为未来添加更多组件类型和诊断方法提供了良好的基础。
