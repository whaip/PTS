#include "ch340.h"
#include <QSerialPortInfo>

CH340::CH340() : m_serialPort(new QSerialPort())
{
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        if (info.description() == "USB-SERIAL CH340" && info.portName() == "COM5") {
            if(openPort(info.portName(), QSerialPort::Baud9600, QSerialPort::Data8, QSerialPort::NoParity, QSerialPort::OneStop)){
                qDebug() << "CH340设备打开成功";
            }else{
                qDebug() << "CH340设备打开失败：" << getLastError();
            }
            break;
        }
    }
    m_running = true;
    m_writeThread = std::thread(&CH340::writeThreadFunc, this);
}

CH340::~CH340()
{
    m_running = false;
    m_queueCondition.notify_all();
    if (m_writeThread.joinable()) {
        m_writeThread.join();
    }
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    delete m_serialPort;
}

void CH340::writeThreadFunc()
{
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueCondition.wait(lock, [this] { 
            return !m_writeQueue.empty() || !m_running; 
        });

        if (!m_running) break;

        processWriteQueue();
    }
}

void CH340::processWriteQueue()
{
    while (!m_writeQueue.empty()) {
        QByteArray data = m_writeQueue.front();
        m_writeQueue.pop();

        std::lock_guard<std::mutex> lock(m_serialPortMutex);
        if (m_serialPort->isOpen()) {
            qint64 bytesWritten = m_serialPort->write(data);
            if (bytesWritten == -1) {
                m_lastError = m_serialPort->errorString();
                qDebug() << "Write error:" << m_lastError;
            } else {
                // 等待数据写入完成，但使用较短的超时时间
                if (!m_serialPort->waitForBytesWritten(100)) {
                    m_lastError = "数据写入超时";
                    qDebug() << "Write timeout";
                }
            }
        }
    }
}

bool CH340::openPort(const QString& portName, 
                    int baudRate,
                    QSerialPort::DataBits dataBits,
                    QSerialPort::Parity parity,
                    QSerialPort::StopBits stopBits)
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(dataBits);
    m_serialPort->setParity(parity);
    m_serialPort->setStopBits(stopBits);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        m_lastError = m_serialPort->errorString();
        return false;
    }
    return true;
}

void CH340::closePort()
{
    std::lock_guard<std::mutex> lock(m_serialPortMutex);
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
}

bool CH340::writeData(const QByteArray& data)
{
    if (!m_serialPort->isOpen()) {
        m_lastError = "串口未打开";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_writeQueue.push(data);
    }
    m_queueCondition.notify_one();
    return true;
}

QByteArray CH340::readData()
{
    std::lock_guard<std::mutex> lock(m_serialPortMutex);
    if (!m_serialPort->isOpen()) {
        m_lastError = "串口未打开";
        return QByteArray();
    }

    if (!m_serialPort->waitForReadyRead(100)) {
        return QByteArray();
    }
    
    return m_serialPort->readAll();
}

bool CH340::isOpen()
{
    std::lock_guard<std::mutex> lock(m_serialPortMutex);
    return m_serialPort->isOpen();
}

QString CH340::getLastError()
{
    std::lock_guard<std::mutex> lock(m_lastErrorMutex);
    return m_lastError;
}

bool CH340::writeString(const QString& str)
{
    if (!m_serialPort->isOpen()) {
        m_lastError = "串口未打开";
        return false;
    }

    QByteArray data = str.toUtf8();
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_writeQueue.push(data);
    }
    m_queueCondition.notify_one();

    if(str == "FF01FF") qDebug() << "ch340 open";
    if(str == "FF00FF") qDebug() << "ch340 close";
    
    return true;
}
