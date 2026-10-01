// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "SettingsDialog.h"

#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <algorithm>
#include <string>

#include "Dpi.h"
#include "Plugin.h"
#include "Strings.h"
#include "resource.h"

namespace {

constexpr int kPageTemplates[] = { IDD_PAGE_GENERAL, IDD_PAGE_OCCURRENCES, IDD_PAGE_MARKS, IDD_PAGE_CARET, IDD_PAGE_NAVIGATION };
constexpr Text kPageTitles[] = { Text::TabGeneral, Text::TabOccurrences, Text::TabOtherMarks, Text::TabCaretLine, Text::TabNavigation };

// Every control whose text depends on the language
struct Label {
	int id;
	Text text;
};

const Label kLabels[] = {
	{ IDC_T_HOVER_GROUP, Text::HoverGroup }, { IDC_PREVIEW, Text::ShowPreview }, { IDC_T_CONTEXT_LINES, Text::ContextLines },
	{ IDC_T_ABOVE_BELOW, Text::AboveAndBelow }, { IDC_T_HOVER_DELAY, Text::HoverDelay }, { IDC_T_MS1, Text::Milliseconds },
	{ IDC_PREVIEW_DELAY_AUTO, Text::Automatic }, { IDC_T_PREVIEW_WIDTH, Text::PreviewWidth }, { IDC_T_PERCENT_EDITOR, Text::PercentOfEditor },
	{ IDC_T_SAMPLE_GROUP, Text::SampleGroup }, { IDC_RESET, Text::ResetDefaults }, { IDOK, Text::Ok }, { IDCANCEL, Text::Cancel },

	{ IDC_ENABLED, Text::ShowMarkers }, { IDC_T_LANGUAGE, Text::Language }, { IDC_T_BAR_WIDTH, Text::BarWidth },
	{ IDC_T_PX1, Text::Pixels }, { IDC_BAR_WIDTH_AUTO, Text::Automatic }, { IDC_T_MARKER_HEIGHT, Text::MinimumMarkerHeight },
	{ IDC_T_PX2, Text::Pixels }, { IDC_T_BACKGROUND, Text::Background }, { IDC_BACKGROUND_COLOR_AUTO, Text::Automatic },
	{ IDC_T_PRIORITY, Text::PriorityNote },

	{ IDC_OCC_ENABLED, Text::MarkOccurrences }, { IDC_FOLLOW_SMART, Text::FollowSmartHighlighting }, { IDC_T_MATCH_CASE, Text::MatchCase },
	{ IDC_T_WHOLE_WORD, Text::WholeWord }, { IDC_T_MIN_LENGTH, Text::MinimumLength }, { IDC_T_CHARACTERS, Text::Characters },
	{ IDC_T_MAX_MARKERS, Text::MaximumMarkers }, { IDC_T_NO_LIMIT, Text::NoLimit }, { IDC_WORD_AT_CARET, Text::WordAtCaret },
	{ IDC_LARGE_FILES, Text::LargeFiles }, { IDC_T_COLOR1, Text::ColumnColor }, { IDC_T_POSITION1, Text::ColumnPosition },
	{ IDC_T_WIDTH1, Text::ColumnWidth }, { IDC_T_OCC, Text::OccurrencesRow }, { IDC_OCC_COLOR_AUTO, Text::Automatic },
	{ IDC_T_PCT1, Text::Percent }, { IDC_T_CUR, Text::CurrentRow }, { IDC_CUR_COLOR_AUTO, Text::Automatic }, { IDC_T_PCT2, Text::Percent },

	{ IDC_T_COLOR2, Text::ColumnColor }, { IDC_T_POSITION2, Text::ColumnPosition }, { IDC_T_WIDTH2, Text::ColumnWidth },
	{ IDC_HIST_ENABLED, Text::ChangeHistory }, { IDC_T_PCT3, Text::Percent }, { IDC_T_HIST_MODIFIED, Text::HistoryModified },
	{ IDC_T_HIST_SAVED, Text::HistorySaved }, { IDC_T_HIST_REVERTED, Text::HistoryReverted },
	{ IDC_T_HIST_REVERTED_MOD, Text::HistoryRevertedModified }, { IDC_HIST_COLOR_AUTO, Text::Automatic },
	{ IDC_BOOK_ENABLED, Text::Bookmarks }, { IDC_BOOK_COLOR_AUTO, Text::Automatic }, { IDC_T_PCT4, Text::Percent },
	{ IDC_FIND_ENABLED, Text::FindMarks }, { IDC_FIND_COLOR_AUTO, Text::Automatic }, { IDC_T_PCT5, Text::Percent },
	{ IDC_TOKEN_ENABLED, Text::StyleTokens }, { IDC_T_PCT6, Text::Percent }, { IDC_TOKEN_COLOR_AUTO, Text::Automatic },
	{ IDC_OTHER_ENABLED, Text::OtherIndicators }, { IDC_OTHER_COLOR_AUTO, Text::Automatic }, { IDC_T_PCT7, Text::Percent },
	{ IDC_T_OTHER_NUMBERS, Text::IndicatorNumbers }, { IDC_T_OTHER_HINT, Text::IndicatorNumbersHint },

	{ IDC_CARET_ENABLED, Text::ShowCaretLine }, { IDC_T_CARET_COLOR, Text::Color }, { IDC_CARET_COLOR_AUTO, Text::Automatic },
	{ IDC_T_CARET_THICKNESS, Text::Thickness }, { IDC_T_PX3, Text::Pixels }, { IDC_T_CARET_POSITION, Text::Position },
	{ IDC_T_CARET_WIDTH, Text::Width }, { IDC_T_PCT8, Text::Percent },

	{ IDC_MOVE_CARET, Text::MoveCaretOnClick }, { IDC_CENTER_ON_CLICK, Text::CenterOnClick }, { IDC_FLASH, Text::FlashLine },
	{ IDC_T_FLASH_DURATION, Text::Duration }, { IDC_T_MS2, Text::Milliseconds }, { IDC_T_FLASH_COLOR, Text::Color },
	{ IDC_FLASH_COLOR_AUTO, Text::Automatic }, { IDC_SCROLL_ON_EMPTY, Text::ScrollOnEmptyClick }, { IDC_T_JUMP_GROUP, Text::JumpGroup },
	{ IDC_WRAP, Text::WrapAround }, { IDC_CENTER_ON_JUMP, Text::CenterOnJump }, { IDC_T_SHORTCUT, Text::ShortcutNote },
};

const int kPositionCombos[] = { IDC_OCC_POSITION, IDC_CUR_POSITION, IDC_HIST_POSITION, IDC_BOOK_POSITION, IDC_FIND_POSITION,
	IDC_TOKEN_POSITION, IDC_OTHER_POSITION, IDC_CARET_POSITION };

COLORREF blend(COLORREF from, COLORREF to, int percent) {
	auto mix = [percent](int a, int b) { return a + (b - a) * percent / 100; };
	return RGB(mix(GetRValue(from), GetRValue(to)), mix(GetGValue(from), GetGValue(to)), mix(GetBValue(from), GetBValue(to)));
}

void fillCombo(HWND combo, std::initializer_list<const wchar_t*> items, int selection) {
	::SendMessageW(combo, CB_RESETCONTENT, 0, 0);
	for (const wchar_t* item : items)
		::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
	::SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(selection), 0);
}

