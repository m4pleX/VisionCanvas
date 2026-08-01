/*
 * 文件名：ToolbarController.h
 * 职责：工具栏按钮文字联动与缩放状态同步
 * 核心功能：
 *   - 缩放比例显示更新（setScale）
 *   - 按钮文字状态联动（十字线/控制点/线宽切换按钮）
 *   - 分辨率显示文案更新
 * 依赖：QPushButton, QLabel（外部传入，不拥有）
 * 注意：不处理业务逻辑，只负责 UI 文案刷新
 */
#pragma once

#include <QObject>

class QPushButton;
class QLabel;

/*  工具栏状态管理：按钮文字、缩放命令、UI 同步 */
/*  不处理视图逻辑，只做 UI 层状态联动 */
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
