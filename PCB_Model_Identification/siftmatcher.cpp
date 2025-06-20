#include "siftmatcher.h"
#include <iostream>

void SiftMatcher::saveDescriptors(const std::string& filename,
                                  const std::vector<cv::Mat>& descriptors,
                                  const std::vector<std::string>& imageNames) {
    cv::FileStorage fs(filename, cv::FileStorage::WRITE);

    fs << "image_count" << (int)descriptors.size();

    for (size_t i = 0; i < descriptors.size(); i++) {
        fs << "image_name_" + std::to_string(i) << imageNames[i];
        fs << "descriptor_" + std::to_string(i) << descriptors[i];
    }

    fs.release();
}

void SiftMatcher::loadDescriptors(const std::string& filename,
                                  std::vector<cv::Mat>& descriptors,
                                  std::vector<std::string>& imageNames) {
    cv::FileStorage fs(filename, cv::FileStorage::READ);

    int image_count;
    fs["image_count"] >> image_count;

    for (int i = 0; i < image_count; i++) {
        cv::Mat descriptor;
        std::string name;

        fs["image_name_" + std::to_string(i)] >> name;
        fs["descriptor_" + std::to_string(i)] >> descriptor;

        descriptors.push_back(descriptor);
        imageNames.push_back(name);
    }

    fs.release();
}

SiftMatcher::SiftMatcher() {
    if (checkGPU()) {
        try {
            // 使用ORB替代SIFT（因为CUDA不直接支持SIFT）
            gpu_detector = cv::cuda::ORB::create(
                500,    // nfeatures
                1.2f,   // scaleFactor
                8,      // nlevels
                31,     // edgeThreshold
                0,      // firstLevel
                2,      // WTA_K
                cv::ORB::HARRIS_SCORE,
                31      // patchSize
            );
            gpu_matcher = cv::cuda::DescriptorMatcher::createBFMatcher(cv::NORM_L2);
            std::cout << "GPU acceleration enabled" << std::endl;
        } catch (const cv::Exception& e) {
            std::cout << "GPU initialization failed: " << e.what() << std::endl;
            gpu_detector.release();
            gpu_matcher.release();
        }
    } else {
        std::cout << "Warning: GPU not available, falling back to CPU implementation" << std::endl;
    }
}

SiftMatcher::~SiftMatcher() {
    gpu_detector.release();
    gpu_matcher.release();
}

bool SiftMatcher::checkGPU() {
    try {
        int deviceCount = cv::cuda::getCudaEnabledDeviceCount();
        if (deviceCount == 0) {
            std::cout << "No CUDA devices found" << std::endl;
            return false;
        }
        
        cv::cuda::DeviceInfo deviceInfo;
        if (!deviceInfo.isCompatible()) {
            std::cout << "CUDA device is not compatible" << std::endl;
            return false;
        }
        
        cv::cuda::setDevice(0);
        std::cout << "Using CUDA device: " << deviceInfo.name() << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cout << "CUDA initialization failed: " << e.what() << std::endl;
        return false;
    }
}

cv::Mat SiftMatcher::extractDescriptor(const std::string& imagePath, int rotation) {
    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cout << "Failed to load image: " << imagePath << std::endl;
        return cv::Mat();
    }

    // 转换为灰度图
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    if (rotation >= 0) {
        switch (rotation) {
            case 0: cv::rotate(gray, gray, cv::ROTATE_90_CLOCKWISE); break;
            case 1: cv::rotate(gray, gray, cv::ROTATE_180); break;
            case 2: cv::rotate(gray, gray, cv::ROTATE_90_COUNTERCLOCKWISE); break;
        }
    }

    if (gpu_detector) {
        try {
            cv::cuda::GpuMat gpu_image(gray);
            cv::cuda::GpuMat gpu_descriptors;
            std::vector<cv::KeyPoint> keypoints;

            gpu_detector->detectAndCompute(gpu_image, cv::cuda::GpuMat(), keypoints, gpu_descriptors);

            cv::Mat descriptors;
            gpu_descriptors.download(descriptors);
            
            if (!descriptors.empty()) {
                descriptors.convertTo(descriptors, CV_32F);
            }
            
            return descriptors;
        } catch (const cv::Exception& e) {
            std::cout << "GPU feature extraction failed: " << e.what() << std::endl;
        }
    }

    // 降级到CPU实现
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    sift->detectAndCompute(gray, cv::noArray(), keypoints, descriptors);
    return descriptors;
}

void SiftMatcher::createDatabase(const std::vector<std::string>& imageFiles) {
    std::vector<cv::Mat> descriptors;
    std::vector<std::string> imageNames;

    for (const auto& imagePath : imageFiles) {
        for (int rotation = -1; rotation < 3; rotation++) {
            cv::Mat desc = extractDescriptor(imagePath, rotation);
            if (!desc.empty()) {
                descriptors.push_back(desc);
                imageNames.push_back(imagePath);

                std::string rotationStr;
                switch(rotation) {
                case -1: rotationStr = "Original"; break;
                case 0: rotationStr = "90°"; break;
                case 1: rotationStr = "180°"; break;
                case 2: rotationStr = "270°"; break;
                }
                std::cout << "Added: " << imagePath << " (" << rotationStr << ")" << std::endl;
            }
        }
    }

    saveDescriptors("descriptors_database.yml", descriptors, imageNames);
    std::cout << "Database created successfully!" << std::endl;
    std::cout << "Total descriptors saved: " << descriptors.size()
              << " (4 rotations for each of " << imageFiles.size() << " images)" << std::endl;
}

