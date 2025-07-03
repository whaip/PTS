#include "wiringguidedialog.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QSplitter>
#include <QDebug>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QUuid>

WiringGuideDialog::WiringGuideDialog(const ComponentSpec& component,
                                   PortManager* portManager,
                                   ComponentDiagnosticManager* diagnostic_manager,
                                   QWidget *parent)
    : QDialog(parent)
    , component_(component)
    , portManager_(portManager)
    , wiringCompleted_(false)
    , currentStepIndex_(0)
    , diagnostic_manager_(diagnostic_manager)
    , isBatchWiring_(false)
{
    // 添加调试信息
    qDebug() << "创建WiringGuideDialog - 组件引用:" << component.reference
             << "类型:" << static_cast<int>(component.type)
             << "标称值:" << component.nominal_value
             << "容差:" << component.tolerance_percent;

    setWindowTitle(QString("接线引导 - %1 (%2)").arg(component.reference).arg(componentTypeToString(component.type)));
    setModal(true);
    resize(1000, 700);

    setupUI();
    updateAvailablePorts();
}

WiringGuideDialog::~WiringGuideDialog()
{
    qDebug() << "WiringGuideDialog: 开始析构...";

    // 先断开信号连接
    disconnect(this, nullptr, nullptr, nullptr);

    // 释放为此组件分配的端口，但要检查portManager是否仍然有效
    // 注意：由于析构顺序问题，portManager可能已经被析构，所以这里不调用其方法
    // 端口释放应该由PortManager自己的析构函数处理
    if (portManager_) {
        qDebug() << "WiringGuideDialog: portManager仍然有效，但跳过端口释放以避免崩溃";
        // 不再调用portManager的方法，因为可能导致崩溃
        // portManager_->releasePortsForUser(component_.reference, false);
    }

    qDebug() << "WiringGuideDialog: 析构完成";
}

void WiringGuideDialog::reject()
{
    // 发出取消信号
    portManager_->releasePortsForUser(component_.reference);
    emit wiringCancelled();
    QDialog::reject();
}

void WiringGuideDialog::closeEvent(QCloseEvent* event)
{
    // 发出取消信号
    emit wiringCancelled();
    QDialog::closeEvent(event);
}

void WiringGuideDialog::setBatchWiring(const QVector<ComponentSpec>& batchComponentSpecs)
{
    batchComponentSpecs_ = batchComponentSpecs;
    isBatchWiring_ = true;
}

void WiringGuideDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 创建选项卡
    tabWidget_ = new QTabWidget();
    mainLayout->addWidget(tabWidget_);

    setupComponentConfigPage();
    setupPortSelectionPage();
    setupWiringInstructionPage();
    // 移除验证页面：setupValidationPage();

    // 底部按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* helpBtn = new QPushButton("帮助");
    QPushButton* cancelBtn = new QPushButton("取消");
    QPushButton* finishBtn = new QPushButton("完成");

    buttonLayout->addWidget(helpBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(finishBtn);

    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(finishBtn, &QPushButton::clicked, [this]() {
        if (wiringCompleted_) {
            emit wiringCompleted(currentScheme_, allocatedPorts_);
            accept();
        } else {
            QMessageBox::warning(this, "警告", "请完成所有接线步骤后再点击完成");
        }
    });
}

