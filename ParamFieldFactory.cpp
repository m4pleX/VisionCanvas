#include "ParamFieldFactory.h"

QList<ParamField> ParamFieldFactory::buildFields(DrawShapeType type)
{
	QList<ParamField> fields;
	switch (type) {
	case Shape_Rect:
		fields = {
			{QStringLiteral("中心 X"), -999999, 999999, [](const DrawShapeItem& s){return s.cx;}, [](DrawShapeItem& s,double v){s.cx=v;}},
			{QStringLiteral("中心 Y"), -999999, 999999, [](const DrawShapeItem& s){return s.cy;}, [](DrawShapeItem& s,double v){s.cy=v;}},
			{QStringLiteral("宽度"),   -999999, 999999, [](const DrawShapeItem& s){return s.w;},  [](DrawShapeItem& s,double v){s.w=v;}},
			{QStringLiteral("高度"),   -999999, 999999, [](const DrawShapeItem& s){return s.h;},  [](DrawShapeItem& s,double v){s.h=v;}},
		}; break;
	case Shape_RotateRect:
		fields = {
			{QStringLiteral("中心 X"), -999999, 999999, [](const DrawShapeItem& s){return s.cx;}, [](DrawShapeItem& s,double v){s.cx=v;}},
			{QStringLiteral("中心 Y"), -999999, 999999, [](const DrawShapeItem& s){return s.cy;}, [](DrawShapeItem& s,double v){s.cy=v;}},
			{QStringLiteral("宽度"),   -999999, 999999, [](const DrawShapeItem& s){return s.w;},  [](DrawShapeItem& s,double v){s.w=v;}},
			{QStringLiteral("高度"),   -999999, 999999, [](const DrawShapeItem& s){return s.h;},  [](DrawShapeItem& s,double v){s.h=v;}},
			{QStringLiteral("角度"),   -999999, 999999, [](const DrawShapeItem& s){return s.angle;}, [](DrawShapeItem& s,double v){s.angle=v;}},
		}; break;
	case Shape_Circle:
		fields = {
			{QStringLiteral("中心 X"), -999999, 999999, [](const DrawShapeItem& s){return s.cx;}, [](DrawShapeItem& s,double v){s.cx=v;}},
			{QStringLiteral("中心 Y"), -999999, 999999, [](const DrawShapeItem& s){return s.cy;}, [](DrawShapeItem& s,double v){s.cy=v;}},
			{QStringLiteral("半径"),   -999999, 999999, [](const DrawShapeItem& s){return s.r;},  [](DrawShapeItem& s,double v){s.r=v;}},
		}; break;
	case Shape_Ellipse:
		fields = {
			{QStringLiteral("中心 X"),  -999999, 999999, [](const DrawShapeItem& s){return s.cx;}, [](DrawShapeItem& s,double v){s.cx=v;}},
			{QStringLiteral("中心 Y"),  -999999, 999999, [](const DrawShapeItem& s){return s.cy;}, [](DrawShapeItem& s,double v){s.cy=v;}},
			{QStringLiteral("半轴 r1"), -999999, 999999, [](const DrawShapeItem& s){return s.r;}, [](DrawShapeItem& s,double v){s.r=v;}},
			{QStringLiteral("半轴 r2"), -999999, 999999, [](const DrawShapeItem& s){return s.r2;}, [](DrawShapeItem& s,double v){s.r2=v;}},
			{QStringLiteral("角度"),    -999999, 999999, [](const DrawShapeItem& s){return s.angle;}, [](DrawShapeItem& s,double v){s.angle=v;}},
		}; break;
	case Shape_Ring:
		fields = {
			{QStringLiteral("中心 X"),   -999999, 999999, [](const DrawShapeItem& s){return s.cx;}, [](DrawShapeItem& s,double v){s.cx=v;}},
			{QStringLiteral("中心 Y"),   -999999, 999999, [](const DrawShapeItem& s){return s.cy;}, [](DrawShapeItem& s,double v){s.cy=v;}},
			{QStringLiteral("半径 r1"),  -999999, 999999, [](const DrawShapeItem& s){return s.r;}, [](DrawShapeItem& s,double v){s.r=v;}},
			{QStringLiteral("半径 r2"),  -999999, 999999, [](const DrawShapeItem& s){return s.r2;}, [](DrawShapeItem& s,double v){s.r2=v;}},
		}; break;
	case Shape_Arc:
		fields = {
			{QStringLiteral("中心 X"), -999999, 999999, [](const DrawShapeItem& s){return s.cx;},    [](DrawShapeItem& s,double v){s.cx=v;}},
			{QStringLiteral("中心 Y"), -999999, 999999, [](const DrawShapeItem& s){return s.cy;},    [](DrawShapeItem& s,double v){s.cy=v;}},
			{QStringLiteral("半径1"),  -999999, 999999, [](const DrawShapeItem& s){return s.r;}, [](DrawShapeItem& s,double v){s.r=v;}},
			{QStringLiteral("半径2"),  -999999, 999999, [](const DrawShapeItem& s){return s.r2;}, [](DrawShapeItem& s,double v){s.r2=v;}},
			{QStringLiteral("起始角"), 0.0, 360.0, [](const DrawShapeItem& s){return s.startAngle;}, [](DrawShapeItem& s,double v){s.startAngle=v;}},
			{QStringLiteral("跨度"),   -360.0, 360.0, [](const DrawShapeItem& s){return s.span;},       [](DrawShapeItem& s,double v){s.span=v;}},
		}; break;
	case Shape_Polygon:
		// 多边形顶点由上层动态生成，不在工厂内静态构造
		break;
	default: break;
	}
	return fields;
}
