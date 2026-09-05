/*
 * 文件名：VisionTools.h
 * 职责：现有视觉工具的具体实现（ITool 子类，第一版 3 个）
 *
 * 粒度约定：每个 ITool 子类 = 「一个可独立执行的视觉任务」（对齐海康 VM 流程模块），
 * 不是算子级。第一版只落地已验证过的 3 个：
 *   - LocateTool    -> PoseLocator（blob 定位）
 *   - GrayDefectTool-> GrayDefectDetector（灰度缺陷检测）
 *   - CaliperTool   -> CaliperDetector（卡尺直线）
 */
#pragma once

#include "ITool.h"

/*  定位工具：blob 定位（产 PoseResult） */
class LocateTool : public ITool
{
public:
	QString name() const override { return QStringLiteral("blobLocator"); }
	ToolCategory category() const override { return Category_Locate; }
	ToolResult run(const ToolContext& ctx) override;

	// 参数：阈值等（第一版用默认，保留 load/save 骨架）
	void loadParams(const QJsonObject& params) override { Q_UNUSED(params); }
	QJsonObject saveParams() const override { return QJsonObject(); }
};

/*  检测工具：灰度缺陷检测（产 DetectionBox） */
class GrayDefectTool : public ITool
{
public:
	QString name() const override { return QStringLiteral("grayDefect"); }
	ToolCategory category() const override { return Category_Inspect; }
	ToolResult run(const ToolContext& ctx) override;

	void loadParams(const QJsonObject& params) override { Q_UNUSED(params); }
	QJsonObject saveParams() const override { return QJsonObject(); }
};

/*  测量工具：卡尺直线（产 Geom_Segment + 边缘散点） */
class CaliperTool : public ITool
{
public:
	QString name() const override { return QStringLiteral("caliperLine"); }
	ToolCategory category() const override { return Category_Measure; }
	ToolResult run(const ToolContext& ctx) override;

	void loadParams(const QJsonObject& params) override { m_params = params; }
	QJsonObject saveParams() const override { return m_params; }

private:
	QJsonObject m_params;   /*  第一版：卡尺框端点 p1x/p1y/p2x/p2y 可经 params 配置 */
};

/*  工厂：按算法名创建工具实例（未来可扩展为注册表/大类分层） */
ITool* createToolByName(const QString& name);
