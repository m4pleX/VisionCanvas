#include "ShapeHandleHelper.h"
#include <QGraphicsScene>
#include <QPainterPath>
#include <QPen>
#include <QTransform>
#include <QtMath>

void ShapeHandleHelper::clearHandles(ShapeHandleSet& hs) {
	for (auto* h : hs.handles) { m_scene->removeItem(h); delete h; }
	hs.handles.clear();
	if (hs.rotateHandle) { m_scene->removeItem(hs.rotateHandle); delete hs.rotateHandle; hs.rotateHandle = nullptr; }
	if (hs.centerCrossH) { m_scene->removeItem(hs.centerCrossH); delete hs.centerCrossH; hs.centerCrossH = nullptr; }
	if (hs.centerCrossV) { m_scene->removeItem(hs.centerCrossV); delete hs.centerCrossV; hs.centerCrossV = nullptr; }
	if (hs.rotateStickLine) { m_scene->removeItem(hs.rotateStickLine); delete hs.rotateStickLine; hs.rotateStickLine = nullptr; }
	if (hs.circumRect) { m_scene->removeItem(hs.circumRect); delete hs.circumRect; hs.circumRect = nullptr; }
}

void ShapeHandleHelper::setHandlesVisible(ShapeHandleSet& hs, bool visible) {
	double o = visible ? 1.0 : 0.0;
	for (auto* h : hs.handles) h->setOpacity(o);
	if (hs.rotateHandle) hs.rotateHandle->setOpacity(o);
	if (hs.centerCrossH) hs.centerCrossH->setOpacity(o);
	if (hs.centerCrossV) hs.centerCrossV->setOpacity(o);
	if (hs.circumRect) hs.circumRect->setOpacity(o);
	if (hs.rotateStickLine) hs.rotateStickLine->setOpacity(o);
}



QGraphicsEllipseItem* ShapeHandleHelper::handleAt(const ShapeHandleSet& hs, const QPointF& scenePos) const {
	if (hs.rotateHandle && hs.rotateHandle->isVisible()
		&& hs.rotateHandle->contains(hs.rotateHandle->mapFromScene(scenePos)))
		return hs.rotateHandle;
	for (auto* h : hs.handles)
		if (h->isVisible() && h->contains(h->mapFromScene(scenePos)))
			return h;
	return nullptr;
}

// ---- 辅助 ----

static void addH(QList<QGraphicsEllipseItem*>& handles, QGraphicsScene* sc,
	const QPointF& pos, const QColor& c, int d0, int d1) {
	auto* h = new QGraphicsEllipseItem(pos.x() - kHandleRadius, pos.y() - kHandleRadius,
		kHandleRadius * 2, kHandleRadius * 2);
	h->setPen(QPen(Qt::white, 1)); h->setBrush(c); h->setZValue(100);
	h->setData(0, d0); h->setData(1, d1); h->setAcceptHoverEvents(true);
	sc->addItem(h); handles.append(h);
}

static void addRot(ShapeHandleSet& hs, QGraphicsScene* sc, const QPointF& mid, const QPointF& rot) {
	hs.rotateStickLine = new QGraphicsLineItem(mid.x(), mid.y(), rot.x(), rot.y());
	QPen sp(QColor(220, 220, 220), 1); sp.setCosmetic(true);
	hs.rotateStickLine->setPen(sp); hs.rotateStickLine->setZValue(95);
	sc->addItem(hs.rotateStickLine);
	hs.rotateHandle = new QGraphicsEllipseItem(rot.x() - kHandleRadius, rot.y() - kHandleRadius,
		kHandleRadius * 2, kHandleRadius * 2);
	hs.rotateHandle->setPen(QPen(QColor(255, 200, 0), 2));
	hs.rotateHandle->setBrush(QColor(255, 180, 0)); hs.rotateHandle->setZValue(100);
	hs.rotateHandle->setData(0, 0); hs.rotateHandle->setData(1, 1);
	hs.rotateHandle->setAcceptHoverEvents(true);
	sc->addItem(hs.rotateHandle);
}

