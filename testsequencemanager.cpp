#include "testsequencemanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <QSettings>

TestSequenceManager::TestSequenceManager(QObject *parent)
    : QObject(parent)
{
    loadRecentFiles();
}

bool TestSequenceManager::loadSequence(const QString& filePath, TestSequence& sequence)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(QString("Cannot open file: %1").arg(filePath));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit errorOccurred(QString("JSON parse error: %1").arg(error.errorString()));
        return false;
    }
    
    if (!doc.isObject()) {
        emit errorOccurred("Invalid JSON format: root is not an object");
        return false;
    }
    
    sequence = jsonToSequence(doc.object());
    addRecentFile(filePath);
    emit sequenceLoaded(sequence);
    
    qDebug() << "Test sequence loaded successfully from:" << filePath;
    return true;
}

bool TestSequenceManager::saveSequence(const QString& filePath, const TestSequence& sequence)
{
    QJsonObject json = sequenceToJson(sequence);
    QJsonDocument doc(json);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred(QString("Cannot create file: %1").arg(filePath));
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    addRecentFile(filePath);
    emit sequenceSaved(filePath);
    
    qDebug() << "Test sequence saved successfully to:" << filePath;
    return true;
}

bool TestSequenceManager::createDefaultSequence(TestSequence& sequence)
{
    sequence.name = "Default PCB Component Test";
    sequence.description = "Standard PCB component fault diagnosis sequence";
    sequence.version = "1.0";
    sequence.createdDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    sequence.modifiedDate = sequence.createdDate;
    
    // 电阻测试步骤
    TestStep resistorStep;
    resistorStep.componentType = "resistor";
    resistorStep.testName = "Resistance and Temperature Coefficient Test";
    resistorStep.enabled = true;
    resistorStep.timeoutMs = 5000;
    resistorStep.specs.resistance.nominal = 1000.0; // 1KΩ
    resistorStep.specs.resistance.tolerance = 0.05; // 5%
    resistorStep.specs.resistance.tempCoefficient = 100e-6; // 100ppm/°C
    resistorStep.parameters["test_voltages"] = QJsonArray{0.1, 0.5, 1.0, 2.0};
    resistorStep.parameters["temperature_range"] = QJsonArray{25, 85};
    
    sequence.steps.append(resistorStep);
    
    // 电容测试步骤
    TestStep capacitorStep;
    capacitorStep.componentType = "capacitor";
    capacitorStep.testName = "Capacitance and ESR Test";
    capacitorStep.enabled = true;
    capacitorStep.timeoutMs = 8000;
    capacitorStep.specs.capacitance.nominal = 100e-6; // 100μF
    capacitorStep.specs.capacitance.tolerance = 0.20; // 20%
    capacitorStep.specs.capacitance.esr = 0.1; // 0.1Ω
    capacitorStep.specs.capacitance.leakageCurrent = 1e-6; // 1μA
    capacitorStep.parameters["test_frequencies"] = QJsonArray{100, 1000, 10000};
    capacitorStep.parameters["test_voltage"] = 5.0;
    
    sequence.steps.append(capacitorStep);
    
    // 电感测试步骤
    TestStep inductorStep;
    inductorStep.componentType = "inductor";
    inductorStep.testName = "Inductance and Q Factor Test";
    inductorStep.enabled = true;
    inductorStep.timeoutMs = 6000;
    inductorStep.specs.inductance.nominal = 100e-6; // 100μH
    inductorStep.specs.inductance.tolerance = 0.10; // 10%
    inductorStep.specs.inductance.qFactory = 50;
    inductorStep.specs.inductance.dcResistance = 0.5; // 0.5Ω
    inductorStep.parameters["test_frequencies"] = QJsonArray{1000, 10000, 100000};
    inductorStep.parameters["test_current"] = 0.1;
    
    sequence.steps.append(inductorStep);
    
    // 二极管测试步骤
    TestStep diodeStep;
    diodeStep.componentType = "diode";
    diodeStep.testName = "Forward and Reverse Characteristics Test";
    diodeStep.enabled = true;
    diodeStep.timeoutMs = 4000;
    diodeStep.specs.diode.forwardVoltage = 0.7; // 0.7V
    diodeStep.specs.diode.reverseLeakage = 1e-9; // 1nA
    diodeStep.specs.diode.breakdownVoltage = 50.0; // 50V
    diodeStep.parameters["forward_currents"] = QJsonArray{0.001, 0.01, 0.1};
    diodeStep.parameters["reverse_voltage"] = 10.0;
    
    sequence.steps.append(diodeStep);
    
    // IC测试步骤
    TestStep icStep;
    icStep.componentType = "ic";
    icStep.testName = "IC Power and Functionality Test";
    icStep.enabled = true;
    icStep.timeoutMs = 10000;
    icStep.specs.ic.supplyVoltage = 5.0; // 5V
    icStep.specs.ic.supplyCurrent = 0.05; // 50mA
    icStep.specs.ic.inputLevels.high = 2.0; // 2V
    icStep.specs.ic.inputLevels.low = 0.8; // 0.8V
    icStep.specs.ic.outputLevels.high = 2.4; // 2.4V
    icStep.specs.ic.outputLevels.low = 0.4; // 0.4V
    icStep.parameters["test_patterns"] = QJsonArray{0x00, 0xFF, 0xAA, 0x55};
    icStep.parameters["clock_frequency"] = 1000000; // 1MHz
    
    sequence.steps.append(icStep);
    
    qDebug() << "Default test sequence created with" << sequence.steps.size() << "steps";
    return true;
}

