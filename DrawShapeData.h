/*
 * 文件名：DrawShapeData.h
 * 职责：纯几何数据定义，不含任何渲染句柄
 * 核心功能：
 *   - 定义 DrawShapeType 枚举（所有形状类型）
 *   - 定义 DrawShapeItem 结构体（每种形状的几何参数）
 *   - 供所有模块共享，无依赖
 * 依赖：无（仅 QList/QPointF）
 * 注意：纯数据结构，不可引入 QGraphicsItem 等渲染类
 */
#pragma once

#include <QList>
#include <QPointF>

enum DrawShapeType
{
	Shape_Rect,
	Shape_RotateRect,
	Shape_Circle,
	Shape_Ellipse,
	Shape_Ring,
	Shape_Arc,
	Shape_Polygon
};

// 纯几何数据，不包含任何 QGraphicsItem 句柄
struct DrawShapeItem
{
	DrawShapeType type;

	struct Rect { double cx = 0, cy = 0, w = 0, h = 0; } rect;
	struct RotatedRect { double cx = 0, cy = 0, w = 0, h = 0, angle = 0; } rotatedRect;
	struct Circle { double cx = 0, cy = 0, r = 0; } circle;
	struct Ellipse { double cx = 0, cy = 0, r1 = 0, r2 = 0, angle = 0; } ellipse;
	struct Ring { double cx = 0, cy = 0, r1 = 0, r2 = 0; } ring;
	struct Arc {
		double cx = 0, cy = 0, rOuter = 0, rInner = 0, startAngle = 0, endAngle = 0;
		double span = 0;   /*  扇环跨度（角度；正=逆时针，负=顺时针） */
	} arc;
	struct Polygon { QList<QPointF> pts; } polygon;

	explicit DrawShapeItem(DrawShapeType t) : type(t) {}
};
