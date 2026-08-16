/*
 *  文件名：AnnotationIO.h
 *  职责：标注数据序列化/反序列化（内存几何模型 <-> JSON 文件）
 *
 *  依赖：DrawShapeData（纯几何数据模型），不依赖任何渲染类
 *  设计约束：
 *    本类只做数据层转换，不接触 QGraphicsItem、场景、UI；
 *    保证"标注文件 -> 几何数据 -> 重建图形"的闭环。
 */
#pragma once

#include <QJsonObject>
#include <QList>
#include "DrawShapeData.h"

class AnnotationIO
{
public:
	/*  单个形状 -> JSON 对象（含 type 标识） */
	static QJsonObject shapeToJson(const DrawShapeItem& s);

	/*  JSON 对象 -> 形状；解析失败返回 nullptr */
	static DrawShapeItem* shapeFromJson(const QJsonObject& o);
};
