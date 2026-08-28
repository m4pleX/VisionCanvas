#include "ParamPanelWidget.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

ParamPanelWidget::ParamPanelWidget(QWidget* parent) : QWidget(parent) {
	setObjectName("paramPanel");
	setAttribute(Qt::WA_StyledBackground, true);
	setStyleSheet("QWidget#paramPanel { background-color: rgba(50, 50, 50, 220); border-radius: 6px; }");

	auto* outer = new QVBoxLayout(this);
	outer->setAlignment(Qt::AlignCenter);

	auto* title = new QLabel(QStringLiteral("\u53c2\u6570\u8bbe\u7f6e"), this);
	title->setStyleSheet("color: white; font-size: 14px; font-weight: bold; background: transparent;");
	title->setAlignment(Qt::AlignCenter);
	outer->addWidget(title);
	outer->addSpacing(10);

	auto* scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
	outer->addWidget(scroll, 1);

	auto* content = new QWidget();
	content->setStyleSheet("background: transparent;");
	m_contentLayout = new QVBoxLayout(content);
	scroll->setWidget(content);

	outer->addSpacing(10);

	auto* btnRow = new QHBoxLayout();
	btnRow->setAlignment(Qt::AlignCenter);

	auto* btnOk = new QPushButton(QStringLiteral("\u786e\u5b9a"), this);
	btnOk->setFixedSize(80, 30);
	btnOk->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; border: none; border-radius: 4px; font-size: 13px; }"
		"QPushButton:hover { background-color: #27ae60; }");
	btnOk->setFocusPolicy(Qt::NoFocus);

	auto* btnCancel = new QPushButton(QStringLiteral("\u53d6\u6d88"), this);
	btnCancel->setFixedSize(80, 30);
	btnCancel->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border: none; border-radius: 4px; font-size: 13px; }"
		"QPushButton:hover { background-color: #c0392b; }");
	btnCancel->setFocusPolicy(Qt::NoFocus);

	btnRow->addWidget(btnOk);
	btnRow->addSpacing(15);
	btnRow->addWidget(btnCancel);
	outer->addLayout(btnRow);

	connect(btnOk, &QPushButton::clicked, this, &ParamPanelWidget::confirmed);
	connect(btnCancel, &QPushButton::clicked, this, &ParamPanelWidget::cancelled);
}

void ParamPanelWidget::buildUI(const QList<ParamField>& fields, DrawShapeItem* shape, bool polyLayout) {
	for (auto* s : m_spins) { m_contentLayout->removeWidget(s); delete s; }
	m_spins.clear();
	for (auto* l : m_labels) { m_contentLayout->removeWidget(l); delete l; }
	m_labels.clear();

	if (polyLayout) {
		for (int i = 0; i < fields.size() / 2; ++i) {
			auto* row = new QHBoxLayout();
			auto* lbl = new QLabel(QStringLiteral("\u9876\u70b9%1").arg(i), this);
			lbl->setStyleSheet("color: #ccc; font-size: 12px; background: transparent; min-width: 36px;");
			lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			m_labels.append(lbl);
			row->addWidget(lbl);

			for (int j = 0; j < 2; ++j) {
				int idx = i * 2 + j;
				auto* xyLbl = new QLabel(j == 0 ? QStringLiteral("X") : QStringLiteral("Y"), this);
				xyLbl->setStyleSheet("color: #999; font-size: 11px; background: transparent;");
				m_labels.append(xyLbl);
				row->addWidget(xyLbl);

				auto* spin = new QDoubleSpinBox(this);
				spin->setDecimals(3);
				spin->setRange(fields[idx].minVal, fields[idx].maxVal);
				spin->setStyleSheet("QDoubleSpinBox { background: #3a3a3a; color: white; border: 1px solid #666; border-radius: 3px; padding: 3px; }");
				spin->setFixedWidth(80);
				spin->setValue(shape ? fields[idx].getter(*shape) : 0);
				spin->setKeyboardTracking(false);
				connect(spin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
					this, &ParamPanelWidget::valueChanged);
				m_spins.append(spin);
				row->addWidget(spin);
			}
			m_contentLayout->addLayout(row);
		}
	} else {
		for (int i = 0; i < fields.size(); ++i) {
			auto& f = fields[i];
			auto* row = new QHBoxLayout();
			auto* lbl = new QLabel(f.name, this);
			lbl->setStyleSheet("color: #ccc; font-size: 12px; background: transparent; min-width: 50px;");
			lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			m_labels.append(lbl);
			row->addWidget(lbl);

			auto* spin = new QDoubleSpinBox(this);
			spin->setDecimals(3);
			spin->setRange(f.minVal, f.maxVal);
			spin->setStyleSheet("QDoubleSpinBox { background: #3a3a3a; color: white; border: 1px solid #666; border-radius: 3px; padding: 3px; }");
			spin->setFixedWidth(120);
			spin->setValue(shape ? f.getter(*shape) : 0);
			spin->setKeyboardTracking(false);
			connect(spin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
				this, &ParamPanelWidget::valueChanged);
			m_spins.append(spin);
			row->addWidget(spin);
			m_contentLayout->addLayout(row);
		}
	}
}

void ParamPanelWidget::syncValues(const QList<ParamField>& fields, const DrawShapeItem& shape) {
	if (!isVisible()) return;
	for (int i = 0; i < fields.size() && i < m_spins.size(); ++i) {
		m_spins[i]->blockSignals(true);
		m_spins[i]->setValue(fields[i].getter(shape));
		m_spins[i]->blockSignals(false);
	}
}

void ParamPanelWidget::applyValues(const QList<ParamField>& fields, DrawShapeItem& shape) {
	for (int i = 0; i < fields.size() && i < m_spins.size(); ++i)
		fields[i].setter(shape, m_spins[i]->value());
	if (fields.size() >= 6 && shape.type == Shape_Arc)
		shape.endAngle = shape.startAngle + shape.span;
}
