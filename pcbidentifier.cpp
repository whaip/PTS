#include "pcbidentifier.h"
#include "cameramanager.h"
#include <QApplication>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QImageReader>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QThread>
#include <algorithm>
#include "include/opencv2/opencv.hpp"

// 静态常量定义
const double PCBIdentifier::DEFAULT_MATCH_THRESHOLD = 0.3;
const int PCBIdentifier::DEFAULT_MAX_RESULTS = 5;
const QStringList PCBIdentifier::SUPPORTED_FORMATS = {"jpg", "jpeg", "png", "bmp", "tiff", "tif"};

PCBIdentifier::PCBIdentifier(QObject *parent)
    : QObject(parent)
    , camera_manager_(nullptr)
    , realtime_timer_(new QTimer(this))
    , is_realtime_running_(false)
    , realtime_interval_ms_(500)  // 默认500ms间隔
    , future_watcher_(new QFutureWatcher<RealtimeIdentificationResult>(this))
    , is_processing_(false)
    , match_threshold_(DEFAULT_MATCH_THRESHOLD)
    , max_results_(DEFAULT_MAX_RESULTS)
    , database_path_("pcb_models.yml")
    , template_directory_("pcb_templates")
    , is_initialized_(false)
{
    // 设置默认数据库路径
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    database_path_ = appDataPath + "/pcb_models.yml";
    template_directory_ = appDataPath + "/pcb_templates";
    qDebug() << "Template directory set to:" << template_directory_;

    // 创建模板目录
    QDir().mkpath(template_directory_);
      // 设置实时识别定时器
    realtime_timer_->setSingleShot(false);
    connect(realtime_timer_, &QTimer::timeout, this, &PCBIdentifier::processRealtimeIdentification);
    
    // 设置线程监视器
    connect(future_watcher_, &QFutureWatcher<RealtimeIdentificationResult>::finished, 
            this, &PCBIdentifier::onIdentificationFinished);
    
    // 初始化匹配器
    if (initializeMatcher()) {
        is_initialized_ = true;
        qDebug() << "PCB Identifier initialized successfully";
        
        // 尝试加载现有数据库
        loadDatabase();
    } else {
        setError("Failed to initialize PCB identifier");
    }
}

PCBIdentifier::~PCBIdentifier()
{
    // 停止实时识别
    stopRealtimeIdentification();
      // 等待线程完成
    if (future_watcher_->isRunning()) {
        // 等待线程完成
        current_future_.waitForFinished();
    }
    
    // 重置处理状态
    {
        QMutexLocker locker(&identification_mutex_);
        is_processing_ = false;
    }
    
    if (is_initialized_) {
        saveDatabase();
    }
}

bool PCBIdentifier::addPCBModel(const QString& modelName, const cv::Mat& imageData)
{
    if (modelName.isEmpty()) {
        setError("模型名称不能为空");
        return false;
    }
      // 验证所有模板图片
    if(imageData.empty()) {
        setError("无效的模板图片");
        return false;
    }
    
    // 复制模板图片到PCBimage文件夹
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString pcbImageDir = documentsPath + "/PCBimage";
    QDir().mkpath(pcbImageDir);

    QString fileName = QString("%1_%2.jpg")
                      .arg(modelName.toUpper().replace(" ", "_"))
                      .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString destPath = pcbImageDir + "/" + fileName;
    if (!cv::imwrite(destPath.toStdString(), imageData)) {
        setError(QString("无法保存模板图片到: %1").arg(destPath));
        return false;
    }

    // 创建模型信息
    PCBModelInfo modelInfo;
    modelInfo.modelName = modelName;
    modelInfo.modelCode = modelName.toUpper().replace(" ", "_");
    modelInfo.templatePaths = { destPath };
    modelInfo.description = QString("PCB模型: %1").arg(modelName);
    modelInfo.lastUpdated = QDateTime::currentDateTime();
    
    // 检查是否已存在
    auto it = std::find_if(pcb_models_.begin(), pcb_models_.end(),
                          [&modelName](const PCBModelInfo& info) {
                              return info.modelName == modelName;
                          });
    
    if (it != pcb_models_.end()) {
        // 更新现有模型
        *it = modelInfo;
        qDebug() << "Updated PCB model:" << modelName;
    } else {
        // 添加新模型
        pcb_models_.append(modelInfo);
        qDebug() << "Added new PCB model:" << modelName;
    }
    
    // 更新SIFT数据库
    try {
        std::vector<std::string> stdTemplatePaths;
        for (const QString& path : modelInfo.templatePaths) {
            stdTemplatePaths.push_back(path.toStdString());        }
        // SIFT_MATCHER->appendToDatabase(stdTemplatePaths);
        
        emit databaseUpdated();
        return true;
        
    } catch (const std::exception& e) {
        setError(QString("更新SIFT数据库失败: %1").arg(e.what()));
        return false;
    }
}

