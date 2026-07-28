#include "ImageCanvasView.h"

#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QtMath>

// ===== 构造/析构 =====

ImageCanvasView::ImageCanvasView(QWidget* parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	m_scene = new QGraphicsScene(this);
	ui.canvas_view_main->setScene(m_scene);
	ui.canvas_view_main->setMouseTracking(true);
	ui.canvas_view_main->setDragMode(QGraphicsView::NoDrag);

	m_pixmapItem = new QGraphicsPixmapItem();
	m_scene->addItem(m_pixmapItem);

	m_bgItem = new QGraphicsRectItem;
	m_bgItem->setZValue(-1);
	m_bgItem->setPen(Qt::NoPen);

	QImage texImg(8, 8, QImage::Format_RGB32);
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++)
			texImg.setPixel(x, y, ((x / 4) + (y / 4)) % 2 == 0 ? qRgb(100, 100, 100) : qRgb(60, 60, 60));

	QBrush checkerBrush;
	checkerBrush.setStyle(Qt::TexturePattern);
	checkerBrush.setTextureImage(texImg);
	m_bgItem->setBrush(checkerBrush);

	const qreal INF = 1000000;
	m_bgItem->setRect(-INF, -INF, INF * 2, INF * 2);
	m_scene->addItem(m_bgItem);

	m_crossH = new QGraphicsLineItem();
	m_crossV = new QGraphicsLineItem();
	QPen pen(QColor(0, 120, 255));
	pen.setWidth(1);
	pen.setCosmetic(true);
	m_crossH->setPen(pen);
	m_crossV->setPen(pen);
	m_crossH->setZValue(99);
	m_crossV->setZValue(99);
	m_crossH->setVisible(false);
	m_crossV->setVisible(false);
	m_scene->addItem(m_crossH);
	m_scene->addItem(m_crossV);

	QPixmap defaultPixmap(1280, 960);
	defaultPixmap.fill(Qt::white);
	m_pixmapItem->setPixmap(defaultPixmap);

	ui.canvas_view_main->viewport()->installEventFilter(this);
	ui.canvas_view_main->viewport()->setMouseTracking(true);

	connect(ui.tool_btn_zoom_in, &QPushButton::clicked, this, &ImageCanvasView::slotZoomIn);
	connect(ui.tool_btn_zoom_out, &QPushButton::clicked, this, &ImageCanvasView::slotZoomOut);
	connect(ui.tool_btn_zoom_reset, &QPushButton::clicked, this, &ImageCanvasView::slotZoomReset);
	connect(ui.tool_btn_zoom_fit, &QPushButton::clicked, this, &ImageCanvasView::slotZoomFit);
	connect(ui.tool_btn_toggle_cross, &QPushButton::clicked, this, &ImageCanvasView::slotToggleCenterCross);
	connect(ui.tool_btn_toggle_line_width, &QPushButton::clicked, this, &ImageCanvasView::slotToggleLineWidth);

	connect(ui.draw_btn_load_image, &QPushButton::clicked, this, &ImageCanvasView::slotLoadImage);
	connect(ui.draw_cbox_shape_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &ImageCanvasView::slot_draw_shape_changed);
	connect(ui.btn_reset_rect, &QPushButton::clicked, this, &ImageCanvasView::slotResetShape);
	connect(ui.draw_btn_open_param, &QPushButton::clicked, this, &ImageCanvasView::slotOpenParamPanel);

	ui.info_lab_scale_ratio->setText("100%");
	ui.info_lab_resolution->setText("(1280, 960)");

	m_infoLabel = new QLabel(this);
	m_infoLabel->setStyleSheet("background: white; color: black; padding: 2px 4px; border:1px solid #999;");
	m_infoLabel->setVisible(false);
}

// ===== 事件处理 =====

