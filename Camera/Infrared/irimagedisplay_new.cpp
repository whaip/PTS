#include "irimagedisplay.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QSizePolicy>
#include <QWidget>
#include <QApplication>
#include <QFontMetrics>
#include <algorithm>

IRImageDisplay::IRImageDisplay(QWidget *parent) 
    : QWidget(parent)
    , m_tempWidth(0)
    , m_tempHeight(0)
    , m_isActive(true)
    , m_markMaxTemp(false)
    , m_mouseOverWidget(false)
    , m_isDestroying(false)
{
    initializeUI();
    setupLabels();
    
    // 启用鼠标跟踪
    setMouseTracking(true);
    
    // 设置窗口属性
    setWindowTitle("IR Image Display");
    setMinimumSize(640, 480);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    qDebug() << "IRImageDisplay initialized successfully";
}

IRImageDisplay::~IRImageDisplay()
{
    qDebug() << "IRImageDisplay destructor called";
    
    // 标记正在销毁
    m_isDestroying = true;
    m_isActive.store(false);
    
    // 安全停止定时器
    safeStopTimer();
    
    // 清理数据
    {
        QMutexLocker locker(&m_dataMutex);
        m_currentImage = QImage();
        m_tempData.clear();
        m_tempWidth = 0;
        m_tempHeight = 0;
    }
    
    // 智能指针会自动清理UI组件
    qDebug() << "IRImageDisplay destroyed successfully";
}

void IRImageDisplay::initializeUI()
{
    // 创建显示标签
    m_displayLabel = std::make_unique<QLabel>(this);
    m_displayLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_displayLabel->setAlignment(Qt::AlignCenter);
    m_displayLabel->setStyleSheet("QLabel { border: 1px solid gray; background-color: black; }");
    
    // 创建更新定时器
    m_updateTimer = std::make_unique<QTimer>(this);
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(UPDATE_INTERVAL_MS);
    connect(m_updateTimer.get(), &QTimer::timeout, this, &IRImageDisplay::updateDisplay);
    
    // 设置布局
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_displayLabel.get());
    setLayout(layout);
}

void IRImageDisplay::setupLabels()
{
    // 创建温度显示标签
    m_maxTempLabel = std::make_unique<QLabel>(this);
    m_minTempLabel = std::make_unique<QLabel>(this);
    m_centerTempLabel = std::make_unique<QLabel>(this);
    
    // 设置标签样式
    QString labelStyle = "QLabel { "
                        "background-color: rgba(0, 0, 0, 180); "
                        "color: white; "
                        "padding: 5px; "
                        "border-radius: 3px; "
                        "font-weight: bold; "
                        "}";
    
    m_maxTempLabel->setStyleSheet(labelStyle);
    m_minTempLabel->setStyleSheet(labelStyle);
    m_centerTempLabel->setStyleSheet(labelStyle);
    
    // 设置属性
    m_maxTempLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_minTempLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_centerTempLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    
    // 设置大小策略
    m_maxTempLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_minTempLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_centerTempLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    // 初始隐藏
    safeHideLabels();
}

void IRImageDisplay::setImage(const QImage& image, const std::vector<uint16_t>& tempData, 
                             uint32_t tempWidth, uint32_t tempHeight)
{
    if (m_isDestroying || !m_isActive.load()) {
        return;
    }
    
    QMutexLocker locker(&m_dataMutex);
    
    // 验证输入数据
    if (image.isNull() || tempData.empty() || tempWidth == 0 || tempHeight == 0) {
        qWarning() << "IRImageDisplay::setImage - Invalid input data";
        return;
    }
    
    if (tempData.size() != static_cast<size_t>(tempWidth * tempHeight)) {
        qWarning() << "IRImageDisplay::setImage - Temperature data size mismatch";
        return;
    }
    
    // 更新数据
    m_currentImage = image;
    m_tempData = tempData;
    m_tempWidth = tempWidth;
    m_tempHeight = tempHeight;
    
    // 启动定时器更新显示
    if (!m_updateTimer->isActive()) {
        m_updateTimer->start();
    }
    
    // 更新温度标签
    QMetaObject::invokeMethod(this, "updateTempLabels", Qt::QueuedConnection);
}

void IRImageDisplay::clear()
{
    if (m_isDestroying) {
        return;
    }
    
    qDebug() << "IRImageDisplay::clear called";
    
    // 停止定时器
    safeStopTimer();
    
    // 清理数据
    {
        QMutexLocker locker(&m_dataMutex);
        m_currentImage = QImage();
        m_tempData.clear();
        m_tempWidth = 0;
        m_tempHeight = 0;
    }
    
    // 清理显示
    if (m_displayLabel) {
        m_displayLabel->clear();
        m_displayLabel->setText("No Image");
    }
    
    // 隐藏温度标签
    safeHideLabels();
    
    // 更新显示
    update();
    
    qDebug() << "IRImageDisplay cleared successfully";
}

