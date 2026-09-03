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
		double left   = shape->cx - shape->w / 2.0;
		double right  = shape->cx + shape->w / 2.0;
		double top    = shape->cy - shape->h / 2.0;
		double bottom = shape->cy + shape->h / 2.0;
		return scenePos.x() >= left && scenePos.x() <= right && scenePos.y() >= top && scenePos.y() <= bottom;
	}
	case Shape_RotateRect:
	{
		double rad = -qDegreesToRadians(shape->angle);
		double dx  = scenePos.x() - shape->cx;
		double dy  = scenePos.y() - shape->cy;
		double lx  = dx * qCos(rad) - dy * qSin(rad);
		double ly  = dx * qSin(rad) + dy * qCos(rad);
		double hw  = shape->w / 2.0;
		double hh  = shape->h / 2.0;
		return lx >= -hw && lx <= hw && ly >= -hh && ly <= hh;
	}
	case Shape_Circle:
	{
		double d2 = (scenePos.x() - shape->cx) * (scenePos.x() - shape->cx)
		          + (scenePos.y() - shape->cy) * (scenePos.y() - shape->cy);
		return d2 <= shape->r * shape->r;
	}
	case Shape_Ellipse:
	{
		double rad = -qDegreesToRadians(shape->angle);
		double dx  = scenePos.x() - shape->cx;
		double dy  = scenePos.y() - shape->cy;
		double lx  = dx * qCos(rad) - dy * qSin(rad);
		double ly  = dx * qSin(rad) + dy * qCos(rad);
		double r1  = shape->r;
		double r2  = shape->r2;
		return (lx * lx) / (r1 * r1) + (ly * ly) / (r2 * r2) <= 1.0;
	}
	case Shape_Ring:
	{
		double d2 = (scenePos.x() - shape->cx) * (scenePos.x() - shape->cx)
		          + (scenePos.y() - shape->cy) * (scenePos.y() - shape->cy);
		double rLarger  = std::max(shape->r, shape->r2);
		double rSmaller = std::min(shape->r, shape->r2);
		return d2 <= rLarger * rLarger && d2 >= rSmaller * rSmaller;
	}
	case Shape_Arc:
	{
		double cx = shape->cx, cy = shape->cy;
		double rBig = std::max(shape->r, shape->r2);
		double rSml = std::min(shape->r, shape->r2);
		double dx = scenePos.x() - cx, dy = scenePos.y() - cy, d = std::sqrt(dx * dx + dy * dy);
		if (d > rBig + 8.0 || d < rSml - 8.0) return false;
		double ang = normAngle360(qRadiansToDegrees(std::atan2(dy, dx)));
		double sa = normAngle360(shape->startAngle), span = shape->span;
		double rel = ang - sa; if (rel < 0) rel += 360.0;
		if (span >= 0) return rel <= span;
		return rel >= 360.0 + span;  // 顺时针弧：检测"非缺口"范围
	}
	case Shape_Polygon:
	{
		QPolygonF poly; for (auto& pt : shape->pts) poly << pt;
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
		double hw = shape.w / 2.0, hh = shape.h / 2.0;
		pts << QPointF(shape.cx - hw, shape.cy - hh)
		    << QPointF(shape.cx + hw, shape.cy - hh)
		    << QPointF(shape.cx + hw, shape.cy + hh)
		    << QPointF(shape.cx - hw, shape.cy + hh);
		break;
	}
	case Shape_RotateRect:
	{
		double hw = shape.w / 2.0, hh = shape.h / 2.0;
		double rad = qDegreesToRadians(shape.angle);
		QPointF local[4] = { {-hw,-hh}, {hw,-hh}, {hw,hh}, {-hw,hh} };
		for (auto& p : local)
		{
			double lx = p.x(), ly = p.y();
			double rx = lx * qCos(rad) - ly * qSin(rad);
			double ry = lx * qSin(rad) + ly * qCos(rad);
			pts << QPointF(rx + shape.cx, ry + shape.cy);
		}
		break;
	}
	case Shape_Circle:
	{
		double cx = shape.cx, cy = shape.cy, r = shape.r;
		for (int i = 0; i < n; ++i)
		{
			double a = 2.0 * M_PI * i / n;
			pts << QPointF(cx + r * qCos(a), cy + r * qSin(a));
		}
		break;
	}
	case Shape_Ellipse:
	{
		double cx = shape.cx, cy = shape.cy, r1 = shape.r, r2 = shape.r2;
		double rad = qDegreesToRadians(shape.angle);
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
		double cx = shape.cx, cy = shape.cy;
		double rL = std::max(shape.r, shape.r2), rS = std::min(shape.r, shape.r2);
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
		double cx = shape.cx, cy = shape.cy;
		double rO = shape.r, rI = shape.r2;
		double saR = qDegreesToRadians(shape.startAngle), spR = qDegreesToRadians(shape.span);
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
		pts = shape.pts;
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
		double hw = shape.w / 2.0, hh = shape.h / 2.0;
		path.addRect(QRectF(shape.cx - hw, shape.cy - hh, shape.w, shape.h));
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
		path.addEllipse(QPointF(shape.cx, shape.cy), shape.r, shape.r);
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
		double rL = std::max(shape.r, shape.r2), rS = std::min(shape.r, shape.r2);
		QPainterPath o; o.addEllipse(QPointF(shape.cx, shape.cy), rL, rL);
		QPainterPath i; i.addEllipse(QPointF(shape.cx, shape.cy), rS, rS);
		path = o.subtracted(i);
		break;
	}
	case Shape_Arc:
	{
		double cx = shape.cx, cy = shape.cy;
		double rO = shape.r, rI = shape.r2;
		double saR = qDegreesToRadians(shape.startAngle), spR = qDegreesToRadians(shape.span);
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
		QPolygonF poly = shape.pts;
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
		double hw = shape.w / 2.0, hh = shape.h / 2.0;
		return QRectF(shape.cx - hw, shape.cy - hh, shape.w, shape.h);
	}
	case Shape_RotateRect:
	case Shape_Ellipse:
		return toPoints(shape).boundingRect();
	case Shape_Circle:
		return QRectF(shape.cx - shape.r, shape.cy - shape.r,
		              shape.r * 2, shape.r * 2);
	case Shape_Ring:
	{
		double r = std::max(shape.r, shape.r2);
		return QRectF(shape.cx - r, shape.cy - r, r * 2, r * 2);
	}
	case Shape_Arc:
		return toPoints(shape).boundingRect();
	case Shape_Polygon:
		return toPoints(shape).boundingRect();
	default:
		return QRectF();
	}
}