bool PCBIdentifier::removePCBModel(const QString& modelName)
{
    auto it = std::find_if(pcb_models_.begin(), pcb_models_.end(),
                          [&modelName](const PCBModelInfo& info) {
                              return info.modelName == modelName;
                          });
    
    if (it == pcb_models_.end()) {
        setError(QString("未找到模型: %1").arg(modelName));
        return false;
    }
    
    // 从SIFT数据库中移除
    try {
        for (const QString& templatePath : it->templatePaths) {
            QFileInfo fileInfo(templatePath);
            QString fileName = fileInfo.baseName();
            SiftMatcher::removeFromDatabase(fileName.toStdString());
        }
        
        // 从模型列表中移除
        pcb_models_.erase(it);
        
        qDebug() << "Removed PCB model:" << modelName;
        emit databaseUpdated();
        return true;
        
    } catch (const std::exception& e) {
        setError(QString("从数据库移除模型失败: %1").arg(e.what()));
        return false;
    }
}

bool PCBIdentifier::updatePCBModel(const QString& modelName, const cv::Mat& image)
{
    // 先移除旧模型，再添加新模型
    if (removePCBModel(modelName)) {
        return addPCBModel(modelName, image);
    }
    return false;
}

QVector<PCBModelInfo> PCBIdentifier::getAvailableModels() const
{
    return pcb_models_;
}

bool PCBIdentifier::createDatabase(const QString& templateDirectory)
{
    template_directory_ = templateDirectory;
    
    if (!QDir(template_directory_).exists()) {
        setError(QString("模板目录不存在: %1").arg(template_directory_));
        return false;
    }
    
    // 查找所有模板图片
    QStringList templateImages = findTemplateImages(template_directory_);
    
    if (templateImages.isEmpty()) {
        setError("在模板目录中未找到有效的图片文件");
        return false;
    }
    
    try {
        // 转换为std::vector<std::string>
        std::vector<std::string> stdTemplateImages;
        for (const QString& path : templateImages) {
            stdTemplateImages.push_back(path.toStdString());        }
        
        // 创建SIFT数据库
        // SIFT_MATCHER->createDatabase(stdTemplateImages);
        
        // 分析模板图片并创建模型信息
        pcb_models_.clear();
        QMap<QString, QStringList> modelGroups;
        
        for (const QString& imagePath : templateImages) {
            QString modelName = extractModelNameFromPath(imagePath);
            modelGroups[modelName].append(imagePath);
        }
        
        // 为每个模型组创建PCBModelInfo
        for (auto it = modelGroups.begin(); it != modelGroups.end(); ++it) {
            PCBModelInfo modelInfo;
            modelInfo.modelName = it.key();
            modelInfo.modelCode = it.key().toUpper().replace(" ", "_");
            modelInfo.templatePaths = it.value();
            modelInfo.description = QString("PCB模型: %1").arg(it.key());
            modelInfo.lastUpdated = QDateTime::currentDateTime();
            
            pcb_models_.append(modelInfo);
        }
        
        // 保存数据库
        saveDatabase();
        
        qDebug() << "Database created successfully with" << pcb_models_.size() << "models";
        emit databaseUpdated();
        return true;
        
    } catch (const std::exception& e) {
        setError(QString("创建数据库失败: %1").arg(e.what()));
        return false;
    }
}

bool PCBIdentifier::loadDatabase(const QString& databasePath)
{
    QString dbPath = databasePath.isEmpty() ? database_path_ : databasePath;
    
    if (!QFileInfo::exists(dbPath)) {
        qDebug() << "Database file does not exist:" << dbPath;
        return false;
    }
    
    // 加载模型信息
    if (!loadModelInfo(dbPath)) {
        return false;
    }
    
    qDebug() << "Database loaded successfully with" << pcb_models_.size() << "models";
    return true;
}

