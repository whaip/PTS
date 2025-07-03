#include "pcbboardmanager.h"
#include <QStandardPaths>
#include <QUuid>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QDebug>
#include <QTimer>
#include <QApplication>
#include <opencv2/features2d.hpp>
#include <opencv2/calib3d.hpp>
#include "PCB_Components_Detect/yolomodel.h"

// PCBBoardInfo JSON序列化实现
QJsonObject PCBBoardInfo::toJson() const {
    QJsonObject obj;
    obj["boardId"] = boardId;
    obj["boardName"] = boardName;
    obj["boardModel"] = boardModel;
    obj["description"] = description;
    obj["imagePath"] = imagePath;
    obj["templatePath"] = templatePath;
    obj["createTime"] = createTime.toString(Qt::ISODate);
    obj["updateTime"] = updateTime.toString(Qt::ISODate);
    obj["isActive"] = isActive;
    
    // 序列化元器件列表
    QJsonArray componentsArray;
    for (const auto& component : components) {
        QJsonObject compObj;
        compObj["id"] = component.id;
        compObj["x"] = component.x;
        compObj["y"] = component.y;
        compObj["w"] = component.w;
        compObj["h"] = component.h;
        compObj["cls"] = component.cls;
        compObj["confidence"] = component.confidence;
        compObj["label"] = component.label;
        compObj["position_number"] = component.position_number;
        compObj["notes"] = QString::fromUtf8(component.notes);
        componentsArray.append(compObj);
    }
    obj["components"] = componentsArray;
    
    return obj;
}

PCBBoardInfo PCBBoardInfo::fromJson(const QJsonObject& json) {
    PCBBoardInfo board;
    board.boardId = json["boardId"].toString();
    board.boardName = json["boardName"].toString();
    board.boardModel = json["boardModel"].toString();
    board.description = json["description"].toString();
    board.imagePath = json["imagePath"].toString();
    board.templatePath = json["templatePath"].toString();
    board.createTime = QDateTime::fromString(json["createTime"].toString(), Qt::ISODate);
    board.updateTime = QDateTime::fromString(json["updateTime"].toString(), Qt::ISODate);
    board.isActive = json["isActive"].toBool();
    
    // 反序列化元器件列表
    QJsonArray componentsArray = json["components"].toArray();
    for (const auto& value : componentsArray) {
        QJsonObject compObj = value.toObject();
        Label component;
        component.id = compObj["id"].toInt();
        component.x = compObj["x"].toInt();
        component.y = compObj["y"].toInt();
        component.w = compObj["w"].toInt();
        component.h = compObj["h"].toInt();
        component.cls = compObj["cls"].toInt();
        component.confidence = compObj["confidence"].toDouble();
        component.label = compObj["label"].toString();
        component.position_number = compObj["position_number"].toString();
        component.notes = compObj["notes"].toString().toUtf8();
        board.components.push_back(component);
    }
    
    return board;
}

// ComponentInfo JSON序列化实现
QJsonObject ComponentInfo::toJson() const {
    QJsonObject obj;
    
    // 基础标签信息
    obj["id"] = labelInfo.id;
    obj["x"] = labelInfo.x;
    obj["y"] = labelInfo.y;
    obj["w"] = labelInfo.w;
    obj["h"] = labelInfo.h;
    obj["cls"] = labelInfo.cls;
    obj["confidence"] = labelInfo.confidence;
    obj["label"] = labelInfo.label;
    obj["position_number"] = labelInfo.position_number;
    obj["notes"] = QString::fromUtf8(labelInfo.notes);
    
    // 扩展信息
    obj["componentName"] = componentName;
    obj["componentValue"] = componentValue;
    obj["componentPackage"] = componentPackage;
    obj["manufacturer"] = manufacturer;
    obj["partNumber"] = partNumber;
    obj["description"] = description;
    obj["isRequired"] = isRequired;
    
    return obj;
}

ComponentInfo ComponentInfo::fromJson(const QJsonObject& json) {
    ComponentInfo component;
    
    // 基础标签信息
    component.labelInfo.id = json["id"].toInt();
    component.labelInfo.x = json["x"].toInt();
    component.labelInfo.y = json["y"].toInt();
    component.labelInfo.w = json["w"].toInt();
    component.labelInfo.h = json["h"].toInt();
    component.labelInfo.cls = json["cls"].toInt();
    component.labelInfo.confidence = json["confidence"].toDouble();
    component.labelInfo.label = json["label"].toString();
    component.labelInfo.position_number = json["position_number"].toString();
    component.labelInfo.notes = json["notes"].toString().toUtf8();
    
    // 扩展信息
    component.componentName = json["componentName"].toString();
    component.componentValue = json["componentValue"].toString();
    component.componentPackage = json["componentPackage"].toString();
    component.manufacturer = json["manufacturer"].toString();
    component.partNumber = json["partNumber"].toString();
    component.description = json["description"].toString();
    component.isRequired = json["isRequired"].toBool();
    return component;
}

