#include "devicemanagertestwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QApplication>
#include <QDateTime>

DeviceManagerTestWindow::DeviceManagerTestWindow(DeviceManager* deviceManager, QWidget *parent)
    : QDialog(parent)
    , deviceManager_(deviceManager)
    , statusUpdateTimer_(new QTimer(this))
{
    setWindowTitle("DeviceManager 测试窗口");
    setMinimumSize(800, 600);
    
    setupUI();
    
    // 连接设备管理器的信号
    if (deviceManager_) {
        connect(deviceManager_, &DeviceManager::deviceStatusChanged,
                this, &DeviceManagerTestWindow::onDeviceStatusChanged);
        connect(deviceManager_, &DeviceManager::errorOccurred,
                this, &DeviceManagerTestWindow::onErrorOccurred);
    }
    
    // 设置状态更新定时器
    connect(statusUpdateTimer_, &QTimer::timeout,
            this, &DeviceManagerTestWindow::updateDeviceStatusDisplay);
    statusUpdateTimer_->start(1000); // 每秒更新一次
    
    // 初始化设备状态显示
    updateDeviceStatusDisplay();
}

DeviceManagerTestWindow::~DeviceManagerTestWindow()
{
}

void DeviceManagerTestWindow::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    
    // 创建测试按钮组
    testButtonsGroup_ = new QGroupBox("测试功能", this);
    QGridLayout* buttonLayout = new QGridLayout(testButtonsGroup_);
    
    initDevicesBtn_ = new QPushButton("初始化设备", this);
    measureVoltageBtn_ = new QPushButton("测量电压", this);
    measureCurrentBtn_ = new QPushButton("测量电流", this);
    measureResistanceBtn_ = new QPushButton("测量电阻", this);
    outputVoltageBtn_ = new QPushButton("输出电压", this);
    deviceStatusBtn_ = new QPushButton("检查设备状态", this);
    clearBtn_ = new QPushButton("清除结果", this);
    
    buttonLayout->addWidget(initDevicesBtn_, 0, 0);
    buttonLayout->addWidget(measureVoltageBtn_, 0, 1);
    buttonLayout->addWidget(measureCurrentBtn_, 0, 2);
    buttonLayout->addWidget(measureResistanceBtn_, 1, 0);
    buttonLayout->addWidget(outputVoltageBtn_, 1, 1);
    buttonLayout->addWidget(deviceStatusBtn_, 1, 2);
    buttonLayout->addWidget(clearBtn_, 2, 0);
    
    // 添加AO单点输出测试按钮
    QPushButton* ao_single_test_btn = new QPushButton("测试AO单点输出");
    ao_single_test_btn->setToolTip("测试模拟输出单点模式");
    buttonLayout->addWidget(ao_single_test_btn, 2, 1);
    
    // 创建参数设置组
    parametersGroup_ = new QGroupBox("测试参数", this);
    QFormLayout* paramLayout = new QFormLayout(parametersGroup_);
    
    channelSpin_ = new QSpinBox(this);
    channelSpin_->setRange(0, 31);
    channelSpin_->setValue(0);
    
    voltageSpin_ = new QDoubleSpinBox(this);
    voltageSpin_->setRange(-10.0, 10.0);
    voltageSpin_->setSingleStep(0.1);
    voltageSpin_->setValue(1.0);
    voltageSpin_->setSuffix(" V");
    
    deviceCombo_ = new QComboBox(this);
    deviceCombo_->addItems({"JY5322", "JY5323", "JY8902", "JY5711"});
    
    timeoutSpin_ = new QSpinBox(this);
    timeoutSpin_->setRange(1000, 30000);
    timeoutSpin_->setValue(5000);
    timeoutSpin_->setSuffix(" ms");
    
    paramLayout->addRow("通道:", channelSpin_);
    paramLayout->addRow("电压值:", voltageSpin_);
    paramLayout->addRow("设备:", deviceCombo_);
    paramLayout->addRow("超时时间:", timeoutSpin_);
    
    // 创建设备状态组
    statusGroup_ = new QGroupBox("设备状态", this);
    QGridLayout* statusLayout = new QGridLayout(statusGroup_);
    
    aoStatusLabel_ = new QLabel("AO (JY5711): 未知", this);
    daq5322StatusLabel_ = new QLabel("DAQ5322: 未知", this);
    daq5323StatusLabel_ = new QLabel("DAQ5323: 未知", this);
    dmmStatusLabel_ = new QLabel("DMM (JY8902): 未知", this);
    
    testProgress_ = new QProgressBar(this);
    testProgress_->setVisible(false);
    
    statusLayout->addWidget(aoStatusLabel_, 0, 0);
    statusLayout->addWidget(daq5322StatusLabel_, 0, 1);
    statusLayout->addWidget(daq5323StatusLabel_, 1, 0);
    statusLayout->addWidget(dmmStatusLabel_, 1, 1);
    statusLayout->addWidget(testProgress_, 2, 0, 1, 2);
    
    // 创建结果显示组
    resultsGroup_ = new QGroupBox("测试结果", this);
    QVBoxLayout* resultLayout = new QVBoxLayout(resultsGroup_);
    
    resultText_ = new QTextEdit(this);
    resultText_->setReadOnly(true);
    resultText_->setMaximumHeight(200);
    
    resultLayout->addWidget(resultText_);
    
    // 将所有组添加到主布局
    mainLayout_->addWidget(testButtonsGroup_);
    mainLayout_->addWidget(parametersGroup_);
    mainLayout_->addWidget(statusGroup_);
    mainLayout_->addWidget(resultsGroup_);
    
    // 连接按钮信号
    connect(initDevicesBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testInitializeDevices);
    connect(measureVoltageBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testMeasureVoltage);
    connect(measureCurrentBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testMeasureCurrent);
    connect(measureResistanceBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testMeasureResistance);
    connect(outputVoltageBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testOutputVoltage);
    connect(deviceStatusBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::testDeviceStatus);
    connect(clearBtn_, &QPushButton::clicked, this, &DeviceManagerTestWindow::clearResults);
}

