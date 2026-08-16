#include "AnnotationIO.h"

#include <QJsonArray>

static const char* kTypeRect       = "rect";
static const char* kTypeRotatedRect= "rotatedRect";
static const char* kTypeCircle     = "circle";
static const char* kTypeEllipse    = "ellipse";
static const char* kTypeRing       = "ring";
static const char* kTypeArc        = "arc";       /*  扇环 */
static const char* kTypePolygon    = "polygon";

QJsonObject AnnotationIO::shapeToJson(const DrawShapeItem& s)
{
	QJsonObject o;
	switch (s.type)
	{
	case Shape_Rect:
		o["type"] = kTypeRect;
		o["cx"] = s.rect.cx; o["cy"] = s.rect.cy; o["w"] = s.rect.w; o["h"] = s.rect.h;
		break;
	case Shape_RotateRect:
		o["type"] = kTypeRotatedRect;
		o["cx"] = s.rotatedRect.cx; o["cy"] = s.rotatedRect.cy;
		o["w"] = s.rotatedRect.w;   o["h"] = s.rotatedRect.h;
		o["angle"] = s.rotatedRect.angle;
		break;
	case Shape_Circle:
		o["type"] = kTypeCircle;
		o["cx"] = s.circle.cx; o["cy"] = s.circle.cy; o["r"] = s.circle.r;
		break;
	case Shape_Ellipse:
		o["type"] = kTypeEllipse;
		o["cx"] = s.ellipse.cx; o["cy"] = s.ellipse.cy;
		o["r1"] = s.ellipse.r1; o["r2"] = s.ellipse.r2;
		o["angle"] = s.ellipse.angle;
		break;
	case Shape_Ring:
		o["type"] = kTypeRing;
		o["cx"] = s.ring.cx; o["cy"] = s.ring.cy; o["r1"] = s.ring.r1; o["r2"] = s.ring.r2;
		break;
	case Shape_Arc:
		o["type"] = kTypeArc;
		o["cx"] = s.arc.cx; o["cy"] = s.arc.cy;
		o["rOuter"] = s.arc.rOuter; o["rInner"] = s.arc.rInner;
		o["startAngle"] = s.arc.startAngle; o["endAngle"] = s.arc.endAngle;
		o["span"] = s.arc.r1;
		break;
	case Shape_Polygon:
	{
		o["type"] = kTypePolygon;
		QJsonArray pts;
		for (const QPointF& p : s.polygon.pts)
		{
			QJsonArray pt;
			pt.append(p.x());
			pt.append(p.y());
			pts.append(pt);
		}
		o["points"] = pts;
		break;
	}
	default:
		break;
	}
	return o;
}

DrawShapeItem* AnnotationIO::shapeFromJson(const QJsonObject& o)
{
	const QString type = o["type"].toString();
	DrawShapeType t;
	if      (type == kTypeRect)        t = Shape_Rect;
	else if (type == kTypeRotatedRect) t = Shape_RotateRect;
	else if (type == kTypeCircle)      t = Shape_Circle;
	else if (type == kTypeEllipse)     t = Shape_Ellipse;
	else if (type == kTypeRing)        t = Shape_Ring;
	else if (type == kTypeArc)         t = Shape_Arc;
	else if (type == kTypePolygon)     t = Shape_Polygon;
	else return nullptr;

	DrawShapeItem* s = new DrawShapeItem(t);
	switch (t)
	{
	case Shape_Rect:
		s->rect = { o["cx"].toDouble(), o["cy"].toDouble(), o["w"].toDouble(), o["h"].toDouble() };
		break;
	case Shape_RotateRect:
		s->rotatedRect = { o["cx"].toDouble(), o["cy"].toDouble(), o["w"].toDouble(),
			               o["h"].toDouble(), o["angle"].toDouble() };
		break;
	case Shape_Circle:
		s->circle = { o["cx"].toDouble(), o["cy"].toDouble(), o["r"].toDouble() };
		break;
	case Shape_Ellipse:
		s->ellipse = { o["cx"].toDouble(), o["cy"].toDouble(), o["r1"].toDouble(),
			           o["r2"].toDouble(), o["angle"].toDouble() };
		break;
	case Shape_Ring:
		s->ring = { o["cx"].toDouble(), o["cy"].toDouble(), o["r1"].toDouble(), o["r2"].toDouble() };
		break;
	case Shape_Arc:
		s->arc.cx = o["cx"].toDouble();       s->arc.cy = o["cy"].toDouble();
		s->arc.rOuter = o["rOuter"].toDouble(); s->arc.rInner = o["rInner"].toDouble();
		s->arc.startAngle = o["startAngle"].toDouble();
		s->arc.endAngle = o["endAngle"].toDouble();
		s->arc.r1 = o["span"].toDouble();
		break;
	case Shape_Polygon:
	{
		const QJsonArray pts = o["points"].toArray();
		for (const auto& v : pts)
		{
			const QJsonArray pt = v.toArray();
			if (pt.size() >= 2)
				s->polygon.pts.append(QPointF(pt[0].toDouble(), pt[1].toDouble()));
		}
		break;
	}
	default:
		break;
	}
	return s;
}
