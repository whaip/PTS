# PCB故障检测系统 - 用户界面与工作流程

## 用户界面设计

### 1. 主界面布局

PCB故障检测系统采用Qt Widgets框架构建用户界面，提供直观、专业的操作体验。

```
┌─────────────────────────────────────────────────────────────────────────┐
│  菜单栏: 文件 | 编辑 | 测试 | 工具 | 视图 | 帮助                           │
├─────────────────────────────────────────────────────────────────────────┤
│  工具栏: [新建] [打开] [保存] [开始测试] [停止] [设置] [导出] [帮助]       │
├─────────────────────────────────────────────────────────────────────────┤
│ ┌─────────────────┐ ┌─────────────────────────────────────────────────┐ │
│ │   设备状态面板   │ │              主工作区域                        │ │
│ │ ┌─────────────┐ │ │ ┌─────────────────────────────────────────────┐ │ │
│ │ │ JY5711 AO   │ │ │ │           测试序列管理                      │ │ │
│ │ │ ●已连接     │ │ │ │ ┌─────────────────────────────────────────┐ │ │ │
│ │ └─────────────┘ │ │ │ │ 组件名称 │ 类型 │ 标称值 │ 状态 │ 结果  │ │ │ │
│ │ ┌─────────────┐ │ │ │ ├─────────────────────────────────────────┤ │ │ │
│ │ │ JY5322 DAQ  │ │ │ │ │   R1    │电阻 │ 1kΩ   │测试中│  --   │ │ │ │
│ │ │ ●已连接     │ │ │ │ │   C1    │电容 │100nF  │ 等待 │  --   │ │ │ │
│ │ └─────────────┘ │ │ │ │   L1    │电感 │100μH  │ 等待 │  --   │ │ │ │
│ │ ┌─────────────┐ │ │ │ └─────────────────────────────────────────┘ │ │ │
│ │ │ JY5323 DAQ  │ │ │ └─────────────────────────────────────────────┘ │ │
│ │ │ ●已连接     │ │ │                                                 │ │
│ │ └─────────────┘ │ │ ┌─────────────────────────────────────────────┐ │ │
│ │ ┌─────────────┐ │ │ │           实时数据显示                      │ │ │
│ │ │ JY8902 DMM  │ │ │ │    [电压波形图]    │    [电流波形图]     │ │ │ │
│ │ │ ●已连接     │ │ │ │                    │                     │ │ │ │
│ │ └─────────────┘ │ │ │                    │                     │ │ │ │
│ │                 │ │ │    [阻抗频谱图]    │    [相位图]         │ │ │ │
│ │ ┌─────────────┐ │ │ │                    │                     │ │ │ │
│ │ │   系统状态   │ │ │ │                    │                     │ │ │ │
│ │ │ 就绪        │ │ │ └─────────────────────────────────────────────┘ │ │
│ │ │ 进度: 25%   │ │ └─────────────────────────────────────────────────┘ │
│ │ └─────────────┘ │                                                   │
│ └─────────────────┘                                                   │
├─────────────────────────────────────────────────────────────────────────┤
│ 状态栏: 当前操作: 测试电阻R1 | 已完成: 5/20 | 时间: 15:30:45              │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2. 主窗口类实现

```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // 菜单操作
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onExportResults();
    
    // 测试控制
    void onStartTest();
    void onPauseTest();
    void onStopTest();
    
    // 设备管理
    void onDeviceStatusChanged(DeviceManager::DeviceType type, DeviceStatus status);
    void onRefreshDevices();
    
    // 测试进度
    void onTestProgress(int percentage);
    void onTestCompleted();
    void onComponentTested(const DiagnosticResult& result);
    
    // 界面更新
    void updateStatusBar();
    void updateDevicePanel();
    void updateDataDisplay();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void setupDockWidgets();
    void connectSignals();
    
    // UI组件
    QMenuBar* m_menuBar;
    QToolBar* m_toolBar;
    QStatusBar* m_statusBar;
    
    // 中央控件
    QWidget* m_centralWidget;
    QSplitter* m_mainSplitter;
    
    // 停靠窗口
    QDockWidget* m_deviceDock;
    QDockWidget* m_logDock;
    QDockWidget* m_resultDock;
    
    // 核心组件
    DeviceStatusPanel* m_devicePanel;
    TestSequenceWidget* m_sequenceWidget;
    DataVisualizationWidget* m_dataWidget;
    LogWidget* m_logWidget;
    ResultTreeWidget* m_resultWidget;
    
    // 业务逻辑
    DeviceManager* m_deviceManager;
    TestSequenceManager* m_sequenceManager;
    FaultDiagnostic* m_diagnostic;
    ResultExporter* m_exporter;
    
    // 状态管理
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QLabel* m_timeLabel;
    QTimer* m_updateTimer;
};
```

### 3. 设备状态面板

```cpp
class DeviceStatusPanel : public QWidget {
    Q_OBJECT

private:
    struct DeviceStatusWidget {
        QGroupBox* groupBox;
        QLabel* statusIcon;
        QLabel* statusText;
        QLabel* infoText;
        QPushButton* configButton;
        QPushButton* testButton;
    };

