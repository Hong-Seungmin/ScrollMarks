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
#include "MarkLayers.h"
#include "NppPreferences.h"
#include "PreviewWindow.h"
#include "Settings.h"

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
	int bookmarkMarker() const { return m_bookmarkMarker; }

	int searchFlags() const;
	Palette palette(const Editor& editor, const Settings& settings) const;
	BarStyle barStyle(const Editor& editor) const;
	UINT previewDelay() const;
	Editor currentEditor() const;
	void flashLine(const Editor& editor, Sci_Position line);

	void setTimer(int barIndex, MarkerBar::TimerKind kind, UINT milliseconds);
	void killTimer(int barIndex, MarkerBar::TimerKind kind);

	void applySettings(const Settings& settings);

	// Menu commands
	void toggleMarkers();
	void openSettings();
	void jump(bool forward);
	void showAbout();

private:
	Plugin() = default;

	void loadSettings();
	void applyLanguage();
	void start();
	void stop();
	MarkerBar* barFor(HWND hwnd);
	void updateMenu();
	void requestMarkerNotifications();
	void clearFlash();
	void prepareFlashMarker(const Editor& editor) const;
	static LRESULT CALLBACK hostProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	HINSTANCE m_module = nullptr;
	NppData m_npp{};
	Settings m_settings;
	bool m_settingsLoaded = false;
	NppPreferences m_preferences;
	std::wstring m_settingsFile;
	MarkerBar m_bars[2];
	PreviewWindow m_preview;
	HWND m_host = nullptr;   // message-only window that owns the timers
	bool m_started = false;
	int m_bookmarkMarker = 20;
	bool m_markerNotifications = false;

	// Briefly highlighted line after a click
	int m_flashMarker = -1;
	HWND m_flashHelper = nullptr;   // hidden Scintilla to clean up a document that is no longer shown
	sptr_t m_flashDocument = 0;
	int m_flashHandle = -1;
	bool m_changingFlash = false;   // ignore the marker change we cause ourselves

	FuncItem m_commands[7];
};