void TestSequenceManager::addTestStep(TestSequence& sequence, const TestStep& step)
{
    sequence.steps.append(step);
    sequence.modifiedDate = QDateTime::currentDateTime().toString(Qt::ISODate);
}

void TestSequenceManager::removeTestStep(TestSequence& sequence, int index)
{
    if (index >= 0 && index < sequence.steps.size()) {
        sequence.steps.removeAt(index);
        sequence.modifiedDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    }
}

void TestSequenceManager::moveTestStep(TestSequence& sequence, int from, int to)
{
    if (from >= 0 && from < sequence.steps.size() && 
        to >= 0 && to < sequence.steps.size() && from != to) {
        sequence.steps.move(from, to);
        sequence.modifiedDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    }
}

void TestSequenceManager::updateTestStep(TestSequence& sequence, int index, const TestStep& step)
{
    if (index >= 0 && index < sequence.steps.size()) {
        sequence.steps[index] = step;
        sequence.modifiedDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    }
}

bool TestSequenceManager::validateSequence(const TestSequence& sequence, QStringList& errors)
{
    errors.clear();
    
    if (sequence.name.isEmpty()) {
        errors << "Sequence name is empty";
    }
    
    if (sequence.steps.isEmpty()) {
        errors << "No test steps defined";
    }
    
    for (int i = 0; i < sequence.steps.size(); ++i) {
        const TestStep& step = sequence.steps[i];
        
        if (step.componentType.isEmpty()) {
            errors << QString("Step %1: Component type is empty").arg(i + 1);
        }
        
        if (step.testName.isEmpty()) {
            errors << QString("Step %1: Test name is empty").arg(i + 1);
        }
        
        if (step.timeoutMs <= 0) {
            errors << QString("Step %1: Invalid timeout value").arg(i + 1);
        }
        
        // 验证组件特定的规格
        if (step.componentType == "resistor") {
            if (step.specs.resistance.nominal <= 0) {
                errors << QString("Step %1: Invalid resistance value").arg(i + 1);
            }
        } else if (step.componentType == "capacitor") {
            if (step.specs.capacitance.nominal <= 0) {
                errors << QString("Step %1: Invalid capacitance value").arg(i + 1);
            }
        }
        // 添加其他组件类型的验证...
    }
    
    return errors.isEmpty();
}

