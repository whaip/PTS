# PCB故障检测系统 - 实现细节

## 系统架构详解

### ComponentDiagnosticFramework 架构

#### 核心设计原则
- **单一职责**: 每个诊断器专门处理一种元件类型
- **开放封闭**: 易于扩展新元件类型，无需修改现有代码
- **依赖倒置**: 依赖抽象接口，不依赖具体实现
- **接口隔离**: 细粒度接口设计，避免接口污染

#### 类层次结构

```cpp
BaseComponentDiagnostic (抽象基类)
├── ResistorDiagnostic     // 电阻器诊断器
├── CapacitorDiagnostic    // 电容器诊断器  
├── InductorDiagnostic     // 电感器诊断器
├── DiodeDiagnostic        // 二极管诊断器
└── ICDiagnostic           // 集成电路诊断器

ComponentDiagnosticManager // 诊断管理器
└── 管理所有诊断器实例
    ├── 任务调度和分发
    ├── 结果收集和处理
    └── 异步执行控制
```

#### 数据流架构

```
ComponentSpec → ComponentDiagnosticManager → BaseComponentDiagnostic
                                           ↓
DeviceManager ← TestConfiguration ← prepareTest()
     ↓                                     ↓
DeviceThread → Hardware → Measurement → executeTest()
     ↓                                     ↓
MeasurementData ← DataCollection ← analyzeResults()
     ↓                                     ↓
ComponentDiagnosticResult ← ResultGeneration
```

### 多线程设备管理

#### DeviceManager 线程模型

```cpp
class DeviceManager {
private:
    QMap<QString, BaseDeviceThread*> deviceThreads_;
    DeviceSyncController* syncController_;
    
    // 线程安全的设备状态管理
    mutable QMutex statusMutex_;
    QMap<QString, DeviceStatus> deviceStatus_;
};
```

#### 设备线程架构

```cpp
// 基础设备线程
class BaseDeviceThread : public QThread {
    Q_OBJECT
protected:
    // 操作队列 (线程安全)
    QQueue<DeviceOperation> operationQueue_;
    QMutex operationMutex_;
    QWaitCondition operationCondition_;
    
    // 结果队列 (线程安全)
    QQueue<DeviceResult> resultQueue_;
    
    virtual DeviceResult executeOperation(const DeviceOperation& op) = 0;
    virtual void handleSyncTrigger() = 0;
};

// AO设备线程 (JY5711)
class AODeviceThread : public BaseDeviceThread {
private:
    JY5710_DeviceHandle deviceHandle_;
    QMap<int, double> channelStates_;
    
    // 波形生成支持
    bool configureWaveform(const DeviceOperation& op);
    bool outputWaveform(const DeviceOperation& op);
};

// DAQ设备线程 (JY5320)
class DAQDeviceThread : public BaseDeviceThread {
private:
    JY5320_DeviceHandle deviceHandle_;
    
    // 多种采集模式
    AcquisitionMode currentMode_;
    QVector<QVector<double>> dataBuffer_;
    QMutex bufferMutex_;
    
    // 数据采集方法
    DeviceResult performSinglePointAcquisition(const DeviceOperation& op);
    DeviceResult configureMultiPointAcquisition(const DeviceOperation& op);
    DeviceResult startContinuousAcquisition(const DeviceOperation& op);
};
```

#### 设备同步控制

```cpp
class DeviceSyncController {
private:
    struct SyncGroup {
        QStringList deviceNames;
        QStringList readyDevices;
        bool triggered = false;
    };
    
    QHash<QString, SyncGroup> syncGroups_;
    QHash<QString, QWaitCondition*> syncConditions_;
    QMutex groupsMutex_;

public:
    // 同步组管理
    void createSyncGroup(const QString& groupName, const QStringList& devices);
    bool waitForSync(const QString& groupName, const QString& device, int timeout);
    void triggerSync(const QString& groupName);
};
```

## 核心算法实现

### 元件诊断算法

#### 电阻器诊断算法

```cpp
class ResistorDiagnostic : public BaseComponentDiagnostic {
public:
    ComponentDiagnosticResult diagnose(const ComponentSpec& spec) override {
        ComponentDiagnosticResult result;
        
        // 1. 准备测试配置
        TestConfiguration config = prepareResistorTest(spec);
        
        // 2. 执行电阻测量
        MeasurementData resistance = measureResistance(config);
        MeasurementData voltage = measureVoltage(config);
        MeasurementData current = measureCurrent(config);
        
        // 3. 计算电阻值
        double calculatedR = calculateResistance(voltage, current);
        
        // 4. 故障分析
        AnalysisResult analysis = analyzeResistorFaults(
            calculatedR, spec.nominal_value, spec.tolerance_percent);
        
        // 5. 生成结果
        result = generateDiagnosticResult(analysis, {resistance, voltage, current});
        
        return result;
    }

private:
    AnalysisResult analyzeResistorFaults(double measured, double nominal, double tolerance) {
        AnalysisResult result;
        
        double deviation = abs(measured - nominal) / nominal * 100.0;
        
        if (measured < 1.0) {
            result.faultType = "SHORT_CIRCUIT";
            result.isPassed = false;
            result.confidence = 0.95;
        } else if (measured > nominal * 1000) {
            result.faultType = "OPEN_CIRCUIT"; 
            result.isPassed = false;
            result.confidence = 0.90;
        } else if (deviation > tolerance) {
            result.faultType = "OUT_OF_TOLERANCE";
            result.isPassed = false;
            result.confidence = 0.85;
        } else {
            result.faultType = "COMPONENT_OK";
            result.isPassed = true;
            result.confidence = 0.95;
        }
        
        result.healthScore = 100.0 * (1.0 - deviation / (tolerance * 2.0));
        result.calculatedValue = measured;
        result.deviation = deviation;
        
        return result;
    }
};
```