    QMap<DeviceManager::DeviceType, DeviceStatusWidget> m_deviceWidgets;
    DeviceManager* m_deviceManager;

public:
    DeviceStatusPanel(DeviceManager* manager, QWidget* parent = nullptr);
    
    void updateDeviceStatus(DeviceManager::DeviceType type, DeviceStatus status);
    void updateDeviceInfo(DeviceManager::DeviceType type, const QString& info);

private slots:
    void onConfigureDevice();
    void onTestDevice();
    void onRefreshAllDevices();

private:
    void setupDeviceWidget(DeviceManager::DeviceType type, const QString& name);
    QIcon getStatusIcon(DeviceStatus status);
    QString getStatusText(DeviceStatus status);
    QColor getStatusColor(DeviceStatus status);

signals:
    void deviceConfigRequested(DeviceManager::DeviceType type);
    void deviceTestRequested(DeviceManager::DeviceType type);
    void refreshRequested();
};
```

### 4. 测试序列管理界面

```cpp
class TestSequenceWidget : public QWidget {
    Q_OBJECT

private:
    QTableWidget* m_sequenceTable;
    QToolBar* m_sequenceToolBar;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_editButton;
    QPushButton* m_importButton;
    QPushButton* m_exportButton;
    
    TestSequenceManager* m_sequenceManager;
    TestSequence* m_currentSequence;

public:
    TestSequenceWidget(TestSequenceManager* manager, QWidget* parent = nullptr);
    
    void setCurrentSequence(TestSequence* sequence);
    void updateSequenceDisplay();
    void highlightCurrentComponent(int index);

private slots:
    void onAddComponent();
    void onRemoveComponent();
    void onEditComponent();
    void onImportSequence();
    void onExportSequence();
    void onComponentDoubleClicked(int row, int column);
    void onSequenceChanged();

private:
    void setupTable();
    void setupToolBar();
    void populateTable();
    void addComponentToTable(const ComponentSpec& spec, int row);
    ComponentSpec getComponentFromRow(int row);
    void updateComponentStatus(int row, const QString& status);
    void updateComponentResult(int row, const DiagnosticResult& result);

signals:
    void componentSelected(const ComponentSpec& spec);
    void sequenceModified();
};
```

## 核心对话框设计

### 1. 组件编辑对话框

```cpp
class ComponentEditDialog : public QDialog {
    Q_OBJECT

private:
    QComboBox* m_typeCombo;
    QLineEdit* m_nameEdit;
    QLineEdit* m_positionEdit;
    QTabWidget* m_parameterTabs;
    
    // 参数编辑控件
    QMap<QString, QWidget*> m_parameterWidgets;
    ComponentSpec m_componentSpec;

public:
    ComponentEditDialog(QWidget* parent = nullptr);
    ComponentEditDialog(const ComponentSpec& spec, QWidget* parent = nullptr);
    
    ComponentSpec getComponentSpec() const;
    void setComponentSpec(const ComponentSpec& spec);

private slots:
    void onTypeChanged();
    void onParameterChanged();
    void onAccept();

private:
    void setupBasicTab();
    void setupParameterTabs();
    void setupResistorTab();
    void setupCapacitorTab();
    void setupInductorTab();
    void setupDiodeTab();
    void setupICTab();
    
