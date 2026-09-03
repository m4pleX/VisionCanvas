#include "PoseLocator.h"

#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

AlgorithmResult PoseLocator::locate(const cv::Mat& rgb, const PoseLocateParams& params)
{
	AlgorithmResult result;
	result.engine   = "opencv";
	result.toolId   = "poseLocator";
	result.resultId = "locate";
	result.sourceWidth  = rgb.cols;
	result.sourceHeight = rgb.rows;

	if (rgb.empty())
		return result;

	// 1. 灰度化 + 轻去噪
	cv::Mat gray;
	if (rgb.channels() == 3)
		cv::cvtColor(rgb, gray, cv::COLOR_BGR2GRAY);
	else
		gray = rgb;

	cv::Mat smooth;
	cv::GaussianBlur(gray, smooth, cv::Size(3, 3), 0);

	// 2. 前景分割：目标显著区别于背景（阈值可调，支持明/暗目标）
	cv::Mat mask;
	cv::inRange(smooth, params.threshLow, params.threshHigh, mask);
	if (params.invert)
		cv::bitwise_not(mask, mask);

	// 3. 闭运算合并断续碎片
	const cv::Mat kMerge = cv::getStructuringElement(cv::MORPH_ELLIPSE, { params.mergeKernel, params.mergeKernel });
	cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kMerge);

	// 4. 连通域提取，取面积最大者作为定位目标
	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	const double imgArea = static_cast<double>(rgb.cols) * rgb.rows;
	const double minArea = imgArea * params.minAreaRatio;

	const std::vector<cv::Point>* best = nullptr;
	double bestArea = 0.0;
	for (const auto& c : contours)
	{
		const double a = cv::contourArea(c);
		if (a < minArea)
			continue;
		if (a > bestArea)
		{
			bestArea = a;
			best = &c;
		}
	}

	if (best == nullptr)
		return result;   // 无有效目标，定位失败（poses 为空）

	// 5. 最小外接旋转矩形 → 中心 + 旋转角（度数 → 弧度）
	const cv::RotatedRect rr = cv::minAreaRect(*best);

	Pose2D pose;
	pose.tx    = rr.center.x;
	pose.ty    = rr.center.y;
	pose.angle = rr.angle * CV_PI / 180.0;   /* 度数 → 弧度，对齐 Pose2D 契约 */
	pose.scale = 1.0;
	pose.score = (bestArea / imgArea < 1.0) ? bestArea / imgArea : 0.99;   /* 面积占比归一化 */

	PoseResult pr;
	pr.classId = 0;
	pr.label   = QStringLiteral("blob");
	pr.pose    = pose;
	result.poses.append(pr);

	return result;
}
