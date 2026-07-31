#include "ImageCanvasView.h"

#include <QPainterPath>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QOpenGLWidget>
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

static inline double normAngle360(double deg) { while(deg<0)deg+=360.0; while(deg>=360.0)deg-=360.0; return deg; }

// 三点外接圆（外心 + 半径）
static bool circumcircle(const QPointF& p1, const QPointF& p2, const QPointF& p3, QPointF& c, double& r) {
	double x1=p1.x(),y1=p1.y(), x2=p2.x(),y2=p2.y(), x3=p3.x(),y3=p3.y();
	double d=2.0*(x1*(y2-y3)+x2*(y3-y1)+x3*(y1-y2));
	if(std::abs(d)<1e-12) return false;
	c=QPointF(((x1*x1+y1*y1)*(y2-y3)+(x2*x2+y2*y2)*(y3-y1)+(x3*x3+y3*y3)*(y1-y2))/d,
	          ((x1*x1+y1*y1)*(x3-x2)+(x2*x2+y2*y2)*(x1-x3)+(x3*x3+y3*y3)*(x2-x1))/d);
	r=std::sqrt((x1-c.x())*(x1-c.x())+(y1-c.y())*(y1-c.y())); return true;
}

static bool solveConcentricArc(const QPointF& A, const QPointF& B, double r1, double r2, QPointF& O, QPointF& P);

// ===== 构造/析构 =====

