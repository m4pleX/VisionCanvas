#include "VisionTools.h"

#include "PoseLocator.h"
#include "GrayDefectDetector.h"
#include "CaliperDetector.h"

/* ===== 定位：blob 定位 ===== */
ToolResult LocateTool::run(const ToolContext& ctx)
{
	ToolResult r;
	if (!ctx.image || ctx.image->empty())
	{
		r.ok = false;
		r.error = QStringLiteral("empty image");
		return r;
	}
	r.data = PoseLocator::locate(*ctx.image);
	return r;
}

/* ===== 检测：灰度缺陷 ===== */
ToolResult GrayDefectTool::run(const ToolContext& ctx)
{
	ToolResult r;
	if (!ctx.image || ctx.image->empty())
	{
		r.ok = false;
		r.error = QStringLiteral("empty image");
		return r;
	}
	r.data = GrayDefectDetector::detect(*ctx.image);
	return r;
}

/* ===== 测量：卡尺直线 ===== */
ToolResult CaliperTool::run(const ToolContext& ctx)
{
	ToolResult r;
	if (!ctx.image || ctx.image->empty())
	{
		r.ok = false;
		r.error = QStringLiteral("empty image");
		return r;
	}

	const float cx = static_cast<float>(ctx.image->cols) / 2.0f;
	const float cy0 = static_cast<float>(ctx.image->rows) * 0.1f;
	const float cy1 = static_cast<float>(ctx.image->rows) * 0.9f;

	cv::Point2f p1(
		static_cast<float>(m_params.value("p1x").toDouble(cx)),
		static_cast<float>(m_params.value("p1y").toDouble(cy0)));
	cv::Point2f p2(
		static_cast<float>(m_params.value("p2x").toDouble(cx)),
		static_cast<float>(m_params.value("p2y").toDouble(cy1)));

	r.data = CaliperDetector::findLine(*ctx.image, p1, p2);
	return r;
}

/* ===== 工厂 ===== */
ITool* createToolByName(const QString& name)
{
	if (name == "blobLocator")  return new LocateTool();
	if (name == "grayDefect")   return new GrayDefectTool();
	if (name == "caliperLine")  return new CaliperTool();
	return nullptr;
}
