cmake_minimum_required(VERSION 4.4)
set(CMAKE_PREFIX_PATH "D:/Qt/6.5.3/msvc2019_64")
project(VisionCanvas)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets)
qt_standard_project_setup()

add_executable(VisionCanvas
        AnnotationIO.cpp
        AnnotationIO.h
        DrawShapeData.h
        ImageCanvasView.cpp
        ImageCanvasView.h
        ImageCanvasView.qrc
        ImageCanvasView.ui
        main.cpp
        ParamPanelWidget.cpp
        ParamPanelWidget.h
        ScaleConfig.h
        ShapeHandleHelper.cpp
        ShapeHandleHelper.h
        ShapePainter.cpp
        ShapePainter.h
        ShapeStyleConfig.h
        ToolbarController.cpp
        ToolbarController.h)

target_link_libraries(VisionCanvas PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGLWidgets)