static void addCross(ShapeHandleSet& hs, QGraphicsScene* sc, double cx, double cy) {
	hs.centerCrossH = new QGraphicsLineItem();
	hs.centerCrossV = new QGraphicsLineItem();
	QPen cp(QColor(0, 200, 0), 1); cp.setCosmetic(true);
	hs.centerCrossH->setPen(cp); hs.centerCrossV->setPen(cp);
	hs.centerCrossH->setZValue(90); hs.centerCrossV->setZValue(90);
	hs.centerCrossH->setLine(cx - kCrossLen, cy, cx + kCrossLen, cy);
	hs.centerCrossV->setLine(cx, cy - kCrossLen, cx, cy + kCrossLen);
	sc->addItem(hs.centerCrossH); sc->addItem(hs.centerCrossV);
}

static void addCirc(ShapeHandleSet& hs, QGraphicsScene* sc, double cx, double cy, double hw, double hh) {
	hs.circumRect = new QGraphicsRectItem(cx - hw, cy - hh, hw * 2, hh * 2);
	QPen p(QColor(160, 160, 160), 1, Qt::DashLine); p.setCosmetic(true);
	hs.circumRect->setPen(p); hs.circumRect->setBrush(Qt::NoBrush);
	hs.circumRect->setZValue(75); sc->addItem(hs.circumRect);
}

// ===== 重建 =====

