/*
 * 文件名：CvImageConverter.h
 * 职责：Qt 图像 与 OpenCV 图像 互转工具（纯静态）
 *
 * 核心功能：
 *   - QImage -> cv::Mat：供算法层（OpenCV）消费
 *   - cv::Mat -> QImage：供结果回显（QGraphicsPixmapItem）
 *
 * 设计约束：
 *   - 转换一律深拷贝（copy），避免 QImage 底层数据被 Qt 回收后 cv::Mat 悬垂，
 *     也避免 cv::Mat 生命周期结束后 QImage 引用失效；
 *   - 仅处理 8bit 三通道（RGB888 ↔ CV_8UC3）与灰度（Gray8 ↔ CV_8UC1），
 *     满足检测/显示场景，不做完整格式矩阵。
 */
#pragma once

class QImage;
namespace cv { class Mat; }

class CvImageConverter
{
public:
	/*  QImage -> cv::Mat（深拷贝，RGB888→BGR 或保留 RGB，按需） */
	static cv::Mat toCvMat(const QImage& image);

	/*  cv::Mat(BGR 或 灰度) -> QImage（深拷贝） */
	static QImage toQImage(const cv::Mat& mat);
};
