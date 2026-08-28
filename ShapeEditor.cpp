#include "ShapeEditor.h"

#include <QPointF>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include "ShapeGeometry.h"

/* ===== 矩形 Handle 拖拽 ===== */
bool ShapeEditor::updateRect(DrawShapeItem* shape, const EditContext& ctx, const QPointF& scenePos)
{
	if (!shape || ctx.handleIdx < 0) return false;

	double sx = scenePos.x(), sy = scenePos.y();
	double& cx = shape->cx;
	double& cy = shape->cy;
	double& w  = shape->w;
	double& h  = shape->h;

	double left0   = ctx.startRect.cx - ctx.startRect.w / 2.0;
	double right0  = ctx.startRect.cx + ctx.startRect.w / 2.0;
	double top0    = ctx.startRect.cy - ctx.startRect.h / 2.0;
	double bottom0 = ctx.startRect.cy + ctx.startRect.h / 2.0;

	double left = left0, right = right0, top = top0, bottom = bottom0;

	switch (ctx.handleIdx)
	{
	case 0: left = sx; top = sy;    break;
	case 1: top = sy;               break;
	case 2: right = sx; top = sy;   break;
	case 3: right = sx;             break;
	case 4: right = sx; bottom = sy;break;
	case 5: bottom = sy;            break;
	case 6: left = sx; bottom = sy; break;
	case 7: left = sx;              break;
	}
	if (left > right) std::swap(left, right);
	if (top > bottom) std::swap(top, bottom);
	cx = (left+right)/2.0;
	cy = (top+bottom)/2.0;
	w  = std::max(right-left, 3.0);
	h  = std::max(bottom-top, 3.0);

	return true;
}

/* ===== 旋转矩形 Handle 拖拽 ===== */
bool ShapeEditor::updateRotatedRect(DrawShapeItem* shape, const EditContext& ctx, const QPointF& scenePos)
{
	if (!shape) return false;

	if (ctx.isRotating)
	{
		// 旋转手柄 → 计算中心到鼠标的角度
		double cx = shape->cx;
		double cy = shape->cy;
		double angle = qRadiansToDegrees(qAtan2(scenePos.y()-cy, scenePos.x()-cx)) + 90.0;
		shape->angle = angle;
		return true;
	}
	else if (ctx.isDragging)
	{
		// 将鼠标点逆旋转到局部坐标系（相对于拖拽起始中心）
		double startAngle = ctx.dragStartAngle;  // 拖拽起始角度
		double rad   = -qDegreesToRadians(startAngle);
		double sx    = scenePos.x();
		double sy    = scenePos.y();
		double dx    = sx - ctx.startRect.cx;
		double dy    = sy - ctx.startRect.cy;
		double lx    = dx * qCos(rad) - dy * qSin(rad);
		double ly    = dx * qSin(rad) + dy * qCos(rad);

		double startW = ctx.startRect.w, startH = ctx.startRect.h;
		double left0   = -startW / 2.0;
		double right0  =  startW / 2.0;
		double top0    = -startH / 2.0;
		double bottom0 =  startH / 2.0;

		double left = left0, right = right0, top = top0, bottom = bottom0;

		// 和普通矩形一样的 8 方向拖拽
		switch (ctx.handleIdx)
		{
		case 0: left = lx; top = ly;         break;  // 左上角
		case 1:           top = ly;          break;  // 上边中点
		case 2: right = lx; top = ly;        break;  // 右上角
		case 3: right = lx;                  break;  // 右边中点
		case 4: right = lx; bottom = ly;     break;  // 右下角
		case 5:           bottom = ly;       break;  // 下边中点
		case 6: left = lx; bottom = ly;      break;  // 左下角
		case 7: left = lx;                   break;  // 左边中点
		}
		if (left > right) std::swap(left, right);
		if (top > bottom) std::swap(top, bottom);

		double newW = std::max(right - left, 3.0);
		double newH = std::max(bottom - top, 3.0);
		double newCxLocal = (left + right) / 2.0;
		double newCyLocal = (top + bottom) / 2.0;

		// 新中心从局部坐标转回世界坐标（绕拖拽起始中心旋转）
		double cosA = qCos(qDegreesToRadians(startAngle));
		double sinA = qSin(qDegreesToRadians(startAngle));
		shape->cx = ctx.startRect.cx + newCxLocal*cosA - newCyLocal*sinA;
		shape->cy = ctx.startRect.cy + newCxLocal*sinA + newCyLocal*cosA;
		shape->w  = newW;
		shape->h  = newH;

		return true;
	}
	return false;
}

