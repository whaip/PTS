#ifndef WIRINGGUIDEDIALOG_H
#define WIRINGGUIDEDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QProgressBar>
#include <QCheckBox>
#include <QLineEdit>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QSplitter>
#include <QTimer>
#include "portmanager.h"
#include "portdefinitions.h"
#include "commontypes.h"
#include "componentdiagnosticmanager.h"

using namespace PortDefinitions;

class WiringGuideDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WiringGuideDialog(const ComponentSpec& component,
                            PortManager* portManager,
                            ComponentDiagnosticManager* diagnostic_manager_,
                            QWidget *parent = nullptr);
    ~WiringGuideDialog();

    // 获取配置结果
    WiringScheme getWiringScheme() const;
    bool isWiringCompleted() const { return wiringCompleted_; }

signals:
    void wiringCompleted(const WiringScheme& scheme);
    void wiringCancelled();

protected:
    void reject() override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onPortSelectionChanged();
    void onAutoAllocatePorts();
    void onManualAllocatePorts();
    void onValidateConnections(); // 保留声明，但实现已简化
    void onStartWiring();
    void onNextStep();
    void onPreviousStep();
    void onResetWiring();
    void onGenerateScheme();
    void updateWiringProgress();
    void onConnectionCompleted();
    void onShowPortDetails();
    void onComponentTypeChanged();

private:
    void setupUI();
    void setupComponentConfigPage();
    void setupPortSelectionPage();
    void setupWiringInstructionPage();
    void setupValidationPage();

    void updateAvailablePorts();
    void updatePortTable();
    void updateWiringInstructions();
    void updateConnectionDiagram();
    void updateValidationResults();
    void generateWiringSteps(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts);
    void createConnectionInstructions();
      // Add missing helper function declarations
    QString generateTestParametersDescription() const;
    QString portTypeToString(PortType type) const;
    QString getPortUsage(PortType type) const;  // Fixed parameter type
    QString getConnectionPoint(PortType type) const;  // Fixed parameter type
    QString getWireColor(PortType type) const;
    QColor getWireColor(const QString& portId) const;  // Added overload for QString
    QString generateDetailedInstruction(const ConnectionInfo& connection) const;
    QString componentTypeToString(ComponentType type) const;
    double getTestVoltage() const;  // Removed parameter
    double getTestFrequency() const;  // Removed parameter
      // Missing connection generation functions
    QVector<ConnectionInfo> generateConnectionsForComponent(ComponentType type, const QVector<PortInfo>& ports);  // Match implementation
    QVector<ConnectionInfo> generateResistorConnections(const QVector<PortInfo>& ports);  // Match implementation
    QVector<ConnectionInfo> generateCapacitorConnections(const QVector<PortInfo>& ports);  // Match implementation
    QVector<ConnectionInfo> generateInductorConnections(const QVector<PortInfo>& ports);  // Match implementation
    QVector<ConnectionInfo> generateDiodeConnections(const QVector<PortInfo>& ports);  // Match implementation
    QVector<ConnectionInfo> generateICConnections(const QVector<PortInfo>& ports);  // Match implementation
    QVector<ConnectionInfo> convertToConnectionInfo(const QVector<WiringConnection>& connections) const;

    // Missing UI update functions
    void updateWiringStepsList();
    bool validateCurrentConfiguration() const;

    ComponentSpec component_;
    PortManager* portManager_;
    WiringScheme currentScheme_;
    ComponentDiagnosticManager* diagnostic_manager_;

    bool wiringCompleted_;
    bool isBatchWiring_;
    int currentStepIndex_;
    QVector<ConnectionInfo> wiringSteps_;

    // UI组件
    QTabWidget* tabWidget_;

    // 元件配置页面
    QWidget* componentConfigPage_;
    QLabel* componentTypeLabel_;
    QLabel* componentValueLabel_;
    QLineEdit* componentRefEdit_;
    QDoubleSpinBox* nominalValueSpin_;
    QDoubleSpinBox* toleranceSpin_;
    QTextEdit* testParametersEdit_;

    // 端口选择页面
    QWidget* portSelectionPage_;
    QTableWidget* availablePortsTable_;
    QTableWidget* selectedPortsTable_;
    QPushButton* autoAllocateBtn_;
    QPushButton* manualAllocateBtn_;
    QPushButton* clearSelectionBtn_;
    QLabel* portStatusLabel_;
      // 接线指导页面
    QWidget* wiringInstructionPage_;
    QListWidget* wiringStepsList_;
    QTextEdit* currentStepDetails_;
    QGraphicsView* connectionDiagramView_;
    QGraphicsScene* connectionDiagramScene_;
    QPushButton* nextStepBtn_;
    QPushButton* prevStepBtn_;
    QProgressBar* wiringProgressBar_;

};

#endif // WIRINGGUIDEDIALOG_H