    void updateParameterWidgets();
    bool validateParameters();
    void applyParameters();

signals:
    void componentSpecChanged(const ComponentSpec& spec);
};
```

### 2. 测试配置对话框

```cpp
class TestConfigDialog : public QDialog {
    Q_OBJECT

private:
    QTabWidget* m_configTabs;
    
    // 基本配置
    QDoubleSpinBox* m_testVoltageSpinBox;
    QDoubleSpinBox* m_testCurrentSpinBox;
    QDoubleSpinBox* m_testFrequencySpinBox;
    QComboBox* m_signalTypeCombo;
    
    // 采样配置
    QDoubleSpinBox* m_sampleRateSpinBox;
    QSpinBox* m_samplesSpinBox;
    QDoubleSpinBox* m_measureTimeSpinBox;
    QSpinBox* m_averageCountSpinBox;
    
    // 触发配置
    QCheckBox* m_triggerEnabledCheck;
    QComboBox* m_triggerSourceCombo;
    QDoubleSpinBox* m_triggerLevelSpinBox;
    
    // 高级配置
    QCheckBox* m_autoRangeCheck;
    QSpinBox* m_retryCountSpinBox;
    QDoubleSpinBox* m_timeoutSpinBox;
    
    MeasurementConfig m_config;

public:
    TestConfigDialog(QWidget* parent = nullptr);
    TestConfigDialog(const MeasurementConfig& config, QWidget* parent = nullptr);
    
    MeasurementConfig getConfiguration() const;
    void setConfiguration(const MeasurementConfig& config);

private slots:
    void onLoadPreset();
    void onSavePreset();
    void onResetToDefaults();
    void onParameterChanged();

private:
    void setupBasicConfigTab();
    void setupSamplingConfigTab();
    void setupTriggerConfigTab();
    void setupAdvancedConfigTab();
    void updateUI();
    bool validateConfiguration();

signals:
    void configurationChanged(const MeasurementConfig& config);
};
```

### 3. 结果查看对话框

```cpp
class ResultViewDialog : public QDialog {
    Q_OBJECT

private:
    QTabWidget* m_resultTabs;
    QTableWidget* m_summaryTable;
    QTextEdit* m_detailText;
    QCustomPlot* m_waveformPlot;
    QCustomPlot* m_spectrumPlot;
    
    DiagnosticResult m_result;
    MeasurementResult m_measurement;

public:
    ResultViewDialog(const DiagnosticResult& result,
                    const MeasurementResult& measurement,
                    QWidget* parent = nullptr);

private slots:
    void onExportReport();
    void onExportData();
    void onPrintReport();

private:
    void setupSummaryTab();
    void setupDetailTab();
    void setupWaveformTab();
    void setupSpectrumTab();
    
    void populateSummaryTable();
    void populateDetailText();
    void plotWaveformData();
    void plotSpectrumData();
    
    QString generateDetailReport();
    void saveWaveformImage(const QString& filename);

signals:
    void exportRequested(const QString& format);
};
```

## 数据可视化组件

### 1. 实时数据显示

```cpp
class DataVisualizationWidget : public QWidget {
    Q_OBJECT

private:
    QTabWidget* m_plotTabs;
    QCustomPlot* m_voltagePlot;
    QCustomPlot* m_currentPlot;
    QCustomPlot* m_impedancePlot;
    QCustomPlot* m_phasePlot;
    QCustomPlot* m_spectrumPlot;
    
    QTimer* m_updateTimer;
    bool m_realTimeMode;

public:
    DataVisualizationWidget(QWidget* parent = nullptr);
    
    void setRealTimeMode(bool enabled);
    void updatePlots(const MeasurementResult& measurement);
    void clearPlots();

public slots:
    void onNewMeasurementData(const MeasurementResult& measurement);
    void onUpdateTimer();

private slots:
    void onPlotSelectionChanged();
    void onAxisRangeChanged();
    void onExportPlot();

private:
    void setupVoltagePlot();
    void setupCurrentPlot();
    void setupImpedancePlot();
    void setupPhasePlot();
    void setupSpectrumPlot();
    
