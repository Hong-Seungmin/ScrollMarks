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
#include "Strings.h"
#include "Version.h"

namespace {

constexpr wchar_t kHostClass[] = L"ScrollMarksHost";
constexpr UINT_PTR kTimerBase = 1000;
constexpr UINT_PTR kTimersPerBar = 16;
constexpr UINT_PTR kFlashTimer = 900;

enum Command { CommandToggle, CommandSettings, CommandSeparator1, CommandNext, CommandPrevious, CommandSeparator2, CommandAbout };

void cmdToggleMarkers() { Plugin::instance().toggleMarkers(); }
void cmdSettings() { Plugin::instance().openSettings(); }
void cmdNext() { Plugin::instance().jump(true); }
void cmdPrevious() { Plugin::instance().jump(false); }
void cmdAbout() { Plugin::instance().showAbout(); }

} // namespace

Plugin& Plugin::instance() {
	static Plugin plugin;
	return plugin;
}

void Plugin::loadSettings() {
	if (m_settingsLoaded)
		return;
	wchar_t directory[MAX_PATH] = {};
	::SendMessage(notepad(), NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, reinterpret_cast<LPARAM>(directory));
	m_settingsFile = std::wstring(directory) + L"\\ScrollMarks.ini";
	m_settings.load(m_settingsFile);
	m_settingsLoaded = true;
}

void Plugin::applyLanguage() {
	const bool korean = m_settings.language == Language::Korean ||
		(m_settings.language == Language::FollowNotepad && Strings::notepadIsKorean(notepad()));
	Strings::setKorean(korean);
}

