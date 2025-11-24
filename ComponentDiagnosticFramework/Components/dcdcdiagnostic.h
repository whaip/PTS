#ifndef DCDCDIAGNOSTIC_H
#define DCDCDIAGNOSTIC_H

#include "../basecomponentdiagnostic.h"

/**
 * @brief DC/DC 电源模块诊断类
 *
 * 典型测试：
 * - 静态输出性能（Vout、Iout、纹波）
 * - 负载调整率 / 线性调整率（多Vin、多负载点）
 * - 效率估算
 * - 温升与保护判断
 */
class DcdcDiagnostic : public BaseComponentDiagnostic
{
    Q_OBJECT

public:
    explicit DcdcDiagnostic(DeviceManager* deviceManager, QObject* parent = nullptr);
    ~DcdcDiagnostic() override = default;

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
    struct LoadPoint {
        double loadPercent;   // 0~1
        double nominalCurrent; // 额定电流下的绝对值
    };

    double calculateEfficiency(double vin, double iin, double vout, double iout) const;
    double calculateRipple(const QVector<double>& waveform, double& vAvg, double& vP2P) const;
};

#endif // DCDCDIAGNOSTIC_H