void IRImageDisplay::setMarkMaxTemp(bool isMark)
{
    m_markMaxTemp.store(isMark);
}

void IRImageDisplay::safeStopTimer()
{
    if (m_updateTimer && m_updateTimer->isActive()) {
        m_updateTimer->stop();
        qDebug() << "Update timer stopped";
    }
}

void IRImageDisplay::safeHideLabels()
{
    if (m_maxTempLabel) m_maxTempLabel->hide();
    if (m_minTempLabel) m_minTempLabel->hide();
    if (m_centerTempLabel) m_centerTempLabel->hide();
}

void IRImageDisplay::updateDisplay()
{
    if (m_isDestroying || !m_isActive.load()) {
        safeStopTimer();
        return;
    }
    
    // 触发重绘
    update();
}

void IRImageDisplay::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    
    if (m_isDestroying || !m_isActive.load()) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    QMutexLocker locker(&m_dataMutex);
    
    if (m_currentImage.isNull()) {
        // 绘制无图像状态
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "No Image Available");
        return;
    }
    
    // 计算显示区域
    QSize displaySize = size();
    QPixmap pixmap = QPixmap::fromImage(m_currentImage).scaled(
        displaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    if (pixmap.isNull()) {
        return;
    }
    
    // 计算居中位置
    QRect drawRect((displaySize.width() - pixmap.width()) / 2,
                   (displaySize.height() - pixmap.height()) / 2,
                   pixmap.width(), pixmap.height());
    
    // 绘制图像
    painter.drawPixmap(drawRect, pixmap);
    
    // 绘制温度覆盖层
    drawTemperatureOverlay(painter, drawRect.size());
    
    // 如果鼠标在窗口内，绘制鼠标位置温度
    if (m_mouseOverWidget && drawRect.contains(m_lastMousePos)) {
        QPoint relativePos = m_lastMousePos - drawRect.topLeft();
        drawMouseTemperature(painter, relativePos, drawRect.size());
    }
    
    // 绘制最高温度标记
    if (m_markMaxTemp.load()) {
        drawMaxTempMarker(painter, drawRect.size());
    }
}

void IRImageDisplay::drawTemperatureOverlay(QPainter& painter, const QSize& displaySize)
{
    // 这个方法用于绘制温度相关的覆盖层
    // 目前留空，可以根据需要添加热图等功能
}

void IRImageDisplay::drawMouseTemperature(QPainter& painter, const QPoint& mousePos, const QSize& displaySize)
{
    if (m_tempData.empty() || displaySize.isEmpty()) {
        return;
    }
    
    // 映射到温度坐标
    QPoint tempCoords = mapToTempCoords(mousePos, displaySize);
    
    if (tempCoords.x() < 0 || tempCoords.x() >= static_cast<int>(m_tempWidth) ||
        tempCoords.y() < 0 || tempCoords.y() >= static_cast<int>(m_tempHeight)) {
        return;
    }
    
    // 获取温度值
    float temperature = getTemperatureAt(tempCoords.x(), tempCoords.y());
    QString tempText = QString::number(temperature, 'f', 1) + "°C";
    
    // 计算文本大小
    QFontMetrics fm(painter.font());
    QRect textRect = fm.boundingRect(tempText);
    textRect.adjust(-5, -3, 5, 3);
    
    // 计算显示位置
    QPoint displayPos = mousePos + QPoint(15, -textRect.height() - 5);
    
    // 确保不超出边界
    if (displayPos.x() + textRect.width() > displaySize.width()) {
        displayPos.setX(mousePos.x() - textRect.width() - 15);
    }
    if (displayPos.y() < 0) {
        displayPos.setY(mousePos.y() + 20);
    }
    
    textRect.moveTopLeft(displayPos);
    
    // 绘制背景
    painter.fillRect(textRect, QColor(0, 0, 0, 180));
    
    // 绘制文本
    painter.setPen(Qt::green);
    painter.drawText(textRect, Qt::AlignCenter, tempText);
}

void IRImageDisplay::drawMaxTempMarker(QPainter& painter, const QSize& displaySize)
{
    if (m_tempData.empty()) {
        return;
    }
    
    // 找到最高温度点
    auto maxIt = std::max_element(m_tempData.begin(), m_tempData.end());
    if (maxIt == m_tempData.end()) {
        return;
    }
    
    int maxIndex = std::distance(m_tempData.begin(), maxIt);
    int maxY = maxIndex / m_tempWidth;
    int maxX = maxIndex % m_tempWidth;
    
    // 映射到显示坐标
    float scaleX = static_cast<float>(displaySize.width()) / m_tempWidth;
    float scaleY = static_cast<float>(displaySize.height()) / m_tempHeight;
    
    QPoint displayPos(static_cast<int>(maxX * scaleX), static_cast<int>(maxY * scaleY));
    
    // 绘制十字光标
    const int crossSize = 12;
    painter.setPen(QPen(Qt::red, 2));
    painter.drawLine(displayPos.x() - crossSize, displayPos.y(), 
                     displayPos.x() + crossSize, displayPos.y());
    painter.drawLine(displayPos.x(), displayPos.y() - crossSize, 
                     displayPos.x(), displayPos.y() + crossSize);
    
    // 绘制温度值
    float maxTemp = static_cast<float>(*maxIt) / TEMP_SCALE_FACTOR;
    QString tempText = QString::number(maxTemp, 'f', 1) + "°C";
    
    QFontMetrics fm(painter.font());
    QRect textRect = fm.boundingRect(tempText);
    textRect.adjust(-3, -2, 3, 2);
    textRect.moveCenter(displayPos + QPoint(0, -crossSize - 10));
    
    painter.fillRect(textRect, QColor(0, 0, 0, 180));
    painter.setPen(Qt::red);
    painter.drawText(textRect, Qt::AlignCenter, tempText);
}

