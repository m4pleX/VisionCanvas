#include "DetectionResultModel.h"

int DetectionResultModel::addResult(const AlgorithmResult& result)
{
	m_results.append(result);
	return m_results.size() - 1;
}

bool DetectionResultModel::removeResult(const QString& engine, const QString& toolId, const QString& resultId)
{
	const int idx = indexOf(engine, toolId, resultId);
	if (idx < 0)
		return false;
	m_results.removeAt(idx);
	return true;
}

void DetectionResultModel::clear()
{
	m_results.clear();
}

int DetectionResultModel::count() const
{
	return m_results.size();
}

bool DetectionResultModel::isEmpty() const
{
	return m_results.isEmpty();
}

const AlgorithmResult& DetectionResultModel::resultAt(int index) const
{
	static const AlgorithmResult empty; // 越界兜底（只读返回）
	if (index < 0 || index >= m_results.size())
		return empty;
	return m_results.at(index);
}

int DetectionResultModel::indexOf(const QString& engine, const QString& toolId, const QString& resultId) const
{
	for (int i = 0; i < m_results.size(); ++i)
	{
		const AlgorithmResult& r = m_results.at(i);
		if (r.engine == engine && r.toolId == toolId && r.resultId == resultId)
			return i;
	}
	return -1;
}

QList<DetectionBox> DetectionResultModel::allDetections() const
{
	QList<DetectionBox> out;
	for (const AlgorithmResult& r : m_results)
	{
		for (const DetectionBox& d : r.detections)
			out.append(d);
	}
	return out;
}
