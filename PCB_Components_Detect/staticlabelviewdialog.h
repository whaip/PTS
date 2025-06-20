#ifndef STATICLABELVIEWDIALOG_H
#define STATICLABELVIEWDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QScrollArea>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <opencv2/opencv.hpp>
#include "yolomodel.h"
#include "ClassList.h"

class StaticLabelViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StaticLabelViewDialog(QWidget *parent = nullptr, 
                                  const QImage& image = QImage(),
                                  const std::vector<Label>& existingLabels = {});
    ~StaticLabelViewDialog();

    // 获取标签结果
    std::vector<Label> getLabels() const;

signals:
    void labelsUpdated(const std::vector<Label>& labels);

private slots:
    void onDetectLabels();
    void onLabelToggled(int index, bool checked);
    void selectAllLabels();
    void deselectAllLabels();
    void onAccept();
    void onReject();
    
private:
    QGraphicsView *imageView;
    QGraphicsScene *scene;
    QGraphicsPixmapItem *imageItem;
    QPushButton *detectButton;
    QPushButton *okButton;
    QPushButton *cancelButton;
    QWidget *labelPanel;
    std::vector<QCheckBox*> labelCheckBoxes;
    
    QImage originalImage;
    cv::Mat cvImage;
    std::vector<Label> currentLabels;
    std::vector<Label> detectedLabels;
    
    void setupUI();
    void setupLabelPanel();
    void updateImageDisplay();
    void drawLabelsOnImage();
    QImage cvMatToQImage(const cv::Mat& mat);
    QString getStyleSheet(const cv::Scalar& color);
    cv::Mat qImageToCvMat(const QImage& qimg);
};

#endif // STATICLABELVIEWDIALOG_H
