#include "labelediting.h"
#include <QDebug>
#include <QPushButton>
#include <QInputDialog>
#include <QColorDialog>
#include <QComboBox>
#include <QtCore>

LabelEditing::LabelEditing(QWidget *parent, const QImage &image, std::vector<Label> label_info, std::vector<Label> label_info_add, std::vector<int>& delete_id, QTableWidget*& labelTable)
    : QGraphicsView(parent)
    , label_image(image)
    , label_info(label_info)
    , label_info_add(label_info_add)
    , delete_id(delete_id)
    , labelTable(labelTable)
{
    scene = new QGraphicsScene(this);
    setScene(scene);
    
    // 启用缓存背景
    setCacheMode(QGraphicsView::CacheBackground);
    // 设置视图更新模式
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    
    setupLabelTable();
    loadImage(label_image);
    
    // 分别设置渲染提示
    setRenderHint(QPainter::Antialiasing);        // 启用抗锯齿
    setRenderHint(QPainter::SmoothPixmapTransform); // 启用平滑缩放
    
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setCursor(Qt::CrossCursor);
    setupUI();
    updateLabelTable();
}

LabelEditing::~LabelEditing()
{
    if (rubberBand) {
        delete rubberBand;
        rubberBand = nullptr;
    }
    if (createRectItem) {
        delete createRectItem;
        createRectItem = nullptr;
    }
}

void LabelEditing::loadImage(const QImage &image)
{
    scene->clear();

    if (image.isNull()) {
        qDebug() << "image is null";
        return;
    }

    // 直接使用原始图像，不进行缩放
    QPixmap pixmap = QPixmap::fromImage(image);
    
    // 创建图像项并添加到场景
    QGraphicsPixmapItem *pixmapItem = scene->addPixmap(pixmap);
    pixmapItem->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    pixmapItem->setCacheMode(QGraphicsItem::DeviceCoordinateCache);

    // 调整场景大小以适应图像
    scene->setSceneRect(pixmap.rect());

    // 添加已有的标注矩形
    if (!label_info.empty()) {
        for (int i = 0; i < label_info.size(); ++i) {
            label_info[i].syncFields();  // 确保兼容性字段同步
            LabelRectItem *rectItem = new LabelRectItem(nullptr, label_info[i]);
            rectItem->setRect(QRectF(label_info[i].point_x, label_info[i].point_y, label_info[i].width, label_info[i].height));
            rectItem->setLabel(label_info[i].label);
            QColor randomColor = QColor::fromRgb(
                QRandomGenerator::global()->bounded(256),  // R
                QRandomGenerator::global()->bounded(256),  // G
                QRandomGenerator::global()->bounded(256)   // B
            );
            rectItem->setPen(QPen(randomColor, 2));
            scene->addItem(rectItem);
            label_rect_item.push_back(rectItem);
        }
    }

    if(!label_info_add.empty()){
        for(int i = 0; i < label_info_add.size(); ++i){
            label_info_add[i].id = -i;
            label_info_add[i].syncFields();  // 确保兼容性字段同步
            LabelRectItem *rectItem = new LabelRectItem(nullptr, label_info_add[i]);
            rectItem->setRect(QRectF(label_info_add[i].point_x, label_info_add[i].point_y, label_info_add[i].width, label_info_add[i].height));
            rectItem->setLabel(label_info_add[i].label);
            QColor randomColor = QColor::fromRgb(
                QRandomGenerator::global()->bounded(256),  // R
                QRandomGenerator::global()->bounded(256),  // G
                QRandomGenerator::global()->bounded(256)   // B
            );
            rectItem->setPen(QPen(randomColor, 2));
            scene->addItem(rectItem);
            label_rect_item_add.push_back(rectItem);
        }
    }
    
    updateLabelTable();

    // 确保首次加载后图片适配视图
    fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

void LabelEditing::setupLabelTable()
{
    labelTable->setColumnCount(5);
    labelTable->setHorizontalHeaderLabels({"ID", "Label", "类型", "位号", "备注"});

    labelTable->hideColumn(0);

    labelTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    labelTable->setSelectionMode(QAbstractItemView::SingleSelection);
    labelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    labelTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    connect(labelTable, &QTableWidget::cellClicked, this, &LabelEditing::onTableRowClicked);
    connect(labelTable, &QTableWidget::cellChanged, this, &LabelEditing::onTableCellChanged);
}

void LabelEditing::updateLabelTable()
{
    qDebug() << "LabelEditing::updateLabelTable - 开始更新表格";
    labelTable->setRowCount(0);

    int row = 0;
    std::vector<LabelRectItem*> items = label_rect_item;
    items.insert(items.end(), label_rect_item_add.begin(), label_rect_item_add.end());
    qDebug() << "LabelEditing::updateLabelTable - 总共" << items.size() << "个标签项";
    
    for (LabelRectItem* item : items) {
        try {
            auto itemInfo = item->getItemInfo();
            int id = std::get<0>(itemInfo);
            qDebug() << "LabelEditing::updateLabelTable - 处理标签项，ID:" << id << "标签:" << item->getLabel();

            labelTable->insertRow(row);
            // ID
            labelTable->setItem(row, 0, new QTableWidgetItem(QString::number(id)));
            // 标签文本
            labelTable->setItem(row, 1, new QTableWidgetItem(item->getLabel()));
            // 类型下拉框
            QComboBox* typeCombo = new QComboBox(labelTable);
            for (int i = 0; i < 16; ++i) {
                typeCombo->addItem(getComponentTypeName(i), i);
            }
            int currentCls = item->getLabelInfo().cls;
            if (currentCls >= 0 && currentCls < 16) {
                typeCombo->setCurrentIndex(currentCls);
            }
            labelTable->setCellWidget(row, 2, typeCombo);
            connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this, item, typeCombo](int index) {
                int newCls = typeCombo->itemData(index).toInt();
                item->setComponentType(newCls);
                Label updatedLabel = item->getLabelInfo();
                emit labelUpdated(updatedLabel);
            });
            // 位号
            labelTable->setItem(row, 3, new QTableWidgetItem(item->getPositionNumber()));
            // 备注
            labelTable->setItem(row, 4, new QTableWidgetItem(QString::fromUtf8(item->getNotes())));
            row++;
        } catch (const std::exception& e) {
            qDebug() << "Error processing item:" << e.what();
        }
    }

    labelTable->resizeColumnsToContents();
}

