/*
 * 文件名：SimulatorDetector.h
 * 职责：模拟检测器（纯假数据，第一阶段验证算法结果契约与上屏链路）
 *
 * 核心功能：
 *   - simulateDetect(imageW, imageH)：基于图像尺寸生成若干假 DetectionBox，
 *     返回符合 AlgorithmResult 契约的检测结果，不依赖 OpenCV / 帧源。
 *
 * 设计约束：
 *   - 纯函数、零 Qt GUI 依赖，仅依赖 AlgorithmResult.h；
 *   - 第二阶段（OpenCV 集成）时，仅需替换本实现，接口保持不变（开闭原则）。
 */
#pragma once

#include "AlgorithmResult.h"

namespace SimulatorDetector
{
	/*  生成模拟检测结果：按图像尺寸比例生成 4 个假检测框 */
	AlgorithmResult simulateDetect(int imageW, int imageH);
}