bool ImageCanvasView::eventFilter(QObject* obj, QEvent* event)
{
	if (obj == m_drawModeOverlay)
	{
		if (event->type() == QEvent::MouseButtonPress ||
			event->type() == QEvent::MouseButtonRelease ||
			event->type() == QEvent::MouseMove ||
			event->type() == QEvent::Wheel)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			if (m_btnCancelDraw && m_btnCancelDraw->geometry().contains(me->pos()))
				return false;
			event->accept();
			return true;
		}
		return false;
	}

	if (obj == ui.canvas_view_main->viewport())
	{
		if (event->type() == QEvent::Wheel)
		{
			QWheelEvent* wheel = static_cast<QWheelEvent*>(event);
			QPointF oldPos = ui.canvas_view_main->mapToScene(wheel->pos());
			m_scaleValue *= (wheel->delta() > 0 ? 1.1 : 0.9);
			m_scaleValue = qBound(0.2, m_scaleValue, 5.0);

			QTransform trans;
			trans.scale(m_scaleValue, m_scaleValue);
			ui.canvas_view_main->setTransform(trans);

			QPointF newPos = ui.canvas_view_main->mapToScene(wheel->pos());
			QPointF delta = newPos - oldPos;
			ui.canvas_view_main->horizontalScrollBar()->setValue(
				ui.canvas_view_main->horizontalScrollBar()->value() + qRound(delta.x()));
			ui.canvas_view_main->verticalScrollBar()->setValue(
				ui.canvas_view_main->verticalScrollBar()->value() + qRound(delta.y()));
			ui.info_lab_scale_ratio->setText(QString("%1%").arg(qRound(m_scaleValue * 100)));
			event->accept();
			return true;
		}

		if (event->type() != QEvent::MouseButtonPress &&
			event->type() != QEvent::MouseButtonRelease &&
			event->type() != QEvent::MouseMove &&
			event->type() != QEvent::Leave)
		{
			return QMainWindow::eventFilter(obj, event);
		}

		QPointF scenePos;
		if (event->type() != QEvent::Leave)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			scenePos = ui.canvas_view_main->mapToScene(me->pos());
		}

		// 坐标/RGB 显示（所有模式统一）
		if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			updateInfoLabel(scenePos, me->globalPos());
		}
		else if (event->type() == QEvent::Leave)
		{
			m_infoLabel->setVisible(false);
		}

		// ===== 绘制模式 =====
		if (m_mode == Mode_Draw)
		{
			if (m_currentShape == Shape_Rect)
			{
				if (event->type() == QEvent::MouseButtonPress)
				{
					QMouseEvent* me = static_cast<QMouseEvent*>(event);
					if (me->button() == Qt::LeftButton)
					{
						if (m_drawStep == 0)
						{
							m_anchorPoint = scenePos;
							m_drawStep = 1;
							m_ghostRect = new QGraphicsRectItem();
							QPen ghostPen(QColor(0, 180, 255), m_penWidth, Qt::DashLine);
							ghostPen.setCosmetic(true);
							m_ghostRect->setPen(ghostPen);
							m_ghostRect->setBrush(QColor(0, 180, 255, 30));
							m_ghostRect->setZValue(99);
							m_ghostRect->setRect(QRectF(m_anchorPoint, QSizeF(0, 0)));
							m_scene->addItem(m_ghostRect);
						}
						else if (m_drawStep == 1)
						{
							commitRect();
							if (m_ghostRect) { m_scene->removeItem(m_ghostRect); delete m_ghostRect; m_ghostRect = nullptr; }
							hideDrawModeOverlay();
							m_mode = Mode_None;
							m_drawStep = 0;
							ui.canvas_view_main->setCursor(Qt::ArrowCursor);
							for (auto* s : m_shapes) { if (s->item) s->item->setVisible(true); }
						}
						event->accept();
						return true;
					}
					else if (me->button() == Qt::RightButton)
					{
						stopDraw();
						event->accept();
						return true;
					}
				}
				else if (event->type() == QEvent::MouseMove && (m_drawStep == 0 || m_drawStep == 1))
				{
					if (m_drawStep == 1)
						updateGhostRect(scenePos);
					event->accept();
					return true;
				}
			}
			return false;
		}

		// ===== 非绘制模式 =====
		static QPoint lastViewPos;
		static bool isPanning = false;

		// Handle / 形状拖拽结束
		if (event->type() == QEvent::MouseButtonRelease)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::LeftButton)
			{
				if (m_isDraggingHandle)
				{
					m_isDraggingHandle = false;
					m_dragShape = nullptr;
					m_dragHandleIndex = -1;
					clearAllHandleHover();
					ui.canvas_view_main->setCursor(Qt::CrossCursor);
					event->accept();
					return true;
				}
				if (m_isDraggingShape)
				{
					m_isDraggingShape = false;
					ui.canvas_view_main->setCursor(Qt::ArrowCursor);
					event->accept();
					return true;
				}
			}
		}

		// Handle 拖拽中
		if (m_isDraggingHandle && event->type() == QEvent::MouseMove)
		{
			updateRectFromHandle(scenePos);
			event->accept();
			return true;
		}

		// 形状拖动中
		if (m_isDraggingShape && event->type() == QEvent::MouseMove)
		{
			if (m_selectedShape)
			{
				m_selectedShape->rect.cx = scenePos.x() - m_dragOffset.x();
				m_selectedShape->rect.cy = scenePos.y() - m_dragOffset.y();
				updateHandlePositions(m_selectedShape);
			}
			event->accept();
			return true;
		}

		if (event->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::LeftButton)
			{
				// 优先检测 handle
				QGraphicsEllipseItem* hitHandle = handleAt(scenePos);
				if (hitHandle && m_selectedShape)
				{
					int hIdx = hitHandle->data(0).toInt();
					if (hIdx >= 0 && hIdx < m_selectedShape->handles.size())
					{
						m_isDraggingHandle = true;
						m_dragShape = m_selectedShape;
						m_dragHandleIndex = hIdx;
						m_dragStartRect = m_selectedShape->rect;
						clearAllHandleHover();
						applyHandleHover(hIdx, true);
						ui.canvas_view_main->setCursor(Qt::ClosedHandCursor);
						event->accept();
						return true;
					}
				}

				// 检测形状内部
				DrawShapeItem* hitShape = shapeAt(scenePos);
				if (hitShape && hitShape->state != ShapeState_Hidden)
				{
					if (hitShape->state == ShapeState_Selected)
					{
						// 点击已选中 → 拖动
						m_isDraggingShape = true;
						m_dragOffset = QPointF(scenePos.x() - hitShape->rect.cx, scenePos.y() - hitShape->rect.cy);
					}
					else
					{
						// 选中形状
						hitShape->state = ShapeState_Selected;
						m_selectedShape = hitShape;
						showHandles(hitShape);
						applyShapeStateStyle(hitShape);
					}
					event->accept();
					return true;
				}
				else
				{
					// 空白区域 → 取消选中
					deselectAll();
					lastViewPos = me->pos();
					isPanning = true;
					ui.canvas_view_main->setCursor(Qt::ClosedHandCursor);
					return true;
				}
			}
		}
		else if (event->type() == QEvent::MouseMove)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			if (isPanning && (me->buttons() & Qt::LeftButton))
			{
				QPoint delta = me->pos() - lastViewPos;
				lastViewPos = me->pos();
				ui.canvas_view_main->horizontalScrollBar()->setValue(ui.canvas_view_main->horizontalScrollBar()->value() - delta.x());
				ui.canvas_view_main->verticalScrollBar()->setValue(ui.canvas_view_main->verticalScrollBar()->value() - delta.y());
			}
			else if (!isPanning)
			{
				// Handle hover
				QGraphicsEllipseItem* hoverHandle = handleAt(scenePos);
				if (hoverHandle && m_selectedShape)
				{
					int hIdx = hoverHandle->data(0).toInt();
					static int lastHoverHandle = -1;
					if (lastHoverHandle != hIdx)
					{
						applyHandleHover(lastHoverHandle, false);
						applyHandleHover(hIdx, true);
						lastHoverHandle = hIdx;
					}
					if (m_hoverShape && m_hoverShape->state == ShapeState_Hover)
					{
						setShapeState(m_hoverShape, ShapeState_Normal);
						m_hoverShape = nullptr;
					}
					ui.canvas_view_main->setCursor(Qt::SizeAllCursor);
				}
				else
				{
					clearAllHandleHover();
					static int lastHoverHandle = -1;
					lastHoverHandle = -1;

					// 形状 hover
					DrawShapeItem* hovered = shapeAt(scenePos);
					if (hovered && hovered->state != ShapeState_Hidden && hovered->state != ShapeState_Selected)
					{
						if (m_hoverShape != hovered)
						{
							if (m_hoverShape && m_hoverShape->state == ShapeState_Hover)
								setShapeState(m_hoverShape, ShapeState_Normal);
							m_hoverShape = hovered;
							setShapeState(hovered, ShapeState_Hover);
							ui.canvas_view_main->setCursor(Qt::PointingHandCursor);
						}
					}
					else
					{
						if (m_hoverShape && m_hoverShape->state == ShapeState_Hover)
						{
							setShapeState(m_hoverShape, ShapeState_Normal);
							m_hoverShape = nullptr;
							ui.canvas_view_main->setCursor(Qt::ArrowCursor);
						}
					}
				}
			}
		}
		else if (event->type() == QEvent::MouseButtonRelease)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::LeftButton && isPanning)
			{
				isPanning = false;
				ui.canvas_view_main->setCursor(Qt::ArrowCursor);
				return true;
			}
		}
	}

	return QMainWindow::eventFilter(obj, event);
}

