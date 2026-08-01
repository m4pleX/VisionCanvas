/*
 * 文件名：ShapePainter.h
 * 职责：形状绘制与图形项构造
 * 核心功能：
 *   - 根据 DrawShapeItem 参数生成 QGraphicsItem（矩形/圆/椭圆/圆弧等）
 *   - 统一画笔、画刷样式
 *   - 不处理交互，只负责"画出来"
 * 依赖：m_scene, DrawShapeItem
 * 注意：纯工具类，不持有临时拖拽状态
 */
#pragma once

#include <QColor>
#include "DrawShapeData.h"

class QGraphicsItem;
class QGraphicsScene;

/*  形状绘制器：根据参数构造 QGraphicsItem，统一画笔样式 */
/*  不持有交互状态，不处理鼠标事件 */
class ShapePainter {
public:
	explicit ShapePainter(QGraphicsScene* scene) : m_scene(scene) {}

	QGraphicsItem* buildItem(const DrawShapeItem& shape);
	void applyStyle(QGraphicsItem* item, const QColor& color, double penWidth, bool isHovered);

private:
	QGraphicsScene* m_scene;
};
