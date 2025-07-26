# PCB故障检测系统 - 组件使用指南

## ComponentDiagnosticFramework 使用指南

### 框架概述

ComponentDiagnosticFramework是系统的核心诊断引擎，采用模块化设计，每种元件类型都有专门的诊断器。

### 基础使用

#### 1. 创建诊断管理器

```cpp
#include "ComponentDiagnosticFramework/componentdiagnosticframework.h"

// 创建设备管理器
DeviceManager* deviceManager = new DeviceManager();

// 创建诊断管理器
ComponentDiagnosticManager* diagnosticManager = 
    ComponentDiagnosticFramework::createDiagnosticManager(deviceManager);
```

#### 2. 单个元件诊断

```cpp
// 创建元件规格
ComponentSpec resistor;
resistor.reference = "R1";
resistor.type = ComponentType::RESISTOR;
resistor.nominal_value = 1000.0;  // 1kΩ
resistor.tolerance_percent = 5.0;  // ±5%
resistor.test_voltage = 1.0;       // 1V测试电压

// 同步诊断
ComponentDiagnosticResult result = diagnosticManager->diagnoseComponent(resistor);

// 检查结果
if (result.isPassed) {
    qDebug() << "元件测试通过，健康度:" << result.healthScore;
} else {
    qDebug() << "元件测试失败:" << result.faultTypes;
}
```

#### 3. 异步诊断

```cpp
// 连接信号
connect(diagnosticManager, &ComponentDiagnosticManager::componentDiagnosisCompleted,
        this, [](const QString& taskId, const QString& componentId, 
                const ComponentDiagnosticResult& result) {
    qDebug() << "异步诊断完成:" << componentId << "结果:" << result.isPassed;
});

// 启动异步诊断
QString taskId = diagnosticManager->diagnoseComponentAsync(resistor);
```

#### 4. 批量诊断

```cpp
QVector<ComponentSpec> components = {resistor, capacitor, inductor};

// 同步批量诊断
QVector<ComponentDiagnosticResult> results = 
    diagnosticManager->diagnoseBatch(components);

// 异步批量诊断
QString batchTaskId = diagnosticManager->diagnoseBatchAsync(components);
```

### 各元件类型使用示例

#### 电阻器诊断

```cpp
ComponentSpec resistor;
resistor.reference = "R5";
resistor.type = ComponentType::RESISTOR;
resistor.nominal_value = 10000.0;     // 10kΩ
resistor.tolerance_percent = 1.0;      // ±1%精密电阻
resistor.test_voltage = 0.5;           // 低电压测试避免自热
resistor.max_current = 0.001;          // 1mA最大测试电流
resistor.requires_dmm = true;          // 使用高精度DMM

ComponentDiagnosticResult result = diagnosticManager->diagnoseComponent(resistor);

// 获取详细测量数据
double resistance = result.measurements["resistance"].toDouble();
double voltage = result.measurements["voltage"].toDouble();
double current = result.measurements["current"].toDouble();
```

#### 电容器诊断

```cpp
ComponentSpec capacitor;
capacitor.reference = "C10";
capacitor.type = ComponentType::CAPACITOR;
capacitor.nominal_value = 0.000001;   // 1μF
capacitor.tolerance_percent = 10.0;    // ±10%
capacitor.test_voltage = 1.0;          // 1V AC测试
capacitor.max_esr = 0.1;              // 最大ESR 100mΩ
capacitor.max_leakage = 1e-9;         // 最大漏电流1nA

ComponentDiagnosticResult result = diagnosticManager->diagnoseComponent(capacitor);

// 获取电容特性数据
double capacitance = result.measurements["capacitance"].toDouble();
double esr = result.measurements["esr"].toDouble();
double leakage = result.measurements["leakage_current"].toDouble();
```

#### 电感器诊断

```cpp
ComponentSpec inductor;
inductor.reference = "L2";
inductor.type = ComponentType::INDUCTOR;
inductor.nominal_value = 0.001;       // 1mH
inductor.tolerance_percent = 20.0;     // ±20%
inductor.test_voltage = 0.1;          // 小信号测试

ComponentDiagnosticResult result = diagnosticManager->diagnoseComponent(inductor);

// 获取电感特性数据
double inductance = result.measurements["inductance"].toDouble();
double dcr = result.measurements["dcr"].toDouble();
double q_factor = result.measurements["q_factor"].toDouble();
```

