// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "SettingsDialog.h"

#include <commdlg.h>
#include <string>

#include "Dpi.h"
#include "Plugin.h"
#include "resource.h"

namespace {

void setCheck(HWND dialog, int id, bool value) {
	::CheckDlgButton(dialog, id, value ? BST_CHECKED : BST_UNCHECKED);
}

bool isChecked(HWND dialog, int id) {
	return ::IsDlgButtonChecked(dialog, id) == BST_CHECKED;
}

int readNumber(HWND dialog, int id, int fallback) {
	BOOL ok = FALSE;
	const UINT value = ::GetDlgItemInt(dialog, id, &ok, FALSE);
	return ok ? static_cast<int>(value) : fallback;
}

} // namespace

SettingsDialog::SettingsDialog(Plugin& plugin, const Settings& settings)
	: m_plugin(plugin), m_settings(settings) {
	for (COLORREF& color : m_customColors)
		color = RGB(255, 255, 255);
}

bool SettingsDialog::run(HINSTANCE module, HWND parent) {
	m_themeColors = m_plugin.themeColors(m_plugin.currentEditor());
	return ::DialogBoxParamW(module, MAKEINTRESOURCEW(IDD_SETTINGS), parent, &SettingsDialog::dialogProc,
		reinterpret_cast<LPARAM>(this)) == IDOK;
}

INT_PTR CALLBACK SettingsDialog::dialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_INITDIALOG) {
		::SetWindowLongPtrW(hwnd, DWLP_USER, lParam);
		reinterpret_cast<SettingsDialog*>(lParam)->m_hwnd = hwnd;
	}
	auto* self = reinterpret_cast<SettingsDialog*>(::GetWindowLongPtrW(hwnd, DWLP_USER));
	return self ? self->handle(message, wParam, lParam) : FALSE;
}

INT_PTR SettingsDialog::handle(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_INITDIALOG:
		load();
		// Notepad++ 8.5.4 and later theme the dialog in dark mode
		::SendMessage(m_plugin.notepad(), NPPM_DARKMODESUBCLASSANDTHEME, static_cast<WPARAM>(NppDarkMode::dmfInit),
			reinterpret_cast<LPARAM>(m_hwnd));
		return TRUE;

	case WM_DRAWITEM:
		drawColorButton(*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			store();
			::EndDialog(m_hwnd, IDOK);
			return TRUE;
		case IDCANCEL:
			::EndDialog(m_hwnd, IDCANCEL);
			return TRUE;
		case IDC_RESET:
			m_settings = Settings();
			load();
			return TRUE;
		case IDC_OCCURRENCE_COLOR:
		case IDC_CURRENT_COLOR:
		case IDC_BACKGROUND_COLOR:
			pickColor(LOWORD(wParam));
			return TRUE;
		case IDC_OCCURRENCE_COLOR_AUTO:
		case IDC_CURRENT_COLOR_AUTO:
		case IDC_BACKGROUND_COLOR_AUTO:
			if (HIWORD(wParam) == BN_CLICKED) {
				// A fixed color starts from the color that was shown automatically
				const int button = LOWORD(wParam) - 1;
				ColorChoice& choice = colorFor(button);
				if (!isChecked(m_hwnd, LOWORD(wParam)) && choice.automatic)
					choice.color = shownColorFor(button, true);
				choice.automatic = isChecked(m_hwnd, LOWORD(wParam));
				updateControls();
			}
			return TRUE;
		case IDC_ENABLED:
		case IDC_BAR_WIDTH_AUTO:
		case IDC_PREVIEW:
		case IDC_PREVIEW_DELAY_AUTO:
			if (HIWORD(wParam) == BN_CLICKED)
				updateControls();
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void SettingsDialog::fillChoice(int id, bool notepadValue, Choice value) {
	const HWND combo = ::GetDlgItem(m_hwnd, id);
	::SendMessageW(combo, CB_RESETCONTENT, 0, 0);
	const std::wstring follow = std::wstring(L"Like Notepad++ (") + (notepadValue ? L"on" : L"off") + L")";
	::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(follow.c_str()));
	::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"On"));
	::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Off"));
	::SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(value), 0);
}

