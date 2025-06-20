#ifndef LABELMANAGERDIALOG_H
#define LABELMANAGERDIALOG_H

#include <QDialog>
#include <QImage>
#include <vector>
#include "labelediting.h"  // 包含 LabelEditingWindow 及 Label 相关定义
#include <QString>

// 新增结构体，用于返回标签变化信息
struct LabelChanges {
    std::vector<Label> updatedLabels;  // 原来已有的标签，需要更新
    std::vector<Label> insertedLabels; // 新建的标签，需要插入
    std::vector<int> deletedIds;       // 需要删除的标签ID
};

class LabelManagerDialog : public QDialog
{
    Q_OBJECT
public:
    // 构造函数：只需传入图片和设备id
    explicit LabelManagerDialog(QWidget *parent,
                                const QImage &image,
                                const int &deviceId,
                                const std::vector<Label> &Addlabels);
    ~LabelManagerDialog();

    // 返回当前编辑后的所有标签（未区分更新与新增）
    std::vector<Label> getLabels() const;

    // 返回详细的标签变化信息：更新、插入和删除
    LabelChanges getLabelChanges() const;

private:
    LabelEditingWindow *m_editWindow;
    // 删除标签的id列表，由 LabelEditingWindow 在删除操作中更新
    std::vector<int> m_deleteIds;
    std::vector<Label> labelInfo;
    std::vector<Label> labelInfoAdd;
};

#endif // LABELMANAGERDIALOG_H 