// ===== 键盘 =====

void ImageCanvasView::keyPressEvent(QKeyEvent* event)
{
	if (m_mode == Mode_Draw && event->key() == Qt::Key_Escape)
	{
		stopDraw();
		ui.draw_cbox_shape_type->blockSignals(true);
		ui.draw_cbox_shape_type->setCurrentIndex(0);
		ui.draw_cbox_shape_type->blockSignals(false);
		return;
	}

	if (event->key() == Qt::Key_Delete && m_selectedShape)
	{
		DrawShapeItem* s = m_selectedShape;
		m_shapes.removeOne(s);
		m_scene->removeItem(s->item);
		delete s->item;
		for (auto* h : s->handles) { m_scene->removeItem(h); delete h; }
		if (s->rotateHandle) { m_scene->removeItem(s->rotateHandle); delete s->rotateHandle; }
		if (s->centerCrossH) { m_scene->removeItem(s->centerCrossH); delete s->centerCrossH; }
		if (s->centerCrossV) { m_scene->removeItem(s->centerCrossV); delete s->centerCrossV; }
		m_selectedShape = nullptr;
		if (m_hoverShape == s) m_hoverShape = nullptr;
		delete s;
		return;
	}

	QMainWindow::keyPressEvent(event);
}

// ===== 缩放 =====

void ImageCanvasView::updateScaleUI()
{
	m_scaleValue = qBound(0.2, m_scaleValue, 5.0);
	ui.info_lab_scale_ratio->setText(QString("%1%").arg(qRound(m_scaleValue * 100)));
	QTransform t;
	t.scale(m_scaleValue, m_scaleValue);
	ui.canvas_view_main->setTransform(t);
}

void ImageCanvasView::slotZoomIn()   { m_scaleValue += 0.01; updateScaleUI(); }
void ImageCanvasView::slotZoomOut()  { m_scaleValue -= 0.01; updateScaleUI(); }
void ImageCanvasView::slotZoomReset(){ m_scaleValue = 1.0; updateScaleUI(); }

void ImageCanvasView::slotZoomFit()
{
	QPixmap pm = m_pixmapItem->pixmap();
	if (pm.isNull()) return;
	QRectF imageRect = pm.rect();
	ui.canvas_view_main->fitInView(imageRect, Qt::KeepAspectRatio);
	QTransform trans = ui.canvas_view_main->transform();
	m_scaleValue = trans.m11();
	m_scaleValue = qBound(0.2, m_scaleValue, 5.0);
	updateScaleUI();
}

// ===== 十字线 & 线宽 =====

void ImageCanvasView::slotToggleCenterCross()
{
	m_showCenterCross = !m_showCenterCross;
	m_crossH->setVisible(m_showCenterCross);
	m_crossV->setVisible(m_showCenterCross);
	updateCenterCross();
}

void ImageCanvasView::slotToggleLineWidth()
{
	m_thinLine = !m_thinLine;
	m_penWidth = m_thinLine ? 0.5 : 2.0;
	for (auto* s : m_shapes)
	{
		if (s->item && s->state != ShapeState_Hidden)
			applyShapeStateStyle(s);
	}
}

void ImageCanvasView::updateCenterCross()
{
	QPixmap pm = m_pixmapItem->pixmap();
	if (pm.isNull()) return;
	double w = pm.width(), h = pm.height();
	m_crossH->setLine(0, h / 2.0, w, h / 2.0);
	m_crossV->setLine(w / 2.0, 0, w / 2.0, h);
}

