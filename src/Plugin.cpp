// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "Plugin.h"

#include <cstdlib>
#include <cwchar>
#include <iterator>

#include "SettingsDialog.h"
#include "Version.h"

namespace {

constexpr wchar_t kHostClass[] = L"ScrollMarksHost";
constexpr UINT_PTR kTimerBase = 1000;
constexpr UINT_PTR kTimersPerBar = 16;

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

int luminance(COLORREF color) {
	return (GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114) / 1000;
}

void cmdToggleMarkers() { Plugin::instance().toggleMarkers(); }
void cmdSettings() { Plugin::instance().openSettings(); }
void cmdAbout() { Plugin::instance().showAbout(); }

} // namespace

Plugin& Plugin::instance() {
	static Plugin plugin;
	return plugin;
}

FuncItem* Plugin::commands(int* count) {
	wcscpy_s(m_commands[0]._itemName, L"Show Markers");
	m_commands[0]._pFunc = cmdToggleMarkers;
	m_commands[0]._init2Check = true;
	wcscpy_s(m_commands[1]._itemName, L"Settings...");
	m_commands[1]._pFunc = cmdSettings;
	m_commands[2]._itemName[0] = L'\0';   // separator
	m_commands[2]._pFunc = nullptr;
	wcscpy_s(m_commands[3]._itemName, L"About ScrollMarks");
	m_commands[3]._pFunc = cmdAbout;
	*count = static_cast<int>(std::size(m_commands));
	return m_commands;
}

void Plugin::notify(const SCNotification* notification) {
	const UINT code = notification->nmhdr.code;
	const HWND from = static_cast<HWND>(notification->nmhdr.hwndFrom);

	if (from == notepad()) {
		switch (code) {
		case NPPN_READY:
			start();
			break;
		case NPPN_SHUTDOWN:
			stop();
			break;
		case NPPN_BUFFERACTIVATED:
			if (m_started) {
				m_preview.hide();
				for (MarkerBar& bar : m_bars)
					bar.onDocumentMaybeSwitched();
			}
			break;
		case NPPN_FILEBEFORECLOSE:
			m_preview.hide();
			break;
		case NPPN_LANGCHANGED:
		case NPPN_WORDSTYLESUPDATED:
		case NPPN_DARKMODECHANGED:
			if (m_started) {
				m_preview.hide();
				m_preview.invalidateAppearance();
				for (MarkerBar& bar : m_bars)
					bar.onAppearanceChanged();
			}
			break;
		}
		return;
	}

	if (!m_started)
		return;
	MarkerBar* bar = barFor(from);
	if (!bar)
		return;

	switch (code) {
	case SCN_UPDATEUI:
		if (notification->updated & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT))
			bar->onSelectionChanged();
		break;
	case SCN_MODIFIED:
		if (notification->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
			bar->onTextChanged();
		break;
	}
}

MarkerBar* Plugin::barFor(HWND hwnd) {
	for (MarkerBar& bar : m_bars) {
		if (bar.attached() && bar.hwnd() == hwnd)
			return &bar;
	}
	return nullptr;
}

void Plugin::start() {
	if (m_started)
		return;

	wchar_t directory[MAX_PATH] = {};
	::SendMessage(notepad(), NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, reinterpret_cast<LPARAM>(directory));
	m_settingsFile = std::wstring(directory) + L"\\ScrollMarks.ini";
	m_settings.load(m_settingsFile);
	m_preferences = NppPreferences::load(notepad());

	WNDCLASSEXW hostClass{ sizeof(WNDCLASSEXW) };
	hostClass.lpfnWndProc = &Plugin::hostProc;
	hostClass.hInstance = m_module;
	hostClass.lpszClassName = kHostClass;
	::RegisterClassExW(&hostClass);
	m_host = ::CreateWindowExW(0, kHostClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, m_module, nullptr);

	m_preview.create(m_module, notepad());
	m_bars[0].attach(*this, m_npp._scintillaMainHandle, 0);
	m_bars[1].attach(*this, m_npp._scintillaSecondHandle, 1);
	m_started = true;
	updateMenu();

	for (MarkerBar& bar : m_bars)
		bar.onSettingsChanged();
}

void Plugin::stop() {
	if (!m_started)
		return;
	m_started = false;
	for (MarkerBar& bar : m_bars)
		bar.detach();
	m_preview.destroy();
	if (m_host)
		::DestroyWindow(m_host);
	m_host = nullptr;
	::UnregisterClassW(kHostClass, m_module);
}

void Plugin::updateMenu() {
	::SendMessage(notepad(), NPPM_SETMENUITEMCHECK, static_cast<WPARAM>(m_commands[0]._cmdID), m_settings.enabled ? TRUE : FALSE);
}

