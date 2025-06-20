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

//=============================================================================
// ImageFeatureInfo 实现
//=============================================================================

std::vector<cv::KeyPoint> ImageFeatureInfo::getCvKeypoints() const {
    std::vector<cv::KeyPoint> cvKeypoints;
    cvKeypoints.reserve(keypoints.size());
    
    for (const auto& kp : keypoints) {
        cvKeypoints.push_back(kp.toCvKeyPoint());
    }
    
    return cvKeypoints;
}

cv::Mat ImageFeatureInfo::getCvDescriptors() const {
    if (descriptors.empty() || descriptorRows == 0 || descriptorCols == 0) {
        return cv::Mat();
    }
    
    cv::Mat desc(descriptorRows, descriptorCols, CV_32F);
    std::memcpy(desc.data, descriptors.data(), descriptors.size() * sizeof(float));
    
    return desc;
}

void ImageFeatureInfo::setFromCv(const std::vector<cv::KeyPoint>& kps, const cv::Mat& desc) {
    // 设置特征点
    keypoints.clear();
    keypoints.reserve(kps.size());
    for (const auto& kp : kps) {
        keypoints.emplace_back(kp);
    }
    
    // 设置描述符
    if (!desc.empty()) {
        descriptorRows = desc.rows;
        descriptorCols = desc.cols;
        
        // 确保数据类型为float
        cv::Mat floatDesc;
        if (desc.type() != CV_32F) {
            desc.convertTo(floatDesc, CV_32F);
        } else {
            floatDesc = desc;
        }
        
        // 复制数据
        descriptors.resize(floatDesc.rows * floatDesc.cols);
        std::memcpy(descriptors.data(), floatDesc.data, descriptors.size() * sizeof(float));
    } else {
        descriptorRows = 0;
        descriptorCols = 0;
        descriptors.clear();
    }
    
    extractTime = QDateTime::currentDateTime();
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
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    image_storage_path_ = documentsPath + "/PCBimage";
    features_storage_path_ = documentsPath + "/PCBimage/features";
    database_path_ = documentsPath + "/PCBimage/PCB_Boards_Database.json";
    
    // 确保目录存在
    ensureDirectoryExists(image_storage_path_);
    ensureDirectoryExists(features_storage_path_);
      // 初始化OpenCV特征检测器
    try {
        sift_detector_ = cv::SIFT::create();
        matcher_ = cv::BFMatcher::create();
        qDebug() << "PCBBoardManager: OpenCV特征检测器初始化成功";
    } catch (const cv::Exception& e) {
        qDebug() << "PCBBoardManager: OpenCV特征检测器初始化失败:" << e.what();
        sift_detector_ = nullptr;
        matcher_ = nullptr;
    }
    
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
    
    qDebug() << "PCBBoardManager::getComponents - 查找板卡ID:" << boardId;
    qDebug() << "PCBBoardManager::getComponents - 总板卡数量:" << boards_.size();
    
    QList<ComponentInfo> result;
    for (const auto& board : boards_) {
        qDebug() << "PCBBoardManager::getComponents - 检查板卡:" << board.boardId << "元器件数量:" << board.components.size();
        if (board.boardId == boardId) {
            qDebug() << "PCBBoardManager::getComponents - 找到匹配的板卡，元器件数量:" << board.components.size();
            for (const auto& label : board.components) {
                ComponentInfo component(label);
                component.componentName = label.label.isEmpty() ? QString("Component_%1").arg(label.id) : label.label;
                result.append(component);
                qDebug() << "PCBBoardManager::getComponents - 添加元器件:" << component.componentName 
                         << "位置:" << label.x << "," << label.y << "尺寸:" << label.w << "x" << label.h;
            }
            break;
        }
    }
    
    qDebug() << "PCBBoardManager::getComponents - 返回元器件数量:" << result.size();
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
    
    QList<QPair<double, PCBBoardInfo>> matches;
    
    for (const auto& board : boards_) {
        if (!board.isActive || board.templatePath.isEmpty()) {
            continue;
        }
        
        cv::Mat templateImage = loadImage(board.templatePath);
        if (templateImage.empty()) {
            continue;
        }
        
        double similarity = calculateSimilarity(inputImage, templateImage);
        if (similarity >= threshold) {
            matches.append(qMakePair(similarity, board));
        }
    }
    
    // 按相似度排序
    std::sort(matches.begin(), matches.end(), [](const QPair<double, PCBBoardInfo>& a, const QPair<double, PCBBoardInfo>& b) {
        return a.first > b.first;
    });
    
    QList<PCBBoardInfo> result;
    for (const auto& match : matches) {
        result.append(match.second);
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
    
    qDebug() << "PCBBoardManager::saveImage - 保存路径:" << fullPath;
    qDebug() << "PCBBoardManager::saveImage - 图片尺寸:" << image.cols << "x" << image.rows;
    
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
        // 发射信号需要在mutex外执行
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

double PCBBoardManager::calculateSimilarity(const cv::Mat& image1, const cv::Mat& image2) {
    if (image1.empty() || image2.empty()) {
        return 0.0;
    }
    
    // 如果SIFT检测器不可用，使用简单的模板匹配作为回退
    if (!sift_detector_ || !matcher_) {
        try {
            cv::Mat result;
            cv::matchTemplate(image1, image2, result, cv::TM_CCOEFF_NORMED);
            double minVal, maxVal;
            cv::minMaxLoc(result, &minVal, &maxVal);
            return maxVal;
        } catch (const cv::Exception& e) {
            qDebug() << "PCBBoardManager::calculateSimilarity - 模板匹配失败:" << e.what();
            return 0.0;
        }
    }
    
    try {
        // 使用SIFT特征匹配计算相似度
        std::vector<cv::KeyPoint> keypoints1, keypoints2;
        cv::Mat descriptors1, descriptors2;
        
        // 提取特征点和描述符
        sift_detector_->detectAndCompute(image1, cv::noArray(), keypoints1, descriptors1);
        sift_detector_->detectAndCompute(image2, cv::noArray(), keypoints2, descriptors2);
        
        if (descriptors1.empty() || descriptors2.empty()) {
            return 0.0;
        }
        
        // 特征匹配
        std::vector<cv::DMatch> matches;
        matcher_->match(descriptors1, descriptors2, matches);
        
        if (matches.empty()) {
            return 0.0;
        }
        
        // 计算好匹配的数量
        double goodMatchThreshold = 75.0;
        int goodMatches = 0;
        
        for (const auto& match : matches) {
            if (match.distance < goodMatchThreshold) {
                goodMatches++;
            }
        }
        
        // 计算相似度分数
        double similarity = static_cast<double>(goodMatches) / qMax(keypoints1.size(), keypoints2.size());
        return qMin(1.0, similarity);
        
    } catch (const cv::Exception& e) {
        qDebug() << "PCBBoardManager::calculateSimilarity - SIFT匹配失败:" << e.what();
        return 0.0;
    } catch (const std::exception& e) {
        qDebug() << "PCBBoardManager::calculateSimilarity - 异常:" << e.what();
        return 0.0;
    }
}

std::vector<cv::KeyPoint> PCBBoardManager::extractKeypoints(const cv::Mat& image) {
    std::vector<cv::KeyPoint> keypoints;
    
    if (image.empty() || !sift_detector_) {
        return keypoints;
    }
    
    try {
        sift_detector_->detect(image, keypoints);
    } catch (const cv::Exception& e) {
        qDebug() << "PCBBoardManager::extractKeypoints - 异常:" << e.what();
        keypoints.clear();
    }
    
    return keypoints;
}

cv::Mat PCBBoardManager::extractDescriptors(const cv::Mat& image, std::vector<cv::KeyPoint>& keypoints) {
    cv::Mat descriptors;
    
    if (image.empty() || !sift_detector_ || keypoints.empty()) {
        return descriptors;
    }
    
    try {
        sift_detector_->compute(image, keypoints, descriptors);
    } catch (const cv::Exception& e) {
        qDebug() << "PCBBoardManager::extractDescriptors - 异常:" << e.what();
        descriptors = cv::Mat();
    }
    
    return descriptors;
}

//=============================================================================
// 多线程相关实现
//=============================================================================

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
            
            QList<PCBBoardInfo> result = identifyBoard(inputImage, threshold);
            
            // 再次检查是否取消
            {
                QMutexLocker locker(&cancel_mutex_);
                if (identification_cancelled_) {
                    return QList<PCBBoardInfo>();
                }
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
    
    qDebug() << "PCBBoardManager::createBoardAsync - 开始异步板卡创建:" << boardName;
    
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
    
    // 在清理定时器的连接中添加自动删除
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