void LabelEditing::onTableCellChanged(int row, int column)
{
    labelTable->disconnect(SIGNAL(cellChanged(int,int)));
    
    QTableWidgetItem* labelItem = labelTable->item(row, 1);
    QTableWidgetItem* positionItem = labelTable->item(row, 3);
    QTableWidgetItem* notesItem = labelTable->item(row, 4);
    QTableWidgetItem* idItem = labelTable->item(row, 0);
    
    if (!labelItem || !positionItem || !notesItem || !idItem) {
        // 重新连接信号并返回
        connect(labelTable, &QTableWidget::cellChanged, this, &LabelEditing::onTableCellChanged);
        return;
    }
    
    QString newLabel = labelItem->text();
    QString newPositionNumber = positionItem->text();
    QByteArray notes = notesItem->text().toUtf8();
    int id = idItem->text().toInt();
    
    // 更新对应的label_info或label_info_add中的标签
    Label updatedLabel;
    auto it = std::find_if(label_rect_item.begin(), label_rect_item.end(),
        [id](const LabelRectItem* label) { return label->getLabelInfo().id == id; });
    if (it != label_rect_item.end()) {
        LabelRectItem* rectItem = *it;
        if (rectItem) {
            rectItem->setLabel(newLabel);
            rectItem->setPositionNumber(newPositionNumber);
            rectItem->setNotes(notes);
            
            // 发射标签更新信号
            updatedLabel = rectItem->getLabelInfo();
        }
    }

    it = std::find_if(label_rect_item_add.begin(), label_rect_item_add.end(),
        [id](const LabelRectItem* label) { return label->getLabelInfo().id == id; });
    if (it != label_rect_item_add.end()) {
        LabelRectItem* rectItem = *it;
        if (rectItem) {
            rectItem->setLabel(newLabel);
            rectItem->setPositionNumber(newPositionNumber);
            rectItem->setNotes(notes);
            
            updatedLabel = rectItem->getLabelInfo();
        }
    }
    
    scene->update();
    // 重新连接信号
    connect(labelTable, &QTableWidget::cellChanged, this, &LabelEditing::onTableCellChanged);
    emit labelUpdated(updatedLabel);
}

