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

#include "Editor.h"
#include "OccurrenceSearch.h"

// A small popup that shows the lines around an occurrence.
//
// It contains its own Scintilla view that shares the document of the editor,
// so the text, syntax colors and highlights are exactly the ones of the editor
// without copying any text. The view is never focused and never edits.
class PreviewWindow {
public:
	struct Request {
		Occurrence occurrence;
		size_t number = 0;          // 1-based number of the occurrence
		size_t total = 0;
		bool totalIsLimited = false;
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
	void copyAppearance(const Editor& source);
	void paintCaption(HDC dc) const;
	void updateFont(UINT dpi);

	HINSTANCE m_module = nullptr;
	HWND m_window = nullptr;
	HWND m_viewWindow = nullptr;
	Editor m_view;
	HWND m_source = nullptr;
	sptr_t m_document = 0;
	sptr_t m_lexer = -1;
	bool m_appearanceValid = false;
	bool m_visible = false;

	std::wstring m_caption;
	COLORREF m_captionBack = RGB(240, 240, 240);
	COLORREF m_captionText = RGB(0, 0, 0);
	int m_captionHeight = 0;
	HFONT m_font = nullptr;
	UINT m_fontDpi = 0;
};
