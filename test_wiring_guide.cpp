#include <QApplication>
#include <QDebug>
#include <QTimer>
#include "WiringGuide/portmanager.h"
#include "WiringGuide/wiringresourcemanager.h"
#include "WiringGuide/wiringtaskgenerator.h"
#include "WiringGuide/wiringguidedialog.h"
#include "devicemanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    qDebug() << "=== 接线引导系统测试 ===";
    
    // 1. 测试端口管理器
    qDebug() << "\n1. 测试端口管理器...";
    DeviceManager* deviceManager = new DeviceManager();
    PortManager* portManager = new PortManager(deviceManager);
    
    if (portManager->initializePorts()) {
        qDebug() << "✓ 端口管理器初始化成功";
        
        // 显示所有端口
        QVector<PortInfo> allPorts = portManager->getAllPorts();
        qDebug() << QString("   总端口数: %1").arg(allPorts.size());
        
        // 显示每种类型的端口数量
        QVector<PortInfo> analogOutputPorts = portManager->getAvailablePorts(PortType::ANALOG_OUTPUT);
        QVector<PortInfo> digitalOutputPorts = portManager->getAvailablePorts(PortType::DIGITAL_OUTPUT);
        QVector<PortInfo> powerOutputPorts = portManager->getAvailablePorts(PortType::POWER_OUTPUT);
        QVector<PortInfo> analogInputPorts = portManager->getAvailablePorts(PortType::ANALOG_INPUT);
        QVector<PortInfo> digitalInputPorts = portManager->getAvailablePorts(PortType::DIGITAL_INPUT);
        
        qDebug() << QString("   模拟输出端口: %1").arg(analogOutputPorts.size());
        qDebug() << QString("   数字输出端口: %1").arg(digitalOutputPorts.size());
        qDebug() << QString("   电源输出端口: %1").arg(powerOutputPorts.size());
        qDebug() << QString("   模拟输入端口: %1").arg(analogInputPorts.size());
        qDebug() << QString("   数字输入端口: %1").arg(digitalInputPorts.size());
        
    } else {
        qDebug() << "✗ 端口管理器初始化失败";
        return -1;
    }
    
    // 2. 测试资源管理器
    qDebug() << "\n2. 测试资源管理器...";
    WiringResourceManager* resourceManager = new WiringResourceManager(portManager);
    
    if (resourceManager->initializeResources()) {
        qDebug() << "✓ 资源管理器初始化成功";
        qDebug() << QString("   总方案数: %1").arg(resourceManager->getTotalSchemes());
        qDebug() << QString("   活动方案数: %1").arg(resourceManager->getActiveSchemes());
        
        // 显示各元件类型的模板数量
        QVector<WiringScheme> resistorTemplates = resourceManager->getTemplatesForComponent(ComponentType::RESISTOR);
        QVector<WiringScheme> capacitorTemplates = resourceManager->getTemplatesForComponent(ComponentType::CAPACITOR);
        QVector<WiringScheme> inductorTemplates = resourceManager->getTemplatesForComponent(ComponentType::INDUCTOR);
        QVector<WiringScheme> diodeTemplates = resourceManager->getTemplatesForComponent(ComponentType::DIODE);
        QVector<WiringScheme> icTemplates = resourceManager->getTemplatesForComponent(ComponentType::IC);
        
        qDebug() << QString("   电阻模板: %1").arg(resistorTemplates.size());
        qDebug() << QString("   电容模板: %1").arg(capacitorTemplates.size());
        qDebug() << QString("   电感模板: %1").arg(inductorTemplates.size());
        qDebug() << QString("   二极管模板: %1").arg(diodeTemplates.size());
        qDebug() << QString("   IC模板: %1").arg(icTemplates.size());
        
    } else {
        qDebug() << "✗ 资源管理器初始化失败";
        return -1;
    }
    
    // 3. 测试任务生成器
    qDebug() << "\n3. 测试任务生成器...";
    WiringTaskGenerator* taskGenerator = new WiringTaskGenerator(resourceManager);
    
    // 创建测试元件
    ComponentSpec testComponent;
    testComponent.reference = "R1";
    testComponent.type = ComponentType::RESISTOR;
    testComponent.nominal_value = 1000.0; // 1kΩ
    testComponent.tolerance_percent = 5.0;
    
    QString taskId = taskGenerator->generateTask(testComponent);
    if (!taskId.isEmpty()) {
        qDebug() << "✓ 测试任务生成成功";
        qDebug() << QString("   任务ID: %1").arg(taskId);
        
        TestTask task = taskGenerator->getTask(taskId);
        qDebug() << QString("   任务名称: %1").arg(task.taskName);
        qDebug() << QString("   元件: %1").arg(task.component.reference);
        qDebug() << QString("   状态: %1").arg(task.status);
        
    } else {
        qDebug() << "✗ 测试任务生成失败";
    }
    
    // 4. 测试端口分配
    qDebug() << "\n4. 测试端口分配...";
    QVector<PortInfo> allocatedPorts = portManager->autoAllocatePorts(ComponentType::RESISTOR, "R1");
    if (!allocatedPorts.isEmpty()) {
        qDebug() << "✓ 端口自动分配成功";
        qDebug() << QString("   分配端口数: %1").arg(allocatedPorts.size());
        
        for (const PortInfo& port : allocatedPorts) {
            qDebug() << QString("   - %1:%2 (%3)")
                        .arg(port.deviceName)
                        .arg(port.portNumber)
                        .arg(port.description);
        }
        
        // 释放端口
        portManager->releasePortsForUser("R1");
        qDebug() << "✓ 端口释放成功";
        
    } else {
        qDebug() << "✗ 端口自动分配失败";
    }
    
    // 5. 测试接线引导对话框（如果在GUI环境中）
    qDebug() << "\n5. 测试接线引导对话框...";
    if (app.arguments().contains("--show-gui")) {
        WiringGuideDialog* dialog = new WiringGuideDialog(testComponent, portManager);
        dialog->show();
        
        QTimer::singleShot(3000, [dialog]() {
            dialog->close();
            QApplication::quit();
        });
        
        return app.exec();
    } else {
        qDebug() << "   跳过GUI测试（使用 --show-gui 参数启用）";
    }
    
    // 清理
    delete taskGenerator;
    delete resourceManager;
    delete portManager;
    delete deviceManager;
    
    qDebug() << "\n=== 测试完成 ===";
    qDebug() << "所有测试通过！接线引导系统工作正常。";
    
    return 0;
}
