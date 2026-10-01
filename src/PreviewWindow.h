// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#pragma once

#include <windows.h>
#include <string>
#include <vector>

#include "Editor.h"
#include "OccurrenceSearch.h"

// A small popup that shows the lines around an occurrence.
//
// It contains its own Scintilla view with its own small document: the lines
// around the occurrence are copied with their syntax styles and indicators, and
// every occurrence of the searched text in them is highlighted like Notepad++
// smart highlighting does. The editor's document is never changed.
class PreviewWindow {
public:
	struct Request {
		Occurrence occurrence;      // selected in the preview; empty for a whole line
		std::vector<Occurrence> highlights;   // occurrences of the searched text near it
		std::wstring caption;       // "Line 12    Bookmark  2 / 7"
		int contextLines = 3;
		int widthPercent = 60;
		POINT anchor{};             // screen point at the left edge of the marker bar
	};

	bool create(HINSTANCE module, HWND owner);
	void destroy();

	void show(const Editor& source, const Request& request);
	void hide();
	void hideFor(HWND source) { if (m_source == source) hide(); }
	bool visibleFor(HWND source) const { return m_visible && m_source == source; }

	// Styles of the editor changed: copy them again on the next show.
	void invalidateAppearance() { m_appearanceValid = false; }

private:
	static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	struct Segment {
		Sci_Position sourceStart;
		Sci_Position sourceEnd;
		Sci_Position previewStart;
	};

	void copyAppearance(const Editor& source);
	void fillContent(const Editor& source, const Request& request);
	Sci_Position toPreview(Sci_Position position) const;
	void fillIndicator(int indicator, int value, Sci_Position start, Sci_Position end);
	void paintCaption(HDC dc) const;
	void updateFont(UINT dpi);

	HINSTANCE m_module = nullptr;
	HWND m_window = nullptr;
	HWND m_viewWindow = nullptr;
	Editor m_view;
	HWND m_source = nullptr;
	sptr_t m_lexer = -1;

	// What the preview currently shows
	bool m_contentValid = false;
	sptr_t m_document = 0;      // editor document it was copied from
	Occurrence m_shown;
	int m_shownContext = -1;
	std::vector<Segment> m_segments;
	int m_numberWidth = 0;
	bool m_appearanceValid = false;
	bool m_visible = false;

	std::wstring m_caption;
	COLORREF m_captionBack = RGB(240, 240, 240);
	COLORREF m_captionText = RGB(0, 0, 0);
	int m_captionHeight = 0;
	HFONT m_font = nullptr;
	UINT m_fontDpi = 0;
};
