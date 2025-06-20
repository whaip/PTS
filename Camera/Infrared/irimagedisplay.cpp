#include "irimagedisplay.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QSizePolicy>
#include <QWidget>
#include <QApplication>
#include <QCursor>
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
    , tempWidth(0)
    , tempHeight(0)
    , displayLabel(nullptr)
    , maxTempLabel(nullptr)
    , minTempLabel(nullptr)
    , centerTempLabel(nullptr)
    , isMarkMaxTemp(false)
{
    initializeUI();
    setupLabels();
    
    // 设置向后兼容指针
    displayLabel = m_displayLabel.get();
    maxTempLabel = m_maxTempLabel.get();
    minTempLabel = m_minTempLabel.get();
    centerTempLabel = m_centerTempLabel.get();
    
    // 创建更新定时器
    m_updateTimer = std::make_unique<QTimer>(this);
    connect(m_updateTimer.get(), &QTimer::timeout, this, &IRImageDisplay::updateDisplay);
    
    // 设置鼠标跟踪
    setMouseTracking(true);
    
    qDebug() << "IRImageDisplay initialized successfully";
}

IRImageDisplay::~IRImageDisplay()
{
    qDebug() << "IRImageDisplay destructor called";
    
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
    
    // 隐藏标签
    safeHideLabels();
    
    qDebug() << "IRImageDisplay destructor completed";
}

void IRImageDisplay::initializeUI()
{
    // 创建主显示标签
    m_displayLabel = std::make_unique<QLabel>(this);
    m_displayLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_displayLabel->setAlignment(Qt::AlignCenter);
    m_displayLabel->setStyleSheet("QLabel { border: 1px solid gray; background-color: black; }");
    
    // 设置布局
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_displayLabel.get());
    setLayout(layout);
    
    // 设置窗口属性
    setWindowTitle("IR Image Display");
    setMinimumSize(640, 480);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void IRImageDisplay::setupLabels()
{
    QString labelStyle = "QLabel { "
                        "background-color: rgba(0, 0, 0, 200); "
                        "color: white; "
                        "padding: 5px; "
                        "border-radius: 3px; "
                        "font-weight: bold; "
                        "}";
    
    // 创建温度标签
    m_maxTempLabel = std::make_unique<QLabel>(this);
    m_minTempLabel = std::make_unique<QLabel>(this);
    m_centerTempLabel = std::make_unique<QLabel>(this);
    
    // 设置样式
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
    
    // 初始状态隐藏
    m_maxTempLabel->hide();
    m_minTempLabel->hide();
    m_centerTempLabel->hide();
}

void IRImageDisplay::GetMousePosition(int &tempX, int &tempY, int &mouseX, int &mouseY) {
    // 检查指针有效性
    if (!m_displayLabel || m_isDestroying) {
        tempX = tempY = mouseX = mouseY = 0;
        return;
    }
    
    QPoint pos = QCursor::pos(); // 获取鼠标的全局位置
    if (m_displayLabel->underMouse()) {
        QPoint labelPos = m_displayLabel->mapFromGlobal(pos); // 获取鼠标在label_image中的相对位置
        mouseX = static_cast<int>(labelPos.x());
        mouseY = static_cast<int>(labelPos.y());
        // 获取label_image中显示的图片
        QPixmap pixmap = m_displayLabel->pixmap();
        if (!pixmap.isNull()) {
            // 获取图片的尺寸
            QSize imageSize = pixmap.size();
            QSize labelSize = m_displayLabel->size();

            // 计算缩放比例
            qreal scaleX = labelPos.x() / static_cast<qreal>(labelSize.width());
            qreal scaleY = labelPos.y() / static_cast<qreal>(labelSize.height());

            // 计算相对坐标
            QMutexLocker locker(&m_dataMutex);
            tempX = static_cast<int>(m_tempWidth * scaleX);
            tempY = static_cast<int>(m_tempHeight * scaleY);
        }
    } else {
        tempX = tempY = mouseX = mouseY = 0;
    }
}

void IRImageDisplay::setImage(const QImage& image, const std::vector<uint16_t>& tempData, uint32_t tempWidth, uint32_t tempHeight) {
    if (m_isDestroying || !m_isActive.load()) {
        return;
    }
    
    if (image.isNull() || tempData.empty() || tempWidth == 0 || tempHeight == 0) {
        qWarning() << "IRImageDisplay::setImage: Invalid input data";
        return;
    }
    
    // 线程安全地更新数据
    {
        QMutexLocker locker(&m_dataMutex);
        m_currentImage = image.copy();
        m_tempData = tempData;
        m_tempWidth = tempWidth;
        m_tempHeight = tempHeight;
        
        // 兼容旧代码
        this->image = m_currentImage;
        this->tempData = m_tempData;
        this->tempWidth = m_tempWidth;
        this->tempHeight = m_tempHeight;
    }
    
    // 更新温度标签
    updateTempLabels();
    
    // 启动定时器进行显示更新
    if (!m_updateTimer->isActive()) {
        m_updateTimer->start(UPDATE_INTERVAL_MS);
    }
    
    // 立即更新显示
    update();
}