void WiringGuideDialog::setupComponentConfigPage()
{
    componentConfigPage_ = new QWidget();
    tabWidget_->addTab(componentConfigPage_, "元件信息");

    QVBoxLayout* layout = new QVBoxLayout(componentConfigPage_);

    // 元件基本信息
    QGroupBox* basicInfoGroup = new QGroupBox("基本信息");
    QFormLayout* basicLayout = new QFormLayout(basicInfoGroup);

    componentTypeLabel_ = new QLabel(componentTypeToString(component_.type));
    componentValueLabel_ = new QLabel(QString::number(component_.nominal_value));

    componentRefEdit_ = new QLineEdit(component_.reference);
    componentRefEdit_->setReadOnly(true);

    nominalValueSpin_ = new QDoubleSpinBox();
    nominalValueSpin_->setRange(0.001, 1000000);
    nominalValueSpin_->setDecimals(6);
    nominalValueSpin_->setValue(component_.nominal_value);
    nominalValueSpin_->setEnabled(false);

    toleranceSpin_ = new QDoubleSpinBox();
    toleranceSpin_->setRange(0.1, 50);
    toleranceSpin_->setDecimals(2);
    toleranceSpin_->setValue(component_.tolerance_percent);
    toleranceSpin_->setSuffix(" %");
    toleranceSpin_->setEnabled(false);

    basicLayout->addRow("元件类型:", componentTypeLabel_);
    basicLayout->addRow("元件编号:", componentRefEdit_);
    basicLayout->addRow("标称值:", nominalValueSpin_);
    basicLayout->addRow("容差:", toleranceSpin_);

    layout->addWidget(basicInfoGroup);

    // 测试参数
    QGroupBox* testParamsGroup = new QGroupBox("测试参数");
    QVBoxLayout* testLayout = new QVBoxLayout(testParamsGroup);

    testParametersEdit_ = new QTextEdit();
    testParametersEdit_->setMaximumHeight(150);
    testParametersEdit_->setReadOnly(true);

    QString testParams = generateTestParametersDescription();
    testParametersEdit_->setPlainText(testParams);

    testLayout->addWidget(testParametersEdit_);
    layout->addWidget(testParamsGroup);

    layout->addStretch();
}

void WiringGuideDialog::setupPortSelectionPage()
{
    portSelectionPage_ = new QWidget();
    tabWidget_->addTab(portSelectionPage_, "端口选择");

    QHBoxLayout* mainLayout = new QHBoxLayout(portSelectionPage_);

    // 左侧：可用端口
    QVBoxLayout* leftLayout = new QVBoxLayout();
    QGroupBox* availableGroup = new QGroupBox("可用端口");
    QVBoxLayout* availableLayout = new QVBoxLayout(availableGroup);

    availablePortsTable_ = new QTableWidget();
    availablePortsTable_->setColumnCount(5);
    availablePortsTable_->setHorizontalHeaderLabels(
        QStringList() << "设备" << "端口" << "类型" << "描述" << "状态");
    availablePortsTable_->horizontalHeader()->setStretchLastSection(true);
    availablePortsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);

    availableLayout->addWidget(availablePortsTable_);
    leftLayout->addWidget(availableGroup);

    // 操作按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    autoAllocateBtn_ = new QPushButton("自动分配");
    manualAllocateBtn_ = new QPushButton("手动添加");
    clearSelectionBtn_ = new QPushButton("清除选择");

    buttonLayout->addWidget(autoAllocateBtn_);
    buttonLayout->addWidget(manualAllocateBtn_);
    buttonLayout->addWidget(clearSelectionBtn_);

    leftLayout->addLayout(buttonLayout);

    // 右侧：已选端口
    QVBoxLayout* rightLayout = new QVBoxLayout();
    QGroupBox* selectedGroup = new QGroupBox("已选端口");
    QVBoxLayout* selectedLayout = new QVBoxLayout(selectedGroup);

    selectedPortsTable_ = new QTableWidget();
    selectedPortsTable_->setColumnCount(6);
    selectedPortsTable_->setHorizontalHeaderLabels(
        QStringList() << "设备" << "端口" << "类型" << "用途" << "连接点" << "操作");
    selectedPortsTable_->horizontalHeader()->setStretchLastSection(true);

    selectedLayout->addWidget(selectedPortsTable_);
    rightLayout->addWidget(selectedGroup);

    // 状态显示
    portStatusLabel_ = new QLabel("请选择测试端口");
    portStatusLabel_->setStyleSheet("QLabel { color: blue; font-weight: bold; }");
    rightLayout->addWidget(portStatusLabel_);

    // 添加到主布局
    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 1);

    // 连接信号
    connect(autoAllocateBtn_, &QPushButton::clicked, this, &WiringGuideDialog::onAutoAllocatePorts);
    connect(manualAllocateBtn_, &QPushButton::clicked, this, &WiringGuideDialog::onManualAllocatePorts);
    connect(clearSelectionBtn_, &QPushButton::clicked, [this]() {
        if(isBatchWiring_)
        {
            for(const auto& comp : batchComponentSpecs_)
            {
                portManager_->releaseAllPorts();
            }
        }else{
            portManager_->releasePortsForUser(component_.reference);
        }
        updatePortTable();
        updateAvailablePorts();
    });
}