void ImageCanvasView::updateInfoLabel(const QPointF& scenePos, const QPoint& globalPos)
{
	double mouseX = scenePos.x(), mouseY = scenePos.y();
	QPixmap pm = m_pixmapItem->pixmap();
	bool inImage = !pm.isNull() && mouseX >= 0.0 && mouseX < pm.width()
		&& mouseY >= 0.0 && mouseY < pm.height();
	QString text;
	if (inImage)
	{
		QImage img = pm.toImage();
		int x = static_cast<int>(floor(mouseX)), y = static_cast<int>(floor(mouseY));
		QRgb rgb = img.pixel(x, y);
		int r = qRed(rgb), g = qGreen(rgb), b = qBlue(rgb);
		int grayVal = qRound(r * 0.299 + g * 0.587 + b * 0.114);
		text = QString("X:%1 Y:%2  R:%3 G:%4 B:%5  Gray:%6")
			.arg(mouseX, 0, 'f', 2).arg(mouseY, 0, 'f', 2).arg(r).arg(g).arg(b).arg(grayVal);
	}
	else { text = QString("X:%1 Y:%2").arg(mouseX, 0, 'f', 2).arg(mouseY, 0, 'f', 2); }
	m_infoLabel->setText(text);
	m_infoLabel->adjustSize();
	m_infoLabel->move(mapFromGlobal(globalPos) + QPoint(20, 5));
	m_infoLabel->setVisible(true);
}

// ===== 绘图面板 =====

void ImageCanvasView::slotLoadImage()
{
	QString filePath = QFileDialog::getOpenFileName(this,
		QStringLiteral("选择图片"), "", QStringLiteral("图片文件(*.png *.jpg *.bmp *.jpeg)"));
	if (filePath.isEmpty()) return;
	QImage loadImg(filePath);
	if (loadImg.isNull())
	{
		QMessageBox::warning(this, QStringLiteral("图片加载失败"), QStringLiteral("无法加载图片"));
		return;
	}
	QPixmap pixmap = QPixmap::fromImage(loadImg);
	m_pixmapItem->setPixmap(pixmap);
	m_scene->setSceneRect(pixmap.rect());
	ui.info_lab_resolution->setText(QString("(%1, %2)").arg(loadImg.width()).arg(loadImg.height()));
	m_scaleValue = 1.0;
	updateScaleUI();
	updateCenterCross();
}

// ===== 绘制模式 =====

void ImageCanvasView::slot_draw_shape_changed(int index)
{
	if (index <= 0)
	{
		if (m_mode == Mode_Draw) stopDraw();
		deselectAll();
		for (auto* s : m_shapes) setShapeState(s, ShapeState_Hidden);
		m_hoverShape = nullptr;
		m_activeShapeIndex = 0;
		ui.canvas_view_main->setCursor(Qt::ArrowCursor);
		return;
	}

	int shapeIdx = index - 1;
	DrawShapeType type = static_cast<DrawShapeType>(shapeIdx);
	m_activeShapeIndex = index;

	DrawShapeItem* existing = findShapeByType(type);
	if (existing)
	{
		if (m_mode == Mode_Draw) stopDraw();
		deselectAll();
		showOnlyShape(type);
		setShapeState(existing, ShapeState_Selected);
		ui.canvas_view_main->setCursor(Qt::ArrowCursor);
	}
	else
	{
		showOnlyShape(type);
		startDraw(type);
	}
}

void ImageCanvasView::slotResetShape()
{
	int index = ui.draw_cbox_shape_type->currentIndex();
	if (index <= 0) return;
	DrawShapeType type = static_cast<DrawShapeType>(index - 1);

	DrawShapeItem* shape = findShapeByType(type);
	if (!shape) return;

	if (shape == m_selectedShape) deselectAll();
	if (shape->item) { m_scene->removeItem(shape->item); delete shape->item; shape->item = nullptr; }
	for (auto* h : shape->handles) { m_scene->removeItem(h); delete h; }
	shape->handles.clear();
	if (shape->rotateHandle) { m_scene->removeItem(shape->rotateHandle); delete shape->rotateHandle; shape->rotateHandle = nullptr; }
	if (shape->centerCrossH) { m_scene->removeItem(shape->centerCrossH); delete shape->centerCrossH; shape->centerCrossH = nullptr; }
	if (shape->centerCrossV) { m_scene->removeItem(shape->centerCrossV); delete shape->centerCrossV; shape->centerCrossV = nullptr; }
	m_shapes.removeOne(shape);
	delete shape;

	showOnlyShape(type);
	startDraw(type);
}

