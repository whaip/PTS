# 硬件系统架构图

## Mermaid格式

```mermaid
graph TB
    %% 主控层
    subgraph "🖥️ 主控计算机系统"
        direction LR
        A1[" 🎛️ DeviceManager<br/>设备管理器<br/>━━━━━━━━━<br/>• 设备初始化<br/>• 状态监控<br/>• 资源调度"]
        A2[" ⚡ SyncController<br/>同步控制器<br/>━━━━━━━━━<br/>• 时序控制<br/>• 触发管理<br/>• 精确同步"]
        A3[" 📊 DataProcessor<br/>数据处理器<br/>━━━━━━━━━<br/>• 实时处理<br/>• 算法分析<br/>• 结果输出"]
        
        A1 ~~~ A2 ~~~ A3
    end
    
    %% 执行层
    subgraph "🔧 信号激励与控制层"
        direction TB
        B1[" 🌊 JY5711 AO<br/>模拟输出设备<br/>━━━━━━━━━━━<br/>• 多路信号激励<br/>• 精确波形生成<br/>• 幅度/频率控制"]
        
        B2[" 🎯 Sync Trigger Hub<br/>同步触发分配器<br/>━━━━━━━━━━━━<br/>• 精准时序分发<br/>• 多设备同步<br/>• 触发信号管理"]
    end
    
    %% 检测层
    subgraph "👁️ 智能检测层"
        direction LR
        B3[" 📷 HD Camera<br/>高清视觉系统<br/>━━━━━━━━━━<br/>• 元器件识别<br/>• 缺陷检测<br/>• 位置定位"]
        
        B4[" 🌡️ Thermal Camera<br/>红外热成像系统<br/>━━━━━━━━━━━<br/>• 实时温度监测<br/>• 热点分布分析<br/>• 过热预警"]
        
        B3 ~~~ B4
    end
    
    %% 采集层
    subgraph "📡 高精度数据采集层"
        direction TB
        C1[" 📈 JY5322 DAQ<br/>高速数据采集系统<br/>━━━━━━━━━━━━<br/>• 32通道同步采集<br/>• 1MSPS采样率<br/>• 16位分辨率"]
        
        C2[" 📉 JY5323 DAQ<br/>多功能数据采集<br/>━━━━━━━━━━━━<br/>• 64通道输入<br/>• 多种信号类型<br/>• 实时数据流"]
        
        C3[" 🔍 JY8902 DMM<br/>高精度数字万用表<br/>━━━━━━━━━━━━━<br/>• 8.5位测量精度<br/>• 多参数检测<br/>• 自动量程切换"]
        
        C1 ~~~ C2 ~~~ C3
    end
    
    %% 物理接口层
    subgraph "🔌 测试接口层"
        D1[" ⚙️ Intelligent Test Fixture<br/>智能测试夹具<br/>━━━━━━━━━━━━━━<br/>• 自动PCB定位<br/>• 多点探针接触<br/>• 压力反馈控制<br/>• 快速装夹机构"]
    end
    
    %% 主要连接线 - 加粗控制线
    A1 ==>|"🔗 USB 3.0/Ethernet"| B1
    A1 ==>|"⚡ Thunderbolt 4"| C1
    A1 ==>|"⚡ Thunderbolt 4"| C2
    A1 ==>|"⚡ Thunderbolt 4"| C3
    
    A2 ==>|"📡 Sync Trigger"| B2
    A3 ==>|"📺 Video Stream"| B3
    
    %% 次要连接线 - 同步信号
    B2 -.->|"🎯 Trigger Signal"| B4
    B2 -.->|"🎯 Trigger Signal"| C1
    B2 -.->|"🎯 Trigger Signal"| C2
    
    %% 设备间同步
    C1 <-.->|"🔄 Sync Link"| C2
    C2 <-.->|"🔄 Sync Link"| C3
    
    %% 物理连接 - 测试信号
    B1 -->|"📊 Test Signals"| D1
    B3 -->|"👀 Visual Detection"| D1
    B4 -.->|"🌡️ Thermal Monitor"| D1
    
    C1 -->|"📐 Measurement"| D1
    C2 -->|"📐 Measurement"| D1
    C3 -->|"📐 Precision Test"| D1
    
    %% 美化样式
    classDef controlSystem fill:#1565C0,stroke:#0D47A1,stroke-width:3px,color:#fff
    classDef signalLayer fill:#7B1FA2,stroke:#4A148C,stroke-width:3px,color:#fff
    classDef detectionLayer fill:#388E3C,stroke:#1B5E20,stroke-width:3px,color:#fff
    classDef acquisitionLayer fill:#F57C00,stroke:#E65100,stroke-width:3px,color:#fff
    classDef fixtureLayer fill:#D32F2F,stroke:#B71C1C,stroke-width:3px,color:#fff
    
    class A1,A2,A3 controlSystem
    class B1,B2 signalLayer
    class B3,B4 detectionLayer
    class C1,C2,C3 acquisitionLayer
    class D1 fixtureLayer
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