#### 二极管诊断

```cpp
ComponentSpec diode;
diode.reference = "D1";
diode.type = ComponentType::DIODE;
diode.test_current = 0.01;            // 10mA正向测试电流
diode.max_voltage = 0.8;              // 最大正向压降
diode.max_leakage = 1e-6;             // 最大反向漏电流

ComponentDiagnosticResult result = diagnosticManager->diagnoseComponent(diode);

// 获取二极管特性
double forward_voltage = result.measurements["forward_voltage"].toDouble();
double reverse_leakage = result.measurements["reverse_leakage"].toDouble();
```

#### 集成电路诊断

```cpp
ComponentSpec ic;
ic.reference = "U1";
ic.type = ComponentType::IC;
ic.test_voltage = 5.0;                // 5V供电
ic.max_current = 0.1;                 // 最大供电电流100mA

ComponentDiagnosticResult result = diagnosticManager->diagnoseComponent(ic);

// 获取IC状态
double supply_current = result.measurements["supply_current"].toDouble();
bool logic_ok = result.measurements["logic_levels_ok"].toBool();
```

## PCB板卡管理系统

### PCBBoardManager 使用

#### 1. 创建和初始化

```cpp
#include "pcbboardmanager.h"

PCBBoardManager* boardManager = new PCBBoardManager();
boardManager->initializeDatabase();  // 初始化数据库
```

#### 2. 创建新板卡

```cpp
// 加载PCB图像
cv::Mat boardImage = cv::imread("pcb_image.jpg");

// 创建板卡
QString boardId = boardManager->createBoard(
    "Arduino UNO R3",           // 板卡名称
    "ARDUINO_UNO_R3",          // 板卡型号
    boardImage,                 // 板卡图像
    "Arduino官方开发板"        // 描述
);

qDebug() << "创建板卡成功，ID:" << boardId;
```

#### 3. 板卡识别

```cpp
// 从摄像头获取图像
cv::Mat inputImage = cameraManager->getLatestImage(CameraType::HD_CAMERA).image;

// 识别板卡
QList<PCBBoardInfo> candidates = boardManager->identifyBoard(inputImage, 0.3);

if (!candidates.isEmpty()) {
    PCBBoardInfo bestMatch = candidates.first();
    qDebug() << "识别到板卡:" << bestMatch.boardName 
             << "匹配度:" << bestMatch.matchScore;
}
```

#### 4. 元件管理

```cpp
// 获取板卡元件列表
QList<Label> components = boardManager->getComponents(boardId);

// 添加新元件
Label newComponent;
newComponent.class_id = 0;  // 电阻器
newComponent.x = 100;
newComponent.y = 150;
newComponent.width = 20;
newComponent.height = 10;
newComponent.confidence = 0.95;
newComponent.class_name = "resistor";

boardManager->addComponent(boardId, newComponent);
```

### PCBIdentifier 使用

#### 1. 初始化识别器

```cpp
#include "pcbidentifier.h"

PCBIdentifier* identifier = new PCBIdentifier();
identifier->setMatchThreshold(0.3);    // 设置匹配阈值
identifier->setMaxResults(5);          // 最多返回5个结果
```

#### 2. 实时识别

```cpp
// 启动实时识别
CameraManager* cameraManager = getCameraManager();
identifier->startRealtimeIdentification(cameraManager);

// 连接识别结果信号
connect(identifier, &PCBIdentifier::realtimeIdentificationCompleted,
        this, [](const RealtimeIdentificationResult& result) {
    if (result.isValid) {
        qDebug() << "实时识别结果:" << result.modelName 
                 << "置信度:" << result.confidence;
    }
});
```

#### 3. 添加PCB模型

```cpp
// 从图像添加模型
cv::Mat modelImage = cv::imread("new_pcb_model.jpg");
identifier->addPCBModel("New_PCB_Model", modelImage);

// 保存数据库
identifier->saveDatabase();
```

## 摄像头控制系统

### CameraManager 使用

#### 1. 初始化摄像头