ImageCanvasView::ImageCanvasView(QWidget* parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	m_scene = new QGraphicsScene(this);
	ui.canvas_view_main->setScene(m_scene);
	ui.canvas_view_main->setViewport(new QOpenGLWidget());
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

	// OpenGL QPen 残影吸收线（z=999，0.01px透明，终身驻留）
	m_spotAbsorber = m_scene->addLine(0, 0, 0, 0.01, QPen(QColor(0,0,0,1), 1));
	m_spotAbsorber->setZValue(999);

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
		// 第一步按下时，吸收线移到点击位置吸收残影
		if (event->type() == QEvent::MouseButtonPress && m_drawStep == 0)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::LeftButton && m_spotAbsorber)
			{
				m_spotAbsorber->setLine(scenePos.x(), scenePos.y(), scenePos.x(), scenePos.y()+0.01);
				ui.canvas_view_main->viewport()->repaint();
			}
		}
		// ===== 四点同心双圆弧 =====
		// 点1=起点A, 点2=终点B, 点3=锁r1, 点4=锁r2
		if (m_currentShape == Shape_Arc)
		{
			if (event->type() == QEvent::MouseButtonPress)
			{
				QMouseEvent* me = static_cast<QMouseEvent*>(event);
				if (me->button() == Qt::LeftButton)
				{
					if (m_drawStep == 0) {
						m_circlePt1 = scenePos; m_drawStep = 1;
						double mrkR = 3.0;
						m_circleMarker1 = new QGraphicsEllipseItem(m_circlePt1.x()-mrkR,m_circlePt1.y()-mrkR,mrkR*2,mrkR*2);
						m_circleMarker1->setPen(Qt::NoPen);m_circleMarker1->setBrush(QColor(255,80,80));m_circleMarker1->setZValue(100);m_scene->addItem(m_circleMarker1);
					}else if(m_drawStep == 1){
						m_circlePt2 = scenePos; m_drawStep = 2;
						double mrkR = 3.0;
						m_circleMarker2 = new QGraphicsEllipseItem(m_circlePt2.x()-mrkR,m_circlePt2.y()-mrkR,mrkR*2,mrkR*2);
						m_circleMarker2->setPen(Qt::NoPen);m_circleMarker2->setBrush(QColor(255,80,80));m_circleMarker2->setZValue(100);m_scene->addItem(m_circleMarker2);
					}else if(m_drawStep == 2){
						// 第三点锁r1
						QPointF A(m_circlePt1),B(m_circlePt2),C(scenePos),O;double r1;
						if(circumcircle(A,B,C,O,r1)&&r1>1.5){
							double aA=normAngle360(qRadiansToDegrees(std::atan2(A.y()-O.y(),A.x()-O.x())));
							double aB=normAngle360(qRadiansToDegrees(std::atan2(B.y()-O.y(),B.x()-O.x())));
							double aC=normAngle360(qRadiansToDegrees(std::atan2(C.y()-O.y(),C.x()-O.x())));
							double sf=aB-aA;if(sf<=0)sf+=360.0;double rc=aC-aA;if(rc<0)rc+=360.0;
							double sp=(rc<=sf)?sf:(sf-360.0);
							m_anchorPoint=O;m_arcStartAngle=aA;m_arcEndAngle=sp;
							m_dragStartRect.cx=O.x();m_dragStartRect.cy=O.y();m_dragStartRect.w=r1;
							m_drawStep=3;
						}
					}else if(m_drawStep == 3){
						commitArc();
						if(m_ghostArcPath){m_scene->removeItem(m_ghostArcPath);delete m_ghostArcPath;m_ghostArcPath=nullptr;}
						if(m_ghostEllipse2){m_scene->removeItem(m_ghostEllipse2);delete m_ghostEllipse2;m_ghostEllipse2=nullptr;}
						if(m_circleMarker1){m_scene->removeItem(m_circleMarker1);delete m_circleMarker1;m_circleMarker1=nullptr;}
						if(m_circleMarker2){m_scene->removeItem(m_circleMarker2);delete m_circleMarker2;m_circleMarker2=nullptr;}
						hideDrawModeOverlay();m_mode=Mode_None;m_drawStep=0;ui.canvas_view_main->setCursor(Qt::ArrowCursor);
					}
					event->accept();return true;
				}else if(me->button()==Qt::RightButton){stopDraw();event->accept();return true;}
			}
			else if(event->type()==QEvent::MouseMove&&m_drawStep>=2&&m_drawStep<=3)
			{
				QPointF A(m_circlePt1),B(m_circlePt2);
				if(m_drawStep==2){
					QPointF C(scenePos),O;double r;
					if(circumcircle(A,B,C,O,r)&&r>1.5){
						double aA=normAngle360(qRadiansToDegrees(std::atan2(A.y()-O.y(),A.x()-O.x())));
						double aB=normAngle360(qRadiansToDegrees(std::atan2(B.y()-O.y(),B.x()-O.x())));
						double aC=normAngle360(qRadiansToDegrees(std::atan2(C.y()-O.y(),C.x()-O.x())));
						double sf=aB-aA;if(sf<=0)sf+=360.0;double rc=aC-aA;if(rc<0)rc+=360.0;
						double sp=(rc<=sf)?sf:(sf-360.0);
						if(!m_ghostArcPath){m_ghostArcPath=new QGraphicsPathItem();QPen p(QColor(0,180,255),m_penWidth,Qt::DashLine);p.setCosmetic(true);m_ghostArcPath->setPen(p);m_ghostArcPath->setZValue(99);m_scene->addItem(m_ghostArcPath);}
						QPainterPath ph;const int N=128;double ar=qDegreesToRadians(aA),sr=qDegreesToRadians(sp);
						ph.moveTo(O.x()+r*qCos(ar),O.y()+r*qSin(ar));
						for(int i=1;i<=N;++i){double ang=ar+sr*i/N;ph.lineTo(O.x()+r*qCos(ang),O.y()+r*qSin(ang));}
						m_ghostArcPath->setPath(ph);
					}
				}else{
					QPointF O=m_anchorPoint;double aA=m_arcStartAngle,sp=m_arcEndAngle,r1=m_dragStartRect.w;
					double r2=std::sqrt((scenePos.x()-O.x())*(scenePos.x()-O.x())+(scenePos.y()-O.y())*(scenePos.y()-O.y()));r2=std::max(r2,2.0);
					double rO=std::max(r1,r2),rI=std::min(r1,r2);
					if(!m_ghostArcPath){m_ghostArcPath=new QGraphicsPathItem();QPen p(QColor(0,180,255),m_penWidth,Qt::DashLine);p.setCosmetic(true);m_ghostArcPath->setPen(p);m_ghostArcPath->setZValue(99);m_scene->addItem(m_ghostArcPath);}
					double aR=qDegreesToRadians(aA),sR=qDegreesToRadians(sp);QPainterPath ph;const int N=128;
					ph.moveTo(O.x()+rO*qCos(aR),O.y()+rO*qSin(aR));
					for(int i=1;i<=N;++i){double ang=aR+sR*i/N;ph.lineTo(O.x()+rO*qCos(ang),O.y()+rO*qSin(ang));}
					double eR=aR+sR;ph.lineTo(O.x()+rI*qCos(eR),O.y()+rI*qSin(eR));
					for(int i=N;i>=0;--i){double ang=aR+sR*i/N;ph.lineTo(O.x()+rI*qCos(ang),O.y()+rI*qSin(ang));}
					ph.closeSubpath();m_ghostArcPath->setPath(ph);
					m_dragStartRect.h=r2;
				}
				event->accept();return true;
			}
			return false;
		}

		// ===== 多边形 =====
		if (m_currentShape == Shape_Polygon)
		{
			if (event->type() == QEvent::MouseButtonPress)
			{
				QMouseEvent* me = static_cast<QMouseEvent*>(event);
				if (me->button() == Qt::LeftButton) {
					// 点击第一个顶点附近且 ≥3 点 → 闭合
					if (m_tempPolyPts.size() >= 3) {
						QPointF d = scenePos - m_tempPolyPts.first();
						if (std::sqrt(d.x()*d.x()+d.y()*d.y()) < 10.0) {
							commitPolygon(); event->accept(); return true;
						}
					}
					m_tempPolyPts.append(scenePos);
					double mrkR = 3.0;
					auto* mk = new QGraphicsEllipseItem(scenePos.x()-mrkR,scenePos.y()-mrkR,mrkR*2,mrkR*2);
					mk->setPen(Qt::NoPen);mk->setBrush(QColor(255,80,80));mk->setZValue(100);m_scene->addItem(mk);m_circleMarkers.append(mk);
					m_drawStep = 1;
				}
				event->accept(); return true;
			}
			else if (event->type() == QEvent::MouseMove && m_drawStep == 1 && m_tempPolyPts.size() >= 1)
			{
				// 已绘边预览
				if (!m_ghostArcPath) { m_ghostArcPath=new QGraphicsPathItem(); QPen pen(QColor(0,180,255),m_penWidth,Qt::DashLine); pen.setCosmetic(true); m_ghostArcPath->setPen(pen); m_ghostArcPath->setZValue(99); m_scene->addItem(m_ghostArcPath); }
				QPainterPath edges; edges.moveTo(m_tempPolyPts.first());
				for(int i=1;i<m_tempPolyPts.size();++i) edges.lineTo(m_tempPolyPts[i]);
				edges.lineTo(scenePos);
				m_ghostArcPath->setPath(edges);

				// 靠近起点时高亮闭合线
				QPointF d = scenePos - m_tempPolyPts.first();
				double dist = std::sqrt(d.x()*d.x()+d.y()*d.y());
				if (m_ghostPolyLine) { m_ghostPolyLine->setVisible(dist < 15.0 && m_tempPolyPts.size()>=3); }
				else if (dist < 15.0 && m_tempPolyPts.size()>=3) {
					m_ghostPolyLine = new QGraphicsLineItem(); QPen p(QColor(255,200,0),m_penWidth+1,Qt::DashLine); p.setCosmetic(true);
					m_ghostPolyLine->setPen(p); m_ghostPolyLine->setZValue(100); m_scene->addItem(m_ghostPolyLine);
				}
				if (m_ghostPolyLine && m_ghostPolyLine->isVisible())
					m_ghostPolyLine->setLine(QLineF(m_tempPolyPts.last(), m_tempPolyPts.first()));

				event->accept(); return true;
			}
			return false;
		}

		if (m_currentShape == Shape_Circle || m_currentShape == Shape_Ring)
		{
			// 圆环：四点构造，前三点确定圆（同圆形），第四点决定另一个圆的半径
			// step0→step1: 点1
			// step1→step2: 点1+点2+鼠标=三角形外心画ghost圆
			// step2→step3: 确定第一个圆，第四点决定第二个半径
			// step3→确认: 提交
			if (event->type() == QEvent::MouseButtonPress)
			{
				QMouseEvent* me = static_cast<QMouseEvent*>(event);
				if (me->button() == Qt::LeftButton)
				{
					if (m_drawStep == 0)
					{
						m_circlePt1 = scenePos;
						m_drawStep = 1;
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
						// 第三步：三点外心确定圆
						// 先计算并固定第一个圆的半径和中心
						double x1=m_circlePt1.x(), y1=m_circlePt1.y();
						double x2=m_circlePt2.x(), y2=m_circlePt2.y();
						double x3=scenePos.x(),   y3=scenePos.y();
						double d = 2.0*(x1*(y2-y3) + x2*(y3-y1) + x3*(y1-y2));
						if (std::abs(d) < 1e-6) { event->accept(); return true; }
						double ux = ((x1*x1+y1*y1)*(y2-y3) + (x2*x2+y2*y2)*(y3-y1) + (x3*x3+y3*y3)*(y1-y2)) / d;
						double uy = ((x1*x1+y1*y1)*(x3-x2) + (x2*x2+y2*y2)*(x1-x3) + (x3*x3+y3*y3)*(x2-x1)) / d;
						double r  = std::sqrt((x1-ux)*(x1-ux) + (y1-uy)*(y1-uy));

						if (m_currentShape == Shape_Circle)
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
						else // Shape_Ring: 进入第四步
						{
							// 保存第一个圆的中心和半径
							m_anchorPoint = QPointF(ux, uy);
							m_dragStartRect.cx = ux;
							m_dragStartRect.cy = uy;
							m_dragStartRect.w = r * 2;  // 存第一个半径在 w 里
							m_drawStep = 3;
							// 更新 ghost ellipse 为已固定的圆（实线预览）
							if (m_ghostEllipse)
							{
								m_ghostEllipse->setRect(ux-r, uy-r, r*2, r*2);
								QPen solidPen(QColor(0, 180, 255), m_penWidth);
								solidPen.setCosmetic(true);
								m_ghostEllipse->setPen(solidPen);
							}
							// 创建第二个 ghost ellipse（内圆预览）
							if (!m_ghostEllipse2)
							{
								m_ghostEllipse2 = new QGraphicsEllipseItem();
								QPen ghostPen2(QColor(255, 140, 0), m_penWidth, Qt::DashLine);
								ghostPen2.setCosmetic(true);
								m_ghostEllipse2->setPen(ghostPen2);
								m_ghostEllipse2->setBrush(QColor(255, 140, 0, 20));
								m_ghostEllipse2->setZValue(98);
								m_scene->addItem(m_ghostEllipse2);
							}
						}
					}
					else if (m_drawStep == 3 && m_currentShape == Shape_Ring)
					{
						// 第四步：确定第二个圆的半径，提交圆环
						commitRing();
						if (m_ghostEllipse) { m_scene->removeItem(m_ghostEllipse); delete m_ghostEllipse; m_ghostEllipse = nullptr; }
						if (m_ghostEllipse2) { m_scene->removeItem(m_ghostEllipse2); delete m_ghostEllipse2; m_ghostEllipse2 = nullptr; }
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
			else if (event->type() == QEvent::MouseMove && m_drawStep >= 1 && m_drawStep <= 3)
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
				else if (m_drawStep == 2)
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
				else if (m_drawStep == 3 && m_currentShape == Shape_Ring)
				{
					// 第四步预览第二个圆：中心已定，鼠标距中心的距离就是第二个半径
					double cx = m_anchorPoint.x(), cy = m_anchorPoint.y();
					double r2 = std::sqrt((scenePos.x()-cx)*(scenePos.x()-cx) + (scenePos.y()-cy)*(scenePos.y()-cy));
					if (m_ghostEllipse2)
						m_ghostEllipse2->setRect(cx-r2, cy-r2, r2*2, r2*2);
				}
				event->accept();
				return true;
			}
			return false;
		}

			// 通用两步绘制（矩形、旋转矩形、椭圆等）
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
						// 椭圆的 ghost ellipse
						if (m_currentShape == Shape_Ellipse)
						{
							m_ghostEllipse = new QGraphicsEllipseItem();
							QPen ghostEPen(QColor(0, 180, 255), m_penWidth, Qt::DashLine);
							ghostEPen.setCosmetic(true);
							m_ghostEllipse->setPen(ghostEPen);
							m_ghostEllipse->setBrush(QColor(0, 180, 255, 20));
							m_ghostEllipse->setZValue(99);
							m_ghostEllipse->setRect(m_ghostRect->rect());
							m_scene->addItem(m_ghostEllipse);
						}
					}
					else if (m_drawStep == 1)
					{
						commitShapeGeneric(m_currentShape);
						if (m_ghostRect) { m_scene->removeItem(m_ghostRect); delete m_ghostRect; m_ghostRect = nullptr; }
						if (m_ghostEllipse) { m_scene->removeItem(m_ghostEllipse); delete m_ghostEllipse; m_ghostEllipse = nullptr; }
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
				{
					updateGhostRect(scenePos);
					if (m_currentShape == Shape_Ellipse && m_ghostEllipse && m_ghostRect)
						m_ghostEllipse->setRect(m_ghostRect->rect());
				}
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
			if (m_dragShape && m_dragShape->type == Shape_Ellipse)
				updateEllipseFromHandle(scenePos);
			else if (m_dragShape && m_dragShape->type == Shape_Arc)
				updateArcFromHandle(scenePos);
			else
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
				else if (m_dragShape && m_dragShape->type == Shape_Ellipse)
					updateEllipseFromHandle(scenePos);
				else if (m_dragShape && m_dragShape->type == Shape_Ring)
					updateRingFromHandle(scenePos);
			else if (m_dragShape && m_dragShape->type == Shape_Arc)
				updateArcFromHandle(scenePos);
			else if (m_dragShape && m_dragShape->type == Shape_Polygon)
				updatePolygonFromHandle(scenePos);
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
				if (m_activeShape->type == Shape_Rect)
				{
					m_activeShape->rect.cx = scenePos.x() - m_dragOffset.x();
					m_activeShape->rect.cy = scenePos.y() - m_dragOffset.y();
				}
				else if (m_activeShape->type == Shape_RotateRect)
				{
					m_activeShape->rotatedRect.cx = scenePos.x() - m_dragOffset.x();
					m_activeShape->rotatedRect.cy = scenePos.y() - m_dragOffset.y();
				}
				else if (m_activeShape->type == Shape_Circle)
				{
					m_activeShape->circle.cx = scenePos.x() - m_dragOffset.x();
					m_activeShape->circle.cy = scenePos.y() - m_dragOffset.y();
				}
				else if (m_activeShape->type == Shape_Ellipse)
				{
					m_activeShape->ellipse.cx = scenePos.x() - m_dragOffset.x();
					m_activeShape->ellipse.cy = scenePos.y() - m_dragOffset.y();
				}
				else if (m_activeShape->type == Shape_Ring)
				{
					m_activeShape->ring.cx = scenePos.x() - m_dragOffset.x();
					m_activeShape->ring.cy = scenePos.y() - m_dragOffset.y();
				}
				else if (m_activeShape->type == Shape_Arc)
				{
					m_activeShape->arc.cx = scenePos.x() - m_dragOffset.x();
					m_activeShape->arc.cy = scenePos.y() - m_dragOffset.y();
				}
				else if (m_activeShape->type == Shape_Polygon)
				{
					double dx = scenePos.x() - m_dragOffset.x() - m_activeShape->polygon.pts[0].x();
					double dy = scenePos.y() - m_dragOffset.y() - m_activeShape->polygon.pts[0].y();
					for(auto& pt:m_activeShape->polygon.pts){pt.rx()+=dx;pt.ry()+=dy;}
				}
				else
				{
					m_activeShape->rect.cx = scenePos.x() - m_dragOffset.x();
					m_activeShape->rect.cy = scenePos.y() - m_dragOffset.y();
				}
				syncParamPanel(m_activeShape);
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
						if (m_activeShape->type == Shape_Arc)
							m_dragStartAngle = normAngle360(m_activeShape->arc.startAngle + m_activeShape->arc.r1 / 2.0);
						else
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
					else if (m_activeShape->type == Shape_Ellipse)
						m_dragStartRect = { m_activeShape->ellipse.cx, m_activeShape->ellipse.cy,
						                    m_activeShape->ellipse.r1 * 2, m_activeShape->ellipse.r2 * 2 };
				else if (m_activeShape->type == Shape_Ring)
				{
					m_dragStartRect = { m_activeShape->ring.cx, m_activeShape->ring.cy,
					                    m_activeShape->ring.r1 * 2, m_activeShape->ring.r2 * 2 };
				}
				else if (m_activeShape->type == Shape_Arc)
				{
					m_dragStartRect = { m_activeShape->arc.cx, m_activeShape->arc.cy,
					                    m_activeShape->arc.startAngle, m_activeShape->arc.endAngle };
				}
				else if (m_activeShape->type == Shape_Polygon)
				{
					m_dragStartRect = { 0,0,0,0 };
				}
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
			else if (m_activeShape->type == Shape_Ellipse)
				m_dragOffset = QPointF(scenePos.x() - m_activeShape->ellipse.cx,
				                       scenePos.y() - m_activeShape->ellipse.cy);
			else if (m_activeShape->type == Shape_Ring)
				m_dragOffset = QPointF(scenePos.x() - m_activeShape->ring.cx,
				                       scenePos.y() - m_activeShape->ring.cy);
			else if (m_activeShape->type == Shape_Arc)
				m_dragOffset = QPointF(scenePos.x() - m_activeShape->arc.cx,
				                       scenePos.y() - m_activeShape->arc.cy);
			else if (m_activeShape->type == Shape_Polygon)
				m_dragOffset = QPointF(scenePos.x() - m_activeShape->polygon.pts[0].x(),
				                       scenePos.y() - m_activeShape->polygon.pts[0].y());
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
					// 形状本体悬停加粗
					bool nowHovered = m_activeShape && isPointInShape(m_activeShape, scenePos);
					if (nowHovered != m_isShapeHovered) {
						applyShapeHover(nowHovered);
						m_isShapeHovered = nowHovered;
					}
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
		m_isShapeHovered = false;
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
		m_isShapeHovered = false;
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
	m_isShapeHovered = false;
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
	m_isShapeHovered = false;
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
	int w = m_isShapeHovered ? m_penWidth + 2 : m_penWidth;
	QPen pen(m_colorSelected, w);
	if (auto* r = dynamic_cast<QGraphicsRectItem*>(shape->item))
		{ r->setPen(pen); r->setBrush(Qt::NoBrush); }
	else if (auto* p = dynamic_cast<QGraphicsPolygonItem*>(shape->item))
		{ p->setPen(pen); p->setBrush(Qt::NoBrush); }
	else if (auto* e = dynamic_cast<QGraphicsEllipseItem*>(shape->item))
		{ e->setPen(pen); e->setBrush(Qt::NoBrush); }
	else if (auto* pa = dynamic_cast<QGraphicsPathItem*>(shape->item))
		{ pa->setPen(pen); pa->setBrush(Qt::NoBrush); }
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
		paramNames << QStringLiteral("中心 X") << QStringLiteral("中心 Y") << QStringLiteral("半径 r1") << QStringLiteral("半径 r2");
		count = 4;
		break;
	case Shape_Arc:
		paramNames << QStringLiteral("中心 X") << QStringLiteral("中心 Y") << QStringLiteral("半径1") << QStringLiteral("半径2") << QStringLiteral("起始角") << QStringLiteral("跨度");
		count = 6; break;
	case Shape_Polygon:
		// 动态：根据实际顶点数生成参数，在下面处理
		count = 0; break;
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
			paramVals[0]=existing->ring.cx; paramVals[1]=existing->ring.cy; paramVals[2]=existing->ring.r1; paramVals[3]=existing->ring.r2;
			break;
		case Shape_Arc:
			paramVals[0]=existing->arc.cx; paramVals[1]=existing->arc.cy;
			paramVals[2]=existing->arc.rOuter; paramVals[3]=existing->arc.rInner;
			paramVals[4]=existing->arc.startAngle; paramVals[5]=existing->arc.r1;
			break;
		case Shape_Polygon:
			// 动态生成每个顶点的 X, Y 参数
			{
				int np=existing->polygon.pts.size();paramNames.clear();paramVals.clear();
				for(int i=0;i<np;++i){paramNames<<QStringLiteral("顶点")+QString::number(i)+QStringLiteral(" X")<<QStringLiteral("顶点")+QString::number(i)+QStringLiteral(" Y");paramVals<<existing->polygon.pts[i].x()<<existing->polygon.pts[i].y();}
				count=paramNames.size();
			}
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
		spin->setKeyboardTracking(false);
		connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
			int idx=ui.draw_cbox_shape_type->currentIndex();if(idx<=0)return;
			DrawShapeItem* s=findShapeByType(static_cast<DrawShapeType>(idx-1));
			if(s)liveApplyParam(s);
		});
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

