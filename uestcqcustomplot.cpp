#include "UESTCQCustomPlot.h"

UESTCQCustomPlot::UESTCQCustomPlot(QWidget *parent)
    : QCustomPlot(parent)
    , scrollBar(nullptr)
    , maxX(0)
    , minX(0)
    , currentColorIndex(0)
    , isUpdating(false)
    , legendToggleButton(nullptr)
    , isLegendExpanded(true)
{
    // 使用软件渲染优化
    setNoAntialiasingOnDrag(true);
    setNotAntialiasedElements(QCP::aeAll);
    setAntialiasedElements(QCP::aePlottables);

    // 其他性能优化设置
    setPlottingHint(QCP::phFastPolylines, true);
    setBufferDevicePixelRatio(1);

    // 配置图例
    legend->setVisible(true);
    legend->setFont(QFont("Helvetica", 9));
    legend->setBrush(QBrush(QColor(255,255,255,230)));
    legend->setSelectableParts(QCPLegend::spItems);

    // 设置图例为多列显示
    legend->setMaximumSize(QSize(QWIDGETSIZE_MAX, 1000));  // 设置最大高度
    legend->setWrap(2);  // 每列最多显示20个项目
    legend->setFillOrder(QCPLegend::foColumnsFirst);  // 按列优先填充

    // 调整图例位置和大小
    axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);

    // 连接信号
    connect(this, &QCustomPlot::legendClick, this, &UESTCQCustomPlot::onLegendClick);
    connect(this, &QCustomPlot::mouseDoubleClick, this, &UESTCQCustomPlot::onMouseDoubleClick);
    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(onXRangeChanged(QCPRange)));
    connect(this, &QCustomPlot::mouseMove, this, &UESTCQCustomPlot::updateCursors);

    // 初始化颜色列表
    initColorPool();

    // 设置交互方式
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    setMouseTracking(true);

    // 设置线程池
    threadPool.setMaxThreadCount(QThread::idealThreadCount());

    // 设置定时器
    updateTimer = new QTimer(this);
    updateTimer->setSingleShot(true);
    connect(updateTimer, &QTimer::timeout, this, &UESTCQCustomPlot::processQueuedUpdates);

    // 性能优化设置
    setOpenGl(true);
    setPlottingHints(QCP::phFastPolylines);
    setAntialiasedElements(QCP::aeNone);
    setNotAntialiasedElements(QCP::aeAll);

    // 使用队列方式重绘
    replot(QCustomPlot::rpQueuedReplot);

    // 创建图例切换按钮
    createLegendToggleButton();
}

UESTCQCustomPlot::~UESTCQCustomPlot()
{
    qDeleteAll(dataManagers);
}

QCPGraph* UESTCQCustomPlot::addLine(const QString& filePath)
{
    // 创建新的数据管理器
    DataManager* manager = new DataManager(this);
    if (!manager->loadFromBinaryFile(filePath)) {
        delete manager;
        return nullptr;
    }

    // 创建新的图形
    QCPGraph* newGraph = addGraph();

    // 分配一个新颜色
    QColor color = getNextColor();
    newGraph->setPen(QPen(color));

    // 设置图例
    QFileInfo fileInfo(filePath);
    newGraph->setName(fileInfo.baseName());

    // 显示图例
    legend->setVisible(true);
    legend->setFont(QFont("Helvetica", 9));
    legend->setBrush(QBrush(QColor(255,255,255,230)));

    // 调整图例布局
    axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);
    replot(QCustomPlot::rpQueuedReplot);

    // 保存数据管理器和图形的关联
    dataManagers[newGraph] = manager;

    // 更新数据范围
    int totalPoints = manager->getTotalSize();

    if (totalPoints > 0) {
        maxX = qMax(maxX, static_cast<double>(totalPoints));
        setDataRange(0, maxX);
    }

    // 加载初始数据
    updateGraphData(newGraph);

    if (newGraph) {
        // 为新曲线添加游标
        addCursor(newGraph, color);
    }

    return newGraph;
}

