#ifndef COMPONENTDIAGNOSTICFRAMEWORK_H
#define COMPONENTDIAGNOSTICFRAMEWORK_H

/**
 * @file componentdiagnosticframework.h
 * @brief 组件诊断框架主头文件
 * 
 * 这个文件提供了组件诊断框架的主要接口和便利函数，
 * 简化了框架的使用和集成。
 */

#include "basecomponentdiagnostic.h"
#include "componentdiagnosticmanager.h"
#include "Components/resistordiagnostic.h"
#include "Components/capacitordiagnostic.h"
// #include "Components/inductordiagnostic.h"
// #include "Components/diodediagnostic.h"
// #include "Components/icdiagnostic.h"

/**
 * @namespace ComponentDiagnosticFramework
 * @brief 组件诊断框架命名空间
 * 
 * 包含了框架的工具函数和便利接口
 */
namespace ComponentDiagnosticFramework {

/**
 * @brief 初始化组件诊断框架
 * @param deviceManager 设备管理器指针
 * @param parent 父对象
 * @return 配置好的诊断管理器实例
 */
ComponentDiagnosticManager* createDiagnosticManager(DeviceManager* deviceManager, QObject* parent = nullptr);

/**
 * @brief 注册所有内置的组件诊断器
 * @param manager 诊断管理器
 * @param deviceManager 设备管理器
 * @return 注册成功的诊断器数量
 */
int registerBuiltinDiagnostics(ComponentDiagnosticManager* manager, DeviceManager* deviceManager);

/**
 * @brief 创建指定类型的组件诊断器
 * @param type 组件类型
 * @param deviceManager 设备管理器
 * @param parent 父对象
 * @return 诊断器实例（调用者负责管理生命周期）
 */
BaseComponentDiagnostic* createComponentDiagnostic(ComponentType type, DeviceManager* deviceManager, QObject* parent = nullptr);

/**
 * @brief 获取组件类型的显示名称
 * @param type 组件类型
 * @return 显示名称
 */
QString getComponentTypeDisplayName(ComponentType type);

/**
 * @brief 获取所有支持的组件类型
 * @return 组件类型列表
 */
QVector<ComponentType> getAllSupportedComponentTypes();

/**
 * @brief 验证组件规格的完整性
 * @param component 组件规格
 * @param errors 错误信息输出
 * @return 是否有效
 */
bool validateComponentSpec(const ComponentSpec& component, QStringList& errors);

/**
 * @brief 创建默认的组件规格
 * @param type 组件类型
 * @param reference 组件标识
 * @param nominalValue 标称值
 * @param tolerance 容差百分比
 * @return 组件规格
 */
ComponentSpec createDefaultComponentSpec(ComponentType type, const QString& reference, 
                                        double nominalValue, double tolerance = 5.0);

/**
 * @brief 格式化诊断结果为可读字符串
 * @param result 诊断结果
 * @param detailed 是否显示详细信息
 * @return 格式化的字符串
 */
QString formatDiagnosticResult(const ComponentDiagnosticResult& result, bool detailed = true);

/**
 * @brief 将诊断结果导出为JSON格式
 * @param result 诊断结果
 * @return JSON字符串
 */
QString exportResultToJson(const ComponentDiagnosticResult& result);

/**
 * @brief 从JSON导入诊断结果
 * @param jsonString JSON字符串
 * @param result 结果输出
 * @return 是否成功导入
 */
bool importResultFromJson(const QString& jsonString, ComponentDiagnosticResult& result);

/**
 * @brief 生成诊断报告
 * @param results 诊断结果列表
 * @param format 报告格式（"html", "txt", "csv"）
 * @return 报告内容
 */
QString generateDiagnosticReport(const QVector<ComponentDiagnosticResult>& results, const QString& format = "html");

/**
 * @brief 获取推荐的测试配置
 * @param component 组件规格
 * @return 推荐的测试配置参数
 */
QMap<QString, QVariant> getRecommendedTestConfig(const ComponentSpec& component);

/**
 * @brief 计算诊断置信度
 * @param result 诊断结果
 * @return 置信度分数 (0-1)
 */
double calculateDiagnosticConfidence(const ComponentDiagnosticResult& result);

/**
 * @brief 框架版本信息
 */
struct FrameworkInfo {
    QString version;
    QString buildDate;
    QString description;
    QStringList supportedComponents;
};

/**
 * @brief 获取框架信息
 * @return 框架信息结构
 */
FrameworkInfo getFrameworkInfo();

/**
 * @brief 错误代码定义
 */
enum class ErrorCode {
    SUCCESS = 0,
    INVALID_COMPONENT_SPEC = 1001,
    UNSUPPORTED_COMPONENT_TYPE = 1002,
    DEVICE_NOT_READY = 1003,
    PORT_ALLOCATION_FAILED = 1004,
    WIRING_SETUP_FAILED = 1005,
    DATA_ACQUISITION_FAILED = 1006,
    ANALYSIS_FAILED = 1007,
    TIMEOUT = 1008,
    CANCELLED = 1009,
    INTERNAL_ERROR = 1010
};

/**
 * @brief 获取错误代码的描述
 * @param code 错误代码
 * @return 错误描述
 */
QString getErrorDescription(ErrorCode code);

/**
 * @brief 诊断框架异常类
 */
class DiagnosticException : public std::exception
{
public:
    DiagnosticException(ErrorCode code, const QString& message = QString())
        : code_(code), message_(message.isEmpty() ? getErrorDescription(code) : message) {}
    
    ErrorCode code() const { return code_; }
    QString message() const { return message_; }
    
    const char* what() const noexcept override {
        return message_.toLocal8Bit().constData();
    }
    
private:
    ErrorCode code_;
    QString message_;
};

} // namespace ComponentDiagnosticFramework

// 便利宏定义
#define DIAGNOSTIC_FRAMEWORK_VERSION "1.0.0"
#define DIAGNOSTIC_FRAMEWORK_BUILD_DATE __DATE__

// 为了方便使用，提供一些全局便利函数
using namespace ComponentDiagnosticFramework;

/**
 * @brief 快速诊断单个组件（便利函数）
 * @param component 组件规格
 * @param deviceManager 设备管理器
 * @return 诊断结果
 */
inline ComponentDiagnosticResult quickDiagnose(const ComponentSpec& component, DeviceManager* deviceManager)
{
    auto diagnostic = createComponentDiagnostic(component.type, deviceManager);
    if (!diagnostic) {
        ComponentDiagnosticResult result;
        result.isPassed = false;
        result.summary = "不支持的组件类型";
        return result;
    }
    
    auto result = diagnostic->diagnoseComponent(component);
    delete diagnostic;
    return result;
}

#endif // COMPONENTDIAGNOSTICFRAMEWORK_H
