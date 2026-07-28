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

// 控制点 hover
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

struct DrawShapeItem
{
	DrawShapeType type;
	QGraphicsItem* item = nullptr;
	QList<QGraphicsEllipseItem*> handles;
	QGraphicsEllipseItem* rotateHandle = nullptr;
	QGraphicsLineItem* centerCrossH = nullptr;
	QGraphicsLineItem* centerCrossV = nullptr;
	QGraphicsLineItem* rotateStickLine = nullptr;
	QGraphicsRectItem* circumRect = nullptr;       // 圆形/椭圆的外接矩形

	struct Rect { double cx = 0, cy = 0, w = 0, h = 0; } rect;
	struct RotatedRect { double cx = 0, cy = 0, w = 0, h = 0, angle = 0; } rotatedRect;
	struct Circle { double cx = 0, cy = 0, r = 0; } circle;
	struct Ellipse { double cx = 0, cy = 0, r1 = 0, r2 = 0, angle = 0; } ellipse;
	struct Ring { double cx = 0, cy = 0, rOuter = 0, rInner = 0; } ring;
	struct Arc { double cx = 0, cy = 0, rOuter = 0, rInner = 0, startAngle = 0, endAngle = 0; } arc;
	struct Polygon { QList<QPointF> pts; } polygon;

	explicit DrawShapeItem(DrawShapeType t) : type(t) {}
};


class ImageCanvasView : public QMainWindow
{
	Q_OBJECT

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
	QVBoxLayout* m_paramContentLayout = nullptr;
	QList<QDoubleSpinBox*> m_paramSpins;
	QList<QLabel*> m_paramLabels;

	QLabel* m_infoLabel = nullptr;

	QList<DrawShapeItem*> m_shapes;
	DrawShapeType m_currentShape = Shape_Rect;
	DrawShapeItem* m_activeShape = nullptr;  // 当前选中/显示的形状（scene 上唯一）
	int m_activeShapeIndex = -1;

	// 样式
	QColor m_colorNormal   = QColor(0, 200, 0);
	QColor m_colorSelected = QColor(0, 120, 255);
	double m_penWidth = 2.0;
	bool m_thinLine = false;

	enum InteractionMode { Mode_View, Mode_Draw, Mode_None };
	InteractionMode m_mode = Mode_None;

	int m_drawStep = 0;
	QPointF m_anchorPoint;
	QGraphicsRectItem* m_ghostRect = nullptr;

	bool m_isDraggingHandle = false;
	bool m_isRotating = false;
	DrawShapeItem* m_dragShape = nullptr;
	int m_dragHandleIndex = -1;
	double m_dragStartAngle = 0;
	DrawShapeItem::Rect m_dragStartRect;

	bool m_isDraggingShape = false;
	QPointF m_dragOffset;
	QPointF m_dragStartCenter;

	// 圆形三点绘制的中间点
	QPointF m_circlePt1;
	QPointF m_circlePt2;
	QGraphicsEllipseItem* m_circleMarker1 = nullptr;  // 点1标记
	QGraphicsEllipseItem* m_circleMarker2 = nullptr;  // 点2标记
	QGraphicsEllipseItem* m_ghostEllipse = nullptr;  // 圆形/椭圆的 ghost
	QGraphicsRectItem* m_ghostCircumRect = nullptr;   // 外接矩形 ghost

	// ---- 方法 ----
	void clearSceneShape();                        // 清除 scene 上当前形状的所有 item
	void rebuildShapeOnScene(DrawShapeItem* shape); // 根据数据重绘形状到 scene
	void applyStyle(DrawShapeItem* shape);          // 给 shape->item 上色

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
	void commitShapeGeneric(DrawShapeType type);
	void commitRect();
	void commitRotatedRect();
	void commitCircle();

	DrawShapeItem* findShapeByType(DrawShapeType type);
	QGraphicsItem* buildShapeItem(const DrawShapeItem& shape);

	void showHandles(DrawShapeItem* shape);
	void updateHandlePositions(DrawShapeItem* shape);

	bool isPointInShape(const DrawShapeItem* shape, const QPointF& scenePos) const;
	QGraphicsEllipseItem* handleAt(const QPointF& scenePos) const;

	void updateRectFromHandle(const QPointF& scenePos);
	void updateRotatedRectFromHandle(const QPointF& scenePos);
	void updateCircleFromHandle(const QPointF& scenePos);
	void applyHandleHover(int handleIndex, bool hover);
	void clearAllHandleHover();
};