bool PCBIdentifier::saveDatabase(const QString& databasePath)
{
    QString dbPath = databasePath.isEmpty() ? database_path_ : databasePath;
    
    return saveModelInfo(dbPath);
}

bool PCBIdentifier::rebuildDatabase()
{
    if (template_directory_.isEmpty()) {
        setError("模板目录未设置");
        return false;
    }
    
    return createDatabase(template_directory_);
}

void PCBIdentifier::setMatchThreshold(double threshold)
{
    match_threshold_ = qBound(0.0, threshold, 1.0);
}

void PCBIdentifier::setMaxResults(int maxResults)
{
    max_results_ = qMax(1, maxResults);
}

void PCBIdentifier::setUseGPU(bool useGPU)
{
    // GPU设置由SiftMatcher内部管理
    Q_UNUSED(useGPU)
}

QStringList PCBIdentifier::getSupportedImageFormats() const
{
    return SUPPORTED_FORMATS;
}

bool PCBIdentifier::validateImagePath(const QString& imagePath) const
{
    QFileInfo fileInfo(imagePath);
    
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }
    
    QString suffix = fileInfo.suffix().toLower();
    return SUPPORTED_FORMATS.contains(suffix);
}

void PCBIdentifier::setError(const QString& error)
{
    last_error_ = error;
    qDebug() << "PCBIdentifier Error:" << error;
    emit errorOccurred(error);
}

QString PCBIdentifier::extractModelNameFromPath(const QString& imagePath) const
{
    QFileInfo fileInfo(imagePath);
    QString baseName = fileInfo.baseName();
    
    // 尝试从文件名中提取模型名称
    // 假设文件名格式为: ModelName_variant.jpg 或 ModelName.jpg
    QStringList parts = baseName.split('_');
    if (!parts.isEmpty()) {
        return parts.first();
    }
    
    return baseName;
}

bool PCBIdentifier::loadModelInfo(const QString& configPath)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QString("无法打开配置文件: %1").arg(configPath));
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        setError("配置文件格式错误");
        return false;
    }
    
    QJsonObject root = doc.object();
    QJsonArray modelsArray = root["models"].toArray();
    
    pcb_models_.clear();
    
    for (const QJsonValue& value : modelsArray) {
        QJsonObject modelObj = value.toObject();
        
        PCBModelInfo info;
        info.modelName = modelObj["name"].toString();
        info.modelCode = modelObj["code"].toString();
        info.description = modelObj["description"].toString();
        info.lastUpdated = QDateTime::fromString(modelObj["lastUpdated"].toString(), Qt::ISODate);
        
        QJsonArray pathsArray = modelObj["templatePaths"].toArray();
        for (const QJsonValue& pathValue : pathsArray) {
            info.templatePaths.append(pathValue.toString());
        }
        
        pcb_models_.append(info);
    }
    
    return true;
}

bool PCBIdentifier::saveModelInfo(const QString& configPath) const
{
    QJsonObject root;
    QJsonArray modelsArray;
    
    for (const PCBModelInfo& info : pcb_models_) {
        QJsonObject modelObj;
        modelObj["name"] = info.modelName;
        modelObj["code"] = info.modelCode;
        modelObj["description"] = info.description;
        modelObj["lastUpdated"] = info.lastUpdated.toString(Qt::ISODate);
        
        QJsonArray pathsArray;
        for (const QString& path : info.templatePaths) {
            pathsArray.append(path);
        }
        modelObj["templatePaths"] = pathsArray;
        
        modelsArray.append(modelObj);
    }
    
    root["models"] = modelsArray;
    root["version"] = "1.0";
    root["createdBy"] = "PCBIdentifier";
    root["lastModified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(root);
    
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    return true;
}

QStringList PCBIdentifier::findTemplateImages(const QString& directory) const
{
    QStringList imageFiles;
    QDir dir(directory);
    
    // 设置文件过滤器
    QStringList filters;
    for (const QString& format : SUPPORTED_FORMATS) {
        filters << QString("*.%1").arg(format);
    }
    
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files);
    
    // 递归查找图片文件
    QFileInfoList fileList = dir.entryInfoList();
    for (const QFileInfo& fileInfo : fileList) {
        imageFiles.append(fileInfo.absoluteFilePath());
    }
    
    // 递归查找子目录
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList dirList = dir.entryInfoList();
    for (const QFileInfo& dirInfo : dirList) {
        imageFiles.append(findTemplateImages(dirInfo.absoluteFilePath()));
    }
    
    return imageFiles;
}