void LabelEditing::onTableRowClicked(int row)
{
    clearAllSelection();
    int id = labelTable->item(row, 0)->text().toInt();
    selectAndCenterRectItem(id, row);
}

void LabelEditing::selectAndCenterRectItem(int labelId, int row)
{
    auto processRectItem = [this](LabelRectItem* rectItem) {
        clearAllSelection();
        if (rectItem) {
            rectItem->setSelected(true);
            selectRectItem = rectItem;

            // 获取矩形的中心点和尺寸
            QRectF rect = rectItem->rect();
            QPointF center = rectItem->mapToScene(rect.center());

            // 计算缩放比例
            const qreal MIN_SCALE = 0.5;  // 最小缩放比例
            const qreal MAX_SCALE = 2.0;  // 最大缩放比例
            const qreal TARGET_COVERAGE = 0.7;  // 目标覆盖率（矩形占视口的比例）

            // 计算基础缩放比例
            qreal scaleX = viewport()->width() / rect.width();
            qreal scaleY = viewport()->height() / rect.height();
            qreal baseScale = qMin(scaleX, scaleY) * TARGET_COVERAGE;

            // 根据矩形大小动态调整缩放比例
            qreal area = rect.width() * rect.height();
            qreal viewArea = viewport()->width() * viewport()->height();
            qreal areaRatio = area / viewArea;

            // 如果矩形面积相对较小，降低缩放比例
            if (areaRatio < 0.01) {  // 矩形面积小于视口面积的1%
                baseScale *= 0.5;  // 降低缩放比例
            }

            // 限制最终缩放比例在合理范围内
            qreal scaleFactor = qBound(MIN_SCALE, baseScale, MAX_SCALE);

            // 应用缩放
            resetTransform();
            scale(scaleFactor, scaleFactor);

            // 计算并应用滚动条位置
            QPointF viewCenter = mapFromScene(center);
            QPointF delta = viewCenter - QPointF(viewport()->rect().center());

            // 创建动画
            QPropertyAnimation* hAnimation = new QPropertyAnimation(horizontalScrollBar(), "value");
            hAnimation->setDuration(200);
            hAnimation->setStartValue(horizontalScrollBar()->value());
            hAnimation->setEndValue(horizontalScrollBar()->value() + delta.x());
            hAnimation->setEasingCurve(QEasingCurve::OutCubic);

            QPropertyAnimation* vAnimation = new QPropertyAnimation(verticalScrollBar(), "value");
            vAnimation->setDuration(200);
            vAnimation->setStartValue(verticalScrollBar()->value());
            vAnimation->setEndValue(verticalScrollBar()->value() + delta.y());
            vAnimation->setEasingCurve(QEasingCurve::OutCubic);

            hAnimation->start(QPropertyAnimation::DeleteWhenStopped);
            vAnimation->start(QPropertyAnimation::DeleteWhenStopped);
        }
    };

    if (labelId > 0) {
        auto it = std::find_if(label_rect_item.begin(), label_rect_item.end(),
            [labelId](const LabelRectItem* label) { return label->getLabelInfo().id == labelId; });
        if (it != label_rect_item.end()) {
            qDebug() << "LabelEditing::selectAndCenterRectItem - 找到标签项，ID:" << labelId;
            processRectItem(*it);
        }
        
        it = std::find_if(label_rect_item_add.begin(), label_rect_item_add.end(),
            [labelId](const LabelRectItem* label) { return label->getLabelInfo().id == labelId; });
        if (it != label_rect_item_add.end()) {
            qDebug() << "LabelEditing::selectAndCenterRectItem - 找到标签项，ID:" << labelId;
            processRectItem(*it);
        }
    }
}