float IRImageDisplay::getTemperatureAt(int x, int y) const {
    QMutexLocker locker(&m_dataMutex);
    if (x >= 0 && x < static_cast<int>(m_tempWidth) && y >= 0 && y < static_cast<int>(m_tempHeight) && !m_tempData.empty()) {
        return static_cast<float>(m_tempData[y * m_tempWidth + x]) / TEMP_SCALE_FACTOR;
    }
    return 0.0f;
}

// 保持向后兼容性
float IRImageDisplay::getTemp(int x, int y) {
    return getTemperatureAt(x, y);
}

void IRImageDisplay::updateTempLabels() {
    if (m_isDestroying || !m_isActive.load()) {
        return;
    }
    
    // 检查UI组件有效性
    if (!m_maxTempLabel || !m_minTempLabel || !m_centerTempLabel) {
        return;
    }
    
    QMutexLocker locker(&m_dataMutex);
    if (m_tempData.empty() || m_tempWidth == 0 || m_tempHeight == 0) {
        locker.unlock();
        safeHideLabels();
        return;
    }
    
    // 查找最高和最低温度
    float maxTemp = -273.15f;
    float minTemp = 1000.0f;
    for (const auto& temp : m_tempData) {
        float currentTemp = static_cast<float>(temp) / TEMP_SCALE_FACTOR;
        maxTemp = std::max(maxTemp, currentTemp);
        minTemp = std::min(minTemp, currentTemp);
    }
    
    // 计算中心温度
    int centerX = m_tempWidth / 2;
    int centerY = m_tempHeight / 2;
    float centerTemp = 0.0f;
    if (centerX >= 0 && centerX < static_cast<int>(m_tempWidth) && 
        centerY >= 0 && centerY < static_cast<int>(m_tempHeight)) {
        centerTemp = static_cast<float>(m_tempData[centerY * m_tempWidth + centerX]) / TEMP_SCALE_FACTOR;
    }
    
    locker.unlock();
    
    // 更新标签文本
    m_maxTempLabel->setText(tr("最高温度: %1 °C").arg(QString::number(maxTemp, 'f', 1)));
    m_minTempLabel->setText(tr("最低温度: %1 °C").arg(QString::number(minTemp, 'f', 1)));
    m_centerTempLabel->setText(tr("中心温度: %1 °C").arg(QString::number(centerTemp, 'f', 1)));
    
    // 调整标签大小以适应内容
    m_maxTempLabel->adjustSize();
    m_minTempLabel->adjustSize();
    m_centerTempLabel->adjustSize();
    
    // 更新标签位置
    m_maxTempLabel->move(MARGIN_LEFT, MARGIN_TOP);
    m_minTempLabel->move(MARGIN_LEFT, MARGIN_TOP + LABEL_SPACING);
    m_centerTempLabel->move(MARGIN_LEFT, MARGIN_TOP + 2 * LABEL_SPACING);
    
    // 显示标签
    m_maxTempLabel->show();
    m_minTempLabel->show();
    m_centerTempLabel->show();
}

void IRImageDisplay::updateDisplay() {
    if (m_isDestroying || !m_isActive.load() || !m_displayLabel) {
        return;
    }
    
    QMutexLocker locker(&m_dataMutex);
    if (m_currentImage.isNull()) {
        return;
    }
    
    QImage imageCopy = m_currentImage.copy();
    locker.unlock();
    
    QSize labelSize = m_displayLabel->size();
    if (labelSize.width() <= 0 || labelSize.height() <= 0) {
        return;
    }
    
    QPixmap pixmap = QPixmap::fromImage(imageCopy).scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    if (!pixmap.isNull()) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        
        // 绘制最高温度标记
        if (m_markMaxTemp.load()) {
            drawMaxTempMarker(painter, pixmap.size());
        }
        
        // 绘制鼠标位置温度
        if (m_mouseOverWidget) {
            drawMouseTemperature(painter, m_lastMousePos, pixmap.size());
        }
        
        m_displayLabel->setPixmap(pixmap);
    }
}

void IRImageDisplay::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    // 主要绘制工作由updateDisplay()处理
}

void IRImageDisplay::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDestroying || !m_isActive.load()) {
        return;
    }
    
    m_lastMousePos = event->pos();
    m_mouseOverWidget = true;
    
    // 立即更新显示
    updateDisplay();
    
    QWidget::mouseMoveEvent(event);
}