QJsonObject TestSequenceManager::sequenceToJson(const TestSequence& sequence)
{
    QJsonObject json;
    json["name"] = sequence.name;
    json["description"] = sequence.description;
    json["version"] = sequence.version;
    json["created_date"] = sequence.createdDate;
    json["modified_date"] = sequence.modifiedDate;
    json["metadata"] = sequence.metadata;
    
    QJsonArray stepsArray;
    for (const TestStep& step : sequence.steps) {
        stepsArray.append(testStepToJson(step));
    }
    json["steps"] = stepsArray;
    
    return json;
}

TestSequence TestSequenceManager::jsonToSequence(const QJsonObject& json)
{
    TestSequence sequence;
    
    sequence.name = json["name"].toString();
    sequence.description = json["description"].toString();
    sequence.version = json["version"].toString();
    sequence.createdDate = json["created_date"].toString();
    sequence.modifiedDate = json["modified_date"].toString();
    sequence.metadata = json["metadata"].toObject();
    
    QJsonArray stepsArray = json["steps"].toArray();
    for (const QJsonValue& value : stepsArray) {
        if (value.isObject()) {
            sequence.steps.append(jsonToTestStep(value.toObject()));
        }
    }
    
    return sequence;
}

QJsonObject TestSequenceManager::testStepToJson(const TestStep& step)
{
    QJsonObject json;
    json["component_type"] = step.componentType;
    json["test_name"] = step.testName;
    json["enabled"] = step.enabled;
    json["timeout_ms"] = step.timeoutMs;
    json["parameters"] = step.parameters;
    json["specs"] = componentSpecsToJson(step.specs);
    
    return json;
}

TestStep TestSequenceManager::jsonToTestStep(const QJsonObject& json)
{
    TestStep step;
    
    step.componentType = json["component_type"].toString();
    step.testName = json["test_name"].toString();
    step.enabled = json["enabled"].toBool(true);
    step.timeoutMs = json["timeout_ms"].toInt(5000);
    step.parameters = json["parameters"].toObject();
    step.specs = jsonToComponentSpecs(json["specs"].toObject());
    
    return step;
}

QJsonObject TestSequenceManager::componentSpecsToJson(const ComponentSpecs& specs)
{
    QJsonObject json;
    
    // 电阻规格
    QJsonObject resistance;
    resistance["nominal"] = specs.resistance.nominal;
    resistance["tolerance"] = specs.resistance.tolerance;
    resistance["temp_coefficient"] = specs.resistance.tempCoefficient;
    json["resistance"] = resistance;
    
    // 电容规格
    QJsonObject capacitance;
    capacitance["nominal"] = specs.capacitance.nominal;
    capacitance["tolerance"] = specs.capacitance.tolerance;
    capacitance["esr"] = specs.capacitance.esr;
    capacitance["leakage_current"] = specs.capacitance.leakageCurrent;
    json["capacitance"] = capacitance;
    
    // 电感规格
    QJsonObject inductance;
    inductance["nominal"] = specs.inductance.nominal;
    inductance["tolerance"] = specs.inductance.tolerance;
    inductance["q_factor"] = specs.inductance.qFactory;
    inductance["dc_resistance"] = specs.inductance.dcResistance;
    json["inductance"] = inductance;
    
    // 二极管规格
    QJsonObject diode;
    diode["forward_voltage"] = specs.diode.forwardVoltage;
    diode["reverse_leakage"] = specs.diode.reverseLeakage;
    diode["breakdown_voltage"] = specs.diode.breakdownVoltage;
    json["diode"] = diode;
    
    // IC规格
    QJsonObject ic;
    ic["supply_voltage"] = specs.ic.supplyVoltage;
    ic["supply_current"] = specs.ic.supplyCurrent;
    
    QJsonObject inputLevels;
    inputLevels["high"] = specs.ic.inputLevels.high;
    inputLevels["low"] = specs.ic.inputLevels.low;
    ic["input_levels"] = inputLevels;
    
    QJsonObject outputLevels;
    outputLevels["high"] = specs.ic.outputLevels.high;
    outputLevels["low"] = specs.ic.outputLevels.low;
    ic["output_levels"] = outputLevels;
    
    json["ic"] = ic;
    
    return json;
}