    void updateVoltagePlot(const MeasurementResult& measurement);
    void updateCurrentPlot(const MeasurementResult& measurement);
    void updateImpedancePlot(const MeasurementResult& measurement);
    void updatePhasePlot(const MeasurementResult& measurement);
    void updateSpectrumPlot(const MeasurementResult& measurement);
    
    void configurePlotAppearance(QCustomPlot* plot);
    void addCursorLines(QCustomPlot* plot);

signals:
    void plotDataSelected(const QVector<double>& xData, const QVector<double>& yData);
    void cursorMoved(double x, double y);
};
```

### 2. 结果树形显示

```cpp
class ResultTreeWidget : public QTreeWidget {
    Q_OBJECT

private:
    QMap<QString, QTreeWidgetItem*> m_componentItems;
    QMenu* m_contextMenu;
    
    QAction* m_viewDetailAction;
    QAction* m_exportAction;
    QAction* m_retestAction;
    QAction* m_deleteAction;

public:
    ResultTreeWidget(QWidget* parent = nullptr);
    
    void addResult(const DiagnosticResult& result);
    void updateResult(const DiagnosticResult& result);
    void clearResults();
    QList<DiagnosticResult> getSelectedResults();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onViewDetail();
    void onExportSelected();
    void onRetestSelected();
    void onDeleteSelected();

private:
    void setupContextMenu();
    QTreeWidgetItem* createComponentItem(const DiagnosticResult& result);
    QTreeWidgetItem* createResultItem(const DiagnosticResult& result, 
                                     QTreeWidgetItem* parent);
    void updateItemAppearance(QTreeWidgetItem* item, const DiagnosticResult& result);
    QIcon getResultIcon(const DiagnosticResult& result);
    QColor getResultColor(const DiagnosticResult& result);

signals:
    void resultSelected(const DiagnosticResult& result);
    void retestRequested(const QString& componentName);
    void exportRequested(const QList<DiagnosticResult>& results);
};
```

## 工作流程设计

### 1. 典型测试流程

```cpp
class TestWorkflow : public QObject {
    Q_OBJECT

public:
    enum WorkflowState {
        IDLE,
        INITIALIZING,
        LOADING_SEQUENCE,
        CONFIGURING_DEVICES,
        RUNNING_TESTS,
        ANALYZING_RESULTS,
        GENERATING_REPORTS,
        COMPLETED,
        ABORTED,
        ERROR
    };

private:
    WorkflowState m_currentState;
    TestSequence* m_testSequence;
    QTimer* m_workflowTimer;
    int m_currentComponentIndex;
    
    MainWindow* m_mainWindow;
    DeviceManager* m_deviceManager;
    TestSequenceManager* m_sequenceManager;
    FaultDiagnostic* m_diagnostic;

public:
    TestWorkflow(MainWindow* mainWindow, QObject* parent = nullptr);
    
    void startWorkflow(TestSequence* sequence);
    void pauseWorkflow();
    void resumeWorkflow();
    void stopWorkflow();
    void abortWorkflow();
    
    WorkflowState getCurrentState() const { return m_currentState; }
    int getCurrentProgress() const;

private slots:
    void onWorkflowTimer();
    void onDeviceReady();
    void onMeasurementComplete(const MeasurementResult& measurement);
    void onDiagnosisComplete(const DiagnosticResult& result);

private:
    void setState(WorkflowState newState);
    void processCurrentState();
    
