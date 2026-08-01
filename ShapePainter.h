#pragma once

#include <QColor>
#include "DrawShapeData.h"

class QGraphicsItem;
class QGraphicsScene;

class ShapePainter {
public:
	explicit ShapePainter(QGraphicsScene* scene) : m_scene(scene) {}

	QGraphicsItem* buildItem(const DrawShapeItem& shape);
	void applyStyle(QGraphicsItem* item, const QColor& color, double penWidth, bool isHovered);

private:
	QGraphicsScene* m_scene;
};
