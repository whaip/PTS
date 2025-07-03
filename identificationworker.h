#ifndef IDENTIFICATIONWORKER_H
#define IDENTIFICATIONWORKER_H

#include <QObject>
#include <QList>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include "pcbboardmanager.h"

class IdentificationWorker : public QObject
{
    Q_OBJECT

public:
    explicit IdentificationWorker(PCBBoardManager* manager, const cv::Mat& image, int maxnumb, QObject *parent = nullptr)
        : QObject(parent), board_manager_(manager), input_image_(image.clone()), maxnumb_(maxnumb) {}

public slots:
    void doWork() {
        try {
            qDebug() << "开始异步板卡识别，图片尺寸:" << input_image_.cols << "x" << input_image_.rows;
            
            std::vector<SiftMatcher::MatchResult> matchresult = SIFT_MATCHER->matchImage(input_image_);

            QList<PCBBoardInfo> result;
            for (const auto& match : matchresult) {
                PCBBoardInfo board;
                qDebug() << "匹配分数：" << match.matchScore;
                board = board_manager_->getBoardById(QString::fromStdString(match.boardId));
                result.append(board);
                if(result.size() > maxnumb_) break;
            }
            emit finished(result);
            
        } catch (const cv::Exception& e) {
            qDebug() << "OpenCV异常:" << QString::fromStdString(e.what());
            emit error(QString("图像处理错误: %1").arg(QString::fromStdString(e.what())));
        } catch (const std::exception& e) {
            qDebug() << "标准异常:" << QString::fromStdString(e.what());
            emit error(QString::fromStdString(e.what()));
        } catch (...) {
            qDebug() << "未知异常发生在板卡识别过程中";
            emit error("识别过程中发生未知错误");
        }
    }

signals:
    void finished(const QList<PCBBoardInfo>& candidates);
    void error(const QString& errorMessage);

private:
    PCBBoardManager* board_manager_;
    cv::Mat input_image_;
    int maxnumb_;
};

#endif // IDENTIFICATIONWORKER_H
