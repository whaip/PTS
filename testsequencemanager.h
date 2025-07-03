#ifndef TESTSEQUENCEMANAGER_H
#define TESTSEQUENCEMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QString>
#include <QList>
#include "faultdiagnostic.h"

// Component specifications structure
struct ComponentSpecs {
    // 电阻规格
    struct {
        double nominal;           // 标称值
        double tolerance;         // 容差 (比例值，如0.05表示5%)
        double tempCoefficient;   // 温度系数 (ppm/°C)
    } resistance;
    
    // 电容规格
    struct {
        double nominal;           // 标称值 (F)
        double tolerance;         // 容差 (比例值)
        double esr;              // 等效串联电阻 (Ω)
        double leakageCurrent;   // 漏电流 (A)
    } capacitance;
    
    // 电感规格
    struct {
        double nominal;           // 标称值 (H)
        double tolerance;         // 容差 (比例值)
        double qFactory;         // 品质因数
        double dcResistance;     // 直流电阻 (Ω)
    } inductance;
    
    // 二极管规格
    struct {
        double forwardVoltage;    // 正向压降 (V)
        double reverseLeakage;    // 反向漏电流 (A)
        double breakdownVoltage;  // 击穿电压 (V)
    } diode;
    
    // IC规格
    struct {
        double supplyVoltage;     // 供电电压 (V)
        double supplyCurrent;     // 供电电流 (A)
        struct {
            double high;          // 高电平 (V)
            double low;           // 低电平 (V)
        } inputLevels;
        struct {
            double high;          // 高电平 (V)
            double low;           // 低电平 (V)
        } outputLevels;
    } ic;
};

struct TestStep {
    QString componentType;      // "resistor", "capacitor", "inductor", "diode", "ic"
    ComponentType common_type;
    QString testName;          // 测试名称
    QJsonObject parameters;    // 测试参数
    QMap<QString, QVariant> specs;      // 获取的参数
    bool enabled;             // 是否启用此测试步骤
    int timeoutMs;            // 超时时间
    QVector<PortInfo> allocatedPorts;
    QString component; // component reference (e.g., "R1", "C2")
};

struct TestSequence {
    QString name;              // 测试序列名称
    QString description;       // 描述
    QList<TestStep> steps;     // 测试步骤
    QString createdDate;       // 创建日期
    QString modifiedDate;      // 修改日期
    QString version;           // 版本号
    QJsonObject metadata;      // 元数据
};

class TestSequenceManager : public QObject
{
    Q_OBJECT

public:
    explicit TestSequenceManager(QObject *parent = nullptr);
    
    // 序列管理
    bool loadSequence(const QString& filePath, TestSequence& sequence);
    bool saveSequence(const QString& filePath, const TestSequence& sequence);
    
    // 序列操作
    void addTestStep(TestSequence& sequence, const TestStep& step);
    void removeTestStep(TestSequence& sequence, int index);
    void removeAllTestStep(TestSequence& sequence);
    void moveTestStep(TestSequence& sequence, int from, int to);
    void updateTestStep(TestSequence& sequence, int index, const TestStep& step);
    
    // 验证和转换
    bool validateSequence(const TestSequence& sequence, QStringList& errors);
    QJsonObject sequenceToJson(const TestSequence& sequence);
    TestSequence jsonToSequence(const QJsonObject& json);
    
    // 导入导出
    bool exportToCSV(const TestSequence& sequence, const QString& filePath);
    bool importFromCSV(const QString& filePath, TestSequence& sequence);
    
    // 最近文件管理
    QStringList getRecentFiles() const;
    void addRecentFile(const QString& filePath);
    
signals:
    void sequenceLoaded(const TestSequence& sequence);
    void sequenceSaved(const QString& filePath);
    void errorOccurred(const QString& error);

private:
    // JSON转换辅助函数
    QJsonObject testStepToJson(const TestStep& step);
    TestStep jsonToTestStep(const QJsonObject& json);
    QJsonObject componentParamsToJson(const QMap<QString, QVariant>& params);
    QMap<QString, QVariant> jsonToComponentParams(const QJsonObject& json);
    
    // 文件管理
    QStringList recent_files_;
    static const int MAX_RECENT_FILES = 10;
    
    void updateRecentFiles();
    void loadRecentFiles();
    void saveRecentFiles();
};

#endif // TESTSEQUENCEMANAGER_H
