// 端口配置和测试执行模块实现
#include "portconfiguration.h"
#include "devicethread.h"  // 添加这个包含
#include <QTabWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QLineEdit>
#include <memory>

//=============================================================================
// PortConfigurationDialog 实现
//=============================================================================

PortConfigurationDialog::PortConfigurationDialog(const TestSchemeSignals& scheme, 
                                                 DeviceManager* deviceManager,
                                                 QWidget *parent)
    : QDialog(parent)
    , testScheme_(scheme)
    , deviceManager_(deviceManager)
    , configurationValid_(false)
{
    setWindowTitle(QString("端口配置 - %1").arg(scheme.name));
    setModal(true);
    resize(800, 600);

    setupUI();
    populateSignalList();
    updateAvailableDevices();
}

PortConfigurationDialog::~PortConfigurationDialog()
{
}

void PortConfigurationDialog::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    
    // 创建选项卡
    configTabs_ = new QTabWidget();
    mainLayout_->addWidget(configTabs_);
    
    setupSignalConfigurationPage();
    setupPortMappingPage();
    setupTestParametersPage();
    
    // 状态栏
    QHBoxLayout* statusLayout = new QHBoxLayout();
    statusLabel_ = new QLabel("请配置端口映射");
    statusLabel_->setStyleSheet("QLabel { color: blue; }");
    statusLayout->addWidget(statusLabel_);
    statusLayout->addStretch();
    
    // 按钮
    testBtn_ = new QPushButton("测试配置");
    resetBtn_ = new QPushButton("重置");
    QPushButton* okBtn = new QPushButton("确定");
    QPushButton* cancelBtn = new QPushButton("取消");
    
    statusLayout->addWidget(testBtn_);
    statusLayout->addWidget(resetBtn_);
    statusLayout->addWidget(okBtn);
    statusLayout->addWidget(cancelBtn);
    
    mainLayout_->addLayout(statusLayout);
    
    // 连接信号
    connect(testBtn_, &QPushButton::clicked, this, &PortConfigurationDialog::onTestConfiguration);
    connect(resetBtn_, &QPushButton::clicked, this, &PortConfigurationDialog::onResetConfiguration);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void PortConfigurationDialog::setupSignalConfigurationPage()
{
    QWidget* page = new QWidget();
    configTabs_->addTab(page, "信号配置");
    
    QVBoxLayout* layout = new QVBoxLayout(page);
    
    // 方案描述
    QGroupBox* descGroup = new QGroupBox("测试方案描述");
    QVBoxLayout* descLayout = new QVBoxLayout(descGroup);
    
    schemeDescription_ = new QTextEdit();
    schemeDescription_->setReadOnly(true);
    schemeDescription_->setMaximumHeight(100);
    schemeDescription_->setText(testScheme_.description);
    descLayout->addWidget(schemeDescription_);
    
    layout->addWidget(descGroup);
    
    // 信号列表
    QGroupBox* signalGroup = new QGroupBox("所需信号");
    QVBoxLayout* signalLayout = new QVBoxLayout(signalGroup);
    
    signalTable_ = new QTableWidget();
    signalTable_->setColumnCount(6);
    signalTable_->setHorizontalHeaderLabels(
        QStringList() << "信号名称" << "信号类型" << "幅值" << "频率" << "必需" << "描述");
    
    signalTable_->horizontalHeader()->setStretchLastSection(true);
    signalTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    signalTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    signalLayout->addWidget(signalTable_);
    layout->addWidget(signalGroup);
}

