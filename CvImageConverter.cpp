#include "CvImageConverter.h"

#include <QImage>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

cv::Mat CvImageConverter::toCvMat(const QImage& image)
{
	// 统一转为 RGB888，便于后续 cvtColor -> BGR/灰度
	QImage rgb = image.convertToFormat(QImage::Format_RGB888);
	if (rgb.isNull())
		return cv::Mat();

	// 深拷贝：QImage 的数据生命周期由 Qt 掌控，拷贝后 cv::Mat 自持内存
	return cv::Mat(rgb.height(), rgb.width(), CV_8UC3,
	               const_cast<uchar*>(rgb.bits()), rgb.bytesPerLine()).clone();
}

QImage CvImageConverter::toQImage(const cv::Mat& mat)
{
	if (mat.empty())
		return QImage();

	// 灰度
	if (mat.type() == CV_8UC1)
	{
		QImage img(mat.cols, mat.rows, QImage::Format_Grayscale8);
		memcpy(img.bits(), mat.data, static_cast<size_t>(mat.cols * mat.rows));
		return img;
	}

	// 三通道（BGR → RGB）
	if (mat.type() == CV_8UC3)
	{
		QImage img(mat.cols, mat.rows, QImage::Format_RGB888);
		// BGR -> RGB 逐像素交换
		for (int y = 0; y < mat.rows; ++y)
		{
			const uchar* src = mat.ptr<uchar>(y);
			uchar* dst = img.scanLine(y);
			for (int x = 0; x < mat.cols; ++x)
			{
				dst[x * 3 + 0] = src[x * 3 + 2]; // R
				dst[x * 3 + 1] = src[x * 3 + 1]; // G
				dst[x * 3 + 2] = src[x * 3 + 0]; // B
			}
		}
		return img;
	}

	return QImage();
}