void ImageCanvasView::showDrawModeOverlay()
{
	QWidget* parent = ui.group_draw_opt;
	if (!m_drawModeOverlay)
	{
		m_drawModeOverlay = new QWidget(parent);
		m_drawModeOverlay->setObjectName("drawModeOverlay");
		m_drawModeOverlay->setStyleSheet(
			"QWidget#drawModeOverlay { background-color: rgba(110, 110, 110, 180); border-radius: 6px; }");
		m_drawModeOverlay->installEventFilter(this);

		QVBoxLayout* outerLayout = new QVBoxLayout(m_drawModeOverlay);
		outerLayout->setAlignment(Qt::AlignCenter);

		QLabel* hintLabel = new QLabel(QStringLiteral("绘制模式中"), m_drawModeOverlay);
		hintLabel->setStyleSheet("color: white; font-size: 14px; font-weight: bold; background: transparent; border: none;");
		hintLabel->setAlignment(Qt::AlignCenter);

		m_btnCancelDraw = new QPushButton(QStringLiteral("取消绘制"), m_drawModeOverlay);
		m_btnCancelDraw->setFixedSize(100, 32);
		m_btnCancelDraw->setStyleSheet(
			"QPushButton { background-color: #e74c3c; color: white; border: none; border-radius: 4px; font-size: 13px; }"
			"QPushButton:hover { background-color: #c0392b; }"
			"QPushButton:pressed { background-color: #a93226; }");
		m_btnCancelDraw->setFocusPolicy(Qt::NoFocus);

		QHBoxLayout* btnLayout = new QHBoxLayout();
		btnLayout->setAlignment(Qt::AlignCenter);
		btnLayout->addWidget(m_btnCancelDraw);

		outerLayout->addWidget(hintLabel);
		outerLayout->addSpacing(10);
		outerLayout->addLayout(btnLayout);

		connect(m_btnCancelDraw, &QPushButton::clicked, this, [this]() {
			stopDraw();
			ui.draw_cbox_shape_type->blockSignals(true);
			ui.draw_cbox_shape_type->setCurrentIndex(0);
			ui.draw_cbox_shape_type->blockSignals(false);
		});
	}
	m_drawModeOverlay->setGeometry(parent->contentsRect());
	m_drawModeOverlay->show();
	m_drawModeOverlay->raise();
}

void ImageCanvasView::hideDrawModeOverlay()
{
	if (m_drawModeOverlay) m_drawModeOverlay->hide();
}

// ===== 参数面板 =====

void ImageCanvasView::slotOpenParamPanel() { showParamPanel(); }

void ImageCanvasView::showParamPanel()
{
	int index = ui.draw_cbox_shape_type->currentIndex();
	if (index <= 0) return;
	DrawShapeType type = static_cast<DrawShapeType>(index - 1);

	QStringList paramNames;
	switch (type)
	{
	case Shape_Rect:
		paramNames << QStringLiteral("中心 X") << QStringLiteral("中心 Y") << QStringLiteral("宽度") << QStringLiteral("高度");
		break;
	default:
		paramNames << QStringLiteral("参数1") << QStringLiteral("参数2") << QStringLiteral("参数3") << QStringLiteral("参数4");
		break;
	}

	QWidget* parent = ui.group_draw_opt;
	if (!m_paramPanel)
	{
		m_paramPanel = new QWidget(parent);
		m_paramPanel->setObjectName("paramPanel");
		m_paramPanel->setStyleSheet("QWidget#paramPanel { background-color: rgba(50, 50, 50, 220); border-radius: 6px; }");
		m_paramPanel->installEventFilter(this);

		QVBoxLayout* outerLayout = new QVBoxLayout(m_paramPanel);
		outerLayout->setAlignment(Qt::AlignCenter);

		QLabel* titleLabel = new QLabel(QStringLiteral("参数设置"), m_paramPanel);
		titleLabel->setStyleSheet("color: white; font-size: 14px; font-weight: bold; background: transparent;");
		titleLabel->setAlignment(Qt::AlignCenter);
		outerLayout->addWidget(titleLabel);
		outerLayout->addSpacing(10);

		for (int i = 0; i < 4; ++i)
		{
			QHBoxLayout* row = new QHBoxLayout();
			m_paramLabel[i] = new QLabel(paramNames[i], m_paramPanel);
			m_paramLabel[i]->setStyleSheet("color: #ccc; font-size: 12px; background: transparent; min-width: 50px;");
			m_paramLabel[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

			m_paramSpin[i] = new QDoubleSpinBox(m_paramPanel);
			m_paramSpin[i]->setDecimals(3);
			m_paramSpin[i]->setRange(-999999, 999999);
			m_paramSpin[i]->setStyleSheet("QDoubleSpinBox { background: #3a3a3a; color: white; border: 1px solid #666; border-radius: 3px; padding: 3px; }");
			m_paramSpin[i]->setFixedWidth(120);

			row->addWidget(m_paramLabel[i]);
			row->addWidget(m_paramSpin[i]);
			outerLayout->addLayout(row);
		}

		outerLayout->addSpacing(10);

		QHBoxLayout* btnLayout = new QHBoxLayout();
		btnLayout->setAlignment(Qt::AlignCenter);

		QPushButton* btnOk = new QPushButton(QStringLiteral("确定"), m_paramPanel);
		btnOk->setFixedSize(80, 30);
		btnOk->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; border: none; border-radius: 4px; font-size: 13px; }"
			"QPushButton:hover { background-color: #27ae60; }");
		btnOk->setFocusPolicy(Qt::NoFocus);

		QPushButton* btnCancel = new QPushButton(QStringLiteral("取消"), m_paramPanel);
		btnCancel->setFixedSize(80, 30);
		btnCancel->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border: none; border-radius: 4px; font-size: 13px; }"
			"QPushButton:hover { background-color: #c0392b; }");
		btnCancel->setFocusPolicy(Qt::NoFocus);

		btnLayout->addWidget(btnOk);
		btnLayout->addSpacing(15);
		btnLayout->addWidget(btnCancel);
		outerLayout->addLayout(btnLayout);

		connect(btnOk, &QPushButton::clicked, this, &ImageCanvasView::applyParamAndRedraw);
		connect(btnCancel, &QPushButton::clicked, this, &ImageCanvasView::hideParamPanel);
	}

	for (int i = 0; i < 4; ++i) m_paramLabel[i]->setText(paramNames[i]);

	DrawShapeItem* existing = findShapeByType(type);
	if (existing && type == Shape_Rect)
	{
		m_paramSpin[0]->setValue(existing->rect.cx);
		m_paramSpin[1]->setValue(existing->rect.cy);
		m_paramSpin[2]->setValue(existing->rect.w);
		m_paramSpin[3]->setValue(existing->rect.h);
	}
	else { for (int i = 0; i < 4; ++i) m_paramSpin[i]->setValue(0); }

	m_paramPanel->setGeometry(parent->contentsRect());
	m_paramPanel->show();
	m_paramPanel->raise();
}