PCBBoardManager::PCBBoardManager(QObject *parent)
    : QObject(parent)
    , identification_watcher_(nullptr)
    , detection_watcher_(nullptr)
    , creation_watcher_(nullptr)
    , identification_cancelled_(false)
    , detection_cancelled_(false)
    , creation_cancelled_(false)
{    // 设置默认路径 - 使用PCBimage文件夹以保持一致性
    QString documentsPath = QDir::currentPath() + "/../..";
    image_storage_path_ = documentsPath+ "/PCBimage";
    features_storage_path_ = documentsPath + "/PCBimage/features";
    database_path_ = documentsPath + "/PCBimage/PCB_Boards_Database.json";
    
    // 确保目录存在
    ensureDirectoryExists(image_storage_path_);
    ensureDirectoryExists(features_storage_path_);
    
    // 初始化YOLO模型 
    yolo_model_ = nullptr;
    try {
        yolo_model_ = YOLOModel::getInstance();
        if (yolo_model_) {
            qDebug() << "PCBBoardManager: YOLO模型初始化成功";
        } else {
            qDebug() << "PCBBoardManager: YOLO模型实例创建失败";
        }
    } catch (const std::exception& e) {
        qDebug() << "PCBBoardManager: YOLO模型初始化失败:" << e.what();
        yolo_model_ = nullptr;
    } catch (...) {
        qDebug() << "PCBBoardManager: YOLO模型初始化发生未知异常";
        yolo_model_ = nullptr;
    }
    
    // 初始化多线程监视器
    initializeWatchers();
}

PCBBoardManager::~PCBBoardManager() {
    cleanupWatchers();
    saveBoardsToDatabase();
}

bool PCBBoardManager::initializeDatabase(const QString& databasePath) {
    if (!databasePath.isEmpty()) {
        database_path_ = databasePath;
    }
    
    QFileInfo fileInfo(database_path_);
    if (!ensureDirectoryExists(fileInfo.absolutePath())) {
        emit databaseError("无法创建数据库目录");
        return false;
    }
    
    loadBoardsFromDatabase();
    return true;
}

QString PCBBoardManager::createBoard(const QString& boardName, const QString& boardModel, 
                                   const cv::Mat& boardImage, const QString& description) {
    qDebug() << "PCBBoardManager::createBoard - 开始创建板卡:" << boardName << "模型:" << boardModel;
    
    if (boardImage.empty()) {
        qDebug() << "PCBBoardManager::createBoard - 图片为空";
        return QString();
    }
    
    QMutexLocker locker(&boards_mutex_);
    
    PCBBoardInfo board;
    board.boardId = generateBoardId();
    board.boardName = boardName;
    board.boardModel = boardModel;
    board.description = description;
    board.createTime = QDateTime::currentDateTime();
    board.updateTime = board.createTime;
    board.isActive = true;
    
    qDebug() << "PCBBoardManager::createBoard - 生成板卡ID:" << board.boardId;
    
    // 保存板卡图片
    board.imagePath = saveImage(boardImage, QString("board_%1").arg(board.boardId));
    if (board.imagePath.isEmpty()) {
        qDebug() << "PCBBoardManager::createBoard - 图片保存失败";
        return QString();
    }
      // 创建模板图片（可以与原图相同，或进行预处理）
    board.templatePath = board.imagePath;
    
    // 自动检测元器件
    qDebug() << "PCBBoardManager::createBoard - 开始自动检测元器件";
    board.components = detectComponents(boardImage);
    qDebug() << "PCBBoardManager::createBoard - 检测到" << board.components.size() << "个元器件";
    
    boards_.append(board);
    // 将新板卡图像加入 SIFT 特征数据库
    try {
        SIFT_MATCHER->appendToDatabase({ board.boardId.toStdString() }, { cv::imread(board.imagePath.toStdString())});
        qDebug() << "PCBBoardManager: 已将新板卡图像添加到特征数据库:" << board.imagePath;
    } catch (const std::exception& e) {
        qDebug() << "PCBBoardManager: 特征数据库追加失败:" << e.what();
    }
    qDebug() << "PCBBoardManager::createBoard - 板卡创建成功，ID:" << board.boardId;
    locker.unlock();
    emit boardAdded(board.boardId);
    return board.boardId;
}

