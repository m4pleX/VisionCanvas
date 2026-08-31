# VisionPlatform —— 机器视觉运行平台

基于 Qt 6 的机器视觉运行平台，提供图像加载、输入几何（ROI / 测量基准）定义、算法引擎接入与算法结果的只读可视化。

## 项目简介

面向机器视觉检测场景的运行平台原型。以「数据与渲染分离」为架构核心：用户绘制的几何图形（ROI / 基准线）作为算法输入，算法引擎产出只读结果（检测框 / 掩膜 / 测量值），经独立叠层可视化，不参与人工编辑。

## 核心特性

- **输入几何定义**：矩形、旋转矩形、圆、椭圆、圆环、扇环、多边形 7 类图形，作为算法 ROI / 测量基准
- **算法接入层**：集成 OpenCV 4.8.1，提供灰度缺陷检测基线（`inRange` 双阈值分割）
- **只读结果可视化**：检测框 + 类别 + 置信度，独立叠层渲染，不污染输入几何
- **多结果宿主**：`DetectionResultModel` 管理多算法实例 / 多次运行结果，按 `engine + toolId + resultId` 定位
- **自研扇环图形**：Qt 原生不含扇环图元，通过 `QPainterPath` 手动构建「外弧 + 内弧 + 闭合」路径实现
- **OpenGL 视口加速**：渲染走 `QOpenGLWidget` 加速路径，并通过强制视口重绘解决 GL 视口与叠加控件间的边缘残影
- **实时像素探针**：鼠标悬停时实时显示图像坐标与像素 RGB / 灰度值
- **Schema 驱动参数面板**：基于 `ParamField` 元数据自动生成属性编辑 UI，新增图形类型无需扩展 switch-case

## 技术架构

| 模块 | 职责 |
| --- | --- |
| `DrawShapeData` | 输入几何数据模型（统一几何字段 + 业务元信息） |
| `AlgorithmResult` | 算法只读结果契约（检测框 / 掩膜 / 测量值） |
| `DetectionResultModel` | 算法结果宿主（多结果管理） |
| `CvImageConverter` | QImage ↔ cv::Mat 转换 |
| `GrayDefectDetector` | OpenCV 灰度缺陷检测 |
| `ShapePainter` | 形状渲染，将几何数据映射为场景图元 |
| `ShapeEditor` / `ShapeGeometry` | 形状几何编辑计算（纯几何，零渲染依赖） |
| `ShapeHandleHelper` | 控制点编辑与形状交互 |
| `ToolbarController` | 工具栏状态、缩放命令与 UI 文案联动 |
| `ParamPanelWidget` | Schema 驱动参数面板，动态生成属性编辑 UI |
| `AnnotationIO` | 几何数据 JSON 序列化 / 反序列化 |
| `ImageCanvasView` | 主视图：场景管理、事件处理、图像加载与结果只读上屏 |

## 数据流

```
用户绘制几何 (DrawShapeItem，作为 ROI / 基准 / 参数)
   ↓ 作为输入
算法引擎 (GrayDefectDetector / 未来 HALCON / YOLO / UNet)
   ↓ 产出
AlgorithmResult（只读结果：框 / 掩膜 / 测量 + 置信度）
   ↓
可视化（只读叠层）/ 判定（OK-NG）/ 导出
```

## 构建环境

- Qt 6.5.3 (MSVC2019 64-bit)
- OpenCV 4.8.1（Debug / Release 分别指向 `install-debug` / `install-release`）
- Visual Studio 2019，v142 工具集
- 模块：`core; gui; widgets; opengl; openglwidgets`

使用 CMake 构建（需先加载 MSVC 环境，参见 `my_vcvars1429.bat`）或打开 `VisionPlatform.sln`。