int SettingsDialog::automaticBarWidth() const {
	return Dpi::systemMetric(SM_CXVSCROLL, 96) / 2;
}

int SettingsDialog::automaticPreviewDelay() const {
	UINT hoverTime = 400;
	::SystemParametersInfoW(SPI_GETMOUSEHOVERTIME, 0, &hoverTime, 0);
	return static_cast<int>(hoverTime);
}

void SettingsDialog::load() {
	const Settings& s = m_settings;
	const NppPreferences& notepad = m_plugin.preferences();

	setCheck(m_hwnd, IDC_ENABLED, s.enabled);
	setCheck(m_hwnd, IDC_FOLLOW_SMART, s.followSmartHighlighting);
	fillChoice(IDC_MATCH_CASE, notepad.matchCase, s.matchCase);
	fillChoice(IDC_WHOLE_WORD, notepad.wholeWord, s.wholeWord);
	::SetDlgItemInt(m_hwnd, IDC_MIN_LENGTH, static_cast<UINT>(s.minimumLength), FALSE);
	::SetDlgItemInt(m_hwnd, IDC_MAX_MARKERS, static_cast<UINT>(s.maximumMarkers), FALSE);
	setCheck(m_hwnd, IDC_WORD_AT_CARET, s.useWordAtCaret);
	setCheck(m_hwnd, IDC_LARGE_FILES, s.markLargeFiles);

	setCheck(m_hwnd, IDC_BAR_WIDTH_AUTO, s.barWidth.automatic);
	::SetDlgItemInt(m_hwnd, IDC_BAR_WIDTH, static_cast<UINT>(s.barWidth.automatic ? automaticBarWidth() : s.barWidth.value), FALSE);
	::SetDlgItemInt(m_hwnd, IDC_MARKER_HEIGHT, static_cast<UINT>(s.minimumMarkerHeight), FALSE);
	setCheck(m_hwnd, IDC_OCCURRENCE_COLOR_AUTO, s.occurrenceColor.automatic);
	setCheck(m_hwnd, IDC_CURRENT_COLOR_AUTO, s.currentColor.automatic);
	setCheck(m_hwnd, IDC_BACKGROUND_COLOR_AUTO, s.backgroundColor.automatic);

	setCheck(m_hwnd, IDC_SELECT_ON_CLICK, s.selectOnClick);
	setCheck(m_hwnd, IDC_CENTER_ON_CLICK, s.centerOnClick);
	setCheck(m_hwnd, IDC_SCROLL_ON_EMPTY, s.scrollOnEmptyClick);
	setCheck(m_hwnd, IDC_PREVIEW, s.previewEnabled);
	::SetDlgItemInt(m_hwnd, IDC_CONTEXT_LINES, static_cast<UINT>(s.previewContextLines), FALSE);
	setCheck(m_hwnd, IDC_PREVIEW_DELAY_AUTO, s.previewDelay.automatic);
	::SetDlgItemInt(m_hwnd, IDC_PREVIEW_DELAY, static_cast<UINT>(s.previewDelay.automatic ? automaticPreviewDelay() : s.previewDelay.value), FALSE);
	::SetDlgItemInt(m_hwnd, IDC_PREVIEW_WIDTH, static_cast<UINT>(s.previewWidthPercent), FALSE);

	updateControls();
}

