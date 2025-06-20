#ifndef PREVIEWDIALOG_H
#define PREVIEWDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <opencv2/opencv.hpp>
#include "detectcamera.h"

class PreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreviewDialog(QWidget *parent = nullptr);
    ~PreviewDialog();

signals:
    void imageCaptured(const cv::Mat& frame);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void updatePreview(const cv::Mat& frame);
    void onCaptureClicked();

private:
    QLabel* previewLabel;
    QPushButton* captureButton;
    QPushButton* cancelButton;
};

#endif // PREVIEWDIALOG_H 