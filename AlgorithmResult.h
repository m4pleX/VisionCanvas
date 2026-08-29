/*
 * 文件名：AlgorithmResult.h
 * 职责：算法结果层的数据契约（机器视觉平台的只读输出载体）
 *
 * 定位（机器视觉平台，而非标注工具）：
 *   - 算法结果（检测/分割/测量）是【机器的判定输出】，属【只读】数据：
 *     人不能拖拽/编辑其几何，只能查看、显隐、清除、导出判定结果。
 *   - DrawShapeItem 的正确角色是【算法的输入几何】（ROI 框、测量基准线、
 *     屏蔽区域等），由用户在运行算法前绘制，属于"参数/预处理几何"，
 *     与算法输出是两种完全不同的语义，【不互相转换、不互相回填】。
 *
 * 数据流（单向，无回填）：
 *   用户绘制几何(DrawShapeItem)
 *        ↓ 作为输入（ROI / 基准 / 参数）
 *   算法引擎(OpenCV/HALCON/YOLO/UNet)
 *        ↓ 产出
 *   AlgorithmResult（只读结果：框/掩膜/测量值 + 置信度 + 判定）
 *        ↓
 *   可视化层（叠加只读显示） / 判定层（OK/NG） / 导出层（结果报表/控制信号）
 *
 *   AlgorithmResult 与 DrawShapeItem 是两套【独立】数据，所有权分离，
 *   生命周期各自管理，不存在自动双向同步或显式互转。
 *
 * 本文件是数据契约；具体引擎适配器在引入各引擎时按需落地。
 */
#pragma once

#include <QString>
#include <QList>
#include <QPolygonF>
#include <QRectF>

/* ========================================================================
 *  通用引擎无关的结果表示：以"框 / 掩膜 / 测量"三类几何载体承载算法输出
 *  （全部为只读输出，供可视化/判定/导出，不参与人工编辑）
 * ======================================================================== */

/*  目标检测结果（矩形框 / 旋转框 / 类别 / 置信度） */
struct DetectionBox {
	int     classId = -1;        /*  类别 id */
	QString label;               /*  类别名 */
	double  confidence = 0.0;    /*  置信度 (0~1) */

	double  cx = 0, cy = 0;      /*  中心点（图像坐标） */
	double  w = 0, h = 0;        /*  宽高 */
	double  angle = 0;           /*  旋转角（0 = 轴对齐，对应 rotatedRect） */
};

/*  分割结果：以多边形轮廓环表示（可栅格化为掩膜） */
struct SegmentationMask {
	int        classId = -1;
	QString    label;
	QPolygonF  contour;          /*  前景轮廓点环（图像坐标，闭合） */
};

/*  测量/定位结果（点、线段、圆等几何量） */
struct MeasureResult {
	int      classId = -1;
	QString  label;
	QPolygonF points;            /*  关键点序列：独点/两点线段/多点轨迹 */
	double   value = 0;          /*  测量值（长度/角度/面积等） */
};

/* ========================================================================
 *  算法结果容器：一次算法处理的完整输出集合（只读，与输入几何隔离）
 *
 *  语义协议（对"未来多步流水线"的关键预设）：
 *    - AlgorithmResult 是【一次工具执行】的标准只读输出载体；
 *    - 同一张图可以有【多个】 AlgorithmResult（多个算法实例 / 多个工具 / 多次运行），
 *      互不干扰；每个 result 通过 engine + toolId + resultId 唯一定位；
 *    - 结果可被上层按需消费：
 *        可视化（叠加只读框/掩膜/测量值）、判定（OK/NG）、
 *        导出（报表 / 控制信号）、以及作为【下一步算法的输入】（流水线串联）。
 * ======================================================================== */
class AlgorithmResult
{
public:
	/*  定位元信息：engine = opencv/halcon/yolo/unet...；toolId = 产生该结果的工具实例 id */
	QString engine;
	QString toolId;
	QString resultId;   /*  结果唯一标识（多实例、多工具并存时用） */

	/*  结果几何载体（三类可并存，代表一次处理的不同产出） */
	QList<DetectionBox>     detections;
	QList<SegmentationMask> masks;
	QList<MeasureResult>    measures;

	/*  源图引用（用于结果叠加回显与坐标换算） */
	QString sourceImagePath;
	int     sourceWidth  = 0;
	int     sourceHeight = 0;
};
