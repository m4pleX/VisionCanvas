/*
 * 文件名：ShapeEditor.h
 * 职责：形状编辑算法（从 ImageCanvasView 的 update*FromHandle 系列抽离）
 *
 * 核心功能：
 *   - 将"拖拽控制点/旋转手柄"的鼠标位移翻译为形状几何变化（纯几何计算）
 *   - 覆盖 7 类图形：Rect / RotateRect / Circle / Ellipse / Ring / Arc / Polygon
 *
 * 依赖：DrawShapeData（几何数据），零渲染依赖
 * 设计约束：
 *   - 纯静态、无状态、零 Qt 渲染依赖，仅修改 DrawShapeItem 几何字段
 *   - 拖拽上下文通过 EditContext 显式传入（不持有状态，生命周期归上层）
 *   - 每个方法返回 bool：true = 几何已修改，上层需刷新渲染/参数面板；
 *     false = 保护性提前退出（越界/空指针），上层无需刷新。
 *     该返回值精确保留了原实现中"越界 return 不刷新"的语义。
 */
#pragma once

#include "DrawShapeData.h"

class QPointF;

/*  拖拽上下文：一次拖拽会话的起点快照与状态标记
 *  注意：dragStartAngle 为引用，指向主类的 m_dragStartAngle，允许旋转分支累进更新 */
struct EditContext
{
	ShapeDragRect startRect;          /*  拖拽起点快照（cx/cy/w/h） */
	int           handleIdx = -1;     /*  被拖拽的控制点序号 */
	bool          isRotating = false; /*  是否旋转手柄拖拽 */
	bool          isDragging = false; /*  是否控制点拖拽（vs 平移） */
	double&       dragStartAngle;     /*  拖拽起始/参考角度（引用主类状态，旋转时累进更新） */
};

class ShapeEditor
{
public:
	/*  纯几何计算：按拖拽上下文修改 shape 的几何字段；
	 *  返回 true 表示已修改（上层应刷新渲染/面板），false 表示越界/空指针提前退出 */
	static bool updateRect(DrawShapeItem* shape, const EditContext& ctx, const QPointF& pos);
	static bool updateRotatedRect(DrawShapeItem* shape, const EditContext& ctx, const QPointF& pos);
	static bool updateCircle(DrawShapeItem* shape, const EditContext& ctx, const QPointF& pos);
	static bool updateEllipse(DrawShapeItem* shape, const EditContext& ctx, const QPointF& pos);
	static bool updateRing(DrawShapeItem* shape, const EditContext& ctx, const QPointF& pos);
	static bool updateArc(DrawShapeItem* shape, const EditContext& ctx, const QPointF& pos);
	static bool updatePolygon(DrawShapeItem* shape, const EditContext& ctx, const QPointF& pos);
};
