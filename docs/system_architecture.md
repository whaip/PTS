# 系统架构图

## Mermaid格式

```mermaid
graph TB
    subgraph "应用服务层 (Application Layer)"
        A1[用户交互界面<br/>MainWindow Control]
        A2[测试序列管理<br/>TestSequence Manager]
        A3[报告生成系统<br/>ResultExporter System]
        A4[数据管理系统<br/>Database Mgmt System]
    end
    
    subgraph "算法处理层 (Algorithm Layer)"
        B1[目标检测引擎<br/>YOLO-based Detection]
        B2[PCB型号识别<br/>SIFT Feature Matching]
        B3[故障诊断框架<br/>Multi-Algorithm Framework]
        B4[决策融合模块<br/>Decision Fusion Module]
    end
    
    subgraph "数据处理层 (Data Processing Layer)"
        C1[图像处理模块<br/>Image Process Module]
        C2[电气数据管理<br/>Electrical Data Management]
        C3[温度数据处理<br/>Temperature Processing]
        C4[时间同步机制<br/>Time Sync Mechanism]
    end
    
    subgraph "硬件感知层 (Hardware Sensing Layer)"
        D1[高清摄像头<br/>HD Camera]
        D2[红外热成像<br/>Thermal Camera]
        D3[电气测试设备<br/>Electrical Test Equipment]
        D4[同步控制单元<br/>Sync Control Unit]
    end
    
    A1 --> B1
    A2 --> B2
    A3 --> B3
    A4 --> B4
    
    B1 --> C1
    B2 --> C1
    B3 --> C2
    B4 --> C3
    
    C1 --> D1
    C2 --> D3
    C3 --> D2
    C4 --> D4
    
    style A1 fill:#e1f5fe
    style A2 fill:#e1f5fe
    style A3 fill:#e1f5fe
    style A4 fill:#e1f5fe
    
    style B1 fill:#f3e5f5
    style B2 fill:#f3e5f5
    style B3 fill:#f3e5f5
    style B4 fill:#f3e5f5
    
    style C1 fill:#e8f5e8
    style C2 fill:#e8f5e8
    style C3 fill:#e8f5e8
    style C4 fill:#e8f5e8
    
    style D1 fill:#fff3e0
    style D2 fill:#fff3e0
    style D3 fill:#fff3e0
    style D4 fill:#fff3e0
```

## 使用说明

1. 复制上面的Mermaid代码
2. 访问 https://mermaid.live/
3. 将代码粘贴到左侧编辑器中
4. 右侧会实时显示生成的图形
5. 可以导出为PNG、SVG等格式

## 其他格式选项

如果您需要其他格式，我也可以为您生成：
- PlantUML格式
- Graphviz格式
- 或者直接生成HTML文件