void WiringGuideDialog::setupWiringInstructionPage()
{
    wiringInstructionPage_ = new QWidget();
    tabWidget_->addTab(wiringInstructionPage_, "接线指导");

    QHBoxLayout* mainLayout = new QHBoxLayout(wiringInstructionPage_);

    // 左侧：步骤列表和详情
    QVBoxLayout* leftLayout = new QVBoxLayout();

    QGroupBox* stepsGroup = new QGroupBox("接线步骤");
    QVBoxLayout* stepsLayout = new QVBoxLayout(stepsGroup);

    wiringStepsList_ = new QListWidget();
    wiringStepsList_->setMaximumHeight(200);
    stepsLayout->addWidget(wiringStepsList_);

    leftLayout->addWidget(stepsGroup);

    // 当前步骤详情
    QGroupBox* detailsGroup = new QGroupBox("当前步骤");
    QVBoxLayout* detailsLayout = new QVBoxLayout(detailsGroup);

    currentStepDetails_ = new QTextEdit();
    currentStepDetails_->setMaximumHeight(150);
    detailsLayout->addWidget(currentStepDetails_);

    // 步骤控制按钮
    QHBoxLayout* stepButtonLayout = new QHBoxLayout();
    prevStepBtn_ = new QPushButton("上一步");
    nextStepBtn_ = new QPushButton("下一步");

    stepButtonLayout->addWidget(prevStepBtn_);
    stepButtonLayout->addWidget(nextStepBtn_);

    detailsLayout->addLayout(stepButtonLayout);
    leftLayout->addWidget(detailsGroup);

    // 进度显示
    wiringProgressBar_ = new QProgressBar();
    leftLayout->addWidget(wiringProgressBar_);

    // 右侧：连接图表
    QVBoxLayout* rightLayout = new QVBoxLayout();

    QGroupBox* diagramGroup = new QGroupBox("连接图表");
    QVBoxLayout* diagramLayout = new QVBoxLayout(diagramGroup);

    connectionDiagramScene_ = new QGraphicsScene();
    connectionDiagramView_ = new QGraphicsView(connectionDiagramScene_);
    connectionDiagramView_->setMinimumHeight(400);

    diagramLayout->addWidget(connectionDiagramView_);
    rightLayout->addWidget(diagramGroup);

    // 添加到主布局
    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 2);

    // 连接信号
    connect(wiringStepsList_, &QListWidget::currentRowChanged,
            this, &WiringGuideDialog::updateWiringInstructions);
    connect(prevStepBtn_, &QPushButton::clicked, this, &WiringGuideDialog::onPreviousStep);
    connect(nextStepBtn_, &QPushButton::clicked, this, &WiringGuideDialog::onNextStep);
}

void WiringGuideDialog::setupValidationPage()
{
    // 验证页面已移除 - 接线完成后直接生成方案
    // 原验证逻辑已整合到接线完成流程中
}

void WiringGuideDialog::updateAvailablePorts()
{
    if (!portManager_) return;

    QVector<PortInfo> allPorts = portManager_->getAllPorts();

    availablePortsTable_->setRowCount(allPorts.size());

    for (int i = 0; i < allPorts.size(); ++i) {
        const PortInfo& port = allPorts[i];

        availablePortsTable_->setItem(i, 0, new QTableWidgetItem(port.deviceName));
        availablePortsTable_->setItem(i, 1, new QTableWidgetItem(QString::number(port.portNumber)));
        availablePortsTable_->setItem(i, 2, new QTableWidgetItem(portTypeToString(port.portType)));
        availablePortsTable_->setItem(i, 3, new QTableWidgetItem(port.description));

        QString status = port.isAvailable ? "可用" : QString("已分配给: %1").arg(port.allocatedTo);
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        statusItem->setBackground(QBrush(port.isAvailable ? QColor(200, 255, 200) : QColor(255, 200, 200)));
        availablePortsTable_->setItem(i, 4, statusItem);
    }
}