// 拖拽过程中实时刷新参数面板数值
void ImageCanvasView::syncParamPanel(DrawShapeItem* shape)
{
	if(!m_paramPanel||m_paramPanel->isHidden()||!shape)return;
	switch(shape->type){
	case Shape_Rect:
		if(m_paramSpins.size()>=4){m_paramSpins[0]->blockSignals(true);m_paramSpins[1]->blockSignals(true);m_paramSpins[2]->blockSignals(true);m_paramSpins[3]->blockSignals(true);m_paramSpins[0]->setValue(shape->rect.cx);m_paramSpins[1]->setValue(shape->rect.cy);m_paramSpins[2]->setValue(shape->rect.w);m_paramSpins[3]->setValue(shape->rect.h);m_paramSpins[0]->blockSignals(false);m_paramSpins[1]->blockSignals(false);m_paramSpins[2]->blockSignals(false);m_paramSpins[3]->blockSignals(false);}break;
	case Shape_RotateRect:
		if(m_paramSpins.size()>=5){m_paramSpins[0]->blockSignals(true);m_paramSpins[1]->blockSignals(true);m_paramSpins[2]->blockSignals(true);m_paramSpins[3]->blockSignals(true);m_paramSpins[4]->blockSignals(true);m_paramSpins[0]->setValue(shape->rotatedRect.cx);m_paramSpins[1]->setValue(shape->rotatedRect.cy);m_paramSpins[2]->setValue(shape->rotatedRect.w);m_paramSpins[3]->setValue(shape->rotatedRect.h);m_paramSpins[4]->setValue(shape->rotatedRect.angle);m_paramSpins[0]->blockSignals(false);m_paramSpins[1]->blockSignals(false);m_paramSpins[2]->blockSignals(false);m_paramSpins[3]->blockSignals(false);m_paramSpins[4]->blockSignals(false);}break;
	case Shape_Circle:
		if(m_paramSpins.size()>=3){m_paramSpins[0]->blockSignals(true);m_paramSpins[1]->blockSignals(true);m_paramSpins[2]->blockSignals(true);m_paramSpins[0]->setValue(shape->circle.cx);m_paramSpins[1]->setValue(shape->circle.cy);m_paramSpins[2]->setValue(shape->circle.r);m_paramSpins[0]->blockSignals(false);m_paramSpins[1]->blockSignals(false);m_paramSpins[2]->blockSignals(false);}break;
	case Shape_Ellipse:
		if(m_paramSpins.size()>=5){m_paramSpins[0]->blockSignals(true);m_paramSpins[1]->blockSignals(true);m_paramSpins[2]->blockSignals(true);m_paramSpins[3]->blockSignals(true);m_paramSpins[4]->blockSignals(true);m_paramSpins[0]->setValue(shape->ellipse.cx);m_paramSpins[1]->setValue(shape->ellipse.cy);m_paramSpins[2]->setValue(shape->ellipse.r1);m_paramSpins[3]->setValue(shape->ellipse.r2);m_paramSpins[4]->setValue(shape->ellipse.angle);m_paramSpins[0]->blockSignals(false);m_paramSpins[1]->blockSignals(false);m_paramSpins[2]->blockSignals(false);m_paramSpins[3]->blockSignals(false);m_paramSpins[4]->blockSignals(false);}break;
	case Shape_Ring:
		if(m_paramSpins.size()>=4){m_paramSpins[0]->blockSignals(true);m_paramSpins[1]->blockSignals(true);m_paramSpins[2]->blockSignals(true);m_paramSpins[3]->blockSignals(true);m_paramSpins[0]->setValue(shape->ring.cx);m_paramSpins[1]->setValue(shape->ring.cy);m_paramSpins[2]->setValue(shape->ring.r1);m_paramSpins[3]->setValue(shape->ring.r2);m_paramSpins[0]->blockSignals(false);m_paramSpins[1]->blockSignals(false);m_paramSpins[2]->blockSignals(false);m_paramSpins[3]->blockSignals(false);}break;
	case Shape_Arc:
		if(m_paramSpins.size()>=6){m_paramSpins[0]->blockSignals(true);m_paramSpins[1]->blockSignals(true);m_paramSpins[2]->blockSignals(true);m_paramSpins[3]->blockSignals(true);m_paramSpins[4]->blockSignals(true);m_paramSpins[5]->blockSignals(true);m_paramSpins[0]->setValue(shape->arc.cx);m_paramSpins[1]->setValue(shape->arc.cy);m_paramSpins[2]->setValue(shape->arc.rOuter);m_paramSpins[3]->setValue(shape->arc.rInner);m_paramSpins[4]->setValue(shape->arc.startAngle);m_paramSpins[5]->setValue(shape->arc.r1);m_paramSpins[0]->blockSignals(false);m_paramSpins[1]->blockSignals(false);m_paramSpins[2]->blockSignals(false);m_paramSpins[3]->blockSignals(false);m_paramSpins[4]->blockSignals(false);m_paramSpins[5]->blockSignals(false);}break;
	case Shape_Polygon:
		{
			int np=(int)shape->polygon.pts.size();
			if(m_paramSpins.size()>=(int)np*2){
				for(int i=0;i<np;++i){m_paramSpins[i*2]->blockSignals(true);m_paramSpins[i*2+1]->blockSignals(true);}
				for(int i=0;i<np;++i){m_paramSpins[i*2]->setValue(shape->polygon.pts[i].x());m_paramSpins[i*2+1]->setValue(shape->polygon.pts[i].y());}
				for(int i=0;i<np;++i){m_paramSpins[i*2]->blockSignals(false);m_paramSpins[i*2+1]->blockSignals(false);}
			}
		}break;
	default:break;
	}
}

