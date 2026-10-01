// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "MarkLayers.h"

#include <algorithm>
#include <cstdlib>

#include "Notepad_plus_msgs.h"

namespace {

// Layout of NppDarkMode::Colors from Notepad_plus_msgs.h
struct DarkModeColors {
	COLORREF background = 0;
	COLORREF softerBackground = 0;
	COLORREF hotBackground = 0;
	COLORREF pureBackground = 0;
	COLORREF errorBackground = 0;
	COLORREF text = 0;
	COLORREF darkerText = 0;
	COLORREF disabledText = 0;
	COLORREF linkText = 0;
	COLORREF edge = 0;
	COLORREF hotEdge = 0;
	COLORREF disabledEdge = 0;
};

// Colors for a light and for a dark background
struct Pair {
	COLORREF light;
	COLORREF dark;
	COLORREF pick(bool darkBackground) const { return darkBackground ? dark : light; }
};

constexpr Pair kCurrent{ RGB(0x09, 0x69, 0xDA), RGB(0x58, 0xA6, 0xFF) };          // blue
constexpr Pair kCaretLine{ RGB(0x24, 0x29, 0x2F), RGB(0xE6, 0xED, 0xF3) };        // near black / near white
constexpr Pair kBookmark{ RGB(0x82, 0x50, 0xDF), RGB(0xBC, 0x8C, 0xFF) };         // violet
constexpr Pair kHistory[kHistoryStates] = {
	{ RGB(0xBF, 0x87, 0x00), RGB(0xD2, 0x99, 0x22) },   // modified: amber
	{ RGB(0x0E, 0x74, 0x90), RGB(0x22, 0xD3, 0xEE) },   // saved: teal
	{ RGB(0x6E, 0x77, 0x81), RGB(0x8B, 0x94, 0x9E) },   // reverted to original: gray
	{ RGB(0xBC, 0x4C, 0x00), RGB(0xF0, 0x88, 0x3E) },   // reverted to modified: orange
};
constexpr Pair kFlash{ RGB(0x09, 0x69, 0xDA), RGB(0x58, 0xA6, 0xFF) };

int luminance(COLORREF color) {
	return (GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114) / 1000;
}

COLORREF blend(COLORREF from, COLORREF to, int percent) {
	auto mix = [percent](int a, int b) { return a + (b - a) * percent / 100; };
	return RGB(mix(GetRValue(from), GetRValue(to)), mix(GetGValue(from), GetGValue(to)), mix(GetBValue(from), GetBValue(to)));
}

COLORREF themeIndicator(const Editor& editor, int indicator) {
	return static_cast<COLORREF>(editor.call(SCI_INDICGETFORE, static_cast<uptr_t>(indicator)) & 0xFFFFFF);
}

} // namespace

COLORREF ensureContrast(COLORREF color, COLORREF background) {
	constexpr int kMinimumDifference = 80;
	const bool darkBackground = luminance(background) < 128;
	const COLORREF target = darkBackground ? RGB(255, 255, 255) : RGB(0, 0, 0);
	for (int percent = 0; percent <= 100; percent += 10) {
		const COLORREF candidate = blend(color, target, percent);
		if (std::abs(luminance(candidate) - luminance(background)) >= kMinimumDifference)
			return candidate;
	}
	return target;
}

Palette automaticPalette(HWND npp, const Editor& editor, const std::vector<int>& otherIndicators) {
	Palette palette;

	DarkModeColors dark;
	if (::SendMessage(npp, NPPM_ISDARKMODEENABLED, 0, 0) &&
		::SendMessage(npp, NPPM_GETDARKMODECOLORS, sizeof(dark), reinterpret_cast<LPARAM>(&dark)))
		palette.background = dark.background;
	else
		palette.background = ::GetSysColor(COLOR_BTNFACE);
	const bool darkBar = luminance(palette.background) < 128;
	const COLORREF bar = palette.background;

	// Theme colors where Notepad++ gives a color a meaning, made readable on the bar
	palette.occurrence = ensureContrast(themeIndicator(editor, kSmartHighlightIndicator), bar);
	palette.findMark = ensureContrast(themeIndicator(editor, kFindMarkIndicator), bar);
	for (int i = 0; i < kStyleTokens; ++i)
		palette.tokens[i] = ensureContrast(themeIndicator(editor, kFirstStyleTokenIndicator - i), bar);
	for (int indicator : otherIndicators)
		palette.otherIndicators.push_back(ensureContrast(themeIndicator(editor, indicator), bar));

	// A fixed palette for the rest
	palette.current = kCurrent.pick(darkBar);
	palette.caretLine = kCaretLine.pick(darkBar);
	palette.bookmark = kBookmark.pick(darkBar);
	for (int i = 0; i < kHistoryStates; ++i)
		palette.history[i] = kHistory[i].pick(darkBar);

	// The flash is drawn on the text, so it depends on the editor background
	const COLORREF editorBack = static_cast<COLORREF>(editor.call(SCI_STYLEGETBACK, STYLE_DEFAULT));
	palette.flash = kFlash.pick(luminance(editorBack) < 128);
	return palette;
}