int comboSelection(HWND combo, int fallback) {
	const LRESULT selection = ::SendMessageW(combo, CB_GETCURSEL, 0, 0);
	return selection == CB_ERR ? fallback : static_cast<int>(selection);
}

} // namespace

SettingsDialog::SettingsDialog(Plugin& plugin, const Settings& settings)
	: m_plugin(plugin), m_settings(settings) {
	for (COLORREF& color : m_customColors)
		color = RGB(255, 255, 255);
}

bool SettingsDialog::run(HINSTANCE module, HWND parent) {
	m_module = module;
	m_editor = m_plugin.currentEditor();
	m_palette = m_plugin.palette(m_editor, m_settings);
	INT_PTR result = IDCANCEL;
	do {
		// A language change reopens the dialog, so every text is laid out in the new language
		m_hwnd = nullptr;
		for (HWND& page : m_pages)
			page = nullptr;
		result = ::DialogBoxParamW(module, MAKEINTRESOURCEW(IDD_SETTINGS), parent, &SettingsDialog::frameProc,
			reinterpret_cast<LPARAM>(this));
	} while (result == IDRETRY);
	return result == IDOK;
}

// ---------------------------------------------------------------------------
// Window procedures

INT_PTR CALLBACK SettingsDialog::frameProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_INITDIALOG) {
		::SetWindowLongPtrW(hwnd, DWLP_USER, lParam);
		reinterpret_cast<SettingsDialog*>(lParam)->m_hwnd = hwnd;
	}
	auto* self = reinterpret_cast<SettingsDialog*>(::GetWindowLongPtrW(hwnd, DWLP_USER));
	return self ? self->handle(message, wParam, lParam) : FALSE;
}