// 从参数面板读取值应用到 shape，不关闭面板
void ImageCanvasView::liveApplyParam(DrawShapeItem* shape)
{
	if(!shape||m_paramSpins.isEmpty())return;
	int n=m_paramSpins.size();
	switch(shape->type){
	case Shape_Rect:
		if(n>=4){shape->rect.cx=m_paramSpins[0]->value();shape->rect.cy=m_paramSpins[1]->value();shape->rect.w=m_paramSpins[2]->value();shape->rect.h=m_paramSpins[3]->value();}break;
	case Shape_RotateRect:
		if(n>=5){shape->rotatedRect.cx=m_paramSpins[0]->value();shape->rotatedRect.cy=m_paramSpins[1]->value();shape->rotatedRect.w=m_paramSpins[2]->value();shape->rotatedRect.h=m_paramSpins[3]->value();shape->rotatedRect.angle=m_paramSpins[4]->value();}break;
	case Shape_Circle:
		if(n>=3){shape->circle.cx=m_paramSpins[0]->value();shape->circle.cy=m_paramSpins[1]->value();shape->circle.r=m_paramSpins[2]->value();}break;
	case Shape_Ellipse:
		if(n>=5){shape->ellipse.cx=m_paramSpins[0]->value();shape->ellipse.cy=m_paramSpins[1]->value();shape->ellipse.r1=m_paramSpins[2]->value();shape->ellipse.r2=m_paramSpins[3]->value();shape->ellipse.angle=m_paramSpins[4]->value();}break;
	case Shape_Ring:
		if(n>=4){shape->ring.cx=m_paramSpins[0]->value();shape->ring.cy=m_paramSpins[1]->value();shape->ring.r1=m_paramSpins[2]->value();shape->ring.r2=m_paramSpins[3]->value();}break;
	case Shape_Arc:
		if(n>=6){shape->arc.cx=m_paramSpins[0]->value();shape->arc.cy=m_paramSpins[1]->value();shape->arc.rOuter=m_paramSpins[2]->value();shape->arc.rInner=m_paramSpins[3]->value();shape->arc.startAngle=m_paramSpins[4]->value();shape->arc.r1=m_paramSpins[5]->value();shape->arc.endAngle=shape->arc.startAngle+shape->arc.r1;}break;
	case Shape_Polygon:{
		int np=(int)shape->polygon.pts.size();if(n>=np*2)for(int i=0;i<np;++i)shape->polygon.pts[i]=QPointF(m_paramSpins[i*2]->value(),m_paramSpins[i*2+1]->value());}break;
	default:break;
	}
	// 清除旧场景图形再重建（避免重复）
	if(shape->item){m_scene->removeItem(shape->item);delete shape->item;shape->item=nullptr;}
	for(auto* h:shape->handles){m_scene->removeItem(h);delete h;}shape->handles.clear();
	if(shape->rotateHandle){m_scene->removeItem(shape->rotateHandle);delete shape->rotateHandle;shape->rotateHandle=nullptr;}
	if(shape->rotateStickLine){m_scene->removeItem(shape->rotateStickLine);delete shape->rotateStickLine;shape->rotateStickLine=nullptr;}
	if(shape->circumRect){m_scene->removeItem(shape->circumRect);delete shape->circumRect;shape->circumRect=nullptr;}
	if(shape->centerCrossH){m_scene->removeItem(shape->centerCrossH);delete shape->centerCrossH;shape->centerCrossH=nullptr;}
	if(shape->centerCrossV){m_scene->removeItem(shape->centerCrossV);delete shape->centerCrossV;shape->centerCrossV=nullptr;}
	rebuildShapeOnScene(shape);
}

