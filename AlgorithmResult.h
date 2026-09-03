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
 * ────────────────────────────────────────────────────────────────
 * 【结果几何词汇表分工】——防止新人/未来自己搞错，务必遵守：
 *   几何本体（点/直线/线段/圆弧/圆/椭圆/轮廓）→ 一律走 ResultGeom
 *     （见 GeomPrimitive.h，值语义）；这是「线/点/轮廓」类几何的【唯一】载体。
 *   标量测量值（长度/角度/距离/面积）→ 走 MeasureResult（仅 value 语义）。
 *   检测框 / 分割掩膜 / 位姿 → 继续走 DetectionBox / SegmentationMask / PoseResult。
 *   几何本体 与 标量测量值 不得互相兼职承载（MeasureResult 不装几何，
 *     ResultGeom 不装标量值）。
 *   迁移状态：MeasureResult 从未被任何工具真实填充过（预留结构），
 *     后续一律用 ResultGeom 承载几何；MeasureResult 仅保留标量用途，
 *     是否最终移除待「标量测量工具」落地时再定。
 * ────────────────────────────────────────────────────────────────
 *
 * 本文件是数据契约；具体引擎适配器在引入各引擎时按需落地。
 */
#pragma once

#include <QString>
#include <QList>
#include <QPolygonF>
#include <QRectF>
#include "DrawShapeData.h"
#include "GeomPrimitive.h"

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

/*  测量结果（标量测量值）：长度/角度/距离/面积等数值型产出。
 *
 *  ⚠ 语义标记（2026-09）：本结构只承载【标量测量值】。
 *    几何本体（点/线/弧/圆/轮廓）【一律走 ResultGeom / GeomPrimitive】，
 *    【禁止】再往本结构塞几何（points 仅作回显点集，不是几何本体）。
 *    状态：预留结构，从未被任何工具真实填充；「标量测量工具」落地时
 *    按需启用，是否保留 points 届时再定。
 *  @see ResultGeom（几何本体载体）、GeomPrimitive
 */
struct MeasureResult {
	int      classId = -1;
	QString  label;
	QPolygonF points;            /*  附带点集（回显用），非几何本体 */
	double   value = 0;          /*  测量值（长度/角度/距离/面积） */
};

/*  定位结果载体：定位/匹配算法产出的位姿（Pose2D），供下游 ROI 校正/跟随。
 *  与 DetectionBox / SegmentationMask / MeasureResult 平级，代表「定位类」算法的只读输出。 */
struct PoseResult {
	int    classId = -1;
	QString label;
	Pose2D pose;
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

	/*  结果几何载体（多类可并存，代表一次处理的不同产出） */
	QList<DetectionBox>     detections;
	QList<SegmentationMask> masks;
	QList<ResultGeom>       geometry;    /*  ★线/点/弧/圆/轮廓 类几何的【唯一】载体（@see GeomPrimitive.h） */
	QList<MeasureResult>    measures;    /*  仅标量测量值；几何一律走上面的 geometry，勿混用 */
	QList<PoseResult>       poses;      /*  定位类结果：位姿（供下游 ROI 校正/跟随） */

	/*  源图引用（用于结果叠加回显与坐标换算） */
	QString sourceImagePath;
	int     sourceWidth  = 0;
	int     sourceHeight = 0;
};
