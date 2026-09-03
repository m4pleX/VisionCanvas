/*
 * 文件名：CaliperDetector.h
 * 职责：卡尺直线检测器（OpenCV 真实实现，最小可行版）
 *
 * 核心功能：
 *   - findLine(rgb, p1, p2, params)：在卡尺框内找一条直线边缘，
 *     产出 AlgorithmResult（geometry = 一条 Geom_Segment 拟合直线 + 边缘散点）
 *
 * 算法思路（对齐 Halcon measure_pos / VisionPro CogCaliperTool 投影范式）：
 *   卡尺框由两点 p1→p2 定义长轴（Start→End，方向决定极性 sense）；
 *   搜索方向 = 垂直于长轴；
 *   沿长轴等距取 N 个采样点，每点垂直于长轴采一条投影线，平均成一维灰度剖面
 *   → 高斯平滑 → 一阶梯度 → 抛物线插值求亚像素峰值 → 按极性过滤边缘点
 *   → cv::fitLine 鲁棒直线拟合 → 端点 = 拟合直线与卡尺框长轴两端点的垂直投影
 *
 * 已知边界（显式记录，不在本版解决）：
 *   - 直线拟合用 cv::fitLine（M-estimator 鲁棒拟合）；若后续实测对
 *     「大量离群点 / 脏污多边缘」场景不够稳，再补 RANSAC + 最小二乘内点重拟合；
 *   - 仅支持单条直线（找第一个满足极性的边缘）；找对边/多边留后续；
 *   - 直线端点限制在卡尺框长轴范围内，不延伸到框外。
 *
 * 设计约束：
 *   - 纯函数，接收 cv::Mat + 卡尺框端点 + 参数，返回 AlgorithmResult，
 *     不接触 Qt GUI/场景；
 *   - 结果几何填 ResultGeom（Geom_Segment 线段 + Geom_Contour 边缘散点），
 *     第一次真实启用 AlgorithmResult.geometry 契约。
 */
#pragma once

#include "AlgorithmResult.h"

#include <opencv2/core.hpp>   /*  需要 cv::Mat / cv::Point2f（Point2f 无法前置声明） */

namespace CaliperDetector
{
	/*  边缘极性（对齐 Halcon Transition 语义，sense 沿 p1→p2 长轴方向） */
	enum EdgePolarity
	{
		Polarity_All = 0,      /*  暗→亮、亮→暗 都算 */
		Polarity_Positive,     /*  只检测 暗→亮（灰度由低到高） */
		Polarity_Negative      /*  只检测 亮→暗（灰度由高到低） */
	};

	/*  卡尺可调参数 */
	struct CaliperParams
	{
		int    caliperCount   = 30;     /*  沿长轴的采样（投影线）数量 */
		double projectionLen  = 60.0;   /*  投影线半长（垂直于长轴两侧延伸，像素） */
		int    projectionWidth = 3;     /*  投影线宽度（平行于长轴方向的平均像素） */
		double sigma          = 1.0;    /*  一维高斯平滑 sigma */
		double edgeThreshold  = 30.0;   /*  一维梯度阈值，低于此值的峰值被过滤 */
		EdgePolarity polarity = Polarity_All;
		double minPointsRatio = 0.5;    /*  有效边缘点占比下限，低于则拟合失败 */
	};

	/*  卡尺直线检测：
	 *   p1, p2 —— 卡尺框长轴两端点（Start→End，定义长轴方向与极性 sense）
	 *   返回 AlgorithmResult：geometry 含一条线段（Geom_Segment，拟合直线）
	 *     与一条轮廓（Geom_Contour，亚像素边缘散点）；拟合失败返回空 geometry。 */
	AlgorithmResult findLine(const cv::Mat& rgb,
	                         const cv::Point2f& p1, const cv::Point2f& p2,
	                         const CaliperParams& params = CaliperParams());
}
