#include "ShapeGeometry.h"

#include <QtMath>
#include <cmath>
#include <algorithm>

/* ===== 几何求解 ===== */

double ShapeGeometry::normAngle360(double deg)
{
	while (deg < 0) deg += 360.0;
	while (deg >= 360.0) deg -= 360.0;
	return deg;
}

bool ShapeGeometry::circumcircle(const QPointF& p1, const QPointF& p2, const QPointF& p3, QPointF& c, double& r)
{
	double x1 = p1.x(), y1 = p1.y(), x2 = p2.x(), y2 = p2.y(), x3 = p3.x(), y3 = p3.y();
	double d = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
	if (std::abs(d) < 1e-12) return false;
	c = QPointF(((x1 * x1 + y1 * y1) * (y2 - y3) + (x2 * x2 + y2 * y2) * (y3 - y1) + (x3 * x3 + y3 * y3) * (y1 - y2)) / d,
	            ((x1 * x1 + y1 * y1) * (x3 - x2) + (x2 * x2 + y2 * y2) * (x1 - x3) + (x3 * x3 + y3 * y3) * (x2 - x1)) / d);
	r = std::sqrt((x1 - c.x()) * (x1 - c.x()) + (y1 - c.y()) * (y1 - c.y()));
	return true;
}

bool ShapeGeometry::solveConcentricArc(const QPointF& A, const QPointF& B, double r1, double r2, QPointF& O, QPointF& P)
{
	double d = std::sqrt((B.x() - A.x()) * (B.x() - A.x()) + (B.y() - A.y()) * (B.y() - A.y()));
	if (d < 1e-6 || r1 < 1.0 || r2 < 1.0) return false;
	double x = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
	double det = r1 * r1 - x * x;
	if (det < -1e-6) return false;
	double y = std::sqrt(std::max(0.0, det));
	double dx = B.x() - A.x(), dy = B.y() - A.y();
	double ux = dx / d, uy = dy / d, vx = -uy, vy = ux;
	O = QPointF(A.x() + x * ux + y * vx, A.y() + x * uy + y * vy);
	double oax = A.x() - O.x(), oay = A.y() - O.y(), obx = B.x() - O.x(), oby = B.y() - O.y();
	double la = std::sqrt(oax * oax + oay * oay), lb = std::sqrt(obx * obx + oby * oby);
	if (la < 1e-6 || lb < 1e-6) return false;
	double mx = oax / la + obx / lb, my = oay / la + oby / lb, ml = std::sqrt(mx * mx + my * my);
	if (ml < 1e-6) return false;
	P = QPointF(O.x() + r1 * mx / ml, O.y() + r1 * my / ml);
	return true;
}

/* ===== 命中判定 ===== */

bool ShapeGeometry::contains(const DrawShapeItem* shape, const QPointF& scenePos)
{
	if (!shape) return false;
	switch (shape->type)
	{
	case Shape_Rect:
	{
		double left   = shape->rect.cx - shape->rect.w / 2.0;
		double right  = shape->rect.cx + shape->rect.w / 2.0;
		double top    = shape->rect.cy - shape->rect.h / 2.0;
		double bottom = shape->rect.cy + shape->rect.h / 2.0;
		return scenePos.x() >= left && scenePos.x() <= right && scenePos.y() >= top && scenePos.y() <= bottom;
	}
	case Shape_RotateRect:
	{
		double rad = -qDegreesToRadians(shape->rotatedRect.angle);
		double dx  = scenePos.x() - shape->rotatedRect.cx;
		double dy  = scenePos.y() - shape->rotatedRect.cy;
		double lx  = dx * qCos(rad) - dy * qSin(rad);
		double ly  = dx * qSin(rad) + dy * qCos(rad);
		double hw  = shape->rotatedRect.w / 2.0;
		double hh  = shape->rotatedRect.h / 2.0;
		return lx >= -hw && lx <= hw && ly >= -hh && ly <= hh;
	}
	case Shape_Circle:
	{
		double d2 = (scenePos.x() - shape->circle.cx) * (scenePos.x() - shape->circle.cx)
		          + (scenePos.y() - shape->circle.cy) * (scenePos.y() - shape->circle.cy);
		return d2 <= shape->circle.r * shape->circle.r;
	}
	case Shape_Ellipse:
	{
		double rad = -qDegreesToRadians(shape->ellipse.angle);
		double dx  = scenePos.x() - shape->ellipse.cx;
		double dy  = scenePos.y() - shape->ellipse.cy;
		double lx  = dx * qCos(rad) - dy * qSin(rad);
		double ly  = dx * qSin(rad) + dy * qCos(rad);
		double r1  = shape->ellipse.r1;
		double r2  = shape->ellipse.r2;
		return (lx * lx) / (r1 * r1) + (ly * ly) / (r2 * r2) <= 1.0;
	}
	case Shape_Ring:
	{
		double d2 = (scenePos.x() - shape->ring.cx) * (scenePos.x() - shape->ring.cx)
		          + (scenePos.y() - shape->ring.cy) * (scenePos.y() - shape->ring.cy);
		double rLarger  = std::max(shape->ring.r1, shape->ring.r2);
		double rSmaller = std::min(shape->ring.r1, shape->ring.r2);
		return d2 <= rLarger * rLarger && d2 >= rSmaller * rSmaller;
	}
	case Shape_Arc:
	{
		double cx = shape->arc.cx, cy = shape->arc.cy;
		double rBig = std::max(shape->arc.rOuter, shape->arc.rInner);
		double rSml = std::min(shape->arc.rOuter, shape->arc.rInner);
		double dx = scenePos.x() - cx, dy = scenePos.y() - cy, d = std::sqrt(dx * dx + dy * dy);
		if (d > rBig + 8.0 || d < rSml - 8.0) return false;
		double ang = normAngle360(qRadiansToDegrees(std::atan2(dy, dx)));
		double sa = normAngle360(shape->arc.startAngle), span = shape->arc.span;
		double rel = ang - sa; if (rel < 0) rel += 360.0;
		if (span >= 0) return rel <= span;
		return rel >= 360.0 + span;  // 顺时针弧：检测"非缺口"范围
	}
	case Shape_Polygon:
	{
		QPolygonF poly; for (auto& pt : shape->polygon.pts) poly << pt;
		return poly.containsPoint(scenePos, Qt::OddEvenFill);
	}
	default: return false;
	}
}