void PortConfigurationDialog::setupPortMappingPage()
{
    QWidget* page = new QWidget();
    configTabs_->addTab(page, "端口映射");
    
    QVBoxLayout* layout = new QVBoxLayout(page);
    
    // 操作按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    autoAssignBtn_ = new QPushButton("自动分配");
    clearAssignBtn_ = new QPushButton("清除分配");
    
    buttonLayout->addWidget(autoAssignBtn_);
    buttonLayout->addWidget(clearAssignBtn_);
    buttonLayout->addStretch();
    
    layout->addLayout(buttonLayout);
    
    // 端口映射表
    QGroupBox* mappingGroup = new QGroupBox("端口映射配置");
    QVBoxLayout* mappingLayout = new QVBoxLayout(mappingGroup);
    
    portMappingTable_ = new QTableWidget();
    portMappingTable_->setColumnCount(7);
    portMappingTable_->setHorizontalHeaderLabels(
        QStringList() << "信号名称" << "信号类型" << "设备" << "通道" << "量程" << "采样率" << "状态");
    
    portMappingTable_->horizontalHeader()->setStretchLastSection(true);
    mappingLayout->addWidget(portMappingTable_);
    
    layout->addWidget(mappingGroup);
    
    // 连接信号
    connect(autoAssignBtn_, &QPushButton::clicked, [this]() {
        PortConfiguration portConfig(deviceManager_);
        if (portConfig.autoAssignPorts(testScheme_, currentConfig_)) {
            createPortMappingTable();
            statusLabel_->setText("自动分配完成");
            statusLabel_->setStyleSheet("QLabel { color: green; }");
        } else {
            statusLabel_->setText("自动分配失败");
            statusLabel_->setStyleSheet("QLabel { color: red; }");
        }
    });
    
    connect(clearAssignBtn_, &QPushButton::clicked, [this]() {
        currentConfig_.outputPorts.clear();
        currentConfig_.inputPorts.clear();
        createPortMappingTable();
        statusLabel_->setText("请重新配置端口映射");
        statusLabel_->setStyleSheet("QLabel { color: blue; }");
    });
}

void PortConfigurationDialog::setupTestParametersPage()
{
    QWidget* page = new QWidget();
    configTabs_->addTab(page, "测试参数");
    
    QVBoxLayout* layout = new QVBoxLayout(page);
    
    // 时序参数
    timingGroup_ = new QGroupBox("时序参数");
    QFormLayout* timingLayout = new QFormLayout(timingGroup_);
    
    timeoutSpin_ = new QSpinBox();
    timeoutSpin_->setRange(1000, 300000);  // 1秒到5分钟
    timeoutSpin_->setValue(30000);         // 默认30秒
    timeoutSpin_->setSuffix(" ms");
    timingLayout->addRow("测试超时:", timeoutSpin_);
    
    layout->addWidget(timingGroup_);
    
    // 同步参数
    synchronizationGroup_ = new QGroupBox("同步参数");
    QFormLayout* syncLayout = new QFormLayout(synchronizationGroup_);
    
    enableSyncCheck_ = new QCheckBox("启用同步触发");
    enableSyncCheck_->setChecked(testScheme_.requiresSynchronization);
    syncLayout->addRow(enableSyncCheck_);
    
    syncGroupEdit_ = new QLineEdit(testScheme_.syncGroupName);
    syncGroupEdit_->setEnabled(testScheme_.requiresSynchronization);
    syncLayout->addRow("同步组名称:", syncGroupEdit_);
    
    layout->addWidget(synchronizationGroup_);
    
    // 数据采集参数
    dataAcquisitionGroup_ = new QGroupBox("数据采集参数");
    QFormLayout* daqLayout = new QFormLayout(dataAcquisitionGroup_);
    
    sampleRateSpin_ = new QDoubleSpinBox();
    sampleRateSpin_->setRange(1.0, 1000000.0);
    sampleRateSpin_->setValue(10000.0);
    sampleRateSpin_->setSuffix(" Hz");
    daqLayout->addRow("采样率:", sampleRateSpin_);
    
    samplesPerChannelSpin_ = new QSpinBox();
    samplesPerChannelSpin_->setRange(1, 1000000);
    samplesPerChannelSpin_->setValue(1000);
    samplesPerChannelSpin_->setSuffix(" 样本");
    daqLayout->addRow("每通道采样数:", samplesPerChannelSpin_);
    
    layout->addWidget(dataAcquisitionGroup_);
    
    layout->addStretch();
    
    // 连接信号
    connect(enableSyncCheck_, &QCheckBox::toggled, syncGroupEdit_, &QLineEdit::setEnabled);
    connect(timeoutSpin_, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &PortConfigurationDialog::onParameterChanged);
}