```cpp
#include "cameramanager.h"

CameraManager* cameraManager = new CameraManager();

// 初始化高清摄像头
bool hdOk = cameraManager->initializeCamera(CameraType::HD_CAMERA);

// 初始化红外摄像头
bool irOk = cameraManager->initializeCamera(CameraType::IR_CAMERA);
```

#### 2. 摄像头控制

```cpp
// 启动高清摄像头
cameraManager->startCamera(CameraType::HD_CAMERA);

// 设置参数
cameraManager->setHDCameraParams(1920, 1080, 30);  // 1080p@30fps

// 获取最新图像
ImageData imageData = cameraManager->getLatestImage(CameraType::HD_CAMERA);
if (imageData.isValid) {
    cv::Mat image = imageData.image;
    // 处理图像...
}
```

#### 3. 红外温度监测

```cpp
// 启动红外摄像头
cameraManager->startCamera(CameraType::IR_CAMERA);

// 设置温度阈值
cameraManager->setTemperatureThreshold(80.0);  // 80°C报警

// 连接温度报警信号
connect(cameraManager, &CameraManager::temperatureAlert,
        this, [](double temperature, const cv::Point& location) {
    qDebug() << "温度报警:" << temperature << "°C at (" 
             << location.x << "," << location.y << ")";
});

// 获取温度数据
ThermalData thermalData = cameraManager->getLatestThermalData();
if (thermalData.isValid) {
    double maxTemp = thermalData.maxTemp;
    double minTemp = thermalData.minTemp;
    cv::Mat heatMap = thermalData.thermalColorMap;
}
```

#### 4. PCB元件检测

```cpp
// 检测PCB元件
ImageData imageData = cameraManager->getLatestImage(CameraType::HD_CAMERA);
PCBDetectionResult detectionResult = cameraManager->detectPCBComponents(imageData.image);

if (detectionResult.isValid) {
    qDebug() << "检测到" << detectionResult.detectedComponents.size() << "个元件";
    
    // 遍历检测结果
    for (int i = 0; i < detectionResult.componentBounds.size(); ++i) {
        cv::Rect bounds = detectionResult.componentBounds[i];
        QString component = detectionResult.detectedComponents[i];
        double confidence = detectionResult.confidences[i];
        
        qDebug() << "元件:" << component 
                 << "位置:" << bounds.x << "," << bounds.y
                 << "置信度:" << confidence;
    }
}
```

## 接线引导系统

### WiringGuide 使用

#### 1. 端口管理器

```cpp
#include "WiringGuide/portmanager.h"

DeviceManager* deviceManager = getDeviceManager();
PortManager* portManager = new PortManager(deviceManager);

// 初始化端口
portManager->initializePorts();

// 获取可用端口
QList<PortInfo> availablePorts = portManager->getAvailablePorts();
```

#### 2. 资源管理器

```cpp
#include "WiringGuide/wiringresourcemanager.h"

WiringResourceManager* resourceManager = new WiringResourceManager(portManager);

// 分配端口资源
QVector<PortRequirement> requirements = {
    PortRequirement(PortType::ANALOG_OUTPUT, 1, "信号输出"),
    PortRequirement(PortType::ANALOG_INPUT, 2, "信号采集")
};

QMap<QString, QVector<PortInfo>> allocation = 
    resourceManager->allocateResources(requirements, "test_task_1");
```

#### 3. 接线引导对话框

```cpp
#include "WiringGuide/wiringguidedialog.h"

// 创建测试配置
TestConfiguration testConfig;
testConfig.testName = "电阻测试";
testConfig.componentReference = "R1";
// ... 设置其他参数

// 显示接线引导
WiringGuideDialog* wiringDialog = new WiringGuideDialog(testConfig, resourceManager);
wiringDialog->show();

// 连接完成信号
connect(wiringDialog, &WiringGuideDialog::wiringCompleted,
        this, [](const WiringScheme& scheme, const QMap<QString, QVector<PortInfo>>& ports) {
    qDebug() << "接线完成，开始执行测试";
    // 执行测试...
});
```

#### 4. 任务生成器

