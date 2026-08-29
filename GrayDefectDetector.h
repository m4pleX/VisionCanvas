/*
 * 文件名：GrayDefectDetector.h
 * 职责：灰度表面缺陷检测器（真实 OpenCV 实现，替换第一阶段 SimulatorDetector 假数据）
 *
 * 核心功能：
 *   - detect(rgb)：对输入图像做灰度缺陷检测，返回 AlgorithmResult（缺陷框）
 *
 * 算法思路（面向 NEU-DET 等灰度钢材表面缺陷）：
 *   灰度化 → 高斯去噪 → 阈值分割(分离缺陷与背景) → 形态学去噪
 *   → findContours 提取连通域 → 按面积/尺寸过滤 → boundingRect 生成 DetectionBox
 *
 * 设计约束：
 *   - 纯函数，接收 cv::Mat，返回 AlgorithmResult，不接触 Qt GUI/场景；
 *   - 阈值与面积参数集中为可调常量，便于后续针对不同缺陷类型微调。
 */
#pragma once

#include "AlgorithmResult.h"

namespace cv { class Mat; }

namespace GrayDefectDetector
{
	/*  灰度缺陷检测：返回缺陷框（label = "defect"，confidence 按面积归一化估算） */
	AlgorithmResult detect(const cv::Mat& rgb);
}