void PortConfigurationDialog::populateSignalList()
{
    signalTable_->setRowCount(testScheme_.signals_.size());
    
    for (int i = 0; i < testScheme_.signals_.size(); ++i) {
        const SignalDefinition& signal = testScheme_.signals_[i];
        
        signalTable_->setItem(i, 0, new QTableWidgetItem(signal.name));
        signalTable_->setItem(i, 1, new QTableWidgetItem(
            SignalConfiguration(this).getSignalTypeDescription(signal.type)));
        signalTable_->setItem(i, 2, new QTableWidgetItem(QString::number(signal.amplitude)));
        signalTable_->setItem(i, 3, new QTableWidgetItem(QString::number(signal.frequency)));
        signalTable_->setItem(i, 4, new QTableWidgetItem(signal.isRequired ? "是" : "否"));
        signalTable_->setItem(i, 5, new QTableWidgetItem(signal.description));
        
        // 设置必需信号的行背景色
        if (signal.isRequired) {
            for (int col = 0; col < 6; ++col) {
                if (signalTable_->item(i, col)) {
                    signalTable_->item(i, col)->setBackground(QColor(255, 240, 240));
                }
            }
        }
    }
}

void PortConfigurationDialog::createPortMappingTable()
{
    int totalPorts = currentConfig_.outputPorts.size() + currentConfig_.inputPorts.size();
    portMappingTable_->setRowCount(totalPorts);
    
    int row = 0;
    
    // 输出端口
    for (const PortConfig& config : currentConfig_.outputPorts) {
        portMappingTable_->setItem(row, 0, new QTableWidgetItem(config.signalName));
        portMappingTable_->setItem(row, 1, new QTableWidgetItem("输出"));
        portMappingTable_->setItem(row, 2, new QTableWidgetItem(config.deviceName));
        portMappingTable_->setItem(row, 3, new QTableWidgetItem(QString::number(config.channel)));
        portMappingTable_->setItem(row, 4, new QTableWidgetItem(
            QString("%1~%2V").arg(config.rangeMin).arg(config.rangeMax)));
        portMappingTable_->setItem(row, 5, new QTableWidgetItem(
            QString::number(config.sampleRate) + " Hz"));
        portMappingTable_->setItem(row, 6, new QTableWidgetItem("已配置"));
        
        ++row;
    }
    
    // 输入端口
    for (const PortConfig& config : currentConfig_.inputPorts) {
        portMappingTable_->setItem(row, 0, new QTableWidgetItem(config.signalName));
        portMappingTable_->setItem(row, 1, new QTableWidgetItem("输入"));
        portMappingTable_->setItem(row, 2, new QTableWidgetItem(config.deviceName));
        portMappingTable_->setItem(row, 3, new QTableWidgetItem(QString::number(config.channel)));
        portMappingTable_->setItem(row, 4, new QTableWidgetItem(
            QString("%1~%2").arg(config.rangeMin).arg(config.rangeMax)));
        portMappingTable_->setItem(row, 5, new QTableWidgetItem(
            QString::number(config.sampleRate) + " Hz"));
        portMappingTable_->setItem(row, 6, new QTableWidgetItem("已配置"));
        
        ++row;
    }
}

void PortConfigurationDialog::updateAvailableDevices()
{
    if (!deviceManager_) return;
    
    QStringList devices = deviceManager_->getConnectedDevices();
    qDebug() << "可用设备:" << devices;
}

