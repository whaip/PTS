#ifndef OPTOCOUPLERDIAGNOSTIC_H
#define OPTOCOUPLERDIAGNOSTIC_H

#include "../basecomponentdiagnostic.h"

class OptocouplerDiagnostic : public BaseComponentDiagnostic
{
    Q_OBJECT

public:
    explicit OptocouplerDiagnostic(DeviceManager* deviceManager, QObject* parent = nullptr);
    ~OptocouplerDiagnostic() override = default;

    ComponentType getSupportedComponentType() const override;
    QString getComponentTypeName() const override;
    QStringList getSupportedModels() const override;

protected:
    QVector<PortRequirement> getPortRequirements(const ComponentSpec& component) const override;
    QMap<QString, QVariant> getRequiredParameters() const override;
    QVector<WiringConnection> generateWiringScheme(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts) const override;
    ComponentTestConfig configureDataAcquisition(const ComponentSpec& component, const QVector<PortInfo>& allocatedPorts) const override;
    TestData executeDataAcquisition(const ComponentTestConfig& config) override;
    ComponentDiagnosticResult analyzeFaults(const ComponentSpec& component, const TestData& testData) override;

    bool validateComponentSpec(const ComponentSpec& component) const override;
    bool preTestSetup(const ComponentSpec& component) override;
    void postTestCleanup() override;

private:
    // 计算CTR和相关参数
    double calculateCTR(double ledCurrent, double collectorCurrent) const;
};

#endif // OPTOCOUPLERDIAGNOSTIC_H
