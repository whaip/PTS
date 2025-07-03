#ifndef CH340_H
#define CH340_H

#include <QString>
#include <QSerialPort>
#include <QByteArray>
#include <QDebug>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

#define g_ch340 CH340::getInstance()

class CH340
{
public:
    static CH340* getInstance(){
        static CH340 instance;
        return &instance;
    }
    CH340(const CH340&) = delete;
    CH340& operator=(const CH340&) = delete;
    ~CH340();

    // 打开串口
    bool openPort(const QString& portName,
                  int baudRate = QSerialPort::Baud9600,
                  QSerialPort::DataBits dataBits = QSerialPort::Data8,
                  QSerialPort::Parity parity = QSerialPort::NoParity,
                  QSerialPort::StopBits stopBits = QSerialPort::OneStop);

    // 检查串口是否打开
    bool isOpen();
    bool Open();
    bool Close();

    // 获取错误信息
    QString getLastError();
private:
    CH340();

    void closePort();
    bool writeData(const QByteArray& data);
    void writeThreadFunc();  // 写入线程函数
    bool writeString(const QString& str);
    void processWriteQueue();  // 处理写入队列
    QByteArray readData();

    QSerialPort* m_serialPort;
    QString m_lastError;
    
    // 写入队列相关
    std::queue<QByteArray> m_writeQueue;
    std::mutex m_queueMutex;
    std::mutex m_lastErrorMutex;
    std::mutex m_serialPortMutex;
    std::condition_variable m_queueCondition;
    std::atomic<bool> m_running{false};
    std::thread m_writeThread;
};

#endif // CH340_H