#### 电容器诊断算法

```cpp
class CapacitorDiagnostic : public BaseComponentDiagnostic {
private:
    ComponentDiagnosticResult diagnoseCapacitor(const ComponentSpec& spec) {
        // 1. AC阻抗测量
        MeasurementData impedance = measureACImpedance(spec, 1000.0); // 1kHz
        
        // 2. 计算电容值
        double frequency = 1000.0;
        double capacitance = 1.0 / (2 * M_PI * frequency * impedance.value);
        
        // 3. ESR测量 (等效串联电阻)
        MeasurementData esr = measureESR(spec);
        
        // 4. 漏电流测量
        MeasurementData leakage = measureLeakageCurrent(spec);
        
        // 5. 综合分析
        return analyzeCapacitorHealth(capacitance, esr.value, leakage.value, spec);
    }
    
    AnalysisResult analyzeCapacitorHealth(double C, double esr, double leakage, 
                                         const ComponentSpec& spec) {
        AnalysisResult result;
        
        double nominalC = spec.nominal_value;
        double tolerance = spec.tolerance_percent;
        double maxESR = spec.max_esr;
        double maxLeakage = spec.max_leakage;
        
        bool capacitanceOK = isWithinTolerance(C, nominalC, tolerance);
        bool esrOK = (esr <= maxESR);
        bool leakageOK = (leakage <= maxLeakage);
        
        if (!capacitanceOK) {
            result.faultTypes.append("CAPACITANCE_OUT_OF_TOLERANCE");
        }
        if (!esrOK) {
            result.faultTypes.append("HIGH_ESR");
        }
        if (!leakageOK) {
            result.faultTypes.append("HIGH_LEAKAGE");
        }
        
        result.isPassed = capacitanceOK && esrOK && leakageOK;
        result.healthScore = calculateCapacitorHealthScore(C, esr, leakage, spec);
        
        return result;
    }
};
```

### 图像处理和AI算法

#### YOLO元件检测

```cpp
class YOLOModel {
private:
    cv::dnn::Net net_;
    std::vector<std::string> classNames_;
    std::vector<cv::Scalar> colors_;
    
public:
    std::vector<Label> detect(const cv::Mat& image, float confThreshold = 0.6) {
        // 1. 图像预处理
        cv::Mat blob;
        cv::dnn::blobFromImage(image, blob, 1/255.0, cv::Size(640, 640), 
                              cv::Scalar(0,0,0), true, false);
        
        // 2. 网络推理
        net_.setInput(blob);
        std::vector<cv::Mat> outputs;
        net_.forward(outputs, getOutputNames());
        
        // 3. 后处理
        std::vector<Label> detections;
        postprocess(image, outputs, confThreshold, detections);
        
        return detections;
    }
    
private:
    void postprocess(const cv::Mat& frame, const std::vector<cv::Mat>& outs,
                    float confThreshold, std::vector<Label>& detections) {
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;
        
        // 解析网络输出
        for (size_t i = 0; i < outs.size(); ++i) {
            float* data = (float*)outs[i].data;
            for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols) {
                cv::Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
                cv::Point classIdPoint;
                double confidence;
                minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
                
                if (confidence > confThreshold) {
                    int centerX = (int)(data[0] * frame.cols);
                    int centerY = (int)(data[1] * frame.rows);
                    int width = (int)(data[2] * frame.cols);
                    int height = (int)(data[3] * frame.rows);
                    
                    boxes.push_back(cv::Rect(centerX - width/2, centerY - height/2, 
                                           width, height));
                    classIds.push_back(classIdPoint.x);
                    confidences.push_back((float)confidence);
                }
            }
        }
        
        // NMS非极大值抑制
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, confThreshold, 0.4, indices);
        
        // 生成最终检测结果
        for (size_t i = 0; i < indices.size(); ++i) {
            int idx = indices[i];
            Label label;
            label.class_id = classIds[idx];
            label.class_name = classNames_[classIds[idx]];
            label.confidence = confidences[idx];
            label.x = boxes[idx].x;
            label.y = boxes[idx].y;
            label.width = boxes[idx].width;
            label.height = boxes[idx].height;
            
            detections.push_back(label);
        }
    }
};
```

#### SIFT特征匹配PCB识别