void LabelEditing::setLabelTableFocus()
{
    if (selectRectItem) {
        int id = std::get<0>(selectRectItem->getItemInfo());
        for(int i = 0; i < labelTable->rowCount(); ++i){
            if(labelTable->item(i, 0)->text().toInt() == id){
                labelTable->selectRow(i);
                labelTable->scrollTo(labelTable->model()->index(i, 0), QAbstractItemView::PositionAtCenter); // 确保选中的行在视图中心
                labelTable->setFocus();
                break;
            }
        }
    }
}

void LabelEditing::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (scene) {
        setSceneRect(scene->sceneRect());
        fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    }
}

void LabelEditing::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        origin = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        QGraphicsView::mousePressEvent(event);
    }
    if (is_editing && event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        QGraphicsItem *item = scene->itemAt(scenePos, transform());
        if (LabelRectItem *rectItem = qgraphicsitem_cast<LabelRectItem*>(item))
        {
            bool ctrlPressed = event->modifiers() & Qt::ControlModifier;
            if (!ctrlPressed) {
                clearAllSelection();
            }
            if (ctrlPressed) {
                // 切换当前项的选择状态
                rectItem->setSelected(!rectItem->getSelected());
            } else {
                rectItem->setSelected(true);
            }
            selectRectItem = rectItem;
            setLabelTableFocus();
        }
        QGraphicsView::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (createRectItem != nullptr) {
            scene->removeItem(createRectItem);
            delete createRectItem;
            createRectItem = nullptr;
        }

        origin = mapToScene(event->pos());
        
        int id = label_rect_item_add.size() > 0 ? label_rect_item_add[label_rect_item_add.size() - 1]->getLabelInfo().id - 1 : 0;
        Label newLabel(id, origin.x(), origin.y(), 1.0, 1.0, 0, 100.0, "-1", "-1");
        rubberBand = new LabelRectItem(nullptr, newLabel);
        
        rubberBand->setRect(QRectF(origin, QSizeF(0, 0)));
        updateRectItemStyle(rubberBand);
        scene->addItem(rubberBand);
    }
}

void LabelEditing::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        if(!is_editing) viewport()->setCursor(Qt::CrossCursor);
        else viewport()->setCursor(Qt::ArrowCursor);
    }

    if (is_editing) {
        QGraphicsView::mouseReleaseEvent(event); // 将事件传递给场景
    }

    if (!is_editing && event->button() == Qt::LeftButton && rubberBand) {
        createRectItem = rubberBand;
        clearAllSelection();
        rubberBand->setSelected(true);
        selectRectItem = rubberBand;
        on_editButton_clicked();
        rubberBand = nullptr;
    }
}

void LabelEditing::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::RightButton) {
        QPointF delta = event->pos() - origin;
        origin = event->pos();

        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    } else if (is_only_view || is_select_model) {return;}
    else if (!is_editing) {
        if (rubberBand) {
            QPointF currentPos = mapToScene(event->pos());
            QRectF newRect(origin, currentPos);
            rubberBand->setRect(newRect);
            scene->update();
        }
    } else {
        QGraphicsView::mouseMoveEvent(event); // 将事件传递给场景
    }
}

void LabelEditing::on_createRectButton_clicked()
{
    createRectButton->setStyleSheet("background-color: blue;");
    editButton->setStyleSheet("");
    viewport()->setCursor(Qt::CrossCursor); // 只改变视图部分的鼠标形状为十字形
    clearAllSelection();
    is_editing = false;
}

