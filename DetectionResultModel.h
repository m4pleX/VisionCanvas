/*
 * 文件名：DetectionResultModel.h
 * 职责：算法结果宿主（数据层）
 *
 * 设计核心：
 *   - 持有【多个】 AlgorithmResult，解决"一张图多个算法实例/工具/多次运行"的并存问题；
 *   - 纯数据、零渲染依赖，不接触 QGraphicsItem / 场景 / UI；
 *   - 通过 engine + toolId + resultId 三元组唯一定位某个结果；
 *   - 所有权：AlgorithmResult 为值语义（Qt 隐式共享容器），本模型复制持有，
 *     无需手动堆管理；结果为只读，不受输入几何（DrawShapeItem）影响。
 *
 * 职责边界（对比 ImageCanvasView）：
 *   ImageCanvasView 负责"把结果画成只读叠层"、"清空/重跑"等交互，属于编排/渲染；
 *   本模型只负责"结果的增删查改"这一数据职责，二者单向依赖（View -> Model）。
 */
#pragma once

#include <QList>
#include <QString>
#include "AlgorithmResult.h"

class DetectionResultModel
{
public:
	/*  追加一个结果（返回其索引） */
	int  addResult(const AlgorithmResult& result);

	/*  按三元组移除；未命中返回 false */
	bool removeResult(const QString& engine, const QString& toolId, const QString& resultId);

	/*  清空 */
	void clear();

	/*  数量 / 是否为空 */
	int  count() const;
	bool isEmpty() const;

	/*  按索引访问（只读，防止外部绕过定位语义乱改） */
	const AlgorithmResult& resultAt(int index) const;

	/*  按三元组定位索引；未命中返回 -1 */
	int  indexOf(const QString& engine, const QString& toolId, const QString& resultId) const;

	/*  汇总全部结果中的所有检测框（跨结果展平，供只读渲染遍历） */
	QList<DetectionBox> allDetections() const;

private:
	QList<AlgorithmResult> m_results;
};
