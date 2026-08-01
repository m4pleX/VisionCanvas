#pragma once

#include <QObject>

class QPushButton;
class QLabel;

class ToolbarController : public QObject {
	Q_OBJECT
public:
	explicit ToolbarController(QPushButton* btnCross, QPushButton* btnCtrlPt,
		QPushButton* btnLineWidth, QLabel* lblScale, QLabel* lblRes,
		QObject* parent = nullptr);

	void setScale(double val);
	double scale() const { return m_scale; }

	void updateCrossBtnText(bool shown);
	void updateCtrlPtBtnText(bool shown);
	void updateLineWidthBtnText(bool thin);
	void updateResolution(int w, int h);

private:
	QPushButton* m_btnCross;
	QPushButton* m_btnCtrlPt;
	QPushButton* m_btnLineWidth;
	QLabel* m_lblScale;
	QLabel* m_lblRes;
	double m_scale = 1.0;
};
