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
			if (m_currentShape == Shape_Circle)
			{
				// 三点画圆：全程实时预览
				// step0→step1: 点1+鼠标=直径画ghost圆
				// step1→step2: 点1+点2+鼠标=三角形外心画ghost圆
				// step2→确认: 提交
				if (event->type() == QEvent::MouseButtonPress)
				{
					QMouseEvent* me = static_cast<QMouseEvent*>(event);
					if (me->button() == Qt::LeftButton)
					{
						if (m_drawStep == 0)
						{
							m_circlePt1 = scenePos;
							m_drawStep = 1;
							// 标记点1
							double mrkR = 3.0;
							m_circleMarker1 = new QGraphicsEllipseItem(
								m_circlePt1.x()-mrkR, m_circlePt1.y()-mrkR, mrkR*2, mrkR*2);
							m_circleMarker1->setPen(Qt::NoPen);
							m_circleMarker1->setBrush(QColor(255, 80, 80));
							m_circleMarker1->setZValue(100);
							m_scene->addItem(m_circleMarker1);
						}
						else if (m_drawStep == 1)
						{
							m_circlePt2 = scenePos;
							m_drawStep = 2;
							// 标记点2
							double mrkR = 3.0;
							m_circleMarker2 = new QGraphicsEllipseItem(
								m_circlePt2.x()-mrkR, m_circlePt2.y()-mrkR, mrkR*2, mrkR*2);
							m_circleMarker2->setPen(Qt::NoPen);
							m_circleMarker2->setBrush(QColor(255, 80, 80));
							m_circleMarker2->setZValue(100);
							m_scene->addItem(m_circleMarker2);
						}
						else if (m_drawStep == 2)
						{
							commitCircle();
							if (m_ghostEllipse) { m_scene->removeItem(m_ghostEllipse); delete m_ghostEllipse; m_ghostEllipse = nullptr; }
							if (m_circleMarker1) { m_scene->removeItem(m_circleMarker1); delete m_circleMarker1; m_circleMarker1 = nullptr; }
							if (m_circleMarker2) { m_scene->removeItem(m_circleMarker2); delete m_circleMarker2; m_circleMarker2 = nullptr; }
							hideDrawModeOverlay();
							m_mode = Mode_None;
							m_drawStep = 0;
							ui.canvas_view_main->setCursor(Qt::ArrowCursor);
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
				else if (event->type() == QEvent::MouseMove && (m_drawStep == 1 || m_drawStep == 2))
				{
					// 确保 ghost ellipse 存在
					if (!m_ghostEllipse)
					{
						m_ghostEllipse = new QGraphicsEllipseItem();
						QPen ghostPen(QColor(0, 180, 255), m_penWidth, Qt::DashLine);
						ghostPen.setCosmetic(true);
						m_ghostEllipse->setPen(ghostPen);
						m_ghostEllipse->setBrush(QColor(0, 180, 255, 30));
						m_ghostEllipse->setZValue(99);
						m_scene->addItem(m_ghostEllipse);
					}

					if (m_drawStep == 1)
					{
						// 两点模式：以点1到鼠标为直径的圆
						double d1x = m_circlePt1.x(), d1y = m_circlePt1.y();
						double mx  = scenePos.x(), my = scenePos.y();
						double cx = (d1x + mx) / 2.0;
						double cy = (d1y + my) / 2.0;
						double r  = std::sqrt((mx-d1x)*(mx-d1x) + (my-d1y)*(my-d1y)) / 2.0;
						m_ghostEllipse->setRect(cx-r, cy-r, r*2, r*2);
					}
					else // m_drawStep == 2
					{
						// 三点模式：三角形外心
						double x1=m_circlePt1.x(), y1=m_circlePt1.y();
						double x2=m_circlePt2.x(), y2=m_circlePt2.y();
						double x3=scenePos.x(),   y3=scenePos.y();
						double d = 2.0*(x1*(y2-y3) + x2*(y3-y1) + x3*(y1-y2));
						if (std::abs(d) < 1e-6) { event->accept(); return true; }
						double ux = ((x1*x1+y1*y1)*(y2-y3) + (x2*x2+y2*y2)*(y3-y1) + (x3*x3+y3*y3)*(y1-y2)) / d;
						double uy = ((x1*x1+y1*y1)*(x3-x2) + (x2*x2+y2*y2)*(x1-x3) + (x3*x3+y3*y3)*(x2-x1)) / d;
						double r  = std::sqrt((x1-ux)*(x1-ux) + (y1-uy)*(y1-uy));
						m_ghostEllipse->setRect(ux-r, uy-r, r*2, r*2);
					}
					event->accept();
					return true;
				}
				return false;
			}

			// 通用两步绘制（矩形、旋转矩形等）
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
						commitShapeGeneric(m_currentShape);
						if (m_ghostRect) { m_scene->removeItem(m_ghostRect); delete m_ghostRect; m_ghostRect = nullptr; }
						hideDrawModeOverlay();
						m_mode = Mode_None;
						m_drawStep = 0;
						ui.canvas_view_main->setCursor(Qt::ArrowCursor);
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
			return false;
		}

		// ===== 非绘制模式 =====
		static QPoint lastViewPos;
		static bool isPanning = false;

		// MouseButtonRelease（停止拖拽）
		if (event->type() == QEvent::MouseButtonRelease)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::LeftButton)
			{
				if (m_isRotating)
				{
					m_isRotating = false;
					m_dragShape = nullptr;
					m_dragHandleIndex = -1;
					ui.canvas_view_main->setCursor(Qt::CrossCursor);
					event->accept();
					return true;
				}
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
				if (isPanning)
				{
					isPanning = false;
					ui.canvas_view_main->setCursor(Qt::ArrowCursor);
					return true;
				}
			}
		}

		// 旋转拖拽中
		if (m_isRotating && event->type() == QEvent::MouseMove)
		{
			updateRotatedRectFromHandle(scenePos);
			event->accept();
			return true;
		}
		// Handle 拖拽中
		if (m_isDraggingHandle && event->type() == QEvent::MouseMove)
		{
			if (m_dragShape && m_dragShape->type == Shape_RotateRect)
				updateRotatedRectFromHandle(scenePos);
			else if (m_dragShape && m_dragShape->type == Shape_Circle)
				updateCircleFromHandle(scenePos);
			else
				updateRectFromHandle(scenePos);
			event->accept();
			return true;
		}
		// 形状拖动中
		if (m_isDraggingShape && event->type() == QEvent::MouseMove)
		{
			if (m_activeShape)
			{
				if (m_activeShape->type == Shape_RotateRect)
				{
					m_activeShape->rotatedRect.cx = scenePos.x() - m_dragOffset.x();
					m_activeShape->rotatedRect.cy = scenePos.y() - m_dragOffset.y();
				}
				else if (m_activeShape->type == Shape_Circle)
				{
					m_activeShape->circle.cx = scenePos.x() - m_dragOffset.x();
					m_activeShape->circle.cy = scenePos.y() - m_dragOffset.y();
				}
				else
				{
					m_activeShape->rect.cx = scenePos.x() - m_dragOffset.x();
					m_activeShape->rect.cy = scenePos.y() - m_dragOffset.y();
				}
				updateHandlePositions(m_activeShape);
			}
			event->accept();
			return true;
		}

		if (event->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::LeftButton && m_activeShape)
			{
				// 优先检测 handle
				QGraphicsEllipseItem* hitHandle = handleAt(scenePos);
				if (hitHandle)
				{
					int hIdx = hitHandle->data(0).toInt();
					int role = hitHandle->data(1).toInt();
					if (role == 1) // 旋转控制点
					{
						m_isRotating = true;
						m_dragShape = m_activeShape;
						m_dragHandleIndex = hIdx;
						m_dragStartAngle = m_activeShape->rotatedRect.angle;
						ui.canvas_view_main->setCursor(Qt::ClosedHandCursor);
						event->accept();
						return true;
					}
					if (hIdx >= 0 && hIdx < m_activeShape->handles.size())
					{
						m_isDraggingHandle = true;
						m_dragShape = m_activeShape;
						m_dragHandleIndex = hIdx;
						if (m_activeShape->type == Shape_RotateRect)
						{
							m_dragStartRect = { m_activeShape->rotatedRect.cx, m_activeShape->rotatedRect.cy,
							                    m_activeShape->rotatedRect.w, m_activeShape->rotatedRect.h };
							m_dragStartAngle = m_activeShape->rotatedRect.angle;
						}
						else if (m_activeShape->type == Shape_Circle)
							m_dragStartRect = { m_activeShape->circle.cx, m_activeShape->circle.cy,
							                    m_activeShape->circle.r * 2, m_activeShape->circle.r * 2 };
						else
							m_dragStartRect = m_activeShape->rect;
						clearAllHandleHover();
						applyHandleHover(hIdx, true);
						ui.canvas_view_main->setCursor(Qt::ClosedHandCursor);
						event->accept();
						return true;
					}
				}

				// 检测形状内部拖动
				if (isPointInShape(m_activeShape, scenePos))
				{
				m_isDraggingShape = true;
				if (m_activeShape->type == Shape_RotateRect)
					m_dragOffset = QPointF(scenePos.x() - m_activeShape->rotatedRect.cx,
					                       scenePos.y() - m_activeShape->rotatedRect.cy);
				else if (m_activeShape->type == Shape_Circle)
					m_dragOffset = QPointF(scenePos.x() - m_activeShape->circle.cx,
					                       scenePos.y() - m_activeShape->circle.cy);
				else
					m_dragOffset = QPointF(scenePos.x() - m_activeShape->rect.cx,
					                       scenePos.y() - m_activeShape->rect.cy);
					event->accept();
					return true;
				}
			}

			// 空白区域 → 平移
			lastViewPos = me->pos();
			isPanning = true;
			ui.canvas_view_main->setCursor(Qt::ClosedHandCursor);
			return true;
		}
		else if (event->type() == QEvent::MouseMove)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			if (isPanning && (me->buttons() & Qt::LeftButton))
			{
				QPoint delta = me->pos() - lastViewPos;
				lastViewPos = me->pos();
				ui.canvas_view_main->horizontalScrollBar()->setValue(
					ui.canvas_view_main->horizontalScrollBar()->value() - delta.x());
				ui.canvas_view_main->verticalScrollBar()->setValue(
					ui.canvas_view_main->verticalScrollBar()->value() - delta.y());
			}
			else if (!isPanning)
			{
				// Handle hover
				QGraphicsEllipseItem* hoverHandle = handleAt(scenePos);
				if (hoverHandle && m_activeShape)
				{
					int hIdx = hoverHandle->data(0).toInt();
					static int lastHoverHandle = -1;
					if (lastHoverHandle != hIdx)
					{
						applyHandleHover(lastHoverHandle, false);
						applyHandleHover(hIdx, true);
						lastHoverHandle = hIdx;
					}
					ui.canvas_view_main->setCursor(Qt::SizeAllCursor);
				}
				else
				{
					clearAllHandleHover();
					static int lastHoverHandle = -1;
					lastHoverHandle = -1;
					ui.canvas_view_main->setCursor(Qt::ArrowCursor);
				}
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

	if (event->key() == Qt::Key_Delete && m_activeShape)
	{
		clearSceneShape();
		m_shapes.removeOne(m_activeShape);
		delete m_activeShape;
		m_activeShape = nullptr;
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
	if (m_activeShape && m_activeShape->item)
		applyStyle(m_activeShape);
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

// ===== Combo box 切换 =====

void ImageCanvasView::slot_draw_shape_changed(int index)
{
	if (index <= 0)
	{
		if (m_mode == Mode_Draw) stopDraw();
		clearSceneShape();
		m_activeShape = nullptr;
		m_activeShapeIndex = 0;
		ui.canvas_view_main->setCursor(Qt::ArrowCursor);
		return;
	}

	int shapeIdx = index - 1;
	DrawShapeType type = static_cast<DrawShapeType>(shapeIdx);
	m_currentShape = type;
	m_activeShapeIndex = index;

	// 先清掉 scene 上旧的
	clearSceneShape();
	m_activeShape = nullptr;

	DrawShapeItem* existing = findShapeByType(type);
	if (existing)
	{
		// 已有数据 → 重绘
		m_activeShape = existing;
		rebuildShapeOnScene(existing);
		ui.canvas_view_main->setCursor(Qt::ArrowCursor);
	}
	else
	{
		// 无数据 → 进入绘制模式
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

	// 清除 scene + 从列表删除
	clearSceneShape();
	m_shapes.removeOne(shape);
	m_activeShape = nullptr;
	delete shape;

	// 进入绘制模式
	startDraw(type);
}

// ===== Scene 清理 & 重绘 =====

void ImageCanvasView::clearSceneShape()
{
	if (!m_activeShape) return;
	if (m_activeShape->item) { m_scene->removeItem(m_activeShape->item); delete m_activeShape->item; m_activeShape->item = nullptr; }
	for (auto* h : m_activeShape->handles) { m_scene->removeItem(h); delete h; }
	m_activeShape->handles.clear();
	if (m_activeShape->rotateHandle) { m_scene->removeItem(m_activeShape->rotateHandle); delete m_activeShape->rotateHandle; m_activeShape->rotateHandle = nullptr; }
	if (m_activeShape->rotateStickLine) { m_scene->removeItem(m_activeShape->rotateStickLine); delete m_activeShape->rotateStickLine; m_activeShape->rotateStickLine = nullptr; }
	if (m_activeShape->circumRect) { m_scene->removeItem(m_activeShape->circumRect); delete m_activeShape->circumRect; m_activeShape->circumRect = nullptr; }
	if (m_activeShape->centerCrossH) { m_scene->removeItem(m_activeShape->centerCrossH); delete m_activeShape->centerCrossH; m_activeShape->centerCrossH = nullptr; }
	if (m_activeShape->centerCrossV) { m_scene->removeItem(m_activeShape->centerCrossV); delete m_activeShape->centerCrossV; m_activeShape->centerCrossV = nullptr; }
}

void ImageCanvasView::rebuildShapeOnScene(DrawShapeItem* shape)
{
	if (!shape) return;
	shape->item = buildShapeItem(*shape);
	if (shape->item) { m_scene->addItem(shape->item); shape->item->setAcceptHoverEvents(true); }
	applyStyle(shape);
	showHandles(shape);
}

// ===== 样式 =====

void ImageCanvasView::applyStyle(DrawShapeItem* shape)
{
	if (!shape || !shape->item) return;
	QPen pen(m_colorSelected, m_penWidth);
	if (auto* r = dynamic_cast<QGraphicsRectItem*>(shape->item))
		{ r->setPen(pen); r->setBrush(Qt::NoBrush); }
	else if (auto* p = dynamic_cast<QGraphicsPolygonItem*>(shape->item))
		{ p->setPen(pen); p->setBrush(Qt::NoBrush); }
	else if (auto* e = dynamic_cast<QGraphicsEllipseItem*>(shape->item))
		{ e->setPen(pen); e->setBrush(Qt::NoBrush); }
}

// ===== 绘制模式浮层 =====

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

	// 各形状的参数名和数量
	QStringList paramNames;
	QVector<double> paramVals;
	int count = 0;

	switch (type)
	{
	case Shape_Rect:
		paramNames << QStringLiteral("中心 X") << QStringLiteral("中心 Y") << QStringLiteral("宽度") << QStringLiteral("高度");
		count = 4;
		break;
	case Shape_RotateRect:
		paramNames << QStringLiteral("中心 X") << QStringLiteral("中心 Y") << QStringLiteral("宽度") << QStringLiteral("高度") << QStringLiteral("角度");
		count = 5;
		break;
	case Shape_Circle:
		paramNames << QStringLiteral("中心 X") << QStringLiteral("中心 Y") << QStringLiteral("半径");
		count = 3;
		break;
	case Shape_Ellipse:
		paramNames << QStringLiteral("中心 X") << QStringLiteral("中心 Y") << QStringLiteral("半轴 r1") << QStringLiteral("半轴 r2") << QStringLiteral("角度");
		count = 5;
		break;
	case Shape_Ring:
		paramNames << QStringLiteral("中心 X") << QStringLiteral("中心 Y") << QStringLiteral("外半径") << QStringLiteral("内半径");
		count = 4;
		break;
	case Shape_Arc:
		paramNames << QStringLiteral("中心 X") << QStringLiteral("中心 Y") << QStringLiteral("外半径") << QStringLiteral("内半径") << QStringLiteral("起始角") << QStringLiteral("终止角");
		count = 6;
		break;
	default:
		paramNames << QStringLiteral("暂无参数");
		count = 0;
		break;
	}

	paramVals.resize(count);
	paramVals.fill(0);

	DrawShapeItem* existing = findShapeByType(type);
	if (existing)
	{
		switch (type)
		{
		case Shape_Rect:
			paramVals[0]=existing->rect.cx; paramVals[1]=existing->rect.cy; paramVals[2]=existing->rect.w; paramVals[3]=existing->rect.h;
			break;
		case Shape_RotateRect:
			paramVals[0]=existing->rotatedRect.cx; paramVals[1]=existing->rotatedRect.cy; paramVals[2]=existing->rotatedRect.w; paramVals[3]=existing->rotatedRect.h; paramVals[4]=existing->rotatedRect.angle;
			break;
		case Shape_Circle:
			paramVals[0]=existing->circle.cx; paramVals[1]=existing->circle.cy; paramVals[2]=existing->circle.r;
			break;
		case Shape_Ellipse:
			paramVals[0]=existing->ellipse.cx; paramVals[1]=existing->ellipse.cy; paramVals[2]=existing->ellipse.r1; paramVals[3]=existing->ellipse.r2; paramVals[4]=existing->ellipse.angle;
			break;
		case Shape_Ring:
			paramVals[0]=existing->ring.cx; paramVals[1]=existing->ring.cy; paramVals[2]=existing->ring.rOuter; paramVals[3]=existing->ring.rInner;
			break;
		case Shape_Arc:
			paramVals[0]=existing->arc.cx; paramVals[1]=existing->arc.cy; paramVals[2]=existing->arc.rOuter; paramVals[3]=existing->arc.rInner; paramVals[4]=existing->arc.startAngle; paramVals[5]=existing->arc.endAngle;
			break;
		default: break;
		}
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

		// 动态内容区
		m_paramContentLayout = new QVBoxLayout();
		outerLayout->addLayout(m_paramContentLayout);

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

	// 重建动态内容（删除旧的 spin/label）
	for (auto* s : m_paramSpins) { m_paramContentLayout->removeWidget(s); delete s; }
	m_paramSpins.clear();
	for (auto* l : m_paramLabels) { m_paramContentLayout->removeWidget(l); delete l; }
	m_paramLabels.clear();

	for (int i = 0; i < count; ++i)
	{
		QHBoxLayout* row = new QHBoxLayout();

		QLabel* label = new QLabel(paramNames[i], m_paramPanel);
		label->setStyleSheet("color: #ccc; font-size: 12px; background: transparent; min-width: 50px;");
		label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_paramLabels.append(label);

		QDoubleSpinBox* spin = new QDoubleSpinBox(m_paramPanel);
		spin->setDecimals(3);
		spin->setRange(-999999, 999999);
		spin->setStyleSheet("QDoubleSpinBox { background: #3a3a3a; color: white; border: 1px solid #666; border-radius: 3px; padding: 3px; }");
		spin->setFixedWidth(120);
		spin->setValue(paramVals[i]);
		m_paramSpins.append(spin);

		row->addWidget(label);
		row->addWidget(spin);
		m_paramContentLayout->addLayout(row);
	}

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

	// 清理旧 scene item
	clearSceneShape();
	m_activeShape = shape;

	int n = m_paramSpins.size();
	switch (type)
	{
	case Shape_Rect:
		if (n>=4) { shape->rect.cx = m_paramSpins[0]->value(); shape->rect.cy = m_paramSpins[1]->value(); shape->rect.w = m_paramSpins[2]->value(); shape->rect.h = m_paramSpins[3]->value(); }
		break;
	case Shape_RotateRect:
		if (n>=5) { shape->rotatedRect.cx = m_paramSpins[0]->value(); shape->rotatedRect.cy = m_paramSpins[1]->value(); shape->rotatedRect.w = m_paramSpins[2]->value(); shape->rotatedRect.h = m_paramSpins[3]->value(); shape->rotatedRect.angle = m_paramSpins[4]->value(); }
		break;
	case Shape_Circle:
		if (n>=3) { shape->circle.cx = m_paramSpins[0]->value(); shape->circle.cy = m_paramSpins[1]->value(); shape->circle.r = m_paramSpins[2]->value(); }
		break;
	case Shape_Ellipse:
		if (n>=5) { shape->ellipse.cx = m_paramSpins[0]->value(); shape->ellipse.cy = m_paramSpins[1]->value(); shape->ellipse.r1 = m_paramSpins[2]->value(); shape->ellipse.r2 = m_paramSpins[3]->value(); shape->ellipse.angle = m_paramSpins[4]->value(); }
		break;
	case Shape_Ring:
		if (n>=4) { shape->ring.cx = m_paramSpins[0]->value(); shape->ring.cy = m_paramSpins[1]->value(); shape->ring.rOuter = m_paramSpins[2]->value(); shape->ring.rInner = m_paramSpins[3]->value(); }
		break;
	case Shape_Arc:
		if (n>=6) { shape->arc.cx = m_paramSpins[0]->value(); shape->arc.cy = m_paramSpins[1]->value(); shape->arc.rOuter = m_paramSpins[2]->value(); shape->arc.rInner = m_paramSpins[3]->value(); shape->arc.startAngle = m_paramSpins[4]->value(); shape->arc.endAngle = m_paramSpins[5]->value(); }
		break;
	default: break;
	}

	rebuildShapeOnScene(shape);
	hideParamPanel();
}

// ===== 绘制流程 =====

void ImageCanvasView::startDraw(DrawShapeType type)
{
	m_currentShape = type;
	m_mode = Mode_Draw;
	m_drawStep = 0;
	if (m_ghostRect) { m_scene->removeItem(m_ghostRect); delete m_ghostRect; m_ghostRect = nullptr; }
	if (m_ghostEllipse) { m_scene->removeItem(m_ghostEllipse); delete m_ghostEllipse; m_ghostEllipse = nullptr; }
	if (m_ghostCircumRect) { m_scene->removeItem(m_ghostCircumRect); delete m_ghostCircumRect; m_ghostCircumRect = nullptr; }
	if (m_circleMarker1) { m_scene->removeItem(m_circleMarker1); delete m_circleMarker1; m_circleMarker1 = nullptr; }
	if (m_circleMarker2) { m_scene->removeItem(m_circleMarker2); delete m_circleMarker2; m_circleMarker2 = nullptr; }
	showDrawModeOverlay();
	ui.canvas_view_main->setCursor(Qt::CrossCursor);
}

void ImageCanvasView::stopDraw()
{
	if (m_ghostRect) { m_scene->removeItem(m_ghostRect); delete m_ghostRect; m_ghostRect = nullptr; }
	if (m_ghostEllipse) { m_scene->removeItem(m_ghostEllipse); delete m_ghostEllipse; m_ghostEllipse = nullptr; }
	if (m_ghostCircumRect) { m_scene->removeItem(m_ghostCircumRect); delete m_ghostCircumRect; m_ghostCircumRect = nullptr; }
	if (m_circleMarker1) { m_scene->removeItem(m_circleMarker1); delete m_circleMarker1; m_circleMarker1 = nullptr; }
	if (m_circleMarker2) { m_scene->removeItem(m_circleMarker2); delete m_circleMarker2; m_circleMarker2 = nullptr; }
	m_mode = Mode_None;
	m_drawStep = 0;
	hideDrawModeOverlay();
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

void ImageCanvasView::commitShapeGeneric(DrawShapeType type)
{
	switch (type)
	{
	case Shape_Rect:       commitRect();       break;
	case Shape_RotateRect: commitRotatedRect();break;
	default: break;
	}
}

// ===== 圆形提交（三点画圆） =====
void ImageCanvasView::commitCircle()
{
	double x1=m_circlePt1.x(), y1=m_circlePt1.y();
	double x2=m_circlePt2.x(), y2=m_circlePt2.y();
	if (!m_ghostEllipse) return;
	QRectF r = m_ghostEllipse->rect();
	double cx = r.center().x(), cy = r.center().y(), radius = r.width()/2.0;
	if (radius < 1.5) return;

	DrawShapeItem* shape = findShapeByType(Shape_Circle);
	if (!shape) { shape = new DrawShapeItem(Shape_Circle); m_shapes.append(shape); }
	shape->circle = { cx, cy, radius };

	clearSceneShape();
	m_activeShape = shape;
	rebuildShapeOnScene(shape);
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
	shape->rect = { cx, cy, w, h };

	clearSceneShape();
	m_activeShape = shape;
	rebuildShapeOnScene(shape);
}

void ImageCanvasView::commitRotatedRect()
{
	if (!m_ghostRect) return;
	QRectF gr = m_ghostRect->rect();
	double cx = gr.center().x();
	double cy = gr.center().y();
	double w  = gr.width();
	double h  = gr.height();
	if (w < 3 || h < 3) return;

	DrawShapeItem* shape = findShapeByType(Shape_RotateRect);
	if (!shape) { shape = new DrawShapeItem(Shape_RotateRect); m_shapes.append(shape); }
	shape->rotatedRect = { cx, cy, w, h, 0.0 };

	clearSceneShape();
	m_activeShape = shape;
	rebuildShapeOnScene(shape);
}

// ===== 形状查询 =====

DrawShapeItem* ImageCanvasView::findShapeByType(DrawShapeType type)
{
	for (auto* s : m_shapes)
		if (s->type == type) return s;
	return nullptr;
}

// ===== 图形构建 =====

QGraphicsItem* ImageCanvasView::buildShapeItem(const DrawShapeItem& shape)
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
	case Shape_RotateRect:
	{
		double hw = shape.rotatedRect.w / 2.0, hh = shape.rotatedRect.h / 2.0;
		double cx = shape.rotatedRect.cx, cy = shape.rotatedRect.cy;
		double rad = qDegreesToRadians(shape.rotatedRect.angle);
		QPolygonF worldPoly;
		QPointF polyPts[4] = { {-hw,-hh},{hw,-hh},{hw,hh},{-hw,hh} };
		for (auto& pt : polyPts)
		{
			double rx = pt.x() * qCos(rad) - pt.y() * qSin(rad);
			double ry = pt.x() * qSin(rad) + pt.y() * qCos(rad);
			worldPoly << QPointF(rx + cx, ry + cy);
		}
		QGraphicsPolygonItem* p = new QGraphicsPolygonItem(worldPoly);
		p->setZValue(80);
		item = p;
		break;
	}
	case Shape_Circle:
	{
		double r = shape.circle.r;
		QGraphicsEllipseItem* e = new QGraphicsEllipseItem(
			shape.circle.cx - r, shape.circle.cy - r, r * 2, r * 2);
		e->setZValue(80);
		item = e;
		break;
	}
	default: break;
	}
	return item;
}

// ===== 控制点 & 中心十字 =====

void ImageCanvasView::showHandles(DrawShapeItem* shape)
{
	if (!shape) return;

	for (auto* h : shape->handles) { m_scene->removeItem(h); delete h; }
	shape->handles.clear();
	if (shape->rotateHandle) { m_scene->removeItem(shape->rotateHandle); delete shape->rotateHandle; shape->rotateHandle = nullptr; }
	if (shape->centerCrossH) { m_scene->removeItem(shape->centerCrossH); delete shape->centerCrossH; shape->centerCrossH = nullptr; }
	if (shape->centerCrossV) { m_scene->removeItem(shape->centerCrossV); delete shape->centerCrossV; shape->centerCrossV = nullptr; }

	double cx = 0, cy = 0, halfW = 0, halfH = 0;
	double angle = 0;
	bool isRotatedRect = false;
	bool isCircle = false;

	switch (shape->type)
	{
	case Shape_Rect:
		cx = shape->rect.cx; cy = shape->rect.cy;
		halfW = shape->rect.w / 2.0; halfH = shape->rect.h / 2.0;
		break;
	case Shape_RotateRect:
		cx = shape->rotatedRect.cx; cy = shape->rotatedRect.cy;
		halfW = shape->rotatedRect.w / 2.0; halfH = shape->rotatedRect.h / 2.0;
		angle = shape->rotatedRect.angle;
		isRotatedRect = true;
		break;
	case Shape_Circle:
		cx = shape->circle.cx; cy = shape->circle.cy;
		halfW = shape->circle.r; halfH = shape->circle.r;
		isCircle = true;
		break;
	}
	if (halfW <= 0 && halfH <= 0) return;

	if (isCircle)
	{
		// 圆形：上下左右 4 个控制点
		QPointF pts[4] = { {halfW,0},{0,-halfH},{-halfW,0},{0,halfH} };
		for (int i = 0; i < 4; ++i)
		{
			QPointF pt = pts[i] + QPointF(cx, cy);
			QGraphicsEllipseItem* h = new QGraphicsEllipseItem(pt.x()-kHandleRadius, pt.y()-kHandleRadius, kHandleRadius*2, kHandleRadius*2);
			h->setPen(QPen(Qt::white, 1));
			h->setBrush(QColor(0, 180, 255));
			h->setZValue(100);
			h->setData(0, i);
			h->setData(1, 0);
			h->setAcceptHoverEvents(true);
			m_scene->addItem(h);
			shape->handles.append(h);
		}

		// 外接矩形（灰色虚线，颜色区分）
		double r = shape->circle.r;
		shape->circumRect = new QGraphicsRectItem(cx-r, cy-r, r*2, r*2);
		QPen circumPen(QColor(160, 160, 160), 1, Qt::DashLine);
		circumPen.setCosmetic(true);
		shape->circumRect->setPen(circumPen);
		shape->circumRect->setBrush(Qt::NoBrush);
		shape->circumRect->setZValue(75);
		m_scene->addItem(shape->circumRect);
	}
	else if (isRotatedRect)
	{
		QPointF localPts[8] = {
			{-halfW,-halfH},{0,-halfH},{halfW,-halfH},
			{halfW,0},{halfW,halfH},{0,halfH},{-halfW,halfH},{-halfW,0}
		};
		for (int i = 0; i < 8; ++i)
		{
			double rx = localPts[i].x() * qCos(qDegreesToRadians(angle)) - localPts[i].y() * qSin(qDegreesToRadians(angle));
			double ry = localPts[i].x() * qSin(qDegreesToRadians(angle)) + localPts[i].y() * qCos(qDegreesToRadians(angle));
			QPointF wp(rx + cx, ry + cy);
			QGraphicsEllipseItem* h = new QGraphicsEllipseItem(wp.x()-kHandleRadius, wp.y()-kHandleRadius, kHandleRadius*2, kHandleRadius*2);
			h->setPen(QPen(Qt::white, 1));
			h->setBrush(QColor(0, 180, 255));
			h->setZValue(100);
			h->setData(0, i);
			h->setData(1, 0);
			h->setAcceptHoverEvents(true);
			m_scene->addItem(h);
			shape->handles.append(h);
		}
	}
	else
	{
		QPointF localPts[8] = {
			{-halfW,-halfH},{0,-halfH},{halfW,-halfH},
			{halfW,0},{halfW,halfH},{0,halfH},{-halfW,halfH},{-halfW,0}
		};
		for (int i = 0; i < 8; ++i)
		{
			QPointF pt = localPts[i] + QPointF(cx, cy);
			QGraphicsEllipseItem* h = new QGraphicsEllipseItem(pt.x()-kHandleRadius, pt.y()-kHandleRadius, kHandleRadius*2, kHandleRadius*2);
			h->setPen(QPen(Qt::white, 1));
			h->setBrush(QColor(0, 180, 255));
			h->setZValue(100);
			h->setData(0, i);
			h->setData(1, 0);
			h->setAcceptHoverEvents(true);
			m_scene->addItem(h);
			shape->handles.append(h);
		}
	}

	if (isRotatedRect)
	{
		double rad = qDegreesToRadians(angle);
		double mx = 0 * qCos(rad) - (-halfH) * qSin(rad);
		double my = 0 * qSin(rad) + (-halfH) * qCos(rad);
		QPointF midTop(mx + cx, my + cy);

		double stickLen = 30.0;
		double nx = qSin(rad), ny = -qCos(rad);  // 上边的外法线（垂直于矩形上边向外）
		QPointF rotPt(midTop.x() + nx*stickLen, midTop.y() + ny*stickLen);

		shape->rotateStickLine = new QGraphicsLineItem(midTop.x(), midTop.y(), rotPt.x(), rotPt.y());
		QPen stickPen(QColor(220,220,220), 1);
		stickPen.setCosmetic(true);
		shape->rotateStickLine->setPen(stickPen);
		shape->rotateStickLine->setZValue(95);
		m_scene->addItem(shape->rotateStickLine);

		shape->rotateHandle = new QGraphicsEllipseItem(rotPt.x()-kHandleRadius, rotPt.y()-kHandleRadius, kHandleRadius*2, kHandleRadius*2);
		shape->rotateHandle->setPen(QPen(QColor(255,200,0), 2));
		shape->rotateHandle->setBrush(QColor(255,180,0));
		shape->rotateHandle->setZValue(100);
		shape->rotateHandle->setData(0, 0);
		shape->rotateHandle->setData(1, 1);
		shape->rotateHandle->setAcceptHoverEvents(true);
		m_scene->addItem(shape->rotateHandle);
	}

	// 中心十字（所有形状都画）
	{
		shape->centerCrossH = new QGraphicsLineItem();
		shape->centerCrossV = new QGraphicsLineItem();
		QPen crossPen(QColor(0,200,0), 1);
		crossPen.setCosmetic(true);
		shape->centerCrossH->setPen(crossPen);
		shape->centerCrossV->setPen(crossPen);
		shape->centerCrossH->setZValue(90);
		shape->centerCrossV->setZValue(90);
		shape->centerCrossH->setLine(cx-kCrossLen, cy, cx+kCrossLen, cy);
		shape->centerCrossV->setLine(cx, cy-kCrossLen, cx, cy+kCrossLen);
		m_scene->addItem(shape->centerCrossH);
		m_scene->addItem(shape->centerCrossV);
	}
}

void ImageCanvasView::updateHandlePositions(DrawShapeItem* shape)
{
	if (!shape) return;

	if (shape->type == Shape_Rect)
	{
		double cx = shape->rect.cx, cy = shape->rect.cy;
		double hw = shape->rect.w / 2.0, hh = shape->rect.h / 2.0;

		QPointF localPts[8] = { {-hw,-hh},{0,-hh},{hw,-hh},{hw,0},{hw,hh},{0,hh},{-hw,hh},{-hw,0} };
		for (int i = 0; i < shape->handles.size() && i < 8; ++i)
			moveHandle(shape->handles[i], localPts[i] + QPointF(cx, cy));

		if (auto* r = dynamic_cast<QGraphicsRectItem*>(shape->item))
			r->setRect(cx - hw, cy - hh, shape->rect.w, shape->rect.h);
		if (shape->centerCrossH) shape->centerCrossH->setLine(cx-kCrossLen, cy, cx+kCrossLen, cy);
		if (shape->centerCrossV) shape->centerCrossV->setLine(cx, cy-kCrossLen, cx, cy+kCrossLen);
	}
	else if (shape->type == Shape_Circle)
	{
		double cx = shape->circle.cx, cy = shape->circle.cy;
		double r = shape->circle.r;
		QPointF pts[4] = { {r,0},{0,-r},{-r,0},{0,r} };
		for (int i = 0; i < 4 && i < shape->handles.size(); ++i)
			moveHandle(shape->handles[i], pts[i] + QPointF(cx, cy));
		if (auto* e = dynamic_cast<QGraphicsEllipseItem*>(shape->item))
			e->setRect(cx-r, cy-r, r*2, r*2);
		if (shape->circumRect)
			shape->circumRect->setRect(cx-r, cy-r, r*2, r*2);
		if (shape->centerCrossH) shape->centerCrossH->setLine(cx-kCrossLen, cy, cx+kCrossLen, cy);
		if (shape->centerCrossV) shape->centerCrossV->setLine(cx, cy-kCrossLen, cx, cy+kCrossLen);
	}
	else if (shape->type == Shape_RotateRect)
	{
		double cx = shape->rotatedRect.cx, cy = shape->rotatedRect.cy;
		double hw = shape->rotatedRect.w / 2.0, hh = shape->rotatedRect.h / 2.0;
		double rad = qDegreesToRadians(shape->rotatedRect.angle);

		QPointF localPts[8] = { {-hw,-hh},{0,-hh},{hw,-hh},{hw,0},{hw,hh},{0,hh},{-hw,hh},{-hw,0} };
		for (int i = 0; i < 8 && i < shape->handles.size(); ++i)
		{
			double rx = localPts[i].x() * qCos(rad) - localPts[i].y() * qSin(rad);
			double ry = localPts[i].x() * qSin(rad) + localPts[i].y() * qCos(rad);
			moveHandle(shape->handles[i], QPointF(rx+cx, ry+cy));
		}

		if (shape->rotateHandle)
		{
			double mx = 0 * qCos(rad) - (-hh) * qSin(rad);
			double my = 0 * qSin(rad) + (-hh) * qCos(rad);
			QPointF midTop(mx+cx, my+cy);
			double stickLen = 30.0;
			QPointF rotPt(midTop.x() + qSin(rad)*stickLen, midTop.y() - qCos(rad)*stickLen);
			moveHandle(shape->rotateHandle, rotPt);
			if (shape->rotateStickLine)
				shape->rotateStickLine->setLine(midTop.x(), midTop.y(), rotPt.x(), rotPt.y());
		}

		if (auto* p = dynamic_cast<QGraphicsPolygonItem*>(shape->item))
		{
			QPolygonF worldPoly;
			QPointF polyPts[4] = { {-hw,-hh},{hw,-hh},{hw,hh},{-hw,hh} };
			for (auto& pt : polyPts)
			{
				double rx = pt.x() * qCos(rad) - pt.y() * qSin(rad);
				double ry = pt.x() * qSin(rad) + pt.y() * qCos(rad);
				worldPoly << QPointF(rx+cx, ry+cy);
			}
			p->setPolygon(worldPoly);
		}
		if (shape->centerCrossH) shape->centerCrossH->setLine(cx-kCrossLen, cy, cx+kCrossLen, cy);
		if (shape->centerCrossV) shape->centerCrossV->setLine(cx, cy-kCrossLen, cx, cy+kCrossLen);
	}
}

// ===== 碰撞检测 =====

bool ImageCanvasView::isPointInShape(const DrawShapeItem* shape, const QPointF& scenePos) const
{
	if (!shape) return false;
	switch (shape->type)
	{
	case Shape_Rect:
	{
		double left   = shape->rect.cx - shape->rect.w / 2.0;
		double right  = shape->rect.cx + shape->rect.w / 2.0;
		double top    = shape->rect.cy - shape->rect.h / 2.0;
		double bottom = shape->rect.cy + shape->rect.h / 2.0;
		return scenePos.x() >= left && scenePos.x() <= right && scenePos.y() >= top && scenePos.y() <= bottom;
	}
	case Shape_RotateRect:
	{
		double rad = -qDegreesToRadians(shape->rotatedRect.angle);
		double dx  = scenePos.x() - shape->rotatedRect.cx;
		double dy  = scenePos.y() - shape->rotatedRect.cy;
		double lx  = dx * qCos(rad) - dy * qSin(rad);
		double ly  = dx * qSin(rad) + dy * qCos(rad);
		double hw  = shape->rotatedRect.w / 2.0;
		double hh  = shape->rotatedRect.h / 2.0;
		return lx >= -hw && lx <= hw && ly >= -hh && ly <= hh;
	}
	case Shape_Circle:
	{
		double d2 = (scenePos.x()-shape->circle.cx)*(scenePos.x()-shape->circle.cx)
		          + (scenePos.y()-shape->circle.cy)*(scenePos.y()-shape->circle.cy);
		return d2 <= shape->circle.r * shape->circle.r;
	}
	default: return false;
	}
}

QGraphicsEllipseItem* ImageCanvasView::handleAt(const QPointF& scenePos) const
{
	if (!m_activeShape) return nullptr;
	if (m_activeShape->rotateHandle && m_activeShape->rotateHandle->isVisible()
		&& m_activeShape->rotateHandle->contains(m_activeShape->rotateHandle->mapFromScene(scenePos)))
		return m_activeShape->rotateHandle;
	for (auto* h : m_activeShape->handles)
		if (h->isVisible() && h->contains(h->mapFromScene(scenePos))) return h;
	return nullptr;
}

// ===== Handle 拖拽 =====

void ImageCanvasView::applyHandleHover(int handleIndex, bool hover)
{
	if (!m_activeShape) return;
	if (handleIndex < 0 || handleIndex >= m_activeShape->handles.size()) return;
	setHandleHover(m_activeShape->handles[handleIndex], hover);
}

void ImageCanvasView::clearAllHandleHover()
{
	if (!m_activeShape) return;
	for (int i = 0; i < m_activeShape->handles.size(); ++i) applyHandleHover(i, false);
}

void ImageCanvasView::updateRectFromHandle(const QPointF& scenePos)
{
	if (!m_dragShape || m_dragHandleIndex < 0) return;

	double sx = scenePos.x(), sy = scenePos.y();
	double& cx = m_dragShape->rect.cx;
	double& cy = m_dragShape->rect.cy;
	double& w  = m_dragShape->rect.w;
	double& h  = m_dragShape->rect.h;

	double left0   = m_dragStartRect.cx - m_dragStartRect.w / 2.0;
	double right0  = m_dragStartRect.cx + m_dragStartRect.w / 2.0;
	double top0    = m_dragStartRect.cy - m_dragStartRect.h / 2.0;
	double bottom0 = m_dragStartRect.cy + m_dragStartRect.h / 2.0;

	double left = left0, right = right0, top = top0, bottom = bottom0;

	switch (m_dragHandleIndex)
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

	updateHandlePositions(m_dragShape);
}

void ImageCanvasView::updateRotatedRectFromHandle(const QPointF& scenePos)
{
	if (!m_dragShape) return;

	if (m_isRotating)
	{
		// 旋转手柄 → 计算中心到鼠标的角度
		double cx = m_dragShape->rotatedRect.cx;
		double cy = m_dragShape->rotatedRect.cy;
		double angle = qRadiansToDegrees(qAtan2(scenePos.y()-cy, scenePos.x()-cx)) + 90.0;
		m_dragShape->rotatedRect.angle = angle;
		updateHandlePositions(m_dragShape);
	}
	else if (m_isDraggingHandle)
	{
		// 将鼠标点逆旋转到局部坐标系（相对于拖拽起始中心）
		double angle = m_dragStartRect.cx;  // m_dragStartRect.cx 存的是 m_dragStartAngle 对应的原始角度... 
		// 不对，m_dragStartRect 是 Rect 类型，存的是起始时刻的 cx,cy,w,h
		// 对于旋转矩形，m_dragStartRect 存的是拖拽开始时的 rotatedRect 的 cx,cy,w,h
		double startAngle = m_dragStartAngle;  // 拖拽起始角度
		double rad   = -qDegreesToRadians(startAngle);
		double sx    = scenePos.x();
		double sy    = scenePos.y();
		double dx    = sx - m_dragStartRect.cx;
		double dy    = sy - m_dragStartRect.cy;
		double lx    = dx * qCos(rad) - dy * qSin(rad);
		double ly    = dx * qSin(rad) + dy * qCos(rad);

		double startW = m_dragStartRect.w, startH = m_dragStartRect.h;
		double left0   = -startW / 2.0;
		double right0  =  startW / 2.0;
		double top0    = -startH / 2.0;
		double bottom0 =  startH / 2.0;

		double left = left0, right = right0, top = top0, bottom = bottom0;

		// 和普通矩形一样的 8 方向拖拽
		switch (m_dragHandleIndex)
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
		m_dragShape->rotatedRect.cx = m_dragStartRect.cx + newCxLocal*cosA - newCyLocal*sinA;
		m_dragShape->rotatedRect.cy = m_dragStartRect.cy + newCxLocal*sinA + newCyLocal*cosA;
		m_dragShape->rotatedRect.w  = newW;
		m_dragShape->rotatedRect.h  = newH;

		updateHandlePositions(m_dragShape);
	}
}

// ===== 圆形 Handle 拖拽 =====
void ImageCanvasView::updateCircleFromHandle(const QPointF& scenePos)
{
	if (!m_dragShape || m_dragHandleIndex < 0 || m_dragHandleIndex >= 4) return;

	// 圆形 4 个控制点：0=右, 1=上, 2=左, 3=下
	// 拖拽时改变半径，保持中心
	double cx0 = m_dragStartRect.cx, cy0 = m_dragStartRect.cy;
	double r0  = m_dragStartRect.w / 2.0;

	double dx = scenePos.x() - cx0;
	double dy = scenePos.y() - cy0;

	double newR = 0;
	switch (m_dragHandleIndex)
	{
	case 0: newR = dx;                    break; // 右边 → r = dx
	case 1: newR = -dy;                   break; // 上边 → r = -dy
	case 2: newR = -dx;                   break; // 左边 → r = -dx
	case 3: newR = dy;                    break; // 下边 → r = dy
	}
	newR = std::max(newR, 1.5);

	m_dragShape->circle.cx = cx0;
	m_dragShape->circle.cy = cy0;
	m_dragShape->circle.r  = newR;

	updateHandlePositions(m_dragShape);
}