BarStyle resolveBarStyle(const Settings& settings, const Palette& palette) {
	BarStyle style;
	style.background = settings.backgroundColor.automatic ? palette.background : settings.backgroundColor.color;

	auto set = [&](Layer layer, const LayerSettings& source, bool enabled) {
		LayerStyle& target = style.layers[layer];
		target.enabled = enabled && source.enabled;
		target.position = source.position;
		target.widthPercent = source.widthPercent;
		target.colorCount = 1;
	};
	auto pick = [](const ColorChoice& choice, COLORREF automatic) { return choice.automatic ? automatic : choice.color; };

	set(LayerOccurrences, settings.occurrences, true);
	style.layers[LayerOccurrences].colors[0] = pick(settings.occurrences.color, palette.occurrence);

	set(LayerCurrent, settings.current, settings.occurrences.enabled);
	style.layers[LayerCurrent].colors[0] = pick(settings.current.color, palette.current);

	set(LayerCaretLine, settings.caretLine, true);
	style.layers[LayerCaretLine].colors[0] = pick(settings.caretLine.color, palette.caretLine);

	set(LayerBookmarks, settings.bookmarks, true);
	style.layers[LayerBookmarks].colors[0] = pick(settings.bookmarks.color, palette.bookmark);

	set(LayerFindMarks, settings.findMarks, true);
	style.layers[LayerFindMarks].colors[0] = pick(settings.findMarks.color, palette.findMark);

	set(LayerChangeHistory, settings.changeHistory, true);
	style.layers[LayerChangeHistory].colorCount = kHistoryStates;
	for (int i = 0; i < kHistoryStates; ++i)
		style.layers[LayerChangeHistory].colors[i] = pick(settings.historyColors[i], palette.history[i]);

	set(LayerStyleTokens, settings.styleTokens, true);
	style.layers[LayerStyleTokens].colorCount = kStyleTokens;
	for (int i = 0; i < kStyleTokens; ++i)
		style.layers[LayerStyleTokens].colors[i] = pick(settings.tokenColors[i], palette.tokens[i]);

	set(LayerOtherIndicators, settings.otherIndicators, true);
	LayerStyle& other = style.layers[LayerOtherIndicators];
	other.colorCount = std::max(1, std::min(kMaxLayerColors, static_cast<int>(palette.otherIndicators.size())));
	for (int i = 0; i < other.colorCount; ++i) {
		const COLORREF automatic = i < static_cast<int>(palette.otherIndicators.size()) ? palette.otherIndicators[static_cast<size_t>(i)] : palette.caretLine;
		other.colors[i] = pick(settings.otherIndicators.color, automatic);
	}
	return style;
}

void layerSpan(const LayerStyle& style, int barWidth, int& left, int& right) {
	int width = (barWidth * style.widthPercent + 50) / 100;
	width = std::clamp(width, 1, std::max(1, barWidth));
	switch (style.position) {
	case Position::Left:
		left = 0;
		break;
	case Position::Right:
		left = barWidth - width;
		break;
	default:
		left = (barWidth - width) / 2;
		break;
	}
	right = left + width;
}

void paintBar(HDC dc, int x, int y, int width, int height, const BarStyle& style, const LayerRows& rows, int rowsTop) {
	if (width <= 0 || height <= 0)
		return;

	// Draw off screen and copy in one go to avoid flicker
	HDC memory = ::CreateCompatibleDC(dc);
	HBITMAP bitmap = ::CreateCompatibleBitmap(dc, width, height);
	if (!memory || !bitmap) {
		if (bitmap)
			::DeleteObject(bitmap);
		if (memory)
			::DeleteDC(memory);
		return;
	}
	HGDIOBJ oldBitmap = ::SelectObject(memory, bitmap);

	HBRUSH background = ::CreateSolidBrush(style.background);
	const RECT all{ 0, 0, width, height };
	::FillRect(memory, &all, background);
	::DeleteObject(background);

	for (int layer = 0; layer < LayerCount; ++layer) {
		const LayerStyle& layerStyle = style.layers[layer];
		const std::vector<unsigned char>& layerRows = rows[static_cast<size_t>(layer)];
		if (!layerStyle.enabled || layerRows.empty())
			continue;

		int left = 0;
		int right = 0;
		layerSpan(layerStyle, width, left, right);
		HBRUSH brushes[kMaxLayerColors] = {};

		const int count = static_cast<int>(layerRows.size());
		for (int row = 0; row < count;) {
			const unsigned char value = layerRows[static_cast<size_t>(row)];
			if (value == 0) {
				++row;
				continue;
			}
			const int first = row;
			while (row < count && layerRows[static_cast<size_t>(row)] == value)
				++row;
			const int index = std::min<int>(value, layerStyle.colorCount) - 1;
			if (!brushes[index])
				brushes[index] = ::CreateSolidBrush(layerStyle.colors[index]);
			const RECT marker{ left, rowsTop + first, right, rowsTop + row };
			::FillRect(memory, &marker, brushes[index]);
		}
		for (HBRUSH brush : brushes) {
			if (brush)
				::DeleteObject(brush);
		}
	}

	::BitBlt(dc, x, y, width, height, memory, 0, 0, SRCCOPY);
	::SelectObject(memory, oldBitmap);
	::DeleteObject(bitmap);
	::DeleteDC(memory);
}