bool PCBBoardManager::updateBoard(const PCBBoardInfo& boardInfo) {
    QMutexLocker locker(&boards_mutex_);
    
    for (int i = 0; i < boards_.size(); ++i) {
        if (boards_[i].boardId == boardInfo.boardId) {
            PCBBoardInfo updatedBoard = boardInfo;
            updatedBoard.updateTime = QDateTime::currentDateTime();
            boards_[i] = updatedBoard;
            
            // 发射信号需要在mutex外执行
            locker.unlock();
            emit boardUpdated(boardInfo.boardId);
            return true;
        }
    }
    
    return false;
}

bool PCBBoardManager::deleteBoard(const QString& boardId) {
    QMutexLocker locker(&boards_mutex_);
    
    for (int i = 0; i < boards_.size(); ++i) {
        if (boards_[i].boardId == boardId) {
            deleteImageFiles(boardId);
            boards_.removeAt(i);
            
            // 发射信号需要在mutex外执行
            locker.unlock();
            emit boardDeleted(boardId);
            return true;
        }
    }
    
    return false;
}

PCBBoardInfo PCBBoardManager::getBoardById(const QString& boardId) const {
    for (const auto& board : boards_) {
        if (board.boardId == boardId) {
            return board;
        }
    }
    
    return PCBBoardInfo();
}

QList<PCBBoardInfo> PCBBoardManager::getAllBoards() const {
    QMutexLocker locker(&boards_mutex_);
    return boards_;
}

QList<PCBBoardInfo> PCBBoardManager::getBoardsByModel(const QString& model) const {
    QMutexLocker locker(&boards_mutex_);
    
    QList<PCBBoardInfo> result;
    for (const auto& board : boards_) {
        if (board.boardModel == model && board.isActive) {
            result.append(board);
        }
    }
    
    return result;
}

QStringList PCBBoardManager::getAllBoardModels() const {
    QMutexLocker locker(&boards_mutex_);
    
    QStringList models;
    for (const auto& board : boards_) {
        if (board.isActive && !models.contains(board.boardModel)) {
            models.append(board.boardModel);
        }
    }
    
    return models;
}

bool PCBBoardManager::addComponent(const QString& boardId, const ComponentInfo& component) {
    QMutexLocker locker(&boards_mutex_);
    
    for (int i = 0; i < boards_.size(); ++i) {
        if (boards_[i].boardId == boardId) {
            Label newLabel = component.labelInfo;
            newLabel.id = generateComponentId(boardId);
            boards_[i].components.push_back(newLabel);
            boards_[i].updateTime = QDateTime::currentDateTime();
            
            // 发射信号需要在mutex外执行
            locker.unlock();
            emit componentAdded(boardId, newLabel.id);
            return true;
        }
    }
    
    return false;
}

