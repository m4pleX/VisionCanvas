/*
 * 文件名：PoseLocator.h
 * 职责：2D 定位器（OpenCV 真实实现，最小可行：找最大连通域 → 位姿）
 *
 * 核心功能：
 *   - locate(rgb)：对输入图像做定位，返回 AlgorithmResult（poses 列表）
 *
 * 算法思路（最小真实实现，用于验证「图像 → Pose2D」链路）：
 *   灰度化 → 高斯去噪 → inRange 前景分割(目标相对背景的灰度) → 闭运算合并碎片
 *   → findContours 提取连通域 → 取面积最大的连通域 → minAreaRect 求最小外接旋转矩形
 *   → 中心 + 旋转角 产出 Pose2D（tx/ty/angle/score）
 *
 * 已知边界（显式记录，不在此步解决）：
 *   - 用 minAreaRect 的角度对近似对称/无规则的 blob 稳定性差，
 *     需要稳定角度时应引入模板匹配 / 找边 / 找圆等更精确的定位算法；
 *   - 前景/背景分割阈值按 params 可调，缺省面向「目标显著区别于背景」的场景。
 *
 * 设计约束：
 *   - 纯函数，接收 cv::Mat，返回 AlgorithmResult，不接触 Qt GUI/场景；
 *   - 与 GrayDefectDetector 职责分离：定位产出 PoseResult，检测产出 DetectionBox；
 *   - 角度统一换算为【弧度】，对齐 Pose2D 契约与 applyPose 的旋转约定。
 */
#pragma once

#include "AlgorithmResult.h"

namespace cv { class Mat; }

namespace PoseLocator
{
	/*  定位的可调参数（面向「目标显著区别于背景」的最小实现） */
	struct PoseLocateParams
	{
		bool   invert        = false;  /*  前景是否比背景更暗（默认目标比背景亮） */
		int    threshLow     = 128;    /*  前景灰度下界 */
		int    threshHigh    = 255;    /*  前景灰度上界 */
		int    mergeKernel   = 5;      /*  闭运算核尺寸：合并断续碎片 */
		double minAreaRatio  = 0.001;  /*  最小连通域占整图面积比，过滤噪声 */
	};

	/*  定位：返回位姿（poses 列表，label = "blob"）。
	 *  无有效目标时返回空 poses（调用方据此判断定位失败）。 */
	AlgorithmResult locate(const cv::Mat& rgb, const PoseLocateParams& params = PoseLocateParams());
}