TestConfiguration PortConfigurationDialog::getConfiguration() const
{
    TestConfiguration config = currentConfig_;
    
    // 更新测试参数
    config.testName = QString("%1_Test").arg(testScheme_.name);
    config.testTimeout = timeoutSpin_->value();
    config.requiresSynchronization = enableSyncCheck_->isChecked();
    config.syncGroupName = syncGroupEdit_->text();
    
    // 更新数据采集参数
    for (PortConfig& port : config.inputPorts) {
        port.sampleRate = sampleRateSpin_->value();
        port.samplesPerChannel = samplesPerChannelSpin_->value();
    }
    
    return config;
}

void PortConfigurationDialog::onTestConfiguration()
{
    validateConfiguration();
    
    if (configurationValid_) {
        QMessageBox::information(this, "配置测试", "端口配置有效，可以进行测试。");
    } else {
        QMessageBox::warning(this, "配置错误", "端口配置无效，请检查配置。");
    }
}

void PortConfigurationDialog::onResetConfiguration()
{
    currentConfig_ = TestConfiguration();
    createPortMappingTable();
    statusLabel_->setText("配置已重置");
    statusLabel_->setStyleSheet("QLabel { color: blue; }");
}

void PortConfigurationDialog::validateConfiguration()
{
    PortConfiguration portConfig(deviceManager_);
    QStringList errors;
    
    configurationValid_ = portConfig.validatePortConfiguration(currentConfig_, errors);
    
    if (configurationValid_) {
        statusLabel_->setText("配置有效");
        statusLabel_->setStyleSheet("QLabel { color: green; }");
    } else {
        statusLabel_->setText(QString("配置错误: %1").arg(errors.join("; ")));
        statusLabel_->setStyleSheet("QLabel { color: red; }");
    }
}

void PortConfigurationDialog::onDeviceSelectionChanged()
{
    updateAvailableChannels();
}

void PortConfigurationDialog::onChannelSelectionChanged()
{
    validatePortMapping();
}

void PortConfigurationDialog::onParameterChanged()
{
    validateConfiguration();
}

void PortConfigurationDialog::updateAvailableChannels()
{
    // 实现通道更新逻辑
}

void PortConfigurationDialog::validatePortMapping()
{
    // 实现端口映射验证
}

//=============================================================================
// PortConfiguration 实现
//=============================================================================

PortConfiguration::PortConfiguration(DeviceManager* deviceManager, QObject *parent)
    : QObject(parent)
    , deviceManager_(deviceManager)
    , testExecutor_(std::make_unique<TestExecutor>(deviceManager, this))
{
    initializePortStatus();
}

PortConfiguration::~PortConfiguration()
{
}

void PortConfiguration::initializePortStatus()
{
    if (!deviceManager_) return;
    
    QStringList devices = deviceManager_->getConnectedDevices();
    
    for (const QString& device : devices) {
        if (device.contains("5711")) {
            allocatedPorts_[device] = QVector<bool>(32, false);  // 5711有32个输出通道
        } else if (device.contains("5323")) {
            allocatedPorts_[device] = QVector<bool>(32, false);  // 5323有32个输出通道
        } else if (device.contains("5322")) {
            allocatedPorts_[device] = QVector<bool>(16, false); // 5322有16个输入通道
        } else if (device.contains("8902")) {
            allocatedPorts_[device] = QVector<bool>(1, false);  // 8902万用表1个通道
        }
    }
}

QStringList PortConfiguration::getAvailableDevices() const
{
    if (!deviceManager_) return QStringList();
    return deviceManager_->getConnectedDevices();
}

QVector<int> PortConfiguration::getAvailableChannels(const QString& deviceName) const
{
    QVector<int> availableChannels;
    
    if (allocatedPorts_.contains(deviceName)) {
        const QVector<bool>& channels = allocatedPorts_[deviceName];
        for (int i = 0; i < channels.size(); ++i) {
            if (!channels[i]) {  // 未分配的通道
                availableChannels.append(i);
            }
        }
    }
    
    return availableChannels;
}