SiftMatcher::MatchResult SiftMatcher::computeMatchScore(const cv::Mat& queryDesc,
                                                        const cv::Mat& trainDesc,
                                                        const std::string& trainImagePath) {
    std::vector<cv::DMatch> matches;
    
    try {
        if (gpu_matcher) {
            cv::Mat queryDesc32F, trainDesc32F;
            queryDesc.convertTo(queryDesc32F, CV_32F);
            trainDesc.convertTo(trainDesc32F, CV_32F);

            cv::cuda::GpuMat gpu_queryDesc(queryDesc32F);
            cv::cuda::GpuMat gpu_trainDesc(trainDesc32F);
            
            gpu_matcher->match(gpu_queryDesc, gpu_trainDesc, matches);
        } else {
            cv::Ptr<cv::FlannBasedMatcher> matcher = cv::FlannBasedMatcher::create();
            matcher->match(queryDesc, trainDesc, matches);
        }
    } catch (const cv::Exception& e) {
        std::cout << "Matching failed: " << e.what() << std::endl;
        return {trainImagePath, 0, 0.0};
    }

    double minDist = 100000, maxDist = 0;
    for (const auto& match : matches) {
        double dist = match.distance;
        if (dist < minDist) minDist = dist;
        if (dist > maxDist) maxDist = dist;
    }

    std::vector<cv::DMatch> goodMatches;
    double totalScore = 0;
    for (const auto& match : matches) {
        if (match.distance <= std::max(2 * minDist, 30.0)) {
            goodMatches.push_back(match);
            totalScore += 1.0 - (match.distance / std::max(maxDist, 0.1));
        }
    }

    double matchScore = 0;
    if (!goodMatches.empty()) {
        matchScore = goodMatches.size() * (totalScore / goodMatches.size());
        if (goodMatches.size() > 1000) {
            matchScore *= 10;
        }
    }

    return {trainImagePath, (int)goodMatches.size(), matchScore};
}

std::vector<SiftMatcher::MatchResult> SiftMatcher::matchImage(const std::string& queryImagePath) {
    std::vector<cv::Mat> loadedDescriptors;
    std::vector<std::string> loadedNames;
    loadDescriptors("descriptors_database.yml", loadedDescriptors, loadedNames);

    cv::Mat queryDesc = extractDescriptor(queryImagePath);
    if (queryDesc.empty()) {
        return {};
    }

    std::vector<MatchResult> results;
    for (size_t i = 0; i < loadedDescriptors.size(); i++) {
        results.push_back(computeMatchScore(queryDesc, loadedDescriptors[i], loadedNames[i]));
    }

    std::sort(results.begin(), results.end());
    return results;
}

void SiftMatcher::appendToDatabase(const std::vector<std::string>& imageFiles) {
    // 首先加载现有数据库
    std::vector<cv::Mat> descriptors;
    std::vector<std::string> imageNames;
    
    try {
        loadDescriptors("descriptors_database.yml", descriptors, imageNames);
    } catch (const cv::Exception& e) {
        // 如果文件不存在或无法读取，就从空开始
        std::cout << "Creating new database" << std::endl;
    }

    // 添加新图片的描述符
    for (const auto& imagePath : imageFiles) {
        for (int rotation = -1; rotation < 3; rotation++) {
            cv::Mat desc = extractDescriptor(imagePath, rotation);
            if (!desc.empty()) {
                descriptors.push_back(desc);
                
                // 获取文件名（不含路径和扩展名）
                std::string filename = imagePath;
                size_t lastSlash = filename.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    filename = filename.substr(lastSlash + 1);
                }
                size_t lastDot = filename.find_last_of(".");
                if (lastDot != std::string::npos) {
                    filename = filename.substr(0, lastDot);
                }
                imageNames.push_back(filename);

                std::string rotationStr;
                switch(rotation) {
                case -1: rotationStr = "Original"; break;
                case 0: rotationStr = "90°"; break;
                case 1: rotationStr = "180°"; break;
                case 2: rotationStr = "270°"; break;
                }
                std::cout << "Added: " << imagePath << " (" << rotationStr << ")" << std::endl;
            }
        }
    }

    // 保存更新后的数据库
    saveDescriptors("descriptors_database.yml", descriptors, imageNames);
    std::cout << "Database updated successfully!" << std::endl;
    std::cout << "Total descriptors saved: " << descriptors.size() 
              << " (including " << imageFiles.size() << " new images)" << std::endl;
}

bool SiftMatcher::removeFromDatabase(const std::string& DeleteImage) {
    std::vector<cv::Mat> descriptors;
    std::vector<std::string> imageNames;
    bool found = false;

    try {
        // 加载现有数据库
        loadDescriptors("descriptors_database.yml", descriptors, imageNames);

        // 创建新的向量来存储保留的数据
        std::vector<cv::Mat> newDescriptors;
        std::vector<std::string> newImageNames;

        // 遍历所有条目，跳过要删除的图片
        for (size_t i = 0; i < imageNames.size(); i++) {
            if (imageNames[i] != DeleteImage) {
                newDescriptors.push_back(descriptors[i]);
                newImageNames.push_back(imageNames[i]);
            } else {
                found = true;
            }
        }

        // 如果找到并删除了图片，保存更新后的数据库
        if (found) {
            saveDescriptors("descriptors_database.yml", newDescriptors, newImageNames);
            std::cout << "Successfully removed " << DeleteImage << " from database" << std::endl;
            std::cout << "Remaining entries: " << newDescriptors.size() << std::endl;
        } else {
            std::cout << "Image " << DeleteImage << " not found in database" << std::endl;
        }

        return found;

    } catch (const cv::Exception& e) {
        std::cout << "Error while removing image from database: " << e.what() << std::endl;
        return false;
    }
}