FuncItem* Plugin::commands(int* count) {
	// Notepad++ asks for the menu before NPPN_READY, so the language is needed now
	loadSettings();
	applyLanguage();

	struct Item { Command command; Text text; PFUNCPLUGINCMD function; };
	const Item items[] = {
		{ CommandToggle, Text::MenuShowMarkers, cmdToggleMarkers },
		{ CommandSettings, Text::MenuSettings, cmdSettings },
		{ CommandSeparator1, Text::Count, nullptr },
		{ CommandNext, Text::MenuNext, cmdNext },
		{ CommandPrevious, Text::MenuPrevious, cmdPrevious },
		{ CommandSeparator2, Text::Count, nullptr },
		{ CommandAbout, Text::MenuAbout, cmdAbout },
	};
	for (const Item& item : items) {
		FuncItem& target = m_commands[item.command];
		if (item.function)
			wcscpy_s(target._itemName, tr(item.text));
		else
			target._itemName[0] = L'\0';   // separator
		target._pFunc = item.function;
		target._pShKey = nullptr;          // no default keys
	}
	m_commands[CommandToggle]._init2Check = m_settings.enabled;
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
		case NPPN_FILESAVED:
			if (m_started) {
				for (MarkerBar& bar : m_bars)
					bar.onSaved();
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
	case SCN_MODIFIED: {
		const int type = notification->modificationType;
		if (type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
			bar->onTextChanged();
		if (m_changingFlash)
			break;
		if (type & SC_MOD_CHANGEINDICATOR)
			bar->onIndicatorsChanged();
		if (type & SC_MOD_CHANGEMARKER)
			bar->onMarkersChanged();
		break;
	}
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

	loadSettings();
	m_preferences = NppPreferences::load(notepad());
	applyLanguage();   // Notepad++ can tell its language for sure now

	const int bookmark = static_cast<int>(::SendMessage(notepad(), NPPM_GETBOOKMARKID, 0, 0));
	if (bookmark > 0)
		m_bookmarkMarker = bookmark;
	int marker = 0;
	if (::SendMessage(notepad(), NPPM_ALLOCATEMARKER, 1, reinterpret_cast<LPARAM>(&marker)) && marker > 0)
		m_flashMarker = marker;

	WNDCLASSEXW hostClass{ sizeof(WNDCLASSEXW) };
	hostClass.lpfnWndProc = &Plugin::hostProc;
	hostClass.hInstance = m_module;
	hostClass.lpszClassName = kHostClass;
	::RegisterClassExW(&hostClass);
	m_host = ::CreateWindowExW(0, kHostClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, m_module, nullptr);
	m_flashHelper = ::CreateWindowExW(0, L"Scintilla", L"", WS_POPUP, 0, 0, 1, 1, notepad(), nullptr, m_module, nullptr);

	m_preview.create(m_module, notepad());
	m_bars[0].attach(*this, m_npp._scintillaMainHandle, 0);
	m_bars[1].attach(*this, m_npp._scintillaSecondHandle, 1);
	if (m_flashMarker >= 0) {
		// Defined in both views, so a document shown twice never gets a margin symbol
		for (MarkerBar& bar : m_bars) {
			if (bar.attached())
				prepareFlashMarker(bar.editor());
		}
	}
	m_started = true;
	requestMarkerNotifications();
	updateMenu();

	for (MarkerBar& bar : m_bars)
		bar.onSettingsChanged();
}

void Plugin::stop() {
	if (!m_started)
		return;
	m_started = false;
	clearFlash();
	for (MarkerBar& bar : m_bars)
		bar.detach();
	m_preview.destroy();
	if (m_flashHelper)
		::DestroyWindow(m_flashHelper);
	m_flashHelper = nullptr;
	if (m_host)
		::DestroyWindow(m_host);
	m_host = nullptr;
	::UnregisterClassW(kHostClass, m_module);
}

void Plugin::requestMarkerNotifications() {
	// Notepad++ 8.7.7+ only forwards some kinds of SCN_MODIFIED. Bookmarks need
	// marker changes; ask once, and only when bookmarks are shown.
	if (m_markerNotifications || !m_settings.enabled || !m_settings.bookmarks.enabled)
		return;
	::SendMessage(notepad(), NPPM_ADDSCNMODIFIEDFLAGS, 0, SC_MOD_CHANGEMARKER);
	m_markerNotifications = true;
}

void Plugin::updateMenu() {
	::SendMessage(notepad(), NPPM_SETMENUITEMCHECK, static_cast<WPARAM>(m_commands[CommandToggle]._cmdID), m_settings.enabled ? TRUE : FALSE);

	// Rename the commands after a language change, keeping any shortcut text
	HMENU menu = reinterpret_cast<HMENU>(::SendMessage(notepad(), NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0));
	if (!menu)
		return;
	const std::pair<Command, Text> names[] = {
		{ CommandToggle, Text::MenuShowMarkers }, { CommandSettings, Text::MenuSettings },
		{ CommandNext, Text::MenuNext }, { CommandPrevious, Text::MenuPrevious }, { CommandAbout, Text::MenuAbout },
	};
	for (const auto& [command, text] : names) {
		const UINT id = static_cast<UINT>(m_commands[command]._cmdID);
		wchar_t current[256] = {};
		if (!::GetMenuStringW(menu, id, current, static_cast<int>(std::size(current)), MF_BYCOMMAND))
			continue;
		std::wstring label = tr(text);
		if (const wchar_t* tab = wcschr(current, L'\t'))
			label += tab;
		if (label == current)
			continue;
		MENUITEMINFOW info{ sizeof(MENUITEMINFOW) };
		info.fMask = MIIM_STRING;
		info.dwTypeData = label.data();
		::SetMenuItemInfoW(menu, id, FALSE, &info);
	}
	::DrawMenuBar(notepad());
}

int Plugin::searchFlags() const {
	const bool matchCase = resolve(m_settings.matchCase, m_preferences.matchCase);
	const bool wholeWord = resolve(m_settings.wholeWord, m_preferences.wholeWord);
	return (matchCase ? SCFIND_MATCHCASE : 0) | (wholeWord ? SCFIND_WHOLEWORD : 0);
}

Palette Plugin::palette(const Editor& editor, const Settings& settings) const {
	return automaticPalette(notepad(), editor, settings.otherIndicatorNumbers());
}

BarStyle Plugin::barStyle(const Editor& editor) const {
	return resolveBarStyle(m_settings, palette(editor, m_settings));
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

// ---------------------------------------------------------------------------
// Flash: a translucent line background for a moment after a click

void Plugin::prepareFlashMarker(const Editor& editor) const {
	if (m_flashMarker < 0)
		return;
	// An opaque line background under the text. Translucent backgrounds over the
	// text are not drawn by every Notepad++ build, so the color is mixed with the
	// editor background here instead.
	const uptr_t marker = static_cast<uptr_t>(m_flashMarker);
	editor.call(SCI_MARKERDEFINE, marker, SC_MARK_BACKGROUND);
	editor.call(SCI_MARKERSETLAYER, marker, SC_LAYER_BASE);
}

void Plugin::flashLine(const Editor& editor, Sci_Position line) {
	if (m_flashMarker < 0 || !m_settings.flashLine)
		return;
	clearFlash();

	const uptr_t marker = static_cast<uptr_t>(m_flashMarker);
	const COLORREF color = m_settings.flashColor.automatic ? palette(editor, m_settings).flash : m_settings.flashColor.color;
	prepareFlashMarker(editor);
	const COLORREF back = static_cast<COLORREF>(editor.call(SCI_STYLEGETBACK, STYLE_DEFAULT));
	auto mix = [](int a, int b) { return a + (b - a) * 35 / 100; };
	const COLORREF shade = RGB(mix(GetRValue(back), GetRValue(color)), mix(GetGValue(back), GetGValue(color)), mix(GetBValue(back), GetBValue(color)));
	editor.call(SCI_MARKERSETBACK, marker, static_cast<sptr_t>(shade));

	m_changingFlash = true;
	m_flashHandle = static_cast<int>(editor.call(SCI_MARKERADD, static_cast<uptr_t>(line), m_flashMarker));
	m_changingFlash = false;
	if (m_flashHandle < 0)
		return;
	// Keep the document alive until the marker is removed again
	m_flashDocument = editor.document();
	editor.call(SCI_ADDREFDOCUMENT, 0, m_flashDocument);
	::SetTimer(m_host, kFlashTimer, static_cast<UINT>(m_settings.flashDuration), nullptr);
}

void Plugin::clearFlash() {
	if (m_host)
		::KillTimer(m_host, kFlashTimer);
	if (!m_flashDocument)
		return;

	m_changingFlash = true;
	bool removed = false;
	for (MarkerBar& bar : m_bars) {
		if (bar.attached() && bar.editor().document() == m_flashDocument) {
			bar.editor().call(SCI_MARKERDELETEHANDLE, static_cast<uptr_t>(m_flashHandle));
			removed = true;
			break;
		}
	}
	if (!removed && m_flashHelper) {
		// The document is not shown any more: open it in the hidden view to clean up
		const Editor helper(m_flashHelper);
		helper.call(SCI_SETDOCPOINTER, 0, m_flashDocument);
		helper.call(SCI_MARKERDELETEHANDLE, static_cast<uptr_t>(m_flashHandle));
		helper.call(SCI_SETDOCPOINTER, 0, 0);
	}
	m_changingFlash = false;

	Editor(m_flashHelper ? m_flashHelper : m_npp._scintillaMainHandle).call(SCI_RELEASEDOCUMENT, 0, m_flashDocument);
	m_flashDocument = 0;
	m_flashHandle = -1;
}

// ---------------------------------------------------------------------------
// Timers

void Plugin::setTimer(int barIndex, MarkerBar::TimerKind kind, UINT milliseconds) {
	if (m_host)
		::SetTimer(m_host, kTimerBase + static_cast<UINT_PTR>(barIndex) * kTimersPerBar + kind, milliseconds, nullptr);
}

void Plugin::killTimer(int barIndex, MarkerBar::TimerKind kind) {
	if (m_host)
		::KillTimer(m_host, kTimerBase + static_cast<UINT_PTR>(barIndex) * kTimersPerBar + kind);
}

LRESULT CALLBACK Plugin::hostProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_TIMER) {
		Plugin& plugin = instance();
		if (wParam == kFlashTimer) {
			plugin.clearFlash();
			return 0;
		}
		if (wParam >= kTimerBase) {
			const UINT_PTR id = wParam - kTimerBase;
			const size_t index = static_cast<size_t>(id / kTimersPerBar);
			if (plugin.m_started && index < std::size(plugin.m_bars))
				plugin.m_bars[index].onTimer(static_cast<MarkerBar::TimerKind>(id % kTimersPerBar));
			else
				::KillTimer(hwnd, wParam);
			return 0;
		}
	}
	return ::DefWindowProcW(hwnd, message, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Commands

void Plugin::applySettings(const Settings& settings) {
	m_settings = settings;
	m_settings.clamp();
	m_settings.save(m_settingsFile);
	applyLanguage();
	m_preview.hide();
	clearFlash();
	requestMarkerNotifications();
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
	else
		applyLanguage();   // the dialog may have previewed another language
}

void Plugin::jump(bool forward) {
	if (!m_started)
		return;
	const HWND current = currentEditor().hwnd();
	if (MarkerBar* bar = barFor(current)) {
		if (!bar->jump(forward))
			::MessageBeep(MB_OK);
	}
}

void Plugin::showAbout() {
	const std::wstring text = std::wstring(SCROLLMARKS_NAME) + L" " + SCROLLMARKS_VERSION_WSTRING + L"\n\n" + tr(Text::AboutBody) +
		L"\n\n" + SCROLLMARKS_HOMEPAGE + L"\n\nGNU General Public License v3";
	::MessageBoxW(notepad(), text.c_str(), tr(Text::AboutTitle), MB_OK | MB_ICONINFORMATION);
}