bool PortConfiguration::isPortAvailable(const QString& deviceName, int channel) const
{
    if (!allocatedPorts_.contains(deviceName)) return false;
    
    const QVector<bool>& channels = allocatedPorts_[deviceName];
    if (channel < 0 || channel >= channels.size()) return false;
    
    return !channels[channel];
}

bool PortConfiguration::allocatePort(const PortConfig& config)
{
    if (!isPortAvailable(config.deviceName, config.channel)) {
        return false;
    }
    
    allocatedPorts_[config.deviceName][config.channel] = true;
    emit portAllocated(config.deviceName, config.channel);
    return true;
}

bool PortConfiguration::releasePort(const QString& deviceName, int channel)
{
    if (!allocatedPorts_.contains(deviceName)) return false;
    
    QVector<bool>& channels = allocatedPorts_[deviceName];
    if (channel < 0 || channel >= channels.size()) return false;
    
    if (channels[channel]) {
        channels[channel] = false;
        emit portReleased(deviceName, channel);
        return true;
    }
    
    return false;
}

void PortConfiguration::releaseAllPorts()
{
    for (auto it = allocatedPorts_.begin(); it != allocatedPorts_.end(); ++it) {
        QVector<bool>& channels = it.value();
        for (int i = 0; i < channels.size(); ++i) {
            if (channels[i]) {
                channels[i] = false;
                emit portReleased(it.key(), i);
            }
        }
    }
}

QString PortConfiguration::getPreferredDevice(SignalType signalType) const
{
    switch (signalType) {
        case SignalType::VOLTAGE_DC:
        case SignalType::VOLTAGE_AC:
        case SignalType::CURRENT_DC:
        case SignalType::CURRENT_AC:
        case SignalType::ANALOG_OUTPUT:
            return "5711";  // 模拟输出设备
            
        case SignalType::DIGITAL_INPUT:
        case SignalType::DIGITAL_OUTPUT:
        case SignalType::ANALOG_INPUT:
            return "5320";  // 数据采集设备
            
        case SignalType::DMM_MEASUREMENT:
        case SignalType::RESISTANCE:
        case SignalType::CAPACITANCE:
        case SignalType::INDUCTANCE:
            return "8902";  // 万用表设备
            
        default:
            return "5320";  // 默认使用数据采集设备
    }
}

bool PortConfiguration::autoAssignPorts(const TestSchemeSignals& scheme, TestConfiguration& config)
{
    config.outputPorts.clear();
    config.inputPorts.clear();
    
    for (const SignalDefinition& Signal : scheme.signals_) {
        PortConfig portConfig;
        portConfig.signalName = Signal.name;
        portConfig.signalType = Signal.type;
        
        // 确定设备类型
        QString preferredDevice = getPreferredDevice(Signal.type);
        
        // 查找可用设备
        QStringList availableDevices = getAvailableDevices();
        QString selectedDevice;
        
        for (const QString& device : availableDevices) {
            if (device.contains(preferredDevice)) {
                selectedDevice = device;
                break;
            }
        }
        
        if (selectedDevice.isEmpty()) {
            qDebug() << "未找到适合的设备用于信号:" << Signal.name;
            continue;
        }
        
        // 查找可用通道
        QVector<int> availableChannels = getAvailableChannels(selectedDevice);
        if (availableChannels.isEmpty()) {
            qDebug() << "设备" << selectedDevice << "没有可用通道";
            continue;
        }
        
        // 分配端口
        portConfig.deviceName = selectedDevice;
        portConfig.channel = availableChannels.first();
        
        // 设置端口参数
        if (Signal.type == SignalType::VOLTAGE_DC || Signal.type == SignalType::VOLTAGE_AC ||
            Signal.type == SignalType::CURRENT_DC || Signal.type == SignalType::CURRENT_AC ||
            Signal.type == SignalType::ANALOG_OUTPUT) {
            portConfig.isOutput = true;
            portConfig.rangeMin = -Signal.amplitude * 1.2;
            portConfig.rangeMax = Signal.amplitude * 1.2;
            config.outputPorts.append(portConfig);
        } else {
            portConfig.isOutput = false;
            portConfig.rangeMin = -10.0;
            portConfig.rangeMax = 10.0;
            portConfig.sampleRate = qMax(Signal.frequency * 10, 10000.0);
            config.inputPorts.append(portConfig);
        }
        
        // 标记端口为已分配
        allocatePort(portConfig);
    }
    
    config.requiresSynchronization = scheme.requiresSynchronization;
    config.syncGroupName = scheme.syncGroupName;
    
    return !config.outputPorts.isEmpty() || !config.inputPorts.isEmpty();
}