int Plugin::searchFlags() const {
	const bool matchCase = resolve(m_settings.matchCase, m_preferences.matchCase);
	const bool wholeWord = resolve(m_settings.wholeWord, m_preferences.wholeWord);
	return (matchCase ? SCFIND_MATCHCASE : 0) | (wholeWord ? SCFIND_WHOLEWORD : 0);
}

BarColors Plugin::themeColors(const Editor& editor) const {
	BarColors colors;

	DarkModeColors dark;
	if (::SendMessage(notepad(), NPPM_ISDARKMODEENABLED, 0, 0) &&
		::SendMessage(notepad(), NPPM_GETDARKMODECOLORS, sizeof(dark), reinterpret_cast<LPARAM>(&dark)))
		colors.background = dark.background;
	else
		colors.background = ::GetSysColor(COLOR_BTNFACE);

	// Occurrences use the color of Notepad++ smart highlighting
	colors.occurrence = static_cast<COLORREF>(editor.call(SCI_INDICGETFORE, kNppSmartHighlightIndicator) & 0xFFFFFF);
	// The selected occurrence uses the caret color: "you are here"
	colors.current = static_cast<COLORREF>(editor.call(SCI_GETELEMENTCOLOUR, SC_ELEMENT_CARET) & 0xFFFFFF);

	// Keep the current marker visible against the background and the others
	if (colors.current == colors.occurrence || std::abs(luminance(colors.current) - luminance(colors.background)) < 64)
		colors.current = luminance(colors.background) > 128 ? RGB(0, 0, 0) : RGB(255, 255, 255);
	return colors;
}

BarColors Plugin::barColors(const Editor& editor) const {
	BarColors colors = themeColors(editor);
	if (!m_settings.backgroundColor.automatic)
		colors.background = m_settings.backgroundColor.color;
	if (!m_settings.occurrenceColor.automatic)
		colors.occurrence = m_settings.occurrenceColor.color;
	if (!m_settings.currentColor.automatic)
		colors.current = m_settings.currentColor.color;
	return colors;
}

UINT Plugin::previewDelay() const {
	if (!m_settings.previewDelay.automatic)
		return static_cast<UINT>(m_settings.previewDelay.value);
	UINT hoverTime = 400;
	::SystemParametersInfoW(SPI_GETMOUSEHOVERTIME, 0, &hoverTime, 0);
	return hoverTime;
}

Editor Plugin::currentEditor() const {
	int view = 0;
	::SendMessage(notepad(), NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&view));
	return Editor(view == 1 ? m_npp._scintillaSecondHandle : m_npp._scintillaMainHandle);
}

void Plugin::setTimer(int barIndex, MarkerBar::TimerKind kind, UINT milliseconds) {
	if (m_host)
		::SetTimer(m_host, kTimerBase + static_cast<UINT_PTR>(barIndex) * kTimersPerBar + kind, milliseconds, nullptr);
}

void Plugin::killTimer(int barIndex, MarkerBar::TimerKind kind) {
	if (m_host)
		::KillTimer(m_host, kTimerBase + static_cast<UINT_PTR>(barIndex) * kTimersPerBar + kind);
}

LRESULT CALLBACK Plugin::hostProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_TIMER && wParam >= kTimerBase) {
		Plugin& plugin = instance();
		const UINT_PTR id = wParam - kTimerBase;
		const size_t index = static_cast<size_t>(id / kTimersPerBar);
		if (plugin.m_started && index < std::size(plugin.m_bars))
			plugin.m_bars[index].onTimer(static_cast<MarkerBar::TimerKind>(id % kTimersPerBar));
		else
			::KillTimer(hwnd, wParam);
		return 0;
	}
	return ::DefWindowProcW(hwnd, message, wParam, lParam);
}

void Plugin::applySettings(const Settings& settings) {
	m_settings = settings;
	m_settings.clamp();
	m_settings.save(m_settingsFile);
	m_preview.hide();
	updateMenu();
	for (MarkerBar& bar : m_bars)
		bar.onSettingsChanged();
}

void Plugin::toggleMarkers() {
	Settings settings = m_settings;
	settings.enabled = !settings.enabled;
	applySettings(settings);
}

void Plugin::openSettings() {
	if (!m_started)
		return;
	m_preferences = NppPreferences::load(notepad());   // pick up saved Notepad++ changes
	SettingsDialog dialog(*this, m_settings);
	if (dialog.run(m_module, notepad()))
		applySettings(dialog.settings());
}

void Plugin::showAbout() {
	const std::wstring text = std::wstring(SCROLLMARKS_NAME) + L" " + SCROLLMARKS_VERSION_WSTRING +
		L"\n\nMarks every occurrence of the selected text next to the scrollbar.\n"
		L"Click a marker to go there, hover it to preview the lines around it.\n\n" +
		SCROLLMARKS_HOMEPAGE + L"\n\nLicensed under the GNU General Public License v3.";
	::MessageBoxW(notepad(), text.c_str(), L"About ScrollMarks", MB_OK | MB_ICONINFORMATION);
}