void ImageCanvasView::hideParamPanel() { if (m_paramPanel) m_paramPanel->hide(); }

void ImageCanvasView::applyParamAndRedraw()
{
	int index = ui.draw_cbox_shape_type->currentIndex();
	if (index <= 0) return;
	DrawShapeType type = static_cast<DrawShapeType>(index - 1);

	DrawShapeItem* shape = findShapeByType(type);
	if (!shape) { shape = new DrawShapeItem(type); m_shapes.append(shape); }
	else
	{
		if (shape->item) { m_scene->removeItem(shape->item); delete shape->item; shape->item = nullptr; }
		for (auto* h : shape->handles) { m_scene->removeItem(h); delete h; }
		shape->handles.clear();
		if (shape->rotateHandle) { m_scene->removeItem(shape->rotateHandle); delete shape->rotateHandle; shape->rotateHandle = nullptr; }
		if (shape->centerCrossH) { m_scene->removeItem(shape->centerCrossH); delete shape->centerCrossH; shape->centerCrossH = nullptr; }
		if (shape->centerCrossV) { m_scene->removeItem(shape->centerCrossV); delete shape->centerCrossV; shape->centerCrossV = nullptr; }
	}

	if (type == Shape_Rect)
	{
		shape->rect.cx = m_paramSpin[0]->value();
		shape->rect.cy = m_paramSpin[1]->value();
		shape->rect.w = m_paramSpin[2]->value();
		shape->rect.h = m_paramSpin[3]->value();
	}

	shape->state = ShapeState_Selected;
	shape->item = buildShapeItem(*shape);
	if (shape->item) { m_scene->addItem(shape->item); shape->item->setAcceptHoverEvents(true); }
	applyShapeStateStyle(shape);
	showHandles(shape);
	deselectAll();
	m_selectedShape = shape;
	showOnlyShape(type);
	hideParamPanel();
}

// ===== 绘制流程 =====

void ImageCanvasView::startDraw(ImageCanvasView::DrawShapeType type)
{
	deselectAll();
	m_currentShape = type;
	m_mode = Mode_Draw;
	m_drawStep = 0;
	if (m_ghostRect) { m_scene->removeItem(m_ghostRect); delete m_ghostRect; m_ghostRect = nullptr; }
	setAllShapesVisible(false);
	showDrawModeOverlay();
	ui.canvas_view_main->setCursor(Qt::CrossCursor);
}

void ImageCanvasView::stopDraw()
{
	if (m_ghostRect) { m_scene->removeItem(m_ghostRect); delete m_ghostRect; m_ghostRect = nullptr; }
	m_mode = Mode_None;
	m_drawStep = 0;
	hideDrawModeOverlay();

	if (!m_selectedShape && m_activeShapeIndex >= 1 && m_activeShapeIndex <= 7)
	{
		DrawShapeType type = static_cast<DrawShapeType>(m_activeShapeIndex - 1);
		DrawShapeItem* existing = findShapeByType(type);
		if (existing) setShapeState(existing, ShapeState_Selected);
		showOnlyShape(type);
	}
	ui.canvas_view_main->setCursor(Qt::ArrowCursor);
}

void ImageCanvasView::updateGhostRect(const QPointF& scenePos)
{
	if (!m_ghostRect) return;
	double x1 = std::min(m_anchorPoint.x(), scenePos.x());
	double y1 = std::min(m_anchorPoint.y(), scenePos.y());
	double x2 = std::max(m_anchorPoint.x(), scenePos.x());
	double y2 = std::max(m_anchorPoint.y(), scenePos.y());
	m_ghostRect->setRect(QRectF(QPointF(x1, y1), QPointF(x2, y2)));
}

void ImageCanvasView::commitRect()
{
	if (!m_ghostRect) return;
	QRectF gr = m_ghostRect->rect();
	double cx = gr.center().x();
	double cy = gr.center().y();
	double w = gr.width();
	double h = gr.height();
	if (w < 3 || h < 3) return;

	DrawShapeItem* shape = findShapeByType(Shape_Rect);
	if (!shape) { shape = new DrawShapeItem(Shape_Rect); m_shapes.append(shape); }
	else
	{
		if (shape->item) { m_scene->removeItem(shape->item); delete shape->item; shape->item = nullptr; }
		for (auto* dh : shape->handles) { m_scene->removeItem(dh); delete dh; }
		shape->handles.clear();
		if (shape->centerCrossH) { m_scene->removeItem(shape->centerCrossH); delete shape->centerCrossH; shape->centerCrossH = nullptr; }
		if (shape->centerCrossV) { m_scene->removeItem(shape->centerCrossV); delete shape->centerCrossV; shape->centerCrossV = nullptr; }
	}

	shape->rect = { cx, cy, w, h };
	shape->state = ShapeState_Selected;
	m_selectedShape = shape;

	shape->item = buildShapeItem(*shape);
	if (shape->item) { m_scene->addItem(shape->item); shape->item->setAcceptHoverEvents(true); }
	applyShapeStateStyle(shape);
	showHandles(shape);
	showOnlyShape(Shape_Rect);
}

void ImageCanvasView::commitShapeGeneric(ImageCanvasView::DrawShapeType type)
{
	// TODO
}

// ===== 形状管理 =====

ImageCanvasView::DrawShapeItem* ImageCanvasView::findShapeByType(ImageCanvasView::DrawShapeType type)
{
	for (auto* s : m_shapes)
		if (s->type == type) return s;
	return nullptr;
}