```cpp
class SiftMatcher {
private:
    cv::Ptr<cv::SIFT> detector_;
    cv::Ptr<cv::DescriptorMatcher> matcher_;
    std::vector<PCBModel> models_;
    
public:
    std::vector<MatchResult> matchImage(const cv::Mat& queryImage) {
        // 1. 提取查询图像特征
        std::vector<cv::KeyPoint> queryKeypoints;
        cv::Mat queryDescriptors;
        detector_->detectAndCompute(queryImage, cv::noArray(), 
                                   queryKeypoints, queryDescriptors);
        
        std::vector<MatchResult> results;
        
        // 2. 与数据库中每个模型匹配
        for (const auto& model : models_) {
            MatchResult result = matchWithModel(queryDescriptors, model);
            if (result.matchScore > 0.3) {  // 阈值过滤
                results.push_back(result);
            }
        }
        
        // 3. 按匹配分数排序
        std::sort(results.begin(), results.end(), 
                 [](const MatchResult& a, const MatchResult& b) {
                     return a.matchScore > b.matchScore;
                 });
        
        return results;
    }
    
private:
    MatchResult matchWithModel(const cv::Mat& queryDesc, const PCBModel& model) {
        MatchResult result;
        result.boardId = model.boardId;
        
        // FLANN匹配
        std::vector<std::vector<cv::DMatch>> knnMatches;
        matcher_->knnMatch(queryDesc, model.descriptors, knnMatches, 2);
        
        // Lowe's ratio test
        std::vector<cv::DMatch> goodMatches;
        for (const auto& match : knnMatches) {
            if (match.size() == 2 && match[0].distance < 0.7 * match[1].distance) {
                goodMatches.push_back(match[0]);
            }
        }
        
        result.matchCount = goodMatches.size();
        result.matchScore = calculateMatchScore(goodMatches, queryDesc.rows);
        
        return result;
    }
    
    double calculateMatchScore(const std::vector<cv::DMatch>& matches, int queryFeatures) {
        if (queryFeatures == 0) return 0.0;
        
        double ratio = (double)matches.size() / queryFeatures;
        double avgDistance = 0.0;
        
        for (const auto& match : matches) {
            avgDistance += match.distance;
        }
        avgDistance /= matches.size();
        
        // 综合匹配比例和平均距离
        double score = ratio * (1.0 - avgDistance / 256.0);
        return std::min(1.0, score);
    }
};
```

### 温度监测和红外处理

#### 红外摄像头数据处理

```cpp
class CameraManager {
private:
    struct IRCameraData {
        void* tempStreamer;
        ThermalData latestThermalData;
        mutable QMutex thermalMutex;
        double temperatureThreshold_{80.0};
    };
    
    // 温度数据回调处理
    static void temperatureCallback(uint16_t *rawData, uint32_t width, 
                                   uint32_t height, uint64_t pts, void *arg) {
        CameraManager* manager = static_cast<CameraManager*>(arg);
        manager->processTemperatureData(rawData, width, height, pts);
    }
    
    void processTemperatureData(uint16_t* rawData, uint32_t width, 
                               uint32_t height, uint64_t pts) {
        QMutexLocker locker(&irCamera_.thermalMutex);
        
        ThermalData& data = irCamera_.latestThermalData;
        data.width = width;
        data.height = height;
        data.rawTempData = rawData;
        data.timestamp = QDateTime::currentDateTime();
        
        // 计算温度统计
        calculateTemperatureStats(data);
        
        // 生成彩色热图
        generateThermalColorMap(data);
        
        // 检查温度报警
        checkTemperatureAlerts(data);
        
        data.isValid = true;
        
        // 发射信号
        emit newThermalDataAvailable(data);
    }
    
    void calculateTemperatureStats(ThermalData& data) {
        if (!data.rawTempData) return;
        
        double minTemp = 1000.0;
        double maxTemp = -273.15;
        double totalTemp = 0.0;
        int pixelCount = data.width * data.height;
        
        for (int i = 0; i < pixelCount; ++i) {
            double temp = convertRawToTemperature(data.rawTempData[i]);
            minTemp = std::min(minTemp, temp);
            maxTemp = std::max(maxTemp, temp);
            totalTemp += temp;
        }
        
        data.minTemp = minTemp;
        data.maxTemp = maxTemp;
        data.avgTemp = totalTemp / pixelCount;
    }
    
    void generateThermalColorMap(ThermalData& data) {
        cv::Mat tempMap(data.height, data.width, CV_32F);
        
        // 转换原始数据到温度值
        for (uint32_t y = 0; y < data.height; ++y) {
            for (uint32_t x = 0; x < data.width; ++x) {
                uint16_t rawValue = data.rawTempData[y * data.width + x];
                float temp = convertRawToTemperature(rawValue);
                tempMap.at<float>(y, x) = temp;
            }
        }
        
        // 归一化到0-255
        cv::Mat normalized;
        cv::normalize(tempMap, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
        
        // 应用JET颜色映射
        cv::applyColorMap(normalized, data.thermalColorMap, cv::COLORMAP_JET);
        data.temperatureMap = tempMap;
    }
    
    void checkTemperatureAlerts(const ThermalData& data) {
        if (data.maxTemp > temperatureThreshold_) {
            // 找到最高温度点位置
            cv::Point maxLoc;
            cv::Mat tempMat(data.height, data.width, CV_32F);
            
            for (uint32_t y = 0; y < data.height; ++y) {
                for (uint32_t x = 0; x < data.width; ++x) {
                    tempMat.at<float>(y, x) = convertRawToTemperature(
                        data.rawTempData[y * data.width + x]);
                }
            }
            
            cv::minMaxLoc(tempMat, nullptr, nullptr, nullptr, &maxLoc);
            
            emit temperatureAlert(data.maxTemp, maxLoc);
        }
    }
};
```