void DeviceManagerTestWindow::testInitializeDevices()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    
    appendResult("开始初始化设备...");
    testProgress_->setVisible(true);
    testProgress_->setRange(0, 0); // 显示无限进度条
    
    bool success = deviceManager_->initializeDeviceThreads();
    
    testProgress_->setVisible(false);
    
    if (success) {
        appendResult("设备初始化成功!");
    } else {
        appendResult("设备初始化失败: " + deviceManager_->getLastError());
    }
}

void DeviceManagerTestWindow::testMeasureVoltage()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    
    QString deviceName = deviceCombo_->currentText();
    int channel = channelSpin_->value();
    int timeout = timeoutSpin_->value();
    
    appendResult(QString("开始测量电压 - 设备: %1, 通道: %2").arg(deviceName).arg(channel));
    
    double result = 0.0;
    bool success = deviceManager_->measureVoltage(deviceName, channel, result, timeout);
    
    if (success) {
        appendResult(QString("电压测量成功: %1 V").arg(result, 0, 'f', 6));
    } else {
        appendResult("电压测量失败: " + deviceManager_->getLastError());
    }
}

void DeviceManagerTestWindow::testMeasureCurrent()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    
    int channel = channelSpin_->value();
    int timeout = timeoutSpin_->value();
    
    appendResult(QString("开始测量电流 - 通道: %1").arg(channel));
    
    double result = 0.0;
    bool success = deviceManager_->measureCurrent(channel, result, timeout);
    
    if (success) {
        appendResult(QString("电流测量成功: %1 A").arg(result, 0, 'f', 6));
    } else {
        appendResult("电流测量失败: " + deviceManager_->getLastError());
    }
}

void DeviceManagerTestWindow::testMeasureResistance()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    DeviceOperation configOp;
    configOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    configOp.parameters["channelCount"] = 1;
    configOp.parameters["sampleRate"] = 1000000.0;

    QVariantList waveforms;
    // 通道0 - 高电平
    QVariantMap HighLevel;
    HighLevel["channel"] = 0;
    HighLevel["type"] = static_cast<int>(PXIe5711_testtype::HighLevelWave);
    HighLevel["amplitude"] = 8.0;
    HighLevel["frequency"] = 1000.0;
    HighLevel["lowRange"] = -10.0;
    HighLevel["highRange"] = 10.0;
    waveforms.append(HighLevel);

    configOp.parameters["waveforms"] = waveforms;

    deviceManager_->submitOperation("JY5711", configOp);
    DeviceResult configResult = deviceManager_->waitForResult("JY5711", 10000);

    if (configResult.success) {
        DeviceOperation outputOp;
        outputOp.command = DeviceCommand::WRITE_DATA;
        outputOp.parameters["waveforms"] = waveforms;
        outputOp.parameters["sampleRate"] = 1000000;
        outputOp.parameters["samplesPerChannel"] = 1000000; // 1秒的数据

        deviceManager_->submitOperation("JY5711", outputOp);
        DeviceResult outputResult = deviceManager_->waitForResult("JY5711", 15000);

        if (outputResult.success) {
            qDebug() << "Waveform output started successfully";

            // 发送软件触发开始输出
            DeviceOperation triggerOp;
            triggerOp.command = DeviceCommand::SYNC_TRIGGER;
            deviceManager_->submitOperation("JY5711", triggerOp);
        }
    } else {
        appendResult("电压输出失败: " + deviceManager_->getLastError());
    }
}

