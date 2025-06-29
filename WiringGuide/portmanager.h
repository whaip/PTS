#ifndef PORTMANAGER_H
#define PORTMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <QTimer>
#include "portdefinitions.h"
#include "devicemanager.h"
#include "commontypes.h"

using namespace PortDefinitions;

class PortManager : public QObject
{
    Q_OBJECT

public:
    explicit PortManager(DeviceManager* deviceManager, QObject *parent = nullptr);
    ~PortManager();

    // 初始化端口系统
    bool initializePorts();
    
    // 端口信息查询
    QVector<PortInfo> getAvailablePorts(PortType type = PortType::ANALOG_INPUT) const;
    QVector<PortInfo> getAllPorts() const;
    PortInfo getPortInfo(const QString& deviceName, int portNumber) const;
      // 端口分配和释放
    bool allocatePort(const QString& deviceName, int portNumber, const QString& allocatedTo);
    bool releasePort(const QString& deviceName, int portNumber);
    void releaseAllPorts();
    void releasePortsForUser(const QString& allocatedTo);
    void releasePortsForUser(const QString& allocatedTo, bool emitSignals);
    
    // 清理无效分配
    void cleanupInvalidAllocations();
    
    // 端口状态检查
    bool isPortAvailable(const QString& deviceName, int portNumber) const;
    bool isPortAllocated(const QString& deviceName, int portNumber) const;
    QString getPortAllocatedTo(const QString& deviceName, int portNumber) const;
    
    // 自动端口分配
    QVector<PortInfo> autoAllocatePorts(QVector<PortRequirement> &requirements, QString &allocatedTo);
    
    // 端口验证
    bool validatePortConfiguration(const QVector<ConnectionInfo>& connections, QStringList& errors) const;
    
    // 获取推荐端口
    QVector<PortInfo> getRecommendedPorts(ComponentType componentType) const;
    
    // 端口配置保存和加载
    bool savePortConfiguration(const QString& filePath) const;
    bool loadPortConfiguration(const QString& filePath);

signals:
    void portAllocated(const QString& deviceName, int portNumber, const QString& allocatedTo);
    void portReleased(const QString& deviceName, int portNumber);
    void portStatusChanged(const QString& deviceName, int portNumber, bool available);

public slots:
    void updateDeviceStatus();
    
private slots:
    void onDeviceStatusChanged(const QString& device, DeviceStatus status);

private:
    DeviceManager* deviceManager_;
    QMap<QString, QVector<PortInfo>> devicePorts_;  // 设备名 -> 端口列表
    mutable QMutex portMutex_;
    QTimer* statusUpdateTimer_;
      void initializeJY5711Ports();
    void initializeJY5323Ports();
    void initializeJY5322Ports();
    void initializeJY8902Ports();  // 添加万用表端口初始化函数
      QString generatePortKey(const QString& deviceName, int portNumber) const;
    PortInfo* findPort(const QString& deviceName, int portNumber);
    const PortInfo* findPort(const QString& deviceName, int portNumber) const;
    
    // 内部清理函数（不加锁）
    void cleanupInvalidAllocationsInternal();
    
    // 元件类型对应的推荐端口配置
    QVector<PortInfo> getResistorPorts() const;
    QVector<PortInfo> getCapacitorPorts() const;
    QVector<PortInfo> getInductorPorts() const;
    QVector<PortInfo> getDiodePorts() const;
    QVector<PortInfo> getICPorts() const;
};

#endif // PORTMANAGER_H
