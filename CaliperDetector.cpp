#include "CaliperDetector.h"

#include <vector>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace
{
	/*  抛物线插值求一维梯度曲线的亚像素峰值位置。
	 *  输入梯度序列 grad，峰值整数索引 i（1<=i<=n-2）；
	 *  用 i-1/i/i+1 三点二次拟合，峰值偏移 delta = 0.5*(y0-y2)/(y0-2y1+y2)，
	 *  返回 i+delta（亚像素，限制在 [-0.5,0.5]）。 */
	double subpixelPeak(const std::vector<double>& grad, int i)
	{
		const double y0 = grad[i - 1], y1 = grad[i], y2 = grad[i + 1];
		const double denom = y0 - 2.0 * y1 + y2;
		if (std::abs(denom) < 1e-9)
			return static_cast<double>(i);
		double delta = 0.5 * (y0 - y2) / denom;
		if (delta < -0.5) delta = -0.5;
		if (delta >  0.5) delta =  0.5;
		return static_cast<double>(i) + delta;
	}

	/*  双线性采样灰度（越界返回 0） */
	double sampleGray(const cv::Mat& gray, double x, double y)
	{
		const int x0 = static_cast<int>(std::floor(x));
		const int y0 = static_cast<int>(std::floor(y));
		if (x0 < 0 || y0 < 0 || x0 + 1 >= gray.cols || y0 + 1 >= gray.rows)
			return 0.0;
		const double fx = x - x0, fy = y - y0;
		const double v00 = gray.at<uchar>(y0, x0);
		const double v10 = gray.at<uchar>(y0, x0 + 1);
		const double v01 = gray.at<uchar>(y0 + 1, x0);
		const double v11 = gray.at<uchar>(y0 + 1, x0 + 1);
		return (v00 * (1 - fx) + v10 * fx) * (1 - fy)
		     + (v01 * (1 - fx) + v11 * fx) * fy;
	}
}