void DeviceManagerTestWindow::testOutputVoltage()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    
    DeviceOperation configOp;
    configOp.command = DeviceCommand::CONFIGURE_CHANNEL;
    configOp.parameters["channelCount"] = 3;
    configOp.parameters["sampleRate"] = 1000000.0;
    
    QVariantList waveforms;
    // 通道0 - 高电平
    QVariantMap HighLevel;
    HighLevel["channel"] = 0;
    HighLevel["type"] = static_cast<int>(PXIe5711_testtype::HighLevelWave);
    HighLevel["amplitude"] = 5.0;
    HighLevel["frequency"] = 1000.0;
    HighLevel["lowRange"] = -10.0;
    HighLevel["highRange"] = 10.0;
    waveforms.append(HighLevel);

    // 通道1 - 方波
    QVariantMap square;
    square["channel"] = 1;
    square["type"] = static_cast<int>(PXIe5711_testtype::SquareWave);
    square["amplitude"] = 3.0;
    square["frequency"] = 500.0;
    square["dutyCycle"] = 0.3;
    square["lowRange"] = -10.0;
    square["highRange"] = 10.0;
    waveforms.append(square);

    // 通道2 - 三角波
    QVariantMap triangle;
    triangle["channel"] = 2;
    triangle["type"] = static_cast<int>(PXIe5711_testtype::TriangleWave);
    triangle["amplitude"] = 2.0;
    triangle["frequency"] = 2000.0;
    triangle["lowRange"] = -10.0;
    triangle["highRange"] = 10.0;
    waveforms.append(triangle);

    configOp.parameters["waveforms"] = waveforms;
    
    deviceManager_->submitOperation("JY5711", configOp);
    DeviceResult configResult = deviceManager_->waitForResult("JY5711", 10000);

    if (configResult.success) {
        DeviceOperation outputOp;
        outputOp.command = DeviceCommand::WRITE_DATA;
        outputOp.parameters["waveforms"] = waveforms;
        outputOp.parameters["sampleRate"] = 1000000;
        outputOp.parameters["samplesPerChannel"] = 1000000; // 1秒的数据
        
        deviceManager_->submitOperation("JY5711", outputOp);
        DeviceResult outputResult = deviceManager_->waitForResult("JY5711", 15000);
        
        if (outputResult.success) {
            qDebug() << "Waveform output started successfully";
            
            // 发送软件触发开始输出
            DeviceOperation triggerOp;
            triggerOp.command = DeviceCommand::SYNC_TRIGGER;
            deviceManager_->submitOperation("JY5711", triggerOp);
        }
    } else {
        appendResult("电压输出失败: " + deviceManager_->getLastError());
    }
}

void DeviceManagerTestWindow::testDeviceStatus()
{
    if (!deviceManager_) {
        appendResult("错误: DeviceManager 未初始化");
        return;
    }
    
    appendResult("检查设备状态...");
    
    QStringList deviceNames = {"JY5711", "JY5322", "JY5323", "JY8902"};
    
    for (const QString& deviceName : deviceNames) {
        DeviceStatus status = deviceManager_->getDeviceStatus(deviceName);
        QString statusText;
        
        switch (status) {
            case DeviceStatus::CONNECTED:
                statusText = "已连接";
                break;
            case DeviceStatus::DISCONNECTED:
                statusText = "未连接";
                break;
            case DeviceStatus::INITIALIZING:
                statusText = "初始化中";
                break;
            case DeviceStatus::BUSY:
                statusText = "忙碌";
                break;
            case DeviceStatus::ERROR:
                statusText = "错误";
                break;
            default:
                statusText = "未知";
                break;
        }
        
        appendResult(QString("%1: %2").arg(deviceName, statusText));
    }
    
    bool systemReady = deviceManager_->isSystemReady();
    appendResult(QString("系统状态: %1").arg(systemReady ? "就绪" : "未就绪"));
}

void DeviceManagerTestWindow::clearResults()
{
    resultText_->clear();
}

void DeviceManagerTestWindow::updateDeviceStatusDisplay()
{
    if (!deviceManager_) {
        return;
    }
    
    auto getStatusText = [](DeviceStatus status) -> QString {
        switch (status) {
            case DeviceStatus::CONNECTED: return "✅ 已连接";
            case DeviceStatus::DISCONNECTED: return "❌ 未连接";
            case DeviceStatus::INITIALIZING: return "🔄 初始化中";
            case DeviceStatus::BUSY: return "⚡ 忙碌";
            case DeviceStatus::ERROR: return "⚠️ 错误";
            default: return "❓ 未知";
        }
    };
    
    aoStatusLabel_->setText("AO (JY5711): " + getStatusText(deviceManager_->getDeviceStatus("JY5711")));
    daq5322StatusLabel_->setText("DAQ5322: " + getStatusText(deviceManager_->getDeviceStatus("JY5322")));
    daq5323StatusLabel_->setText("DAQ5323: " + getStatusText(deviceManager_->getDeviceStatus("JY5323")));
    dmmStatusLabel_->setText("DMM (JY8902): " + getStatusText(deviceManager_->getDeviceStatus("JY8902")));
}

void DeviceManagerTestWindow::appendResult(const QString& result)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    resultText_->append(QString("[%1] %2").arg(timestamp, result));
    
    // 自动滚动到底部
    QTextCursor cursor = resultText_->textCursor();
    cursor.movePosition(QTextCursor::End);
    resultText_->setTextCursor(cursor);
}

void DeviceManagerTestWindow::onDeviceStatusChanged(const QString& device, DeviceStatus status)
{
    Q_UNUSED(status)
    appendResult(QString("设备状态变化: %1").arg(device));
    updateDeviceStatusDisplay();
}

void DeviceManagerTestWindow::onErrorOccurred(const QString& error)
{
    appendResult("设备错误: " + error);
}