## 数据管理和持久化

### 数据库设计

#### PCB板卡数据库

```cpp
class PCBBoardManager {
private:
    QString database_path_;
    QList<PCBBoardInfo> boards_;
    
    bool saveBoardsToDatabase() {
        QJsonObject root;
        QJsonArray boardsArray;
        
        for (const PCBBoardInfo& board : boards_) {
            boardsArray.append(board.toJson());
        }
        
        root["boards"] = boardsArray;
        root["version"] = "1.0";
        root["lastModified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        QJsonDocument doc(root);
        QFile file(database_path_);
        
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        
        file.write(doc.toJson());
        return true;
    }
    
    void loadBoardsFromDatabase() {
        QFile file(database_path_);
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
        
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root = doc.object();
        QJsonArray boardsArray = root["boards"].toArray();
        
        boards_.clear();
        for (const QJsonValue& value : boardsArray) {
            PCBBoardInfo board = PCBBoardInfo::fromJson(value.toObject());
            boards_.append(board);
        }
    }
};
```

#### 检测记录数据库

```cpp
class PCBDetectionManager {
private:
    QList<PCBDetectionRecord> records_;
    QString database_path_;
    QMutex records_mutex_;
    
public:
    bool saveDetectionRecord(const PCBDetectionRecord& record) {
        QMutexLocker locker(&records_mutex_);
        
        PCBDetectionRecord newRecord = record;
        if (newRecord.id.isEmpty()) {
            newRecord.id = generateUniqueId();
        }
        if (newRecord.timestamp.isNull()) {
            newRecord.timestamp = QDateTime::currentDateTime();
        }
        
        records_.append(newRecord);
        
        // 限制记录数量
        while (records_.size() > max_records_count_) {
            PCBDetectionRecord oldRecord = records_.takeFirst();
            deleteImageFiles(oldRecord.id);  // 清理关联的图像文件
        }
        
        emit recordAdded(newRecord.id);
        return true;
    }
    
    QString saveAnnotatedImage(const cv::Mat& image, const std::vector<Label>& labels,
                              const std::vector<std::string>& classNames,
                              const std::vector<cv::Scalar>& colors,
                              const QString& prefix) {
        if (image.empty()) {
            return QString();
        }
        
        cv::Mat annotatedImage = createAnnotatedImage(image, labels, classNames, colors);
        return saveImage(annotatedImage, prefix);
    }
    
private:
    cv::Mat createAnnotatedImage(const cv::Mat& originalImage,
                                const std::vector<Label>& labels,
                                const std::vector<std::string>& classNames,
                                const std::vector<cv::Scalar>& colors) {
        cv::Mat annotated = originalImage.clone();
        
        for (const Label& label : labels) {
            // 绘制边界框
            cv::Rect boundingBox(label.x, label.y, label.width, label.height);
            cv::Scalar color = colors[label.class_id % colors.size()];
            cv::rectangle(annotated, boundingBox, color, 2);
            
            // 绘制标签文本
            QString labelText = QString("%1: %.2f").arg(label.class_name.c_str())
                                                   .arg(label.confidence);
            cv::putText(annotated, labelText.toStdString(), 
                       cv::Point(label.x, label.y - 5),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
        }
        
        return annotated;
    }
};
```

### 结果导出系统

```cpp
class ResultExporter {
public:
    bool exportToPDF(const QVector<DiagnosticResult>& results, const QString& fileName) {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        
        QPainter painter(&printer);
        QTextDocument document;
        
        // 生成HTML报告内容
        QString htmlContent = generateHTMLReport(results);
        document.setHtml(htmlContent);
        
        // 渲染PDF
        document.print(&printer);
        
        emit exportCompleted(fileName);
        return true;
    }
    
    bool exportToCSV(const QVector<DiagnosticResult>& results, const QString& fileName) {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        
        // CSV头部
        out << "Timestamp,Component,Reference,Type,Result,Health Score,Confidence,Faults,Notes\n";
        
        // 数据行
        for (const DiagnosticResult& result : results) {
            QStringList row = {
                result.timestamp.toString(Qt::ISODate),
                result.componentType,
                result.componentId,
                componentTypeToString(static_cast<ComponentType>(result.componentType.toInt())),
                (result.result == DiagnosticResult::PASS ? "PASS" : "FAIL"),
                QString::number(result.healthScore, 'f', 2),
                QString::number(result.confidence, 'f', 2),
                result.faultTypes.join(";"),
                result.notes
            };
            
            out << row.join(",") << "\n";
        }
        
        emit exportCompleted(fileName);
        return true;
    }
    
private:
    QString generateHTMLReport(const QVector<DiagnosticResult>& results) {
        QString html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>PCB故障检测报告</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 20px; border-radius: 5px; }
        .summary { margin: 20px 0; }
        .results { width: 100%; border-collapse: collapse; }
        .results th, .results td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        .results th { background-color: #f2f2f2; }
        .pass { color: green; font-weight: bold; }
        .fail { color: red; font-weight: bold; }
    </style>
</head>
<body>
)";
        
        // 添加报告头部
        html += QString(R"(
    <div class="header">
        <h1>PCB故障检测报告</h1>
        <p>生成时间: %1</p>
        <p>测试数量: %2</p>
    </div>
)").arg(QDateTime::currentDateTime().toString(Qt::DefaultLocaleLong# PCB故障检测系统 - 实现细节

## 系统架构详解

### ComponentDiagnosticFramework 架构

#### 核心设计原则
- **单一职责**: 每个诊断器专门处理一种元件类型
- **开放封闭**: 易于扩展新元件类型，无需修改现有代码
- **依赖倒置**: 依赖抽象接口，不依赖具体实现
- **接口隔离**: 细粒度接口设计，避免接口污染

#### 类层次结构

```cpp
BaseComponentDiagnostic (抽象基类)
├── ResistorDiagnostic     // 电阻器诊断器
├── CapacitorDiagnostic    // 电容器诊断器  
├── InductorDiagnostic     // 电感器诊断器
├── DiodeDiagnostic        // 二极管诊断器
└── ICDiagnostic           // 集成电路诊断器