void IRImageDisplay::leaveEvent(QEvent *event) {
    m_mouseOverWidget = false;
    // 更新显示以移除鼠标位置的温度显示
    updateDisplay();
    QWidget::leaveEvent(event);
}

// Helper methods implementation
void IRImageDisplay::safeStopTimer() {
    if (m_updateTimer && m_updateTimer->isActive()) {
        m_updateTimer->stop();
    }
    if (timer.isActive()) {
        timer.stop();
    }
}

void IRImageDisplay::safeHideLabels() {
    if (m_maxTempLabel) {
        m_maxTempLabel->hide();
    }
    if (m_minTempLabel) {
        m_minTempLabel->hide();
    }
    if (m_centerTempLabel) {
        m_centerTempLabel->hide();
    }
}

QPoint IRImageDisplay::mapToTempCoords(const QPoint& displayPos, const QSize& displaySize) const {
    QMutexLocker locker(&m_dataMutex);
    if (m_tempWidth == 0 || m_tempHeight == 0 || displaySize.width() == 0 || displaySize.height() == 0) {
        return QPoint(0, 0);
    }
    
    float scaleX = static_cast<float>(m_tempWidth) / displaySize.width();
    float scaleY = static_cast<float>(m_tempHeight) / displaySize.height();
    
    int tempX = static_cast<int>(displayPos.x() * scaleX);
    int tempY = static_cast<int>(displayPos.y() * scaleY);
    
    // 确保坐标在有效范围内
    tempX = qBound(0, tempX, static_cast<int>(m_tempWidth - 1));
    tempY = qBound(0, tempY, static_cast<int>(m_tempHeight - 1));
    
    return QPoint(tempX, tempY);
}

void IRImageDisplay::drawMaxTempMarker(QPainter& painter, const QSize& displaySize) {
    QMutexLocker locker(&m_dataMutex);
    if (m_tempData.empty() || m_tempWidth == 0 || m_tempHeight == 0) {
        return;
    }
    
    // 找到最高温度点
    auto maxTempIt = std::max_element(m_tempData.begin(), m_tempData.end());
    int maxTempIndex = std::distance(m_tempData.begin(), maxTempIt);
    int maxTempY = maxTempIndex / m_tempWidth;
    int maxTempX = maxTempIndex % m_tempWidth;
    
    // 计算在显示图像上的坐标
    float scaleX = static_cast<float>(displaySize.width()) / m_tempWidth;
    float scaleY = static_cast<float>(displaySize.height()) / m_tempHeight;
    
    int displayX = static_cast<int>(maxTempX * scaleX);
    int displayY = static_cast<int>(maxTempY * scaleY);
    
    // 确保十字光标在可见范围内
    const int crossSize = 10;
    const int margin = 5;
    
    // 限制十字光标位置在显示区域内
    displayX = qBound(crossSize + margin, displayX, displaySize.width() - crossSize - margin);
    displayY = qBound(crossSize + margin, displayY, displaySize.height() - crossSize - margin);
    
    // 绘制十字光标
    painter.setPen(QPen(Qt::red, 2));
    painter.drawLine(displayX - crossSize, displayY, displayX + crossSize, displayY);
    painter.drawLine(displayX, displayY - crossSize, displayX, displayY + crossSize);
    
    // 绘制温度值
    float maxTemp = static_cast<float>(*maxTempIt) / TEMP_SCALE_FACTOR;
    QString tempText = QString::number(maxTemp, 'f', 1) + "°C";
    
    // 计算文本位置
    QFontMetrics fm(painter.font());
    int textWidth = fm.horizontalAdvance(tempText);
    int textHeight = fm.height();
    
    // 智能计算文本位置，确保完全可见
    int textX = displayX + 10; // 默认在十字光标右侧
    int textY = displayY - 5;   // 默认在十字光标上方
    
    // 检查右边界
    if (textX + textWidth + 10 > displaySize.width()) {
        textX = displayX - textWidth - 10; // 移到左侧
    }
    
    // 检查左边界
    if (textX < 5) {
        textX = 5;
    }
    
    // 检查上边界
    if (textY - textHeight - 5 < 5) {
        textY = displayY + textHeight + 15; // 移到下方
    }
    
    // 检查下边界
    if (textY + 5 > displaySize.height()) {
        textY = displaySize.height() - 10;
    }
    
    // 绘制背景矩形
    QRect textRect(textX - 5, textY - textHeight - 5, textWidth + 10, textHeight + 10);
    painter.fillRect(textRect, QColor(0, 0, 0, 128));
    
    // 绘制温度文本
    painter.setPen(Qt::red);
    painter.drawText(textX, textY, tempText);
}

