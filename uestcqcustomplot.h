#ifndef UESTCQCUSTOMPLOT_H
#define UESTCQCUSTOMPLOT_H

#include "qcustomplot.h"
#include <QOpenGLWidget>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QScrollBar>
#include <QMap>
#include "DataManager.h"
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

    QCPGraph* addLine(const QString& filePath);
    void removeLine(std::vector<QCPGraph*> graphs);
    void setHorizontalScrollBar(QScrollBar* scrollBar);
    void replot(QCustomPlot::RefreshPriority refreshPriority = QCustomPlot::rpQueuedReplot);

    QCPGraph* addRealTimeLine(const QString& name);
    void updateRealTimeLines(QVector<QCPGraph*> graphs, QVector<QList<double>> data);

private slots:
    void onXRangeChanged(const QCPRange &range);
    void onScrollBarValueChanged(int value);
    void onMouseDoubleClick(QMouseEvent* event);
    void onLegendClick(QCPLegend* legend, QCPAbstractLegendItem* item, QMouseEvent* event);
    void processQueuedUpdates();

protected:
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void setDataRange(double lower, double upper);
    void updateScrollBar();
    void updateMaxRange();
    void updateGraphData(QCPGraph* graph);
    void updateAllGraphsData();
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
    static const int MINIMUM_POINTS = 1000;
    QMap<QCPGraph*, DataManager*> dataManagers;
    QLabel* coordLabel;
    QMap<QCPGraph*, Cursor> cursors;
    QVector<QColor> colorPool;
    int currentColorIndex;

    QThreadPool threadPool;
    QMap<QCPGraph*, QFutureWatcher<DataManager::DataBlock>*> dataWatchers;
    bool isUpdating;
    QTimer* updateTimer;

    QCPItemText* legendToggleButton;
    bool isLegendExpanded;
    int lastIndex = 0;

    QMutex dataAccessMutex;
};

#endif // UESTCQCUSTOMPLOT_H
