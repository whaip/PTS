QT       += core gui printsupport concurrent opengl openglwidgets serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# 包含路径
INCLUDEPATH += include \
               include/onnxruntime \
               include/opencv2 \
               ComponentDiagnosticFramework


# 库路径
win32 {
    LIBS += -L$$PWD/lib/windows -L$$PWD/lib/pxie5320 -L$$PWD/lib/pxie5711 -L$$PWD/lib/pxie8902
    LIBS += -L$$PWD/lib/fftw -L$$PWD/lib/opencv_release -L$$PWD/lib/onnxruntime
    
    # RtNet红外摄像头库
    contains(QT_ARCH, x86_64) {
        LIBS += -L$$PWD/lib/windows/x64 -lRtNet
    } else {
        LIBS += -L$$PWD/lib/windows/x86 -lRtNet
    }
    
    # JYTEK设备库
    LIBS += -lJY5320Core -lJY5710Core -lJY8902Core
      # FFTW库
    LIBS += -lfftw3
    
    # OpenCV库
    LIBS += -lopencv_world4110
    
    # ONNX Runtime库
    LIBS += -lonnxruntime
}

SOURCES += \
    PCB_Model_Identification/ImageDialog/imageflowdialog.cpp \
    PCB_Model_Identification/pcb_extract.cpp \
    main.cpp \
    mainwindow.cpp \
    devicemanager.cpp \
    devicethread.cpp \
    devicemanagertestwindow.cpp \
    faultdiagnostic.cpp\
    signalconfiguration.cpp \
    portconfiguration.cpp \
    faultanalysis.cpp \
    testsequencemanager.cpp \    
    resultexporter.cpp \
    ch340.cpp \
    ComponentDiagnosticFramework/basecomponentdiagnostic.cpp \
    ComponentDiagnosticFramework/componentdiagnosticframework.cpp \
    ComponentDiagnosticFramework/componentdiagnosticmanager.cpp \
    ComponentDiagnosticFramework/Components/resistordiagnostic.cpp \
    ComponentDiagnosticFramework/Components/capacitordiagnostic.cpp \
    ComponentDiagnosticFramework/Components/inductordiagnostic.cpp \
    ComponentDiagnosticFramework/Components/diodediagnostic.cpp \
    ComponentDiagnosticFramework/Components/icdiagnostic.cpp \
    pcbidentifier.cpp \
    pcbidentificationdialog.cpp \
    cameramanager.cpp \
    cameracontrolwidget.cpp \
    identificationworker.cpp \
    Camera/Infrared/irimagedisplay.cpp \
    PCB_Model_Identification/siftmatcher.cpp \
    include/qcustomplot.cpp \
    realtimepcbanalyzerwidget.cpp \
    PCB_Components_Detect/yolomodel.cpp \
    PCB_Components_Detect/labelmanagerdialog.cpp \
    PCB_Components_Detect/labelediting.cpp \
    PCB_Components_Detect/labelrectitem.cpp \
    pcbdetectionmanager.cpp \    
    pcbboardmanager.cpp \    
    pcbboardmanagementwidget.cpp \
    PCB_Components_Detect/staticlabelviewdialog.cpp \
    uestcqcustomplot.cpp \
    WiringGuide/wiringguidedialog.cpp \
    WiringGuide/portmanager.cpp \
    WiringGuide/wiringguidedialog.cpp \
    WiringGuide/wiringresourcemanager.cpp \
    WiringGuide/wiringtaskgenerator.cpp

HEADERS += \
    PCB_Model_Identification/ImageDialog/imagedialog.h \
    PCB_Model_Identification/ImageDialog/imageflowdialog.h \
    PCB_Model_Identification/pcb_extract.h \
    mainwindow.h \
    devicemanager.h \
    devicethread.h \
    devicemanagertestwindow.h \
    commontypes.h \
    faultdiagnostic.h\
    signalconfiguration.h \
    portconfiguration.h \
    faultanalysis.h \
    testsequencemanager.h \    
    resultexporter.h \
    ch340.h \
    ComponentDiagnosticFramework/basecomponentdiagnostic.h \
    ComponentDiagnosticFramework/componentdiagnosticframework.h \
    ComponentDiagnosticFramework/componentdiagnosticmanager.h \
    ComponentDiagnosticFramework/Components/resistordiagnostic.h \
    ComponentDiagnosticFramework/Components/capacitordiagnostic.h \
    ComponentDiagnosticFramework/Components/inductordiagnostic.h \
    ComponentDiagnosticFramework/Components/diodediagnostic.h \
    ComponentDiagnosticFramework/Components/icdiagnostic.h \
    pcbidentifier.h \
    pcbidentificationdialog.h \
    cameramanager.h \
    cameracontrolwidget.h \
    identificationworker.h \
    Camera/Infrared/irimagedisplay.h \
    PCB_Model_Identification/siftmatcher.h \
    include/qcustomplot.h \
    realtimepcbanalyzerwidget.h \
    PCB_Components_Detect/yolomodel.h \
    PCB_Components_Detect/ClassList.h \
    PCB_Components_Detect/labelmanagerdialog.h \
    PCB_Components_Detect/labelediting.h \
    PCB_Components_Detect/labelrectitem.h \
    pcbdetectionmanager.h \    
    pcbboardmanager.h \    
    pcbboardmanagementwidget.h \
    5711waveformconfig.h \
    PCB_Components_Detect/staticlabelviewdialog.h \
    uestcqcustomplot.h \
    WiringGuide/wiringguidedialog.h \
    WiringGuide/portdefinitions.h \
    WiringGuide/portmanager.h \
    WiringGuide/wiringresourcemanager.h \
    WiringGuide/wiringtaskgenerator.h

FORMS += \
    mainwindow.ui
    
win32 {
    CONFIG(debug, debug|release) {
        DESTDIR = $$PWD/build/debug
    } else {
        DESTDIR = $$PWD/build/release
    }

    # 复制必要的DLL文件
    QMAKE_POST_LINK += $$quote(xcopy /Y /Q $$shell_path($$PWD/bin/*.dll) $$shell_path($$DESTDIR) > nul$$escape_expand(\n\t))
}
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