/* ===== 面向算法层的几何出口 ===== */

QPolygonF ShapeGeometry::toPoints(const DrawShapeItem& shape, int nLineInterp)
{
	QPolygonF pts;
	const int n = std::max(4, nLineInterp);
	switch (shape.type)
	{
	case Shape_Rect:
	{
		double hw = shape.rect.w / 2.0, hh = shape.rect.h / 2.0;
		pts << QPointF(shape.rect.cx - hw, shape.rect.cy - hh)
		    << QPointF(shape.rect.cx + hw, shape.rect.cy - hh)
		    << QPointF(shape.rect.cx + hw, shape.rect.cy + hh)
		    << QPointF(shape.rect.cx - hw, shape.rect.cy + hh);
		break;
	}
	case Shape_RotateRect:
	{
		double hw = shape.rotatedRect.w / 2.0, hh = shape.rotatedRect.h / 2.0;
		double rad = qDegreesToRadians(shape.rotatedRect.angle);
		QPointF local[4] = { {-hw,-hh}, {hw,-hh}, {hw,hh}, {-hw,hh} };
		for (auto& p : local)
		{
			double lx = p.x(), ly = p.y();
			double rx = lx * qCos(rad) - ly * qSin(rad);
			double ry = lx * qSin(rad) + ly * qCos(rad);
			pts << QPointF(rx + shape.rotatedRect.cx, ry + shape.rotatedRect.cy);
		}
		break;
	}
	case Shape_Circle:
	{
		double cx = shape.circle.cx, cy = shape.circle.cy, r = shape.circle.r;
		for (int i = 0; i < n; ++i)
		{
			double a = 2.0 * M_PI * i / n;
			pts << QPointF(cx + r * qCos(a), cy + r * qSin(a));
		}
		break;
	}
	case Shape_Ellipse:
	{
		double cx = shape.ellipse.cx, cy = shape.ellipse.cy, r1 = shape.ellipse.r1, r2 = shape.ellipse.r2;
		double rad = qDegreesToRadians(shape.ellipse.angle);
		for (int i = 0; i < n; ++i)
		{
			double a = 2.0 * M_PI * i / n;
			double lx = r1 * qCos(a), ly = r2 * qSin(a);
			pts << QPointF(cx + lx * qCos(rad) - ly * qSin(rad), cy + lx * qSin(rad) + ly * qCos(rad));
		}
		break;
	}
	case Shape_Ring:
	{
		// 圆环近似为外圆点集 + 内圆反向点集，形成环形带
		double cx = shape.ring.cx, cy = shape.ring.cy;
		double rL = std::max(shape.ring.r1, shape.ring.r2), rS = std::min(shape.ring.r1, shape.ring.r2);
		for (int i = 0; i < n; ++i)
		{
			double a = 2.0 * M_PI * i / n;
			pts << QPointF(cx + rL * qCos(a), cy + rL * qSin(a));
		}
		for (int i = n; i >= 0; --i)
		{
			double a = 2.0 * M_PI * i / n;
			pts << QPointF(cx + rS * qCos(a), cy + rS * qSin(a));
		}
		break;
	}
	case Shape_Arc:
	{
		double cx = shape.arc.cx, cy = shape.arc.cy;
		double rO = shape.arc.rOuter, rI = shape.arc.rInner;
		double saR = qDegreesToRadians(shape.arc.startAngle), spR = qDegreesToRadians(shape.arc.span);
		for (int i = 0; i <= n; ++i)
		{
			double a = saR + spR * i / n;
			pts << QPointF(cx + rO * qCos(a), cy + rO * qSin(a));
		}
		for (int i = n; i >= 0; --i)
		{
			double a = saR + spR * i / n;
			pts << QPointF(cx + rI * qCos(a), cy + rI * qSin(a));
		}
		break;
	}
	case Shape_Polygon:
		pts = shape.polygon.pts;
		break;
	default:
		break;
	}
	return pts;
}

