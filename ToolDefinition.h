/*
 * 文件名：ToolDefinition.h
 * 职责：视觉工具流程编排的数据模型（大类 + 算法 tag + 参数）
 *
 * 设计定位（对齐海康 VisionMaster / VisionPro 的任务大类分类）：
 *   - 编排层 = 视觉【任务大类】（采集/定位/检测/测量/识别/输出），
 *     不是细粒度算子；算法选择（细分实现）藏在大类的「algorithm」字段里；
 *   - 流程 = 工具步骤的有序列表（顺序 = 执行顺序，第一版仅线性）；
 *   - 参数用 QJsonObject 通用容器，避免过早绑定各算法参数结构体
 *     （Rule of Three：等参数形状稳定后再强类型化）。
 *
 * 与本项目资产的映射：
 *   定位(blob)      -> PoseLocator
 *   检测(灰度缺陷)   -> GrayDefectDetector
 *   测量(卡尺直线)   -> CaliperDetector
 *   校正(跟随)      -> ShapeGeometry::applyPose，内化在「定位」大类，不独立成类
 */
#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>

/*  工具任务大类（对齐工业视觉标准分类） */
enum ToolCategory
{
	Category_Acquire = 0,   /*  采集/图源（占位，不实现） */
	Category_Locate,        /*  定位：找工件位置/姿态 */
	Category_Inspect,       /*  检测：判缺陷/存在/质量 */
	Category_Measure,       /*  测量：尺寸/距离/角度 */
	Category_Identify,      /*  识别：OCR/读码（占位） */
	Category_Output         /*  输出：结果/报表/IO（占位） */
};

/*  大类 -> 显示名 */
inline QString toolCategoryName(ToolCategory c)
{
	switch (c)
	{
	case Category_Acquire:  return QStringLiteral("采集");
	case Category_Locate:   return QStringLiteral("定位");
	case Category_Inspect:  return QStringLiteral("检测");
	case Category_Measure:  return QStringLiteral("测量");
	case Category_Identify: return QStringLiteral("识别");
	case Category_Output:   return QStringLiteral("输出");
	}
	return QStringLiteral("未知");
}

/*  单个工具步骤：一个大类 + 选定的细分算法 + 参数 */
struct ToolStep
{
	QString      id;         /*  本步骤唯一标识（结果按此定位） */
	ToolCategory category;   /*  任务大类 */
	QString      algorithm;  /*  细分算法 tag（如 "blobLocator"/"grayDefect"/"caliperLine"） */
	QStringList  inputs;     /*  引用上游哪些步骤的 id（第一版默认=上一个，空表示吃整图） */
	QJsonObject  params;     /*  算法参数（通用容器） */
};
