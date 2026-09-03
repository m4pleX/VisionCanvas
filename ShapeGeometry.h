/*
 * 文件名：ShapeGeometry.h
 * 职责：纯静态几何工具库（形状几何能力的唯一出口）
 *
 * 核心功能：
 *   - 形状命中判定（contains，迁移自 ImageCanvasView::isPointInShape）
 *   - 几何求解（normAngle360 / circumcircle / solveConcentricArc）
 *   - 面向算法层的形状几何出口：
 *       toPoints()        形状 -> 点集（服务 OpenCV 轮廓 / HALCON XLD / YOLO OBB）
 *       toPolygonPath()   形状 -> 填充路径（服务 UNet 掩膜栅格化）
 *       boundingRect()    形状 -> 轴对齐外接框（服务 YOLO bbox / OpenCV Rect）
 *
 * 依赖：DrawShapeData（纯几何数据模型），不依赖任何渲染类。
 * 设计约束：
 *   - 所有对 DrawShapeItem 字段的直接访问收敛到本库内部；
 *     第二阶段"字段类型无关化"时，仅需改本文件一处。
 *   - 纯静态无状态，调用方不持副本（单一数据源原则）。
 *   - 参数主存、点集按需：toPoints(n) 按采样精度 n 生成点集，不改变主存精度。
 */
#pragma once

#include <QPolygonF>
#include <QPainterPath>
#include <QRectF>
#include "DrawShapeData.h"

class ShapeGeometry
{
public:
	/* ===== 几何求解 ===== */

	/*  角度归一化到 [0, 360) */
	static double normAngle360(double deg);

	/*  三点外接圆：求外心 c 与半径 r，三点共线返回 false */
	static bool circumcircle(const QPointF& p1, const QPointF& p2, const QPointF& p3, QPointF& c, double& r);

	/*  同心圆、弧求解：给定两端点 A/B 与两半径 r1/r2，求圆心 O 与中点方向点 P */
	static bool solveConcentricArc(const QPointF& A, const QPointF& B, double r1, double r2, QPointF& O, QPointF& P);

	/* ===== 命中判定 ===== */

	/*  形状命中判定：场景坐标 scenePos 是否落在形状内（含容差） */
	static bool contains(const DrawShapeItem* shape, const QPointF& scenePos);

	/* ===== 面向算法层的几何出口 ===== */

	/*  形状 -> 点集（逆时针有序）。nLineInterp: 圆/椭圆/环/弧的周界采样段数（越大越精细） */
	static QPolygonF toPoints(const DrawShapeItem& shape, int nLineInterp = 128);

	/*  形状 -> 填充路径（可直接 QPainterPath 栅格化，服务分割掩膜） */
	static QPainterPath toPolygonPath(const DrawShapeItem& shape, int nLineInterp = 128);

	/*  形状 -> 轴对齐外接框（服务目标检测框） */
	static QRectF boundingRect(const DrawShapeItem& shape);

	/* ===== 位姿校正（定位 → ROI 跟随） ===== */

	/*  位姿校正：将基准 ROI 按位姿（平移+旋转+缩放）生成跟随工件的新 ROI。
	 *  语义（对齐 OpenCV getRotationMatrix2D / VisionPro CogFixtureTool）：
	 *    P' = scale · R(angle) · P + T
	 *    - 旋转 angle 逆时针为正（弧度），先旋转/缩放、再平移；
	 *    - 形状含角度的字段（RotateRect/Ellipse 的 angle、Arc 的 startAngle）
	 *      在原有角度上叠加 pose.angle（弧度转度），体现"工件转多少 ROI 转多少"；
	 *    - 尺寸字段（w/h/r/r2）乘以 scale（工件缩放）；
	 *    - 返回值是【新 ROI】（值拷贝，不修改 base），并打上 sourceToolId 来源标记。 */
	static DrawShapeItem applyPose(const DrawShapeItem& base, const Pose2D& pose, const QString& sourceToolId);
};