void LabelEditing::on_editButton_clicked()
{
    createRectButton->setStyleSheet("");
    editButton->setStyleSheet("background-color: blue;");
    viewport()->setCursor(Qt::ArrowCursor); // 只改变视图部分的鼠标形状为箭头形
    is_editing = true;
}

void LabelEditing::on_finishButton_clicked()
{
    if (createRectItem) {
        label_rect_item_add.push_back(createRectItem);
        
        // 发射标签添加信号
        Label newLabel = createRectItem->getLabelInfo();
        newLabel.updateFromCompat();
        emit labelAdded(newLabel);
        createRectItem = nullptr;
    }else if(selectRectItem){
        emit labelUpdated(selectRectItem->getLabelInfo());
    }
    clearAllSelection();
    is_editing = false;
    on_createRectButton_clicked();
    updateLabelTable();
}

void LabelEditing::on_deleteButton_clicked()
{
    if (selectRectItem) {
        if (selectRectItem == createRectItem) {
            createRectItem = nullptr;
            scene->removeItem(selectRectItem);
            scene->update();
            return;
        }
        if (selectRectItem->getSelected()) {
            scene->removeItem(selectRectItem);
            int id = std::get<0>(selectRectItem->getItemInfo());
            
            // 发射标签删除信号
            emit labelDeleted(id);
            
            if (id > 0) {
                auto it = std::find_if(label_rect_item.begin(), label_rect_item.end(), [id](const LabelRectItem* label) {
                    return label->getLabelInfo().id == id;
                });
                if (it != label_rect_item.end()) {
                    label_rect_item.erase(it);
                    delete_id.push_back(id);
                }
            }else{
                auto it = std::find_if(label_rect_item_add.begin(), label_rect_item_add.end(), [id](const LabelRectItem* label) {
                    return label->getLabelInfo().id == id;
                });
                if (it != label_rect_item_add.end()) {
                    label_rect_item_add.erase(it);
                }
            }
            delete selectRectItem;
            selectRectItem = nullptr;
            scene->update();
        }
        updateLabelTable();
    }
}

void LabelEditing::reloadItems()
{
    // 清除场景中的所有项
    scene->clear();
    
    // 重新加载图像
    QPixmap pixmap = QPixmap::fromImage(label_image);
    QGraphicsPixmapItem *pixmapItem = scene->addPixmap(pixmap);
    pixmapItem->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    pixmapItem->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    
    // 重新加载已有的标注矩形
    for (auto& rectItem : label_rect_item) {
        QColor randomColor = QColor::fromRgb(
            QRandomGenerator::global()->bounded(256),
            QRandomGenerator::global()->bounded(256),
            QRandomGenerator::global()->bounded(256)
        );
        rectItem->setPen(QPen(randomColor, 2));
        scene->addItem(rectItem);
    }
    
    // 重新加载新添加的标注矩形
    for (size_t i = 0; i < label_rect_item_add.size(); ++i) {
        QColor randomColor = QColor::fromRgb(
            QRandomGenerator::global()->bounded(256),
            QRandomGenerator::global()->bounded(256),
            QRandomGenerator::global()->bounded(256)
        );
        label_rect_item_add[i]->setPen(QPen(randomColor, 2));
        scene->addItem(label_rect_item_add[i]);
    }
    
    updateLabelTable();
}

void LabelEditing::wheelEvent(QWheelEvent *event)
{
    const double scaleFactor = 1.15; // 缩放因子

    // 获取鼠标在视图中的位置
    QPointF viewPos = event->position();

    // 将视图坐标转换为场景坐标
    QPointF scenePos = mapToScene(viewPos.toPoint());

    if (event->angleDelta().y() > 0) {
        // 放大
        scale(scaleFactor, scaleFactor);
    } else {
        // 缩小
        scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }

    // 调整视图中心以保持鼠标位置不变
    QPointF newViewPos = mapFromScene(scenePos);
    QPointF delta = newViewPos - viewPos;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + delta.y());
}

