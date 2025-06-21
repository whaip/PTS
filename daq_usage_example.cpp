#include "devicemanager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QThread>
#include <algorithm>
#include <numeric>

/**
 * JY5320/JY5322 数据采集使用示例
 * 展示单点、多点和连续采集三种模式的使用方法
 */

class DAQExample : public QObject
{
    Q_OBJECT
    
public:
    DAQExample(QObject* parent = nullptr) : QObject(parent)
    {
        deviceManager_ = new DeviceManager(this);
    }
    
    void runExample()
    {
        qDebug() << "=== JY5320/JY5322 数据采集示例 ===";
        
        // 初始化设备管理器
        if (!initializeDevices()) {
            qDebug() << "设备初始化失败";
            return;
        }
        
        // 演示三种采集模式
        demonstrateSinglePointAcquisition();
        QThread::msleep(1000);
        
        demonstrateMultiPointAcquisition();
        QThread::msleep(1000);
        
        demonstrateContinuousAcquisition();
        
        qDebug() << "=== 示例完成 ===";
    }
    
private:
    bool initializeDevices()
    {
        qDebug() << "\n--- 初始化设备 ---";
        
        // 初始化设备管理器
        if (!deviceManager_->initializeDeviceThreads()) {
            qDebug() << "设备线程初始化失败:" << deviceManager_->getLastError();
            return false;
        }
        
        // 检查可用的DAQ设备
        QVector<QString> daqDevices = deviceManager_->getDAQDevices();
        if (daqDevices.isEmpty()) {
            qDebug() << "未找到可用的DAQ设备";
            return false;
        }
        
        qDebug() << "找到DAQ设备:" << daqDevices;
        deviceName_ = daqDevices.first();
        
        return true;
    }
    
    void demonstrateSinglePointAcquisition()
    {
        qDebug() << "\n--- 单点采集示例 ---";
        
        // 单通道单点采集
        double result = 0.0;
        if (deviceManager_->singlePointAcquisition(deviceName_, 0, result, -10.0, 10.0, 5000)) {
            qDebug() << "通道0单点采集结果:" << result << "V";
        } else {
            qDebug() << "单点采集失败:" << deviceManager_->getLastError();
        }
        
        // 多通道单点采集
        QVector<int> channels = {0, 1, 2, 3};
        QVector<double> results;
        if (deviceManager_->singlePointAcquisition(deviceName_, channels, results, -10.0, 10.0, 5000)) {
            qDebug() << "多通道单点采集结果:";
            for (int i = 0; i < channels.size(); ++i) {
                qDebug() << QString("  通道%1: %2 V").arg(channels[i]).arg(results[i]);
            }
        } else {
            qDebug() << "多通道单点采集失败:" << deviceManager_->getLastError();
        }
    }
    
    void demonstrateMultiPointAcquisition()
    {
        qDebug() << "\n=== 多点采集示例（渐进式数据读取）===";
        
        QVector<int> channels = {0, 1, 2};
        double sampleRate = 10000.0;       // 10kHz采样率
        int samplesPerChannel = 1000;      // 每通道1000个样本
        
        qDebug() << QString("配置多点采集: 通道%1-%2, 采样率:%3Hz, 样本数:%4")
                   .arg(channels.first()).arg(channels.last())
                   .arg(sampleRate).arg(samplesPerChannel);
        qDebug() << "采用渐进式读取模式：每20个样本读取一次，累积到目标样本数后通知完成";
        
        // 1. 配置多点采集
        if (!deviceManager_->configureMultiPointAcquisition(deviceName_, channels, 
                                                            sampleRate, samplesPerChannel,
                                                            -10.0, 10.0)) {
            qDebug() << "配置多点采集失败:" << deviceManager_->getLastError();
            return;
        }
        
        // 2. 启动采集
        if (!deviceManager_->startMultiPointAcquisition(deviceName_)) {
            qDebug() << "启动多点采集失败:" << deviceManager_->getLastError();
            return;
        }
        
        qDebug() << "多点采集已启动，开始渐进式数据读取...";
        
        // 3. 渐进式数据读取循环
        QVector<QVector<double>> finalChannelData;
        bool acquisitionComplete = false;
        int batchCount = 0;
        int maxAttempts = 100; // 最多尝试100次
        
        while (!acquisitionComplete && batchCount < maxAttempts) {
            batchCount++;
            
            // 读取数据（可能返回批次数据或完整数据）
            QVector<QVector<double>> batchData;
            bool batchSuccess = deviceManager_->readMultiPointData(deviceName_, batchData);
            
            if (batchSuccess) {
                if (!batchData.isEmpty()) {
                    // 收到完整数据，采集完成
                    finalChannelData = batchData;
                    acquisitionComplete = true;
                    qDebug() << QString("采集完成！总批次数: %1, 最终数据 - 通道数: %2, 每通道样本数: %3")
                           .arg(batchCount)
                           .arg(finalChannelData.size())
                           .arg(finalChannelData.isEmpty() ? 0 : finalChannelData[0].size());
                    
                    // 显示数据统计
                    for (int ch = 0; ch < finalChannelData.size(); ++ch) {
                        if (!finalChannelData[ch].isEmpty()) {
                            double minVal = *std::min_element(finalChannelData[ch].begin(), finalChannelData[ch].end());
                            double maxVal = *std::max_element(finalChannelData[ch].begin(), finalChannelData[ch].end());
                            double avgVal = std::accumulate(finalChannelData[ch].begin(), 
                                                           finalChannelData[ch].end(), 0.0) / finalChannelData[ch].size();
                            
                            qDebug() << QString("通道 %1: 样本数=%2, 范围=[%3, %4], 平均值=%5")
                                   .arg(ch)
                                   .arg(finalChannelData[ch].size())
                                   .arg(minVal, 0, 'f', 3)
                                   .arg(maxVal, 0, 'f', 3)
                                   .arg(avgVal, 0, 'f', 3);
                        }
                    }
                } else {
                    // 这是一个批次完成通知，显示进度
                    QString lastError = deviceManager_->getLastError();
                    if (lastError.contains("progress")) {
                        qDebug() << QString("批次 %1 完成 - %2").arg(batchCount).arg(lastError);
                    }
                }
            } else {
                QString error = deviceManager_->getLastError();
                if (error.contains("Waiting for batch data")) {
                    // 正常等待状态，不显示错误
                    if (batchCount % 10 == 0) {
                        qDebug() << QString("等待数据... (批次 %1/%2)").arg(batchCount).arg(maxAttempts);
                    }
                } else {
                    qDebug() << QString("批次 %1 读取失败: %2").arg(batchCount).arg(error);
                }
            }
            
            // 短暂休眠避免过于频繁的轮询
            QThread::msleep(100);
        }
        
        // 4. 停止采集
        if (!deviceManager_->stopMultiPointAcquisition(deviceName_)) {
            qDebug() << "停止多点采集失败:" << deviceManager_->getLastError();
        } else {
            qDebug() << "多点采集已停止";
        }
        
        if (acquisitionComplete) {
            qDebug() << "渐进式多点采集成功完成！";
        } else {
            qDebug() << "警告: 采集超时或未完成";
        }
    }
    