void ShapeHandleHelper::rebuildHandles(const DrawShapeItem& shape, ShapeHandleSet& hs) {
	clearHandles(hs);

	double cx = 0, cy = 0, hW = 0, hH = 0, angle = 0;
	int kind = 0; // 0=rect,1=rotRect,2=circle,3=ellipse,4=ring,5=arc,6=poly

	switch (shape.type) {
	case Shape_Rect:         cx = shape.cx; cy = shape.cy; hW = shape.w / 2; hH = shape.h / 2; break;
	case Shape_RotateRect:   cx = shape.cx; cy = shape.cy; hW = shape.w / 2; hH = shape.h / 2; angle = shape.angle; kind = 1; break;
	case Shape_Circle:       cx = shape.cx; cy = shape.cy; hW = shape.r; hH = shape.r; kind = 2; break;
	case Shape_Ellipse:      cx = shape.cx; cy = shape.cy; hW = shape.r; hH = shape.r2; angle = shape.angle; kind = 3; break;
	case Shape_Ring:         cx = shape.cx; cy = shape.cy; kind = 4; break;
	case Shape_Arc:          cx = shape.cx; cy = shape.cy; kind = 5; break;
	case Shape_Polygon:      kind = 6; break;
	}
	if (kind != 4 && kind != 5 && kind != 6 && hW <= 0 && hH <= 0) return;

	if (kind == 2) { // Circle
		QPointF pts[4] = { {hW,0},{0,-hH},{-hW,0},{0,hH} };
		for (int i = 0; i < 4; ++i) addH(hs.handles, m_scene, pts[i] + QPointF(cx, cy), QColor(0, 180, 255), i, 0);
		addCirc(hs, m_scene, cx, cy, shape.r, shape.r);
	} else if (kind == 4) { // Ring
		double rL = std::max(shape.r, shape.r2), rS = std::min(shape.r, shape.r2);
		QPointF oP[4] = { {rL,0},{0,-rL},{-rL,0},{0,rL} };
		for (int i = 0; i < 4; ++i) addH(hs.handles, m_scene, oP[i] + QPointF(cx, cy), QColor(0, 180, 255), i, 0);
		QPointF iP[4] = { {rS,0},{0,-rS},{-rS,0},{0,rS} };
		for (int i = 0; i < 4; ++i) addH(hs.handles, m_scene, iP[i] + QPointF(cx, cy), QColor(255, 140, 0), i + 4, 0);
		addCirc(hs, m_scene, cx, cy, rL, rL);
	} else if (kind == 5) { // Arc
		double rO = shape.r, rI = shape.r2;
		double sa = shape.startAngle, sp = shape.span;
		double saR = qDegreesToRadians(sa), eaR = qDegreesToRadians(sa + sp);
		double midA = sa + sp / 2, midR = qDegreesToRadians(midA);
		QPointF oPts[3] = { {cx + rO * qCos(saR), cy + rO * qSin(saR)}, {cx + rO * qCos(eaR), cy + rO * qSin(eaR)}, {cx + rO * qCos(midR), cy + rO * qSin(midR)} };
		QColor oCs[3] = { QColor(255,80,80), QColor(255,80,80), QColor(0,180,255) };
		for (int i = 0; i < 3; ++i) addH(hs.handles, m_scene, oPts[i], oCs[i], i, 0);
		QPointF iPts[3] = { {cx + rI * qCos(midR), cy + rI * qSin(midR)}, {cx + rI * qCos(saR), cy + rI * qSin(saR)}, {cx + rI * qCos(eaR), cy + rI * qSin(eaR)} };
		QColor iCs[3] = { QColor(255,140,0), QColor(255,80,80), QColor(255,80,80) };
		for (int i = 0; i < 3; ++i) addH(hs.handles, m_scene, iPts[i], iCs[i], i + 3, 0);
		double rRot = std::max(rO, rI);
		addRot(hs, m_scene, QPointF(cx + rRot * qCos(midR), cy + rRot * qSin(midR)),
			QPointF(cx + (rRot + 30) * qCos(midR), cy + (rRot + 30) * qSin(midR)));
	} else if (kind == 6) { // Polygon
		for (int i = 0; i < shape.pts.size(); ++i)
			addH(hs.handles, m_scene, shape.pts[i], QColor(0, 180, 255), i, 0);
	} else if (kind == 3) { // Ellipse
		double rad = qDegreesToRadians(angle);
		QPointF lP[4] = { {hW,0},{0,-hH},{-hW,0},{0,hH} };
		for (int i = 0; i < 4; ++i) {
			double rx = lP[i].x() * qCos(rad) - lP[i].y() * qSin(rad);
			double ry = lP[i].x() * qSin(rad) + lP[i].y() * qCos(rad);
			addH(hs.handles, m_scene, QPointF(rx + cx, ry + cy), QColor(0, 180, 255), i, 0);
		}
		double hw = std::sqrt(hW*hW * qCos(rad)*qCos(rad) + hH*hH * qSin(rad)*qSin(rad));
		double hh = std::sqrt(hW*hW * qSin(rad)*qSin(rad) + hH*hH * qCos(rad)*qCos(rad));
		addCirc(hs, m_scene, cx, cy, hw, hh);
		addRot(hs, m_scene, QPointF(-(-hH) * qSin(rad) + cx, (-hH) * qCos(rad) + cy),
			QPointF(-(-hH) * qSin(rad) + cx + qSin(rad) * 30, (-hH) * qCos(rad) + cy - qCos(rad) * 30));
	} else if (kind == 1) { // RotatedRect
		double rad = qDegreesToRadians(angle);
		QPointF lP[8] = { {-hW,-hH},{0,-hH},{hW,-hH},{hW,0},{hW,hH},{0,hH},{-hW,hH},{-hW,0} };
		for (int i = 0; i < 8; ++i) {
			double rx = lP[i].x() * qCos(rad) - lP[i].y() * qSin(rad);
			double ry = lP[i].x() * qSin(rad) + lP[i].y() * qCos(rad);
			addH(hs.handles, m_scene, QPointF(rx + cx, ry + cy), QColor(0, 180, 255), i, 0);
		}
		double mx = -(-hH) * qSin(rad), my = (-hH) * qCos(rad);
		addRot(hs, m_scene, QPointF(mx + cx, my + cy),
			QPointF(mx + cx + qSin(rad) * 30, my + cy - qCos(rad) * 30));
	} else { // Rect
		QPointF lP[8] = { {-hW,-hH},{0,-hH},{hW,-hH},{hW,0},{hW,hH},{0,hH},{-hW,hH},{-hW,0} };
		for (int i = 0; i < 8; ++i) addH(hs.handles, m_scene, lP[i] + QPointF(cx, cy), QColor(0, 180, 255), i, 0);
	}
	addCross(hs, m_scene, cx, cy);
}

