#include "RecipeIO.h"

#include <QJsonArray>

static const char* kTypeRect       = "rect";
static const char* kTypeRotatedRect= "rotatedRect";
static const char* kTypeCircle     = "circle";
static const char* kTypeEllipse    = "ellipse";
static const char* kTypeRing       = "ring";
static const char* kTypeArc        = "arc";       /*  扇环 */
static const char* kTypePolygon    = "polygon";

QJsonObject RecipeIO::shapeToJson(const DrawShapeItem& s)
{
	QJsonObject o;
	switch (s.type)
	{
	case Shape_Rect:
		o["type"] = kTypeRect;
		o["cx"] = s.cx; o["cy"] = s.cy; o["w"] = s.w; o["h"] = s.h;
		break;
	case Shape_RotateRect:
		o["type"] = kTypeRotatedRect;
		o["cx"] = s.cx; o["cy"] = s.cy;
		o["w"] = s.w;   o["h"] = s.h;
		o["angle"] = s.angle;
		break;
	case Shape_Circle:
		o["type"] = kTypeCircle;
		o["cx"] = s.cx; o["cy"] = s.cy; o["r"] = s.r;
		break;
	case Shape_Ellipse:
		o["type"] = kTypeEllipse;
		o["cx"] = s.cx; o["cy"] = s.cy;
		o["r1"] = s.r; o["r2"] = s.r2;
		o["angle"] = s.angle;
		break;
	case Shape_Ring:
		o["type"] = kTypeRing;
		o["cx"] = s.cx; o["cy"] = s.cy; o["r1"] = s.r; o["r2"] = s.r2;
		break;
	case Shape_Arc:
		o["type"] = kTypeArc;
		o["cx"] = s.cx; o["cy"] = s.cy;
		o["rOuter"] = s.r; o["rInner"] = s.r2;
		o["startAngle"] = s.startAngle;
		o["endAngle"] = s.endAngle;
		o["span"] = s.span;
		break;
	case Shape_Polygon:
	{
		o["type"] = kTypePolygon;
		QJsonArray pts;
		for (const QPointF& p : s.pts)
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

DrawShapeItem* RecipeIO::shapeFromJson(const QJsonObject& o)
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
		s->cx = o["cx"].toDouble(); s->cy = o["cy"].toDouble();
		s->w  = o["w"].toDouble();  s->h  = o["h"].toDouble();
		break;
	case Shape_RotateRect:
		s->cx = o["cx"].toDouble(); s->cy = o["cy"].toDouble();
		s->w  = o["w"].toDouble();  s->h  = o["h"].toDouble();
		s->angle = o["angle"].toDouble();
		break;
	case Shape_Circle:
		s->cx = o["cx"].toDouble(); s->cy = o["cy"].toDouble(); s->r = o["r"].toDouble();
		break;
	case Shape_Ellipse:
		s->cx = o["cx"].toDouble(); s->cy = o["cy"].toDouble();
		s->r  = o["r1"].toDouble(); s->r2 = o["r2"].toDouble();
		s->angle = o["angle"].toDouble();
		break;
	case Shape_Ring:
		s->cx = o["cx"].toDouble(); s->cy = o["cy"].toDouble();
		s->r  = o["r1"].toDouble(); s->r2 = o["r2"].toDouble();
		break;
	case Shape_Arc:
		s->cx = o["cx"].toDouble(); s->cy = o["cy"].toDouble();
		s->r  = o["rOuter"].toDouble(); s->r2 = o["rInner"].toDouble();
		s->startAngle = o["startAngle"].toDouble();
		s->endAngle   = o["endAngle"].toDouble();
		s->span       = o["span"].toDouble();
		break;
	case Shape_Polygon:
	{
		const QJsonArray pts = o["points"].toArray();
		for (const auto& v : pts)
		{
			const QJsonArray pt = v.toArray();
			if (pt.size() >= 2)
				s->pts.append(QPointF(pt[0].toDouble(), pt[1].toDouble()));
		}
		break;
	}
	default:
		break;
	}
	return s;
}