PCBIdentificationResult PCBIdentifier::processMatchResults(const std::vector<SiftMatcher::MatchResult>& matches) const
{
    PCBIdentificationResult result;
    
    if (matches.empty()) {
        result.errorMessage = "未找到匹配结果";
        return result;
    }
    
    // 取最佳匹配结果
    const auto& bestMatch = matches[0];
    
    if (bestMatch.matchScore < match_threshold_) {
        result.errorMessage = QString("匹配分数过低: %1 (阈值: %2)")
                             .arg(bestMatch.matchScore)
                             .arg(match_threshold_);
        return result;
    }
    
    result.modelName = QString::fromStdString(bestMatch.boardId);
    result.matchCount = bestMatch.goodMatchCount;
    result.matchScore = bestMatch.matchScore;
    
    // 计算置信度 (0-100)
    result.confidence = qMin(100.0, (bestMatch.matchScore / match_threshold_) * 100.0);
    
    result.isValid = true;
    
    return result;
}

bool PCBIdentifier::validateTemplate(const QString& templatePath) const
{
    if (!validateImagePath(templatePath)) {
        return false;
    }
    
    // 尝试加载图片以验证其有效性
    QImageReader reader(templatePath);
    return reader.canRead();
}

// 实时摄像头识别功能实现

RealtimeIdentificationResult PCBIdentifier::identifyPCBFromImage(const cv::Mat& image)
{
    // 使用线程化版本防止主线程阻塞
    return identifyPCBFromImageThreaded(image);
}

bool PCBIdentifier::startRealtimeIdentification(CameraManager* cameraManager)
{
    if (!is_initialized_) {
        setError("PCB识别器未初始化");
        return false;
    }
    
    if (!cameraManager) {
        setError("摄像头管理器为空");
        return false;
    }
    
    if (!cameraManager->isCameraAvailable(CameraType::HD_CAMERA)) {
        setError("高清摄像头不可用");
        return false;
    }
      camera_manager_ = cameraManager;
    
    // 先断开之前可能存在的连接，避免重复连接
    if (camera_manager_) {
        disconnect(camera_manager_, &CameraManager::newImageAvailable,
                  this, &PCBIdentifier::onCameraImageReceived);
    }
    
    // 连接摄像头信号
    connect(camera_manager_, &CameraManager::newImageAvailable,
            this, &PCBIdentifier::onCameraImageReceived);
    
    // 启动定时器进行周期性识别
    realtime_timer_->start(realtime_interval_ms_);
    is_realtime_running_ = true;
    
    qDebug() << "实时PCB识别已启动，识别间隔:" << realtime_interval_ms_ << "ms";
    emit realtimeIdentificationStarted();
    
    return true;
}

void PCBIdentifier::stopRealtimeIdentification()
{
    if (!is_realtime_running_) {
        return;
    }
    
    // 停止定时器
    realtime_timer_->stop();
    
    // 断开摄像头信号连接
    if (camera_manager_) {
        disconnect(camera_manager_, &CameraManager::newImageAvailable,
                   this, &PCBIdentifier::onCameraImageReceived);
    }
      // 等待当前的识别任务完成
    if (future_watcher_->isRunning()) {
        current_future_.waitForFinished(); // 等待完成
    }
    
    // 重置处理状态
    {
        QMutexLocker locker(&identification_mutex_);
        is_processing_ = false;
    }
    
    is_realtime_running_ = false;
    
    qDebug() << "实时PCB识别已停止";
    emit realtimeIdentificationStopped();
}

bool PCBIdentifier::isRealtimeIdentificationRunning() const
{
    return is_realtime_running_;
}

void PCBIdentifier::setRealtimeProcessingInterval(int intervalMs)
{
    realtime_interval_ms_ = qMax(100, intervalMs);  // 最小100ms间隔
    
    if (is_realtime_running_) {
        realtime_timer_->setInterval(realtime_interval_ms_);
    }
}

