#include "ShapePainter.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QPainterPath>
#include <QPen>
#include <QTransform>
#include <QtMath>

QGraphicsItem* ShapePainter::buildItem(const DrawShapeItem& shape) {
	QGraphicsItem* item = nullptr;
	switch (shape.type) {
	case Shape_Rect: {
		auto* r = new QGraphicsRectItem(shape.rect.cx - shape.rect.w / 2, shape.rect.cy - shape.rect.h / 2, shape.rect.w, shape.rect.h);
		r->setZValue(80); item = r; break;
	}
	case Shape_RotateRect: {
		double hw = shape.rotatedRect.w / 2, hh = shape.rotatedRect.h / 2, cx = shape.rotatedRect.cx, cy = shape.rotatedRect.cy, rad = qDegreesToRadians(shape.rotatedRect.angle);
		QPolygonF wp; QPointF pp[4] = { {-hw,-hh},{hw,-hh},{hw,hh},{-hw,hh} };
		for (auto& pt : pp) { double rx = pt.x() * qCos(rad) - pt.y() * qSin(rad); double ry = pt.x() * qSin(rad) + pt.y() * qCos(rad); wp << QPointF(rx + cx, ry + cy); }
		auto* p = new QGraphicsPolygonItem(wp); p->setZValue(80); item = p; break;
	}
	case Shape_Circle: {
		double r = shape.circle.r;
		auto* e = new QGraphicsEllipseItem(shape.circle.cx - r, shape.circle.cy - r, r * 2, r * 2);
		e->setZValue(80); item = e; break;
	}
	case Shape_Polygon: {
		QPolygonF poly; for (auto& pt : shape.polygon.pts) poly << pt;
		auto* p = new QGraphicsPolygonItem(poly); p->setZValue(80); item = p; break;
	}
	case Shape_Ellipse: {
		QPainterPath path; path.addEllipse(QPointF(0, 0), shape.ellipse.r1, shape.ellipse.r2);
		QTransform t; t.translate(shape.ellipse.cx, shape.ellipse.cy); t.rotate(shape.ellipse.angle);
		auto* p = new QGraphicsPathItem(); p->setPath(t.map(path)); p->setZValue(80); item = p; break;
	}
	case Shape_Ring: {
		double rL = std::max(shape.ring.r1, shape.ring.r2), rS = std::min(shape.ring.r1, shape.ring.r2);
		QPainterPath o, i; o.addEllipse(QPointF(0, 0), rL, rL); i.addEllipse(QPointF(0, 0), rS, rS);
		QTransform t; t.translate(shape.ring.cx, shape.ring.cy);
		auto* p = new QGraphicsPathItem(); p->setPath(t.map(o.subtracted(i))); p->setZValue(80); item = p; break;
	}
	case Shape_Arc: {
		double cx = shape.arc.cx, cy = shape.arc.cy, rO = shape.arc.rOuter, rI = shape.arc.rInner, sa = shape.arc.startAngle, sp = shape.arc.r1;
		double saR = qDegreesToRadians(sa), spR = qDegreesToRadians(sp); const int N = 128;
		QPainterPath path; path.moveTo(cx + rO * qCos(saR), cy + rO * qSin(saR));
		for (int i = 1; i <= N; ++i) { double a = saR + spR * i / N; path.lineTo(cx + rO * qCos(a), cy + rO * qSin(a)); }
		double eR = saR + spR; path.lineTo(cx + rI * qCos(eR), cy + rI * qSin(eR));
		for (int i = N; i >= 0; --i) { double a = saR + spR * i / N; path.lineTo(cx + rI * qCos(a), cy + rI * qSin(a)); }
		path.closeSubpath(); auto* p = new QGraphicsPathItem(); p->setPath(path); p->setZValue(80); item = p; break;
	}
	default: break;
	}
	return item;
}

void ShapePainter::applyStyle(QGraphicsItem* item, const QColor& color, double penWidth, bool isHovered) {
	if (!item) return;
	int w = isHovered ? (int)(penWidth + 2) : (int)penWidth;
	QPen pen(color, w);
	pen.setCosmetic(true);  
	if (auto* r = dynamic_cast<QGraphicsRectItem*>(item))       { r->setPen(pen); r->setBrush(Qt::NoBrush); }
	else if (auto* po = dynamic_cast<QGraphicsPolygonItem*>(item)) { po->setPen(pen); po->setBrush(Qt::NoBrush); }
	else if (auto* e = dynamic_cast<QGraphicsEllipseItem*>(item))  { e->setPen(pen); e->setBrush(Qt::NoBrush); }
	else if (auto* pa = dynamic_cast<QGraphicsPathItem*>(item))    { pa->setPen(pen); pa->setBrush(Qt::NoBrush); }
}