AlgorithmResult CaliperDetector::findLine(const cv::Mat& rgb,
                                          const cv::Point2f& p1, const cv::Point2f& p2,
                                          const CaliperParams& params)
{
	AlgorithmResult result;
	result.engine   = "opencv";
	result.toolId   = "caliperDetector";
	result.resultId = "findLine";
	result.sourceWidth  = rgb.cols;
	result.sourceHeight = rgb.rows;

	if (rgb.empty())
		return result;

	// 灰度化
	cv::Mat gray;
	if (rgb.channels() == 3)
		cv::cvtColor(rgb, gray, cv::COLOR_BGR2GRAY);
	else
		gray = rgb;

	const double dx = static_cast<double>(p2.x - p1.x);
	const double dy = static_cast<double>(p2.y - p1.y);
	const double len = std::sqrt(dx * dx + dy * dy);
	if (len < 1e-6)
		return result;

	// 长轴单位方向 t；搜索方向 n = 垂直于 t
	const double tx = dx / len, ty = dy / len;
	const double nx = -ty, ny = tx;

	const int N = params.caliperCount < 2 ? 2 : params.caliperCount;
	const int Lhalf = static_cast<int>(std::round(params.projectionLen));
	if (Lhalf <= 0)
		return result;
	const int wHalf = params.projectionWidth < 1 ? 1 : params.projectionWidth;

	std::vector<double> edgeDist;    /*  亚像素偏移（沿搜索方向，相对长轴中心） */
	std::vector<cv::Point2f> edgePts;

	// 对每个采样点：垂直于长轴采一条投影线 → 一维灰度 → 平滑 → 梯度 → 亚像素峰值
	for (int k = 0; k < N; ++k)
	{
		const double s = (k + 0.5) / N;   /*  采样比例，避开端点 */
		const double cx = p1.x + dx * s;
		const double cy = p1.y + dy * s;

		// 一维灰度剖面：d 从 -Lhalf .. +Lhalf（步长 1 像素），沿 n 方向，
		// 并对垂直于 n 的宽度方向（±wHalf）做平均
		const int M = 2 * Lhalf + 1;
		std::vector<double> profile(M, 0.0);
		for (int j = 0; j < M; ++j)
		{
			const double d = j - Lhalf;              /*  沿搜索方向的偏移 */
			double acc = 0.0; int cnt = 0;
			for (int w = -wHalf; w <= wHalf; ++w)
			{
				const double px = cx + nx * d + tx * w;
				const double py = cy + ny * d + ty * w;
				acc += sampleGray(gray, px, py);
				++cnt;
			}
			profile[j] = acc / cnt;
		}

		// 高斯平滑（一维，简单 3 点迭代近似）
		if (params.sigma > 0)
		{
			std::vector<double> tmp = profile;
			const int iters = params.sigma >= 1.5 ? 2 : 1;
			for (int it = 0; it < iters; ++it)
			{
				for (int j = 1; j < M - 1; ++j)
					tmp[j] = 0.25 * profile[j - 1] + 0.5 * profile[j] + 0.25 * profile[j + 1];
				profile = tmp;
			}
		}

		// 一阶梯度（中心差分）
		std::vector<double> grad(M, 0.0);
		for (int j = 1; j < M - 1; ++j)
			grad[j] = (profile[j + 1] - profile[j - 1]) * 0.5;

		// 找 |梯度| 最大的峰值（亚像素），并按极性过滤
		int bestIdx = -1;
		double bestAbs = params.edgeThreshold;
		for (int j = 1; j < M - 1; ++j)
		{
			const double g = grad[j];
			bool ok = (params.polarity == Polarity_All)
			       || (params.polarity == Polarity_Positive && g > 0)
			       || (params.polarity == Polarity_Negative && g < 0);
			if (!ok) continue;
			if (std::abs(g) > bestAbs)
			{
				bestAbs = std::abs(g);
				bestIdx = j;
			}
		}
		if (bestIdx < 0)
			continue;   /*  该采样点未找到满足条件的边缘 */

		const double dSub = subpixelPeak(grad, bestIdx);   /*  亚像素索引 */
		const double dOff = dSub - Lhalf;                  /*  相对长轴中心偏移 */
		edgeDist.push_back(dOff);
		edgePts.push_back(cv::Point2f(
			static_cast<float>(cx + nx * dOff),
			static_cast<float>(cy + ny * dOff)));
	}

	// 有效点占比不足 → 拟合失败
	if (edgePts.size() < static_cast<size_t>(N * params.minPointsRatio) || edgePts.size() < 2)
		return result;

	// cv::fitLine 鲁棒拟合（M-estimator）
	cv::Vec4f line;
	cv::fitLine(edgePts, line, cv::DIST_HUBER, 0, 0.01, 0.01);
	const double vx = line[0], vy = line[1];   /*  方向向量（单位） */
	const double ox = line[2], oy = line[3];   /*  直线上一点 */

	// 端点 = p1、p2 在拟合直线上的垂直投影
	auto project = [&](cv::Point2f pt) -> cv::Point2f {
		const double relx = static_cast<double>(pt.x - ox);
		const double rely = static_cast<double>(pt.y - oy);
		const double t = relx * vx + rely * vy;
		return cv::Point2f(static_cast<float>(ox + vx * t),
		                   static_cast<float>(oy + vy * t));
	};
	const cv::Point2f A = project(p1);
	const cv::Point2f B = project(p2);

	// 拟合质量：内点占比（点到拟合直线垂直距离 < 阈值视为内点）
	double inlierRatio = 0.0;
	{
		int inlier = 0;
		for (const auto& p : edgePts)
		{
			const double relx = static_cast<double>(p.x - ox);
			const double rely = static_cast<double>(p.y - oy);
			const double cross = relx * vy - rely * vx;   /*  点到直线距离 */
			if (std::abs(cross) < 2.0)
				++inlier;
		}
		inlierRatio = static_cast<double>(inlier) / edgePts.size();
	}

	// 结果几何：线段（拟合直线）
	ResultGeom lineGeom;
	lineGeom.geom.type = Geom_Segment;
	lineGeom.classId   = 0;
	lineGeom.label     = QStringLiteral("line");
	lineGeom.score     = inlierRatio;
	lineGeom.toolId    = "caliperDetector";
	lineGeom.geom.cx   = A.x; lineGeom.geom.cy = A.y;
	lineGeom.geom.ex   = B.x; lineGeom.geom.ey = B.y;
	result.geometry.append(lineGeom);

	// 结果几何：边缘散点（轮廓，供回显）
	ResultGeom ptsGeom;
	ptsGeom.geom.type  = Geom_Contour;
	ptsGeom.classId    = 0;
	ptsGeom.label      = QStringLiteral("edge_points");
	ptsGeom.score      = 1.0;
	ptsGeom.toolId     = "caliperDetector";
	for (const auto& p : edgePts)
		ptsGeom.geom.contour.append(QPointF(p.x, p.y));
	result.geometry.append(ptsGeom);

	return result;
}