    void initializeWorkflow();
    void loadTestSequence();
    void configureDevices();
    void runCurrentTest();
    void analyzeResults();
    void generateReports();
    void completeWorkflow();
    void handleError(const QString& error);

signals:
    void stateChanged(WorkflowState newState);
    void progressChanged(int percentage);
    void componentTestStarted(const ComponentSpec& spec);
    void componentTestCompleted(const DiagnosticResult& result);
    void workflowCompleted();
    void workflowAborted();
    void errorOccurred(const QString& error);
};
```

### 2. 用户操作流程

#### 项目创建流程
```cpp
void MainWindow::createNewProject() {
    // 1. 检查当前项目状态
    if (hasUnsavedChanges()) {
        if (promptSaveChanges() == QMessageBox::Cancel) {
            return;
        }
    }
    
    // 2. 创建新项目对话框
    ProjectWizard wizard(this);
    if (wizard.exec() != QDialog::Accepted) {
        return;
    }
    
    // 3. 获取项目配置
    ProjectConfig config = wizard.getProjectConfig();
    
    // 4. 初始化项目
    m_currentProject = new Project(config, this);
    
    // 5. 更新界面
    updateWindowTitle();
    updateMenuState();
    resetWorkspace();
    
    // 6. 显示项目信息
    showProjectInfo(config);
}
```

#### 设备连接流程
```cpp
void MainWindow::connectDevices() {
    // 1. 显示连接进度对话框
    DeviceConnectionDialog dialog(m_deviceManager, this);
    connect(&dialog, &DeviceConnectionDialog::deviceConnected,
            this, &MainWindow::onDeviceConnected);
    
    // 2. 开始连接设备
    if (dialog.exec() == QDialog::Accepted) {
        // 3. 检查连接状态
        if (m_deviceManager->areAllDevicesReady()) {
            updateDevicePanel();
            enableTestControls(true);
            showStatusMessage("所有设备已连接就绪", 3000);
        } else {
            QMessageBox::warning(this, "连接警告", 
                               "部分设备连接失败，请检查设备状态");
        }
    }
}
```

#### 测试执行流程
```cpp
void MainWindow::startTesting() {
    // 1. 验证测试条件
    if (!validateTestConditions()) {
        return;
    }
    
    // 2. 获取测试配置
    TestConfigDialog configDialog(this);
    if (configDialog.exec() != QDialog::Accepted) {
        return;
    }
    
    MeasurementConfig config = configDialog.getConfiguration();
    
    // 3. 创建测试序列
    TestSequence* sequence = m_sequenceManager->createSequence(
        m_sequenceWidget->getComponentList());
    
    // 4. 应用测试配置
    sequence->setGlobalSetting("measurement_config", 
                              QVariant::fromValue(config));
    
    // 5. 启动工作流程
    m_workflow->startWorkflow(sequence);
    
    // 6. 更新界面状态
    enableTestControls(false);
    m_progressBar->setVisible(true);
    m_statusLabel->setText("测试进行中...");
}
```

### 3. 结果处理流程

```cpp
void MainWindow::handleTestCompletion() {
    // 1. 获取测试结果
    QList<DiagnosticResult> results = m_sequenceManager->getResults();
    
    // 2. 更新结果显示
    m_resultWidget->clearResults();
    for (const auto& result : results) {
        m_resultWidget->addResult(result);
    }
    
    // 3. 生成统计信息
    TestStatistics stats = calculateTestStatistics(results);
    updateStatisticsDisplay(stats);
    
    // 4. 自动保存结果
    if (m_autoSaveEnabled) {
        saveTestResults(results);
    }
    
    // 5. 显示完成对话框
    TestCompletionDialog dialog(stats, this);
    connect(&dialog, &TestCompletionDialog::exportRequested,
            this, &MainWindow::exportResults);
    connect(&dialog, &TestCompletionDialog::reportRequested,
            this, &MainWindow::generateReport);
    
    dialog.exec();
    
    // 6. 恢复界面状态
    enableTestControls(true);
    m_progressBar->setVisible(false);
    m_statusLabel->setText("测试完成");
}
```

## 用户体验优化

### 1. 响应式界面设计

```cpp
class ResponsiveMainWindow : public MainWindow {
public:
    void resizeEvent(QResizeEvent* event) override {
        MainWindow::resizeEvent(event);
        
        // 根据窗口大小调整布局
        QSize size = event->size();
        
        if (size.width() < 1024) {
            // 小屏幕模式：隐藏侧边栏
            m_deviceDock->setVisible(false);
            m_logDock->setVisible(false);
        } else {
            // 正常模式：显示所有面板
            m_deviceDock->setVisible(true);
            m_logDock->setVisible(true);
        }
        
        // 调整图表大小
        adjustPlotSizes(size);
    }

private:
    void adjustPlotSizes(const QSize& windowSize) {
        if (windowSize.width() < 800) {
            // 小屏幕：单列显示图表
            m_dataWidget->setPlotLayout(1, 2);
        } else if (windowSize.width() < 1200) {
            // 中等屏幕：双列显示
            m_dataWidget->setPlotLayout(2, 2);
        } else {
            // 大屏幕：多列显示
            m_dataWidget->setPlotLayout(2, 3);
        }
    }
};
```

### 2. 键盘快捷键支持

```cpp
void MainWindow::setupShortcuts() {
    // 文件操作
    QShortcut* newShortcut = new QShortcut(QKeySequence::New, this);
    connect(newShortcut, &QShortcut::activated, this, &MainWindow::onNewProject);
    
    QShortcut* openShortcut = new QShortcut(QKeySequence::Open, this);
    connect(openShortcut, &QShortcut::activated, this, &MainWindow::onOpenProject);
    
    QShortcut* saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::onSaveProject);
    
    // 测试控制
    QShortcut* startShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(startShortcut, &QShortcut::activated, this, &MainWindow::onStartTest);
    
    QShortcut* stopShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5), this);
    connect(stopShortcut, &QShortcut::activated, this, &MainWindow::onStopTest);
    
    // 界面控制
    QShortcut* fullscreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreenShortcut, &QShortcut::activated, this, &MainWindow::toggleFullscreen);
    
    // 设备控制
    QShortcut* refreshShortcut = new QShortcut(QKeySequence(Qt::Key_F9), this);
    connect(refreshShortcut, &QShortcut::activated, this, &MainWindow::onRefreshDevices);
}
```

### 3. 状态保存和恢复

```cpp
void MainWindow::saveWindowState() {
    QSettings settings;
    settings.beginGroup("MainWindow");
    
    // 保存窗口几何形状
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    
    // 保存分割器状态
    settings.setValue("mainSplitter", m_mainSplitter->saveState());
    
    // 保存停靠窗口状态
    settings.setValue("deviceDockVisible", m_deviceDock->isVisible());
    settings.setValue("logDockVisible", m_logDock->isVisible());
    settings.setValue("resultDockVisible", m_resultDock->isVisible());
    
    // 保存表格列宽
    settings.setValue("sequenceTableHeader", 
                     m_sequenceWidget->getTableWidget()->horizontalHeader()->saveState());
    
    settings.endGroup();
}