void UESTCQCustomPlot::removeLine(std::vector<QCPGraph*> graphs)
{
    // 先停止所有更新操作
    isUpdating = true;  // 防止新的更新开始

    // 取消所有正在进行的异步操作，但不等待它们完成
    for (auto it = dataWatchers.begin(); it != dataWatchers.end(); ++it) {
        if (it.value()) {
            disconnect(it.value(), nullptr, this, nullptr);  // 断开所有信号连接
            it.value()->cancel();
        }
    }

    QMutexLocker locker(&dataAccessMutex);

    for (QCPGraph* graph : graphs) {
        if (!graph) continue;

        // 从dataWatchers中移除并清理
        if (dataWatchers.contains(graph)) {
            auto watcher = dataWatchers[graph];
            if (watcher) {
                watcher->cancel();
                watcher->deleteLater();  // 使用deleteLater而不是直接删除
            }
            dataWatchers.remove(graph);
        }

        // 删除数据管理器
        if (dataManagers.contains(graph)) {
            auto manager = dataManagers[graph];
            dataManagers.remove(graph);
            delete manager;  // 数据管理器可以直接删除
        }

        // 删除游标
        if (cursors.contains(graph)) {
            removeCursor(graph);
        }

        // 删除图形
        removeGraph(graph);
    }

    // 更新最大范围
    updateMaxRange();

    // 更新图例布局
    axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);

    isUpdating = false;  // 恢复更新状态

    // 使用定时器延迟重绘，给待取消的操作一些时间
    QTimer::singleShot(100, this, [this]() {
        replot(QCustomPlot::rpQueuedReplot);
    });
}

void UESTCQCustomPlot::setHorizontalScrollBar(QScrollBar* scrollBar)
{
    this->scrollBar = scrollBar;
    if (scrollBar) {
        // 设置初始范围
        scrollBar->setMinimum(0);
        scrollBar->setMaximum(100);
        scrollBar->setPageStep(10);
        scrollBar->setSingleStep(1);

        connect(scrollBar, &QScrollBar::valueChanged, this, &UESTCQCustomPlot::onScrollBarValueChanged);

        // 初始化滚动条状态
        QTimer::singleShot(0, this, [this]() {
            updateScrollBar();
        });
    }
}

void UESTCQCustomPlot::onXRangeChanged(const QCPRange &range)
{
    if (!scrollBar) return;

    updateScrollBar();

    if (range.lower < minX || range.upper > maxX)
    {
        QCPRange boundedRange = range;
        if (boundedRange.lower < minX)
        {
            boundedRange.lower = minX;
            boundedRange.upper = minX + range.size();
            if (boundedRange.upper > maxX)
                boundedRange.upper = maxX;
        }
        else if (boundedRange.upper > maxX)
        {
            boundedRange.upper = maxX;
            boundedRange.lower = maxX - range.size();
            if (boundedRange.lower < minX)
                boundedRange.lower = minX;
        }
        xAxis->setRange(boundedRange);
    }

    // 更新所有图形的数据
    updateAllGraphsData();
}

void UESTCQCustomPlot::onScrollBarValueChanged(int value)
{
    if (!scrollBar) return;

    double newLower = value;
    double newUpper = value + scrollBar->pageStep();

    xAxis->setRange(newLower, newUpper);
    replot(QCustomPlot::rpQueuedReplot);
}

void UESTCQCustomPlot::onMouseDoubleClick(QMouseEvent* event)
{
    Q_UNUSED(event)

    // 恢复到初始视图范围
    if (maxX > 0) {
        setDataRange(0, maxX);
        // 恢复Y轴范围
        yAxis->rescale();
        replot(QCustomPlot::rpQueuedReplot);
    }
}

void UESTCQCustomPlot::onLegendClick(QCPLegend* legend, QCPAbstractLegendItem* item, QMouseEvent* event)
{
    Q_UNUSED(legend)
    Q_UNUSED(event)

    QCPPlottableLegendItem* plItem = qobject_cast<QCPPlottableLegendItem*>(item);
    if (plItem) {
        QCPGraph* graph = qobject_cast<QCPGraph*>(plItem->plottable());
        if (graph) {
            graph->setVisible(!graph->visible());

            if (cursors.contains(graph)) {
                Cursor& cursor = cursors[graph];
                if (!graph->visible()) {
                    cursor.tracer->setVisible(false);
                    cursor.label->setVisible(false);
                }
            }

            plItem->setTextColor(graph->visible() ? Qt::black : Qt::lightGray);
            replot(QCustomPlot::rpQueuedReplot);
        }
    }
}

void UESTCQCustomPlot::processQueuedUpdates()
{
    if (!isUpdating) {
        updateAllGraphsData();
    }
}

void UESTCQCustomPlot::leaveEvent(QEvent *event)
{
    QCustomPlot::leaveEvent(event);
    // 隐藏所有游标
    for (auto& cursor : cursors) {
        cursor.tracer->setVisible(false);
        cursor.label->setVisible(false);
    }
    replot(QCustomPlot::rpQueuedReplot);
}

