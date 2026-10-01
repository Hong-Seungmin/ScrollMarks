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

// A switch that either follows Notepad++ or is forced on or off.
enum class Choice { FollowNotepad, On, Off };

inline bool resolve(Choice choice, bool notepadValue) {
	return choice == Choice::FollowNotepad ? notepadValue : choice == Choice::On;
}

enum class Language { FollowNotepad, English, Korean };

// Where a kind of marker sits horizontally in the bar.
enum class Position { Left, Center, Right };

// A color that is either automatic or fixed.
struct ColorChoice {
	bool automatic = true;
	COLORREF color = RGB(0, 0, 0);
};

// A number that is either automatic or fixed.
struct NumberChoice {
	bool automatic = true;
	int value = 0;
};

// How one kind of marker is drawn.
struct LayerSettings {
	bool enabled = true;
	Position position = Position::Center;
	int widthPercent = 100;   // of the bar width
	ColorChoice color;
};

constexpr int kHistoryStates = 4;   // modified, saved, reverted to original, reverted to modified
constexpr int kStyleTokens = 5;

// User settings, stored in plugins\Config\ScrollMarks.ini.
struct Settings {
	// General
	bool enabled = true;
	Language language = Language::FollowNotepad;
	NumberChoice barWidth;                 // pixels at 96 DPI, automatic = scrollbar width
	int minimumMarkerHeight = 2;           // pixels at 96 DPI
	ColorChoice backgroundColor;

	// Occurrences of the selected text
	LayerSettings occurrences{ true, Position::Center, 28, {} };
	LayerSettings current{ true, Position::Center, 100, {} };
	bool followSmartHighlighting = true;
	Choice matchCase = Choice::FollowNotepad;
	Choice wholeWord = Choice::FollowNotepad;
	bool useWordAtCaret = false;
	bool markLargeFiles = false;
	int minimumLength = 1;
	int maximumMarkers = 100000;           // 0 = no limit

	// Other marks
	LayerSettings changeHistory{ true, Position::Left, 22, {} };
	ColorChoice historyColors[kHistoryStates];
	LayerSettings bookmarks{ true, Position::Right, 22, {} };
	LayerSettings findMarks{ true, Position::Center, 42, {} };
	LayerSettings styleTokens{ true, Position::Center, 56, {} };
	ColorChoice tokenColors[kStyleTokens];
	LayerSettings otherIndicators{ false, Position::Center, 56, {} };
	std::wstring otherIndicatorList;       // "9, 19"

	// Caret line
	LayerSettings caretLine{ true, Position::Center, 100, {} };
	int caretLineThickness = 2;            // pixels at 96 DPI

	// Click and keys
	bool moveCaretOnClick = false;
	bool centerOnClick = true;
	bool flashLine = true;
	int flashDuration = 600;               // milliseconds
	ColorChoice flashColor;
	bool scrollOnEmptyClick = true;
	bool wrapAround = true;
	bool centerOnJump = true;

	// Hover preview
	bool previewEnabled = true;
	int previewContextLines = 3;
	NumberChoice previewDelay;             // milliseconds, automatic = Windows mouse hover time
	int previewWidthPercent = 60;

	void load(const std::wstring& file);
	void save(const std::wstring& file) const;
	void clamp();

	std::vector<int> otherIndicatorNumbers() const;
};
