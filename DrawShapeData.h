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
		bool isBiarc = false;
		double ax = 0, ay = 0, bx = 0, by = 0;
		double o1x = 0, o1y = 0, r1 = 0, r2 = 0;
		double px = 0, py = 0;
	} arc;
	struct Polygon { QList<QPointF> pts; } polygon;

	explicit DrawShapeItem(DrawShapeType t) : type(t) {}
};