void ImageCanvasView::setAllShapesVisible(bool visible)
{
	for (auto* s : m_shapes)
	{
		if (s->item) s->item->setVisible(visible);
		if (!visible && s == m_selectedShape) deselectAll();
	}
}

void ImageCanvasView::showOnlyShape(ImageCanvasView::DrawShapeType type)
{
	for (auto* s : m_shapes)
	{
		bool isTarget = (s->type == type);
		if (isTarget)
		{
			if (s->state == ShapeState_Hidden)
				setShapeState(s, ShapeState_Normal);
		}
		else
		{
			setShapeState(s, ShapeState_Hidden);
			if (s == m_hoverShape) m_hoverShape = nullptr;
		}
	}
}

// ===== 图形构建 =====

QGraphicsItem* ImageCanvasView::buildShapeItem(const ImageCanvasView::DrawShapeItem& shape)
{
	QGraphicsItem* item = nullptr;
	switch (shape.type)
	{
	case Shape_Rect:
	{
		QGraphicsRectItem* r = new QGraphicsRectItem(
			shape.rect.cx - shape.rect.w / 2.0, shape.rect.cy - shape.rect.h / 2.0,
			shape.rect.w, shape.rect.h);
		r->setZValue(80);
		item = r;
		break;
	}
	default: break;
	}
	return item;
}

void ImageCanvasView::applyShapeStateStyle(DrawShapeItem* shape)
{
	if (!shape || !shape->item) return;

	QColor c;
	switch (shape->state)
	{
	case ShapeState_Normal:   c = m_colorNormal;   break;
	case ShapeState_Hover:    c = m_colorHover;    break;
	case ShapeState_Selected: c = m_colorSelected; break;
	default: return;
	}

	QPen pen(c, m_penWidth);
	QGraphicsRectItem* rItem = dynamic_cast<QGraphicsRectItem*>(shape->item);
	if (rItem) { rItem->setPen(pen); rItem->setBrush(Qt::NoBrush); }
}

// ===== 控制点 & 中心十字 =====

void ImageCanvasView::showHandles(ImageCanvasView::DrawShapeItem* shape)
{
	if (!shape) return;

	for (auto* h : shape->handles) { m_scene->removeItem(h); delete h; }
	shape->handles.clear();
	if (shape->rotateHandle) { m_scene->removeItem(shape->rotateHandle); delete shape->rotateHandle; shape->rotateHandle = nullptr; }

	double cx = 0, cy = 0, halfW = 0, halfH = 0;
	if (shape->type == Shape_Rect) { cx = shape->rect.cx; cy = shape->rect.cy; halfW = shape->rect.w / 2.0; halfH = shape->rect.h / 2.0; }
	if (halfW <= 0 && halfH <= 0) return;

	QPointF localPts[8] = {
		{-halfW, -halfH}, {0, -halfH}, {halfW, -halfH},
		{halfW, 0},       {halfW, halfH}, {0, halfH},
		{-halfW, halfH},  {-halfW, 0}
	};

	for (int i = 0; i < 8; ++i)
	{
		QPointF pt = localPts[i] + QPointF(cx, cy);
		QGraphicsEllipseItem* h = new QGraphicsEllipseItem(
			pt.x() - kHandleRadius, pt.y() - kHandleRadius, kHandleRadius * 2, kHandleRadius * 2);
		h->setPen(QPen(Qt::white, 1));
		h->setBrush(QColor(0, 180, 255));
		h->setZValue(100);
		h->setData(0, i);
		h->setAcceptHoverEvents(true);
		m_scene->addItem(h);
		shape->handles.append(h);
		h->setVisible(true);
	}

	// 中心十字
	if (shape->centerCrossH) { m_scene->removeItem(shape->centerCrossH); delete shape->centerCrossH; }
	if (shape->centerCrossV) { m_scene->removeItem(shape->centerCrossV); delete shape->centerCrossV; }
	shape->centerCrossH = new QGraphicsLineItem();
	shape->centerCrossV = new QGraphicsLineItem();
	QPen crossPen(QColor(0, 200, 0), 1);
	crossPen.setCosmetic(true);
	shape->centerCrossH->setPen(crossPen);
	shape->centerCrossV->setPen(crossPen);
	shape->centerCrossH->setZValue(90);
	shape->centerCrossV->setZValue(90);
	shape->centerCrossH->setLine(cx - kCrossLen, cy, cx + kCrossLen, cy);
	shape->centerCrossV->setLine(cx, cy - kCrossLen, cx, cy + kCrossLen);
	m_scene->addItem(shape->centerCrossH);
	m_scene->addItem(shape->centerCrossV);
}

void ImageCanvasView::updateHandlePositions(DrawShapeItem* shape)
{
	if (!shape || shape->type != Shape_Rect) return;

	double cx = shape->rect.cx, cy = shape->rect.cy;
	double halfW = shape->rect.w / 2.0, halfH = shape->rect.h / 2.0;

	QPointF localPts[8] = {
		{-halfW, -halfH}, {0, -halfH}, {halfW, -halfH},
		{halfW, 0},       {halfW, halfH}, {0, halfH},
		{-halfW, halfH},  {-halfW, 0}
	};

	for (int i = 0; i < shape->handles.size() && i < 8; ++i)
	{
		QPointF pt = localPts[i] + QPointF(cx, cy);
		moveHandle(shape->handles[i], pt);
	}
	if (shape->item)
	{
		QGraphicsRectItem* rItem = dynamic_cast<QGraphicsRectItem*>(shape->item);
		if (rItem) rItem->setRect(cx - halfW, cy - halfH, shape->rect.w, shape->rect.h);
	}
	if (shape->centerCrossH) shape->centerCrossH->setLine(cx - kCrossLen, cy, cx + kCrossLen, cy);
	if (shape->centerCrossV) shape->centerCrossV->setLine(cx, cy - kCrossLen, cx, cy + kCrossLen);
}