ComponentSpecs TestSequenceManager::jsonToComponentSpecs(const QJsonObject& json)
{
    ComponentSpecs specs;
    
    // 电阻规格
    QJsonObject resistance = json["resistance"].toObject();
    specs.resistance.nominal = resistance["nominal"].toDouble();
    specs.resistance.tolerance = resistance["tolerance"].toDouble();
    specs.resistance.tempCoefficient = resistance["temp_coefficient"].toDouble();
    
    // 电容规格
    QJsonObject capacitance = json["capacitance"].toObject();
    specs.capacitance.nominal = capacitance["nominal"].toDouble();
    specs.capacitance.tolerance = capacitance["tolerance"].toDouble();
    specs.capacitance.esr = capacitance["esr"].toDouble();
    specs.capacitance.leakageCurrent = capacitance["leakage_current"].toDouble();
    
    // 电感规格
    QJsonObject inductance = json["inductance"].toObject();
    specs.inductance.nominal = inductance["nominal"].toDouble();
    specs.inductance.tolerance = inductance["tolerance"].toDouble();
    specs.inductance.qFactory = inductance["q_factor"].toDouble();
    specs.inductance.dcResistance = inductance["dc_resistance"].toDouble();
    
    // 二极管规格
    QJsonObject diode = json["diode"].toObject();
    specs.diode.forwardVoltage = diode["forward_voltage"].toDouble();
    specs.diode.reverseLeakage = diode["reverse_leakage"].toDouble();
    specs.diode.breakdownVoltage = diode["breakdown_voltage"].toDouble();
    
    // IC规格
    QJsonObject ic = json["ic"].toObject();
    specs.ic.supplyVoltage = ic["supply_voltage"].toDouble();
    specs.ic.supplyCurrent = ic["supply_current"].toDouble();
    
    QJsonObject inputLevels = ic["input_levels"].toObject();
    specs.ic.inputLevels.high = inputLevels["high"].toDouble();
    specs.ic.inputLevels.low = inputLevels["low"].toDouble();
    
    QJsonObject outputLevels = ic["output_levels"].toObject();
    specs.ic.outputLevels.high = outputLevels["high"].toDouble();
    specs.ic.outputLevels.low = outputLevels["low"].toDouble();
    
    return specs;
}

bool TestSequenceManager::exportToCSV(const TestSequence& sequence, const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(QString("Cannot create CSV file: %1").arg(filePath));
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // CSV头部
    out << "Step,Component Type,Test Name,Enabled,Timeout(ms),Parameters\n";
    
    // 写入测试步骤
    for (int i = 0; i < sequence.steps.size(); ++i) {
        const TestStep& step = sequence.steps[i];
        
        QString parameters;
        QJsonDocument paramDoc(step.parameters);
        parameters = paramDoc.toJson(QJsonDocument::Compact);
        parameters.replace("\"", "\"\""); // 转义CSV中的引号
        
        out << QString("%1,\"%2\",\"%3\",%4,%5,\"%6\"\n")
               .arg(i + 1)
               .arg(step.componentType)
               .arg(step.testName)
               .arg(step.enabled ? "Yes" : "No")
               .arg(step.timeoutMs)
               .arg(parameters);
    }
    
    file.close();
    qDebug() << "Test sequence exported to CSV:" << filePath;
    return true;
}

QStringList TestSequenceManager::getRecentFiles() const
{
    return recent_files_;
}

void TestSequenceManager::addRecentFile(const QString& filePath)
{
    recent_files_.removeAll(filePath);
    recent_files_.prepend(filePath);
    
    while (recent_files_.size() > MAX_RECENT_FILES) {
        recent_files_.removeLast();
    }
    
    saveRecentFiles();
}

void TestSequenceManager::loadRecentFiles()
{
    QSettings settings;
    recent_files_ = settings.value("recentFiles").toStringList();
    
    // 移除不存在的文件
    QStringList existing;
    for (const QString& file : recent_files_) {
        if (QFile::exists(file)) {
            existing.append(file);
        }
    }
    recent_files_ = existing;
}

void TestSequenceManager::saveRecentFiles()
{
    QSettings settings;
    settings.setValue("recentFiles", recent_files_);
}