bool PortConfiguration::validatePortConfiguration(const TestConfiguration& config, QStringList& errors)
{
    errors.clear();
    
    // 检查是否有端口配置
    if (config.outputPorts.isEmpty() && config.inputPorts.isEmpty()) {
        errors << "必须配置至少一个端口";
        return false;
    }
    
    // 检查端口冲突
    QSet<QPair<QString, int>> usedPorts;
    
    for (const PortConfig& port : config.outputPorts) {
        QPair<QString, int> portId(port.deviceName, port.channel);
        if (usedPorts.contains(portId)) {
            errors << QString("端口冲突: %1 通道 %2").arg(port.deviceName).arg(port.channel);
        }
        usedPorts.insert(portId);
        
        if (!isPortAvailable(port.deviceName, port.channel)) {
            errors << QString("端口不可用: %1 通道 %2").arg(port.deviceName).arg(port.channel);
        }
    }
    
    for (const PortConfig& port : config.inputPorts) {
        QPair<QString, int> portId(port.deviceName, port.channel);
        if (usedPorts.contains(portId)) {
            errors << QString("端口冲突: %1 通道 %2").arg(port.deviceName).arg(port.channel);
        }
        usedPorts.insert(portId);
        
        if (!isPortAvailable(port.deviceName, port.channel)) {
            errors << QString("端口不可用: %1 通道 %2").arg(port.deviceName).arg(port.channel);
        }
    }
    
    // 检查同步配置
    if (config.requiresSynchronization && config.syncGroupName.isEmpty()) {
        errors << "启用同步时必须指定同步组名称";
    }
    
    return errors.isEmpty();
}

//=============================================================================
// TestExecutor 实现
//=============================================================================

TestExecutor::TestExecutor(DeviceManager* deviceManager, QObject *parent)
    : QObject(parent)
    , deviceManager_(deviceManager)
    , currentStepIndex_(0)
    , testRunning_(false)
    , stepTimer_(new QTimer(this))
    , executionLoop_(nullptr)
    , isExecuting_(false)
    , timeoutMs_(30000)
{
    // 连接计时器信号
    connect(stepTimer_, &QTimer::timeout, this, &TestExecutor::onStepTimeout);
}

TestExecutor::~TestExecutor()
{
    if (isExecuting_) {
        stopExecution();
    }
}