void LabelEditing::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (is_select_model)
    {
        emit closeRequested(); 
        return;
    }
    if (!is_editing || is_only_view)
        return;

    QPointF scenePos = mapToScene(event->pos());
    QGraphicsItem *item = scene->itemAt(scenePos, QTransform());
    if (LabelRectItem *rectItem = qgraphicsitem_cast<LabelRectItem*>(item))
    {
        bool ok;
        newLabel = QInputDialog::getText(this,
                                        "修改标签",
                                        "请输入新的标签:",
                                        QLineEdit::Normal,
                                        rectItem->getLabel(),
                                        &ok);
        if (ok && !newLabel.isEmpty()) {
            rectItem->setLabel(newLabel);
            
            // 发射标签更新信号
            Label updatedLabel = rectItem->getLabelInfo();
            emit labelUpdated(updatedLabel);
            
            updateLabelTable();
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void LabelEditing::clearAllSelection()
{
    // 取消所有矩形的选中状态
    QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem* item : items) {
        if (LabelRectItem* rectItem = qgraphicsitem_cast<LabelRectItem*>(item)) {
            rectItem->setSelected(false);
        }
        item->update();
    }
    if (selectRectItem != nullptr) {
        selectRectItem = nullptr;
    }
}

void LabelEditing::getAllLabelItemInfo(std::vector<Label> &result_label_info)
{
    std::vector<LabelRectItem*> items = label_rect_item;
    items.insert(items.end(), label_rect_item_add.begin(), label_rect_item_add.end());
    for (auto& item : items) {
        Label label = item->getLabelInfo();
        result_label_info.push_back(label);
    }
}

LabelRectItem* LabelEditing::getRectItemById(int id) const
{
    for (LabelRectItem* item : label_rect_item) {
        if (item->getLabelInfo().id == id)
            return item;
    }
    for (LabelRectItem* item : label_rect_item_add) {
        if (item->getLabelInfo().id == id)
            return item;
    }
    return nullptr;
}

void LabelEditing::setupUI()
{
    // 原有的按钮
    createRectButton = new QPushButton("创建矩形", this);
    createRectButton->setStyleSheet("background-color: blue;");
    editButton = new QPushButton("编辑/查看", this);
    finishButton = new QPushButton("保存矩形", this);
    deleteButton = new QPushButton("删除", this);  // 添加删除按钮

    // 添加颜色选择按钮
    colorButton = new QPushButton("画笔颜色", this);
    colorButton->setStyleSheet(QString("background-color: %1").arg(currentColor.name()));

    // 添加线宽选择下拉框
    lineWidthComboBox = new QComboBox(this);
    lineWidthComboBox->addItems({"0.1px", "0.3px", "0.5px", "0.7px", "1px", "2px", "3px", "4px", "5px"});
    lineWidthComboBox->setCurrentText("2px");
    
    // 计算最长项的宽度
    int maxWidth = 0;
    QFontMetrics fm(lineWidthComboBox->font());
    for(int i = 0; i < lineWidthComboBox->count(); i++) {
        int width = fm.horizontalAdvance(lineWidthComboBox->itemText(i));
        maxWidth = qMax(maxWidth, width);
    }
    // 添加一些额外的空间用于下拉箭头和边距
    lineWidthComboBox->setMinimumWidth(maxWidth + 40);

    // 连接信号和槽
    connect(createRectButton, &QPushButton::clicked, this, &LabelEditing::on_createRectButton_clicked);
    connect(editButton, &QPushButton::clicked, this, &LabelEditing::on_editButton_clicked);
    connect(finishButton, &QPushButton::clicked, this, &LabelEditing::on_finishButton_clicked);
    connect(deleteButton, &QPushButton::clicked, this, &LabelEditing::on_deleteButton_clicked);  // 连接删除按钮信号
    connect(colorButton, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(currentColor, this, "选择颜色");
        if (color.isValid()) {
            currentColor = color;
            colorButton->setStyleSheet(QString("background-color: %1").arg(color.name()));
        }
    });
    connect(lineWidthComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                // 根据索引计算实际线宽
                switch(index) {
                case 0: currentLineWidth = 0.1; break;
                case 1: currentLineWidth = 0.3; break;
                case 2: currentLineWidth = 0.5; break;
                case 3: currentLineWidth = 0.7; break;
                case 4: currentLineWidth = 1.0; break;
                default: currentLineWidth = index - 3; // 2px及以上
                }
            });

    // 设置按钮位置和大小，根据文本自动调整大小
    createRectButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    editButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    finishButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    deleteButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    colorButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    createRectButton->adjustSize();
    editButton->adjustSize();
    finishButton->adjustSize();
    deleteButton->adjustSize();
    colorButton->adjustSize();

    int nextX = 10;
    createRectButton->setGeometry(nextX, 10, createRectButton->width(), 30);
    nextX += createRectButton->width() + 10;
    editButton->setGeometry(nextX, 10, editButton->width(), 30);
    nextX += editButton->width() + 10;
    finishButton->setGeometry(nextX, 10, finishButton->width(), 30);
    nextX += finishButton->width() + 10;
    deleteButton->setGeometry(nextX, 10, deleteButton->width(), 30);
    nextX += deleteButton->width() + 10;
    colorButton->setGeometry(nextX, 10, colorButton->width(), 30);
    nextX += colorButton->width() + 10;
    lineWidthComboBox->setGeometry(nextX, 10, lineWidthComboBox->minimumWidth(), 30);
}