void WiringGuideDialog::updatePortTable()
{
    if (!portManager_) return;

    QVector<PortInfo> allPorts = portManager_->getAllPorts();
    QVector<PortInfo> selectedPorts;

    // 找出已分配给当前元件的端口
    if (isBatchWiring_) {
        // 批量模式：展平所有组件的已分配端口
        for (auto it = allocatedPorts_.constBegin(); it != allocatedPorts_.constEnd(); ++it) {
            const QVector<PortInfo>& portsList = it.value();
            for (const PortInfo& port : portsList) {
                selectedPorts.append(port);
            }
        }
    } else {
        // 单组件模式：获取当前组件的已分配端口
        const QString& key = component_.reference;
        if (allocatedPorts_.contains(key)) {
            const QVector<PortInfo>& portsList = allocatedPorts_.value(key);
            for (const PortInfo& port : portsList) {
                selectedPorts.append(port);
            }
        }
    }

    selectedPortsTable_->setRowCount(selectedPorts.size());

    for (int i = 0; i < selectedPorts.size(); ++i) {
        const PortInfo& port = selectedPorts[i];

        selectedPortsTable_->setItem(i, 0, new QTableWidgetItem(port.deviceName));
        selectedPortsTable_->setItem(i, 1, new QTableWidgetItem(QString::number(port.portNumber)));
        selectedPortsTable_->setItem(i, 2, new QTableWidgetItem(portTypeToString(port.portType)));
        selectedPortsTable_->setItem(i, 3, new QTableWidgetItem(getPortUsage(port.portType)));
        selectedPortsTable_->setItem(i, 4, new QTableWidgetItem(getConnectionPoint(port.portType)));

        QPushButton* removeBtn = new QPushButton("移除");
        selectedPortsTable_->setCellWidget(i, 5, removeBtn);

        connect(removeBtn, &QPushButton::clicked, [this, port]() {
            portManager_->releasePort(port.deviceName, port.portNumber);
            updatePortTable();
            updateAvailablePorts();
        });
    }

    // 更新状态标签
    if (selectedPorts.isEmpty()) {
        portStatusLabel_->setText("请选择测试端口");
        portStatusLabel_->setStyleSheet("QLabel { color: blue; font-weight: bold; }");
    } else {
        portStatusLabel_->setText(QString("已选择 %1 个端口").arg(selectedPorts.size()));
        portStatusLabel_->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    }
}

void WiringGuideDialog::onAutoAllocatePorts()
{
    if (!portManager_) return;

    allocatedPorts_.clear();
    if (isBatchWiring_) {
        for (auto& component : batchComponentSpecs_) {
            QVector<PortRequirement> requirements = diagnostic_manager_->getPortRequirements(component);
            QString allocatedTo = component.reference;
            QVector<PortInfo> allocatedPorts = portManager_->autoAllocatePorts(requirements, allocatedTo);
            if(allocatedPorts.isEmpty()){
                QMessageBox::StandardButton reply = QMessageBox::question(this, 
                    "端口分配失败", 
                    QString("组件 %1 的端口分配失败：%2，测试时将跳过该测试，是否继续？").arg(allocatedTo).arg(portManager_->getLastError()),
                    QMessageBox::Yes | QMessageBox::No);
                    
                if (reply == QMessageBox::No) {
                    portManager_->releaseAllPorts();
                    return;
                }
            }
            allocatedPorts_[component.reference] = allocatedPorts;
            component.allocatedPorts = allocatedPorts;
        }
    } else {
        QVector<PortRequirement> requirements = diagnostic_manager_->getPortRequirements(component_);
        QString allocatedTo = component_.reference;
        QVector<PortInfo> allocatedPorts = portManager_->autoAllocatePorts(requirements, allocatedTo);
        allocatedPorts_[component_.reference] = allocatedPorts;
    }

    if (allocatedPorts_.isEmpty()) {
        QMessageBox::warning(this, "警告", "没有足够的可用端口进行自动分配");
        return;
    }

    updatePortTable();
    updateAvailablePorts();
    generateWiringSteps(component_, allocatedPorts_);

    QMessageBox::information(this, "成功",
        QString("自动分配了 %1 个端口，请查看已选端口列表").arg(allocatedPorts_.size()));
}