ComponentDiagnosticManager // 诊断管理器
└── 管理所有诊断器实例
    ├── 任务调度和分发
    ├── 结果收集和处理
    └── 异步执行控制
```

#### 数据流架构

```
ComponentSpec → ComponentDiagnosticManager → BaseComponentDiagnostic
                                           ↓
DeviceManager ← TestConfiguration ← prepareTest()
     ↓                                     ↓
DeviceThread → Hardware → Measurement → executeTest()
     ↓                                     ↓
MeasurementData ← DataCollection ← analyzeResults()
     ↓                                     ↓
ComponentDiagnosticResult ← ResultGeneration
```

### 多线程设备管理

#### DeviceManager 线程模型

```cpp
class DeviceManager {
private:
    QMap<QString, BaseDeviceThread*> deviceThreads_;
    DeviceSyncController* syncController_;
    
    // 线程安全的设备状态管理
    mutable QMutex statusMutex_;
    QMap<QString, DeviceStatus> deviceStatus_;
};
```

#### 设备线程架构

```cpp
// 基础设备线程
class BaseDeviceThread : public QThread {
    Q_OBJECT
protected:
    // 操作队列 (线程安全)
    QQueue<DeviceOperation> operationQueue_;
    QMutex operationMutex_;
    QWaitCondition operationCondition_;
    
    // 结果队列 (线程安全)
    QQueue<DeviceResult> resultQueue_;
    
    virtual DeviceResult executeOperation(const DeviceOperation& op) = 0;
    virtual void handleSyncTrigger() = 0;
};

// AO设备线程 (JY5711)
class AODeviceThread : public BaseDeviceThread {
private:
    JY5710_DeviceHandle deviceHandle_;
    QMap<int, double> channelStates_;
    
    // 波形生成支持
    bool configureWaveform(const DeviceOperation& op);
    bool outputWaveform(const DeviceOperation& op);
};

// DAQ设备线程 (JY5320)
class DAQDeviceThread : public BaseDeviceThread {
private:
    JY5320_DeviceHandle deviceHandle_;
    
    // 多种采集模式
    AcquisitionMode currentMode_;
    QVector<QVector<double>> dataBuffer_;
    QMutex bufferMutex_;
    
    // 数据采集方法
    DeviceResult performSinglePointAcquisition(const DeviceOperation& op);
    DeviceResult configureMultiPointAcquisition(const DeviceOperation& op);
    DeviceResult startContinuousAcquisition(const DeviceOperation& op);
};
```

#### 设备同步控制

```cpp
class DeviceSyncController {
private:
    struct SyncGroup {
        QStringList deviceNames;
        QStringList readyDevices;
        bool triggered = false;
    };
    
    QHash<QString, SyncGroup> syncGroups_;
    QHash<QString, QWaitCondition*> syncConditions_;
    QMutex groupsMutex_;

public:
    // 同步组管理
    void createSyncGroup(const QString& groupName, const QStringList& devices);
    bool waitForSync(const QString& groupName, const QString& device, int timeout);
    void triggerSync(const QString& groupName);
};
```

## 核心算法实现

### 元件诊断算法

#### 电阻器诊断算法

```cpp
class ResistorDiagnostic : public BaseComponentDiagnostic {
public:
    ComponentDiagnosticResult diagnose(const ComponentSpec& spec) override {
        ComponentDiagnosticResult result;
        
        // 1. 准备测试配置
        TestConfiguration config = prepareResistorTest(spec);
        
        // 2. 执行电阻测量
        MeasurementData resistance = measureResistance(config);
        MeasurementData voltage = measureVoltage(config);
        MeasurementData current = measureCurrent(config);
        
        // 3. 计算电阻值
        double calculatedR = calculateResistance(voltage, current);
        
        // 4. 故障分析
        AnalysisResult analysis = analyzeResistorFaults(
            calculatedR, spec.nominal_value, spec.tolerance_percent);
        
        // 5. 生成结果
        result = generateDiagnosticResult(analysis, {resistance, voltage, current});
        
        return result;
    }

private:
    AnalysisResult analyzeResistorFaults(double measured, double nominal, double tolerance) {
        AnalysisResult result;
        
        double deviation = abs(measured - nominal) / nominal * 100.0;
        
        if (measured < 1.0) {
            result.faultType = "SHORT_CIRCUIT";
            result.isPassed = false;
            result.confidence = 0.95;
        } else if (measured > nominal * 1000) {
            result.faultType = "OPEN_CIRCUIT"; 
            result.isPassed = false;
            result.confidence = 0.90;
        } else if (deviation > tolerance) {
            result.faultType = "OUT_OF_TOLERANCE";
            result.isPassed = false;
            result.confidence = 0.85;
        } else {
            result.faultType = "COMPONENT_OK";
            result.isPassed = true;
            result.confidence = 0.95;
        }
        
        result.healthScore = 100.0 * (1.0 - deviation / (tolerance * 2.0));
        result.calculatedValue = measured;
        result.deviation = deviation;
        
        return result;
    }
};
```

#### 电容器诊断算法

```cpp
class CapacitorDiagnostic : public BaseComponentDiagnostic {
private:
    ComponentDiagnosticResult diagnoseCapacitor(const ComponentSpec& spec) {
        // 1. AC阻抗测量
        MeasurementData impedance = measureACImpedance(spec, 1000.0); // 1kHz
        
        // 2. 计算电容值
        double frequency = 1000.0;
        double capacitance = 1.0 / (2 * M_PI * frequency * impedance.value);
        
        // 3. ESR测量 (等效串联电阻)
        MeasurementData esr = measureESR(spec);
        
        // 4. 漏电流测量
        MeasurementData leakage = measureLeakageCurrent(spec);
        
        // 5. 综合分析
        return analyzeCapacitorHealth(capacitance, esr.value, leakage.value, spec);
    }
    
