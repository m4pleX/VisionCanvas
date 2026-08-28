/*
 * 文件名：ParamFieldFactory.h
 * 职责：参数面板字段 schema 的纯静态工厂（从 ImageCanvasView::buildParamFields 抽离）
 *
 * 核心功能：
 *   - 按 DrawShapeType 生成对应的 ParamField 列表（用于参数面板 UI 生成与读写）
 *   - 多边形(Shape_Polygon)的字段由顶点动态生成，不在本工厂内静态构造
 *
 * 依赖：DrawShapeData（几何数据）、ParamPanelWidget.h（ParamField 定义）
 * 设计约束：
 *   - 纯静态、零状态、零 this 依赖，是"零副作用工具类"的样板
 *   - 不接触 QGraphicsScene / UI 控件 / 主类成员，仅做 type -> schema 映射
 */
#pragma once

#include <QList>
#include "DrawShapeData.h"
#include "ParamPanelWidget.h"

class ParamFieldFactory
{
public:
	/*  按图形类型生成参数字段 schema（纯函数） */
	static QList<ParamField> buildFields(DrawShapeType type);
};
