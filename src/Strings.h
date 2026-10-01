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

// User interface texts in English and Korean.
enum class Text {
	// Menu
	MenuShowMarkers, MenuSettings, MenuNext, MenuPrevious, MenuAbout,
	AboutTitle, AboutBody,

	// Settings dialog: frame
	DialogTitle, TabGeneral, TabOccurrences, TabOtherMarks, TabCaretLine, TabNavigation,
	HoverGroup, ShowPreview, ContextLines, AboveAndBelow, HoverDelay, Milliseconds, PreviewWidth, PercentOfEditor,
	SampleGroup, ResetDefaults, Ok, Cancel,
	Automatic, Pixels, Percent, ColumnColor, ColumnPosition, ColumnWidth,
	Left, Center, Right, LikeNotepadOn, LikeNotepadOff, On, Off,

	// General
	ShowMarkers, Language, LanguageAuto, BarWidth, MinimumMarkerHeight, Background, PriorityNote,

	// Occurrences
	MarkOccurrences, FollowSmartHighlighting, MatchCase, WholeWord, MinimumLength, Characters,
	MaximumMarkers, NoLimit, WordAtCaret, LargeFiles, OccurrencesRow, CurrentRow,
	WholeWordWarning, WholeWordWarningFind,

	// Other marks
	ChangeHistory, HistoryModified, HistorySaved, HistoryReverted, HistoryRevertedModified,
	Bookmarks, FindMarks, StyleTokens, OtherIndicators, IndicatorNumbers, IndicatorNumbersHint,

	// Caret line
	ShowCaretLine, Color, Thickness, Position, Width,

	// Click and keys
	MoveCaretOnClick, CenterOnClick, FlashLine, Duration, ScrollOnEmptyClick,
	JumpGroup, WrapAround, CenterOnJump, ShortcutNote,

	// Preview caption
	CaptionLine, KindOccurrence, KindBookmark, KindFindMark, KindStyleToken, KindIndicator,

	Count
};

namespace Strings {

void setKorean(bool korean);
bool korean();
const wchar_t* get(Text text);

// Detects whether Notepad++ runs in Korean.
bool notepadIsKorean(HWND npp);

} // namespace Strings

inline const wchar_t* tr(Text text) { return Strings::get(text); }