```cpp
#include "WiringGuide/wiringtaskgenerator.h"

WiringTaskGenerator* taskGenerator = new WiringTaskGenerator(resourceManager);

// 从元件列表生成任务
QVector<ComponentSpec> components = getComponentsFromPCB();
QVector<TestTask> tasks = taskGenerator->generateTasksFromComponents(components);

// 连接任务生成信号
connect(taskGenerator, &WiringTaskGenerator::taskGenerated,
        this, [](const TestTask& task) {
    qDebug() << "生成测试任务:" << task.taskName;
    // 执行任务...
});
```

## 数据管理和导出

### 结果导出器

```cpp
#include "resultexporter.h"

ResultExporter* exporter = new ResultExporter();

// 导出诊断结果
QVector<DiagnosticResult> results = getAllTestResults();
QString fileName = "test_results_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

// 导出为CSV
exporter->exportToCSV(results, fileName + ".csv");

// 导出为JSON
exporter->exportToJSON(results, fileName + ".json");

// 导出为PDF报告
exporter->exportToPDF(results, fileName + ".pdf");
```

### PCB检测数据管理

```cpp
#include "pcbdetectionmanager.h"

PCBDetectionManager* detectionManager = new PCBDetectionManager();
detectionManager->initializeDatabase();

// 保存检测记录
PCBDetectionRecord record;
record.imagePath = "pcb_image.jpg";
record.pcbModel = "Arduino UNO R3";
record.pcbConfidence = 0.95;
record.componentLabels = detectedLabels;
record.totalComponents = detectedLabels.size();

detectionManager->saveDetectionRecord(record);

// 查询历史记录
QList<PCBDetectionRecord> records = detectionManager->getAllRecords();
```

## 信号和槽连接

### 系统级信号连接

```cpp
// 设备状态监控
connect(deviceManager, &DeviceManager::deviceStatusChanged,
        this, &MainWindow::onDeviceStatusChanged);

// 诊断完成处理
connect(diagnosticManager, &ComponentDiagnosticManager::componentDiagnosisCompleted,
        this, &MainWindow::onComponentDiagnosisCompleted);

// 摄像头图像更新
connect(cameraManager, &CameraManager::newImageAvailable,
        this, &MainWindow::onNewImageAvailable);

// 温度报警处理
connect(cameraManager, &CameraManager::temperatureAlert,
        this, &MainWindow::onTemperatureAlert);

// PCB识别结果
connect(pcbIdentifier, &PCBIdentifier::realtimeIdentificationCompleted,
        this, &MainWindow::onPCBIdentificationCompleted);
```

## 错误处理和调试

### 常见错误处理

```cpp
// 设备连接错误
connect(deviceManager, &DeviceManager::errorOccurred,
        this, [](const QString& error) {
    QMessageBox::warning(this, "设备错误", error);
    // 记录错误日志
    qDebug() << "Device error:" << error;
});

// 诊断异常处理
try {
    ComponentDiagnosticResult result = diagnosticManager->diagnoseComponent(component);
} catch (const ComponentDiagnosticFramework::DiagnosticException& e) {
    qDebug() << "诊断异常:" << e.what();
} catch (const std::exception& e) {
    qDebug() << "标准异常:" << e.what();
}

// 摄像头错误处理
connect(cameraManager, &CameraManager::errorOccurred,
        this, [](CameraType type, const QString& error) {
    QString typeStr = (type == CameraType::HD_CAMERA) ? "高清摄像头" : "红外摄像头";
    qDebug() << typeStr << "错误:" << error;
});
```

### 调试和日志

```cpp
// 启用详细日志
qDebug() << "Component diagnostic started for:" << component.reference;
qDebug() << "Test parameters - Voltage:" << component.test_voltage 
         << "Current:" << component.test_current;

// 性能监控
QElapsedTimer timer;
timer.start();
ComponentDiagnosticResult result = diagnosticManager->diagnoseComponent(component);
qDebug() << "Diagnosis completed in" << timer.elapsed() << "ms";

// 设备状态查询
DeviceStatus status = deviceManager->getDeviceStatus("JY5711");
qDebug() << "Device JY5711 status:" << static_cast<int>(status);
```

---

*本指南涵盖了系统主要组件的使用方法，更多详细信息请参考相应的头文件和实现文件。*