INT_PTR CALLBACK SettingsDialog::pageProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_INITDIALOG) {
		::SetWindowLongPtrW(hwnd, DWLP_USER, lParam);
		return FALSE;
	}
	auto* self = reinterpret_cast<SettingsDialog*>(::GetWindowLongPtrW(hwnd, DWLP_USER));
	// Pages hand their commands and drawing to the dialog
	if (self && (message == WM_COMMAND || message == WM_DRAWITEM))
		return self->handle(message, wParam, lParam);
	return FALSE;
}

INT_PTR SettingsDialog::handle(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_INITDIALOG:
		createPages();
		applyTexts();
		load();
		showPage(m_page);
		// Notepad++ 8.5.4 and later theme the dialog and its pages in dark mode
		::SendMessage(m_plugin.notepad(), NPPM_DARKMODESUBCLASSANDTHEME, static_cast<WPARAM>(NppDarkMode::dmfInit),
			reinterpret_cast<LPARAM>(m_hwnd));
		return TRUE;

	case WM_NOTIFY: {
		const NMHDR* header = reinterpret_cast<const NMHDR*>(lParam);
		if (header->idFrom == IDC_TABS && header->code == TCN_SELCHANGE)
			showPage(TabCtrl_GetCurSel(control(IDC_TABS)));
		break;
	}

	case WM_DRAWITEM: {
		const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
		if (item->CtlID == IDC_SAMPLE)
			drawSample(*item);
		else
			drawColorButton(*item);
		::SetWindowLongPtrW(m_hwnd, DWLP_MSGRESULT, TRUE);
		return TRUE;
	}

	case WM_COMMAND: {
		const int id = LOWORD(wParam);
		const int code = HIWORD(wParam);
		switch (id) {
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
			changed();
			return TRUE;
		}
		if (code == BN_CLICKED) {
			for (const ColorSlot& slot : colorSlots()) {
				if (slot.button == id) {
					pickColor(id);
					return TRUE;
				}
				if (slot.autoBox == id) {
					colorAutoClicked(id);
					return TRUE;
				}
			}
			changed();
			return TRUE;
		}
		if (code == EN_CHANGE || code == CBN_SELCHANGE) {
			changed();
			if (id == IDC_LANGUAGE && code == CBN_SELCHANGE && !m_loading) {
				// Show the dialog in the chosen language right away
				const int choice = comboSelection(control(IDC_LANGUAGE), 0);
				Strings::setKorean(choice == 2 || (choice == 0 && Strings::notepadIsKorean(m_plugin.notepad())));
				::EndDialog(m_hwnd, IDRETRY);
			}
			return TRUE;
		}
		break;
	}
	}
	return FALSE;
}

// ---------------------------------------------------------------------------
// Control helpers

HWND SettingsDialog::control(int id) const {
	if (HWND item = ::GetDlgItem(m_hwnd, id))
		return item;
	for (HWND page : m_pages) {
		if (HWND item = page ? ::GetDlgItem(page, id) : nullptr)
			return item;
	}
	return nullptr;
}