bool ShapeGeometry::cropRect(const DrawShapeItem& shape, int imgW, int imgH, QRect& out)
{
	const QRectF b = boundingRect(shape);
	if (b.width() <= 0 || b.height() <= 0)
		return false;

	// 四舍五入到整数像素
	const int x = qRound(b.left());
	const int y = qRound(b.top());
	const int w = qRound(b.width());
	const int h = qRound(b.height());

	// 完全出界判定（在图像范围之外）
	if (x >= imgW || y >= imgH || x + w <= 0 || y + h <= 0)
		return false;

	// 与 [0,imgW)x[0,imgH) 求交
	const int x0 = std::max(x, 0);
	const int y0 = std::max(y, 0);
	const int x1 = std::min(x + w, imgW);
	const int y1 = std::min(y + h, imgH);

	if (x1 <= x0 || y1 <= y0)
		return false;

	out = QRect(x0, y0, x1 - x0, y1 - y0);
	return true;
}

/* ===== 位姿校正（定位 → ROI 跟随） ===== */

DrawShapeItem ShapeGeometry::applyPose(const DrawShapeItem& base, const Pose2D& pose, const QString& sourceToolId)
{
	DrawShapeItem out = base;               /*  值拷贝：新 ROI 不修改基准 */
	out.sourceToolId = sourceToolId;        /*  来源标记：该 ROI 由定位工具校正生成 */

	const double s   = pose.scale;
	const double rad = pose.angle;          /*  弧度 */
	const double c   = qCos(rad);
	const double sn  = qSin(rad);

	/*  点变换：P' = scale · R(angle) · P + T（先旋转缩放、再平移） */
	auto transformPt = [&](const QPointF& p) -> QPointF
	{
		const double x = p.x(), y = p.y();
		const double rx = (x * c - y * sn) * s;
		const double ry = (x * sn + y * c) * s;
		return QPointF(rx + pose.tx, ry + pose.ty);
	};

	switch (base.type)
	{
	case Shape_Rect:
	case Shape_Circle:
	case Shape_Ring:
	{
		/*  无角度形状：仅中心点随位姿平移/旋转/缩放 */
		QPointF c2 = transformPt(QPointF(base.cx, base.cy));
		out.cx = c2.x(); out.cy = c2.y();
		out.w  = base.w  * s; out.h = base.h  * s;   /*  缩放尺寸 */
		out.r  = base.r  * s; out.r2 = base.r2 * s;
		break;
	}
	case Shape_RotateRect:
	case Shape_Ellipse:
	{
		QPointF c2 = transformPt(QPointF(base.cx, base.cy));
		out.cx = c2.x(); out.cy = c2.y();
		out.w  = base.w  * s; out.h  = base.h  * s;
		out.r  = base.r  * s; out.r2 = base.r2 * s;
		/*  角度叠加：工件转多少(弧度转度)，形状角度也加多少 */
		out.angle = base.angle + qRadiansToDegrees(rad);
		break;
	}
	case Shape_Arc:
	{
		QPointF c2 = transformPt(QPointF(base.cx, base.cy));
		out.cx = c2.x(); out.cy = c2.y();
		out.r  = base.r  * s; out.r2 = base.r2 * s;
		out.startAngle = base.startAngle + qRadiansToDegrees(rad);
		out.endAngle   = base.endAngle   + qRadiansToDegrees(rad);
		break;
	}
	case Shape_Polygon:
	{
		out.pts.clear();
		for (const QPointF& p : base.pts)
			out.pts.append(transformPt(p));
		break;
	}
	default:
		break;
	}
	return out;
}
