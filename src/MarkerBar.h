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
#include "MarkLayers.h"
#include "MarkScan.h"
#include "OccurrenceSearch.h"

class Plugin;

// The strip on the right of the vertical scrollbar of one Scintilla view.
//
// It owns the occurrence search and the mark scan of its view and draws every
// kind of marker in its own layer. A marker is drawn where the top of the
// scrollbar thumb is when its line is the first line on screen, so the marker
// of every line that is on screen lies inside the thumb, including documents
// where Windows enlarges the thumb to its minimum size and when scrolling
// beyond the last line is enabled.
class MarkerBar {
public:
	enum TimerKind : UINT_PTR {
		EvaluateTimer = 1,   // re-read the selection after a short pause
		SearchTimer = 2,     // next slice of the occurrence search
		HoverTimer = 3,      // show the preview
		ScanDelayTimer = 4,  // start collecting marks after a pause
		ScanTimer = 5,       // next slice of the mark scan
	};

	void attach(Plugin& plugin, HWND scintilla, int index);
	void detach();
	bool attached() const { return m_editor.hwnd() != nullptr; }
	HWND hwnd() const { return m_editor.hwnd(); }
	const Editor& editor() const { return m_editor; }

	// Events forwarded by the plugin
	void onSelectionChanged();
	void onTextChanged();
	void onIndicatorsChanged();
	void onMarkersChanged();
	void onSaved();
	void onDocumentMaybeSwitched();
	void onSettingsChanged();
	void onAppearanceChanged();
	void onTimer(TimerKind kind);

	// Moves the caret to the previous or next occurrence of the selected text
	// or of the word at the caret. Returns false when there is none.
	bool jump(bool forward);

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

	struct Hit {
		Layer layer = LayerCount;
		size_t index = 0;
		bool valid() const { return layer != LayerCount; }
		bool operator==(const Hit& other) const { return layer == other.layer && index == other.index; }
	};

	static LRESULT CALLBACK subclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data);

	bool shown() const;
	bool occurrencesActive() const;
	int barWidth() const;
	UINT dpi() const;
	void refreshFrame();
	Layout computeLayout() const;
	RECT barRect() const;
	bool barContains(POINT screen) const;

	// Occurrences
	void evaluateSelection();
	bool readSearchText(std::string& text, Sci_Position& start) const;
	void beginSearch(std::string text, int flags, Sci_Position currentStart);
	void continueSearch();
	void clearOccurrences();
	void setCurrent(Sci_Position start);
	void updateCurrentIndex();
	void updateCaretLine();
	void stopTimers();

	// Other marks
	void scheduleScan(unsigned sources, UINT delay);
	void startScan();
	void continueScan();

	// Drawing
	void onPainted();
	void updateOccurrenceLines();
	void updateMarkLines(Layer layer);
	void invalidateLines();
	void rebuildRows();
	void rebuildCaretRow();
	void fillRows(Layer layer, Sci_Position displayLine, int height, unsigned char value);
	double pixelsPerLine() const;
	int markerHeight() const;
	void paintNow();

	// Mouse
	Hit hitAt(POINT screen) const;
	long long nearest(const std::vector<Sci_Position>& lines, double y, double tolerance) const;
	Mark markOf(const Hit& hit) const;
	std::wstring captionOf(const Hit& hit, Sci_Position line) const;
	void onMouseMove(POINT screen);
	void onMouseLeave();
	void onClick(POINT screen);
	void goTo(const Hit& hit);
	void scrollToLine(Sci_Position docLine, bool center);
	void scrollTo(int screenY);
	void showPreview();

	Plugin* m_plugin = nullptr;
	Editor m_editor;
	int m_index = 0;
	sptr_t m_document = 0;

	// Occurrences of the selected text
	OccurrenceSearch m_search;
	std::vector<Sci_Position> m_lines;   // display line of every occurrence
	Sci_Position m_lastDocLine = -1;
	Sci_Position m_lastDisplayLine = 0;
	long long m_current = -1;
	Sci_Position m_currentStart = -1;

	// Last seen selection, to ignore SCN_UPDATEUI that changed nothing
	Sci_Position m_selectionStart = -1;
	Sci_Position m_selectionEnd = -1;
	Sci_Position m_caret = -1;
	Sci_Position m_caretDocLine = -1;
	bool m_textChanged = false;
	bool m_editPending = false;

	// Change history, bookmarks and indicators
	MarkScan m_scan;
	unsigned m_pendingScan = 0;
	std::vector<int> m_otherIndicators;            // numbers of the last scan
	std::vector<Sci_Position> m_markLines[LayerCount];
	bool m_markLinesValid[LayerCount] = {};

	Layout m_layout;
	bool m_layoutValid = false;
	double m_scale = 0;
	LayerRows m_rows;

	bool m_trackingMouse = false;
	Hit m_hover;
	POINT m_hoverPoint{};
};