void ImageCanvasView::applyParamAndRedraw()
{
	int index = ui.draw_cbox_shape_type->currentIndex();
	if (index <= 0) return;
	DrawShapeType type = static_cast<DrawShapeType>(index - 1);

	DrawShapeItem* shape = findShapeByType(type);
	if (!shape) { shape = new DrawShapeItem(type); m_shapes.append(shape); }
	clearSceneShape();
	m_activeShape = shape;

	liveApplyParam(shape);
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
	if (m_ghostEllipse2) { m_scene->removeItem(m_ghostEllipse2); delete m_ghostEllipse2; m_ghostEllipse2 = nullptr; }
	if (m_ghostArcPath) { m_scene->removeItem(m_ghostArcPath); delete m_ghostArcPath; m_ghostArcPath = nullptr; }
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
	if (m_ghostEllipse2) { m_scene->removeItem(m_ghostEllipse2); delete m_ghostEllipse2; m_ghostEllipse2 = nullptr; }
	if (m_ghostArcPath) { m_scene->removeItem(m_ghostArcPath); delete m_ghostArcPath; m_ghostArcPath = nullptr; }
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
	case Shape_Ellipse:    commitEllipse();    break;
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

// ===== 圆环提交（四点构造） =====
void ImageCanvasView::commitRing()
{
	// 第一个圆的半径存于 m_dragStartRect.w/2
	double cx = m_dragStartRect.cx, cy = m_dragStartRect.cy;
	double r1 = m_dragStartRect.w / 2.0;
	double r2 = 0;
	if (m_ghostEllipse2)
	{
		QRectF r = m_ghostEllipse2->rect();
		r2 = r.width() / 2.0;
	}
	if (r1 < 1.5 || r2 < 1.5) return;

	DrawShapeItem* shape = findShapeByType(Shape_Ring);
	if (!shape) { shape = new DrawShapeItem(Shape_Ring); m_shapes.append(shape); }
	shape->ring = { cx, cy, r1, r2 };

	clearSceneShape();
	m_activeShape = shape;
	rebuildShapeOnScene(shape);
}

// ===== 同心双圆弧求解 =====
static bool solveConcentricArc(const QPointF& A, const QPointF& B, double r1, double r2, QPointF& O, QPointF& P)
{
	double d = std::sqrt((B.x()-A.x())*(B.x()-A.x())+(B.y()-A.y())*(B.y()-A.y()));
	if (d<1e-6 || r1<1.0||r2<1.0) return false;
	double x = (r1*r1 - r2*r2 + d*d)/(2.0*d);
	double det = r1*r1 - x*x;
	if (det<-1e-6) return false;
	double y = std::sqrt(std::max(0.0,det));
	double dx=B.x()-A.x(), dy=B.y()-A.y();
	double ux=dx/d, uy=dy/d, vx=-uy, vy=ux;
	O = QPointF(A.x()+x*ux+y*vx, A.y()+x*uy+y*vy);
	double oax=A.x()-O.x(), oay=A.y()-O.y(), obx=B.x()-O.x(), oby=B.y()-O.y();
	double la=std::sqrt(oax*oax+oay*oay), lb=std::sqrt(obx*obx+oby*oby);
	if(la<1e-6||lb<1e-6) return false;
	double mx=oax/la+obx/lb, my=oay/la+oby/lb, ml=std::sqrt(mx*mx+my*my);
	if(ml<1e-6) return false;
	P = QPointF(O.x()+r1*mx/ml, O.y()+r1*my/ml);
	return true;
}

// ===== 同心双圆弧提交 =====
void ImageCanvasView::commitArc()
{
	double cx=m_dragStartRect.cx,cy=m_dragStartRect.cy,r1=m_dragStartRect.w,r2=m_dragStartRect.h;
	r2=std::max(r2,2.0);if(r1<2.0||r2<2.0)return;
	double aA=m_arcStartAngle,span=m_arcEndAngle;
	double rOuter=std::max(r1,r2),rInner=std::min(r1,r2);
	DrawShapeItem* shape=findShapeByType(Shape_Arc);
	if(!shape){shape=new DrawShapeItem(Shape_Arc);m_shapes.append(shape);}
	shape->arc.cx=cx;shape->arc.cy=cy;
	shape->arc.rOuter=rOuter;shape->arc.rInner=rInner;
	shape->arc.startAngle=aA;shape->arc.endAngle=aA+span;shape->arc.r1=span;
	clearSceneShape();m_activeShape=shape;rebuildShapeOnScene(shape);
}

// ===== 多边形提交 =====
void ImageCanvasView::commitPolygon()
{
	if(m_tempPolyPts.size()<3)return;
	DrawShapeItem* shape=findShapeByType(Shape_Polygon);
	if(!shape){shape=new DrawShapeItem(Shape_Polygon);m_shapes.append(shape);}
	shape->polygon.pts=m_tempPolyPts;
	// 清理绘制标记
	for(auto* mk:m_circleMarkers){m_scene->removeItem(mk);delete mk;}
	m_circleMarkers.clear();
	if(m_ghostPolyLine){m_scene->removeItem(m_ghostPolyLine);delete m_ghostPolyLine;m_ghostPolyLine=nullptr;}
	if(m_ghostArcPath){m_scene->removeItem(m_ghostArcPath);delete m_ghostArcPath;m_ghostArcPath=nullptr;}
	m_tempPolyPts.clear();
	hideDrawModeOverlay();m_mode=Mode_None;m_drawStep=0;
	ui.canvas_view_main->setCursor(Qt::ArrowCursor);
	clearSceneShape();m_activeShape=shape;rebuildShapeOnScene(shape);
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

// ===== 椭圆提交 =====
void ImageCanvasView::commitEllipse()
{
	if (!m_ghostRect) return;
	QRectF gr = m_ghostRect->rect();
	double cx = gr.center().x(), cy = gr.center().y();
	double r1 = gr.width()/2.0, r2 = gr.height()/2.0;
	if (r1 < 1.5 || r2 < 1.5) return;

	DrawShapeItem* shape = findShapeByType(Shape_Ellipse);
	if (!shape) { shape = new DrawShapeItem(Shape_Ellipse); m_shapes.append(shape); }
	shape->ellipse = { cx, cy, r1, r2, 0.0 };

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
	case Shape_Polygon:
	{
		QPolygonF poly;for(auto& pt:shape.polygon.pts)poly<<pt;
		QGraphicsPolygonItem* p=new QGraphicsPolygonItem(poly);p->setZValue(80);item=p;break;
	}
	case Shape_Ellipse:
	{
		double r1 = shape.ellipse.r1, r2 = shape.ellipse.r2;
		double rad = qDegreesToRadians(shape.ellipse.angle);
		// 用 QPainterPath 画旋转椭圆
		QPainterPath path;
		path.addEllipse(QPointF(0,0), r1, r2);
		QTransform t;
		t.translate(shape.ellipse.cx, shape.ellipse.cy);
		t.rotate(shape.ellipse.angle);
		QGraphicsPathItem* p = new QGraphicsPathItem();
		p->setPath(t.map(path));
		p->setZValue(80);
		item = p;
		break;
	}
	case Shape_Ring:
	{
		// 圆环：大圆减去小圆，用 QPainterPath
		double rLarger  = std::max(shape.ring.r1, shape.ring.r2);
		double rSmaller = std::min(shape.ring.r1, shape.ring.r2);
		QPainterPath outerPath, innerPath;
		outerPath.addEllipse(QPointF(0,0), rLarger, rLarger);
		innerPath.addEllipse(QPointF(0,0), rSmaller, rSmaller);
		QPainterPath ringPath = outerPath.subtracted(innerPath);
		QTransform t;
		t.translate(shape.ring.cx, shape.ring.cy);
		QGraphicsPathItem* p = new QGraphicsPathItem();
		p->setPath(t.map(ringPath));
		p->setZValue(80);
		item = p;
		break;
	}
	case Shape_Arc:
	{
		double cx=shape.arc.cx,cy=shape.arc.cy,rO=shape.arc.rOuter,rI=shape.arc.rInner;
		double sa=shape.arc.startAngle,span=shape.arc.r1,saR=qDegreesToRadians(sa),spR=qDegreesToRadians(span);
		const int N=128;QPainterPath path;
		path.moveTo(cx+rO*qCos(saR),cy+rO*qSin(saR));
		for(int i=1;i<=N;++i){double a=saR+spR*i/N;path.lineTo(cx+rO*qCos(a),cy+rO*qSin(a));}
		double eR=saR+spR;path.lineTo(cx+rI*qCos(eR),cy+rI*qSin(eR));
		for(int i=N;i>=0;--i){double a=saR+spR*i/N;path.lineTo(cx+rI*qCos(a),cy+rI*qSin(a));}
		path.closeSubpath();
		QGraphicsPathItem* p=new QGraphicsPathItem();p->setPath(path);p->setZValue(80);item=p;break;
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
	bool isEllipse = false;
	bool isRing = false;
	bool isArc = false;
	bool isPolygon = false;

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
	case Shape_Ellipse:
		cx = shape->ellipse.cx; cy = shape->ellipse.cy;
		halfW = shape->ellipse.r1; halfH = shape->ellipse.r2;
		angle = shape->ellipse.angle;
		isEllipse = true;
		break;
	case Shape_Ring:
		cx = shape->ring.cx; cy = shape->ring.cy;
		isRing = true;
		break;
	case Shape_Arc:
		cx = shape->arc.cx; cy = shape->arc.cy;
		isArc = true;
		break;
	case Shape_Polygon:
		isPolygon = true;
		break;
	}
	if (!isRing && !isArc && !isPolygon && halfW <= 0 && halfH <= 0) return;

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
	else if (isRing)
	{
		// 圆环：8 个控制点（外圆4个 + 内圆4个），不区分内外，按大小决定
		double r1 = shape->ring.r1, r2 = shape->ring.r2;
		double rLarger  = std::max(r1, r2);
		double rSmaller = std::min(r1, r2);

		// 外圆 4 个控制点 (索引 0-3)：上下左右，蓝色
		QPointF outerPts[4] = { {rLarger,0},{0,-rLarger},{-rLarger,0},{0,rLarger} };
		for (int i = 0; i < 4; ++i)
		{
			QPointF pt = outerPts[i] + QPointF(cx, cy);
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

		// 内圆 4 个控制点 (索引 4-7)：上下左右，橙色
		QPointF innerPts[4] = { {rSmaller,0},{0,-rSmaller},{-rSmaller,0},{0,rSmaller} };
		for (int i = 0; i < 4; ++i)
		{
			QPointF pt = innerPts[i] + QPointF(cx, cy);
			QGraphicsEllipseItem* h = new QGraphicsEllipseItem(pt.x()-kHandleRadius, pt.y()-kHandleRadius, kHandleRadius*2, kHandleRadius*2);
			h->setPen(QPen(Qt::white, 1));
			h->setBrush(QColor(255, 140, 0));  // 橙色区分内圆
			h->setZValue(100);
			h->setData(0, i + 4);
			h->setData(1, 0);
			h->setAcceptHoverEvents(true);
			m_scene->addItem(h);
			shape->handles.append(h);
		}

		// 外接矩形由较大半径的圆决定
		shape->circumRect = new QGraphicsRectItem(cx-rLarger, cy-rLarger, rLarger*2, rLarger*2);
		QPen circumPen(QColor(160, 160, 160), 1, Qt::DashLine);
		circumPen.setCosmetic(true);
		shape->circumRect->setPen(circumPen);
		shape->circumRect->setBrush(Qt::NoBrush);
		shape->circumRect->setZValue(75);
		m_scene->addItem(shape->circumRect);
	}
	else if (isArc)
	{
		double cx=shape->arc.cx,cy=shape->arc.cy,rO=shape->arc.rOuter,rI=shape->arc.rInner;
		double sa=shape->arc.startAngle,span=shape->arc.r1;
		double saR=qDegreesToRadians(sa),eaR=qDegreesToRadians(sa+span);
		double midA=sa+span/2.0,midR=qDegreesToRadians(midA);

		// 外弧: 起点(0,红)、终点(1,红)、中点半径(2,蓝)
		QPointF oPts[3]={{cx+rO*qCos(saR),cy+rO*qSin(saR)},{cx+rO*qCos(eaR),cy+rO*qSin(eaR)},{cx+rO*qCos(midR),cy+rO*qSin(midR)}};
		QColor oCs[3]={QColor(255,80,80),QColor(255,80,80),QColor(0,180,255)};
		for(int i=0;i<3;++i){auto* h=new QGraphicsEllipseItem(oPts[i].x()-kHandleRadius,oPts[i].y()-kHandleRadius,kHandleRadius*2,kHandleRadius*2);h->setPen(QPen(Qt::white,1));h->setBrush(oCs[i]);h->setZValue(100);h->setData(0,i);h->setData(1,0);h->setAcceptHoverEvents(true);m_scene->addItem(h);shape->handles.append(h);}

		// 内弧: 中点半径(3,橙)、起点(4,红)、终点(5,红)
		QPointF iPts[3]={{cx+rI*qCos(midR),cy+rI*qSin(midR)},{cx+rI*qCos(saR),cy+rI*qSin(saR)},{cx+rI*qCos(eaR),cy+rI*qSin(eaR)}};
		QColor iCs[3]={QColor(255,140,0),QColor(255,80,80),QColor(255,80,80)};
		for(int i=0;i<3;++i){auto* h=new QGraphicsEllipseItem(iPts[i].x()-kHandleRadius,iPts[i].y()-kHandleRadius,kHandleRadius*2,kHandleRadius*2);h->setPen(QPen(Qt::white,1));h->setBrush(iCs[i]);h->setZValue(100);h->setData(0,i+3);h->setData(1,0);h->setAcceptHoverEvents(true);m_scene->addItem(h);shape->handles.append(h);}

		// 旋转手柄
		double rRot=std::max(rO,rI),sL=30.0;QPointF omP(cx+rRot*qCos(midR),cy+rRot*qSin(midR)),rotP(cx+(rRot+sL)*qCos(midR),cy+(rRot+sL)*qSin(midR));
		shape->rotateStickLine=new QGraphicsLineItem(omP.x(),omP.y(),rotP.x(),rotP.y());QPen sP(QColor(220,220,220),1);sP.setCosmetic(true);shape->rotateStickLine->setPen(sP);shape->rotateStickLine->setZValue(95);m_scene->addItem(shape->rotateStickLine);
		shape->rotateHandle=new QGraphicsEllipseItem(rotP.x()-kHandleRadius,rotP.y()-kHandleRadius,kHandleRadius*2,kHandleRadius*2);shape->rotateHandle->setPen(QPen(QColor(255,200,0),2));shape->rotateHandle->setBrush(QColor(255,180,0));shape->rotateHandle->setZValue(100);shape->rotateHandle->setData(0,0);shape->rotateHandle->setData(1,1);shape->rotateHandle->setAcceptHoverEvents(true);m_scene->addItem(shape->rotateHandle);
	}
	else if (shape->type == Shape_Polygon)
	{
		auto& pts=shape->polygon.pts;for(int i=0;i<pts.size();++i){
			auto* h=new QGraphicsEllipseItem(pts[i].x()-kHandleRadius,pts[i].y()-kHandleRadius,kHandleRadius*2,kHandleRadius*2);
			h->setPen(QPen(Qt::white,1));h->setBrush(QColor(0,180,255));h->setZValue(100);h->setData(0,i);h->setData(1,0);h->setAcceptHoverEvents(true);
			m_scene->addItem(h);shape->handles.append(h);
		}
	}
	else if (isEllipse)
	{
		// 椭圆：4 个控制点（上下左右，局部坐标），旋转后转换到世界坐标系
		QPointF localPts[4] = { {halfW,0},{0,-halfH},{-halfW,0},{0,halfH} };
		for (int i = 0; i < 4; ++i)
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

		// 外接矩形（旋转后椭圆外接的轴对齐矩形）
		// 椭圆旋转角度 ang 后，AABB 的 x 和 y 半范围：
		// hw = sqrt((r1*cos)? + (r2*sin)?), hh = sqrt((r1*sin)? + (r2*cos)?)
		double r1 = shape->ellipse.r1, r2 = shape->ellipse.r2;
		double rad = qDegreesToRadians(angle);
		double cosA = qCos(rad), sinA = qSin(rad);
		double hw = std::sqrt(r1*r1*cosA*cosA + r2*r2*sinA*sinA);
		double hh = std::sqrt(r1*r1*sinA*sinA + r2*r2*cosA*cosA);
		shape->circumRect = new QGraphicsRectItem(cx-hw, cy-hh, hw*2, hh*2);
		shape->circumRect->setZValue(75);
		QPen circumPen(QColor(160,160,160), 1, Qt::DashLine);
		circumPen.setCosmetic(true);
		shape->circumRect->setPen(circumPen);
		shape->circumRect->setBrush(Qt::NoBrush);
		m_scene->addItem(shape->circumRect);

		// 旋转手柄（上边中点上方）
		double mx = 0 * qCos(rad) - (-r2) * qSin(rad);
		double my = 0 * qSin(rad) + (-r2) * qCos(rad);
		QPointF midTop(mx + cx, my + cy);
		double stickLen = 30.0;
		QPointF rotPt(midTop.x() + qSin(rad)*stickLen, midTop.y() - qCos(rad)*stickLen);

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
	else if (shape->type == Shape_Ring)
	{
		double cx = shape->ring.cx, cy = shape->ring.cy;
		double r1 = shape->ring.r1, r2 = shape->ring.r2;
		double rLarger  = std::max(r1, r2);
		double rSmaller = std::min(r1, r2);

		// 外圆 4 个控制点 (索引 0-3)
		QPointF outerPts[4] = { {rLarger,0},{0,-rLarger},{-rLarger,0},{0,rLarger} };
		for (int i = 0; i < 4 && i + 0 < shape->handles.size(); ++i)
			moveHandle(shape->handles[i], outerPts[i] + QPointF(cx, cy));

		// 内圆 4 个控制点 (索引 4-7)
		QPointF innerPts[4] = { {rSmaller,0},{0,-rSmaller},{-rSmaller,0},{0,rSmaller} };
		for (int i = 0; i < 4 && i + 4 < shape->handles.size(); ++i)
			moveHandle(shape->handles[i + 4], innerPts[i] + QPointF(cx, cy));

		// 更新圆环 path item: 大圆减去小圆
		if (auto* p = dynamic_cast<QGraphicsPathItem*>(shape->item))
		{
			QPainterPath outerPath, innerPath;
			outerPath.addEllipse(QPointF(0,0), rLarger, rLarger);
			innerPath.addEllipse(QPointF(0,0), rSmaller, rSmaller);
			QPainterPath ringPath = outerPath.subtracted(innerPath);
			QTransform t;
			t.translate(cx, cy);
			p->setPath(t.map(ringPath));
		}

		// 外接矩形由较大半径决定
		if (shape->circumRect)
			shape->circumRect->setRect(cx-rLarger, cy-rLarger, rLarger*2, rLarger*2);

		if (shape->centerCrossH) shape->centerCrossH->setLine(cx-kCrossLen, cy, cx+kCrossLen, cy);
		if (shape->centerCrossV) shape->centerCrossV->setLine(cx, cy-kCrossLen, cx, cy+kCrossLen);
	}
	else if (shape->type == Shape_Arc)
	{
		double cx=shape->arc.cx,cy=shape->arc.cy,rO=shape->arc.rOuter,rI=shape->arc.rInner;
		double sa=shape->arc.startAngle,span=shape->arc.r1,saR=qDegreesToRadians(sa),spR=qDegreesToRadians(span);
		double eR=saR+spR,midA=sa+span/2.0,midR=qDegreesToRadians(midA);
		if(0<(int)shape->handles.size())moveHandle(shape->handles[0],QPointF(cx+rO*qCos(saR),cy+rO*qSin(saR)));
		if(1<(int)shape->handles.size())moveHandle(shape->handles[1],QPointF(cx+rO*qCos(eR),cy+rO*qSin(eR)));
		if(2<(int)shape->handles.size())moveHandle(shape->handles[2],QPointF(cx+rO*qCos(midR),cy+rO*qSin(midR)));
		if(3<(int)shape->handles.size())moveHandle(shape->handles[3],QPointF(cx+rI*qCos(midR),cy+rI*qSin(midR)));
		if(4<(int)shape->handles.size())moveHandle(shape->handles[4],QPointF(cx+rI*qCos(saR),cy+rI*qSin(saR)));
		if(5<(int)shape->handles.size())moveHandle(shape->handles[5],QPointF(cx+rI*qCos(eR),cy+rI*qSin(eR)));
		if(shape->rotateHandle){double rRot=std::max(rO,rI),sL=30.0;QPointF omP(cx+rRot*qCos(midR),cy+rRot*qSin(midR)),rotP(cx+(rRot+sL)*qCos(midR),cy+(rRot+sL)*qSin(midR));moveHandle(shape->rotateHandle,rotP);if(shape->rotateStickLine)shape->rotateStickLine->setLine(omP.x(),omP.y(),rotP.x(),rotP.y());}

		if (shape->centerCrossH) shape->centerCrossH->setLine(cx-kCrossLen, cy, cx+kCrossLen, cy);
		if (shape->centerCrossV) shape->centerCrossV->setLine(cx, cy-kCrossLen, cx, cy+kCrossLen);

		if (auto* p = dynamic_cast<QGraphicsPathItem*>(shape->item)) {
			const int N=128;QPainterPath ph;
			ph.moveTo(cx+rO*qCos(saR),cy+rO*qSin(saR));
			for(int i=1;i<=N;++i){double a=saR+spR*i/N;ph.lineTo(cx+rO*qCos(a),cy+rO*qSin(a));}
			double endR=saR+spR;ph.lineTo(cx+rI*qCos(endR),cy+rI*qSin(endR));
			for(int i=N;i>=0;--i){double a=saR+spR*i/N;ph.lineTo(cx+rI*qCos(a),cy+rI*qSin(a));}
			ph.closeSubpath();p->setPath(ph);
		}
	}
	else if (shape->type == Shape_Polygon)
	{
		auto& pts=shape->polygon.pts;
		for(int i=0;i<pts.size()&&i<(int)shape->handles.size();++i)moveHandle(shape->handles[i],pts[i]);
		if(auto* p=dynamic_cast<QGraphicsPolygonItem*>(shape->item)){
			QPolygonF poly;for(auto& pt:pts)poly<<pt;p->setPolygon(poly);
		}
	}
	else if (shape->type == Shape_Ellipse)
	{
		double cx = shape->ellipse.cx, cy = shape->ellipse.cy;
		double r1 = shape->ellipse.r1, r2 = shape->ellipse.r2;
		double rad = qDegreesToRadians(shape->ellipse.angle);

		// 4 个控制点
		QPointF localPts[4] = { {r1,0},{0,-r2},{-r1,0},{0,r2} };
		for (int i = 0; i < 4 && i < shape->handles.size(); ++i)
		{
			double rx = localPts[i].x() * qCos(rad) - localPts[i].y() * qSin(rad);
			double ry = localPts[i].x() * qSin(rad) + localPts[i].y() * qCos(rad);
			moveHandle(shape->handles[i], QPointF(rx+cx, ry+cy));
		}

		// 旋转手柄
		if (shape->rotateHandle)
		{
			double mx = 0 * qCos(rad) - (-r2) * qSin(rad);
			double my = 0 * qSin(rad) + (-r2) * qCos(rad);
			QPointF midTop(mx+cx, my+cy);
			double stickLen = 30.0;
			QPointF rotPt(midTop.x() + qSin(rad)*stickLen, midTop.y() - qCos(rad)*stickLen);
			moveHandle(shape->rotateHandle, rotPt);
			if (shape->rotateStickLine)
				shape->rotateStickLine->setLine(midTop.x(), midTop.y(), rotPt.x(), rotPt.y());
		}

		// 外接矩形
		if (shape->circumRect)
		{
			double cosA = qCos(rad), sinA = qSin(rad);
			double hw = std::sqrt(r1*r1*cosA*cosA + r2*r2*sinA*sinA);
			double hh = std::sqrt(r1*r1*sinA*sinA + r2*r2*cosA*cosA);
			shape->circumRect->setRect(cx-hw, cy-hh, hw*2, hh*2);
		}

		// item
		if (auto* p = dynamic_cast<QGraphicsPathItem*>(shape->item))
		{
			QPainterPath path;
			path.addEllipse(QPointF(0,0), r1, r2);
			QTransform t;
			t.translate(cx, cy);
			t.rotate(shape->ellipse.angle);
			p->setPath(t.map(path));
		}
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
	case Shape_Ring:
	{
		double d2 = (scenePos.x()-shape->ring.cx)*(scenePos.x()-shape->ring.cx)
		          + (scenePos.y()-shape->ring.cy)*(scenePos.y()-shape->ring.cy);
		double rLarger  = std::max(shape->ring.r1, shape->ring.r2);
		double rSmaller = std::min(shape->ring.r1, shape->ring.r2);
		return d2 <= rLarger * rLarger && d2 >= rSmaller * rSmaller;
	}
	case Shape_Arc:
	{
		double cx=shape->arc.cx,cy=shape->arc.cy;
		double rBig=std::max(shape->arc.rOuter,shape->arc.rInner);
		double rSml=std::min(shape->arc.rOuter,shape->arc.rInner);
		double dx=scenePos.x()-cx,dy=scenePos.y()-cy,d=std::sqrt(dx*dx+dy*dy);
		if(d>rBig+8.0||d<rSml-8.0)return false;
		double ang=normAngle360(qRadiansToDegrees(std::atan2(dy,dx)));
		double sa=normAngle360(shape->arc.startAngle),span=shape->arc.r1;
		double rel=ang-sa;if(rel<0)rel+=360.0;
		if(span>=0)return rel<=span;
		return rel>=360.0+span;  // 顺时针弧：检测"非缺口"范围
	}
	case Shape_Polygon:
	{
		QPolygonF poly;for(auto& pt:shape->polygon.pts)poly<<pt;
		return poly.containsPoint(scenePos,Qt::OddEvenFill);
	}
	case Shape_Ellipse:
	{
		double rad = -qDegreesToRadians(shape->ellipse.angle);
		double dx  = scenePos.x() - shape->ellipse.cx;
		double dy  = scenePos.y() - shape->ellipse.cy;
		double lx  = dx * qCos(rad) - dy * qSin(rad);
		double ly  = dx * qSin(rad) + dy * qCos(rad);
		double r1  = shape->ellipse.r1;
		double r2  = shape->ellipse.r2;
		return (lx*lx)/(r1*r1) + (ly*ly)/(r2*r2) <= 1.0;
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

void ImageCanvasView::applyShapeHover(bool hover)
{
	if (!m_activeShape || !m_activeShape->item) return;
	int w = hover ? m_penWidth + 2 : m_penWidth;
	QPen pen(m_colorSelected, w);
	pen.setCosmetic(true);
	if (auto* r = dynamic_cast<QGraphicsRectItem*>(m_activeShape->item))       r->setPen(pen);
	else if (auto* po = dynamic_cast<QGraphicsPolygonItem*>(m_activeShape->item)) po->setPen(pen);
	else if (auto* e = dynamic_cast<QGraphicsEllipseItem*>(m_activeShape->item)) e->setPen(pen);
	else if (auto* pa = dynamic_cast<QGraphicsPathItem*>(m_activeShape->item))  pa->setPen(pen);
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
	syncParamPanel(m_dragShape);
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
		syncParamPanel(m_dragShape);
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
		syncParamPanel(m_dragShape);
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
	syncParamPanel(m_dragShape);
}

// ===== 圆环 Handle 拖拽 =====
void ImageCanvasView::updateRingFromHandle(const QPointF& scenePos)
{
	if (!m_dragShape || m_dragHandleIndex < 0 || m_dragHandleIndex >= 8) return;

	// m_dragStartRect: cx, cy, 拖拽前 r1*2, 拖拽前 r2*2
	double cx0 = m_dragStartRect.cx, cy0 = m_dragStartRect.cy;
	double oldR1 = m_dragStartRect.w / 2.0;
	double oldR2 = m_dragStartRect.h / 2.0;

	double dx = scenePos.x() - cx0;
	double dy = scenePos.y() - cy0;

	// 0-3 是原来较大半径的控制点，4-7 是原来较小半径的控制点
	// 拖拽哪个就修改对应的 r1 或 r2，不限制大小关系
	// 显示时自动按 max/min 区分外圆内圆
	if (m_dragHandleIndex < 4)
	{
		double newR = 0;
		switch (m_dragHandleIndex)
		{
		case 0: newR = dx;    break;
		case 1: newR = -dy;   break;
		case 2: newR = -dx;   break;
		case 3: newR = dy;    break;
		}
		newR = std::max(newR, 1.0);
		// 更新当前拖拽的那个半径（可能是 r1 也可能是 r2）
		if (oldR1 >= oldR2)
			m_dragShape->ring.r1 = newR;
		else
			m_dragShape->ring.r2 = newR;
	}
	else
	{
		double newR = 0;
		switch (m_dragHandleIndex)
		{
		case 4: newR = dx;    break;
		case 5: newR = -dy;   break;
		case 6: newR = -dx;   break;
		case 7: newR = dy;    break;
		}
		newR = std::max(newR, 1.0);
		if (oldR1 >= oldR2)
			m_dragShape->ring.r2 = newR;
		else
			m_dragShape->ring.r1 = newR;
	}

	m_dragShape->ring.cx = cx0;
	m_dragShape->ring.cy = cy0;

	updateHandlePositions(m_dragShape);
	syncParamPanel(m_dragShape);
}

// ===== 多边形 Handle 拖拽 =====
void ImageCanvasView::updatePolygonFromHandle(const QPointF& scenePos)
{
	if(!m_dragShape||m_dragHandleIndex<0||m_dragHandleIndex>=m_dragShape->polygon.pts.size())return;
	m_dragShape->polygon.pts[m_dragHandleIndex]=scenePos;
	updateHandlePositions(m_dragShape);
	syncParamPanel(m_dragShape);
}

// ===== 圆弧 Handle 拖拽 =====
void ImageCanvasView::updateArcFromHandle(const QPointF& scenePos)
{
	if (!m_dragShape) return;

	double cx = m_dragShape->arc.cx, cy = m_dragShape->arc.cy;

	// 旋转
	if (m_isRotating) {
		double newMidAng = normAngle360(qRadiansToDegrees(std::atan2(scenePos.y()-cy, scenePos.x()-cx)));
		double delta = newMidAng - m_dragStartAngle;
		if (delta > 180.0) delta -= 360.0;
		if (delta < -180.0) delta += 360.0;
		m_dragShape->arc.startAngle += delta;
		m_dragStartAngle = newMidAng;
		updateHandlePositions(m_dragShape);
		syncParamPanel(m_dragShape);
		return;
	}

	if (m_dragHandleIndex < 0 || m_dragHandleIndex >= 6) return;

	if(m_dragHandleIndex==0||m_dragHandleIndex==1||m_dragHandleIndex==4||m_dragHandleIndex==5){
		double na=normAngle360(qRadiansToDegrees(std::atan2(scenePos.y()-cy,scenePos.x()-cx)));
		if(m_dragHandleIndex==0||m_dragHandleIndex==4)m_dragShape->arc.startAngle=na;else m_dragShape->arc.endAngle=na;
		double sa=m_dragShape->arc.startAngle,ea=m_dragShape->arc.endAngle;bool cw=(m_dragShape->arc.r1<0);
		double sf=ea-sa;if(sf<=0)sf+=360.0;m_dragShape->arc.r1=cw?(sf-360.0):sf;
	}else if(m_dragHandleIndex==2){double nr=std::sqrt((scenePos.x()-cx)*(scenePos.x()-cx)+(scenePos.y()-cy)*(scenePos.y()-cy));m_dragShape->arc.rOuter=std::max(nr,1.5);
	}else if(m_dragHandleIndex==3){double nr=std::sqrt((scenePos.x()-cx)*(scenePos.x()-cx)+(scenePos.y()-cy)*(scenePos.y()-cy));m_dragShape->arc.rInner=std::max(nr,1.5);}
	updateHandlePositions(m_dragShape);
	syncParamPanel(m_dragShape);
}

// ===== 椭圆 Handle 拖拽 =====
void ImageCanvasView::updateEllipseFromHandle(const QPointF& scenePos)
{
	if (!m_dragShape || m_dragHandleIndex < 0 || m_dragHandleIndex >= 4) return;

	if (m_isRotating)
	{
		// 旋转手柄 → 计算角度
		double cx = m_dragShape->ellipse.cx;
		double cy = m_dragShape->ellipse.cy;
		double angle = qRadiansToDegrees(qAtan2(scenePos.y()-cy, scenePos.x()-cx)) + 90.0;
		m_dragShape->ellipse.angle = angle;
		updateHandlePositions(m_dragShape);
		syncParamPanel(m_dragShape);
		return;
	}

	// 普通控制点拖拽：先转回局部坐标系
	double angle = m_dragShape->ellipse.angle;
	double rad   = -qDegreesToRadians(angle);
	double dx    = scenePos.x() - m_dragStartRect.cx;
	double dy    = scenePos.y() - m_dragStartRect.cy;
	double lx    = dx * qCos(rad) - dy * qSin(rad);
	double ly    = dx * qSin(rad) + dy * qCos(rad);

	double newR1 = m_dragStartRect.w / 2.0;
	double newR2 = m_dragStartRect.h / 2.0;

	switch (m_dragHandleIndex)
	{
	case 0: newR1 =  lx; break;  // 右
	case 1: newR2 = -ly; break;  // 上
	case 2: newR1 = -lx; break;  // 左
	case 3: newR2 =  ly; break;  // 下
	}
	newR1 = std::max(newR1, 1.5);
	newR2 = std::max(newR2, 1.5);

	m_dragShape->ellipse.cx = m_dragStartRect.cx;
	m_dragShape->ellipse.cy = m_dragStartRect.cy;
	m_dragShape->ellipse.r1 = newR1;
	m_dragShape->ellipse.r2 = newR2;

	updateHandlePositions(m_dragShape);
	syncParamPanel(m_dragShape);
}
