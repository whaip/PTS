// 端口配置和测试执行模块
#ifndef PORTCONFIGURATION_H
#define PORTCONFIGURATION_H

#include <QObject>
#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QProgressBar>
#include <QTimer>
#include <QEventLoop>
#include "signalconfiguration.h"
#include "devicemanager.h"
#include "commontypes.h"

// 前向声明
class TestExecutor;

// 使用commontypes.h中的PortConfig和TestConfiguration

// 端口配置选择对话框
class PortConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PortConfigurationDialog(const TestSchemeSignals& scheme, 
                                   DeviceManager* deviceManager,
                                   QWidget *parent = nullptr);
    ~PortConfigurationDialog();
    
    TestConfiguration getConfiguration() const;

private slots:
    void onDeviceSelectionChanged();
    void onChannelSelectionChanged();
    void onParameterChanged();
    void onTestConfiguration();
    void onResetConfiguration();
    void validateConfiguration();

private:
    void setupUI();
    void setupSignalConfigurationPage();
    void setupPortMappingPage();
    void setupTestParametersPage();
    void updateAvailableDevices();
    void updateAvailableChannels();
    void populateSignalList();
    void createPortMappingTable();
    void validatePortMapping();
    
    TestSchemeSignals testScheme_;
    DeviceManager* deviceManager_;
    TestConfiguration currentConfig_;
    
    // UI组件
    QVBoxLayout* mainLayout_;
    QTabWidget* configTabs_;
    
    // 信号配置页面
    QTableWidget* signalTable_;
    QTextEdit* schemeDescription_;
    
    // 端口映射页面
    QTableWidget* portMappingTable_;
    QPushButton* autoAssignBtn_;
    QPushButton* clearAssignBtn_;
    
    // 测试参数页面
    QGroupBox* timingGroup_;
    QGroupBox* synchronizationGroup_;
    QGroupBox* dataAcquisitionGroup_;
    
    QSpinBox* timeoutSpin_;
    QCheckBox* enableSyncCheck_;
    QLineEdit* syncGroupEdit_;
    QDoubleSpinBox* sampleRateSpin_;
    QSpinBox* samplesPerChannelSpin_;
    
    // 状态显示
    QLabel* statusLabel_;
    QPushButton* testBtn_;
    QPushButton* resetBtn_;
    
    bool configurationValid_;
};

// 端口配置管理器
class PortConfiguration : public QObject
{
    Q_OBJECT

public:
    explicit PortConfiguration(DeviceManager* deviceManager, QObject *parent = nullptr);
    ~PortConfiguration();
    
    // 获取可用设备和端口信息
    QStringList getAvailableDevices() const;
    QVector<int> getAvailableChannels(const QString& deviceName) const;
    bool isPortAvailable(const QString& deviceName, int channel) const;
    
    // 端口分配和释放
    bool allocatePort(const PortConfig& config);
    bool releasePort(const QString& deviceName, int channel);
    void releaseAllPorts();
    
    // 自动端口分配
    bool autoAssignPorts(const TestSchemeSignals& scheme, TestConfiguration& config);
    
    // 验证端口配置
    bool validatePortConfiguration(const TestConfiguration& config, QStringList& errors);
    
    // 获取测试执行器
    TestExecutor* getTestExecutor() { return testExecutor_.get(); }
    
    // 验证连接
    bool verifyConnections();
    
public slots:
    void stopAllOperations();

signals:
    void portAllocated(const QString& deviceName, int channel);
    void portReleased(const QString& deviceName, int channel);
    void configurationChanged();

private:
    DeviceManager* deviceManager_;
    QMap<QString, QVector<bool>> allocatedPorts_; // 设备名 -> 通道分配状态
    std::unique_ptr<TestExecutor> testExecutor_; // 测试执行器
    TestConfiguration currentConfig_;             // 当前配置
    
    void initializePortStatus();
    QString getPreferredDevice(SignalType signalType) const;
    QVector<int> getPreferredChannels(const QString& deviceName, SignalType signalType) const;
};

// 测试执行器
class TestExecutor : public QObject
{
    Q_OBJECT

public:
    explicit TestExecutor(DeviceManager* deviceManager, QObject *parent = nullptr);
    ~TestExecutor();
      // 执行测试
    bool executeTest(const QString& testId, const TestConfiguration& config);
    bool executeTest(const TestConfiguration& config);  // 重载版本保持向后兼容
    void executeTestAsync(const TestConfiguration& config);
    
    // 停止测试
    void stopTest();
    bool isTestRunning() const;
    
    // 获取测试结果
    QMap<QString, QVector<double>> getLastTestResults() const;
    QString getLastError() const;

signals:
    void testStarted(const QString& testId);
    void testProgress(int percentage);
    void testCompleted(const QString& testId, const TestData& testData);  // 修改参数
    void testFailed(const QString& testId, const QString& error);        // 修改参数
    void testFailed(const QString& error);                               // 重载版本
    void testStopped(const QString& testId);
    void stepCompleted(const QString& stepName, const QMap<QString, QVariant>& stepResults);

private slots:
    void onStepTimeout();
    void onDataAcquisitionCompleted();

private:
    struct TestStep {
        QString name;
        QString description;
        QVector<PortConfig> activePorts;
        int duration;  // 持续时间(ms)
        bool requiresSync;
        QMap<QString, QVariant> parameters;
    };
      DeviceManager* deviceManager_;
    TestConfiguration currentConfig_;
    QVector<TestStep> testSteps_;
    int currentStepIndex_;
    bool testRunning_;
    QTimer* stepTimer_;
    QEventLoop* executionLoop_;
    
    // 新增成员变量
    bool isExecuting_;
    int timeoutMs_;
    QString currentTestId_;
    
    QMap<QString, QVector<double>> testResults_;
    QString lastError_;
    
    // 测试执行方法
    bool prepareTest(const TestConfiguration& config);
    bool executeTestStep(const TestStep& step);
    bool executeOutputStep(const TestStep& step);
    bool executeInputStep(const TestStep& step);
    bool executeSynchronizedStep(const TestStep& step);
    void finalizeTest();
      // 新增私有方法
    void stopExecution();
    void setTimeout(int timeoutMs);
    void waitForTestCompletion(const QString& operationId);     // 修改参数
    void processTestResult(const QMap<QString, QVariant>& result);  // 修改参数
    void setLastError(const QString& error);

    // 特定元件测试流程
    QVector<TestStep> createResistorTestSteps(const TestConfiguration& config);
    QVector<TestStep> createCapacitorTestSteps(const TestConfiguration& config);
    QVector<TestStep> createInductorTestSteps(const TestConfiguration& config);
    QVector<TestStep> createDiodeTestSteps(const TestConfiguration& config);
    QVector<TestStep> createICTestSteps(const TestConfiguration& config);
    
    // 辅助方法
    bool initializeDevices();
    bool configureOutputPort(const PortConfig& config);
    bool configureInputPort(const PortConfig& config);
    bool startDataAcquisition(const QVector<PortConfig>& inputPorts);
    bool stopDataAcquisition();
    
    void setError(const QString& error);
};

#endif // PORTCONFIGURATION_H