void UESTCQCustomPlot::wheelEvent(QWheelEvent *event)
{
    QCustomPlot::wheelEvent(event);

    if (!dataManagers.isEmpty()) {
        replot(QCustomPlot::rpQueuedReplot);
    }
}

void UESTCQCustomPlot::setDataRange(double lower, double upper)
{
    minX = lower;
    maxX = upper;
    xAxis->setRange(lower, upper);
    updateScrollBar();
}

void UESTCQCustomPlot::updateScrollBar()
{
    if (!scrollBar) {
        return;
    }

    QCPRange range = xAxis->range();

    try {
        scrollBar->setMinimum(minX);
        scrollBar->setMaximum(qMax(maxX - range.size(), minX));
        scrollBar->setPageStep(range.size());
        scrollBar->setSingleStep(range.size() / 10);
        scrollBar->setValue(range.lower);
    } catch (const std::exception& e) {
        qDebug() << "Error updating scrollbar:" << e.what();
    } catch (...) {
        qDebug() << "Unknown error updating scrollbar";
    }
}

void UESTCQCustomPlot::updateMaxRange()
{
    maxX = 0;
    for (DataManager* manager : dataManagers) {
        maxX = qMax(maxX, static_cast<double>(manager->getTotalSize()));
    }
    setDataRange(0, maxX);
}

void UESTCQCustomPlot::updateGraphData(QCPGraph* graph)
{
    if (isUpdating) return;  // 如果正在删除过程中，不开始新的更新

    QMutexLocker locker(&dataAccessMutex);

    if (!graph || !dataManagers.contains(graph)) {
        return;
    }

    // 如果已经有一个watcher在运行，先取消它
    if (dataWatchers.contains(graph)) {
        auto watcher = dataWatchers[graph];
        if (watcher) {
            disconnect(watcher, nullptr, this, nullptr);  // 断开所有信号连接
            watcher->cancel();
            watcher->deleteLater();
        }
        dataWatchers.remove(graph);
    }

    // 创建新的watcher
    auto watcher = new QFutureWatcher<DataManager::DataBlock>(this);

    // 使用QPointer来追踪graph对象
    QPointer<QCPGraph> graphPtr(graph);
    QPointer<UESTCQCustomPlot> thisPtr(this);  // 也追踪this指针

    connect(watcher, &QFutureWatcher<DataManager::DataBlock>::finished, this,
            [thisPtr, graphPtr, watcher]() {
                if (!thisPtr || !graphPtr || thisPtr->isUpdating) {
                    watcher->deleteLater();
                    return;
                }

                QMutexLocker locker(&thisPtr->dataAccessMutex);

                DataManager::DataBlock data = watcher->result();
                if (!data.x.isEmpty() && !data.y.isEmpty()) {
                    graphPtr->data()->clear();
                    graphPtr->addData(data.x, data.y);
                }

                thisPtr->dataWatchers.remove(graphPtr);
                watcher->deleteLater();

                // 检查是否所有更新都完成
                if (thisPtr->dataWatchers.isEmpty() && !thisPtr->updateTimer->isActive()) {
                    thisPtr->replot(QCustomPlot::rpQueuedReplot);
                }
            });

    // 启动异步加载
    QCPRange range = xAxis->range();
    DataManager* manager = dataManagers[graph];
    QFuture<DataManager::DataBlock> future = QtConcurrent::run(&threadPool,
                                                               [manager, range]() {
                                                                   return manager->loadDataInRange(range.lower, range.upper);
                                                               });

    watcher->setFuture(future);
    dataWatchers[graph] = watcher;
}

void UESTCQCustomPlot::updateAllGraphsData()
{
    // 如果已经在更新中，加入队列
    if (isUpdating) {
        if (!updateTimer->isActive()) {
            updateTimer->start(100); // 100ms 后重试
        }
        return;
    }

    isUpdating = true;

    // 取消所有正在进行的更新
    for (auto watcher : dataWatchers) {
        watcher->cancel();
        delete watcher;
    }
    dataWatchers.clear();

    // 更新所有图形
    for (auto it = dataManagers.begin(); it != dataManagers.end(); ++it) {
        if (it.key()->visible()) {  // 只更新可见的图形
            updateGraphData(it.key());
        }
    }

    isUpdating = false;
}