// ===== 更新 =====

void ShapeHandleHelper::updatePositions(const DrawShapeItem& shape, ShapeHandleSet& hs, QGraphicsItem* shapeItem) {
	switch (shape.type) {
	case Shape_Rect: {
		double cx = shape.cx, cy = shape.cy, hw = shape.w / 2, hh = shape.h / 2;
		QPointF lP[8] = { {-hw,-hh},{0,-hh},{hw,-hh},{hw,0},{hw,hh},{0,hh},{-hw,hh},{-hw,0} };
		for (int i = 0; i < hs.handles.size() && i < 8; ++i) moveHandle(hs.handles[i], lP[i] + QPointF(cx, cy));
		if (auto* r = dynamic_cast<QGraphicsRectItem*>(shapeItem)) r->setRect(cx - hw, cy - hh, shape.w, shape.h);
		hs.centerCrossH->setLine(cx - kCrossLen, cy, cx + kCrossLen, cy);
		hs.centerCrossV->setLine(cx, cy - kCrossLen, cx, cy + kCrossLen);
		break;
	}
	case Shape_Circle: {
		double cx = shape.cx, cy = shape.cy, r = shape.r;
		QPointF pts[4] = { {r,0},{0,-r},{-r,0},{0,r} };
		for (int i = 0; i < 4 && i < hs.handles.size(); ++i) moveHandle(hs.handles[i], pts[i] + QPointF(cx, cy));
		if (auto* e = dynamic_cast<QGraphicsEllipseItem*>(shapeItem)) e->setRect(cx - r, cy - r, r * 2, r * 2);
		if (hs.circumRect) hs.circumRect->setRect(cx - r, cy - r, r * 2, r * 2);
		hs.centerCrossH->setLine(cx - kCrossLen, cy, cx + kCrossLen, cy);
		hs.centerCrossV->setLine(cx, cy - kCrossLen, cx, cy + kCrossLen);
		break;
	}
	case Shape_Ring: {
		double cx = shape.cx, cy = shape.cy;
		double rL = std::max(shape.r, shape.r2), rS = std::min(shape.r, shape.r2);
		QPointF oP[4] = { {rL,0},{0,-rL},{-rL,0},{0,rL} };
		for (int i = 0; i < 4 && i < hs.handles.size(); ++i) moveHandle(hs.handles[i], oP[i] + QPointF(cx, cy));
		QPointF iP[4] = { {rS,0},{0,-rS},{-rS,0},{0,rS} };
		for (int i = 0; i < 4 && i + 4 < hs.handles.size(); ++i) moveHandle(hs.handles[i + 4], iP[i] + QPointF(cx, cy));
		if (auto* p = dynamic_cast<QGraphicsPathItem*>(shapeItem)) {
			QPainterPath o, i; o.addEllipse(QPointF(0, 0), rL, rL); i.addEllipse(QPointF(0, 0), rS, rS);
			QTransform t; t.translate(cx, cy); p->setPath(t.map(o.subtracted(i)));
		}
		if (hs.circumRect) hs.circumRect->setRect(cx - rL, cy - rL, rL * 2, rL * 2);
		hs.centerCrossH->setLine(cx - kCrossLen, cy, cx + kCrossLen, cy);
		hs.centerCrossV->setLine(cx, cy - kCrossLen, cx, cy + kCrossLen);
		break;
	}
	case Shape_Arc: {
		double cx = shape.cx, cy = shape.cy, rO = shape.r, rI = shape.r2;
		double sa = shape.startAngle, sp = shape.span, saR = qDegreesToRadians(sa), spR = qDegreesToRadians(sp);
		double eR = saR + spR, midA = sa + sp / 2, midR = qDegreesToRadians(midA);
		if (0 < hs.handles.size()) moveHandle(hs.handles[0], QPointF(cx + rO * qCos(saR), cy + rO * qSin(saR)));
		if (1 < hs.handles.size()) moveHandle(hs.handles[1], QPointF(cx + rO * qCos(eR), cy + rO * qSin(eR)));
		if (2 < hs.handles.size()) moveHandle(hs.handles[2], QPointF(cx + rO * qCos(midR), cy + rO * qSin(midR)));
		if (3 < hs.handles.size()) moveHandle(hs.handles[3], QPointF(cx + rI * qCos(midR), cy + rI * qSin(midR)));
		if (4 < hs.handles.size()) moveHandle(hs.handles[4], QPointF(cx + rI * qCos(saR), cy + rI * qSin(saR)));
		if (5 < hs.handles.size()) moveHandle(hs.handles[5], QPointF(cx + rI * qCos(eR), cy + rI * qSin(eR)));
		if (hs.rotateHandle) {
			double rRot = std::max(rO, rI);
			QPointF om(cx + rRot * qCos(midR), cy + rRot * qSin(midR));
			QPointF rp(cx + (rRot + 30) * qCos(midR), cy + (rRot + 30) * qSin(midR));
			moveHandle(hs.rotateHandle, rp); if (hs.rotateStickLine) hs.rotateStickLine->setLine(om.x(), om.y(), rp.x(), rp.y());
		}
		hs.centerCrossH->setLine(cx - kCrossLen, cy, cx + kCrossLen, cy);
		hs.centerCrossV->setLine(cx, cy - kCrossLen, cx, cy + kCrossLen);
		if (auto* p = dynamic_cast<QGraphicsPathItem*>(shapeItem)) {
			const int N = 128; QPainterPath ph;
			ph.moveTo(cx + rO * qCos(saR), cy + rO * qSin(saR));
			for (int i = 1; i <= N; ++i) { double a = saR + spR * i / N; ph.lineTo(cx + rO * qCos(a), cy + rO * qSin(a)); }
			double endR = saR + spR; ph.lineTo(cx + rI * qCos(endR), cy + rI * qSin(endR));
			for (int i = N; i >= 0; --i) { double a = saR + spR * i / N; ph.lineTo(cx + rI * qCos(a), cy + rI * qSin(a)); }
			ph.closeSubpath(); p->setPath(ph);
		}
		break;
	}
	case Shape_Polygon: {
		auto& pts = shape.pts;
		for (int i = 0; i < pts.size() && i < hs.handles.size(); ++i) moveHandle(hs.handles[i], pts[i]);
		if (auto* p = dynamic_cast<QGraphicsPolygonItem*>(shapeItem)) { QPolygonF poly; for (auto& pt : pts) poly << pt; p->setPolygon(poly); }
		if (hs.centerCrossH && hs.centerCrossV) {
			double cx = 0, cy = 0; for (auto& pt : pts) { cx += pt.x(); cy += pt.y(); }
			cx /= pts.size(); cy /= pts.size();
			hs.centerCrossH->setLine(cx - kCrossLen, cy, cx + kCrossLen, cy);
			hs.centerCrossV->setLine(cx, cy - kCrossLen, cx, cy + kCrossLen);
		}
		break;
	}
	case Shape_RotateRect: {
		double cx = shape.cx, cy = shape.cy, hw = shape.w / 2, hh = shape.h / 2;
		double rad = qDegreesToRadians(shape.angle);
		QPointF lP[8] = { {-hw,-hh},{0,-hh},{hw,-hh},{hw,0},{hw,hh},{0,hh},{-hw,hh},{-hw,0} };
		for (int i = 0; i < 8 && i < hs.handles.size(); ++i) {
			double rx = lP[i].x() * qCos(rad) - lP[i].y() * qSin(rad);
			double ry = lP[i].x() * qSin(rad) + lP[i].y() * qCos(rad);
			moveHandle(hs.handles[i], QPointF(rx + cx, ry + cy));
		}
		if (auto* p = dynamic_cast<QGraphicsPolygonItem*>(shapeItem)) {
			QPolygonF wp; QPointF pp[4] = { {-hw,-hh},{hw,-hh},{hw,hh},{-hw,hh} };
			for (auto& pt : pp) { double rx = pt.x() * qCos(rad) - pt.y() * qSin(rad); double ry = pt.x() * qSin(rad) + pt.y() * qCos(rad); wp << QPointF(rx + cx, ry + cy); }
			p->setPolygon(wp);
		}
		hs.centerCrossH->setLine(cx - kCrossLen, cy, cx + kCrossLen, cy);
		hs.centerCrossV->setLine(cx, cy - kCrossLen, cx, cy + kCrossLen);
		if (hs.rotateHandle) {
			double mx = -(-hh) * qSin(rad), my = (-hh) * qCos(rad);
			QPointF mt(mx + cx, my + cy), rp(mx + cx + qSin(rad) * 30, my + cy - qCos(rad) * 30);
			moveHandle(hs.rotateHandle, rp); if (hs.rotateStickLine) hs.rotateStickLine->setLine(mt.x(), mt.y(), rp.x(), rp.y());
		}
		break;
	}
	case Shape_Ellipse: {
		double cx = shape.cx, cy = shape.cy, r1 = shape.r, r2 = shape.r2;
		double rad = qDegreesToRadians(shape.angle);
		QPointF lP[4] = { {r1,0},{0,-r2},{-r1,0},{0,r2} };
		for (int i = 0; i < 4 && i < hs.handles.size(); ++i) {
			double rx = lP[i].x() * qCos(rad) - lP[i].y() * qSin(rad);
			double ry = lP[i].x() * qSin(rad) + lP[i].y() * qCos(rad);
			moveHandle(hs.handles[i], QPointF(rx + cx, ry + cy));
		}
		if (auto* p = dynamic_cast<QGraphicsPathItem*>(shapeItem)) {
			QPainterPath path; path.addEllipse(QPointF(0, 0), r1, r2);
			QTransform t; t.translate(cx, cy); t.rotate(shape.angle); p->setPath(t.map(path));
		}
		if (hs.circumRect) {
			double hw = std::sqrt(r1*r1 * qCos(rad)*qCos(rad) + r2*r2 * qSin(rad)*qSin(rad));
			double hh = std::sqrt(r1*r1 * qSin(rad)*qSin(rad) + r2*r2 * qCos(rad)*qCos(rad));
			hs.circumRect->setRect(cx - hw, cy - hh, hw * 2, hh * 2);
		}
		hs.centerCrossH->setLine(cx - kCrossLen, cy, cx + kCrossLen, cy);
		hs.centerCrossV->setLine(cx, cy - kCrossLen, cx, cy + kCrossLen);
		if (hs.rotateHandle) {
			double mx = -(-r2) * qSin(rad), my = (-r2) * qCos(rad);
			QPointF mt(mx + cx, my + cy), rp(mx + cx + qSin(rad) * 30, my + cy - qCos(rad) * 30);
			moveHandle(hs.rotateHandle, rp); if (hs.rotateStickLine) hs.rotateStickLine->setLine(mt.x(), mt.y(), rp.x(), rp.y());
		}
		break;
	}
	default: break;
	}
}
