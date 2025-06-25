// test_refactored_diagnostic.cpp
// 测试重构后的诊断系统

#include "faultdiagnostic.h"
#include "devicemanager.h"
#include "commontypes.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

void testBasicDiagnostic()
{
    qDebug() << "=== 基础诊断功能测试 ===";
    
    // 创建设备管理器（模拟）
    DeviceManager* deviceManager = new DeviceManager();
    
    // 创建故障诊断器
    FaultDiagnostic* diagnostic = new FaultDiagnostic(deviceManager);
    
    // 检查支持的组件类型
    QStringList supportedTypes = diagnostic->getSupportedComponentTypes();
    qDebug() << "支持的组件类型:" << supportedTypes;
    
    // 测试电阻器诊断
    ComponentSpec resistor;
    resistor.reference = "R1";
    resistor.type = ComponentType::RESISTOR;
    resistor.nominal_value = 1000.0;  // 1kΩ
    resistor.tolerance_percent = 5.0;
    resistor.channel = 0;
    
    qDebug() << "开始测试电阻器诊断...";
    auto result = diagnostic->diagnoseComponent(resistor);
    
    qDebug() << "诊断结果:";
    qDebug() << "  组件ID:" << result.componentId;
    qDebug() << "  结果:" << (result.result == DiagnosticResult::PASS ? "PASS" : "FAIL");
    qDebug() << "  健康评分:" << result.healthScore;
    qDebug() << "  置信度:" << result.confidence;
    qDebug() << "  备注:" << result.notes;
    
    // 测试电容器诊断
    ComponentSpec capacitor;
    capacitor.reference = "C1";
    capacitor.type = ComponentType::CAPACITOR;
    capacitor.nominal_value = 1e-6;  // 1µF
    capacitor.tolerance_percent = 10.0;
    capacitor.channel = 1;
    
    qDebug() << "开始测试电容器诊断...";
    auto capResult = diagnostic->diagnoseComponent(capacitor);
    
    qDebug() << "电容器诊断结果:";
    qDebug() << "  组件ID:" << capResult.componentId;
    qDebug() << "  结果:" << (capResult.result == DiagnosticResult::PASS ? "PASS" : "FAIL");
    qDebug() << "  健康评分:" << capResult.healthScore;
    
    // 测试批量诊断
    QVector<ComponentSpec> components;
    components.append(resistor);
    components.append(capacitor);
    
    qDebug() << "开始测试批量诊断...";
    auto batchResults = diagnostic->diagnoseBatch(components);
    
    qDebug() << "批量诊断完成，共" << batchResults.size() << "个结果";
    
    // 清理
    delete diagnostic;
    delete deviceManager;
    
    qDebug() << "基础诊断功能测试完成";
}

void testAsyncDiagnostic()
{
    qDebug() << "=== 异步诊断功能测试 ===";
    
    DeviceManager* deviceManager = new DeviceManager();
    FaultDiagnostic* diagnostic = new FaultDiagnostic(deviceManager);
    
    // 连接信号用于异步测试
    QObject::connect(diagnostic, &FaultDiagnostic::componentDiagnosisCompleted,
                     [](const QString& taskId, const QString& componentId, const DiagnosticResult& result) {
        qDebug() << "异步诊断完成 - 任务ID:" << taskId << "组件:" << componentId;
        qDebug() << "结果:" << (result.result == DiagnosticResult::PASS ? "PASS" : "FAIL");
    });
    
    ComponentSpec testComponent;
    testComponent.reference = "R_ASYNC";
    testComponent.type = ComponentType::RESISTOR;
    testComponent.nominal_value = 2200.0;
    testComponent.tolerance_percent = 5.0;
    
    QString taskId = diagnostic->diagnoseComponentAsync(testComponent);
    qDebug() << "启动异步诊断，任务ID:" << taskId;
    
    // 等待一段时间让异步操作完成
    QTimer::singleShot(1000, [=]() {
        auto status = diagnostic->getTaskStatus(taskId);
        qDebug() << "任务状态:" << status;
        
        // 清理
        delete diagnostic;
        delete deviceManager;
        
        qDebug() << "异步诊断功能测试完成";
    });
}

void testCompatibilityMethods()
{
    qDebug() << "=== 向后兼容性测试 ===";
    
    DeviceManager* deviceManager = new DeviceManager();
    FaultDiagnostic* diagnostic = new FaultDiagnostic(deviceManager);
    
    // 测试向后兼容的测量方法
    qDebug() << "测试向后兼容的测量方法...";
    
    auto resistance = diagnostic->measureResistance(0, 5.0, 1000.0);
    qDebug() << "电阻测量结果:" << resistance.primary_value << "Ω, 有效:" << resistance.valid;
    
    auto capacitance = diagnostic->measureCapacitance(1, 1000.0);
    qDebug() << "电容测量结果:" << capacitance.primary_value << "F, 有效:" << capacitance.valid;
    
    // 测试向后兼容的诊断方法
    ComponentSpec resistorSpec;
    resistorSpec.reference = "R_COMPAT";
    resistorSpec.nominal_value = 4700.0;
    resistorSpec.tolerance_percent = 5.0;
    
    auto resistorResult = diagnostic->diagnoseResistor(resistorSpec);
    qDebug() << "兼容性电阻诊断结果:" << (resistorResult.result == DiagnosticResult::PASS ? "PASS" : "FAIL");
    
    // 清理
    delete diagnostic;
    delete deviceManager;
    
    qDebug() << "向后兼容性测试完成";
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "开始测试重构后的故障诊断系统...";
    
    try {
        testBasicDiagnostic();
        testAsyncDiagnostic();
        testCompatibilityMethods();
        
        qDebug() << "所有测试完成!";
        
    } catch (const std::exception& e) {
        qCritical() << "测试过程中发生异常:" << e.what();
        return 1;
    }
    
    // 运行事件循环一段时间以处理异步操作
    QTimer::singleShot(2000, &app, &QCoreApplication::quit);
    return app.exec();
}
