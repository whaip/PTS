#ifndef UESTCQCUSTOMPLOT_H
#define UESTCQCUSTOMPLOT_H

#include "qcustomplot.h"
#include <QOpenGLWidget>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QScrollBar>
#include <QMap>
#include <QLabel>
#include <random>
#include <algorithm>
#include <QFuture>
#include <QFutureWatcher>
#include <QDateTime>
#include <queue>
#include <QPair>

class UESTCQCustomPlot : public QCustomPlot
{
    Q_OBJECT
public:
    // 游标结构体
    struct Cursor {
        QCPItemTracer* tracer;     // 游标点
        QCPItemText* label;        // 坐标标签
        QCPGraph* graph;           // 关联的图形
    };

    explicit UESTCQCustomPlot(QWidget *parent = nullptr);
    ~UESTCQCustomPlot();

    // 移除文件加载相关方法，只保留实时数据相关方法
    void setHorizontalScrollBar(QScrollBar* scrollBar);
    void replot(QCustomPlot::RefreshPriority refreshPriority = QCustomPlot::rpQueuedReplot);

    QCPGraph* addRealTimeLine(const QString& name);
    void updateRealTimeLines(QVector<QCPGraph*> graphs, QVector<QVector<double>> data);
    void removeAllLines(); 

private slots:
    void onXRangeChanged(const QCPRange &range);
    void onScrollBarValueChanged(int value);
    void onMouseDoubleClick(QMouseEvent* event);
    void onLegendClick(QCPLegend* legend, QCPAbstractLegendItem* item, QMouseEvent* event);

protected:
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void setDataRange(double lower, double upper);
    void updateScrollBar();
    void addCursor(QCPGraph* graph, const QColor& color);
    void removeCursor(QCPGraph* graph);
    void updateCursors(QMouseEvent* event);
    void initColorPool();
    QColor getNextColor();
    void createLegendToggleButton();
    void toggleLegend();
    void updateLegendToggleButtonPosition();

private:
    QScrollBar* scrollBar;
    double maxX;
    double minX;
    QMap<QCPGraph*, Cursor> cursors;
    QVector<QColor> colorPool;
    int currentColorIndex;

    QCPItemText* legendToggleButton;
    bool isLegendExpanded;
};

#endif // UESTCQCUSTOMPLOT_H