QPainterPath ShapeGeometry::toPolygonPath(const DrawShapeItem& shape, int nLineInterp)
{
	QPainterPath path;
	switch (shape.type)
	{
	case Shape_Rect:
	{
		double hw = shape.rect.w / 2.0, hh = shape.rect.h / 2.0;
		path.addRect(QRectF(shape.rect.cx - hw, shape.rect.cy - hh, shape.rect.w, shape.rect.h));
		break;
	}
	case Shape_RotateRect:
	{
		QPolygonF p = toPoints(shape, nLineInterp);
		path.addPolygon(p);
		path.closeSubpath();
		break;
	}
	case Shape_Circle:
		path.addEllipse(QPointF(shape.circle.cx, shape.circle.cy), shape.circle.r, shape.circle.r);
		break;
	case Shape_Ellipse:
	{
		// 椭圆带旋转角：用变换后点集近似（QPainterPath 本身不含旋转椭圆原语）
		QPolygonF p = toPoints(shape, nLineInterp);
		path.addPolygon(p);
		path.closeSubpath();
		break;
	}
	case Shape_Ring:
	{
		double rL = std::max(shape.ring.r1, shape.ring.r2), rS = std::min(shape.ring.r1, shape.ring.r2);
		QPainterPath o; o.addEllipse(QPointF(shape.ring.cx, shape.ring.cy), rL, rL);
		QPainterPath i; i.addEllipse(QPointF(shape.ring.cx, shape.ring.cy), rS, rS);
		path = o.subtracted(i);
		break;
	}
	case Shape_Arc:
	{
		double cx = shape.arc.cx, cy = shape.arc.cy;
		double rO = shape.arc.rOuter, rI = shape.arc.rInner;
		double saR = qDegreesToRadians(shape.arc.startAngle), spR = qDegreesToRadians(shape.arc.span);
		const int n = std::max(4, nLineInterp);
		path.moveTo(cx + rO * qCos(saR), cy + rO * qSin(saR));
		for (int i = 1; i <= n; ++i) { double a = saR + spR * i / n; path.lineTo(cx + rO * qCos(a), cy + rO * qSin(a)); }
		double eR = saR + spR; path.lineTo(cx + rI * qCos(eR), cy + rI * qSin(eR));
		for (int i = n; i >= 0; --i) { double a = saR + spR * i / n; path.lineTo(cx + rI * qCos(a), cy + rI * qSin(a)); }
		path.closeSubpath();
		break;
	}
	case Shape_Polygon:
	{
		QPolygonF poly = shape.polygon.pts;
		path.addPolygon(poly);
		path.closeSubpath();
		break;
	}
	default:
		break;
	}
	return path;
}

QRectF ShapeGeometry::boundingRect(const DrawShapeItem& shape)
{
	switch (shape.type)
	{
	case Shape_Rect:
	{
		double hw = shape.rect.w / 2.0, hh = shape.rect.h / 2.0;
		return QRectF(shape.rect.cx - hw, shape.rect.cy - hh, shape.rect.w, shape.rect.h);
	}
	case Shape_RotateRect:
	case Shape_Ellipse:
		return toPoints(shape).boundingRect();
	case Shape_Circle:
		return QRectF(shape.circle.cx - shape.circle.r, shape.circle.cy - shape.circle.r,
		              shape.circle.r * 2, shape.circle.r * 2);
	case Shape_Ring:
	{
		double r = std::max(shape.ring.r1, shape.ring.r2);
		return QRectF(shape.ring.cx - r, shape.ring.cy - r, r * 2, r * 2);
	}
	case Shape_Arc:
		return toPoints(shape).boundingRect();
	case Shape_Polygon:
		return toPoints(shape).boundingRect();
	default:
		return QRectF();
	}
}
