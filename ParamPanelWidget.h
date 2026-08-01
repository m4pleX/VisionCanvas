#pragma once

#include <QWidget>
#include <QList>
#include <QDoubleSpinBox>
#include <QLabel>
#include <functional>
#include "DrawShapeData.h"

class QVBoxLayout;

struct ParamField {
	QString   name;
	double    minVal = -999999;
	double    maxVal =  999999;
	std::function<double(const DrawShapeItem&)> getter;
	std::function<void(DrawShapeItem&, double)> setter;
};

class ParamPanelWidget : public QWidget {
	Q_OBJECT
public:
	explicit ParamPanelWidget(QWidget* parent = nullptr);
	void buildUI(const QList<ParamField>& fields, DrawShapeItem* shape, bool polyLayout = false);
	void syncValues(const QList<ParamField>& fields, const DrawShapeItem& shape);
	void applyValues(const QList<ParamField>& fields, DrawShapeItem& shape);
	QList<QDoubleSpinBox*> spins() const { return m_spins; }

signals:
	void confirmed();
	void cancelled();
	void valueChanged();

private:
	QVBoxLayout* m_contentLayout = nullptr;
	QList<QDoubleSpinBox*> m_spins;
	QList<QLabel*> m_labels;
};