void PCBIdentifier::setCameraManager(CameraManager* cameraManager)
{
    camera_manager_ = cameraManager;
}

CameraManager* PCBIdentifier::getCameraManager() const
{
    return camera_manager_;
}

// 槽函数实现

void PCBIdentifier::onCameraImageReceived()
{
    // 此函数在接收到新图像时被调用，但实际处理由定时器控制
    // 这样可以避免过于频繁的识别请求
}

void PCBIdentifier::processRealtimeIdentification()
{
    if (!camera_manager_ || !is_realtime_running_) {
        return;
    }
    
    // 获取最新的高清摄像头图像
    ImageData imageData = camera_manager_->getLatestImage(CameraType::HD_CAMERA);
    
    if (!imageData.isValid || imageData.image.empty()) {
        return;
    }
    
    // 避免重复处理相同的图像
    static QDateTime lastProcessedImageTime;
    if (lastProcessedImageTime.isValid() && lastProcessedImageTime == imageData.timestamp) {
        return; // 相同的图像，跳过处理
    }
    
    // 使用线程化处理避免阻塞主线程
    processImageInBackground(imageData.image);
    
    lastProcessedImageTime = imageData.timestamp;
}

// 私有辅助方法

RealtimeIdentificationResult PCBIdentifier::processRealtimeMatchResults(
    const std::vector<SiftMatcher::MatchResult>& matches, const cv::Mat& originalImage) const
{
    RealtimeIdentificationResult result;
    
    if (matches.empty()) {
        result.isValid = false;
        return result;
    }
    
    // 取最佳匹配结果
    const auto& bestMatch = matches[0];
    
    if (bestMatch.matchScore < match_threshold_) {
        result.isValid = false;
        return result;
    }
    
    // 从图片路径提取模型名称
    result.modelName = QString::fromStdString(bestMatch.boardId);
    result.matchCount = bestMatch.goodMatchCount;
    result.matchScore = bestMatch.matchScore;
    
    // 计算置信度 (0-100)
    result.confidence = qMin(100.0, (bestMatch.matchScore / match_threshold_) * 100.0);
    
    // 创建带标注的图像
    result.annotatedImage = annotateIdentificationResult(originalImage, result);
    
    result.isValid = true;
    result.timestamp = QDateTime::currentDateTime();
    
    return result;
}

