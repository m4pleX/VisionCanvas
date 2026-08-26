/*
 *  文件名：ScaleConfig.h
 *  职责：画布缩放参数与夹紧逻辑的唯一来源
 *
 *  为什么单独成文件：
 *    缩放区间/步进系数散落在 ImageCanvasView 与 ToolbarController 中，
 *    曾出现 4 处 qBound(0.2, x, 5.0) 各自硬编码。改一处漏一处会导致
 *    "标签显示与画布实际变换不一致"。
 *
 *  约束：
 *    - 所有缩放入口（Ctrl+滚轮 / 工具栏按钮 / 适配窗口 / 工具栏显示）
 *      必须经由 ScaleConfig::clamp()，禁止再写裸 qBound。
 *    - 修改缩放范围只需改本文件，全局生效。
 */
#pragma once

#include <QtGlobal>

namespace ScaleConfig
{
	// ===== 缩放区间（唯一来源）=====
	constexpr double MinScale     = 0.05;   // 最小倍率
	constexpr double MaxScale     = 25.0;   // 最大倍率
	constexpr double DefaultScale = 1.0;   // 默认 / 复位倍率

	// ===== 步进参数 =====
	constexpr double WheelStepUp   = 1.1;  // Ctrl+滚轮 放大系数
	constexpr double WheelStepDown = 0.9;  // Ctrl+滚轮 缩小系数
	constexpr double ButtonStep    = 0.01; // 工具栏 + / - 按钮单步增量

	/*  夹紧缩放值到 [MinScale, MaxScale] */
	inline double clamp(double v)
	{
		return qBound(MinScale, v, MaxScale);
	}
}