    AnalysisResult analyzeCapacitorHealth(double C, double esr, double leakage, 
                                         const ComponentSpec& spec) {
        AnalysisResult result;
        
        double nominalC = spec.nominal_value;
        double tolerance = spec.tolerance_percent;
        double maxESR = spec.max_esr;
        double maxLeakage = spec.max_leakage;
        
        bool capacitanceOK = isWithinTolerance(C, nominalC, tolerance);
        bool esrOK = (esr <= maxESR);
        bool leakageOK = (leakage <= maxLeakage);
        
        if (!capacitanceOK) {
            result.faultTypes.append("CAPACITANCE_OUT_OF_TOLERANCE");
        }
        if (!esrOK) {
            result.faultTypes.append("HIGH_ESR");
        }
        if (!leakageOK) {
            result.faultTypes.append("HIGH_LEAKAGE");
        }
        
        result.isPassed = capacitanceOK && esrOK && leakageOK;
        result.healthScore = calculateCapacitorHealthScore(C, esr, leakage, spec);
        
        return result;
    }
};
```

### 图像处理和AI算法

#### YOLO元件检测

```cpp
class YOLOModel {
private:
    cv::dnn::Net net_;
    std::vector<std::string> classNames_;
    std::vector<cv::Scalar> colors_;
    
public:
    std::vector<Label> detect(const cv::Mat& image, float confThreshold = 0.6) {
        // 1. 图像预处理
        cv::Mat blob;
        cv::dnn::blobFromImage(image, blob, 1/255.0, cv::Size(640, 640), 
                              cv::Scalar(0,0,0), true, false);
        
        // 2. 网络推理
        net_.setInput(blob);
        std::vector<cv::Mat> outputs;
        net_.forward(outputs, getOutputNames());
        
        // 3. 后处理
        std::vector<Label> detections;
        postprocess(image, outputs, confThreshold, detections);
        
        return detections;
    }
    
private:
    void postprocess(const cv::Mat& frame, const std::vector<cv::Mat>& outs,
                    float confThreshold, std::vector<Label>& detections) {
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;
        
        // 解析网络输出
        for (size_t i = 0; i < outs.size(); ++i) {
            float* data = (float*)outs[i].data;
            for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols) {
                cv::Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
                cv::Point classIdPoint;
                double confidence;
                minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
                
                if (confidence > confThreshold) {
                    int centerX = (int)(data[0] * frame.cols);
                    int centerY = (int)(data[1] * frame.rows);
                    int width = (int)(data[2] * frame.cols);
                    int height = (int)(data[3] * frame.rows);
                    
                    boxes.push_back(cv::Rect(centerX - width/2, centerY - height/2, 
                                           width, height));
                    classIds.push_back(classIdPoint.x);
                    confidences.push_back((float)confidence);
                }
            }
        }
        