cv::Mat PCBIdentifier::annotateIdentificationResult(const cv::Mat& image, const RealtimeIdentificationResult& result) const
{
    cv::Mat annotatedImage = image.clone();
    
    if (!result.isValid) {
        return annotatedImage;
    }
    
    // 添加文本标注
    QString labelText = QString("%1 (%.1f%%)").arg(result.modelName).arg(result.confidence);
    std::string label = labelText.toStdString();
    
    // 设置文本参数
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = 1.0;
    int thickness = 2;
    cv::Scalar textColor(0, 255, 0);  // 绿色文本
    cv::Scalar backgroundColor(0, 0, 0);  // 黑色背景
    
    // 获取文本尺寸
    int baseline = 0;
    cv::Size textSize = cv::getTextSize(label, fontFace, fontScale, thickness, &baseline);
    
    // 绘制文本背景
    cv::Point textOrg(10, 30);
    cv::rectangle(annotatedImage, 
                  cv::Point(textOrg.x, textOrg.y - textSize.height),
                  cv::Point(textOrg.x + textSize.width, textOrg.y + baseline),
                  backgroundColor, cv::FILLED);
    
    // 绘制文本
    cv::putText(annotatedImage, label, textOrg, fontFace, fontScale, textColor, thickness);
    
    // 添加置信度条
    int barWidth = 200;
    int barHeight = 20;
    cv::Point barStart(10, 50);
    
    // 绘制置信度条背景
    cv::rectangle(annotatedImage, barStart, 
                  cv::Point(barStart.x + barWidth, barStart.y + barHeight),
                  cv::Scalar(50, 50, 50), cv::FILLED);
    
    // 绘制置信度条填充
    int fillWidth = static_cast<int>((result.confidence / 100.0) * barWidth);
    cv::Scalar barColor = result.confidence > 70 ? cv::Scalar(0, 255, 0) :  // 绿色
                         result.confidence > 40 ? cv::Scalar(0, 255, 255) : // 黄色
                                                 cv::Scalar(0, 0, 255);     // 红色
    
    cv::rectangle(annotatedImage, barStart,
                  cv::Point(barStart.x + fillWidth, barStart.y + barHeight),
                  barColor, cv::FILLED);
    
    // 添加时间戳
    QString timeText = result.timestamp.toString("hh:mm:ss.zzz");
    cv::putText(annotatedImage, timeText.toStdString(),
                cv::Point(10, annotatedImage.rows - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    
    return annotatedImage;
}

// 线程化识别方法实现
RealtimeIdentificationResult PCBIdentifier::identifyPCBFromImageThreaded(const cv::Mat& image)
{
    RealtimeIdentificationResult result;
    
    if (!is_initialized_) {
        result.isValid = false;
        return result;
    }
    
    if (image.empty()) {
        result.isValid = false;
        return result;
    }
    
    // 检查是否有正在进行的识别任务
    {
        QMutexLocker locker(&identification_mutex_);
        if (is_processing_) {
            // 如果正在处理，返回上一次的结果或空结果
            result.isValid = false;
            result.modelName = "处理中...";
            return result;
        }
    }
    
    try {
        
        // 使用SIFT匹配器进行识别
        auto matches = SIFT_MATCHER->matchImage(image);
        
        if (matches.empty()) {
            result.isValid = false;
        } else {
            // 处理匹配结果
            result = processRealtimeMatchResults(matches, image);
        }
        
        qDebug() << "Threaded real-time PCB identification completed:" << result.modelName 
                 << "confidence:" << result.confidence;
        
    } catch (const std::exception& e) {
        setError(QString("线程化实时识别过程中发生错误: %1").arg(e.what()));
        result.isValid = false;
    }
    
    return result;
}

void PCBIdentifier::processImageInBackground(const cv::Mat& image)
{
    // 检查是否有正在进行的任务
    {
        QMutexLocker locker(&identification_mutex_);
        if (is_processing_) {
            return; // 如果正在处理，直接返回
        }
        is_processing_ = true;
    }
    
    // 保存当前图像副本
    {
        QMutexLocker imageLocker(&image_mutex_);
        current_image_ = image.clone();
    }
    
    // 创建异步任务
    current_future_ = QtConcurrent::run([this]() -> RealtimeIdentificationResult {
        cv::Mat processingImage;
        {
            QMutexLocker imageLocker(&image_mutex_);
            processingImage = current_image_.clone();
        }
        
        RealtimeIdentificationResult result;
        
        if (!is_initialized_ || processingImage.empty()) {
            result.isValid = false;
            return result;
        }
        
        try {
            
            // 使用SIFT匹配器进行识别
            auto matches = SIFT_MATCHER->matchImage(current_image_);
            
            if (!matches.empty()) {
                // 处理匹配结果
                result = processRealtimeMatchResults(matches, processingImage);
            } else {
                result.isValid = false;
            }
            
        } catch (const std::exception& e) {
            qDebug() << "Background identification error:" << e.what();
            result.isValid = false;
        }
        
        return result;
    });
    
    // 设置监视器
    future_watcher_->setFuture(current_future_);
}

void PCBIdentifier::onIdentificationFinished()
{
    if (!future_watcher_->isFinished()) {
        return;
    }
    
    try {
        RealtimeIdentificationResult result = future_watcher_->result();
        
        // 重置处理状态
        {
            QMutexLocker locker(&identification_mutex_);
            is_processing_ = false;
        }
        
        // 发出信号
        if (result.isValid) {
            emit realtimeIdentificationCompleted(result);
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error retrieving identification result:" << e.what();
        
        // 重置处理状态
        {
            QMutexLocker locker(&identification_mutex_);
            is_processing_ = false;
        }
    }
}

//=============================================================================
// 初始化匹配器
//=============================================================================
bool PCBIdentifier::initializeMatcher()
{
    // 初始化 SiftMatcher 实例并检查 GPU 支持
    bool gpuOk = SIFT_MATCHER->checkGPU();
    qDebug() << "PCBIdentifier: SIFT matcher GPU support:" << gpuOk;
    return true;
}

//=============================================================================
// 检查是否启用 GPU
//=============================================================================
bool PCBIdentifier::isGPUEnabled() const
{
    // 返回底层 SiftMatcher 的 GPU 支持状态
    return SIFT_MATCHER->checkGPU();
}
