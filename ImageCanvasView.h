#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ImageCanvasView.h"
#include "DrawShapeData.h"

class QGraphicsEllipseItem;
class QGraphicsItem;
class QGraphicsLineItem;
class QGraphicsPathItem;
class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QGraphicsScene;
class QKeyEvent;
class QPushButton;

class ParamPanelWidget;
struct ParamField;
class ShapePainter;
class ShapeHandleHelper;
struct ShapeHandleSet;

class ImageCanvasView : public QMainWindow
{
	Q_OBJECT

public:
	ImageCanvasView(QWidget* parent = nullptr);
	~ImageCanvasView() override;

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
	void slotToggleControlPoints();
	void slotLoadImage();
	void slot_draw_shape_changed(int index);
	void slotResetShape();
	void slotOpenParamPanel();

private:
	Ui::ImageCanvasViewClass ui;

	QGraphicsScene* m_scene = nullptr;
	QGraphicsPixmapItem* m_pixmapItem = nullptr;
	QGraphicsRectItem* m_bgItem = nullptr;
	QGraphicsLineItem* m_spotAbsorber = nullptr;  // OpenGL QPen 残影吸收线
	double m_scaleValue = 1.0;

	bool m_showCenterCross = false;
	bool m_showControlPoints = true;
	QGraphicsLineItem* m_crossH = nullptr;
	QGraphicsLineItem* m_crossV = nullptr;

	QWidget* m_drawModeOverlay = nullptr;
	QPushButton* m_btnCancelDraw = nullptr;

	class ParamPanelWidget* m_paramPanel = nullptr;

	QLabel* m_infoLabel = nullptr;

	// 分离出的辅助类
	ShapePainter* m_painter = nullptr;
	ShapeHandleHelper* m_handleHelper = nullptr;

	QList<DrawShapeItem*> m_shapes;
	DrawShapeType m_currentShape = Shape_Rect;
	DrawShapeItem* m_activeShape = nullptr;  // 当前选中/显示的形状（纯数据）
	int m_activeShapeIndex = -1;

	// 当前活跃形状的渲染句柄（独立管理）
	QGraphicsItem* m_shapeItem = nullptr;
	ShapeHandleSet* m_activeHandleSet = nullptr;

	// 样式
	QColor m_colorSelected = QColor(0, 120, 255);
	double m_penWidth = 2.0;
	bool m_isShapeHovered = false;
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
	QGraphicsEllipseItem* m_circleMarker1 = nullptr;
	QGraphicsEllipseItem* m_circleMarker2 = nullptr;
	QGraphicsEllipseItem* m_ghostEllipse = nullptr;
	QGraphicsEllipseItem* m_ghostEllipse2 = nullptr;
	QGraphicsPathItem*   m_ghostArcPath = nullptr;
	QGraphicsRectItem* m_ghostCircumRect = nullptr;

	// Arc 绘制暂存
	double m_arcStartAngle = 0;
	double m_arcEndAngle = 0;

	// 多边形绘制暂存
	QList<QPointF> m_tempPolyPts;
	QList<QGraphicsEllipseItem*> m_circleMarkers;
	QGraphicsLineItem* m_ghostPolyLine = nullptr;

	// ---- 方法 ----
	void clearSceneShape();
	void rebuildShapeOnScene(DrawShapeItem* shape);

	void showDrawModeOverlay();
	void hideDrawModeOverlay();
	void updateScaleUI();
	void updateCenterCross();
	void updateInfoLabel(const QPointF& scenePos, const QPoint& globalPos);

	void showParamPanel();
	void hideParamPanel();
	void applyParamAndRedraw();
	void syncParamPanel(DrawShapeItem* shape);
	QList<struct ParamField> buildParamFields(DrawShapeType type) const;

	void startDraw(DrawShapeType type);
	void stopDraw();
	void updateGhostRect(const QPointF& scenePos);
	void commitShapeGeneric(DrawShapeType type);
	void commitRect();
	void commitRotatedRect();
	void commitCircle();
	void commitEllipse();
	void commitRing();
	void commitArc();
	void commitPolygon();

	DrawShapeItem* findShapeByType(DrawShapeType type);

	bool isPointInShape(const DrawShapeItem* shape, const QPointF& scenePos) const;
	QGraphicsEllipseItem* handleAt(const QPointF& scenePos) const;

	void updateRectFromHandle(const QPointF& scenePos);
	void updateRotatedRectFromHandle(const QPointF& scenePos);
	void updateCircleFromHandle(const QPointF& scenePos);
	void updateEllipseFromHandle(const QPointF& scenePos);
	void updateRingFromHandle(const QPointF& scenePos);
	void updateArcFromHandle(const QPointF& scenePos);
	void updatePolygonFromHandle(const QPointF& scenePos);
	void applyHandleHover(int handleIndex, bool hover);
	void applyShapeHover(bool hover);
	void clearAllHandleHover();
};