void UESTCQCustomPlot::addCursor(QCPGraph* graph, const QColor& color)
{
    Cursor cursor;

    // 创建游标点
    cursor.tracer = new QCPItemTracer(this);
    cursor.tracer->setGraph(graph);
    cursor.tracer->setStyle(QCPItemTracer::tsCircle);
    cursor.tracer->setPen(QPen(color));
    cursor.tracer->setBrush(color);
    cursor.tracer->setSize(7);
    cursor.tracer->setVisible(false);

    // 创建标签
    cursor.label = new QCPItemText(this);
    cursor.label->setPositionAlignment(Qt::AlignLeft|Qt::AlignBottom);
    cursor.label->setPadding(QMargins(5, 5, 5, 5));
    cursor.label->setBrush(QBrush(QColor(255, 255, 255, 230)));
    cursor.label->setPen(QPen(color));
    cursor.label->setFont(QFont("Arial", 8));
    cursor.label->setVisible(false);

    cursor.graph = graph;
    cursors[graph] = cursor;
}

void UESTCQCustomPlot::removeCursor(QCPGraph* graph)
{
    if (cursors.contains(graph)) {
        removeItem(cursors[graph].tracer);
        removeItem(cursors[graph].label);
        cursors.remove(graph);
    }
}

void UESTCQCustomPlot::updateCursors(QMouseEvent* event)
{
    // 获取鼠标X坐标对应的数据坐标
    double x = xAxis->pixelToCoord(event->pos().x());

    // 获取绘图区域的边界
    QRect plotRect = axisRect()->rect();
    bool isNearRightEdge = event->pos().x() > plotRect.right() - 150; // 距离右边界150像素

    // 更新每个游标
    for (auto it = cursors.begin(); it != cursors.end(); ++it) {
        QCPGraph* graph = it.key();
        Cursor& cursor = it.value();

        // 如果图形不可见，则隐藏游标
        if (!graph->visible()) {
            cursor.tracer->setVisible(false);
            cursor.label->setVisible(false);
            continue;
        }

        // 找到最近的数据点
        double key = x;
        double value = 0;
        bool found = false;

        // 使用图形的数据接口查找最近点
        QCPGraphDataContainer::const_iterator it_lower = graph->data()->findBegin(x);
        QCPGraphDataContainer::const_iterator it_upper = graph->data()->findEnd(x);

        if (it_lower != graph->data()->end() && it_upper != graph->data()->end()) {
            if (std::abs(it_lower->key - x) < std::abs(it_upper->key - x)) {
                key = it_lower->key;
                value = it_lower->value;
            } else {
                key = it_upper->key;
                value = it_upper->value;
            }
            found = true;
        }

        if (found) {
            // 更新游标位置
            cursor.tracer->setGraphKey(key);
            cursor.tracer->setVisible(true);

            // 更新标签位置和对齐方式
            if (isNearRightEdge) {
                // 靠近右边界时，将标签显示在左侧
                cursor.label->setPositionAlignment(Qt::AlignRight|Qt::AlignBottom);
                cursor.label->position->setParentAnchor(cursor.tracer->position);
                cursor.label->position->setCoords(-10, 10);  // 向左偏移
            } else {
                // 正常情况下显示在右侧
                cursor.label->setPositionAlignment(Qt::AlignLeft|Qt::AlignBottom);
                cursor.label->position->setParentAnchor(cursor.tracer->position);
                cursor.label->position->setCoords(10, 10);
            }

            // 更新标签文本
            cursor.label->setText(QString("(%1, %2)").arg(key, 0, 'f', 0).arg(value, 0, 'f', 4));
            cursor.label->setVisible(true);
        } else {
            cursor.tracer->setVisible(false);
            cursor.label->setVisible(false);
        }
    }

    replot(QCustomPlot::rpQueuedReplot);
}

void UESTCQCustomPlot::initColorPool()
{
    // 创建预定义的颜色池
    colorPool = {
        QColor(31, 119, 180),   // 蓝色
        QColor(255, 127, 14),   // 橙色
        QColor(44, 160, 44),    // 绿色
        QColor(214, 39, 40),    // 红色
        QColor(148, 103, 189),  // 紫色
        QColor(140, 86, 75),    // 棕色
        QColor(227, 119, 194),  // 粉色
        QColor(127, 127, 127),  // 灰色
        QColor(188, 189, 34),   // 黄绿色
        QColor(23, 190, 207)    // 青色
    };

    // 随机打乱颜色顺序
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(colorPool.begin(), colorPool.end(), gen);

    currentColorIndex = 0;
}

QColor UESTCQCustomPlot::getNextColor()
{
    if (colorPool.isEmpty()) {
        initColorPool();  // 如果用完了就重新初始化
    }

    QColor color = colorPool[currentColorIndex];
    currentColorIndex = (currentColorIndex + 1) % colorPool.size();
    return color;
}

