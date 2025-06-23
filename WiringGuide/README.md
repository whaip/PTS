# 接线引导系统使用指南

## 概述

本文档介绍了基于mainwindow和portconfiguration重新设计的完善合理的端口选择和接线引导系统。

## 系统架构

### 1. 端口定义 (PortDefinitions)
- **JY5711端口分配**:
  - 0-15: 模拟量输出端口 (ANALOG_OUTPUT)
  - 16-27: 数字量输出端口 (DIGITAL_OUTPUT) 
  - 28-31: 电源输出端口 (POWER_OUTPUT)
- **JY5323端口分配**:
  - 0-31: 模拟量输入端口 (ANALOG_INPUT)
- **JY5322端口分配**:
  - 0-15: 数字量输入端口 (DIGITAL_INPUT)

### 2. 核心组件

#### PortManager (端口管理器)
- 管理所有设备端口的分配和释放
- 提供端口可用性查询
- 支持自动端口分配
- 根据元件类型推荐最佳端口配置

#### WiringResourceManager (接线资源管理器)
- 管理接线方案模板
- 验证接线配置的有效性
- 保存和加载接线方案
- 提供接线方案的CRUD操作

#### WiringTaskGenerator (接线任务生成器)
- 基于元件规格生成测试任务
- 管理任务生命周期
- 支持批量任务生成
- 提供任务执行调度

#### WiringGuideDialog (接线引导对话框)
- 提供图形化接线引导界面
- 分步骤显示接线指导
- 支持连接验证
- 生成最终接线方案

## 使用流程

### 1. 系统初始化
```cpp
// 在MainWindow构造函数中
port_manager_ = new PortManager(device_manager_, this);
port_manager_->initializePorts();

wiring_resource_manager_ = new WiringResourceManager(port_manager_, this);
wiring_resource_manager_->initializeResources();

task_generator_ = new WiringTaskGenerator(wiring_resource_manager_, this);
```

### 2. 开始接线引导
```cpp
void MainWindow::startWiringGuide()
{
    // 从UI获取元件信息
    ComponentSpec component = createComponentFromUI();
    
    // 显示接线引导对话框
    showWiringGuideForComponent(component);
}
```

### 3. 接线引导流程
1. **元件信息配置** - 确认元件类型、参数等
2. **端口选择** - 自动或手动选择测试端口
3. **接线指导** - 分步骤显示接线说明和图表
4. **验证结果** - 验证连接正确性

### 4. 测试执行
接线完成后自动生成测试任务并执行故障诊断。

## 端口分配策略

### 电阻测试 (RESISTOR)
- 1个模拟输出端口 (信号源)
- 2个模拟输入端口 (电压测量)
- 1个电源输出端口 (电源供应)

### 电容测试 (CAPACITOR)
- 1个模拟输出端口 (交流信号源)
- 2个模拟输入端口 (电压测量)

### 电感测试 (INDUCTOR)
- 1个模拟输出端口 (交流信号源)
- 2个模拟输入端口 (电压测量)

### 二极管测试 (DIODE)
- 1个模拟输出端口 (电压扫描)
- 2个模拟输入端口 (电压电流测量)
- 1个电源输出端口 (偏置电源)

### IC测试 (IC)
- 2个模拟输出端口 (信号源)
- 4个模拟输入端口 (信号测量)
- 2个数字输出端口 (控制信号)
- 2个数字输入端口 (状态检测)
- 1个电源输出端口 (供电)

## 接线方案模板

系统预定义了各种元件类型的标准接线模板：

### 电阻标准模板
- 测试方法: 四线法
- 测试电压: 1V
- 测试电流: 自适应
- 测量精度: 0.1%

### 电容标准模板
- 测试方法: 交流测量
- 测试频率: 1kHz
- 测试电压: 1V
- 测量精度: 1%

## 配置文件

### 接线方案保存路径
- 用户方案: `Documents/FaultDetect/WiringSchemes/`
- 系统模板: `Documents/FaultDetect/WiringTemplates/`
- 配置文件: `Documents/FaultDetect/Config/`

### 方案文件格式
```json
{
  "schemeId": "uuid",
  "schemeName": "方案名称",
  "componentType": 0,
  "description": "方案描述",
  "connections": [
    {
      "sourcePort": {
        "deviceName": "JY5711",
        "portNumber": 0,
        "portType": 0,
        "description": "模拟输出端口 AO0"
      },
      "wireColor": "红色",
      "instruction": "连接到电阻第一端"
    }
  ],
  "testParameters": {
    "test_method": "four_wire",
    "test_voltage": 1.0
  }
}
```

## API接口

### 端口管理
```cpp
// 获取可用端口
QVector<PortInfo> ports = portManager->getAvailablePorts(PortType::ANALOG_OUTPUT);

// 分配端口
bool success = portManager->allocatePort("JY5711", 0, "R1");

// 释放端口
portManager->releasePort("JY5711", 0);

// 自动分配
QVector<PortInfo> allocated = portManager->autoAllocatePorts(ComponentType::RESISTOR, "R1");
```

### 资源管理
```cpp
// 保存方案
bool success = resourceManager->saveWiringScheme(scheme);

// 加载模板
QVector<WiringScheme> templates = resourceManager->getTemplatesForComponent(ComponentType::RESISTOR);

// 生成优化方案
WiringScheme optimal = resourceManager->generateOptimalScheme(component);
```

### 任务生成
```cpp
// 生成任务
QString taskId = taskGenerator->generateTask(component);

// 准备执行
bool ready = taskGenerator->prepareTaskExecution(taskId);

// 完成任务
taskGenerator->finalizeTaskExecution(taskId, true, "执行成功");
```

## 错误处理

### 常见错误及解决方法

1. **端口初始化失败**
   - 检查设备连接
   - 确认设备驱动安装
   - 重启设备管理器

2. **端口分配冲突**
   - 释放已占用端口
   - 选择其他可用端口
   - 检查端口分配状态

3. **接线验证失败**
   - 检查物理连接
   - 确认端口配置
   - 验证信号兼容性

4. **任务生成失败**
   - 检查元件参数
   - 确认模板可用性
   - 验证资源分配

## 扩展功能

### 自定义接线模板
用户可以创建和保存自定义的接线模板，系统支持：
- 模板编辑器
- 参数自定义
- 验证规则定义
- 模板导入导出

### 批量操作
- 批量端口分配
- 批量任务生成
- 批量方案应用
- 批量结果导出

### 历史记录
- 接线历史追踪
- 任务执行记录
- 错误日志管理
- 性能统计分析

## 测试验证

使用提供的测试程序验证系统功能：
```bash
# 编译测试程序
make -f Makefile.wiring_test

# 运行控制台测试
make -f Makefile.wiring_test test

# 运行GUI测试
make -f Makefile.wiring_test run-gui
```

## 注意事项

1. **设备安全**: 确保在安全的电压电流范围内操作
2. **端口保护**: 避免短路和过载
3. **数据备份**: 定期备份接线方案和配置
4. **版本兼容**: 确保所有组件版本匹配
5. **权限管理**: 控制用户对系统配置的访问权限
