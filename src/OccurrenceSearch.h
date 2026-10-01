// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#pragma once

#include <string>
#include <vector>

#include "Editor.h"

// One place in the document where the searched text occurs.
struct Occurrence {
	Sci_Position start = 0;
	Sci_Position end = 0;
};

// Finds every occurrence of a text in a document, a few milliseconds at a time,
// so that even very large documents never block the user interface.
class OccurrenceSearch {
public:
	void begin(std::string text, int flags, size_t limit, Sci_Position documentLength);
	void reset();

	// Continues the search until the time budget is used up.
	// Returns true when the whole document has been searched.
	bool resume(const Editor& editor, double budgetMilliseconds);

	bool finished() const { return m_finished; }
	bool truncated() const { return m_truncated; }
	bool empty() const { return m_text.empty(); }
	const std::string& text() const { return m_text; }
	int flags() const { return m_flags; }
	const std::vector<Occurrence>& occurrences() const { return m_occurrences; }

private:
	std::string m_text;
	int m_flags = 0;
	size_t m_limit = 0;
	Sci_Position m_next = 0;        // where the next match may start
	Sci_Position m_searchedTo = 0;  // end of the last window that had no further match
	Sci_Position m_end = 0;
	bool m_finished = true;
	bool m_truncated = false;
	std::vector<Occurrence> m_occurrences;
};
