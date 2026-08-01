#pragma once

#include <QList>
#include <QPointF>
#include <QGraphicsEllipseItem>
#include "DrawShapeData.h"

class QGraphicsItem;
class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsScene;

static const double kHandleRadius = 4.0;
static const double kCrossLen = 6.0;

inline void moveHandle(QGraphicsEllipseItem* h, const QPointF& center) {
	h->setRect(center.x() - kHandleRadius, center.y() - kHandleRadius,
		kHandleRadius * 2, kHandleRadius * 2);
}

struct ShapeHandleSet {
	QList<QGraphicsEllipseItem*> handles;
	QGraphicsEllipseItem* rotateHandle = nullptr;
	QGraphicsLineItem* centerCrossH = nullptr;
	QGraphicsLineItem* centerCrossV = nullptr;
	QGraphicsLineItem* rotateStickLine = nullptr;
	QGraphicsRectItem* circumRect = nullptr;
};

class ShapeHandleHelper {
public:
	explicit ShapeHandleHelper(QGraphicsScene* scene) : m_scene(scene) {}

	void rebuildHandles(const DrawShapeItem& shape, ShapeHandleSet& hs);
	void updatePositions(const DrawShapeItem& shape, ShapeHandleSet& hs, QGraphicsItem* shapeItem);
	void clearHandles(ShapeHandleSet& hs);

	static void applyHover(QGraphicsEllipseItem* h, bool hover);
	void clearAllHover(const ShapeHandleSet& hs);
	void setHoverAt(const ShapeHandleSet& hs, int index, bool hover);

	QGraphicsEllipseItem* handleAt(const ShapeHandleSet& hs, const QPointF& scenePos) const;
	void setHandlesVisible(ShapeHandleSet& hs, bool visible);

private:
	QGraphicsScene* m_scene;
};
