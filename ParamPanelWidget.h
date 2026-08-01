/*
 * 文件名：ParamPanelWidget.h
 * 职责：参数面板 UI 控件
 * 核心功能：
 *   - 根据 ParamField schema 自动生成标签和 QDoubleSpinBox
 *   - syncValues：拖拽时单向刷新面板数值
 *   - applyValues：确认时从面板读回数据写入 shape
 *   - 多边形 X/Y 并排布局，支持 QScrollArea 滚动
 * 依赖：DrawShapeItem, ParamField
 * 注意：不修改场景/形状数据，仅读写参数值
 */
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

// 参数面板：数值编辑、实时预览、参数提交
// 单向驱动画布更新，不处理鼠标事件
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
