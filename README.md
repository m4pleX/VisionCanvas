# VisionCanvas —— 高性能图像标注工具

基于 Qt 6 的桌面图像标注软件，提供 7 类基础与工程图形标注、OpenGL 视口加速渲染与 JSON 格式标注数据持久化。

## 项目简介

自主研发的跨平台图像标注工具，面向目标检测 / 几何标定等场景。以「数据与渲染分离」为架构核心，图形统一由纯几何数据模型 `DrawShapeItem` 描述，通过自研 `ShapePainter` 渲染器映射为场景图元，保证数据可序列化、可跨会话复现。

## 核心特性

- **7 类图形标注**：矩形、旋转矩形、圆、多边形、椭圆、圆环、扇环
- **自研扇环图形**：Qt 原生不含扇环图元，通过 `QPainterPath` 手动构建「外弧 + 内弧 + 闭合」路径实现，支持内外半径与起止角度独立调节
- **OpenGL 视口加速**：渲染走 `QOpenGLWidget` 加速路径，并通过强制视口重绘解决 GL 视口与叠加控件间的边缘残影
- **实时像素探针**：鼠标悬停时实时显示图像坐标与像素 RGB / 灰度值，为图像分析提供即时的像素级反馈
- **Schema 驱动参数面板**：基于 `ParamField` 元数据自动生成属性编辑 UI，新增图形类型无需扩展 switch-case
- **标注数据持久化**：7 类图形完整 JSON 序列化与反序列化，标注文件内嵌图片路径与尺寸，可随时恢复标注现场

## 技术架构

| 模块 | 职责 |
| --- | --- |
| `ShapePainter` | 图形渲染，将几何数据映射为场景图元 |
| `ShapeHandleHelper` | 控制点编辑与形状交互 |
| `ToolbarController` | 工具栏状态、缩放命令与 UI 文案联动 |
| `ParamPanelWidget` | Schema 驱动参数面板，动态生成属性编辑 UI |
| `AnnotationIO` | 标注数据 JSON 序列化 / 反序列化 |
| `ImageCanvasView` | 主视图：场景管理、事件处理、图像加载 |

## 构建环境

- Qt 6.5.3 (MSVC2019 64-bit)
- Visual Studio 2019，v142 工具集
- 模块：`core; gui; widgets; opengl; openglwidgets`

使用 Visual Studio 打开 `VisionCanvas.sln` 直接构建即可。
