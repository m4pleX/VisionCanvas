#include "ToolRegistry.h"

#include <opencv2/core.hpp>
#include "PoseLocator.h"
#include "GrayDefectDetector.h"
#include "CaliperDetector.h"

ToolRegistry makeDefaultRegistry()
{
	ToolRegistry r;

	/* 定位大类：blob 定位（PoseLocator，默认参数） */
	r.registerTool(Category_Locate, "blobLocator", "Blob 定位",
		[](FlowContext& ctx, const QJsonObject&) -> AlgorithmResult {
			return PoseLocator::locate(ctx.image);
		});

	/* 检测大类：灰度缺陷检测（GrayDefectDetector，默认参数） */
	r.registerTool(Category_Inspect, "grayDefect", "灰度缺陷检测",
		[](FlowContext& ctx, const QJsonObject&) -> AlgorithmResult {
			return GrayDefectDetector::detect(ctx.image);
		});

	/* 测量大类：卡尺直线（CaliperDetector）
	 * 第一版从 params 读卡尺框端点 p1x/p1y/p2x/p2y；缺省用图中部竖直卡尺。 */
	r.registerTool(Category_Measure, "caliperLine", "卡尺直线",
		[](FlowContext& ctx, const QJsonObject& p) -> AlgorithmResult {
			const float cx = static_cast<float>(ctx.image.cols) / 2.0f;
			const float cy0 = static_cast<float>(ctx.image.rows) * 0.1f;
			const float cy1 = static_cast<float>(ctx.image.rows) * 0.9f;

			cv::Point2f p1(p.value("p1x").toDouble(cx), p.value("p1y").toDouble(cy0));
			cv::Point2f p2(p.value("p2x").toDouble(cx), p.value("p2y").toDouble(cy1));

			return CaliperDetector::findLine(ctx.image, p1, p2);
		});

	return r;
}

void runFlow(const ToolRegistry& registry, QList<ToolStep>& steps, FlowContext& ctx)
{
	for (ToolStep& step : steps)
	{
		// 同步图像尺寸到上下文（供结果 overlay 坐标换算）
		if (step.id.isEmpty())
			step.id = step.algorithm;

		const AlgorithmResult r = registry.execute(step.algorithm, ctx, step.params);
		ctx.results[step.id] = r;
	}
}
