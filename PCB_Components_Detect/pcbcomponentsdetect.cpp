#include "pcbcomponentsdetect.h"
#include "ui_pcbcomponentsdetect.h"
#include <QMessageBox>
#include <QCloseEvent>
#include <QPushButton>

PCBComponentsDetect::PCBComponentsDetect(QWidget *parent)
    : QWidget(parent)
    , yolomodel(YOLOModel::getInstance())
    , ui(new Ui::PCBComponentsDetect)
{
    ui->setupUi(this);
    
    imageViewer = std::make_shared<ImageViewer>(nullptr, "首次加载模型需要较长一段时间，请耐心等待。。。");
    imageViewer->resize(800, 600);
    ui->horizontalLayout_image->addWidget(imageViewer.get());

    QPushButton* selectAllButton = new QPushButton("全选", this);
    QPushButton* deselectAllButton = new QPushButton("取消全选", this);
    ui->verticalLayout_label->addWidget(selectAllButton);
    ui->verticalLayout_label->addWidget(deselectAllButton);

    connect(selectAllButton, &QPushButton::clicked, [this]() {
        class_display.clear();
        for (int i = 0; i < color.size(); ++i) {
            class_display.push_back(i);
            is_pressed[i] = true;
            // 更新按钮样式
            QPushButton* button = static_cast<QPushButton*>(ui->verticalLayout_label->itemAt(i + 2)->widget());
            button->setStyleSheet("background-color: rgb(" + QString::number(color[i][2]) + ", " +
                                  QString::number(color[i][1]) + ", " +
                                  QString::number(color[i][0]) + ");");
        }
        this->yolomodel->set_class_display(class_display);
    });

    connect(deselectAllButton, &QPushButton::clicked, [this]() {
        class_display.clear();
        for (int i = 0; i < color.size(); ++i) {
            is_pressed[i] = false;
            // 更新按钮样式
            QPushButton* button = static_cast<QPushButton*>(ui->verticalLayout_label->itemAt(i + 2)->widget());
            button->setStyleSheet("background-color: gray;");
        }
        this->yolomodel->set_class_display(class_display);
    });

    for (int i = 0; i < color.size(); ++i) {
        QPushButton* button = new QPushButton(this);
        button->setStyleSheet("background-color: rgb(" + QString::number(color[i][2]) + ", " +
                              QString::number(color[i][1]) + ", " +
                              QString::number(color[i][0]) + ");");
        button->setText(class_name[i].c_str());
        ui->verticalLayout_label->addWidget(button);

        // 按钮点击事件
        connect(button, &QPushButton::clicked, [this, button, i]() {
            is_pressed[i] = !is_pressed[i]; // 切换状态
            if (!is_pressed[i]) {
                button->setStyleSheet("background-color: gray;"); // 设置为灰色
                if(std::find(class_display.begin(), class_display.end(), i) != class_display.end())
                {
                    class_display.erase(std::remove(class_display.begin(), class_display.end(), i), class_display.end());
                    this->yolomodel->set_class_display(class_display);
                }
            } else {
                button->setStyleSheet("background-color: rgb(" + QString::number(color[i][2]) + ", " +
                                      QString::number(color[i][1]) + ", " +
                                      QString::number(color[i][0]) + ");"); // 恢复颜色
                if(std::find(class_display.begin(), class_display.end(), i) == class_display.end())
                {
                    class_display.push_back(i);
                    this->yolomodel->set_class_display(class_display);
                }
            }
        });
    }
}

PCBComponentsDetect::~PCBComponentsDetect()
{
    delete ui;
    stop_recog = true;
    cout << "PCBComponentsDetect::~PCBComponentsDetect()" << endl;
    if(recogThread && recogThread->joinable()) {
        recogThread->join();
    }
    if(CAMERA->isRunning()) {
        disconnect(CAMERA, &DetectCamera::frameReady, this, &PCBComponentsDetect::displayFrame);
        CAMERA->setParams(); // 恢复默认参数
        CAMERA->stop();
    }
    cout << "PCBComponentsDetect::~PCBComponentsDetect() end" << endl;
}

void PCBComponentsDetect::startrecog()
{
    std::lock_guard<std::mutex> lock(mtx);

    stop_recog = false;
    if(CAMERA->isRunning()) {
        CAMERA->stop();
    }
    
    // 设置高分辨率和较低帧率以获取更清晰的图像
    CAMERA->setParams(7680, 4320, 20);
    connect(CAMERA, &DetectCamera::frameReady, this, &PCBComponentsDetect::displayFrame);
    CAMERA->start();
    
    recogThread = std::make_shared<std::thread>(&PCBComponentsDetect::recog, this);
    this->show();
}

void PCBComponentsDetect::recog()
{
    while(!stop_recog)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return !pause_recog; });
        if(!receive_image.empty() && is_recog)
        {
            is_recog = false;
            recog_image = yolomodel->recognize(receive_image);
            is_display = true;
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 减少CPU占用
    }
}

void PCBComponentsDetect::displayFrame(const cv::Mat& frame)
{
    receive_image = frame;
    if(is_display && !recog_image.empty())
    {
        std::unique_lock<std::mutex> lock(mtx);
        is_display = false;
        QImage img = QImage(recog_image.data, recog_image.cols, recog_image.rows, recog_image.step, QImage::Format_RGB888).rgbSwapped();
        QSize image_size = imageViewer->size();
        QPixmap pixmap = QPixmap::fromImage(img).scaled(image_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if(image_size == imageViewer->size())
        {
            imageViewer->setImage(pixmap);
        }
        is_recog = true;
    }
}

void PCBComponentsDetect::closeEvent(QCloseEvent *event)
{
    if(!this->isVisible())
        return;
        
    std::lock_guard<std::mutex> lock(mtx);
    
    // 设置退出标志并等待线程结束
    stop_recog = true;
    pause_recog = false;
    
    if(recogThread && recogThread->joinable()) {
        recogThread->join();
    }
    
    if(CAMERA->isRunning()) {
        disconnect(CAMERA, &DetectCamera::frameReady, this, &PCBComponentsDetect::displayFrame);
        CAMERA->setParams(); // 恢复默认参数
        CAMERA->stop();
    }
    
    cv.notify_one();
    cout << "PCBComponentsDetect::closeEvent()" << endl;
    event->accept();
}

void PCBComponentsDetect::on_pbsave_clicked()
{
    {std::unique_lock<std::mutex> lock(mtx);
    pause_recog = true;}
    emit signal_save_image_label(receive_image, yolomodel->get_labels());
    {std::unique_lock<std::mutex> lock(mtx);
    stop_recog = true;
    pause_recog = false;
    cv.notify_one();}
}


void PCBComponentsDetect::on_pbcancle_clicked()
{
    this->close();
}

