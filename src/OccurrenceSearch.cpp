// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "OccurrenceSearch.h"

#include <algorithm>

namespace {

// Size of the document window handed to one SCI_SEARCHINTARGET call. Keeps a
// single call short even when there is no match for megabytes.
constexpr Sci_Position kWindowBytes = 1 << 20;

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

} // namespace

void OccurrenceSearch::begin(std::string text, int flags, size_t limit, Sci_Position documentLength) {
	m_text = std::move(text);
	m_flags = flags;
	m_limit = limit;
	m_next = 0;
	m_searchedTo = 0;
	m_end = documentLength;
	m_finished = m_text.empty();
	m_truncated = false;
	m_occurrences.clear();
}

void OccurrenceSearch::reset() {
	m_text.clear();
	m_flags = 0;
	m_finished = true;
	m_truncated = false;
	m_occurrences.clear();
	m_occurrences.shrink_to_fit();
}

bool OccurrenceSearch::resume(const Editor& editor, double budgetMilliseconds) {
	if (m_finished)
		return true;

	// The target and the search flags are shared with Notepad++ and other
	// plugins: restore them when this slice is done.
	const sptr_t savedTargetStart = editor.call(SCI_GETTARGETSTART);
	const sptr_t savedTargetEnd = editor.call(SCI_GETTARGETEND);
	const sptr_t savedFlags = editor.call(SCI_GETSEARCHFLAGS);
	editor.call(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(m_flags));

	const Sci_Position textLength = static_cast<Sci_Position>(m_text.size());
	// A match may start up to this many bytes before the end of a window.
	// Case folding can change the byte length, so leave some room.
	const Sci_Position overlap = textLength * 4;
	const double deadline = nowMilliseconds() + budgetMilliseconds;

	unsigned steps = 0;
	while (!m_finished) {
		const Sci_Position windowEnd = std::min(m_end, std::max(m_searchedTo, m_next) + kWindowBytes);
		editor.call(SCI_SETTARGETRANGE, static_cast<uptr_t>(m_next), windowEnd);
		const Sci_Position found = editor.call(SCI_SEARCHINTARGET, static_cast<uptr_t>(textLength), m_text.c_str());

		if (found >= 0) {
			const Sci_Position foundEnd = editor.call(SCI_GETTARGETEND);
			m_occurrences.push_back({ found, foundEnd });
			m_next = foundEnd > found ? foundEnd : found + 1;
			if (m_limit && m_occurrences.size() >= m_limit) {
				m_truncated = true;
				m_finished = true;
			}
		} else if (windowEnd >= m_end) {
			m_finished = true;
		} else {
			m_searchedTo = windowEnd;
			m_next = std::max(m_next, windowEnd - overlap);
		}

		if ((++steps & 15) == 0 && nowMilliseconds() >= deadline)
			break;
	}

	editor.call(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(savedFlags));
	editor.call(SCI_SETTARGETRANGE, static_cast<uptr_t>(savedTargetStart), savedTargetEnd);
	return m_finished;
}