bool PCBBoardManager::updateComponent(const QString& boardId, const ComponentInfo& component) {
    QMutexLocker locker(&boards_mutex_);
    
    for (int i = 0; i < boards_.size(); ++i) {
        if (boards_[i].boardId == boardId) {
            for (auto& comp : boards_[i].components) {
                if (comp.id == component.labelInfo.id) {
                    comp = component.labelInfo;
                    boards_[i].updateTime = QDateTime::currentDateTime();
                    
                    // 发射信号需要在mutex外执行
                    locker.unlock();
                    emit componentUpdated(boardId, component.labelInfo.id);
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool PCBBoardManager::removeComponent(const QString& boardId, int componentId) {
    QMutexLocker locker(&boards_mutex_);
    
    for (int i = 0; i < boards_.size(); ++i) {
        if (boards_[i].boardId == boardId) {
            auto& components = boards_[i].components;
            for (auto it = components.begin(); it != components.end(); ++it) {
                if (it->id == componentId) {
                    components.erase(it);
                    boards_[i].updateTime = QDateTime::currentDateTime();
                    
                    // 发射信号需要在mutex外执行
                    locker.unlock();
                    emit componentRemoved(boardId, componentId);
                    return true;
                }
            }
        }
    }
    
    return false;
}

QList<ComponentInfo> PCBBoardManager::getComponents(const QString& boardId) const {
    QMutexLocker locker(&boards_mutex_);
    
    QList<ComponentInfo> result;
    for (const auto& board : boards_) {
        if (board.boardId == boardId) {
            for (const auto& label : board.components) {
                ComponentInfo component(label);
                component.componentName = label.label.isEmpty() ? QString("Component_%1").arg(label.id) : label.label;
                result.append(component);
            }
            break;
        }
    }
    
    return result;
}

ComponentInfo PCBBoardManager::getComponent(const QString& boardId, int componentId) const {
    QMutexLocker locker(&boards_mutex_);
    
    for (const auto& board : boards_) {
        if (board.boardId == boardId) {
            for (const auto& label : board.components) {
                if (label.id == componentId) {
                    return ComponentInfo(label);
                }
            }
            break;
        }
    }
    
    return ComponentInfo();
}

QList<PCBBoardInfo> PCBBoardManager::identifyBoard(const cv::Mat& inputImage, double threshold) {
    if (inputImage.empty()) {
        return QList<PCBBoardInfo>();
    }
    QMutexLocker locker(&boards_mutex_);
    // 使用 SIFT_MATCHER 识别并匹配已知板卡图像
    std::vector<SiftMatcher::MatchResult> matchResults = SIFT_MATCHER->matchImage(inputImage);
    QList<PCBBoardInfo> result;
    for (const auto& mr : matchResults) {
        // 阈值过滤
        if (mr.matchScore < threshold) break;
        QString id = QString::fromStdString(mr.boardId);
        // 在已加载板卡中查找匹配的图像路径
        for (const auto& board : boards_) {
            if (board.boardId == id) {
                result.append(board);
                break;
            }
        }
    }
    return result;
}

PCBBoardInfo PCBBoardManager::getBestMatch(const cv::Mat& inputImage, double threshold) {
    QList<PCBBoardInfo> matches = identifyBoard(inputImage, threshold);
    return matches.isEmpty() ? PCBBoardInfo() : matches.first();
}

std::vector<Label> PCBBoardManager::detectComponents(const cv::Mat& boardImage) {
    std::vector<Label> components;
    
    if (boardImage.empty()) {
        qDebug() << "PCBBoardManager::detectComponents - 输入图像为空";
        return components;
    }
    
    if (!yolo_model_) {
        qDebug() << "PCBBoardManager::detectComponents - YOLO模型未初始化";
        return components;
    }
    
    try {
        qDebug() << "PCBBoardManager::detectComponents - 开始检测元器件";
        
        // 创建图像副本以避免潜在的内存问题
        cv::Mat imageCopy = boardImage.clone();
        
        // 使用YOLO模型进行检测
        cv::Mat annotatedImage = yolo_model_->recognize(imageCopy);
        components = yolo_model_->get_labels();
        
        qDebug() << "PCBBoardManager::detectComponents - 检测到" << components.size() << "个元器件";
        
        // 为每个检测到的元器件分配唯一ID
        for (size_t i = 0; i < components.size(); ++i) {
            components[i].id = static_cast<int>(i + 1);
        }
        
    } catch (const cv::Exception& e) {
        qDebug() << "PCBBoardManager::detectComponents - OpenCV异常:" << e.what();
        components.clear();
    } catch (const std::exception& e) {
        qDebug() << "PCBBoardManager::detectComponents - 检测过程中发生异常:" << e.what();
        components.clear();
    } catch (...) {
        qDebug() << "PCBBoardManager::detectComponents - 发生未知异常";
        components.clear();
    }
    
    return components;
}

cv::Mat PCBBoardManager::createAnnotatedImage(const cv::Mat& originalImage, const QList<ComponentInfo>& components) {
    cv::Mat result = originalImage.clone();
    
    for (const auto& component : components) {
        const Label& label = component.labelInfo;
        
        // 绘制边界框
        cv::Scalar color(0, 255, 0); // 绿色
        cv::rectangle(result, cv::Point(label.x, label.y), 
                     cv::Point(label.x + label.w, label.y + label.h), color, 2);
        
        // 绘制标签
        QString labelText = component.componentName.isEmpty() ? label.label : component.componentName;
        cv::putText(result, labelText.toStdString(), cv::Point(label.x, label.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
    }
    
    return result;
}

QString PCBBoardManager::saveImage(const cv::Mat& image, const QString& prefix) {
    qDebug() << "PCBBoardManager::saveImage - 开始保存图片，前缀:" << prefix;
    
    if (image.empty()) {
        qDebug() << "PCBBoardManager::saveImage - 图片为空";
        return QString();
    }
    
    QString filename = createImageFileName(prefix);
    QString fullPath = image_storage_path_ + "/" + filename;
    
    // 确保目录存在
    if (!ensureDirectoryExists(image_storage_path_)) {
        qDebug() << "PCBBoardManager::saveImage - 无法创建目录:" << image_storage_path_;
        return QString();
    }
    
    if (cv::imwrite(fullPath.toStdString(), image)) {
        qDebug() << "PCBBoardManager::saveImage - 图片保存成功:" << fullPath;
        return fullPath;
    }
    
    qDebug() << "PCBBoardManager::saveImage - 图片保存失败";
    return QString();
}

bool PCBBoardManager::deleteImageFiles(const QString& boardId) {
    PCBBoardInfo board = getBoardById(boardId);
    if (board.boardId.isEmpty()) {
        return false;
    }
    
    bool success = true;
    
    if (!board.imagePath.isEmpty() && QFile::exists(board.imagePath)) {
        success &= QFile::remove(board.imagePath);
    }
    
    if (!board.templatePath.isEmpty() && QFile::exists(board.templatePath) 
        && board.templatePath != board.imagePath) {
        success &= QFile::remove(board.templatePath);
    }
    
    return success;
}

cv::Mat PCBBoardManager::loadImage(const QString& imagePath) const {
    qDebug() << "PCBBoardManager::loadImage - 尝试加载图片:" << imagePath;
    
    if (imagePath.isEmpty()) {
        qDebug() << "PCBBoardManager::loadImage - 图片路径为空";
        return cv::Mat();
    }
    
    if (!QFile::exists(imagePath)) {
        qDebug() << "PCBBoardManager::loadImage - 图片文件不存在:" << imagePath;
        return cv::Mat();
    }
    
    cv::Mat image = cv::imread(imagePath.toStdString());
    if (image.empty()) {
        qDebug() << "PCBBoardManager::loadImage - cv::imread加载失败:" << imagePath;
    } else {
        qDebug() << "PCBBoardManager::loadImage - 图片加载成功，尺寸:" << image.cols << "x" << image.rows;
    }
    
    return image;
}

bool PCBBoardManager::exportBoard(const QString& boardId, const QString& filePath) {
    PCBBoardInfo board = getBoardById(boardId);
    if (board.boardId.isEmpty()) {
        return false;
    }
    
    QJsonObject boardObj = board.toJson();
    QJsonDocument doc(boardObj);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    return true;
}

bool PCBBoardManager::importBoard(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    PCBBoardInfo board = PCBBoardInfo::fromJson(doc.object());
    
    if (!board.boardId.isEmpty()) {
        {
            QMutexLocker locker(&boards_mutex_);
            boards_.append(board);
        }
        emit boardAdded(board.boardId);
        return true;
    }
    
    return false;
}

bool PCBBoardManager::exportAllBoards(const QString& filePath) {
    QMutexLocker locker(&boards_mutex_);
    
    QJsonArray boardsArray;
    for (const auto& board : boards_) {
        boardsArray.append(board.toJson());
    }
    
    QJsonObject rootObj;
    rootObj["version"] = "1.0";
    rootObj["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObj["boardCount"] = boardsArray.size();
    rootObj["boards"] = boardsArray;
    
    QJsonDocument doc(rootObj);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    return true;
}

int PCBBoardManager::getTotalBoardsCount() const {
    QMutexLocker locker(&boards_mutex_);
    return boards_.size();
}

int PCBBoardManager::getTotalComponentsCount() const {
    QMutexLocker locker(&boards_mutex_);
    
    int total = 0;
    for (const auto& board : boards_) {
        total += board.components.size();
    }
    
    return total;
}

QMap<QString, int> PCBBoardManager::getComponentTypeStatistics() const {
    QMutexLocker locker(&boards_mutex_);
    
    QMap<QString, int> stats;
    for (const auto& board : boards_) {
        for (const auto& component : board.components) {
            stats[component.label]++;
        }
    }
    
    return stats;
}

// 私有方法实现
QString PCBBoardManager::generateBoardId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

int PCBBoardManager::generateComponentId(const QString& boardId) const {
    PCBBoardInfo board = getBoardById(boardId);
    if (board.boardId.isEmpty()) {
        return 1;
    }
    
    int maxId = 0;
    for (const auto& component : board.components) {
        maxId = qMax(maxId, component.id);
    }
    
    return maxId + 1;
}

bool PCBBoardManager::ensureDirectoryExists(const QString& path) const {
    QDir dir;
    return dir.mkpath(path);
}

void PCBBoardManager::loadBoardsFromDatabase() {
    QFile file(database_path_);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject rootObj = doc.object();
    
    QJsonArray boardsArray = rootObj["boards"].toArray();
    
    QMutexLocker locker(&boards_mutex_);
    boards_.clear();
    
    for (const auto& value : boardsArray) {
        PCBBoardInfo board = PCBBoardInfo::fromJson(value.toObject());
        if (!board.boardId.isEmpty()) {
            boards_.append(board);
        }
    }
}

bool PCBBoardManager::saveBoardsToDatabase() {
    QMutexLocker locker(&boards_mutex_);
    
    QJsonArray boardsArray;
    for (const auto& board : boards_) {
        boardsArray.append(board.toJson());
    }
    
    QJsonObject rootObj;
    rootObj["version"] = "1.0";
    rootObj["saveTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObj["boardCount"] = boardsArray.size();
    rootObj["boards"] = boardsArray;
    
    QJsonDocument doc(rootObj);
    
    QFile file(database_path_);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        return true;
    }
    
    return false;
}

QString PCBBoardManager::createImageFileName(const QString& prefix, const QString& extension) const {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    return QString("%1_%2%3").arg(prefix).arg(timestamp).arg(extension);
}


void PCBBoardManager::initializeWatchers()
{
    // 初始化板卡识别监视器
    identification_watcher_ = new QFutureWatcher<QList<PCBBoardInfo>>(this);
    connect(identification_watcher_, &QFutureWatcher<QList<PCBBoardInfo>>::started,
            this, &PCBBoardManager::boardIdentificationStarted);
    connect(identification_watcher_, &QFutureWatcher<QList<PCBBoardInfo>>::finished,
            this, [this]() {
                if (!identification_cancelled_) {
                    QList<PCBBoardInfo> result = identification_watcher_->result();
                    for (auto& board : result) {
                        auto it = std::find_if(boards_.begin(), boards_.end(), [board](const PCBBoardInfo& b) {
                            return b.imagePath == board.imagePath;
                        });
                        if (it != boards_.end()) {
                            board = *it;
                        }
                    }
                    emit boardIdentificationProgress(100, "识别完成");
                    emit boardIdentificationFinished(result);
                } else {
                    emit boardIdentificationProgress(100, "识别已取消");
                }
                identification_cancelled_ = false;
            });
    
    // 初始化元器件检测监视器
    detection_watcher_ = new QFutureWatcher<std::vector<Label>>(this);
    connect(detection_watcher_, &QFutureWatcher<std::vector<Label>>::started,
            this, &PCBBoardManager::componentDetectionStarted);
    connect(detection_watcher_, &QFutureWatcher<std::vector<Label>>::finished,
            this, [this]() {
                if (!detection_cancelled_) {
                    std::vector<Label> result = detection_watcher_->result();
                    emit componentDetectionProgress(100, "检测完成");
                    emit componentDetectionFinished(result, "");
                } else {
                    emit componentDetectionProgress(100, "检测已取消");
                }
                detection_cancelled_ = false;
            });
    
    // 初始化板卡创建监视器
    creation_watcher_ = new QFutureWatcher<QString>(this);
    connect(creation_watcher_, &QFutureWatcher<QString>::started,
            this, &PCBBoardManager::boardCreationStarted);
    connect(creation_watcher_, &QFutureWatcher<QString>::finished,
            this, [this]() {
                if (!creation_cancelled_) {
                    QString result = creation_watcher_->result();
                    emit boardCreationProgress(100, "创建完成");
                    emit boardCreationFinished(result);
                } else {
                    emit boardCreationProgress(100, "创建已取消");
                }
                creation_cancelled_ = false;
            });
    
    qDebug() << "PCBBoardManager: 多线程监视器初始化完成";
}

void PCBBoardManager::cleanupWatchers()
{
    if (identification_watcher_) {
        if (identification_watcher_->isRunning()) {
            identification_watcher_->cancel();
            identification_watcher_->waitForFinished();
        }
        identification_watcher_->deleteLater();
        identification_watcher_ = nullptr;
    }
    
    if (detection_watcher_) {
        if (detection_watcher_->isRunning()) {
            detection_watcher_->cancel();
            detection_watcher_->waitForFinished();
        }
        detection_watcher_->deleteLater();
        detection_watcher_ = nullptr;
    }
    
    if (creation_watcher_) {
        if (creation_watcher_->isRunning()) {
            creation_watcher_->cancel();
            creation_watcher_->waitForFinished();
        }
        creation_watcher_->deleteLater();
        creation_watcher_ = nullptr;
    }
    
    qDebug() << "PCBBoardManager: 多线程监视器清理完成";
}

void PCBBoardManager::identifyBoardAsync(const cv::Mat& inputImage, double threshold)
{
    if (identification_watcher_ && identification_watcher_->isRunning()) {
        qDebug() << "PCBBoardManager::identifyBoardAsync - 识别任务正在进行中";
        return;
    }
    
    qDebug() << "PCBBoardManager::identifyBoardAsync - 开始异步板卡识别";
    
    // 重置取消标志
    {
        QMutexLocker locker(&cancel_mutex_);
        identification_cancelled_ = false;
    }
    
    // 创建进度报告定时器
    QTimer* progressTimer = new QTimer(this);
    progressTimer->setInterval(300);
    
    // 进度报告
    int progressValue = 20;
    connect(progressTimer, &QTimer::timeout, [this, progressTimer, &progressValue]() {
        if (identification_cancelled_) {
            progressTimer->stop();
            progressTimer->deleteLater();
            return;
        }
        
        progressValue = qMin(progressValue + 10, 90);
        emit boardIdentificationProgress(progressValue, "正在分析图像特征...");
        
        if (progressValue >= 90) {
            progressTimer->stop();
        }
    });
    
    // 启动定时器
    progressTimer->start();
    
    // 在清理定时器的连接中添加自动删除
    connect(identification_watcher_, &QFutureWatcher<QList<PCBBoardInfo>>::finished,
            progressTimer, [progressTimer]() {
                progressTimer->stop();
                progressTimer->deleteLater();
            });
    
    // 启动异步任务
    auto future = QtConcurrent::run([this, inputImage, threshold]() -> QList<PCBBoardInfo> {
        try {
            // 检查是否取消
            {
                QMutexLocker locker(&cancel_mutex_);
                if (identification_cancelled_) {
                    return QList<PCBBoardInfo>();
                }
            }
            
            qDebug() << "开始异步板卡识别，图片尺寸:" << inputImage.cols << "x" << inputImage.rows;
            
            std::vector<SiftMatcher::MatchResult> matchResults = SIFT_MATCHER->matchImage(inputImage);
            
            // 再次检查是否取消
            {
                QMutexLocker locker(&cancel_mutex_);
                if (identification_cancelled_) {
                    return QList<PCBBoardInfo>();
                }
            }
            QList<PCBBoardInfo> result;
            for (const auto& match : matchResults) {
                result.append(getBoardById(QString::fromStdString(match.boardId)));
            }
            
            qDebug() << "异步板卡识别完成，找到" << result.size() << "个候选项";
            return result;
            
        } catch (const cv::Exception& e) {
            qDebug() << "异步板卡识别OpenCV异常:" << QString::fromStdString(e.what());
            emit boardIdentificationError(QString("图像处理错误: %1").arg(QString::fromStdString(e.what())));
            return QList<PCBBoardInfo>();
        } catch (const std::exception& e) {
            qDebug() << "异步板卡识别标准异常:" << QString::fromStdString(e.what());
            emit boardIdentificationError(QString::fromStdString(e.what()));
            return QList<PCBBoardInfo>();
        } catch (...) {
            qDebug() << "异步板卡识别未知异常";
            emit boardIdentificationError("识别过程中发生未知错误");
            return QList<PCBBoardInfo>();
        }
    });
    
    identification_watcher_->setFuture(future);
    
    emit boardIdentificationProgress(20, "开始板卡识别...");
}

void PCBBoardManager::cancelBoardIdentification()
{
    QMutexLocker locker(&cancel_mutex_);
    identification_cancelled_ = true;
    
    if (identification_watcher_ && identification_watcher_->isRunning()) {
        identification_watcher_->cancel();
        qDebug() << "PCBBoardManager::cancelBoardIdentification - 已取消板卡识别任务";
    }
}

void PCBBoardManager::detectComponentsAsync(const cv::Mat& boardImage, const QString& boardId)
{
    if (detection_watcher_ && detection_watcher_->isRunning()) {
        qDebug() << "PCBBoardManager::detectComponentsAsync - 检测任务正在进行中";
        return;
    }
    
    qDebug() << "PCBBoardManager::detectComponentsAsync - 开始异步元器件检测";
    
    // 重置取消标志
    {
        QMutexLocker locker(&cancel_mutex_);
        detection_cancelled_ = false;
    }
    
    // 创建进度报告定时器
    QTimer* progressTimer = new QTimer(this);
    progressTimer->setInterval(400);
    
    // 进度报告
    int progressValue = 15;
    connect(progressTimer, &QTimer::timeout, [this, progressTimer, &progressValue]() {
        if (detection_cancelled_) {
            progressTimer->stop();
            progressTimer->deleteLater();
            return;
        }
        
        progressValue = qMin(progressValue + 12, 85);
        emit componentDetectionProgress(progressValue, "正在检测元器件...");
        
        if (progressValue >= 85) {
            progressTimer->stop();
        }
    });
    
    // 启动定时器
    progressTimer->start();
    
    // 在清理定时器的连接中添加自动删除
    connect(detection_watcher_, &QFutureWatcher<std::vector<Label>>::finished,
            progressTimer, [progressTimer]() {
                progressTimer->stop();
                progressTimer->deleteLater();
            });
    
    // 启动异步任务
    auto future = QtConcurrent::run([this, boardImage, boardId]() -> std::vector<Label> {
        try {
            // 检查是否取消
            {
                QMutexLocker locker(&cancel_mutex_);
                if (detection_cancelled_) {
                    return std::vector<Label>();
                }
            }
            
            qDebug() << "开始异步元器件检测，图片尺寸:" << boardImage.cols << "x" << boardImage.rows;
            
            std::vector<Label> result = detectComponents(boardImage);
            
            // 再次检查是否取消
            {
                QMutexLocker locker(&cancel_mutex_);
                if (detection_cancelled_) {
                    return std::vector<Label>();
                }
            }
            
            qDebug() << "异步元器件检测完成，检测到" << result.size() << "个元器件";
            return result;
            
        } catch (const cv::Exception& e) {
            qDebug() << "异步元器件检测OpenCV异常:" << QString::fromStdString(e.what());
            emit componentDetectionError(QString("图像处理错误: %1").arg(QString::fromStdString(e.what())));
            return std::vector<Label>();
        } catch (const std::exception& e) {
            qDebug() << "异步元器件检测标准异常:" << QString::fromStdString(e.what());
            emit componentDetectionError(QString::fromStdString(e.what()));
            return std::vector<Label>();
        } catch (...) {
            qDebug() << "异步元器件检测未知异常";
            emit componentDetectionError("检测过程中发生未知错误");
            return std::vector<Label>();
        }
    });
    
    detection_watcher_->setFuture(future);
    
    emit componentDetectionProgress(15, "开始元器件检测...");
}

void PCBBoardManager::cancelComponentDetection()
{
    QMutexLocker locker(&cancel_mutex_);
    detection_cancelled_ = true;
    
    if (detection_watcher_ && detection_watcher_->isRunning()) {
        detection_watcher_->cancel();
        qDebug() << "PCBBoardManager::cancelComponentDetection - 已取消元器件检测任务";
    }
}

void PCBBoardManager::createBoardAsync(const QString& boardName, const QString& boardModel, 
                                          const cv::Mat& boardImage, const QString& description)
{
    if (creation_watcher_ && creation_watcher_->isRunning()) {
        qDebug() << "PCBBoardManager::createBoardAsync - 创建任务正在进行中";
        return;
    }

    // 重置取消标志
    {
        QMutexLocker locker(&cancel_mutex_);
        creation_cancelled_ = false;
    }

    // 创建进度报告定时器
    QTimer* progressTimer = new QTimer(this);
    progressTimer->setInterval(500);

    // 进度报告
    int progressValue = 20;
    connect(progressTimer, &QTimer::timeout, [this, progressTimer, &progressValue]() {
        if (creation_cancelled_) {
            progressTimer->stop();
            progressTimer->deleteLater();
            return;
        }
        progressValue = qMin(progressValue + 8, 80);
        emit boardCreationProgress(progressValue, "正在创建板卡...");
        if (progressValue >= 80) {
            progressTimer->stop();
        }
    });

    // 启动定时器
    progressTimer->start();

    // 清理定时器连接
    connect(creation_watcher_, &QFutureWatcher<QString>::finished,
            progressTimer, [progressTimer]() {
                progressTimer->stop();
                progressTimer->deleteLater();
            });

    // 启动异步任务
    auto future = QtConcurrent::run([this, boardName, boardModel, boardImage, description]() -> QString {
        try {
            // 检查是否取消
            {
                QMutexLocker locker(&cancel_mutex_);
                if (creation_cancelled_) {
                    return QString();
                }
            }

            qDebug() << "开始异步板卡创建，图片尺寸:" << boardImage.cols << "x" << boardImage.rows;
            QString result = createBoard(boardName, boardModel, boardImage, description);

            // 再次检查是否取消
            {
                QMutexLocker locker(&cancel_mutex_);
                if (creation_cancelled_) {
                    return QString();
                }
            }

            qDebug() << "异步板卡创建完成，ID:" << result;
            return result;
        } catch (const cv::Exception& e) {
            qDebug() << "异步板卡创建OpenCV异常:" << QString::fromStdString(e.what());
            emit boardCreationError(QString("图像处理错误: %1").arg(QString::fromStdString(e.what())));
            return QString();
        } catch (const std::exception& e) {
            qDebug() << "异步板卡创建标准异常:" << QString::fromStdString(e.what());
            emit boardCreationError(QString::fromStdString(e.what()));
            return QString();
        } catch (...) {
            qDebug() << "异步板卡创建未知异常";
            emit boardCreationError("创建过程中发生未知错误");
            return QString();
        }
    });

    creation_watcher_->setFuture(future);
    emit boardCreationProgress(20, "开始板卡创建...");
}

void PCBBoardManager::cancelBoardCreation()
{
    QMutexLocker locker(&cancel_mutex_);
    creation_cancelled_ = true;
    
    if (creation_watcher_ && creation_watcher_->isRunning()) {
        creation_watcher_->cancel();
        qDebug() << "PCBBoardManager::cancelBoardCreation - 已取消板卡创建任务";
    }
}
