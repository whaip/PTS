#include "labelmanagerdialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
// #include "database.h"
#include <QMessageBox>

LabelManagerDialog::LabelManagerDialog(QWidget *parent,
                                       const QImage &image,
                                       const int &deviceId,
                                       const std::vector<Label> &Addlabels)
    : QDialog(parent),
      labelInfoAdd(Addlabels)
{
    setWindowTitle("设备标签管理");
    resize(900, 700);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);    if (Addlabels.size() == 0) {  // 如果新增标签为空，即为设备原有标签的编辑，则从数据库中获取标签，否则为新建设备
        // TODO: Implement database functionality
        // Database db("LabelManager", this);
        // QString errorMessage;
        // if (!db.get_deviceelement(deviceId, "", labelInfo, errorMessage)) {
        //     QMessageBox::warning(this, "错误", "获取设备标签失败: " + errorMessage);
        // }
        QMessageBox::information(this, "提示", "数据库功能暂未实现，请先添加标签");
    }

    // 创建 LabelEditingWindow，用于编辑标签
    m_editWindow = new LabelEditingWindow(this, image, labelInfo, labelInfoAdd, m_deleteIds);
    mainLayout->addWidget(m_editWindow);

    // 添加对话框按钮（确定和取消）
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &LabelManagerDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &LabelManagerDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

LabelManagerDialog::~LabelManagerDialog()
{
    delete m_editWindow;
}

std::vector<Label> LabelManagerDialog::getLabels() const {
    std::vector<Label> labels;
    if (m_editWindow && m_editWindow->labelEditing) {
        // 获取所有标签信息
        m_editWindow->labelEditing->getAllLabelItemInfo(labels);
    }
    return labels;
}

LabelChanges LabelManagerDialog::getLabelChanges() const {
    LabelChanges changes;
    std::vector<Label> allLabels;
    if (m_editWindow && m_editWindow->labelEditing) {
        m_editWindow->labelEditing->getAllLabelItemInfo(allLabels);
    }
    // 按照 id 进行区分：id > 0 表示原有标签需要更新；否则表示新建标签，需插入
    for (const auto &lbl : allLabels) {
        if (lbl.id > 0)
            changes.updatedLabels.push_back(lbl);
        else
            changes.insertedLabels.push_back(lbl);
    }
    changes.deletedIds = m_deleteIds;
    return changes;
} 
