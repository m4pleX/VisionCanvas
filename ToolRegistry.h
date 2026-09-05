/*
 * 文件名：ToolRegistry.h
 * 职责：视觉工具注册表（算法 tag -> 执行函数 查表）+ 流程执行上下文
 *
 * 设计定位：
 *   - 把「细分算法」通过字符串 tag 注册到查表，运行时按 tag dispatch；
 *   - 统一执行契约：每个算法 = 吃 FlowContext（图 + 上游结果）+ 参数 JSON
 *     -> 产 AlgorithmResult；与 PoseLocator/GrayDefectDetector/CaliperDetector
 *     的「吃图产结果」签名对齐；
 *   - 不引入继承/工厂体系（Rule of Three：算法个数尚少，查表足够）。
 *
 * 元信息（供 UI 列出某大类下的算法）：
 *   ToolMeta { category, algorithm, displayName }
 */
#pragma once

#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>
#include <QVariant>
#include <QJsonObject>
#include <functional>

#include "AlgorithmResult.h"
#include "ToolDefinition.h"

#include <opencv2/core.hpp>   /*  FlowContext 持有 cv::Mat 值成员，需完整定义 */

/*  流程执行上下文：图引用贯穿 + 各步骤结果挂接 */
struct FlowContext
{
	cv::Mat                          image;             /*  当前图像（引用贯穿，不复制大图） */
	QHash<QString, AlgorithmResult>  results;           /*  步骤 id -> 结果（上游产出供下游吃） */
	int                              sourceWidth  = 0;
	int                              sourceHeight = 0;
};

/*  统一执行契约：算法实现 = 吃 context + 参数 -> 产 AlgorithmResult */
using ToolExecutor = std::function<AlgorithmResult(FlowContext&, const QJsonObject& params)>;

/*  算法元信息（注册时登记，供 UI 枚举） */
struct ToolMeta
{
	ToolCategory category;
	QString      algorithm;     /*  tag */
	QString      displayName;   /*  显示名 */
};

/*  工具注册表：算法 tag -> 执行函数 + 元信息 */
class ToolRegistry
{
public:
	/*  注册一个算法（挂在大类下） */
	void registerTool(ToolCategory category, const QString& algorithm,
	                  const QString& displayName, ToolExecutor exec)
	{
		ToolMeta meta{ category, algorithm, displayName };
		m_metas[algorithm] = meta;
		m_executors[algorithm] = exec;
	}

	/*  是否已注册某算法 */
	bool hasAlgorithm(const QString& algorithm) const
	{
		return m_executors.contains(algorithm);
	}

	/*  执行：按 tag 查表，找不到返回空 AlgorithmResult */
	AlgorithmResult execute(const QString& algorithm, FlowContext& ctx,
	                        const QJsonObject& params) const
	{
		AlgorithmResult empty;
		auto it = m_executors.constFind(algorithm);
		if (it == m_executors.constEnd())
			return empty;
		return it.value()(ctx, params);
	}

	/*  某大类下的算法 tag 列表（供 UI 枚举，保持注册顺序） */
	QStringList algorithmsIn(ToolCategory category) const
	{
		QStringList out;
		for (auto it = m_metas.constBegin(); it != m_metas.constEnd(); ++it)
		{
			if (it.value().category == category)
				out << it.key();
		}
		return out;
	}

	/*  算法 tag -> 显示名 */
	QString displayName(const QString& algorithm) const
	{
		auto it = m_metas.constFind(algorithm);
		return it == m_metas.constEnd() ? algorithm : it.value().displayName;
	}

private:
	QHash<QString, ToolMeta>     m_metas;
	QHash<QString, ToolExecutor> m_executors;
};

/*  构造默认注册表：把现有工具接入（第一版固定默认参数，参数 UI 后补） */
ToolRegistry makeDefaultRegistry();

/*  顺序执行一条流程：按 steps 顺序逐个执行，结果挂到 ctx.results。
 *  这是最简线性调度（海康式流程树的核心）；分支/并行留待后续。 */
void runFlow(const ToolRegistry& registry, QList<ToolStep>& steps, FlowContext& ctx);