        // NMS非极大值抑制
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, confThreshold, 0.4, indices);
        
        // 生成最终检测结果
        for (size_t i = 0; i < indices.size(); ++i) {
            int idx = indices[i];
            Label label;
            label.class_id = classIds[idx];
            label.class_name = classNames_[classIds[idx]];
            label.confidence = confidences[idx];
            label.x = boxes[idx].x;
            label.y = boxes[idx].y;
            label.width = boxes[idx].width;
            label.height = boxes[idx].height;
            
            detections.push_back(label);
        }
    }
};
```

#### SIFT特征匹配PCB识别

```cpp
class SiftMatcher {
private:
    cv::Ptr<cv::SIFT> detector_;
    cv::Ptr<cv::DescriptorMatcher> matcher_;
    std::vector<PCBModel> models_;
    
public:
    std::vector<MatchResult> matchImage(const cv::Mat& queryImage) {
        // 1. 提取查询图像特征
        std::vector<cv::KeyPoint> queryKeypoints;
        cv::Mat queryDescriptors;
        detector_->detectAndCompute(queryImage, cv::noArray(), 
                                   queryKeypoints, queryDescriptors);
        
        std::vector<MatchResult> results;
        
        // 2. 与数据库中每个模型匹配
        for (const auto& model : models_) {
            MatchResult result = matchWithModel(queryDescriptors, model);
            if (result.matchScore > 0.3) {  // 阈值过滤
                results.push_back(result);
            }
        }
        
        // 3. 按匹配分数排序
        std::sort(results.begin(), results.end(), 
                 [](const MatchResult& a, const MatchResult& b) {
                     return a.matchScore > b.matchScore;
                 });
        
        return results;
    }
    
private:
    MatchResult matchWithModel(const cv::Mat& queryDesc, const PCBModel& model) {
        MatchResult result;
        result.boardId = model.boardId;
        
        // FLANN匹配
        std::vector<std::vector<cv::DMatch>> knnMatches;
        matcher_->knnMatch(queryDesc, model.descriptors, knnMatches, 2);
        
        // Lowe's ratio test
        std::vector<cv::DMatch> goodMatches;
        for (const auto& match : knnMatches) {
            if (match.size() == 2 && match[0].distance < 0.7 * match[1].distance) {
                goodMatches.push_back(match[0]);
            }
        }
        
        result.matchCount = goodMatches.size();
        result.matchScore = calculateMatchScore(goodMatches, queryDesc.rows);
        
        return result;
    }
    
    double calculateMatchScore(const std::vector<cv::DMatch>& matches, int queryFeatures) {
        if (queryFeatures == 0) return 0.0;
        
        double ratio = (double)matches.size() / queryFeatures;
        double avgDistance = 0.0;
        
        for (const auto& match : matches) {
            avgDistance += match.distance;
        }
        avgDistance /= matches.size();
        
        // 综合匹配比例和平均距离
        double score = ratio * (1.0 - avgDistance / 256.0);
        return std::min(1.0, score);
    }
};
```

### 温度监测和红外处理

#### 红外摄像头数据处理

```cpp
class CameraManager {
private:
    struct IRCameraData {
        void* tempStreamer;
        ThermalData latestThermalData;
        mutable QMutex thermalMutex;
        double temperatureThreshold_{80.0};
    };
    
    // 温度数据回调处理
    static void temperatureCallback(uint16_t *rawData, uint32_t width, 
                                   uint32_t height, uint64_t pts, void *arg) {
        CameraManager* manager = static_cast<CameraManager*>(arg);
        manager->processTemperatureData(rawData, width, height, pts);
    }
    
    void processTemperatureData(uint16_t* rawData, uint32_t width, 
                               uint32_t height, uint64_t pts) {
        QMutexLocker locker(&irCamera_.thermalMutex);
        
        ThermalData& data = irCamera_.latestThermalData;
        data.width = width;
        data.height = height;
        data.rawTempData = rawData;
        data.timestamp = QDateTime::currentDateTime();
        
        // 计算温度统计
        calculateTemperatureStats(data);
        
        // 生成彩色热图
        generateThermalColorMap(data);
        
        // 检查温度报警
        checkTemperatureAlerts(data);
        
        data.isValid = true;
        
        // 发射信号
        emit newThermalDataAvailable(data);
    }
    
    void calculateTemperatureStats(ThermalData& data) {
        if (!data.rawTempData) return;
        
        double minTemp = 1000.0;
        double maxTemp = -273.15;
        double totalTemp = 0.0;
        int pixelCount = data.width * data.height;
        
        for (int i = 0; i < pixelCount; ++i) {
            double temp = convertRawToTemperature(data.rawTempData[i]);
            minTemp = std::min(minTemp, temp);
            maxTemp = std::max(maxTemp, temp);
            totalTemp += temp;
        }
        
        data.minTemp = minTemp;
        data.maxTemp = maxTemp;
        data.avgTemp = totalTemp / pixelCount;
    }
    
    void generateThermalColorMap(ThermalData& data) {
        cv::Mat tempMap(data.height, data.width, CV_32F);
        
        // 转换原始数据到温度值
        for (uint32_t y = 0; y < data.height; ++y) {
            for (uint32_t x = 0; x < data.width; ++x) {
                uint16_t rawValue = data.rawTempData[y * data.width + x];
                float temp = convertRawToTemperature(rawValue);
                tempMap.at<float>(y, x) = temp;
            }
        }
        
        // 归一化到0-255
        cv::Mat normalized;
        cv::normalize(tempMap, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
        
        // 应用JET颜色映射
        cv::applyColorMap(normalized, data.thermalColorMap, cv::COLORMAP_JET);
        data.temperatureMap = tempMap;
    }
    
    void checkTemperatureAlerts(const ThermalData& data) {
        if (data.maxTemp > temperatureThreshold_) {
            // 找到最高温度点位置
            cv::Point maxLoc;
            cv::Mat tempMat(data.height, data.width, CV_32F);
            
            for (uint32_t y = 0; y < data.height; ++y) {
                for (uint32_t x = 0; x < data.width; ++x) {
                    tempMat.at<float>(y, x) = convertRawToTemperature(
                        data.rawTempData[y * data.width + x]);
                }
            }
            
            cv::minMaxLoc(tempMat, nullptr, nullptr, nullptr, &maxLoc);
            
            emit temperatureAlert(data.maxTemp, maxLoc);
        }
    }
};
```

## 数据管理和持久化

### 数据库设计

#### PCB板卡数据库

```cpp
class PCBBoardManager {
private:
    QString database_path_;
    QList<PCBBoardInfo> boards_;
    
