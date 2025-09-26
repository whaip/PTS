 # 软件系统架构图

## Mermaid格式

```mermaid
graph TB
    subgraph "应用层 (Application Layer)"
        A1[MainWindow<br/>主控界面]
        A2[TestSequence<br/>Manager<br/>测试序列管理]
        A3[RealtimePCB<br/>Analyzer<br/>实时分析器]
        A4[Result<br/>Visualization<br/>结果可视化]
    end
    
    subgraph "服务层 (Service Layer)"
        B1[FaultDiagnostic<br/>故障诊断服务]
        B2[PCBIdentifier<br/>Service<br/>识别服务]
        B3[PCBDetection<br/>Manager<br/>检测管理服务]
        B4[PCBBoard<br/>Manager<br/>板卡管理服务]
    end
    
    subgraph "设备层 (Device Layer)"
        C1[DeviceManager<br/>设备管理器]
        C2[CameraManager<br/>摄像头管理器]
        C3[DeviceThread<br/>Pool<br/>设备线程池]
        C4[DeviceSync<br/>Controller<br/>同步控制器]
    end
    
    subgraph "驱动适配层 (Driver Layer)"
        D1[JY5711<br/>Driver<br/>Interface]
        D2[JY5322/5323<br/>Driver<br/>Interface]
        D3[JY8902<br/>Driver<br/>Interface]
        D4[Camera<br/>Driver<br/>Interface]
    end
    
    %% 连接关系 - 保持原有相对位置
    A1 --> B4
    A2 --> B3
    A3 --> B2
    A4 --> B1
    
    B1 --> C4
    B2 --> C3
    B3 --> C2
    B4 --> C1
    
    C1 --> D1
    C2 --> D4
    C3 --> D2
    C4 --> D3
    
    %% 样式设置
    style A1 fill:#e3f2fd
    style A2 fill:#e3f2fd
    style A3 fill:#e3f2fd
    style A4 fill:#e3f2fd
    
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

## 架构层次说明

- **应用层**：用户界面和主要功能模块
- **服务层**：业务逻辑和核心服务
- **设备层**：设备管理和控制
- **驱动适配层**：硬件驱动接口

## 其他格式选项

如果您需要其他格式，我也可以为您生成：
- PlantUML格式
- Graphviz格式
- 或者直接生成HTML文件