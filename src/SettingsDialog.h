// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#pragma once

#include <windows.h>

#include "MarkerBar.h"
#include "Settings.h"

class Plugin;

class SettingsDialog {
public:
	SettingsDialog(Plugin& plugin, const Settings& settings);

	// Shows the dialog. Returns true when the user pressed OK.
	bool run(HINSTANCE module, HWND parent);
	const Settings& settings() const { return m_settings; }

private:
	static INT_PTR CALLBACK dialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR handle(UINT message, WPARAM wParam, LPARAM lParam);

	void fillChoice(int id, bool notepadValue, Choice value);
	void load();
	void store();
	void updateControls();
	void pickColor(int buttonId);
	void drawColorButton(const DRAWITEMSTRUCT& item) const;
	ColorChoice& colorFor(int buttonId);
	COLORREF shownColor(int buttonId) const;
	COLORREF shownColorFor(int buttonId, bool automatic) const;
	int automaticBarWidth() const;
	int automaticPreviewDelay() const;

	Plugin& m_plugin;
	Settings m_settings;
	BarColors m_themeColors;
	HWND m_hwnd = nullptr;
	COLORREF m_customColors[16] = {};
};
