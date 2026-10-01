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

// A switch that either follows Notepad++ or is forced on or off.
enum class Choice { FollowNotepad, On, Off };

inline bool resolve(Choice choice, bool notepadValue) {
	return choice == Choice::FollowNotepad ? notepadValue : choice == Choice::On;
}

// A color that is either derived from the Notepad++ theme or fixed.
struct ColorChoice {
	bool automatic = true;
	COLORREF color = RGB(0, 0, 0);
	bool operator==(const ColorChoice& other) const { return automatic == other.automatic && color == other.color; }
};

// A number that is either derived from Notepad++ or Windows, or fixed.
struct NumberChoice {
	bool automatic = true;
	int value = 0;
	bool operator==(const NumberChoice& other) const { return automatic == other.automatic && value == other.value; }
};

// User settings, stored in plugins\Config\ScrollMarks.ini.
struct Settings {
	bool enabled = true;

	// Which text is marked
	bool followSmartHighlighting = true;   // only when Notepad++ smart highlighting is on
	Choice matchCase = Choice::FollowNotepad;
	Choice wholeWord = Choice::FollowNotepad;
	bool useWordAtCaret = false;           // mark the word at the caret when nothing is selected
	bool markLargeFiles = false;           // ignore Notepad++'s large file restriction
	int minimumLength = 1;                 // characters
	int maximumMarkers = 100000;           // 0 = no limit

	// How markers look (pixels are at 96 DPI and scaled with the monitor)
	NumberChoice barWidth;                 // automatic = half the scrollbar width
	int minimumMarkerHeight = 2;
	ColorChoice occurrenceColor;           // automatic = Notepad++ smart highlighting color
	ColorChoice currentColor;              // automatic = caret color
	ColorChoice backgroundColor;           // automatic = scrollbar track color

	// Clicking
	bool selectOnClick = true;
	bool centerOnClick = true;
	bool scrollOnEmptyClick = true;

	// Hover preview
	bool previewEnabled = true;
	int previewContextLines = 3;           // lines above and below
	NumberChoice previewDelay;             // milliseconds, automatic = Windows mouse hover time
	int previewWidthPercent = 60;          // of the editor width

	void load(const std::wstring& file);
	void save(const std::wstring& file) const;
	void clamp();
};
