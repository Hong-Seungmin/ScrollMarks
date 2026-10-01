// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#pragma once

#include <windows.h>
#include <array>
#include <vector>

#include "Editor.h"
#include "Settings.h"

// Kinds of markers, in drawing order: later layers are drawn on top.
enum Layer : int {
	LayerChangeHistory,
	LayerBookmarks,
	LayerOtherIndicators,
	LayerStyleTokens,
	LayerFindMarks,
	LayerOccurrences,
	LayerCurrent,
	LayerCaretLine,
	LayerCount
};

// One marked place: a range in the document and the color it is drawn with.
struct Mark {
	Sci_Position start = 0;
	Sci_Position end = 0;
	Sci_Position line = 0;
	unsigned char value = 1;   // 1-based index into the layer's colors
};

constexpr int kMaxLayerColors = 8;

struct LayerStyle {
	bool enabled = false;
	Position position = Position::Center;
	int widthPercent = 100;
	COLORREF colors[kMaxLayerColors] = {};
	int colorCount = 1;
};

struct BarStyle {
	COLORREF background = RGB(240, 240, 240);
	LayerStyle layers[LayerCount];
};

// One entry per pixel of the scrollbar track for every layer:
// 0 = nothing, otherwise the 1-based color index.
using LayerRows = std::array<std::vector<unsigned char>, LayerCount>;

// Colors used when a color setting is automatic.
struct Palette {
	COLORREF background = 0;
	COLORREF occurrence = 0;
	COLORREF current = 0;
	COLORREF caretLine = 0;
	COLORREF bookmark = 0;
	COLORREF findMark = 0;
	COLORREF history[kHistoryStates] = {};
	COLORREF tokens[kStyleTokens] = {};
	std::vector<COLORREF> otherIndicators;
	COLORREF flash = 0;
};

// Notepad++ indicators
constexpr int kSmartHighlightIndicator = 29;
constexpr int kFindMarkIndicator = 31;
constexpr int kFirstStyleTokenIndicator = 25;   // style 1 is 25, style 5 is 21

Palette automaticPalette(HWND npp, const Editor& editor, const std::vector<int>& otherIndicators);
BarStyle resolveBarStyle(const Settings& settings, const Palette& palette);
COLORREF ensureContrast(COLORREF color, COLORREF background);

// Horizontal span of a layer inside a bar of the given width.
void layerSpan(const LayerStyle& style, int barWidth, int& left, int& right);

// Draws the bar into dc at (x, y). rowsTop is the bar y of the first row.
void paintBar(HDC dc, int x, int y, int width, int height, const BarStyle& style, const LayerRows& rows, int rowsTop);