void MainWindow::restoreWindowState() {
    QSettings settings;
    settings.beginGroup("MainWindow");
    
    // 恢复窗口几何形状
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    // 恢复分割器状态
    m_mainSplitter->restoreState(settings.value("mainSplitter").toByteArray());
    
    // 恢复停靠窗口状态
    m_deviceDock->setVisible(settings.value("deviceDockVisible", true).toBool());
    m_logDock->setVisible(settings.value("logDockVisible", true).toBool());
    m_resultDock->setVisible(settings.value("resultDockVisible", true).toBool());
    
    // 恢复表格列宽
    m_sequenceWidget->getTableWidget()->horizontalHeader()->restoreState(
        settings.value("sequenceTableHeader").toByteArray());
    
    settings.endGroup();
}
```

### 4. 帮助和提示系统

```cpp
class HelpSystem : public QObject {
    Q_OBJECT

private:
    QMap<QString, QString> m_helpTexts;
    QToolTip* m_contextTooltip;
    QTimer* m_tooltipTimer;

public:
    HelpSystem(QObject* parent = nullptr);
    
    void registerWidget(QWidget* widget, const QString& helpId);
    void showContextHelp(const QString& helpId);
    void showQuickTip(QWidget* widget, const QString& tip);

private slots:
    void onWidgetEntered();
    void onWidgetLeft();
    void onShowTooltip();

private:
    void loadHelpTexts();
    QString getHelpText(const QString& helpId);

signals:
    void helpRequested(const QString& helpId);
};

// 使用示例
void MainWindow::setupHelp() {
    m_helpSystem = new HelpSystem(this);
    
    // 注册控件帮助
    m_helpSystem->registerWidget(m_startButton, "start_test");
    m_helpSystem->registerWidget(m_devicePanel, "device_status");
    m_helpSystem->registerWidget(m_sequenceWidget, "test_sequence");
    
    // 连接帮助信号
    connect(m_helpSystem, &HelpSystem::helpRequested,
            this, &MainWindow::showHelpDialog);
}
```

---

*本文档详细介绍了PCB故障检测系统的用户界面设计和工作流程，包括主界面布局、核心对话框、数据可视化组件以及用户体验优化策略。*
