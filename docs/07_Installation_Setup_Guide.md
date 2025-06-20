# PCB故障检测系统 - 安装与配置指南

## 目录
1. [系统要求](#系统要求)
2. [硬件安装](#硬件安装)
3. [软件安装](#软件安装)
4. [驱动程序配置](#驱动程序配置)
5. [项目编译](#项目编译)
6. [系统配置](#系统配置)
7. [设备连接验证](#设备连接验证)
8. [常见问题排除](#常见问题排除)

## 系统要求

### 硬件要求
- **操作系统**: Windows 10/11 (64位)
- **处理器**: Intel Core i5 或 AMD Ryzen 5 及以上
- **内存**: 8GB RAM 最低，推荐16GB
- **存储空间**: 至少2GB可用空间
- **USB接口**: 可用的USB 2.0/3.0接口用于设备连接

### 测试设备要求
系统支持以下硬件设备：
- **JY5711 AO设备**: 模拟输出设备
- **JY5322 DAQ设备**: 数据采集设备（插槽5）
- **JY5323 DAQ设备**: 数据采集设备（插槽3）
- **JY8902 DMM设备**: 数字万用表设备

### 软件依赖
- **Qt Framework**: 5.12.0 或更高版本
- **Qt Creator**: 推荐使用4.8.0或更高版本
- **编译器**: MSVC 2017/2019/2022 或 MinGW
- **CMake**: 3.16.0或更高版本（可选）

## 硬件安装

### 1. 设备连接顺序
```
1. 确保所有设备断电
2. 按以下顺序连接设备：
   - JY5711 AO设备 → USB端口
   - JY5322 DAQ设备 → 插槽5
   - JY5323 DAQ设备 → 插槽3
   - JY8902 DMM设备 → USB端口
3. 确保所有连接牢固
4. 按设备说明书上电
```

### 2. 硬件检查清单
```
□ 所有设备LED指示灯正常
□ USB连接线无损坏
□ 设备固件版本兼容
□ 电源供应稳定
□ 接地连接良好
```

### 3. 设备槽位配置
```cpp
// 设备槽位映射
JY5322 → Slot 5  // 第一个DAQ设备
JY5323 → Slot 3  // 第二个DAQ设备
JY5711 → USB通信 // AO设备
JY8902 → USB通信 // DMM设备
```

## 软件安装

### 1. Qt环境安装

#### Windows平台
```powershell
# 下载Qt在线安装器
# 从 https://www.qt.io/download-qt-installer 下载

# 或使用离线安装包
# 选择Qt 5.12.12 LTS版本
# 包含以下组件：
- Qt 5.12.12
  - MSVC 2017 64-bit
  - Qt Creator 4.15.2
  - Qt Quick Controls 2
  - Qt Charts
```

#### 验证Qt安装
```powershell
# 检查Qt版本
qmake --version

# 检查Qt Creator
qtcreator --version
```

### 2. 编译工具链安装

#### MSVC编译器
```powershell
# 安装Visual Studio Community 2019/2022
# 确保包含以下组件：
- MSVC v142/v143 compiler toolset
- Windows 10/11 SDK
- CMake tools for C++
```

#### 验证编译环境
```powershell
# 检查MSVC编译器
cl

# 检查CMake
cmake --version

# 检查nmake
nmake /?
```

## 驱动程序配置

### 1. 设备驱动安装

#### JY5711驱动安装
```powershell
# 1. 连接JY5711设备到计算机
# 2. 运行设备管理器
devmgmt.msc

# 3. 查找未识别设备
# 4. 右键选择"更新驱动程序"
# 5. 浏览到驱动程序文件夹：
cd "d:\FaultDetect\Program\FaultDetect\PTS\JYDevice\drivers\JY5711"

# 6. 选择对应的.inf文件安装
```

#### JY5322/5323驱动安装
```powershell
# DAQ设备驱动安装
cd "d:\FaultDetect\Program\FaultDetect\PTS\JYDevice\drivers\JY5320"

# 安装步骤：
# 1. 以管理员身份运行命令提示符
# 2. 执行驱动安装脚本
install_driver.bat

# 3. 重启计算机使驱动生效
```

#### JY8902驱动安装
```powershell
# DMM设备驱动安装
cd "d:\FaultDetect\Program\FaultDetect\PTS\JYDevice\drivers\JY8902"

# 使用设备制造商提供的安装程序
setup.exe
```

### 2. 驱动验证
```cpp
// 在代码中验证设备连接
bool DeviceManager::checkDeviceAvailability()
{
    // JY5711设备检查
    JY5710_DeviceHandle aoHandle = nullptr;
    if (JY5710_Open(0, &aoHandle) == 0) {
        JY5710_Close(aoHandle);
        qDebug() << "JY5711 available";
    }
    
    // JY5322设备检查（插槽5）
    JY5320_DeviceHandle daqHandle5322 = nullptr;
    if (JY5320_Open(5, &daqHandle5322) == 0) {
        JY5320_Close(daqHandle5322);
        qDebug() << "JY5322 available";
    }
    
    // JY5323设备检查（插槽3）
    JY5320_DeviceHandle daqHandle5323 = nullptr;
    if (JY5320_Open(3, &daqHandle5323) == 0) {
        JY5320_Close(daqHandle5323);
        qDebug() << "JY5323 available";
    }
    
    // JY8902设备检查
    JY8902_DeviceHandle dmmHandle = nullptr;
    if (JY8902_Open(0, &dmmHandle) == 0) {
        JY8902_Close(dmmHandle);
        qDebug() << "JY8902 available";
    }
    
    return true;
}
```

## 项目编译

### 1. 源码获取
```powershell
# 克隆项目仓库
git clone <repository-url>
cd FaultDetect\Program\FaultDetect\PTS

# 或者解压源码包到指定目录
# 确保路径为: d:\FaultDetect\Program\FaultDetect\PTS\
```

### 2. 依赖库配置

#### 设置环境变量
```powershell
# 添加Qt路径到环境变量
$env:QTDIR = "C:\Qt\5.12.12\msvc2017_64"
$env:PATH += ";$env:QTDIR\bin"

# 添加设备库路径
$env:JY_DEVICE_PATH = "d:\FaultDetect\Program\FaultDetect\PTS\JYDevice"
```

#### 库文件检查
```powershell
# 检查必需的库文件
ls "d:\FaultDetect\Program\FaultDetect\PTS\lib\"

# 应包含以下文件：
- JY5710.lib    # AO设备库
- JY5320.lib    # DAQ设备库  
- JY8902.lib    # DMM设备库
- 其他依赖库...
```

### 3. 项目编译步骤

#### 使用Qt Creator编译
```
1. 打开Qt Creator
2. 文件 → 打开文件或项目
3. 选择 PTS.pro 文件
4. 配置项目构建套件：
   - 编译器：MSVC2017/2019
   - Qt版本：5.12.12
   - 构建目录：build/
5. 构建 → 构建项目 "PTS"
```

#### 使用命令行编译
```powershell
# 进入项目目录
cd "d:\FaultDetect\Program\FaultDetect\PTS"

# 生成Makefile
qmake PTS.pro

# 编译项目
nmake

# 或者使用并行编译
nmake -j4
```

#### 调试版本编译
```powershell
# 编译调试版本
qmake CONFIG+=debug PTS.pro
nmake

# 或在Qt Creator中选择Debug模式
```

### 4. 编译验证
```powershell
# 检查生成的可执行文件
ls "d:\FaultDetect\Program\FaultDetect\PTS\release\PTS.exe"

# 检查依赖的DLL文件
windeployqt --debug --compiler-runtime release\PTS.exe
```

## 系统配置

### 1. 配置文件设置

#### config.ini配置
```ini
[System]
version=1.0.0
debug_mode=false
log_level=INFO

[Devices]
jy5711_enabled=true
jy5322_enabled=true
jy5322_slot=5
jy5323_enabled=true
jy5323_slot=3
jy8902_enabled=true

[Timeouts]
device_init_timeout=25000
operation_timeout=5000
measurement_timeout=3000

[Paths]
log_path=./logs/
result_path=./results/
config_path=./config/
```

#### 日志配置
```cpp
// 日志系统配置
void MainWindow::initializeLogging()
{
    // 创建日志目录
    QDir logDir("./logs");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
    
    // 配置日志文件
    QString logFile = QString("./logs/pts_%1.log")
                      .arg(QDateTime::currentDateTime()
                      .toString("yyyyMMdd_hhmmss"));
    
    // 设置日志输出
    qInstallMessageHandler(customMessageOutput);
}
```

### 2. 数据库配置（可选）
```sql
-- 创建测试结果数据库
CREATE DATABASE PCBTestResults;
USE PCBTestResults;

-- 创建测试记录表
CREATE TABLE TestRecords (
    id INT AUTO_INCREMENT PRIMARY KEY,
    pcb_id VARCHAR(50),
    test_time DATETIME,
    test_result ENUM('PASS', 'FAIL'),
    details TEXT
);

-- 创建故障记录表
CREATE TABLE FaultRecords (
    id INT AUTO_INCREMENT PRIMARY KEY,
    test_id INT,
    component_type VARCHAR(20),
    component_id VARCHAR(50),
    fault_type VARCHAR(50),
    measured_value DOUBLE,
    expected_value DOUBLE,
    FOREIGN KEY (test_id) REFERENCES TestRecords(id)
);
```

### 3. 权限配置
```powershell
# 确保程序具有必要权限
# 以管理员身份运行PowerShell

# 设置程序文件权限
icacls "d:\FaultDetect\Program\FaultDetect\PTS" /grant Users:F /T

# 设置设备访问权限
# 在组策略中允许硬件设备访问
gpedit.msc
```

## 设备连接验证

### 1. 设备检测工具
```cpp
// 设备检测实用程序
class DeviceDetectionTool
{
public:
    static bool checkAllDevices()
    {
        qDebug() << "=== 设备连接检测 ===";
        
        bool allConnected = true;
        
        // 检测JY5711
        if (checkJY5711()) {
            qDebug() << "✓ JY5711 AO设备: 已连接";
        } else {
            qDebug() << "✗ JY5711 AO设备: 未连接";
            allConnected = false;
        }
        
        // 检测JY5322
        if (checkJY5322()) {
            qDebug() << "✓ JY5322 DAQ设备: 已连接";
        } else {
            qDebug() << "✗ JY5322 DAQ设备: 未连接";
            allConnected = false;
        }
        
        // 检测JY5323
        if (checkJY5323()) {
            qDebug() << "✓ JY5323 DAQ设备: 已连接";
        } else {
            qDebug() << "✗ JY5323 DAQ设备: 未连接";
            allConnected = false;
        }
        
        // 检测JY8902
        if (checkJY8902()) {
            qDebug() << "✓ JY8902 DMM设备: 已连接";
        } else {
            qDebug() << "✗ JY8902 DMM设备: 未连接";
            allConnected = false;
        }
        
        return allConnected;
    }
    
private:
    static bool checkJY5711()
    {
        JY5710_DeviceHandle handle = nullptr;
        int result = JY5710_Open(0, &handle);
        if (result == 0) {
            JY5710_Close(handle);
            return true;
        }
        return false;
    }
    
    static bool checkJY5322()
    {
        JY5320_DeviceHandle handle = nullptr;
        int result = JY5320_Open(5, &handle);
        if (result == 0) {
            JY5320_Close(handle);
            return true;
        }
        return false;
    }
    
    static bool checkJY5323()
    {
        JY5320_DeviceHandle handle = nullptr;
        int result = JY5320_Open(3, &handle);
        if (result == 0) {
            JY5320_Close(handle);
            return true;
        }
        return false;
    }
    
    static bool checkJY8902()
    {
        JY8902_DeviceHandle handle = nullptr;
        int result = JY8902_Open(0, &handle);
        if (result == 0) {
            JY8902_Close(handle);
            return true;
        }
        return false;
    }
};
```

### 2. 连接验证脚本
```powershell
# 设备连接验证脚本
# device_check.ps1

Write-Host "PCB故障检测系统 - 设备连接检查" -ForegroundColor Green

# 检查USB设备
Write-Host "`n检查USB设备连接..." -ForegroundColor Yellow
Get-WmiObject -Class Win32_USBHub | Select-Object Name, DeviceID

# 检查COM端口
Write-Host "`n检查串口设备..." -ForegroundColor Yellow
Get-WmiObject -Class Win32_SerialPort | Select-Object Name, DeviceID

# 检查设备管理器中的未知设备
Write-Host "`n检查未知设备..." -ForegroundColor Yellow
Get-WmiObject -Class Win32_PnPEntity | Where-Object {$_.ConfigManagerErrorCode -ne 0} | Select-Object Name, ConfigManagerErrorCode

Write-Host "`n设备检查完成" -ForegroundColor Green
```

### 3. 自动化测试
```cpp
// 系统自检程序
bool MainWindow::performSystemSelfTest()
{
    qDebug() << "开始系统自检...";
    
    // 1. 设备连接检查
    if (!DeviceDetectionTool::checkAllDevices()) {
        QMessageBox::critical(this, "自检失败", "设备连接检查失败！");
        return false;
    }
    
    // 2. 设备初始化测试
    DeviceManager* deviceManager = new DeviceManager(this);
    if (!deviceManager->initializeDeviceThreads()) {
        QMessageBox::critical(this, "自检失败", "设备初始化失败！");
        return false;
    }
    
    // 3. 基本功能测试
    if (!performBasicFunctionTest(deviceManager)) {
        QMessageBox::critical(this, "自检失败", "基本功能测试失败！");
        return false;
    }
    
    // 4. 清理资源
    deviceManager->shutdownDeviceThreads();
    delete deviceManager;
    
    QMessageBox::information(this, "自检完成", "系统自检通过！");
    return true;
}

bool MainWindow::performBasicFunctionTest(DeviceManager* deviceManager)
{
    // AO设备测试
    if (!deviceManager->outputVoltage(0, 1.0)) {
        qDebug() << "AO设备测试失败";
        return false;
    }
    
    // DAQ设备测试
    double voltage;
    if (!deviceManager->measureVoltage("JY5322", 0, voltage)) {
        qDebug() << "JY5322测试失败";
        return false;
    }
    
    if (!deviceManager->measureVoltage("JY5323", 0, voltage)) {
        qDebug() << "JY5323测试失败";
        return false;
    }
    
    // DMM设备测试
    double resistance;
    if (!deviceManager->measureResistance(resistance)) {
        qDebug() << "DMM设备测试失败";
        return false;
    }
    
    return true;
}
```

## 常见问题排除

### 1. 设备连接问题

#### 问题：设备无法识别
```
症状：设备管理器中显示未知设备
解决方案：
1. 重新安装设备驱动程序
2. 检查USB连接线是否损坏
3. 尝试不同的USB端口
4. 重启计算机
```

#### 问题：设备初始化超时
```cpp
// 增加初始化超时时间
const int timeout = 30000; // 从25秒增加到30秒

// 或者分别设置不同设备的超时时间
QMap<QString, int> deviceTimeouts;
deviceTimeouts["JY5711"] = 10000;  // AO设备10秒
deviceTimeouts["JY5322"] = 15000;  // DAQ设备15秒
deviceTimeouts["JY5323"] = 15000;  // DAQ设备15秒
deviceTimeouts["JY8902"] = 20000;  // DMM设备20秒
```

### 2. 编译问题

#### 问题：找不到头文件
```powershell
# 解决方案：添加包含路径
# 在PTS.pro中添加：
INCLUDEPATH += $$PWD/include
INCLUDEPATH += $$PWD/JYDevice/include
```

#### 问题：链接错误
```powershell
# 解决方案：检查库文件路径
# 在PTS.pro中确认：
LIBS += -L$$PWD/lib -lJY5710
LIBS += -L$$PWD/lib -lJY5320  
LIBS += -L$$PWD/lib -lJY8902
```

### 3. 运行时问题

#### 问题：DLL缺失
```powershell
# 解决方案：部署Qt DLL
windeployqt --debug --compiler-runtime PTS.exe

# 或手动复制所需DLL到程序目录
copy "C:\Qt\5.12.12\msvc2017_64\bin\Qt5Core.dll" .
copy "C:\Qt\5.12.12\msvc2017_64\bin\Qt5Gui.dll" .
copy "C:\Qt\5.12.12\msvc2017_64\bin\Qt5Widgets.dll" .
```

#### 问题：权限不足
```powershell
# 解决方案：以管理员身份运行
# 或修改程序权限设置
# 在程序属性中设置"以管理员身份运行此程序"
```

### 4. 性能问题

#### 问题：设备响应慢
```cpp
// 解决方案：优化设备操作
// 1. 使用异步操作
bool DeviceManager::measureVoltageAsync(const QString& deviceName, 
                                       int channel, double& result, 
                                       int timeout_ms)
{
    // 异步操作实现
    DeviceOperation operation;
    operation.command = DeviceCommand::READ_DATA;
    operation.channel = channel;
    operation.timeout = timeout_ms;
    
    return submitOperation(deviceName, operation);
}

// 2. 实现操作队列
class OperationQueue
{
    QQueue<DeviceOperation> operations_;
    QMutex queueMutex_;
    
public:
    void enqueue(const DeviceOperation& op) {
        QMutexLocker locker(&queueMutex_);
        operations_.enqueue(op);
    }
    
    DeviceOperation dequeue() {
        QMutexLocker locker(&queueMutex_);
        return operations_.dequeue();
    }
};
```

### 5. 日志和调试

#### 启用详细日志
```cpp
// 在main.cpp中设置日志级别
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 启用详细调试输出
    QLoggingCategory::setFilterRules("*=true");
    qSetMessagePattern("%{time hh:mm:ss.zzz} [%{type}] %{category}: %{message}");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
```

#### 创建调试配置文件
```ini
# debug.ini
[Logging]
level=DEBUG
file=./logs/debug.log
console=true

[Devices]
simulation_mode=false
timeout_multiplier=2.0

[Testing]
stop_on_first_error=false
detailed_reporting=true
```

### 6. 系统维护

#### 定期维护任务
```powershell
# 系统维护脚本 - maintenance.ps1

# 1. 清理日志文件（保留最近30天）
Get-ChildItem ".\logs\*.log" | Where-Object {$_.CreationTime -lt (Get-Date).AddDays(-30)} | Remove-Item

# 2. 清理临时文件
Remove-Item ".\temp\*" -Recurse -Force

# 3. 备份配置文件
Copy-Item ".\config.ini" ".\backup\config_$(Get-Date -Format 'yyyyMMdd').ini"

# 4. 检查磁盘空间
$disk = Get-WmiObject -Class Win32_LogicalDisk | Where-Object {$_.DeviceID -eq "C:"}
$freeSpaceGB = [math]::Round($disk.FreeSpace / 1GB, 2)
Write-Host "可用磁盘空间: $freeSpaceGB GB"

# 5. 验证设备连接
& "device_check.ps1"
```

## 总结

本安装与配置指南涵盖了PCB故障检测系统的完整部署流程，包括：

1. **系统要求确认** - 确保硬件和软件环境满足要求
2. **硬件安装** - 正确连接和配置测试设备
3. **软件安装** - Qt环境和编译工具链的安装
4. **驱动配置** - 各种设备驱动程序的安装和配置  
5. **项目编译** - 源码编译和依赖库配置
6. **系统配置** - 配置文件和系统参数设置
7. **设备验证** - 连接测试和功能验证
8. **问题排除** - 常见问题的诊断和解决方案

遵循本指南可以确保系统正确安装和配置，为后续的PCB故障检测工作提供可靠的基础环境。

---
*文档版本: 1.0*  
*更新日期: 2025年6月*  
*适用版本: PCB故障检测系统 v1.0+*
