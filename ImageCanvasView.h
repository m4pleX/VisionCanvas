#pragma once

#include <QtWidgets/QMainWindow>
#include <QKeyEvent>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include "ui_ImageCanvasView.h"

class QDoubleSpinBox;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QLabel;
class QPushButton;

// 控制点 & 中心十字 常量
static const double kHandleRadius = 4.0;
static const double kCrossLen = 6.0;

// 控制点 hover（用 friend 或全局内联）
inline void setHandleHover(QGraphicsEllipseItem* h, bool hover) {
	QPointF c = h->rect().center();
	double r = hover ? kHandleRadius + 2 : kHandleRadius;
	h->setRect(c.x() - r, c.y() - r, r * 2, r * 2);
	h->setPen(hover ? QPen(QColor(255, 200, 0), 2) : QPen(Qt::white, 1));
	h->setBrush(hover ? QColor(255, 200, 0) : QColor(0, 180, 255));
}

inline void moveHandle(QGraphicsEllipseItem* h, const QPointF& center) {
	h->setRect(center.x() - kHandleRadius, center.y() - kHandleRadius,
	           kHandleRadius * 2, kHandleRadius * 2);
}

class ImageCanvasView : public QMainWindow
{
	Q_OBJECT

public:
	enum DrawShapeType
	{
		Shape_Rect,
		Shape_RotateRect,
		Shape_Circle,
		Shape_Ellipse,
		Shape_Ring,
		Shape_Arc,
		Shape_Polygon
	};

	enum ShapeState
	{
		ShapeState_Normal,
		ShapeState_Hover,
		ShapeState_Selected,
		ShapeState_Hidden
	};

	struct DrawShapeItem
	{
		DrawShapeType type;
		QGraphicsItem* item = nullptr;
		ShapeState state = ShapeState_Normal;
		QList<QGraphicsEllipseItem*> handles;
		QGraphicsEllipseItem* rotateHandle = nullptr;
		QGraphicsLineItem* centerCrossH = nullptr;
		QGraphicsLineItem* centerCrossV = nullptr;

		struct Rect { double cx = 0, cy = 0, w = 0, h = 0; } rect;
		struct RotatedRect { double cx = 0, cy = 0, w = 0, h = 0, angle = 0; } rotatedRect;
		struct Circle { double cx = 0, cy = 0, r = 0; } circle;
		struct Ellipse { double cx = 0, cy = 0, r1 = 0, r2 = 0, angle = 0; } ellipse;
		struct Ring { double cx = 0, cy = 0, rOuter = 0, rInner = 0; } ring;
		struct Arc { double cx = 0, cy = 0, rOuter = 0, rInner = 0, startAngle = 0, endAngle = 0; } arc;
		struct Polygon { QList<QPointF> pts; } polygon;

		explicit DrawShapeItem(DrawShapeType t) : type(t) {}
	};

public:
	ImageCanvasView(QWidget* parent = nullptr);
	~ImageCanvasView() override = default;

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private slots:
	void slotZoomIn();
	void slotZoomOut();
	void slotZoomReset();
	void slotZoomFit();
	void slotToggleCenterCross();
	void slotToggleLineWidth();
	void slotLoadImage();
	void slot_draw_shape_changed(int index);
	void slotResetShape();
	void slotOpenParamPanel();

private:
	Ui::ImageCanvasViewClass ui;

	QGraphicsScene* m_scene = nullptr;
	QGraphicsPixmapItem* m_pixmapItem = nullptr;
	QGraphicsRectItem* m_bgItem = nullptr;
	double m_scaleValue = 1.0;

	bool m_showCenterCross = false;
	QGraphicsLineItem* m_crossH = nullptr;
	QGraphicsLineItem* m_crossV = nullptr;

	QWidget* m_drawModeOverlay = nullptr;
	QPushButton* m_btnCancelDraw = nullptr;

	QWidget* m_paramPanel = nullptr;
	QDoubleSpinBox* m_paramSpin[4] = {};
	QLabel* m_paramLabel[4] = {};

	QLabel* m_infoLabel = nullptr;

	QList<DrawShapeItem*> m_shapes;
	DrawShapeType m_currentShape = Shape_Rect;
	DrawShapeItem* m_selectedShape = nullptr;
	DrawShapeItem* m_hoverShape = nullptr;
	int m_activeShapeIndex = -1;

	// 样式
	QColor m_colorNormal   = QColor(0, 200, 0);    // 绿色
	QColor m_colorHover    = QColor(0, 120, 255);  // 蓝色
	QColor m_colorSelected = QColor(0, 120, 255);  // 蓝色
	double m_penWidth = 2.0;
	bool m_thinLine = false;

	enum InteractionMode { Mode_View, Mode_Draw, Mode_None };
	InteractionMode m_mode = Mode_None;

	int m_drawStep = 0;
	QPointF m_anchorPoint;
	QGraphicsRectItem* m_ghostRect = nullptr;

	bool m_isDraggingHandle = false;
	DrawShapeItem* m_dragShape = nullptr;
	int m_dragHandleIndex = -1;
	DrawShapeItem::Rect m_dragStartRect;

	bool m_isDraggingShape = false;
	QPointF m_dragOffset;
	QPointF m_dragStartCenter;

	// 方法
	void showDrawModeOverlay();
	void hideDrawModeOverlay();
	void updateScaleUI();
	void updateCenterCross();
	void updateInfoLabel(const QPointF& scenePos, const QPoint& globalPos);

	void showParamPanel();
	void hideParamPanel();
	void applyParamAndRedraw();

	void startDraw(DrawShapeType type);
	void stopDraw();
	void updateGhostRect(const QPointF& scenePos);
	void commitRect();
	void commitShapeGeneric(DrawShapeType type);

	DrawShapeItem* findShapeByType(DrawShapeType type);
	void setAllShapesVisible(bool visible);
	void showOnlyShape(DrawShapeType type);

	QGraphicsItem* buildShapeItem(const DrawShapeItem& shape);
	void applyShapeStateStyle(DrawShapeItem* shape);

	void showHandles(DrawShapeItem* shape);
	void updateHandlePositions(DrawShapeItem* shape);
	void hideAllHandles();

	void deselectAll();
	bool isPointInShape(const DrawShapeItem* shape, const QPointF& scenePos) const;
	DrawShapeItem* shapeAt(const QPointF& scenePos) const;
	QGraphicsEllipseItem* handleAt(const QPointF& scenePos) const;
	void setShapeState(DrawShapeItem* shape, ShapeState state);

	void updateRectFromHandle(const QPointF& scenePos);
	void applyHandleHover(int handleIndex, bool hover);
	void clearAllHandleHover();
};