void LabelEditing::updateRectItemStyle(LabelRectItem* item)
{
    if (item) {
        item->setPen(QPen(currentColor, currentLineWidth));
    }
}

void LabelEditing::closeEvent(QCloseEvent *event)
{
    emit window_close();
    event->accept();
}

void LabelEditing::setonlyviewmodel()
{
    is_only_view = true;
    on_editButton_clicked();
    createRectButton->setVisible(false);
    editButton->setVisible(false);
    finishButton->setVisible(false);
    deleteButton->setVisible(false);
    colorButton->setVisible(false);
    lineWidthComboBox->setVisible(false);
    labelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void LabelEditing::setSelectModel()
{
    is_select_model = true;
    on_editButton_clicked();
    createRectButton->setVisible(false);
    editButton->setVisible(false);
    finishButton->setVisible(false);
    deleteButton->setVisible(false);
    colorButton->setVisible(false);
    lineWidthComboBox->setVisible(false);
    labelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void LabelEditing::triggerTableRowClickById(int labelId)
{
    // 遍历表格的所有行，查找匹配的 label.id
    QMetaObject::invokeMethod(this, [this, labelId]() {
        for(int row = 0; row < labelTable->rowCount(); ++row) {
            QTableWidgetItem* idItem = labelTable->item(row, 0);
            if(idItem && idItem->text().toInt() == labelId) {
                selectAndCenterRectItem(labelId, row);
                break;
            }
        }
    }, Qt::QueuedConnection);
}

std::vector<Label> LabelEditing::getSelectedLabelItemInfos() const
{
    std::vector<Label> selectedLabels;
    std::vector<LabelRectItem*> items = label_rect_item;
    items.insert(items.end(), label_rect_item_add.begin(), label_rect_item_add.end());
    for (auto* item : items) {
        if (item && item->getSelected()) {
            selectedLabels.push_back(item->getLabelInfo());
        }
    }
    return selectedLabels;
}

void LabelEditing::refreshTable()
{
    qDebug() << "LabelEditing::refreshTable - 开始刷新表格";
    updateLabelTable();
    qDebug() << "LabelEditing::refreshTable - 表格刷新完成";
}
