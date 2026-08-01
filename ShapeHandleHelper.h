/*
 * 文件名：ShapeHandleHelper.h
 * 职责：控制点/旋转手柄/中心十字的创建与更新
 * 核心功能：
 *   - rebuildHandles：根据形状数据创建全部辅助图形项
 *   - updatePositions：拖拽/修改后仅更新已有 handle 位置和形状体
 *   - clearHandles：统一清理所有辅助项（removeItem + delete）
 *   - hover 和可见性管理
 * 依赖：m_scene, DrawShapeItem, ShapeHandleSet
 * 注意：不直接操作 DrawShapeItem 数据，只管理渲染
 */
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

// 控制点管理：创建、拖拽、hover、可见性
// 与 ShapePainter 配对使用，只负责"操作手柄"
class ShapeHandleHelper {
public:
	explicit ShapeHandleHelper(QGraphicsScene* scene) : m_scene(scene) {}

	void rebuildHandles(const DrawShapeItem& shape, ShapeHandleSet& hs);
	void updatePositions(const DrawShapeItem& shape, ShapeHandleSet& hs, QGraphicsItem* shapeItem);
	void clearHandles(ShapeHandleSet& hs);

	QGraphicsEllipseItem* handleAt(const ShapeHandleSet& hs, const QPointF& scenePos) const;
	void setHandlesVisible(ShapeHandleSet& hs, bool visible);

private:
	QGraphicsScene* m_scene;
};
