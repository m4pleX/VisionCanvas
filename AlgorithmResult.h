/*
 * 文件名：AlgorithmResult.h
 * 职责：算法结果层的所有权边界契约 + 引擎转换接口草案（仅声明，不实现）
 *
 * 设计核心（单一数据源 + 明确所有权）：
 *   1. 算法结果（检测/分割/测量）与用户标注 DrawShapeItem 是两个【独立】的数据集合，
 *      各自拥有独立的生命周期与所有权，【绝不自动双向同步】。
 *   2. 二者之间只通过【显式导入/导出】单向流动：
 *        - 导入：算法结果 -> 转换为一批 DrawShapeItem，显式加入标注集合；
 *        - 导出：标注集合 -> 转换为算法引擎所需的输入格式（OpenCV/HALCON/YOLO/UNet/COCO）。
 *     用户手动编辑标注，不反向修改算法结果；算法跑批，也不直接覆盖用户标注。
 *   3. 引擎转换仅做【格式转换】，不改动源数据（单一数据源的只读视图/转换结果原则）。
 *
 * 本文件仅为契约草案，不包含任何实现；具体引擎适配器在第二阶段按需落地。
 */
#pragma once

#include <QString>
#include <QList>
#include <QPolygonF>
#include <QRectF>
#include "DrawShapeData.h"

/* ========================================================================
 *  通用引擎无关的结果表示：以"框 / 掩膜 / 轮廓"三类几何载体承载各类算法输出
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
 *  算法结果容器：一次算法处理的完整输出集合（独立所有权，与标注集合隔离）
 * ======================================================================== */
class AlgorithmResult
{
public:
	/*  引擎来源标识：opencv / halcon / yolo / unet / ... */
	QString engine;

	/*  结果几何载体（三类可并存，代表一次处理的不同产出） */
	QList<DetectionBox>     detections;
	QList<SegmentationMask> masks;
	QList<MeasureResult>    measures;

	/*  源图引用（用于结果叠加回显与坐标换算） */
	QString sourceImagePath;
	int     sourceWidth  = 0;
	int     sourceHeight = 0;
};

/* ========================================================================
 *  引擎转换接口草案（仅声明，第二阶段实现）
 *  约定：所有 toXxx 均为【读标注 -> 产出引擎格式】，不修改标注；
 *        所有 fromXxx 均为【读引擎格式 -> 产出一批 DrawShapeItem】，供显式导入。
 * ======================================================================== */
namespace AlgorithmBridge
{
	/*  ---- 导出标注 -> 引擎输入 ---- */
	// 待实现：QList<DrawShapeItem> -> OpenCV 结构（cv::Rect/RotatedRect/Point 数组）
	// 待实现：QList<DrawShapeItem> -> HALCON 结构（HRegion/HXLDContour）
	// 待实现：QList<DrawShapeItem> -> YOLO 标签（归一化 [class,cx,cy,w,h]/OBB）
	// 待实现：QList<DrawShapeItem> -> UNet 掩膜（逐形状 toMask + 类别索引）

	/*  ---- 导入引擎结果 -> 标注 ---- */
	// 待实现：convertDetectionToShapes(const QList<DetectionBox>&) -> 一批 DrawShapeItem
	// 待实现：convertMaskToShapes(const QList<SegmentationMask>&)      -> 一批 DrawShapeItem
	// 待实现：importCoco(const QString& jsonPath)  -> 一批 DrawShapeItem
	// 待实现：exportCoco(const QList<DrawShapeItem>&, const QString& outPath) -> bool
}
