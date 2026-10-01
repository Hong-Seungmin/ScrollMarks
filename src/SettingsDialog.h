// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#pragma once

#include <windows.h>
#include <vector>

#include "Editor.h"
#include "MarkLayers.h"
#include "Settings.h"

class Plugin;

// Tabbed settings dialog. The hover preview settings and a sample of the bar
// sit below the tabs, so they are visible on every tab and the sample shows
// every change at once.
class SettingsDialog {
public:
	SettingsDialog(Plugin& plugin, const Settings& settings);

	// Shows the dialog. Returns true when the user pressed OK.
	bool run(HINSTANCE module, HWND parent);
	const Settings& settings() const { return m_settings; }

private:
	static constexpr int kPageCount = 5;

	struct ColorSlot {
		int button;
		int autoBox;
		ColorChoice* choice;
		COLORREF automatic;
	};

	struct LayerRow {
		int enabledBox;   // 0 when the row has no own switch
		int positionCombo;
		int widthEdit;
		LayerSettings* layer;
	};

	static INT_PTR CALLBACK frameProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK pageProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR handle(UINT message, WPARAM wParam, LPARAM lParam);

	HWND control(int id) const;
	bool checked(int id) const;
	void check(int id, bool value);
	int number(int id, int fallback) const;
	void setNumber(int id, int value);
	void enable(int id, bool value);

	void createPages();
	void showPage(int index);
	void applyTexts();
	void fillCombos();
	void load();
	void store();
	void updateControls();
	void changed();

	std::vector<ColorSlot> colorSlots();
	std::vector<LayerRow> layerRows();
	void colorAutoClicked(int autoBox);
	void pickColor(int button);
	void drawColorButton(const DRAWITEMSTRUCT& item);
	void drawSample(const DRAWITEMSTRUCT& item);
	int automaticBarWidth() const;
	int automaticPreviewDelay() const;

	Plugin& m_plugin;
	Settings m_settings;
	Editor m_editor;
	Palette m_palette;
	HINSTANCE m_module = nullptr;
	HWND m_hwnd = nullptr;
	HWND m_pages[kPageCount] = {};
	int m_page = 0;
	bool m_loading = false;
	COLORREF m_customColors[16] = {};
};