void SettingsDialog::store() {
	Settings& s = m_settings;

	s.enabled = isChecked(m_hwnd, IDC_ENABLED);
	s.followSmartHighlighting = isChecked(m_hwnd, IDC_FOLLOW_SMART);
	s.matchCase = static_cast<Choice>(::SendDlgItemMessageW(m_hwnd, IDC_MATCH_CASE, CB_GETCURSEL, 0, 0));
	s.wholeWord = static_cast<Choice>(::SendDlgItemMessageW(m_hwnd, IDC_WHOLE_WORD, CB_GETCURSEL, 0, 0));
	s.minimumLength = readNumber(m_hwnd, IDC_MIN_LENGTH, s.minimumLength);
	s.maximumMarkers = readNumber(m_hwnd, IDC_MAX_MARKERS, s.maximumMarkers);
	s.useWordAtCaret = isChecked(m_hwnd, IDC_WORD_AT_CARET);
	s.markLargeFiles = isChecked(m_hwnd, IDC_LARGE_FILES);

	s.barWidth.automatic = isChecked(m_hwnd, IDC_BAR_WIDTH_AUTO);
	if (!s.barWidth.automatic)
		s.barWidth.value = readNumber(m_hwnd, IDC_BAR_WIDTH, automaticBarWidth());
	s.minimumMarkerHeight = readNumber(m_hwnd, IDC_MARKER_HEIGHT, s.minimumMarkerHeight);
	s.occurrenceColor.automatic = isChecked(m_hwnd, IDC_OCCURRENCE_COLOR_AUTO);
	s.currentColor.automatic = isChecked(m_hwnd, IDC_CURRENT_COLOR_AUTO);
	s.backgroundColor.automatic = isChecked(m_hwnd, IDC_BACKGROUND_COLOR_AUTO);

	s.selectOnClick = isChecked(m_hwnd, IDC_SELECT_ON_CLICK);
	s.centerOnClick = isChecked(m_hwnd, IDC_CENTER_ON_CLICK);
	s.scrollOnEmptyClick = isChecked(m_hwnd, IDC_SCROLL_ON_EMPTY);
	s.previewEnabled = isChecked(m_hwnd, IDC_PREVIEW);
	s.previewContextLines = readNumber(m_hwnd, IDC_CONTEXT_LINES, s.previewContextLines);
	s.previewDelay.automatic = isChecked(m_hwnd, IDC_PREVIEW_DELAY_AUTO);
	if (!s.previewDelay.automatic)
		s.previewDelay.value = readNumber(m_hwnd, IDC_PREVIEW_DELAY, automaticPreviewDelay());
	s.previewWidthPercent = readNumber(m_hwnd, IDC_PREVIEW_WIDTH, s.previewWidthPercent);

	s.clamp();
}

void SettingsDialog::updateControls() {
	const bool enabled = isChecked(m_hwnd, IDC_ENABLED);
	const bool preview = enabled && isChecked(m_hwnd, IDC_PREVIEW);
	const int all[] = {
		IDC_FOLLOW_SMART, IDC_MATCH_CASE, IDC_WHOLE_WORD, IDC_MIN_LENGTH, IDC_MAX_MARKERS, IDC_WORD_AT_CARET, IDC_LARGE_FILES,
		IDC_BAR_WIDTH_AUTO, IDC_MARKER_HEIGHT, IDC_OCCURRENCE_COLOR_AUTO, IDC_CURRENT_COLOR_AUTO, IDC_BACKGROUND_COLOR_AUTO,
		IDC_SELECT_ON_CLICK, IDC_CENTER_ON_CLICK, IDC_SCROLL_ON_EMPTY, IDC_PREVIEW,
	};
	for (int id : all)
		::EnableWindow(::GetDlgItem(m_hwnd, id), enabled);

	// Automatic values are shown but cannot be edited
	const bool barWidthAuto = isChecked(m_hwnd, IDC_BAR_WIDTH_AUTO);
	if (barWidthAuto)
		::SetDlgItemInt(m_hwnd, IDC_BAR_WIDTH, static_cast<UINT>(automaticBarWidth()), FALSE);
	::EnableWindow(::GetDlgItem(m_hwnd, IDC_BAR_WIDTH), enabled && !barWidthAuto);

	::EnableWindow(::GetDlgItem(m_hwnd, IDC_OCCURRENCE_COLOR), enabled && !isChecked(m_hwnd, IDC_OCCURRENCE_COLOR_AUTO));
	::EnableWindow(::GetDlgItem(m_hwnd, IDC_CURRENT_COLOR), enabled && !isChecked(m_hwnd, IDC_CURRENT_COLOR_AUTO));
	::EnableWindow(::GetDlgItem(m_hwnd, IDC_BACKGROUND_COLOR), enabled && !isChecked(m_hwnd, IDC_BACKGROUND_COLOR_AUTO));

	::EnableWindow(::GetDlgItem(m_hwnd, IDC_CONTEXT_LINES), preview);
	::EnableWindow(::GetDlgItem(m_hwnd, IDC_PREVIEW_DELAY_AUTO), preview);
	::EnableWindow(::GetDlgItem(m_hwnd, IDC_PREVIEW_WIDTH), preview);
	const bool delayAuto = isChecked(m_hwnd, IDC_PREVIEW_DELAY_AUTO);
	if (delayAuto)
		::SetDlgItemInt(m_hwnd, IDC_PREVIEW_DELAY, static_cast<UINT>(automaticPreviewDelay()), FALSE);
	::EnableWindow(::GetDlgItem(m_hwnd, IDC_PREVIEW_DELAY), preview && !delayAuto);

	for (int id : { IDC_OCCURRENCE_COLOR, IDC_CURRENT_COLOR, IDC_BACKGROUND_COLOR })
		::InvalidateRect(::GetDlgItem(m_hwnd, id), nullptr, TRUE);
}