    bool saveBoardsToDatabase() {
        QJsonObject root;
        QJsonArray boardsArray;
        
        for (const PCBBoardInfo& board : boards_) {
            boardsArray.append(board.toJson());
        }
        
        root["boards"] = boardsArray;
        root["version"] = "1.0";
        root["lastModified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        QJsonDocument doc(root);
        QFile file(database_path_);
        
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        
        file.write(doc.toJson());
        return true;
    }
    
    void loadBoardsFromDatabase() {
        QFile file(database_path_);
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
        
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root = doc.object();
        QJsonArray boardsArray = root["boards"].toArray();
        
        boards_.clear();
        for (const QJsonValue& value : boardsArray) {
            PCBBoardInfo board = PCBBoardInfo::fromJson(value.toObject());
            boards_.append(board);
        }
    }
};
```

#### 检测记录数据库

```cpp
class PCBDetectionManager {
private:
    QList<PCBDetectionRecord> records_;
    QString database_path_;
    QMutex records_mutex_;
    
public:
    bool saveDetectionRecord(const PCBDetectionRecord& record) {
        QMutexLocker locker(&records_mutex_);
        
        PCBDetectionRecord newRecord = record;
        if (newRecord.id.isEmpty()) {
            newRecord.id = generateUniqueId();
        }
        if (newRecord.timestamp.isNull()) {
            newRecord.timestamp = QDateTime::currentDateTime();
        }
        
        records_.append(newRecord);
        
        // 限制记录数量
        while (records_.size() > max_records_count_) {
            PCBDetectionRecord oldRecord = records_.takeFirst();
            deleteImageFiles(oldRecord.id);  // 清理关联的图像文件
        }
        
        emit recordAdded(newRecord.id);
        return true;
    }
    
    QString saveAnnotatedImage(const cv::Mat& image, const std::vector<Label>& labels,
                              const std::vector<std::string>& classNames,
                              const std::vector<cv::Scalar>& colors,
                              const QString& prefix) {
        if (image.empty()) {
            return QString();
        }
        
        cv::Mat annotatedImage = createAnnotatedImage(image, labels, classNames, colors);
        return saveImage(annotatedImage, prefix);
    }
    
private:
    cv::Mat createAnnotatedImage(const cv::Mat& originalImage,
                                const std::vector<Label>& labels,
                                const std::vector<std::string>& classNames,
                                const std::vector<cv::Scalar>& colors) {
        cv::Mat annotated = originalImage.clone();
        
        for (const Label& label : labels) {
            // 绘制边界框
            cv::Rect boundingBox(label.x, label.y, label.width, label.height);
            cv::Scalar color = colors[label.class_id % colors.size()];
            cv::rectangle(annotated, boundingBox, color, 2);
            
            // 绘制标签文本
            QString labelText = QString("%1: %.2f").arg(label.class_name.c_str())
                                                   .arg(label.confidence);
            cv::putText(annotated, labelText.toStdString(), 
                       cv::Point(label.x, label.y - 5),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
        }
        
        return annotated;
    }
};
```

### 结果导出系统

```cpp
class ResultExporter {
public:
    bool exportToPDF(const QVector<DiagnosticResult>& results, const QString& fileName) {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        
        QPainter painter(&printer);
        QTextDocument document;
        
        // 生成HTML报告内容
        QString htmlContent = generateHTMLReport(results);
        document.setHtml(htmlContent);
        
        // 渲染PDF
        document.print(&printer);
        
        emit exportCompleted(fileName);
        return true;
    }
    
    bool exportToCSV(const QVector<DiagnosticResult>& results, const QString& fileName) {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        
        // CSV头部
        out << "Timestamp,Component,Reference,Type,Result,Health Score,Confidence,Faults,Notes\n";
        
        // 数据行
        for (const DiagnosticResult& result : results) {
            QStringList row = {
                result.timestamp.toString(Qt::ISODate),
                result.componentType,
                result.componentId,
                componentTypeToString(static_cast<ComponentType>(result.componentType.toInt())),
                (result.result == DiagnosticResult::PASS ? "PASS" : "FAIL"),
                QString::number(result.healthScore, 'f', 2),
                QString::number(result.confidence, 'f', 2),
                result.faultTypes.join(";"),
                result.notes
            };
            
            out << row.join(",") << "\n";
        }
        
        emit exportCompleted(fileName);
        return true;
    }
    
private:
    QString generateHTMLReport(const QVector<DiagnosticResult>& results) {
        QString html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>PCB故障检测报告</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 20px; border-radius: 5px; }
        .summary { margin: 20px 0; }
        .results { width: 100%; border-collapse: collapse; }
        .results th, .results td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        .results th { background-color: #f2f2f2; }
        .pass { color: green; font-weight: bold; }
        .fail { color: red; font-weight: bold; }
    </style>
</head>
<body>
)";
        
        // 添加报告头部
        html += QString(R"(
    <div class="header">
        <h1>PCB故障检测报告</h1>
        <p>生成时间: %1</p>
        <p>测试数量: %2</p>
    </div>
)").arg(QDateTime::currentDateTime().toString(Qt::DefaultLocaleLong