bool SettingsDialog::checked(int id) const {
	return ::SendMessageW(control(id), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void SettingsDialog::check(int id, bool value) {
	::SendMessageW(control(id), BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
}

int SettingsDialog::number(int id, int fallback) const {
	wchar_t text[32] = {};
	::GetWindowTextW(control(id), text, static_cast<int>(std::size(text)));
	wchar_t* end = nullptr;
	const long value = std::wcstol(text, &end, 10);
	return (text[0] && end && *end == L'\0') ? static_cast<int>(value) : fallback;
}

void SettingsDialog::setNumber(int id, int value) {
	::SetWindowTextW(control(id), std::to_wstring(value).c_str());
}

void SettingsDialog::enable(int id, bool value) {
	::EnableWindow(control(id), value ? TRUE : FALSE);
}

// ---------------------------------------------------------------------------
// Pages and texts

void SettingsDialog::createPages() {
	const HWND tabs = control(IDC_TABS);
	for (int i = 0; i < kPageCount; ++i) {
		TCITEMW item{};
		item.mask = TCIF_TEXT;
		item.pszText = const_cast<wchar_t*>(tr(kPageTitles[i]));
		TabCtrl_InsertItem(tabs, i, &item);
	}

	RECT area{};
	::GetWindowRect(tabs, &area);
	::MapWindowPoints(nullptr, m_hwnd, reinterpret_cast<POINT*>(&area), 2);
	TabCtrl_AdjustRect(tabs, FALSE, &area);

	for (int i = 0; i < kPageCount; ++i) {
		m_pages[i] = ::CreateDialogParamW(m_module, MAKEINTRESOURCEW(kPageTemplates[i]), m_hwnd, &SettingsDialog::pageProc,
			reinterpret_cast<LPARAM>(this));
		if (!m_pages[i])
			continue;
		::EnableThemeDialogTexture(m_pages[i], ETDT_ENABLETAB);
		::SetWindowPos(m_pages[i], tabs, area.left, area.top, area.right - area.left, area.bottom - area.top, SWP_NOACTIVATE);
	}
}

void SettingsDialog::showPage(int index) {
	m_page = std::clamp(index, 0, kPageCount - 1);
	for (int i = 0; i < kPageCount; ++i) {
		if (m_pages[i])
			::ShowWindow(m_pages[i], i == m_page ? SW_SHOW : SW_HIDE);
	}
	TabCtrl_SetCurSel(control(IDC_TABS), m_page);
}

void SettingsDialog::applyTexts() {
	const bool wasLoading = m_loading;
	m_loading = true;
	::SetWindowTextW(m_hwnd, tr(Text::DialogTitle));
	for (const Label& label : kLabels) {
		if (HWND item = control(label.id))
			::SetWindowTextW(item, tr(label.text));
	}
	const HWND tabs = control(IDC_TABS);
	for (int i = 0; i < kPageCount; ++i) {
		TCITEMW item{};
		item.mask = TCIF_TEXT;
		item.pszText = const_cast<wchar_t*>(tr(kPageTitles[i]));
		TabCtrl_SetItem(tabs, i, &item);
	}
	fillCombos();
	m_loading = wasLoading;
}

void SettingsDialog::fillCombos() {
	const NppPreferences& notepad = m_plugin.preferences();
	const int language = static_cast<int>(m_settings.language);
	fillCombo(control(IDC_LANGUAGE), { tr(Text::LanguageAuto), L"English", L"한국어" }, language);
	fillCombo(control(IDC_MATCH_CASE), { tr(notepad.matchCase ? Text::LikeNotepadOn : Text::LikeNotepadOff), tr(Text::On), tr(Text::Off) },
		static_cast<int>(m_settings.matchCase));
	fillCombo(control(IDC_WHOLE_WORD), { tr(notepad.wholeWord ? Text::LikeNotepadOn : Text::LikeNotepadOff), tr(Text::On), tr(Text::Off) },
		static_cast<int>(m_settings.wholeWord));
	const std::vector<LayerRow> rows = layerRows();
	for (const LayerRow& row : rows)
		fillCombo(control(row.positionCombo), { tr(Text::Left), tr(Text::Center), tr(Text::Right) }, static_cast<int>(row.layer->position));
}

std::vector<SettingsDialog::ColorSlot> SettingsDialog::colorSlots() {
	Settings& s = m_settings;
	const Palette& p = m_palette;
	std::vector<ColorSlot> slots = {
		{ IDC_BACKGROUND_COLOR, IDC_BACKGROUND_COLOR_AUTO, &s.backgroundColor, p.background },
		{ IDC_OCC_COLOR, IDC_OCC_COLOR_AUTO, &s.occurrences.color, p.occurrence },
		{ IDC_CUR_COLOR, IDC_CUR_COLOR_AUTO, &s.current.color, p.current },
		{ IDC_BOOK_COLOR, IDC_BOOK_COLOR_AUTO, &s.bookmarks.color, p.bookmark },
		{ IDC_FIND_COLOR, IDC_FIND_COLOR_AUTO, &s.findMarks.color, p.findMark },
		{ IDC_OTHER_COLOR, IDC_OTHER_COLOR_AUTO, &s.otherIndicators.color, p.otherIndicators.empty() ? p.caretLine : p.otherIndicators.front() },
		{ IDC_CARET_COLOR, IDC_CARET_COLOR_AUTO, &s.caretLine.color, p.caretLine },
		{ IDC_FLASH_COLOR, IDC_FLASH_COLOR_AUTO, &s.flashColor, p.flash },
	};
	for (int i = 0; i < kHistoryStates; ++i)
		slots.push_back({ IDC_HIST_COLOR1 + i, IDC_HIST_COLOR_AUTO, &s.historyColors[i], p.history[i] });
	for (int i = 0; i < kStyleTokens; ++i)
		slots.push_back({ IDC_TOKEN_COLOR1 + i, IDC_TOKEN_COLOR_AUTO, &s.tokenColors[i], p.tokens[i] });
	return slots;
}

std::vector<SettingsDialog::LayerRow> SettingsDialog::layerRows() {
	Settings& s = m_settings;
	return {
		{ IDC_OCC_ENABLED, IDC_OCC_POSITION, IDC_OCC_WIDTH, &s.occurrences },
		{ 0, IDC_CUR_POSITION, IDC_CUR_WIDTH, &s.current },
		{ IDC_HIST_ENABLED, IDC_HIST_POSITION, IDC_HIST_WIDTH, &s.changeHistory },
		{ IDC_BOOK_ENABLED, IDC_BOOK_POSITION, IDC_BOOK_WIDTH, &s.bookmarks },
		{ IDC_FIND_ENABLED, IDC_FIND_POSITION, IDC_FIND_WIDTH, &s.findMarks },
		{ IDC_TOKEN_ENABLED, IDC_TOKEN_POSITION, IDC_TOKEN_WIDTH, &s.styleTokens },
		{ IDC_OTHER_ENABLED, IDC_OTHER_POSITION, IDC_OTHER_WIDTH, &s.otherIndicators },
		{ IDC_CARET_ENABLED, IDC_CARET_POSITION, IDC_CARET_WIDTH, &s.caretLine },
	};
}

int SettingsDialog::automaticBarWidth() const {
	return Dpi::systemMetric(SM_CXVSCROLL, 96);
}

int SettingsDialog::automaticPreviewDelay() const {
	UINT hoverTime = 400;
	::SystemParametersInfoW(SPI_GETMOUSEHOVERTIME, 0, &hoverTime, 0);
	return static_cast<int>(hoverTime);
}

// ---------------------------------------------------------------------------
// Settings <-> controls

void SettingsDialog::load() {
	m_loading = true;
	const Settings& s = m_settings;

	check(IDC_ENABLED, s.enabled);
	check(IDC_BAR_WIDTH_AUTO, s.barWidth.automatic);
	setNumber(IDC_BAR_WIDTH, s.barWidth.automatic ? automaticBarWidth() : s.barWidth.value);
	setNumber(IDC_MARKER_HEIGHT, s.minimumMarkerHeight);

	check(IDC_FOLLOW_SMART, s.followSmartHighlighting);
	setNumber(IDC_MIN_LENGTH, s.minimumLength);
	setNumber(IDC_MAX_MARKERS, s.maximumMarkers);
	check(IDC_WORD_AT_CARET, s.useWordAtCaret);
	check(IDC_LARGE_FILES, s.markLargeFiles);

	for (const LayerRow& row : layerRows()) {
		if (row.enabledBox)
			check(row.enabledBox, row.layer->enabled);
		setNumber(row.widthEdit, row.layer->widthPercent);
	}
	::SetWindowTextW(control(IDC_OTHER_LIST), s.otherIndicatorList.c_str());
	setNumber(IDC_CARET_THICKNESS, s.caretLineThickness);

	for (const ColorSlot& slot : colorSlots())
		check(slot.autoBox, slot.choice->automatic);

	check(IDC_MOVE_CARET, s.moveCaretOnClick);
	check(IDC_CENTER_ON_CLICK, s.centerOnClick);
	check(IDC_FLASH, s.flashLine);
	setNumber(IDC_FLASH_DURATION, s.flashDuration);
	check(IDC_SCROLL_ON_EMPTY, s.scrollOnEmptyClick);
	check(IDC_WRAP, s.wrapAround);
	check(IDC_CENTER_ON_JUMP, s.centerOnJump);

	check(IDC_PREVIEW, s.previewEnabled);
	setNumber(IDC_CONTEXT_LINES, s.previewContextLines);
	check(IDC_PREVIEW_DELAY_AUTO, s.previewDelay.automatic);
	setNumber(IDC_PREVIEW_DELAY, s.previewDelay.automatic ? automaticPreviewDelay() : s.previewDelay.value);
	setNumber(IDC_PREVIEW_WIDTH, s.previewWidthPercent);

	fillCombos();
	m_loading = false;
	updateControls();
}

void SettingsDialog::store() {
	Settings& s = m_settings;

	s.enabled = checked(IDC_ENABLED);
	s.language = static_cast<Language>(comboSelection(control(IDC_LANGUAGE), 0));
	s.barWidth.automatic = checked(IDC_BAR_WIDTH_AUTO);
	if (!s.barWidth.automatic)
		s.barWidth.value = number(IDC_BAR_WIDTH, automaticBarWidth());
	s.minimumMarkerHeight = number(IDC_MARKER_HEIGHT, s.minimumMarkerHeight);

	s.followSmartHighlighting = checked(IDC_FOLLOW_SMART);
	s.matchCase = static_cast<Choice>(comboSelection(control(IDC_MATCH_CASE), 0));
	s.wholeWord = static_cast<Choice>(comboSelection(control(IDC_WHOLE_WORD), 0));
	s.minimumLength = number(IDC_MIN_LENGTH, s.minimumLength);
	s.maximumMarkers = number(IDC_MAX_MARKERS, s.maximumMarkers);
	s.useWordAtCaret = checked(IDC_WORD_AT_CARET);
	s.markLargeFiles = checked(IDC_LARGE_FILES);

	for (const LayerRow& row : layerRows()) {
		if (row.enabledBox)
			row.layer->enabled = checked(row.enabledBox);
		row.layer->position = static_cast<Position>(comboSelection(control(row.positionCombo), static_cast<int>(row.layer->position)));
		row.layer->widthPercent = number(row.widthEdit, row.layer->widthPercent);
	}
	wchar_t list[128] = {};
	::GetWindowTextW(control(IDC_OTHER_LIST), list, static_cast<int>(std::size(list)));
	s.otherIndicatorList = list;
	s.caretLineThickness = number(IDC_CARET_THICKNESS, s.caretLineThickness);

	for (const ColorSlot& slot : colorSlots())
		slot.choice->automatic = checked(slot.autoBox);

	s.moveCaretOnClick = checked(IDC_MOVE_CARET);
	s.centerOnClick = checked(IDC_CENTER_ON_CLICK);
	s.flashLine = checked(IDC_FLASH);
	s.flashDuration = number(IDC_FLASH_DURATION, s.flashDuration);
	s.scrollOnEmptyClick = checked(IDC_SCROLL_ON_EMPTY);
	s.wrapAround = checked(IDC_WRAP);
	s.centerOnJump = checked(IDC_CENTER_ON_JUMP);

	s.previewEnabled = checked(IDC_PREVIEW);
	s.previewContextLines = number(IDC_CONTEXT_LINES, s.previewContextLines);
	s.previewDelay.automatic = checked(IDC_PREVIEW_DELAY_AUTO);
	if (!s.previewDelay.automatic)
		s.previewDelay.value = number(IDC_PREVIEW_DELAY, automaticPreviewDelay());
	s.previewWidthPercent = number(IDC_PREVIEW_WIDTH, s.previewWidthPercent);
}

void SettingsDialog::updateControls() {
	const bool wasLoading = m_loading;
	m_loading = true;
	const Settings& s = m_settings;

	// Rows of disabled kinds of markers
	for (const LayerRow& row : layerRows()) {
		const bool on = row.enabledBox ? checked(row.enabledBox) : checked(IDC_OCC_ENABLED);
		enable(row.positionCombo, on);
		enable(row.widthEdit, on);
	}
	const bool occurrences = checked(IDC_OCC_ENABLED);
	for (int id : { IDC_FOLLOW_SMART, IDC_MATCH_CASE, IDC_WHOLE_WORD, IDC_MIN_LENGTH, IDC_MAX_MARKERS, IDC_WORD_AT_CARET, IDC_LARGE_FILES,
			IDC_OCC_COLOR_AUTO, IDC_CUR_COLOR_AUTO })
		enable(id, occurrences);
	enable(IDC_HIST_COLOR_AUTO, checked(IDC_HIST_ENABLED));
	enable(IDC_BOOK_COLOR_AUTO, checked(IDC_BOOK_ENABLED));
	enable(IDC_FIND_COLOR_AUTO, checked(IDC_FIND_ENABLED));
	enable(IDC_TOKEN_COLOR_AUTO, checked(IDC_TOKEN_ENABLED));
	enable(IDC_OTHER_COLOR_AUTO, checked(IDC_OTHER_ENABLED));
	enable(IDC_OTHER_LIST, checked(IDC_OTHER_ENABLED));
	for (int id : { IDC_CARET_COLOR_AUTO, IDC_CARET_THICKNESS })
		enable(id, checked(IDC_CARET_ENABLED));
	for (int id : { IDC_FLASH_DURATION, IDC_FLASH_COLOR_AUTO })
		enable(id, checked(IDC_FLASH));

	// A color can be picked when it is not automatic and its row is on
	for (const ColorSlot& slot : colorSlots()) {
		enable(slot.button, !checked(slot.autoBox) && ::IsWindowEnabled(control(slot.autoBox)));
		::InvalidateRect(control(slot.button), nullptr, TRUE);
	}

	// Automatic numbers are shown but cannot be edited
	if (s.barWidth.automatic)
		setNumber(IDC_BAR_WIDTH, automaticBarWidth());
	enable(IDC_BAR_WIDTH, !s.barWidth.automatic);
	const bool preview = checked(IDC_PREVIEW);
	enable(IDC_CONTEXT_LINES, preview);
	enable(IDC_PREVIEW_DELAY_AUTO, preview);
	enable(IDC_PREVIEW_WIDTH, preview);
	if (s.previewDelay.automatic)
		setNumber(IDC_PREVIEW_DELAY, automaticPreviewDelay());
	enable(IDC_PREVIEW_DELAY, preview && !s.previewDelay.automatic);

	// Whole word off here but on in Notepad++: the editor would highlight less than the bar shows
	const NppPreferences& notepad = m_plugin.preferences();
	const bool disagree = occurrences && notepad.wholeWord && s.wholeWord == Choice::Off;
	if (HWND warning = control(IDC_T_WHOLE_WORD_WARNING)) {
		::SetWindowTextW(warning, tr(notepad.followsFindDialog ? Text::WholeWordWarningFind : Text::WholeWordWarning));
		::ShowWindow(warning, disagree ? SW_SHOW : SW_HIDE);
	}

	::InvalidateRect(control(IDC_SAMPLE), nullptr, FALSE);
	m_loading = wasLoading;
}

void SettingsDialog::changed() {
	if (m_loading)
		return;
	store();
	// The list of other indicators decides their automatic colors
	m_palette = m_plugin.palette(m_editor, m_settings);
	updateControls();
}

// ---------------------------------------------------------------------------
// Colors

void SettingsDialog::colorAutoClicked(int autoBox) {
	const bool automatic = checked(autoBox);
	for (const ColorSlot& slot : colorSlots()) {
		if (slot.autoBox != autoBox)
			continue;
		// A fixed color starts from the color that was shown automatically
		if (!automatic && slot.choice->automatic)
			slot.choice->color = slot.automatic;
		slot.choice->automatic = automatic;
	}
	changed();
}

void SettingsDialog::pickColor(int button) {
	for (const ColorSlot& slot : colorSlots()) {
		if (slot.button != button)
			continue;
		CHOOSECOLORW chooser{ sizeof(CHOOSECOLORW) };
		chooser.hwndOwner = m_hwnd;
		chooser.rgbResult = slot.choice->automatic ? slot.automatic : slot.choice->color;
		chooser.lpCustColors = m_customColors;
		chooser.Flags = CC_FULLOPEN | CC_RGBINIT;
		if (::ChooseColorW(&chooser)) {
			slot.choice->color = chooser.rgbResult;
			slot.choice->automatic = false;
			changed();
		}
		return;
	}
}

void SettingsDialog::drawColorButton(const DRAWITEMSTRUCT& item) {
	COLORREF color = RGB(0, 0, 0);
	bool found = false;
	for (const ColorSlot& slot : colorSlots()) {
		if (slot.button == static_cast<int>(item.CtlID)) {
			color = slot.choice->automatic ? slot.automatic : slot.choice->color;
			found = true;
			break;
		}
	}
	if (!found)
		return;

	RECT rect = item.rcItem;
	const bool disabled = (item.itemState & ODS_DISABLED) != 0;
	::DrawEdge(item.hDC, &rect, (item.itemState & ODS_SELECTED) ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT | BF_ADJUST);
	::InflateRect(&rect, -1, -1);
	if (disabled) {
		// Automatic colors are shown inside a gray frame: visible, not editable
		HBRUSH frame = ::CreateSolidBrush(::GetSysColor(COLOR_GRAYTEXT));
		::FrameRect(item.hDC, &rect, frame);
		::DeleteObject(frame);
		::InflateRect(&rect, -2, -2);
	}
	HBRUSH brush = ::CreateSolidBrush(color);
	::FillRect(item.hDC, &rect, brush);
	::DeleteObject(brush);
	if (item.itemState & ODS_FOCUS) {
		RECT focus = item.rcItem;
		::InflateRect(&focus, -1, -1);
		::DrawFocusRect(item.hDC, &focus);
	}
}

// ---------------------------------------------------------------------------
// Sample: a small editor with a scrollbar and the bar, drawn with the current settings

void SettingsDialog::drawSample(const DRAWITEMSTRUCT& item) {
	const RECT area = item.rcItem;
	const int width = area.right - area.left;
	const int height = area.bottom - area.top;
	if (width <= 0 || height <= 0)
		return;
	const UINT dpi = Dpi::forWindow(m_hwnd);
	HDC dc = item.hDC;

	const COLORREF editorBack = static_cast<COLORREF>(m_editor.call(SCI_STYLEGETBACK, STYLE_DEFAULT));
	const COLORREF editorFore = static_cast<COLORREF>(m_editor.call(SCI_STYLEGETFORE, STYLE_DEFAULT));
	const BarStyle style = resolveBarStyle(m_settings, m_palette);

	const int scrollbarWidth = Dpi::systemMetric(SM_CXVSCROLL, dpi);
	int barWidth = m_settings.barWidth.automatic ? scrollbarWidth : Dpi::scale(m_settings.barWidth.value, dpi);
	barWidth = std::clamp(barWidth, 2, width / 3);
	const int barLeft = area.right - barWidth;
	const int scrollbarLeft = barLeft - scrollbarWidth;

	// Text area with a few lines of "text"
	HBRUSH back = ::CreateSolidBrush(editorBack);
	const RECT text{ area.left, area.top, scrollbarLeft, area.bottom };
	::FillRect(dc, &text, back);
	::DeleteObject(back);
	HBRUSH ink = ::CreateSolidBrush(blend(editorBack, editorFore, 35));
	const int lineHeight = Dpi::scale(7, dpi);
	static const int lengths[] = { 60, 82, 45, 70, 30, 90, 55, 75, 40, 65, 85, 50 };
	for (int y = area.top + Dpi::scale(4, dpi), i = 0; y + Dpi::scale(3, dpi) < area.bottom; y += lineHeight, ++i) {
		const int length = (scrollbarLeft - area.left - Dpi::scale(8, dpi)) * lengths[i % 12] / 100;
		const RECT line{ area.left + Dpi::scale(4, dpi), y, area.left + Dpi::scale(4, dpi) + length, y + Dpi::scale(3, dpi) };
		::FillRect(dc, &line, ink);
	}
	::DeleteObject(ink);

	// Scrollbar with the thumb around the caret line
	constexpr int kLines = 100;
	const double scale = static_cast<double>(height) / kLines;
	HBRUSH track = ::CreateSolidBrush(style.background);
	const RECT scrollbar{ scrollbarLeft, area.top, barLeft, area.bottom };
	::FillRect(dc, &scrollbar, track);
	::DeleteObject(track);
	HBRUSH thumbBrush = ::CreateSolidBrush(blend(style.background, RGB(128, 128, 128), 60));
	const RECT thumb{ scrollbarLeft + 2, area.top + static_cast<int>(26 * scale), barLeft - 2, area.top + static_cast<int>(42 * scale) };
	::FillRect(dc, &thumb, thumbBrush);
	::DeleteObject(thumbBrush);

	// Marks that overlap on purpose, to show the priorities
	LayerRows rows;
	for (auto& layer : rows)
		layer.assign(static_cast<size_t>(height), 0);
	const int markerHeight = std::max(Dpi::scale(m_settings.minimumMarkerHeight, dpi), static_cast<int>(scale + 0.5));
	auto mark = [&](Layer layer, int line, unsigned char value, int rowsHigh) {
		const int top = static_cast<int>(line * scale);
		for (int y = std::max(0, top); y < std::min(height, top + rowsHigh); ++y) {
			if (!rows[layer][static_cast<size_t>(y)])
				rows[layer][static_cast<size_t>(y)] = value;
		}
	};
	for (int line : { 6, 7, 52 })
		mark(LayerChangeHistory, line, 1, markerHeight);
	for (int line : { 30, 31 })
		mark(LayerChangeHistory, line, 2, markerHeight);
	mark(LayerChangeHistory, 80, 3, markerHeight);
	mark(LayerChangeHistory, 90, 4, markerHeight);
	for (int line : { 18, 62 })
		mark(LayerBookmarks, line, 1, markerHeight);
	mark(LayerOtherIndicators, 44, 1, markerHeight);
	mark(LayerStyleTokens, 12, 1, markerHeight);
	mark(LayerStyleTokens, 44, 2, markerHeight);
	mark(LayerStyleTokens, 70, 3, markerHeight);
	for (int line : { 12, 76 })
		mark(LayerFindMarks, line, 1, markerHeight);
	for (int line : { 12, 37, 62, 88 })
		mark(LayerOccurrences, line, 1, markerHeight);
	mark(LayerCurrent, 37, 1, markerHeight);
	const int thickness = Dpi::scale(m_settings.caretLineThickness, dpi);
	{
		const int top = static_cast<int>(33 * scale) + (markerHeight - thickness) / 2;
		for (int y = std::max(0, top); y < std::min(height, top + thickness); ++y)
			rows[LayerCaretLine][static_cast<size_t>(y)] = 1;
	}

	BarStyle shown = style;
	if (!m_settings.enabled) {
		for (LayerStyle& layer : shown.layers)
			layer.enabled = false;
	}
	paintBar(dc, barLeft, area.top, barWidth, height, shown, rows, 0);

	HBRUSH frame = ::CreateSolidBrush(::GetSysColor(COLOR_BTNSHADOW));
	::FrameRect(dc, &area, frame);
	::DeleteObject(frame);
}