void IRImageDisplay::drawMouseTemperature(QPainter& painter, const QPoint& mousePos, const QSize& displaySize) {
    if (!m_displayLabel || !m_displayLabel->underMouse()) {
        return;
    }
    
    QPoint tempCoords = mapToTempCoords(mousePos, displaySize);
    float temp = getTemperatureAt(tempCoords.x(), tempCoords.y());
    
    if (temp == 0.0f) {
        return; // Invalid temperature
    }
    
    QString tempText = QString::number(temp, 'f', 2) + " °C";
    
    // 计算温度显示的矩形区域
    const int rectWidth = 80;
    const int rectHeight = 20;
    const int xOffset = 10; // 向右偏移量
    const int MARGIN = 10;  // 边缘安全距离
    
    // 默认显示在鼠标右侧
    int rectX = mousePos.x() + xOffset;
    int rectY = mousePos.y() - rectHeight/2;
    
    // 检查是否超出右边界
    if (rectX + rectWidth + MARGIN > displaySize.width()) {
        // 如果超出右边界，显示在鼠标左侧
        rectX = mousePos.x() - rectWidth - xOffset;
    }

    // 检查是否与标签重叠
    QRect tempRect(rectX, rectY, rectWidth, rectHeight);
    QRect maxLabelRect(MARGIN_LEFT, MARGIN_TOP, 150, 25);
    QRect minLabelRect(MARGIN_LEFT, MARGIN_TOP + LABEL_SPACING, 150, 25);
    QRect centerLabelRect(MARGIN_LEFT, MARGIN_TOP + 2 * LABEL_SPACING, 150, 25);
    
    if (tempRect.intersects(maxLabelRect) || 
        tempRect.intersects(minLabelRect) || 
        tempRect.intersects(centerLabelRect)) {
        // 如果与标签重叠，显示在最下面一个标签的下方
        rectY = MARGIN_TOP + 3 * LABEL_SPACING + 5; // 最后一个标签下方5像素
        rectX = MARGIN_LEFT; // 与标签对齐
    }
    
    // 绘制半透明背景
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 128));
    painter.drawRect(QRect(rectX, rectY, rectWidth, rectHeight));
    
    // 绘制温度文本
    painter.setPen(QPen(Qt::green, 2));
    painter.drawText(QPoint(rectX + 5, rectY + rectHeight - 5), tempText);
}

void IRImageDisplay::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    
    if (m_isDestroying || !m_isActive.load()) {
        return;
    }
    
    // 检查新的标签是否有效
    if (!m_maxTempLabel || !m_minTempLabel || !m_centerTempLabel) {
        return;
    }
    
    QSize size = event->size();
    int w = size.width();
    int h = size.height();
    
    if (w * 288 > h * 384) {
        w = h * 384 / 288;
    } else {
        h = w * 288 / 384;
    }
    
    // 更新标签位置到左上角
    m_maxTempLabel->move(MARGIN_LEFT, MARGIN_TOP);
    m_minTempLabel->move(MARGIN_LEFT, MARGIN_TOP + LABEL_SPACING);
    m_centerTempLabel->move(MARGIN_LEFT, MARGIN_TOP + 2 * LABEL_SPACING);
    
    QWidget::resize(w, h);
}

void IRImageDisplay::clear() {
    if (m_isDestroying) {
        return;
    }
    
    m_isActive.store(false);
    
    // 停止定时器，防止在清理过程中继续更新
    safeStopTimer();
    
    // 线程安全地清理图像数据
    {
        QMutexLocker locker(&m_dataMutex);
        m_currentImage = QImage();
        m_tempData.clear();
        m_tempWidth = 0;
        m_tempHeight = 0;
        
        // 兼容旧代码
        this->image = QImage();
        this->tempData.clear();
        this->tempWidth = 0;
        this->tempHeight = 0;
    }
    
    // 清理UI显示
    if (m_displayLabel) {
        m_displayLabel->clear();
    }
    
    safeHideLabels();
    
    // 强制更新UI
    update();
    
    m_isActive.store(true);
}

void IRImageDisplay::setMarkMaxTemp(bool isMark) {
    m_markMaxTemp.store(isMark);
    
    // 兼容旧代码
    isMarkMaxTemp = isMark;
    
    // 立即更新显示
    if (m_isActive.load()) {
        updateDisplay();
    }
}

// 保持向后兼容性
void IRImageDisplay::MarkMaxTemp(bool isMark) {
    setMarkMaxTemp(isMark);
}

// Legacy method - replaced by updateDisplay()
void IRImageDisplay::Imageupdate() {
    updateDisplay();
}
