#ifndef IRIMAGEDISPLAY_H
#define IRIMAGEDISPLAY_H

#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QString>
#include <QLabel>
#include <QPoint>
#include <QSize>
#include <QPixmap>
#include <QTimer>
#include <QResizeEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QMouseEvent>
#include <QPaintEvent>
#include <memory>
#include <atomic>

class IRImageDisplay : public QWidget
{
    Q_OBJECT
    
public:
    explicit IRImageDisplay(QWidget *parent = nullptr);
    ~IRImageDisplay();
      // 公共接口
    void setImage(const QImage& image, const std::vector<uint16_t>& tempData, uint32_t tempWidth, uint32_t tempHeight);
    void clear();
    void setMarkMaxTemp(bool isMark = true);
    bool isActive() const { return m_isActive.load(); }
    
    // Legacy methods for backward compatibility - deprecated
    void GetMousePosition(int &tempX, int &tempY, int &mouseX, int &mouseY);
    float getTemp(int x, int y);
    void Imageupdate();
    void MarkMaxTemp(bool isMark);
    
protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void updateDisplay();

private:
    // 内部方法
    void initializeUI();
    void setupLabels();
    void updateTempLabels();
    void drawTemperatureOverlay(QPainter& painter, const QSize& displaySize);
    void drawMouseTemperature(QPainter& painter, const QPoint& mousePos, const QSize& displaySize);
    void drawMaxTempMarker(QPainter& painter, const QSize& displaySize);
    QPoint mapToTempCoords(const QPoint& displayPos, const QSize& displaySize) const;
    float getTemperatureAt(int x, int y) const;
    void safeHideLabels();
    void safeStopTimer();
    
    // 数据成员 - 使用mutex保护
    mutable QMutex m_dataMutex;
    QImage m_currentImage;
    std::vector<uint16_t> m_tempData;
    uint32_t m_tempWidth;
    uint32_t m_tempHeight;
    
    // UI组件 - 使用智能指针管理
    std::unique_ptr<QLabel> m_displayLabel;
    std::unique_ptr<QLabel> m_maxTempLabel;
    std::unique_ptr<QLabel> m_minTempLabel;
    std::unique_ptr<QLabel> m_centerTempLabel;
    std::unique_ptr<QTimer> m_updateTimer;
    
    // 状态变量 - 使用原子变量保证线程安全
    std::atomic<bool> m_isActive;
    std::atomic<bool> m_markMaxTemp;
    QPoint m_lastMousePos;
    bool m_mouseOverWidget;
    bool m_isDestroying;
    
    // 常量
    static const int UPDATE_INTERVAL_MS = 33; // ~30 FPS
    static const int MARGIN_LEFT = 10;
    static const int MARGIN_TOP = 10;
    static const int LABEL_SPACING = 35;
    static const int TEMP_SCALE_FACTOR = 10; // 温度数据缩放因子 (0.1)

    // Legacy members for backward compatibility - deprecated
    QImage image;
    std::vector<uint16_t> tempData;
    uint32_t tempWidth;
    uint32_t tempHeight;
    QLabel* displayLabel;
    QLabel* maxTempLabel;
    QLabel* minTempLabel;
    QLabel* centerTempLabel;
    QTimer timer;
    bool isMarkMaxTemp;
};

#endif // IRIMAGEDISPLAY_H