/* ===== 圆形 Handle 拖拽 ===== */
bool ShapeEditor::updateCircle(DrawShapeItem* shape, const EditContext& ctx, const QPointF& scenePos)
{
	if (!shape || ctx.handleIdx < 0 || ctx.handleIdx >= 4) return false;

	// 圆形 4 个控制点：0=右, 1=上, 2=左, 3=下
	// 拖拽时改变半径，保持中心
	double cx0 = ctx.startRect.cx, cy0 = ctx.startRect.cy;
	double r0  = ctx.startRect.w / 2.0;

	double dx = scenePos.x() - cx0;
	double dy = scenePos.y() - cy0;

	double newR = 0;
	switch (ctx.handleIdx)
	{
	case 0: newR = dx;                    break; // 右边 → r = dx
	case 1: newR = -dy;                   break; // 上边 → r = -dy
	case 2: newR = -dx;                   break; // 左边 → r = -dx
	case 3: newR = dy;                    break; // 下边 → r = dy
	}
	newR = std::max(newR, 1.5);

	shape->cx = cx0;
	shape->cy = cy0;
	shape->r  = newR;

	return true;
}

/* ===== 圆环 Handle 拖拽 ===== */
bool ShapeEditor::updateRing(DrawShapeItem* shape, const EditContext& ctx, const QPointF& scenePos)
{
	if (!shape || ctx.handleIdx < 0 || ctx.handleIdx >= 8) return false;

	// ctx.startRect: cx, cy, 拖拽前 r1*2, 拖拽前 r2*2
	double cx0 = ctx.startRect.cx, cy0 = ctx.startRect.cy;
	double oldR1 = ctx.startRect.w / 2.0;
	double oldR2 = ctx.startRect.h / 2.0;

	double dx = scenePos.x() - cx0;
	double dy = scenePos.y() - cy0;

	// 0-3 是原来较大半径的控制点，4-7 是原来较小半径的控制点
	// 拖拽哪个就修改对应的 r1 或 r2，不限制大小关系
	// 显示时自动按 max/min 区分外圆内圆
	if (ctx.handleIdx < 4)
	{
		double newR = 0;
		switch (ctx.handleIdx)
		{
		case 0: newR = dx;    break;
		case 1: newR = -dy;   break;
		case 2: newR = -dx;   break;
		case 3: newR = dy;    break;
		}
		newR = std::max(newR, 1.0);
		// 更新当前拖拽的那个半径（可能是 r1 也可能是 r2）
		if (oldR1 >= oldR2)
			shape->r = newR;
		else
			shape->r2 = newR;
	}
	else
	{
		double newR = 0;
		switch (ctx.handleIdx)
		{
		case 4: newR = dx;    break;
		case 5: newR = -dy;   break;
		case 6: newR = -dx;   break;
		case 7: newR = dy;    break;
		}
		newR = std::max(newR, 1.0);
		if (oldR1 >= oldR2)
			shape->r2 = newR;
		else
			shape->r = newR;
	}

	shape->cx = cx0;
	shape->cy = cy0;

	return true;
}

/* ===== 多边形 Handle 拖拽 ===== */
bool ShapeEditor::updatePolygon(DrawShapeItem* shape, const EditContext& ctx, const QPointF& scenePos)
{
	if(!shape||ctx.handleIdx<0||ctx.handleIdx>=shape->pts.size())return false;
	shape->pts[ctx.handleIdx]=scenePos;
	return true;
}

