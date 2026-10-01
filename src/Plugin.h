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

#include "PluginInterface.h"
#include "MarkerBar.h"
#include "NppPreferences.h"
#include "PreviewWindow.h"
#include "Settings.h"

// Notepad++ draws its smart highlighting with this Scintilla indicator.
constexpr int kNppSmartHighlightIndicator = 29;

class Plugin {
public:
	static Plugin& instance();

	// Notepad++ plugin interface
	void setModule(HINSTANCE module) { m_module = module; }
	void setInfo(const NppData& data) { m_npp = data; }
	FuncItem* commands(int* count);
	void notify(const SCNotification* notification);

	// Services used by the marker bars and the settings dialog
	HINSTANCE module() const { return m_module; }
	HWND notepad() const { return m_npp._nppHandle; }
	const Settings& settings() const { return m_settings; }
	const NppPreferences& preferences() const { return m_preferences; }
	PreviewWindow& preview() { return m_preview; }

	int searchFlags() const;
	BarColors themeColors(const Editor& editor) const;   // colors derived from Notepad++
	BarColors barColors(const Editor& editor) const;     // theme colors with user overrides
	UINT previewDelay() const;
	Editor currentEditor() const;

	void setTimer(int barIndex, MarkerBar::TimerKind kind, UINT milliseconds);
	void killTimer(int barIndex, MarkerBar::TimerKind kind);

	void applySettings(const Settings& settings);

	// Menu commands
	void toggleMarkers();
	void openSettings();
	void showAbout();

private:
	Plugin() = default;

	void start();
	void stop();
	MarkerBar* barFor(HWND hwnd);
	void updateMenu();
	static LRESULT CALLBACK hostProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	HINSTANCE m_module = nullptr;
	NppData m_npp{};
	Settings m_settings;
	NppPreferences m_preferences;
	std::wstring m_settingsFile;
	MarkerBar m_bars[2];
	PreviewWindow m_preview;
	HWND m_host = nullptr;   // message-only window that owns the timers
	bool m_started = false;
	FuncItem m_commands[4];
};
