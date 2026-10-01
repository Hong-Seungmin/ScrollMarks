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

class Plugin;

struct BarColors {
	COLORREF background = RGB(240, 240, 240);
	COLORREF occurrence = RGB(0, 255, 0);
	COLORREF current = RGB(0, 0, 0);
};

// The strip between the text and the vertical scrollbar of one Scintilla view.
//
// It owns the occurrence search of its view and draws a marker for every
// occurrence. A marker is drawn where the top of the scrollbar thumb is when
// its line is the first line on screen, so the marker of every line that is on
// screen lies inside the thumb, including documents where Windows enlarges the
// thumb to its minimum size and when scrolling beyond the last line is enabled.
class MarkerBar {
public:
	enum TimerKind : UINT_PTR { EvaluateTimer = 1, SearchTimer = 2, HoverTimer = 3 };

	void attach(Plugin& plugin, HWND scintilla, int index);
	void detach();
	bool attached() const { return m_editor.hwnd() != nullptr; }
	HWND hwnd() const { return m_editor.hwnd(); }
	const Editor& editor() const { return m_editor; }

	// Events forwarded by the plugin
	void onSelectionChanged();
	void onTextChanged();
	void onDocumentMaybeSwitched();
	void onSettingsChanged();
	void onAppearanceChanged();
	void onTimer(TimerKind kind);

private:
	struct Layout {
		RECT bar{};            // window coordinates
		int trackTop = 0;      // window y of the first pixel of the scrollbar track
		int trackLength = 0;
		int thumbLength = 0;   // 0 when the scrollbar cannot scroll
		int rangeMin = 0;
		int rangeMax = 0;
		int page = 0;
		bool operator==(const Layout& other) const;
		bool operator!=(const Layout& other) const { return !(*this == other); }
	};

	enum Row : unsigned char { RowEmpty = 0, RowOccurrence = 1, RowCurrent = 2 };

	static LRESULT CALLBACK subclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data);

	bool shown() const;
	bool active() const;
	int barWidth() const;
	UINT dpi() const;
	void refreshFrame();
	Layout computeLayout() const;
	RECT barRect() const;                       // window coordinates
	bool barContains(POINT screen) const;

	// Selection and search
	void evaluateSelection();
	bool readSearchText(std::string& text, Sci_Position& start) const;
	void beginSearch(std::string text, int flags, Sci_Position currentStart);
	void continueSearch();
	void clear();
	void setCurrent(Sci_Position start);
	void updateCurrentIndex();
	void stopTimers();

	// Drawing
	void onPainted();
	void updateLines();
	void rebuildRows();
	double pixelsPerLine() const;
	int markerHeight() const;
	void paintNow();
	void paint(HDC dc);

	// Mouse
	int occurrenceAt(int screenY) const;
	void onMouseMove(POINT screen);
	void onMouseLeave();
	void onClick(POINT screen);
	void goTo(size_t index);
	void scrollTo(int screenY);
	void showPreview();

	Plugin* m_plugin = nullptr;
	Editor m_editor;
	int m_index = 0;
	sptr_t m_document = 0;

	OccurrenceSearch m_search;
	std::vector<Sci_Position> m_lines;   // display line of every occurrence
	Sci_Position m_lastDocLine = -1;
	Sci_Position m_lastDisplayLine = 0;
	long long m_current = -1;            // index of the selected occurrence
	Sci_Position m_currentStart = -1;

	// Last seen selection, to ignore SCN_UPDATEUI that changed nothing
	Sci_Position m_selectionStart = -1;
	Sci_Position m_selectionEnd = -1;
	Sci_Position m_caret = -1;
	bool m_textChanged = false;
	bool m_editPending = false;

	Layout m_layout;
	bool m_layoutValid = false;
	double m_scale = 0;
	std::vector<unsigned char> m_rows;   // one entry per pixel of the track

	bool m_trackingMouse = false;
	long long m_hoverIndex = -1;
	POINT m_hoverPoint{};
};