bool TestExecutor::executeTest(const QString& testId, const TestConfiguration& config)
{
    if (isExecuting_) {
        setLastError("测试正在执行中");
        return false;
    }
    
    if (!deviceManager_) {
        setLastError("设备管理器未初始化");
        return false;
    }
    
    currentTestId_ = testId;
    currentConfig_ = config;
    isExecuting_ = true;
    
    qDebug() << "开始执行测试:" << testId;
    emit testStarted(testId);
    
    // 使用 deviceManager_->submitOperation 提交测试操作
    QString operationId = QString("test_operation_%1").arg(testId);
    
    // 创建测试参数
    QMap<QString, QVariant> operationParams;
    operationParams["test_id"] = testId;
    operationParams["timeout"] = config.timeout;
    operationParams["synchronization"] = config.enableSynchronization;
    
    // 添加端口映射信息
    for (auto it = config.portMappings.begin(); it != config.portMappings.end(); ++it) {
        QString key = QString("port_%1").arg(it.key());
        QMap<QString, QVariant> portInfo;
        portInfo["device"] = it.value().deviceName;
        portInfo["channel"] = it.value().channel;
        portInfo["signal_type"] = it.value().signalType;
        operationParams[key] = portInfo;
    }
      // 使用 deviceManager_->submitOperation 提交测试操作
    DeviceOperation testOperation;
    testOperation.command = DeviceCommand::START_MEASUREMENT;  // 使用测量开始命令
    testOperation.parameters = operationParams;
    testOperation.deviceName = config.outputPorts.isEmpty() ? 
                               config.inputPorts.first().deviceName : 
                               config.outputPorts.first().deviceName;
    testOperation.timeout = config.timeout;
    
    bool submitResult = deviceManager_->submitOperation(testOperation.deviceName, testOperation);
    
    if (!submitResult) {
        setLastError(QString("提交测试操作失败: %1").arg(deviceManager_->getLastError()));
        isExecuting_ = false;
        emit testFailed(testId, lastError_);
        return false;
    }
      // 等待操作完成
    QTimer::singleShot(100, this, [this, testOperation]() {
        waitForTestCompletion(testOperation.deviceName);
    });
    
    return true;
}

// TestExecutor::executeTest 重载版本 - 向后兼容
bool TestExecutor::executeTest(const TestConfiguration& config)
{
    // 生成一个唯一的测试ID
    QString testId = QString("test_%1").arg(QDateTime::currentMSecsSinceEpoch());
    return executeTest(testId, config);
}

// 异步执行测试
void TestExecutor::executeTestAsync(const TestConfiguration& config)
{
    if (isExecuting_) {
        emit testFailed("测试正在执行中");
        return;
    }
    
    // 在后台线程中执行测试
    QTimer::singleShot(0, [this, config]() {
        QString testId = QString("async_test_%1").arg(QDateTime::currentMSecsSinceEpoch());
        bool success = executeTest(testId, config);
        if (!success) {
            emit testFailed(getLastError());
        }
    });
}

void TestExecutor::stopExecution()
{
    if (!isExecuting_) {
        return;
    }
    
    qDebug() << "停止测试执行:" << currentTestId_;
    
    isExecuting_ = false;
    emit testStopped(currentTestId_);
}

void TestExecutor::setTimeout(int timeoutMs)
{
    timeoutMs_ = timeoutMs;
}

QString TestExecutor::getLastError() const
{
    return lastError_;
}

void TestExecutor::waitForTestCompletion(const QString& deviceName)
{
    if (!isExecuting_ || !deviceManager_) {
        return;
    }
    
    // 使用 deviceManager_->waitForResult 等待结果
    DeviceResult result = deviceManager_->waitForResult(deviceName, timeoutMs_);
    
    if (!result.success) {
        setLastError(QString("等待测试结果失败: %1").arg(result.error));
        isExecuting_ = false;
        emit testFailed(currentTestId_, lastError_);
        return;
    }
    
    // 处理测试结果
    processTestResult(result.data);
}