void WiringGuideDialog::onManualAllocatePorts()
{
    int currentRow = availablePortsTable_->currentRow();
    if (currentRow < 0) {
        QMessageBox::information(this, "提示", "请先选择一个可用端口");
        return;
    }

    QString deviceName = availablePortsTable_->item(currentRow, 0)->text();
    int portNumber = availablePortsTable_->item(currentRow, 1)->text().toInt();
    QString portType = availablePortsTable_->item(currentRow, 2)->text();

    qDebug() << "尝试手动分配端口：" << deviceName << portNumber << "类型：" << portType;

    // 检查端口类型是否适合当前元件
    if (component_.type == ComponentType::RESISTOR ||
        component_.type == ComponentType::CAPACITOR ||
        component_.type == ComponentType::INDUCTOR) {

        if (portType != "万用表测量") {
            QMessageBox::warning(this, "端口类型不匹配",
                QString("该元件需要万用表测量端口，您选择的是%1端口。\n"
                       "请选择JY8902设备的万用表测量端口。").arg(portType));
            return;
        }
    }

    if (portManager_->allocatePort(deviceName, portNumber, component_.reference)) {
        qDebug() << "端口分配成功";
        updatePortTable();
        // generateWiringSteps(component_, QVector<PortInfo>{portManager_->getPortInfo(deviceName, portNumber)});

        QMessageBox::information(this, "成功",
            QString("已添加端口：%1-%2 (%3)").arg(deviceName).arg(portNumber).arg(portType));
    } else {
        qDebug() << "端口分配失败";
        QMessageBox::warning(this, "错误", "端口分配失败，端口可能已被占用");
    }
}

void WiringGuideDialog::generateWiringSteps(const ComponentSpec& component, const QMap<QString, QVector<PortInfo>>& allocatedPorts)
{
    if (!portManager_) return;

    wiringSteps_.clear();
    if (isBatchWiring_) {
        for (const auto& component : batchComponentSpecs_) {
            QVector<ConnectionInfo> temp = convertToConnectionInfo(diagnostic_manager_->generateWiringScheme(component, allocatedPorts[component.reference]));
            wiringSteps_ += temp.toList();
        }
    } else {
        wiringSteps_ = convertToConnectionInfo(diagnostic_manager_->generateWiringScheme(component, allocatedPorts[component.reference]));
    }

    // 更新步骤列表
    updateWiringStepsList();

    // 更新图表
    updateConnectionDiagram();
}

void WiringGuideDialog::updateWiringStepsList()
{
    qDebug() << "updateWiringStepsList: 接线步骤数量：" << wiringSteps_.size();

    wiringStepsList_->clear();

    for (int i = 0; i < wiringSteps_.size(); ++i) {
        const ConnectionInfo& connection = wiringSteps_[i];
        QString stepText = QString("步骤 %1: %2 → %3")
                          .arg(i + 1)
                          .arg(connection.sourcePort.description)
                          .arg(connection.instruction);

        qDebug() << "添加接线步骤：" << stepText;

        QListWidgetItem* item = new QListWidgetItem(stepText);
        if (connection.isCompleted) {
            item->setBackground(QBrush(QColor(200, 255, 200)));
        }
        wiringStepsList_->addItem(item);
    }

    if (!wiringSteps_.isEmpty()) {
        wiringStepsList_->setCurrentRow(currentStepIndex_);
        updateWiringInstructions();
    } else {
        qDebug() << "没有接线步骤可显示";
    }
}