void ImageCanvasView::hideAllHandles()
{
	for (auto* shape : m_shapes)
	{
		for (auto* h : shape->handles) h->setVisible(false);
		if (shape->rotateHandle) shape->rotateHandle->setVisible(false);
	}
}

// ===== 选中 & 状态 =====

void ImageCanvasView::deselectAll()
{
	if (m_selectedShape)
		setShapeState(m_selectedShape, ShapeState_Normal);
	m_selectedShape = nullptr;
}

bool ImageCanvasView::isPointInShape(const DrawShapeItem* shape, const QPointF& scenePos) const
{
	if (!shape || shape->state == ShapeState_Hidden) return false;
	switch (shape->type)
	{
	case Shape_Rect:
	{
		double left = shape->rect.cx - shape->rect.w / 2.0;
		double right = shape->rect.cx + shape->rect.w / 2.0;
		double top = shape->rect.cy - shape->rect.h / 2.0;
		double bottom = shape->rect.cy + shape->rect.h / 2.0;
		return scenePos.x() >= left && scenePos.x() <= right && scenePos.y() >= top && scenePos.y() <= bottom;
	}
	default: return false;
	}
}

ImageCanvasView::DrawShapeItem* ImageCanvasView::shapeAt(const QPointF& scenePos) const
{
	for (auto* shape : m_shapes)
		if (isPointInShape(shape, scenePos)) return shape;
	return nullptr;
}

QGraphicsEllipseItem* ImageCanvasView::handleAt(const QPointF& scenePos) const
{
	if (!m_selectedShape) return nullptr;
	for (auto* h : m_selectedShape->handles)
		if (h->isVisible() && h->contains(h->mapFromScene(scenePos))) return h;
	return nullptr;
}

void ImageCanvasView::setShapeState(DrawShapeItem* shape, ShapeState state)
{
	if (!shape) return;
	if (shape->state == state) return;

	shape->state = state;

	if (shape->item) shape->item->setVisible(state != ShapeState_Hidden);

	if (state == ShapeState_Hidden || state == ShapeState_Normal)
	{
		for (auto* h : shape->handles) h->setVisible(false);
		if (shape->rotateHandle) shape->rotateHandle->setVisible(false);
		if (shape->centerCrossH) shape->centerCrossH->setVisible(false);
		if (shape->centerCrossV) shape->centerCrossV->setVisible(false);
	}
	else if (state == ShapeState_Selected)
	{
		if (m_selectedShape && m_selectedShape != shape)
		{
			m_selectedShape->state = ShapeState_Normal;
			applyShapeStateStyle(m_selectedShape);
			for (auto* h : m_selectedShape->handles) h->setVisible(false);
			if (m_selectedShape->rotateHandle) m_selectedShape->rotateHandle->setVisible(false);
			if (m_selectedShape->centerCrossH) m_selectedShape->centerCrossH->setVisible(false);
			if (m_selectedShape->centerCrossV) m_selectedShape->centerCrossV->setVisible(false);
		}
		m_selectedShape = shape;
		showHandles(shape);
	}
	else if (state == ShapeState_Hover)
	{
		if (m_hoverShape && m_hoverShape != shape && m_hoverShape->state == ShapeState_Hover)
			setShapeState(m_hoverShape, ShapeState_Normal);
		m_hoverShape = shape;
	}

	applyShapeStateStyle(shape);
}

// ===== Handle 拖拽 =====

void ImageCanvasView::applyHandleHover(int handleIndex, bool hover)
{
	if (!m_selectedShape) return;
	if (handleIndex < 0 || handleIndex >= m_selectedShape->handles.size()) return;
	setHandleHover(m_selectedShape->handles[handleIndex], hover);
}

void ImageCanvasView::clearAllHandleHover()
{
	if (!m_selectedShape) return;
	for (int i = 0; i < m_selectedShape->handles.size(); ++i) applyHandleHover(i, false);
}

void ImageCanvasView::updateRectFromHandle(const QPointF& scenePos)
{
	if (!m_dragShape || m_dragHandleIndex < 0) return;

	double sx = scenePos.x(), sy = scenePos.y();
	double& cx = m_dragShape->rect.cx;
	double& cy = m_dragShape->rect.cy;
	double& w = m_dragShape->rect.w;
	double& h = m_dragShape->rect.h;

	double left0   = m_dragStartRect.cx - m_dragStartRect.w / 2.0;
	double right0  = m_dragStartRect.cx + m_dragStartRect.w / 2.0;
	double top0    = m_dragStartRect.cy - m_dragStartRect.h / 2.0;
	double bottom0 = m_dragStartRect.cy + m_dragStartRect.h / 2.0;

	double left = left0, right = right0, top = top0, bottom = bottom0;

	switch (m_dragHandleIndex)
	{
	case 0: left = sx; top = sy; break;
	case 1: top = sy; break;
	case 2: right = sx; top = sy; break;
	case 3: right = sx; break;
	case 4: right = sx; bottom = sy; break;
	case 5: bottom = sy; break;
	case 6: left = sx; bottom = sy; break;
	case 7: left = sx; break;
	}

	if (left > right) std::swap(left, right);
	if (top > bottom) std::swap(top, bottom);

	cx = (left + right) / 2.0;
	cy = (top + bottom) / 2.0;
	w = right - left;
	h = bottom - top;

	if (w < 3) w = 3;
	if (h < 3) h = 3;

	updateHandlePositions(m_dragShape);
}
