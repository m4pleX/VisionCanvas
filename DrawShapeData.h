/*
 * 文件名：DrawShapeData.h
 * 职责：纯几何数据定义，不含任何渲染句柄
 * 核心功能：
 *   - 定义 DrawShapeType 枚举（所有形状类型）
 *   - 定义 DrawShapeItem 结构体（统一几何字段，类型无关，供所有模块共享）
 *   - 定义 ShapeDragRect（拖拽起点的几何快照，供拖拽编排使用）
 * 依赖：无（仅 QList/QPointF）
 * 注意：纯数据结构，不可引入 QGraphicsItem 等渲染类
 *
 * 字段语义（统一几何字段，按 shape.type 决定哪些字段有效）：
 *   cx,cy        中心点        Rect / RotateRect / Circle / Ellipse / Ring / Arc
 *   w,h          宽高          Rect / RotateRect
 *   r            第一半径      Circle(半径) / Ellipse(半轴r1) / Ring(外半径) / Arc(外半径 rOuter)
 *   r2           第二半径      Ellipse(半轴r2) / Ring(内半径) / Arc(内半径 rInner)
 *   angle        旋转角        RotateRect / Ellipse
 *   startAngle   起始角(度)    Arc
 *   span         跨度(度,正逆时针/负顺时针)  Arc
 *   pts          点集          Polygon
 */
#pragma once

#include <QList>
#include <QPointF>
#include <QString>

enum DrawShapeType
{
	Shape_Rect,
	Shape_RotateRect,
	Shape_Circle,
	Shape_Ellipse,
	Shape_Ring,
	Shape_Arc,
	Shape_Polygon
};

/*  拖拽起点的几何快照（与具体形状类型无关，仅存 cx/cy/w/h） */
struct ShapeDragRect
{
	double cx = 0, cy = 0, w = 0, h = 0;
};

/*  2D 位姿：定位/匹配算法的标准输出，用于后续 ROI 跟随校正。
 *
 *  单位约定：
 *    - angle 使用【弧度 radians】，与 HALCON / VisionPro 的位姿/变换数据一致，
 *      便于直接参与旋转矩阵运算（cos/sin 天然吃弧度）；
 *    - DrawShapeItem 的 angle / startAngle / span 仍使用【度】，供 UI 展示与编辑；
 *      单位换算只发生在「位姿 → ROI 校正」的边界处。
 *
 *  字段语义：
 *    tx/ty  平移分量（图像像素坐标）
 *    angle  旋转角（弧度）
 *    scale  均匀缩放（1.0 = 原尺寸；模板匹配可能非 1）
 *    score  定位置信度 / 匹配分数（0~1），供下游按精度过滤或选择基准 ROI */
struct Pose2D
{
	double tx    = 0.0;   /*  平移 X（图像坐标，像素） */
	double ty    = 0.0;   /*  平移 Y */
	double angle = 0.0;   /*  旋转角（弧度，radians） */
	double scale = 1.0;   /*  均匀缩放 */
	double score = 0.0;   /*  定位置信度（0~1） */
};

// 纯几何数据，不包含任何 QGraphicsItem 句柄
struct DrawShapeItem
{
	DrawShapeType type;

	/*  ---- 业务元信息（与几何解耦，供多实例 / ROI 语义 / 算法类别标注使用） ---- */
	QString id;              /*  唯一标识；多实例化的地基，缺失时由上层生成 */
	QString label;           /*  业务标签：ROI 用途 / YOLO class 名 / 用户自定义语义 */
	int     classId = -1;    /*  算法类别 id（-1 = 未分类） */
	QString sourceToolId;    /*  ROI 来源：空 = 人工绘制；非空 = 由该定位工具（id）校正生成 */

	/*  ---- 统一几何字段（类型无关，按 type 决定有效性） ---- */
	double cx = 0, cy = 0;        /*  中心点 */
	double w  = 0, h  = 0;        /*  宽高（Rect/RotateRect） */
	double r  = 0;                /*  第一半径：Circle.半径 / Ellipse.r1 / Ring.r1 / Arc.rOuter */
	double r2 = 0;                /*  第二半径：Ellipse.r2 / Ring.r2 / Arc.rInner */
	double angle = 0;             /*  旋转角（RotateRect/Ellipse） */
	double startAngle = 0;        /*  起始角，度（Arc） */
	double endAngle = 0;          /*  终止角，度（Arc；独立存储，供拖拽边界检查） */
	double span = 0;              /*  跨度，度；正=逆时针，负=顺时针（Arc，= endAngle - startAngle 规范化） */
	QList<QPointF> pts;           /*  点集（Polygon） */

	explicit DrawShapeItem(DrawShapeType t) : type(t) {}
};
