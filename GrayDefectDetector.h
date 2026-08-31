/*
 * 文件名：GrayDefectDetector.h
 * 职责：灰度表面缺陷检测器（OpenCV 真实实现）
 *
 * 核心功能：
 *   - detect(rgb)：对输入图像做灰度缺陷检测，返回 AlgorithmResult（缺陷框）
 *
 * 算法思路（面向"干净背景 + 明显亮/暗缺陷"的灰度图，验证链路可行）：
 *   灰度化 → 高斯去噪 → inRange 双区间分割(暗缺陷 <80 / 亮缺陷 >180)
 *   → 闭运算合并碎片 → findContours 提取连通域 → 面积/占图比过滤
 *   → boundingRect 生成 DetectionBox
 *
 * 适用边界：本实现是"传统算法基线"，用于验证 AlgorithmResult 契约与上屏链路。
 *   - 对均匀背景 + 明确灰度突变的缺陷有效（本方案）；
 *   - 对复杂纹理缺陷（NEU-DET 划痕/裂纹等）非所长，应交给后续 YOLO/UNet 层。
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
