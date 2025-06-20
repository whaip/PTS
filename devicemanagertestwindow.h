#ifndef DEVICEMANAGERTESTWINDOW_H
#define DEVICEMANAGERTESTWINDOW_H

#include <QDialog>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QProgressBar>
#include <QTimer>
#include "devicemanager.h"

class DeviceManagerTestWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DeviceManagerTestWindow(DeviceManager* deviceManager, QWidget *parent = nullptr);
    ~DeviceManagerTestWindow();

private slots:
    void testInitializeDevices();
    void testMeasureVoltage();
    void testMeasureCurrent();
    void testMeasureResistance();
    void testOutputVoltage();
    void testDeviceStatus();
    void clearResults();
    void onDeviceStatusChanged(const QString& device, DeviceStatus status);
    void onErrorOccurred(const QString& error);

private:
    void setupUI();
    void updateDeviceStatusDisplay();
    void appendResult(const QString& result);
    
    DeviceManager* deviceManager_;
    
    // UI组件
    QVBoxLayout* mainLayout_;
    QHBoxLayout* buttonLayout_;
    QGroupBox* testButtonsGroup_;
    QGroupBox* parametersGroup_;
    QGroupBox* statusGroup_;
    QGroupBox* resultsGroup_;
    
    // 测试按钮
    QPushButton* initDevicesBtn_;
    QPushButton* measureVoltageBtn_;
    QPushButton* measureCurrentBtn_;
    QPushButton* measureResistanceBtn_;
    QPushButton* outputVoltageBtn_;
    QPushButton* deviceStatusBtn_;
    QPushButton* clearBtn_;
    
    // 参数控件
    QSpinBox* channelSpin_;
    QDoubleSpinBox* voltageSpin_;
    QComboBox* deviceCombo_;
    QSpinBox* timeoutSpin_;
    
    // 状态显示
    QLabel* aoStatusLabel_;
    QLabel* daq5322StatusLabel_;
    QLabel* daq5323StatusLabel_;
    QLabel* dmmStatusLabel_;
    QProgressBar* testProgress_;
    
    // 结果显示
    QTextEdit* resultText_;
    
    QTimer* statusUpdateTimer_;
};

#endif // DEVICEMANAGERTESTWINDOW_H