ColorChoice& SettingsDialog::colorFor(int buttonId) {
	if (buttonId == IDC_CURRENT_COLOR)
		return m_settings.currentColor;
	if (buttonId == IDC_BACKGROUND_COLOR)
		return m_settings.backgroundColor;
	return m_settings.occurrenceColor;
}

COLORREF SettingsDialog::shownColorFor(int buttonId, bool automatic) const {
	switch (buttonId) {
	case IDC_CURRENT_COLOR:
		return automatic ? m_themeColors.current : m_settings.currentColor.color;
	case IDC_BACKGROUND_COLOR:
		return automatic ? m_themeColors.background : m_settings.backgroundColor.color;
	default:
		return automatic ? m_themeColors.occurrence : m_settings.occurrenceColor.color;
	}
}

COLORREF SettingsDialog::shownColor(int buttonId) const {
	return shownColorFor(buttonId, isChecked(m_hwnd, buttonId + 1));   // the "Automatic" box follows its button
}

void SettingsDialog::pickColor(int buttonId) {
	ColorChoice& choice = colorFor(buttonId);
	CHOOSECOLORW chooser{ sizeof(CHOOSECOLORW) };
	chooser.hwndOwner = m_hwnd;
	chooser.rgbResult = shownColor(buttonId);
	chooser.lpCustColors = m_customColors;
	chooser.Flags = CC_FULLOPEN | CC_RGBINIT;
	if (::ChooseColorW(&chooser)) {
		choice.color = chooser.rgbResult;
		::InvalidateRect(::GetDlgItem(m_hwnd, buttonId), nullptr, TRUE);
	}
}

void SettingsDialog::drawColorButton(const DRAWITEMSTRUCT& item) const {
	RECT rect = item.rcItem;
	const bool disabled = (item.itemState & ODS_DISABLED) != 0;
	::DrawEdge(item.hDC, &rect, (item.itemState & ODS_SELECTED) ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT | BF_ADJUST);
	::InflateRect(&rect, -2, -2);
	if (disabled) {
		// Automatic colors are shown inside a gray frame: visible, not editable
		HBRUSH frame = ::CreateSolidBrush(::GetSysColor(COLOR_GRAYTEXT));
		::FrameRect(item.hDC, &rect, frame);
		::DeleteObject(frame);
		::InflateRect(&rect, -2, -2);
	}
	HBRUSH brush = ::CreateSolidBrush(shownColor(static_cast<int>(item.CtlID)));
	::FillRect(item.hDC, &rect, brush);
	::DeleteObject(brush);
	if (item.itemState & ODS_FOCUS) {
		RECT focus = item.rcItem;
		::InflateRect(&focus, -1, -1);
		::DrawFocusRect(item.hDC, &focus);
	}
}
