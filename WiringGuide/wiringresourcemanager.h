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

    bool validateWiringScheme(const WiringScheme& scheme, QStringList& errors, const QString& currentUser) const;
    bool checkPortConflicts(const WiringScheme& scheme, QStringList& conflicts, const QString& currentUser) const;
    
    // 资源分配和释放
    bool allocateResourcesForScheme(const WiringScheme& scheme);
    bool releaseResourcesForScheme(const WiringScheme& scheme);
    void releaseAllResources();

signals:
    void resourcesAllocated(const QString& schemeId);
    void resourcesReleased(const QString& schemeId);

private slots:
    void onPortStatusChanged(const QString& deviceName, int portNumber, bool available);

private:
    PortManager* portManager_;
    
    bool isDestructing_;  // 析构标志

      // 验证辅助方法
    bool validatePortAvailability(const QVector<ConnectionInfo>& connections, QStringList& errors, const QString& currentUser) const;
    bool validateSignalCompatibility(const QVector<ConnectionInfo>& connections, QStringList& errors) const;
};

#endif // WIRINGRESOURCEMANAGER_H