void WiringGuideDialog::updateWiringInstructions()
{
    if (currentStepIndex_ < 0 || currentStepIndex_ >= wiringSteps_.size()) {
        currentStepDetails_->clear();
        return;
    }

    const ConnectionInfo& connection = wiringSteps_[currentStepIndex_];

    QString details = QString(
        "当前步骤: %1\n\n"
        "源端口: %2 - %3\n"
        "目标: %4\n"
        "线缆颜色: %5\n\n"
        "详细说明:\n%6"
    ).arg(currentStepIndex_ + 1)
     .arg(connection.sourcePort.deviceName)
     .arg(connection.sourcePort.description)
     .arg(connection.instruction)
     .arg(connection.wireColor)
     .arg(generateDetailedInstruction(connection));

    currentStepDetails_->setPlainText(details);
      // 更新按钮状态
    prevStepBtn_->setEnabled(currentStepIndex_ > 0);
    // 修改逻辑：允许在最后一步也能点击"下一步"按钮来完成该步骤
    nextStepBtn_->setEnabled(currentStepIndex_ < wiringSteps_.size());

    // 更新进度条
    int progress = (currentStepIndex_ + 1) * 100 / qMax(1, wiringSteps_.size());
    wiringProgressBar_->setValue(progress);

    // 更新图表
    updateConnectionDiagram();
}

void WiringGuideDialog::updateConnectionDiagram()
{
    if (!connectionDiagramScene_) return;

    connectionDiagramScene_->clear();

    if (wiringSteps_.isEmpty() || currentStepIndex_ >= wiringSteps_.size()) {
        return;
    }

    const ConnectionInfo& connection = wiringSteps_[currentStepIndex_];

    // 绘制简化的连接图
    double centerX = 200;
    double centerY = 100;
    double portSpacing = 80;

    // 绘制源端口
    QGraphicsRectItem* sourceRect = connectionDiagramScene_->addRect(
        centerX - 100, centerY - 20, 80, 40,
        QPen(Qt::blue, 2), QBrush(QColor(173, 216, 230)));

    QGraphicsTextItem* sourceText = connectionDiagramScene_->addText(
        QString("%1:%2").arg(connection.sourcePort.deviceName).arg(connection.sourcePort.portNumber),
        QFont("Arial", 10));
    sourceText->setPos(centerX - 95, centerY - 15);

    // 绘制目标点
    QGraphicsRectItem* targetRect = connectionDiagramScene_->addRect(
        centerX + 20, centerY - 20, 80, 40,
        QPen(Qt::red, 2), QBrush(QColor(240, 128, 128)));

    QGraphicsTextItem* targetText = connectionDiagramScene_->addText(
        "元件连接点", QFont("Arial", 10));
    targetText->setPos(centerX + 25, centerY - 15);

    // 绘制连接线
    QPen connectionPen(getWireColor(connection.wireColor), 3);
    QGraphicsLineItem* connectionLine = connectionDiagramScene_->addLine(
        centerX - 20, centerY, centerX + 20, centerY, connectionPen);

    // 添加箭头
    QGraphicsLineItem* arrow1 = connectionDiagramScene_->addLine(
        centerX + 15, centerY - 5, centerX + 20, centerY, connectionPen);
    QGraphicsLineItem* arrow2 = connectionDiagramScene_->addLine(
        centerX + 15, centerY + 5, centerX + 20, centerY, connectionPen);

    // 添加说明文字
    QGraphicsTextItem* instructionText = connectionDiagramScene_->addText(
        connection.instruction, QFont("Arial", 9));
    instructionText->setPos(centerX - 50, centerY + 50);

    connectionDiagramView_->fitInView(connectionDiagramScene_->itemsBoundingRect(), Qt::KeepAspectRatio);
}

void WiringGuideDialog::onNextStep()
{
    // 首先标记当前步骤为完成
    if (currentStepIndex_ >= 0 && currentStepIndex_ < wiringSteps_.size()) {
        wiringSteps_[currentStepIndex_].isCompleted = true;
    }

    // 如果不是最后一步，移动到下一步
    if (currentStepIndex_ < wiringSteps_.size() - 1) {
        currentStepIndex_++;
        updateWiringInstructions();
        updateWiringStepsList();
    } else {
        // 如果是最后一步，只更新列表显示，不移动索引
        updateWiringStepsList();
        // 完成最后一步后，禁用"下一步"按钮
        nextStepBtn_->setEnabled(false);
    }

    // 检查是否所有步骤都完成
    bool allCompleted = true;
    for (const ConnectionInfo& connection : wiringSteps_) {
        if (!connection.isCompleted) {
            allCompleted = false;
            break;
        }
    }

    if (allCompleted) {
        wiringCompleted_ = true;
        // 直接完成接线，跳过验证步骤
        onGenerateScheme();
    }
}

