#include "SimulatorDetector.h"

AlgorithmResult SimulatorDetector::simulateDetect(int imageW, int imageH)
{
	AlgorithmResult result;
	result.engine   = "simulator";
	result.toolId   = "simulateDetect";
	result.resultId = "fake";
	result.sourceWidth  = imageW;
	result.sourceHeight = imageH;

	// 基于图像尺寸比例生成 4 个假检测框，保证任何尺寸图均可视
	const double w = imageW > 0 ? imageW : 1280;
	const double h = imageH > 0 ? imageH : 960;

	struct FakeBox { double cx, cy, bw, bh; const char* label; double conf; };
	const FakeBox specs[4] = {
		{ 0.30, 0.30, 0.20, 0.20, "part",      0.95 },
		{ 0.70, 0.35, 0.16, 0.24, "part",      0.88 },
		{ 0.50, 0.70, 0.28, 0.18, "assembly",  0.91 },
		{ 0.20, 0.75, 0.14, 0.14, "defect",    0.72 },
	};

	for (int i = 0; i < 4; ++i)
	{
		DetectionBox box;
		box.classId    = i;
		box.label      = QString::fromUtf8(specs[i].label);
		box.confidence = specs[i].conf;
		box.cx = specs[i].cx * w;
		box.cy = specs[i].cy * h;
		box.w  = specs[i].bw * w;
		box.h  = specs[i].bh * h;
		box.angle = 0.0;
		result.detections.append(box);
	}

	return result;
}