    void demonstrateContinuousAcquisition()
    {
        qDebug() << "\n--- 连续采集示例 ---";
        
        QVector<int> channels = {0, 1};
        double sampleRate = 1000.0;  // 1kHz
        int bufferSize = 5000;       // 缓冲区大小
        
        // 配置连续采集
        if (!deviceManager_->configureContinuousAcquisition(deviceName_, channels, sampleRate, bufferSize)) {
            qDebug() << "连续采集配置失败:" << deviceManager_->getLastError();
            return;
        }
        
        qDebug() << "连续采集已配置: 通道" << channels << ", 采样率" << sampleRate << "Hz, 缓冲区" << bufferSize;
        
        // 启动连续采集
        if (!deviceManager_->startContinuousAcquisition(deviceName_)) {
            qDebug() << "连续采集启动失败:" << deviceManager_->getLastError();
            return;
        }
        
        qDebug() << "连续采集已启动，连续读取数据 5 秒...";
        
        // 连续读取数据5秒
        QElapsedTimer timer;
        timer.start();
        int readCount = 0;
        
        while (timer.elapsed() < 5000) { // 5秒
            QVector<QVector<double>> channelData;
            
            if (deviceManager_->readContinuousData(deviceName_, channelData, 1000, 500)) {
                readCount++;
                
                if (readCount % 5 == 1) { // 每5次读取显示一次信息
                    qDebug() << QString("连续读取 #%1:").arg(readCount);
                    for (int ch = 0; ch < channelData.size(); ++ch) {
                        if (!channelData[ch].isEmpty()) {
                            double avg = 0.0;
                            for (double value : channelData[ch]) {
                                avg += value;
                            }
                            avg /= channelData[ch].size();
                            
                            qDebug() << QString("  通道%1: %2 样本, 平均值: %3 V")
                                       .arg(channels[ch])
                                       .arg(channelData[ch].size())
                                       .arg(avg, 0, 'f', 3);
                        }
                    }
                }
                
                // 模拟数据处理延迟
                QThread::msleep(200);
            } else {
                QThread::msleep(100);
            }
        }
        
        // 停止连续采集
        deviceManager_->stopContinuousAcquisition(deviceName_);
        qDebug() << QString("连续采集已停止，总共读取了 %1 次数据").arg(readCount);
        
        // 显示采集状态
        QVariantMap status = deviceManager_->getAcquisitionStatus(deviceName_);
        qDebug() << "采集状态:" << status;
    }
    
private:
    DeviceManager* deviceManager_;
    QString deviceName_;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    DAQExample example;
    
    // 使用定时器启动示例，以便消息循环运行
    QTimer::singleShot(100, &example, &DAQExample::runExample);
    
    // 5分钟后自动退出
    QTimer::singleShot(300000, &app, &QCoreApplication::quit);
    
    return app.exec();
}

#include "daq_usage_example.moc" 