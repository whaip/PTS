#include "previewdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>

PreviewDialog::PreviewDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("相机预览"));
    setMinimumSize(800, 600);

    // 创建界面元素
    previewLabel = new QLabel(this);
    previewLabel->setMinimumSize(640, 480);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setScaledContents(true);

    captureButton = new QPushButton(tr("捕获"), this);
    cancelButton = new QPushButton(tr("取消"), this);

    // 布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(previewLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(captureButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    if(CAMERA->isRunning())
    {
        CAMERA->stop();
    }
    CAMERA->setParams(7680, 4320, 20);
    CAMERA->start();

    // 连接信号
    connect(captureButton, &QPushButton::clicked, this, &PreviewDialog::onCaptureClicked);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(CAMERA, &DetectCamera::frameReady, this, &PreviewDialog::updatePreview);
}

PreviewDialog::~PreviewDialog()
{
}

void PreviewDialog::paintEvent(QPaintEvent* event)
{
    QDialog::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 计算预览标签的实际位置和大小
    QRect labelRect = previewLabel->geometry();
    QRect videoRect = QRect(
        labelRect.x() + 80,
        labelRect.y() + 60,
        480,
        360
    );

    // 绘制边框
    QPen pen(Qt::green, 2);
    painter.setPen(pen);
    painter.drawRect(videoRect);

    // 在四个角落绘制L形标记
    int cornerSize = 20;
    painter.setPen(QPen(Qt::green, 3));
    
    // 左上角
    painter.drawLine(videoRect.left(), videoRect.top(), videoRect.left() + cornerSize, videoRect.top());
    painter.drawLine(videoRect.left(), videoRect.top(), videoRect.left(), videoRect.top() + cornerSize);
    
    // 右上角
    painter.drawLine(videoRect.right(), videoRect.top(), videoRect.right() - cornerSize, videoRect.top());
    painter.drawLine(videoRect.right(), videoRect.top(), videoRect.right(), videoRect.top() + cornerSize);
    
    // 左下角
    painter.drawLine(videoRect.left(), videoRect.bottom(), videoRect.left() + cornerSize, videoRect.bottom());
    painter.drawLine(videoRect.left(), videoRect.bottom(), videoRect.left(), videoRect.bottom() - cornerSize);
    
    // 右下角
    painter.drawLine(videoRect.right(), videoRect.bottom(), videoRect.right() - cornerSize, videoRect.bottom());
    painter.drawLine(videoRect.right(), videoRect.bottom(), videoRect.right(), videoRect.bottom() - cornerSize);
}

void PreviewDialog::updatePreview(const cv::Mat& frame)
{
    QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
    QPixmap pix = QPixmap::fromImage(img)
                    .scaled(previewLabel->size(),
                            Qt::KeepAspectRatio,
                            Qt::SmoothTransformation);
    previewLabel->setPixmap(pix);

    QRect labelRect = previewLabel->geometry();
    
    QSize s = pix.size();
    QRect videoRect(labelRect.x() + (labelRect.width() - s.width())/2,
                    labelRect.y() + (labelRect.height() - s.height())/2,
                    s.width(), s.height());
}

void PreviewDialog::onCaptureClicked()
{
    cv::Mat frame = CAMERA->getCurrentFrame();
    if (!frame.empty()) {
        emit imageCaptured(frame);
        accept();
    }
}
