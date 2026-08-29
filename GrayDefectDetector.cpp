#include "GrayDefectDetector.h"

#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

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

	// 1. 灰度化 + 高斯去噪
	cv::Mat gray;
	if (rgb.channels() == 3)
		cv::cvtColor(rgb, gray, cv::COLOR_BGR2GRAY);
	else
		gray = rgb;

	cv::Mat blur;
	cv::GaussianBlur(gray, blur, cv::Size(5, 5), 0);

	// 2. 阈值分割：分离缺陷与背景。
	//    缺陷可能比背景亮(划痕)也可能暗(麻面)，用 Otsu 自适应阈值。
	//    因表面缺陷通常占比较小，Otsu 会把背景主体分对，缺陷落在相反侧。
	cv::Mat bin;
	double otsuVal = cv::threshold(blur, bin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

	// 3. 判断缺陷是亮侧还是暗侧：比较均值，缺陷通常是小占比、偏离均值的部分。
	//    简化策略：同时取亮缺陷和暗缺陷，分别处理。
	cv::Mat maskBright = bin;                                  // 亮缺陷（大于 Otsu 阈值）
	cv::Mat maskDark;
	cv::threshold(blur, maskDark, otsuVal * 0.5, 255, cv::THRESH_BINARY_INV); // 暗缺陷

	// 4. 形态学去噪（开运算去小噪点）
	cv::Mat kSmall = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
	cv::morphologyEx(maskBright, maskBright, cv::MORPH_OPEN, kSmall);
	cv::morphologyEx(maskDark, maskDark, cv::MORPH_OPEN, kSmall);

	// 5. 提取连通域，过滤小面积噪声，生成检测框
	auto extractBoxes = [&result, &rgb](const cv::Mat& mask, const char* label) {
		std::vector<std::vector<cv::Point>> contours;
		cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

		// 面积阈值：小于图总面积 0.02% 视为噪声
		const double minArea = rgb.cols * rgb.rows * 0.0002;

		int count = 0;
		for (const auto& c : contours)
		{
			double area = cv::contourArea(c);
			if (area < minArea)
				continue;
			cv::Rect r = cv::boundingRect(c);

			DetectionBox box;
			box.classId    = count++;
			box.label      = QString::fromUtf8(label);
			box.cx = r.x + r.width / 2.0;
			box.cy = r.y + r.height / 2.0;
			box.w  = r.width;
			box.h  = r.height;
			// 置信度：用面积相对整图占比的 log 归一化近似（仅用于演示）
			double ratio = area / (rgb.cols * rgb.rows);
			box.confidence = cv::saturate_cast<double>(1.0 - 10.0 * ratio);
			box.confidence = box.confidence < 0.2 ? 0.2 : box.confidence;
			result.detections.append(box);
		}
	};

	extractBoxes(maskBright, "bright_defect");
	extractBoxes(maskDark, "dark_defect");

	return result;
}