void TestExecutor::processTestResult(const QMap<QString, QVariant>& result)
{
    isExecuting_ = false;
    
    bool success = result.value("success", false).toBool();
    
    if (!success) {
        QString error = result.value("error", "未知错误").toString();
        setLastError(error);
        emit testFailed(currentTestId_, error);
        return;
    }
    
    // 构建TestData结构
    TestData testData;
    testData.testId = currentTestId_;
    testData.timestamp = QDateTime::currentDateTime();
    testData.valid = true;
    
    // 提取测量数据
    QVariantMap measurements = result.value("measurements").toMap();
    for (auto it = measurements.begin(); it != measurements.end(); ++it) {
        QMap<QString, QVariant> measurement;
        QVariantMap rawData = it.value().toMap();
        
        // 转换测量数据格式
        measurement["signal_name"] = it.key();
        measurement["value"] = rawData.value("value", 0.0);
        measurement["voltage"] = rawData.value("voltage", 0.0);
        measurement["current"] = rawData.value("current", 0.0);
        measurement["power"] = rawData.value("power", 0.0);
        measurement["frequency"] = rawData.value("frequency", 0.0);
        measurement["phase"] = rawData.value("phase", 0.0);
        measurement["esr"] = rawData.value("esr", 0.0);
        measurement["leakage_current"] = rawData.value("leakage_current", 0.0);
        measurement["valid"] = rawData.value("valid", true);
        
        testData.measurements.append(measurement);
    }
    
    // 添加元数据
    // testData.metadata = result.value("metadata").toMap();
    
    qDebug() << "测试执行完成:" << currentTestId_ << "数据点数:" << testData.measurements.size();
    emit testCompleted(currentTestId_, testData);
}

void TestExecutor::setLastError(const QString& error)
{
    lastError_ = error;
    qWarning() << "TestExecutor错误:" << error;
}

// 步骤超时处理
void TestExecutor::onStepTimeout()
{
    qWarning() << "测试步骤超时:" << currentStepIndex_;
    setLastError(QString("测试步骤 %1 执行超时").arg(currentStepIndex_));
    
    if (executionLoop_ && executionLoop_->isRunning()) {
        executionLoop_->exit(1); // 退出码1表示超时
    }
}

// 数据采集完成处理
void TestExecutor::onDataAcquisitionCompleted()
{
    qDebug() << "数据采集完成";
    emit stepCompleted("数据采集", QMap<QString, QVariant>());
    
    if (executionLoop_ && executionLoop_->isRunning()) {
        executionLoop_->quit();
    }
}

//=============================================================================
// PortConfiguration 类的补充方法
//=============================================================================

bool PortConfiguration::verifyConnections()
{
    if (!deviceManager_) {
        return false;
    }
    
    // 使用设备管理器验证连接
    QString operationId = "verify_connections";
    QMap<QString, QVariant> params;
    
    // 添加当前配置的端口信息
    for (auto it = currentConfig_.portMappings.begin(); it != currentConfig_.portMappings.end(); ++it) {
        QString key = QString("verify_port_%1").arg(it.key());
        QMap<QString, QVariant> portInfo;
        portInfo["device"] = it.value().deviceName;
        portInfo["channel"] = it.value().channel;
        params[key] = portInfo;
    }
      // 创建验证操作
    DeviceOperation verifyOperation;
    verifyOperation.command = DeviceCommand::READ_DATA;  // 使用读取数据命令验证连接
    verifyOperation.parameters = params;
    verifyOperation.timeout = 10000;
    
    // 选择一个设备进行验证（取第一个可用设备）
    QString deviceName = currentConfig_.outputPorts.isEmpty() ? 
                        (currentConfig_.inputPorts.isEmpty() ? 
                         deviceManager_->getConnectedDevices().first() : 
                         currentConfig_.inputPorts.first().deviceName) :
                        currentConfig_.outputPorts.first().deviceName;
    
    bool submitResult = deviceManager_->submitOperation(deviceName, verifyOperation);
    if (!submitResult) {
        qWarning() << "提交连接验证操作失败:" << deviceManager_->getLastError();
        return false;
    }
    
    // 等待验证结果
    DeviceResult result = deviceManager_->waitForResult(deviceName, 10000); // 10秒超时
      if (!result.success) {
        qWarning() << "连接验证失败:" << result.error;
        return false;
    }
    
    return result.success;
}

void PortConfiguration::stopAllOperations()
{
    qDebug() << "停止端口配置的所有操作...";
    
    // 停止任何正在进行的端口分配
    // 断开设备连接
    // 清理资源
    
    qDebug() << "端口配置操作已停止";
}