/* ===== 扇环 Handle 拖拽 ===== */
bool ShapeEditor::updateArc(DrawShapeItem* shape, const EditContext& ctx, const QPointF& scenePos)
{
	if (!shape) return false;

	double cx = shape->cx, cy = shape->cy;

	// 旋转
	if (ctx.isRotating) {
		double newMidAng = ShapeGeometry::normAngle360(qRadiansToDegrees(std::atan2(scenePos.y()-cy, scenePos.x()-cx)));
		double delta = newMidAng - ctx.dragStartAngle;
		if (delta > 180.0) delta -= 360.0;
		if (delta < -180.0) delta += 360.0;
		shape->startAngle += delta;
		ctx.dragStartAngle = newMidAng;
		return true;
	}

	if (ctx.handleIdx < 0 || ctx.handleIdx >= 6) return false;

	if(ctx.handleIdx==0||ctx.handleIdx==1||ctx.handleIdx==4||ctx.handleIdx==5){
		double na=ShapeGeometry::normAngle360(qRadiansToDegrees(std::atan2(scenePos.y()-cy,scenePos.x()-cx)));
		if(ctx.handleIdx==0||ctx.handleIdx==4)shape->startAngle=na;else shape->endAngle=na;
		double sa=shape->startAngle,ea=shape->endAngle;bool cw=(shape->span<0);
		double sf=ea-sa;if(sf<=0)sf+=360.0;shape->span=cw?(sf-360.0):sf;
		// 归一化未拖拽的那一端，避免残留 >360 的值导致后续计算异常
		if(ctx.handleIdx==0||ctx.handleIdx==4)
			shape->endAngle=ShapeGeometry::normAngle360(shape->endAngle);
		else
			shape->startAngle=ShapeGeometry::normAngle360(shape->startAngle);
	}else if(ctx.handleIdx==2){double nr=std::sqrt((scenePos.x()-cx)*(scenePos.x()-cx)+(scenePos.y()-cy)*(scenePos.y()-cy));shape->r=std::max(nr,1.5);
	}else if(ctx.handleIdx==3){double nr=std::sqrt((scenePos.x()-cx)*(scenePos.x()-cx)+(scenePos.y()-cy)*(scenePos.y()-cy));shape->r2=std::max(nr,1.5);}
	return true;
}

/* ===== 椭圆 Handle 拖拽 ===== */
bool ShapeEditor::updateEllipse(DrawShapeItem* shape, const EditContext& ctx, const QPointF& scenePos)
{
	if (!shape || ctx.handleIdx < 0 || ctx.handleIdx >= 4) return false;

	if (ctx.isRotating)
	{
		// 旋转手柄 → 计算角度
		double cx = shape->cx;
		double cy = shape->cy;
		double angle = qRadiansToDegrees(qAtan2(scenePos.y()-cy, scenePos.x()-cx)) + 90.0;
		shape->angle = angle;
		return true;
	}

	// 普通控制点拖拽：先转回局部坐标系
	double angle = shape->angle;
	double rad   = -qDegreesToRadians(angle);
	double dx    = scenePos.x() - ctx.startRect.cx;
	double dy    = scenePos.y() - ctx.startRect.cy;
	double lx    = dx * qCos(rad) - dy * qSin(rad);
	double ly    = dx * qSin(rad) + dy * qCos(rad);

	double newR1 = ctx.startRect.w / 2.0;
	double newR2 = ctx.startRect.h / 2.0;

	switch (ctx.handleIdx)
	{
	case 0: newR1 =  lx; break;  // 右
	case 1: newR2 = -ly; break;  // 上
	case 2: newR1 = -lx; break;  // 左
	case 3: newR2 =  ly; break;  // 下
	}
	newR1 = std::max(newR1, 1.5);
	newR2 = std::max(newR2, 1.5);

	shape->cx = ctx.startRect.cx;
	shape->cy = ctx.startRect.cy;
	shape->r = newR1;
	shape->r2 = newR2;

	return true;
}
