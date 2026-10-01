// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "MarkScan.h"

#include <algorithm>

namespace {

double nowMilliseconds() {
	static const double frequency = [] {
		LARGE_INTEGER value{};
		::QueryPerformanceFrequency(&value);
		return static_cast<double>(value.QuadPart) / 1000.0;
	}();
	LARGE_INTEGER counter{};
	::QueryPerformanceCounter(&counter);
	return static_cast<double>(counter.QuadPart) / frequency;
}

// Scintilla's change history markers, generated from the undo history.
// SCI_MARKERGET reports them, SCI_MARKERNEXT does not, so lines are read one by one.
constexpr int kHistoryMask = (1 << SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN) | (1 << SC_MARKNUM_HISTORY_SAVED) |
	(1 << SC_MARKNUM_HISTORY_MODIFIED) | (1 << SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED);

// Color index of a line: modified, saved, reverted to original, reverted to modified
unsigned char historyValue(sptr_t markers) {
	if (markers & (1 << SC_MARKNUM_HISTORY_MODIFIED))
		return 1;
	if (markers & (1 << SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED))
		return 4;
	if (markers & (1 << SC_MARKNUM_HISTORY_SAVED))
		return 2;
	if (markers & (1 << SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN))
		return 3;
	return 0;
}

} // namespace

void MarkScan::clear() {
	m_tasks.clear();
	m_task = 0;
	for (int layer = 0; layer < LayerCount; ++layer) {
		m_marks[layer].clear();
		m_pending[layer].clear();
		m_rescanned[layer] = false;
	}
}

void MarkScan::begin(unsigned sources, const Request& request, const Editor& editor) {
	// Sources that were still being collected are collected again too
	if (running()) {
		for (size_t i = m_task; i < m_tasks.size(); ++i) {
			const Task& task = m_tasks[i];
			if (task.kind == Kind::History)
				sources |= History;
			else if (task.kind == Kind::Marker)
				sources |= Bookmarks;
			else
				sources |= Indicators;
		}
	}

	m_tasks.clear();
	m_task = 0;
	m_cursor = 0;
	m_length = editor.length();
	m_lines = editor.lineCount();
	for (int layer = 0; layer < LayerCount; ++layer) {
		m_pending[layer].clear();
		m_rescanned[layer] = false;
	}

	auto rescan = [&](Layer layer, bool enabled) {
		m_rescanned[layer] = true;
		return enabled;
	};

	if (sources & Indicators) {
		if (rescan(LayerFindMarks, request.findMarks))
			m_tasks.push_back({ Kind::Indicator, LayerFindMarks, kFindMarkIndicator, 1 });
		if (rescan(LayerStyleTokens, request.styleTokens)) {
			for (int i = 0; i < kStyleTokens; ++i)
				m_tasks.push_back({ Kind::Indicator, LayerStyleTokens, kFirstStyleTokenIndicator - i, static_cast<unsigned char>(i + 1) });
		}
		if (rescan(LayerOtherIndicators, !request.otherIndicators.empty())) {
			for (size_t i = 0; i < request.otherIndicators.size(); ++i)
				m_tasks.push_back({ Kind::Indicator, LayerOtherIndicators, request.otherIndicators[i], static_cast<unsigned char>(i + 1) });
		}
	}
	if ((sources & Bookmarks) && rescan(LayerBookmarks, request.bookmarks))
		m_tasks.push_back({ Kind::Marker, LayerBookmarks, request.bookmarkMarker, 1 });
	if ((sources & History) && rescan(LayerChangeHistory, request.history) && editor.call(SCI_GETCHANGEHISTORY) != SC_CHANGE_HISTORY_DISABLED)
		m_tasks.push_back({ Kind::History, LayerChangeHistory, 0, 0 });

	if (m_tasks.empty())
		finish();
}

bool MarkScan::resume(const Editor& editor, double budgetMilliseconds) {
	if (!running())
		return true;

	const double deadline = nowMilliseconds() + budgetMilliseconds;
	unsigned steps = 0;
	while (m_task < m_tasks.size()) {
		const Task& task = m_tasks[m_task];
		bool done = false;

		switch (task.kind) {
		case Kind::Indicator: {
			// Runs of equal indicator value, jumping from one run to the next
			const uptr_t indicator = static_cast<uptr_t>(task.number);
			const Sci_Position end = editor.call(SCI_INDICATOREND, indicator, m_cursor);
			if (end <= m_cursor || m_cursor >= m_length) {
				done = true;
				break;
			}
			if (editor.call(SCI_INDICATORVALUEAT, indicator, m_cursor) != 0)
				m_pending[task.layer].push_back({ m_cursor, end, editor.lineFromPosition(m_cursor), task.value });
			m_cursor = end;
			break;
		}
		case Kind::Marker: {
			const Sci_Position line = editor.call(SCI_MARKERNEXT, static_cast<uptr_t>(m_cursor), static_cast<sptr_t>(1) << task.number);
			if (line < 0) {
				done = true;
				break;
			}
			const Sci_Position start = editor.call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(line));
			m_pending[task.layer].push_back({ start, start, line, 1 });
			m_cursor = line + 1;
			break;
		}
		case Kind::History: {
			// Up to 512 lines per step
			const Sci_Position stop = std::min(m_lines, m_cursor + 512);
			for (; m_cursor < stop; ++m_cursor) {
				const unsigned char value = historyValue(editor.call(SCI_MARKERGET, static_cast<uptr_t>(m_cursor)) & kHistoryMask);
				if (value) {
					const Sci_Position start = editor.call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(m_cursor));
					m_pending[task.layer].push_back({ start, start, m_cursor, value });
				}
			}
			done = m_cursor >= m_lines;
			break;
		}
		}

		if (done) {
			++m_task;
			m_cursor = 0;
		}
		if ((++steps & 15) == 0 && nowMilliseconds() >= deadline)
			break;
	}

	if (!running())
		finish();
	return !running();
}

void MarkScan::finish() {
	for (int layer = 0; layer < LayerCount; ++layer) {
		if (!m_rescanned[layer])
			continue;
		std::vector<Mark>& marks = m_pending[layer];
		// Several indicators share a layer: keep the marks in document order
		std::stable_sort(marks.begin(), marks.end(), [](const Mark& a, const Mark& b) { return a.start < b.start; });
		m_marks[layer].swap(marks);
		marks.clear();
		m_rescanned[layer] = false;
	}
	m_tasks.clear();
	m_task = 0;
}
