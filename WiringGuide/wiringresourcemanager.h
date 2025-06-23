#ifndef WIRINGRESOURCEMANAGER_H
#define WIRINGRESOURCEMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include "portdefinitions.h"
#include "portmanager.h"

using namespace PortDefinitions;

class WiringResourceManager : public QObject
{
    Q_OBJECT

public:
    explicit WiringResourceManager(PortManager* portManager, QObject *parent = nullptr);
    ~WiringResourceManager();

    // 初始化资源管理器
    bool initializeResources();
    
    // 接线方案管理
    bool saveWiringScheme(const WiringScheme& scheme, const QString& filePath = QString());
    bool loadWiringScheme(const QString& filePath, WiringScheme& scheme);
    QStringList getAvailableSchemes() const;
    WiringScheme getScheme(const QString& schemeId) const;
    
    // 接线模板管理
    QVector<WiringScheme> getTemplatesForComponent(ComponentType componentType) const;
    bool saveTemplate(const WiringScheme& scheme);
    bool deleteTemplate(const QString& schemeId);    // 接线验证
    bool validateWiringScheme(const WiringScheme& scheme, QStringList& errors) const;
    bool validateWiringScheme(const WiringScheme& scheme, QStringList& errors, const QString& currentUser) const;
    bool checkPortConflicts(const WiringScheme& scheme, QStringList& conflicts) const;
    bool checkPortConflicts(const WiringScheme& scheme, QStringList& conflicts, const QString& currentUser) const;
    
    // 资源分配和释放
    bool allocateResourcesForScheme(const WiringScheme& scheme);
    bool releaseResourcesForScheme(const WiringScheme& scheme);
    void releaseAllResources();
    
    // 获取推荐方案
    QVector<WiringScheme> getRecommendedSchemes(ComponentType componentType) const;
    WiringScheme generateOptimalScheme(const ComponentSpec& component) const;
    
    // 统计信息
    int getTotalSchemes() const;
    int getActiveSchemes() const;
    QMap<ComponentType, int> getSchemeStatistics() const;

signals:
    void schemeCreated(const QString& schemeId);
    void schemeUpdated(const QString& schemeId);
    void schemeDeleted(const QString& schemeId);
    void resourcesAllocated(const QString& schemeId);
    void resourcesReleased(const QString& schemeId);

private slots:
    void onPortStatusChanged(const QString& deviceName, int portNumber, bool available);

private:
    PortManager* portManager_;
    QMap<QString, WiringScheme> loadedSchemes_;
    QMap<ComponentType, QVector<WiringScheme>> templates_;
    QMap<QString, QString> activeAllocations_; // schemeId -> allocation info
    
    QString schemesPath_;
    QString templatesPath_;
    QString configPath_;
    
    bool isDestructing_;  // 析构标志
    
    // 初始化方法
    bool createDirectories();
    bool loadDefaultTemplates();
    bool loadExistingSchemes();
    
    // 文件操作
    QString generateSchemeFilePath(const QString& schemeId) const;
    QString generateTemplateFilePath(ComponentType componentType, const QString& schemeId) const;
    bool saveSchemeToFile(const WiringScheme& scheme, const QString& filePath) const;
    bool loadSchemeFromFile(const QString& filePath, WiringScheme& scheme) const;
    
    // 模板生成
    WiringScheme createDefaultResistorTemplate() const;
    WiringScheme createDefaultCapacitorTemplate() const;
    WiringScheme createDefaultInductorTemplate() const;
    WiringScheme createDefaultDiodeTemplate() const;
    WiringScheme createDefaultICTemplate() const;
      // 验证辅助方法
    bool validateConnections(const QVector<ConnectionInfo>& connections, QStringList& errors) const;
    bool validatePortAvailability(const QVector<ConnectionInfo>& connections, QStringList& errors) const;
    bool validatePortAvailability(const QVector<ConnectionInfo>& connections, QStringList& errors, const QString& currentUser) const;
    bool validateSignalCompatibility(const QVector<ConnectionInfo>& connections, QStringList& errors) const;
    
    // 工具方法
    QString componentTypeToString(ComponentType type) const;
    ComponentType stringToComponentType(const QString& typeString) const;
    QJsonObject connectionToJson(const ConnectionInfo& connection) const;
    ConnectionInfo connectionFromJson(const QJsonObject& jsonObj) const;
    QJsonObject portInfoToJson(const PortInfo& port) const;
    PortInfo portInfoFromJson(const QJsonObject& jsonObj) const;
};

#endif // WIRINGRESOURCEMANAGER_H
