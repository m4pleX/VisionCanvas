#include "ToolbarController.h"
#include "ScaleConfig.h"
#include <QLabel>
#include <QPushButton>
#include <QtMath>

ToolbarController::ToolbarController(QPushButton* btnCross, QPushButton* btnCtrlPt,
	QPushButton* btnLineWidth, QLabel* lblScale, QLabel* lblRes, QObject* parent)
	: QObject(parent), m_btnCross(btnCross), m_btnCtrlPt(btnCtrlPt),
	  m_btnLineWidth(btnLineWidth), m_lblScale(lblScale), m_lblRes(lblRes) {}

void ToolbarController::setScale(double val) {
	m_scale = ScaleConfig::clamp(val);
	m_lblScale->setText(QString("%1%").arg(qRound(m_scale * 100.0)));
}

void ToolbarController::updateCrossBtnText(bool shown) {
	m_btnCross->setText(shown
		? QStringLiteral("\u9690\u85cf\u5341\u5b57\u7ebf")
		: QStringLiteral("\u663e\u793a\u5341\u5b57\u7ebf"));
}

void ToolbarController::updateCtrlPtBtnText(bool shown) {
	m_btnCtrlPt->setText(shown
		? QStringLiteral("\u9690\u85cf\u63a7\u5236\u70b9")
		: QStringLiteral("\u663e\u793a\u63a7\u5236\u70b9"));
}

void ToolbarController::updateLineWidthBtnText(bool thin) {
	m_btnLineWidth->setText(thin
		? QStringLiteral("\u5207\u6362\u4e3a\u7c97\u7ebf")
		: QStringLiteral("\u5207\u6362\u4e3a\u7ec6\u7ebf"));
}

void ToolbarController::updateResolution(int w, int h) {
	m_lblRes->setText(QString("(%1, %2)").arg(w).arg(h));
}
