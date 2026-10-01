// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#pragma once

#include <vector>

#include "Editor.h"
#include "MarkLayers.h"

// Collects the marks Notepad++ already keeps in the document: change history,
// bookmarks, find marks, style tokens and other indicators. Like the
// occurrence search it works a few milliseconds at a time.
class MarkScan {
public:
	enum Source : unsigned {
		History = 1,
		Bookmarks = 2,
		Indicators = 4,
		All = History | Bookmarks | Indicators,
	};

	struct Request {
		bool history = false;
		bool bookmarks = false;
		int bookmarkMarker = 20;
		bool findMarks = false;
		bool styleTokens = false;
		std::vector<int> otherIndicators;
	};

	// Starts collecting the given sources again. Marks of the other sources stay.
	void begin(unsigned sources, const Request& request, const Editor& editor);
	bool resume(const Editor& editor, double budgetMilliseconds);
	bool running() const { return m_task < m_tasks.size(); }
	void clear();

	const std::vector<Mark>& marks(Layer layer) const { return m_marks[layer]; }

private:
	enum class Kind { Indicator, Marker, History };
	struct Task {
		Kind kind;
		Layer layer;
		int number;            // indicator or marker number
		unsigned char value;   // color index of the marks
	};

	void finish();

	std::vector<Task> m_tasks;
	size_t m_task = 0;
	Sci_Position m_cursor = 0;   // position or line, depending on the task
	Sci_Position m_length = 0;
	Sci_Position m_lines = 0;
	bool m_rescanned[LayerCount] = {};
	std::vector<Mark> m_pending[LayerCount];
	std::vector<Mark> m_marks[LayerCount];
};
