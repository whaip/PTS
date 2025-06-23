// 测试重构后的FaultDiagnostic系统
#include "faultdiagnostic.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 模拟设备管理器
    DeviceManager deviceManager;
    
    // 创建FaultDiagnostic实例
    FaultDiagnostic faultDiagnostic(&deviceManager);
    
    qDebug() << "=== FaultDiagnostic 模块化重构测试 ===";
    
    // 测试1: 获取可用的测试方案
    qDebug() << "1. 可用测试方案:";
    QStringList schemes = faultDiagnostic.getAvailableTestSchemes();
    for (const QString& scheme : schemes) {
        qDebug() << "  -" << scheme;
    }
    
    // 测试2: 设置测试方案
    qDebug() << "\n2. 设置测试方案:";
    if (faultDiagnostic.setActiveTestScheme("电阻测试(标准)")) {
        qDebug() << "  成功设置方案:" << faultDiagnostic.getActiveTestScheme();
    } else {
        qDebug() << "  设置方案失败";
    }
    
    // 测试3: 检查端口配置状态
    qDebug() << "\n3. 端口配置状态:";
    qDebug() << "  已配置:" << (faultDiagnostic.isPortConfigured() ? "是" : "否");
    
    // 测试4: 执行完整工作流程
    qDebug() << "\n4. 执行完整诊断工作流程:";
    bool workflowResult = faultDiagnostic.executeFullDiagnosticWorkflow("resistor");
    qDebug() << "  工作流程结果:" << (workflowResult ? "成功" : "失败");
    
    // 测试5: 传统接口兼容性测试
    qDebug() << "\n5. 传统接口兼容性测试:";
    ComponentSpec testComponent;
    testComponent.type = ComponentType::RESISTOR;
    testComponent.reference = "R1";
    testComponent.nominal_value = 1000.0;
    testComponent.tolerance = 0.05;
    testComponent.channel = 1;
    testComponent.test_voltage = 1.0;
    
    DiagnosticResult result = faultDiagnostic.diagnoseComponent(testComponent);
    qDebug() << "  组件诊断结果:";
    qDebug() << "    组件ID:" << result.componentId;
    qDebug() << "    结果:" << result.result;
    qDebug() << "    健康度:" << result.healthScore;
    qDebug() << "    置信度:" << result.confidence;
    qDebug() << "    备注:" << result.notes;
    
    qDebug() << "\n=== 测试完成 ===";
    
    return 0;
}