void WiringGuideDialog::onPreviousStep()
{
    if (currentStepIndex_ > 0) {
        currentStepIndex_--;
        updateWiringInstructions();
    }
}

void WiringGuideDialog::onValidateConnections()
{
    // 验证步骤已移除 - 直接跳过验证
    // 接线完成后直接生成方案
}

void WiringGuideDialog::updateValidationResults()
{
    // 验证结果显示已移除 - 直接跳过
    // 接线完成后直接生成方案，无需显示验证结果
}

void WiringGuideDialog::onGenerateScheme()
{
    if(!isBatchWiring_) {
        currentScheme_.schemeId = QUuid::createUuid().toString();
        currentScheme_.schemeName = QString("%1测试方案").arg(component_.reference);
        currentScheme_.componentType = component_.type;
        currentScheme_.description = QString("为%1元件生成的接线方案").arg(componentTypeToString(component_.type));
        currentScheme_.connections = wiringSteps_;

        // 添加测试参数
        currentScheme_.testParameters["component_reference"] = component_.reference;
        currentScheme_.testParameters["nominal_value"] = component_.nominal_value;
        currentScheme_.testParameters["tolerance"] = component_.tolerance_percent;
        currentScheme_.testParameters["test_voltage"] = getTestVoltage();
        currentScheme_.testParameters["test_frequency"] = getTestFrequency();

        QMessageBox::information(this, "成功",
            QString("接线方案生成成功!\n方案ID: %1").arg(currentScheme_.schemeId));
    }else{
        currentScheme_.schemeId = QUuid::createUuid().toString();
        currentScheme_.schemeName = QString("%1测试方案").arg("Batch test");
        currentScheme_.componentType = batchComponentSpecs_[0].type;
        currentScheme_.description = QString("为Batch Test生成的接线方案");
        currentScheme_.connections = wiringSteps_;
    }

    // 直接发出完成信号，跳过验证页面的显示
    emit wiringCompleted(currentScheme_, allocatedPorts_);
    accept();
}

