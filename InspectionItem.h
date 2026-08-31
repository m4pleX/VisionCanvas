/*
 *  文件名：InspectionItem.h
 *  职责：检测项数据模型（ROI 与算法的关联层）
 *
 *  定位：
 *    - 检测项 = 算法类型 + 引用的 ROI 集合 + 参数 + 判定规则；
 *    - 是"算法用哪个 ROI 实例"的答案：item.roiIds 引用 roiList 中的 ROI id；
 *    - 值语义（与 DrawShapeItem 的指针语义区分），供 RecipeIO 序列化。
 *
 *  字段语义：
 *    id            检测项唯一标识
 *    name          检测项名称（UI 展示）
 *    algorithmType 算法类型（"grayDefect" / 未来 "measure" / "yolo" ...）
 *    roiIds        引用的 ROI id 列表（多对一：一个检测项用多个 ROI）
 *    params        算法参数（QJsonObject 通用容器，避免过早绑定具体参数结构体）
 *    passRule      判定规则（OK/NG，QJsonObject 通用容器）
 *
 *  设计约束：
 *    - 纯数据模型，不接触 UI / 渲染 / 算法实现；
 *    - params/passRule 用通用容器承载，遵循 Rule of Three，避免过早绑定具体算法参数形状。
 */
#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>

struct InspectionItem
{
	QString      id;
	QString      name;
	QString      algorithmType;
	QStringList  roiIds;      /*  引用的 ROI id（对应 DrawShapeItem::id） */
	QJsonObject  params;      /*  算法参数（通用容器，避免过早绑定具体结构体） */
	QJsonObject  passRule;    /*  判定规则（OK/NG） */
};
