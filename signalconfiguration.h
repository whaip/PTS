// 信号配置模块 - 管理测试信号类型和测试方案
#ifndef SIGNALCONFIGURATION_H
#define SIGNALCONFIGURATION_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QStringList>
#include "commontypes.h"

// 单个信号定义
struct SignalDefinition {
    QString name;               // 信号名称
    SignalType type;            // 信号类型
    double amplitude;           // 幅值
    double frequency;           // 频率 (Hz)
    double phase;               // 相位 (度)
    bool isRequired;            // 是否必需
    QString description;        // 描述
    QMap<QString, QVariant> parameters; // 额外参数
    
    SignalDefinition() : type(SignalType::VOLTAGE_DC), amplitude(0), frequency(0), 
                        phase(0), isRequired(true) {}
};

// 测试方案结构
struct TestSchemeSignals {
    QString name;                           // 方案名称
    QString description;                    // 方案描述
    QVector<SignalDefinition> signals_;      // 信号列表
    QStringList compatibleComponents;       // 兼容的元件类型
    bool requiresSynchronization;           // 是否需要同步
    QString syncGroupName;                  // 同步组名称
    
    TestSchemeSignals() : requiresSynchronization(false) {}
};

// 自定义信号配置
struct SignalConfig {
    QString name;
    SignalType type;
    double amplitude;
    double frequency;
    double phase;
    bool isEnabled;
    QMap<QString, QVariant> parameters;
    
    SignalConfig() : type(SignalType::VOLTAGE_DC), amplitude(0), frequency(0), 
                    phase(0), isEnabled(true) {}
};

// 信号配置管理器
class SignalConfiguration : public QObject
{
    Q_OBJECT

public:
    explicit SignalConfiguration(QObject *parent = nullptr);
    ~SignalConfiguration();

    // 获取预定义的测试方案
    QStringList getAvailableTestSchemes() const;
    TestSchemeSignals getTestScheme(const QString& schemeName) const;
    
    // 自定义信号配置
    bool addCustomSignal(const SignalConfig& signal);
    bool removeCustomSignal(const QString& signalName);
    QStringList getCustomSignals() const;
    
    // 为特定元件类型获取推荐的测试方案
    QStringList getRecommendedSchemes(ComponentType componentType) const;
    
    // 创建自定义测试方案
    bool createCustomScheme(const TestSchemeSignals& scheme);
    bool saveSchemeToFile(const TestSchemeSignals& scheme, const QString& filePath);
    bool loadSchemeFromFile(const QString& filePath, TestSchemeSignals& scheme);
    
    // 验证测试方案的有效性
    bool validateTestScheme(const TestSchemeSignals& scheme, QStringList& errors) const;
    
    // 获取信号类型的描述
    QString getSignalTypeDescription(SignalType type) const;
    QStringList getAllSignalTypes() const;

signals:
    void schemeAdded(const QString& schemeName);
    void schemeRemoved(const QString& schemeName);
    void customSignalAdded(const QString& signalName);
    void customSignalRemoved(const QString& signalName);

private:
    void initializePredefinedSchemes();
    void setupResistorSchemes();
    void setupCapacitorSchemes();
    void setupInductorSchemes();
    void setupDiodeSchemes();
    void setupICSchemes();
    
    QMap<QString, TestSchemeSignals> predefinedSchemes_;
    QMap<QString, SignalConfig> customSignals_;
    QMap<ComponentType, QStringList> componentSchemeMap_;
};

#endif // SIGNALCONFIGURATION_H