void WiringGuideDialog::onResetWiring()
{
    if (QMessageBox::question(this, "确认重置",
                             "确定要重置所有接线配置吗？这将清除当前的所有设置。",
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

        // 释放所有已分配的端口
        portManager_->releasePortsForUser(component_.reference);

        // 重置状态
        currentStepIndex_ = 0;
        wiringCompleted_ = false;
        currentScheme_ = WiringScheme();
        wiringSteps_.clear();
          // 更新界面
        updateAvailablePorts();
        updatePortTable();
        updateWiringStepsList();
        tabWidget_->setCurrentIndex(0);

        // 验证按钮已移除，无需设置状态
    }
}

WiringScheme WiringGuideDialog::getWiringScheme() const
{
    return currentScheme_;
}

// 辅助函数实现
QString WiringGuideDialog::componentTypeToString(ComponentType type) const
{
    switch (type) {
    case ComponentType::RESISTOR: return "电阻";
    case ComponentType::CAPACITOR: return "电容";
    case ComponentType::INDUCTOR: return "电感";
    case ComponentType::DIODE: return "二极管";
    case ComponentType::IC: return "集成电路";
    default: return "未知";
    }
}

QString WiringGuideDialog::portTypeToString(PortType type) const
{
    switch (type) {
    case PortType::ANALOG_OUTPUT: return "模拟输出";
    case PortType::DIGITAL_OUTPUT: return "数字输出";
    case PortType::POWER_OUTPUT: return "电源输出";
    case PortType::ANALOG_INPUT: return "模拟输入";
    case PortType::DIGITAL_INPUT: return "数字输入";
    case PortType::DMM_MEASUREMENT: return "万用表测量";
    default: return "未知";
    }
}

QString WiringGuideDialog::getPortUsage(PortType type) const
{
    switch (type) {
    case PortType::ANALOG_OUTPUT: return "信号源";
    case PortType::DIGITAL_OUTPUT: return "控制信号";
    case PortType::POWER_OUTPUT: return "电源供应";
    case PortType::ANALOG_INPUT: return "信号采集";
    case PortType::DIGITAL_INPUT: return "状态检测";
    case PortType::DMM_MEASUREMENT: return "万用表测量";
    default: return "通用";
    }
}

QString WiringGuideDialog::getConnectionPoint(PortType type) const
{
    switch (type) {
    case PortType::ANALOG_OUTPUT: return "元件引脚A";
    case PortType::POWER_OUTPUT: return "电源端";
    case PortType::ANALOG_INPUT: return "元件引脚B";
    case PortType::DMM_MEASUREMENT: return "元件引脚";
    default: return "元件引脚";
    }
}

QColor WiringGuideDialog::getWireColor(const QString& colorName) const
{
    if (colorName == "红色") return Qt::red;
    if (colorName == "黑色") return Qt::black;
    if (colorName == "黄色") return Qt::yellow;
    if (colorName == "绿色") return Qt::green;
    if (colorName == "蓝色") return Qt::blue;
    return Qt::gray;
}

QVector<ConnectionInfo> WiringGuideDialog::convertToConnectionInfo(const QVector<WiringConnection>& connections) const
{
    QVector<ConnectionInfo> infoList;
    for (const WiringConnection& conn : connections) {
        ConnectionInfo info;
        PortInfo targetPort;
        targetPort.deviceName = conn.componentPin;
        info.sourcePort = conn.targetPort;
        info.targetPort = targetPort;
        info.wireColor = conn.wireColor;
        info.instruction = conn.instruction;
        infoList.append(info);
    }
    return infoList;
}

QString WiringGuideDialog::generateDetailedInstruction(const ConnectionInfo& connection) const
{
    return QString("1. 使用%1线缆\n2. 将一端连接到%2的端口%3\n3. 将另一端%4\n4. 确保连接牢固")
           .arg(connection.wireColor)
           .arg(connection.sourcePort.deviceName)
           .arg(connection.sourcePort.portNumber)
           .arg(connection.instruction);
}

QString WiringGuideDialog::generateTestParametersDescription() const
{
    QString params;

    switch (component_.type) {
    case ComponentType::RESISTOR:
        params = QString("测试类型: 直流电阻测试\n"
                        "测试电压: 1V\n"
                        "测试电流: 自适应\n"
                        "测量精度: 0.1%\n"
                        "预期阻值: %1 Ω (±%2%)")
                 .arg(component_.nominal_value)
                 .arg(component_.tolerance_percent);
        break;
    case ComponentType::CAPACITOR:
        params = QString("测试类型: 电容测试\n"
                        "测试频率: 1kHz\n"
                        "测试电压: 1V\n"
                        "测量精度: 1%\n"
                        "预期容值: %1 F (±%2%)")
                 .arg(component_.nominal_value)
                 .arg(component_.tolerance_percent);
        break;
    default:
        params = QString("预期值: %1 (±%2%)")
                 .arg(component_.nominal_value)
                 .arg(component_.tolerance_percent);
        break;
    }

    return params;
}

bool WiringGuideDialog::validateCurrentConfiguration() const
{
    // 检查是否有必要的端口配置
    QStringList errors;
    return portManager_->validatePortConfiguration(wiringSteps_, errors);
}

double WiringGuideDialog::getTestVoltage() const
{
    switch (component_.type) {
    case ComponentType::RESISTOR:
    case ComponentType::CAPACITOR:
    case ComponentType::INDUCTOR:
        return 1.0; // 1V
    case ComponentType::DIODE:
        return 3.3; // 3.3V
    case ComponentType::IC:
        return 5.0; // 5V
    default:
        return 1.0;
    }
}

double WiringGuideDialog::getTestFrequency() const
{
    switch (component_.type) {
    case ComponentType::RESISTOR:
        return 0; // DC
    case ComponentType::CAPACITOR:
    case ComponentType::INDUCTOR:
        return 1000; // 1kHz
    default:
        return 1000;
    }
}