void UESTCQCustomPlot::createLegendToggleButton()
{
    legendToggleButton = new QCPItemText(this);

    // 将按钮设置到最上层
    legendToggleButton->setLayer("overlay");

    // 获取坐标轴矩形
    QRect axisRect = this->axisRect()->rect();

    // 计算按钮位置（左上角）
    double buttonX = axisRect.left() + 50;  // 距离左边界50像素
    double buttonY = axisRect.top() + 20;   // 距离顶部20像素

    legendToggleButton->position->setType(QCPItemPosition::ptAbsolute);
    legendToggleButton->position->setCoords(buttonX, buttonY);

    legendToggleButton->setText("隐藏图例 ▼");
    legendToggleButton->setFont(QFont("Arial", 10, QFont::Bold));
    legendToggleButton->setPadding(QMargins(8, 5, 8, 5));
    legendToggleButton->setBrush(QBrush(QColor(255, 255, 255, 230)));
    legendToggleButton->setPen(QPen(Qt::lightGray));

    legendToggleButton->setTextAlignment(Qt::AlignCenter);
    legendToggleButton->setSelectable(false);
}

void UESTCQCustomPlot::toggleLegend()
{
    isLegendExpanded = !isLegendExpanded;
    legend->setVisible(isLegendExpanded);
    legendToggleButton->setText(isLegendExpanded ? "隐藏图例 ▼" : "显示图例 ▲");
    replot(QCustomPlot::rpQueuedReplot);
}

void UESTCQCustomPlot::updateLegendToggleButtonPosition()
{
    if (!legendToggleButton) return;

    QRect axisRect = this->axisRect()->rect();
    double buttonX = axisRect.left() + 50;  // 保持与createLegendToggleButton一致
    double buttonY = axisRect.top() + 20;

    legendToggleButton->position->setCoords(buttonX, buttonY);
}

void UESTCQCustomPlot::replot(QCustomPlot::RefreshPriority refreshPriority)
{
    if (legendToggleButton) {
        updateLegendToggleButtonPosition();
    }
    QCustomPlot::replot(refreshPriority);
}

void UESTCQCustomPlot::mousePressEvent(QMouseEvent* event)
{
    if (legendToggleButton) {
        // 获取按钮的像素坐标
        QPointF buttonPos = legendToggleButton->position->pixelPosition();

        // 扩大点击区域，适应新的按钮大小
        QRectF buttonRect(
            buttonPos.x() - 40,  // 左边界
            buttonPos.y() - 15,  // 上边界
            100,                 // 宽度，增加以适应文本
            30                  // 高度
            );

        if (buttonRect.contains(event->pos())) {
            toggleLegend();
            event->accept();
            return;
        }
    }

    QCustomPlot::mousePressEvent(event);
}

QCPGraph* UESTCQCustomPlot::addRealTimeLine(const QString& name)
{
    QCPGraph* graph = addGraph();
    // 分配一个新颜色
    QColor color = getNextColor();
    graph->setPen(QPen(color));
    graph->setName(name);

    // 配置图例显示
    legend->setVisible(true);
    legend->setFont(QFont("Helvetica", 9));
    legend->setBrush(QBrush(QColor(255,255,255,230)));
    axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);
    replot(QCustomPlot::rpQueuedReplot);

    addCursor(graph, color);

    return graph;
}

void UESTCQCustomPlot::updateRealTimeLines(QVector<QCPGraph*> graphs, QVector<QList<double>> data)
{
    if (graphs.size() != data.size() || graphs.isEmpty())
        return;

    qreal globalMin = 0;
    qreal globalMax = 0;
    bool first = true;

    for (int i = 0; i < graphs.size(); ++i) {
        if(!graphs[i])
            continue;

        // 为当前图生成对应的 keys 数组
        QList<qreal> keys;
        int dataSize = data[i].size();
        for (int j = 0; j < dataSize; ++j) {
            keys.append(j);
        }

        // 清空数据前先隐藏该图的游标，防止更新过程中触发 QCPItemTracer::updatePosition
        if (dataSize == 0 && cursors.contains(graphs[i])) {
            cursors[graphs[i]].tracer->setVisible(false);
            cursors[graphs[i]].label->setVisible(false);
        }

        graphs[i]->data()->clear();
        if(dataSize > 0) {
            graphs[i]->addData(keys, data[i]);
            if(first) {
                globalMin = keys.first();
                globalMax = keys.last();
                first = false;
            } else {
                globalMin = qMin(globalMin, keys.first());
                globalMax = qMax(globalMax, keys.last());
            }
        }
        maxX = dataSize;
    }

    replot(QCustomPlot::rpQueuedReplot);
}