QPoint IRImageDisplay::mapToTempCoords(const QPoint& displayPos, const QSize& displaySize) const
{
    if (displaySize.isEmpty() || m_tempWidth == 0 || m_tempHeight == 0) {
        return QPoint(-1, -1);
    }
    
    float scaleX = static_cast<float>(m_tempWidth) / displaySize.width();
    float scaleY = static_cast<float>(m_tempHeight) / displaySize.height();
    
    int tempX = static_cast<int>(displayPos.x() * scaleX);
    int tempY = static_cast<int>(displayPos.y() * scaleY);
    
    return QPoint(tempX, tempY);
}

float IRImageDisplay::getTemperatureAt(int x, int y) const
{
    if (x < 0 || x >= static_cast<int>(m_tempWidth) || 
        y < 0 || y >= static_cast<int>(m_tempHeight) || 
        m_tempData.empty()) {
        return 0.0f;
    }
    
    size_t index = y * m_tempWidth + x;
    if (index >= m_tempData.size()) {
        return 0.0f;
    }
    
    return static_cast<float>(m_tempData[index]) / TEMP_SCALE_FACTOR;
}

void IRImageDisplay::updateTempLabels()
{
    if (m_isDestroying || !m_isActive.load()) {
        return;
    }
    
    QMutexLocker locker(&m_dataMutex);
    
    if (m_tempData.empty() || m_tempWidth == 0 || m_tempHeight == 0) {
        safeHideLabels();
        return;
    }
    
    // 计算温度统计
    auto minMaxIt = std::minmax_element(m_tempData.begin(), m_tempData.end());
    float maxTemp = static_cast<float>(*minMaxIt.second) / TEMP_SCALE_FACTOR;
    float minTemp = static_cast<float>(*minMaxIt.first) / TEMP_SCALE_FACTOR;
    
    // 计算中心温度
    int centerX = m_tempWidth / 2;
    int centerY = m_tempHeight / 2;
    float centerTemp = getTemperatureAt(centerX, centerY);
    
    // 更新标签
    if (m_maxTempLabel) {
        m_maxTempLabel->setText(tr("最高: %1°C").arg(QString::number(maxTemp, 'f', 1)));
        m_maxTempLabel->adjustSize();
        m_maxTempLabel->move(MARGIN_LEFT, MARGIN_TOP);
        m_maxTempLabel->show();
    }
    
    if (m_minTempLabel) {
        m_minTempLabel->setText(tr("最低: %1°C").arg(QString::number(minTemp, 'f', 1)));
        m_minTempLabel->adjustSize();
        m_minTempLabel->move(MARGIN_LEFT, MARGIN_TOP + LABEL_SPACING);
        m_minTempLabel->show();
    }
    
    if (m_centerTempLabel) {
        m_centerTempLabel->setText(tr("中心: %1°C").arg(QString::number(centerTemp, 'f', 1)));
        m_centerTempLabel->adjustSize();
        m_centerTempLabel->move(MARGIN_LEFT, MARGIN_TOP + 2 * LABEL_SPACING);
        m_centerTempLabel->show();
    }
}

void IRImageDisplay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDestroying || !m_isActive.load()) {
        return;
    }
    
    m_lastMousePos = event->pos();
    m_mouseOverWidget = true;
    
    // 触发重绘以显示鼠标位置温度
    update();
    
    QWidget::mouseMoveEvent(event);
}

void IRImageDisplay::leaveEvent(QEvent *event)
{
    m_mouseOverWidget = false;
    update(); // 清除鼠标温度显示
    
    QWidget::leaveEvent(event);
}

void IRImageDisplay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    if (m_isDestroying) {
        return;
    }
    
    // 更新标签位置
    if (m_maxTempLabel) {
        m_maxTempLabel->move(MARGIN_LEFT, MARGIN_TOP);
    }
    if (m_minTempLabel) {
        m_minTempLabel->move(MARGIN_LEFT, MARGIN_TOP + LABEL_SPACING);
    }
    if (m_centerTempLabel) {
        m_centerTempLabel->move(MARGIN_LEFT, MARGIN_TOP + 2 * LABEL_SPACING);
    }
}
