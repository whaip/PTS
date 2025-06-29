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
    json["specs"] = componentParamsToJson(step.specs);
    
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
    step.specs = jsonToComponentParams(json["specs"].toObject());
    
    return step;
}

QJsonObject TestSequenceManager::componentParamsToJson(const QMap<QString, QVariant>& params)
{
    QJsonObject json;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        json.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    return json;
}

QMap<QString, QVariant> TestSequenceManager::jsonToComponentParams(const QJsonObject& json)
{
    QMap<QString, QVariant> params;
    for (auto it = json.constBegin(); it != json.constEnd(); ++it) {
        params.insert(it.key(), it.value().toVariant());
    }
    return params;
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
