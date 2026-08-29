#include "GrayDefectDetector.h"

#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace
{
	// ==== 可调参数（灰度阈值方案，面向"干净背景 + 明显亮/暗缺陷"） ====
	constexpr int    kDarkLow    = 0;    // 暗缺陷灰度上界（<80 判为暗缺陷）
	constexpr int    kDarkHigh   = 80;
	constexpr int    kBrightLow  = 180;  // 亮缺陷灰度下界（>180 判为亮缺陷）
	constexpr int    kBrightHigh = 255;
	constexpr int    kMergeKernel    = 5;   // 闭运算核：合并断续碎片
	constexpr double kMinContourArea = 30.0; // 最小轮廓面积（像素），过滤孤立噪声
	constexpr double kMaxBoxRatio    = 0.4;  // 检测框占整图比例上限，防背景整体误检
	constexpr double kConfFullGain   = 120.0; // 缺陷像素灰度偏离背景的量归一化用
}

AlgorithmResult GrayDefectDetector::detect(const cv::Mat& rgb)
{
	AlgorithmResult result;
	result.engine   = "opencv";
	result.toolId   = "grayDefectDetector";
	result.resultId = "detect";
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

	// 2. 灰度区间分割（inRange）：
	//    暗缺陷 = 灰度 < kDarkHigh；亮缺陷 = 灰度 > kBrightLow。
	//    背景为中间灰度的场景下，直方图三峰清晰，双区间精确分离亮/暗缺陷。
	cv::Mat maskDark, maskBright;
	cv::inRange(smooth, kDarkLow, kDarkHigh, maskDark);
	cv::inRange(smooth, kBrightLow, kBrightHigh, maskBright);

	// 3. 闭运算合并断续碎片
	const cv::Mat kMerge = cv::getStructuringElement(cv::MORPH_ELLIPSE, { kMergeKernel, kMergeKernel });
	cv::morphologyEx(maskDark, maskDark, cv::MORPH_CLOSE, kMerge);
	cv::morphologyEx(maskBright, maskBright, cv::MORPH_CLOSE, kMerge);

	// 4. 连通域提取 + 几何过滤 + 置信度（缺陷像素灰度偏离背景程度的归一化）
	auto extractBoxes = [&](const cv::Mat& mask, const char* label) {
		std::vector<std::vector<cv::Point>> contours;
		cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

		const double maxBoxArea = rgb.cols * rgb.rows * kMaxBoxRatio;

		int count = 0;
		for (const auto& c : contours)
		{
			if (cv::contourArea(c) < kMinContourArea)
				continue;

			const cv::Rect r = cv::boundingRect(c);
			if (r.area() > maxBoxArea)
				continue; // 背景整体翻转等异常

			// 置信度：框内缺陷像素相对背景灰度的平均偏离量归一化
			const double bg = 128.0; // 基准背景灰度（合成图/均匀背景场景）
			double dev = cv::mean(smooth(r), mask(r))[0];
			double conf = std::abs(dev - bg) / (kConfFullGain * 0.5) + 0.2;
			conf = conf < 0.2 ? 0.2 : conf;
			conf = conf > 0.99 ? 0.99 : conf;

			DetectionBox box;
			box.classId    = count++;
			box.label      = QString::fromUtf8(label);
			box.cx = r.x + r.width / 2.0;
			box.cy = r.y + r.height / 2.0;
			box.w  = r.width;
			box.h  = r.height;
			box.confidence = conf;
			result.detections.append(box);
		}
	};

	extractBoxes(maskBright, "bright_defect");
	extractBoxes(maskDark, "dark_defect");

	return result;
}
