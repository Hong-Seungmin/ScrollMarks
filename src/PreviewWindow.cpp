// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "PreviewWindow.h"

#include <algorithm>
#include <cwchar>
#include <string>

#include "Dpi.h"

namespace {

constexpr wchar_t kWindowClass[] = L"ScrollMarksPreview";
constexpr int kLastIndicator = 35;   // INDICATOR_MAX

COLORREF blend(COLORREF from, COLORREF to, int percent) {
	auto mix = [percent](int a, int b) { return a + (b - a) * percent / 100; };
	return RGB(mix(GetRValue(from), GetRValue(to)), mix(GetGValue(from), GetGValue(to)), mix(GetBValue(from), GetBValue(to)));
}

std::wstring withSeparators(size_t value) {
	std::wstring digits = std::to_wstring(value);
	for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
		digits.insert(static_cast<size_t>(i), L",");
	return digits;
}

} // namespace

bool PreviewWindow::create(HINSTANCE module, HWND owner) {
	m_module = module;

	WNDCLASSEXW windowClass{ sizeof(WNDCLASSEXW) };
	windowClass.style = CS_DROPSHADOW;
	windowClass.lpfnWndProc = &PreviewWindow::windowProc;
	windowClass.hInstance = module;
	windowClass.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
	windowClass.lpszClassName = kWindowClass;
	::RegisterClassExW(&windowClass);

	m_window = ::CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kWindowClass, L"", WS_POPUP | WS_BORDER,
		0, 0, 10, 10, owner, nullptr, module, this);
	if (!m_window)
		return false;

	// Notepad++ registers the Scintilla window class for the whole process
	m_viewWindow = ::CreateWindowExW(0, L"Scintilla", L"", WS_CHILD | WS_VISIBLE | WS_DISABLED,
		0, 0, 10, 10, m_window, nullptr, module, nullptr);
	if (!m_viewWindow) {
		destroy();
		return false;
	}

	m_view.attach(m_viewWindow);
	m_view.call(SCI_SETVSCROLLBAR, 0);
	m_view.call(SCI_SETHSCROLLBAR, 0);
	m_view.call(SCI_SETCARETSTYLE, CARETSTYLE_INVISIBLE);
	m_view.call(SCI_SETCARETLINEVISIBLEALWAYS, 1);
	m_view.call(SCI_SETWRAPMODE, SC_WRAP_NONE);
	m_view.call(SCI_USEPOPUP, SC_POPUP_NEVER);
	m_view.call(SCI_SETMOUSEDWELLTIME, SC_TIME_FOREVER);
	m_view.call(SCI_SETENDATLASTLINE, 0);   // lets the last lines of a file be centered too
	m_view.call(SCI_SETLAYOUTCACHE, SC_CACHE_PAGE);
	m_view.call(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
	for (int margin = 1; margin < 5; ++margin)
		m_view.call(SCI_SETMARGINWIDTHN, static_cast<uptr_t>(margin), 0);
	return true;
}

void PreviewWindow::destroy() {
	hide();
	if (m_window)
		::DestroyWindow(m_window);   // also destroys the Scintilla view
	m_window = nullptr;
	m_viewWindow = nullptr;
	m_view.attach(nullptr);
	if (m_font)
		::DeleteObject(m_font);
	m_font = nullptr;
	if (m_module)
		::UnregisterClassW(kWindowClass, m_module);
}

void PreviewWindow::hide() {
	if (!m_window)
		return;
	if (m_visible)
		::ShowWindow(m_window, SW_HIDE);
	m_visible = false;
	if (m_document) {
		// Give the document back so closing it in Notepad++ really frees it
		m_view.call(SCI_SETDOCPOINTER, 0, 0);
		m_document = 0;
	}
	m_source = nullptr;
}

void PreviewWindow::updateFont(UINT dpi) {
	if (m_font && m_fontDpi == dpi)
		return;
	if (m_font)
		::DeleteObject(m_font);

	NONCLIENTMETRICSW metrics{ sizeof(NONCLIENTMETRICSW) };
	using SystemParametersInfoForDpiFn = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
	static const auto systemParametersInfoForDpi = reinterpret_cast<SystemParametersInfoForDpiFn>(
		::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi"));
	if (!(systemParametersInfoForDpi && systemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi)))
		::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
	m_font = ::CreateFontIndirectW(&metrics.lfStatusFont);
	m_fontDpi = dpi;

	HDC dc = ::GetDC(m_window);
	HGDIOBJ old = ::SelectObject(dc, m_font);
	TEXTMETRICW text{};
	::GetTextMetricsW(dc, &text);
	::SelectObject(dc, old);
	::ReleaseDC(m_window, dc);
	m_captionHeight = text.tmHeight + Dpi::scale(6, dpi);
}

void PreviewWindow::copyAppearance(const Editor& source) {
	// Styles, zoom and indicators belong to the view, not to the document
	for (int style = 0; style <= STYLE_MAX; ++style) {
		const uptr_t s = static_cast<uptr_t>(style);
		m_view.call(SCI_STYLESETFORE, s, source.call(SCI_STYLEGETFORE, s));
		m_view.call(SCI_STYLESETBACK, s, source.call(SCI_STYLEGETBACK, s));
		m_view.call(SCI_STYLESETWEIGHT, s, source.call(SCI_STYLEGETWEIGHT, s));
		m_view.call(SCI_STYLESETITALIC, s, source.call(SCI_STYLEGETITALIC, s));
		m_view.call(SCI_STYLESETUNDERLINE, s, source.call(SCI_STYLEGETUNDERLINE, s));
		m_view.call(SCI_STYLESETSIZEFRACTIONAL, s, source.call(SCI_STYLEGETSIZEFRACTIONAL, s));
		m_view.call(SCI_STYLESETEOLFILLED, s, source.call(SCI_STYLEGETEOLFILLED, s));
		m_view.call(SCI_STYLESETCASE, s, source.call(SCI_STYLEGETCASE, s));
		m_view.call(SCI_STYLESETCHARACTERSET, s, source.call(SCI_STYLEGETCHARACTERSET, s));
		m_view.call(SCI_STYLESETVISIBLE, s, source.call(SCI_STYLEGETVISIBLE, s));
		char font[128] = {};
		if (source.call(SCI_STYLEGETFONT, s, font) > 0 && font[0])
			m_view.call(SCI_STYLESETFONT, s, font);
	}

	for (int indicator = 0; indicator <= kLastIndicator; ++indicator) {
		const uptr_t i = static_cast<uptr_t>(indicator);
		m_view.call(SCI_INDICSETSTYLE, i, source.call(SCI_INDICGETSTYLE, i));
		m_view.call(SCI_INDICSETFORE, i, source.call(SCI_INDICGETFORE, i));
		m_view.call(SCI_INDICSETALPHA, i, source.call(SCI_INDICGETALPHA, i));
		m_view.call(SCI_INDICSETOUTLINEALPHA, i, source.call(SCI_INDICGETOUTLINEALPHA, i));
		m_view.call(SCI_INDICSETUNDER, i, source.call(SCI_INDICGETUNDER, i));
	}

	m_view.call(SCI_SETTECHNOLOGY, static_cast<uptr_t>(source.call(SCI_GETTECHNOLOGY)));
	m_view.call(SCI_SETFONTQUALITY, static_cast<uptr_t>(source.call(SCI_GETFONTQUALITY)));
	m_view.call(SCI_SETZOOM, static_cast<uptr_t>(source.call(SCI_GETZOOM)));
	m_view.call(SCI_SETVIEWWS, static_cast<uptr_t>(source.call(SCI_GETVIEWWS)));
	m_view.call(SCI_SETINDENTATIONGUIDES, static_cast<uptr_t>(source.call(SCI_GETINDENTATIONGUIDES)));
	m_view.call(SCI_SETSELECTIONLAYER, static_cast<uptr_t>(source.call(SCI_GETSELECTIONLAYER)));
	m_view.call(SCI_SETCARETLINELAYER, static_cast<uptr_t>(source.call(SCI_GETCARETLINELAYER)));

	// The preview never has the focus, so show the selection with the
	// editor's active colors.
	auto copyElement = [&](int from, int to) {
		if (source.call(SCI_GETELEMENTISSET, static_cast<uptr_t>(from)))
			m_view.call(SCI_SETELEMENTCOLOUR, static_cast<uptr_t>(to), source.call(SCI_GETELEMENTCOLOUR, static_cast<uptr_t>(from)));
		else
			m_view.call(SCI_RESETELEMENTCOLOUR, static_cast<uptr_t>(to));
	};
	copyElement(SC_ELEMENT_SELECTION_TEXT, SC_ELEMENT_SELECTION_TEXT);
	copyElement(SC_ELEMENT_SELECTION_BACK, SC_ELEMENT_SELECTION_BACK);
	copyElement(SC_ELEMENT_SELECTION_TEXT, SC_ELEMENT_SELECTION_INACTIVE_TEXT);
	copyElement(SC_ELEMENT_SELECTION_BACK, SC_ELEMENT_SELECTION_INACTIVE_BACK);
	copyElement(SC_ELEMENT_WHITE_SPACE, SC_ELEMENT_WHITE_SPACE);

	// Highlight the line of the occurrence, even when the editor does not
	// highlight its current line.
	const COLORREF back = static_cast<COLORREF>(source.call(SCI_STYLEGETBACK, STYLE_DEFAULT));
	const COLORREF fore = static_cast<COLORREF>(source.call(SCI_STYLEGETFORE, STYLE_DEFAULT));
	if (source.call(SCI_GETELEMENTISSET, SC_ELEMENT_CARET_LINE_BACK))
		m_view.call(SCI_SETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK, source.call(SCI_GETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK));
	else
		m_view.call(SCI_SETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK, static_cast<sptr_t>(blend(back, fore, 12)) | 0xFF000000);

	m_captionBack = static_cast<COLORREF>(source.call(SCI_STYLEGETBACK, STYLE_LINENUMBER));
	m_captionText = static_cast<COLORREF>(source.call(SCI_STYLEGETFORE, STYLE_LINENUMBER));
}

void PreviewWindow::show(const Editor& source, const Request& request) {
	if (!m_window || !source.hwnd())
		return;

	const sptr_t document = source.document();
	if (document != m_document) {
		m_view.call(SCI_SETDOCPOINTER, 0, document);   // adds a reference to the document
		m_document = document;
	}
	const sptr_t lexer = source.call(SCI_GETLEXER);
	if (!m_appearanceValid || m_source != source.hwnd() || lexer != m_lexer) {
		copyAppearance(source);
		m_lexer = lexer;
		m_appearanceValid = true;
	}
	m_source = source.hwnd();

	const UINT dpi = Dpi::forWindow(source.hwnd());
	updateFont(dpi);

	// Line numbers wide enough for the last line of the document
	const std::string widest = "_" + std::to_string(std::max<Sci_Position>(10, source.lineCount()));
	const int numberWidth = static_cast<int>(m_view.call(SCI_TEXTWIDTH, STYLE_LINENUMBER, widest.c_str())) + Dpi::scale(4, dpi);
	m_view.call(SCI_SETMARGINWIDTHN, 0, numberWidth);

	// Size: the lines around the occurrence plus the caption
	const int lines = 2 * request.contextLines + 1;
	const int lineHeight = static_cast<int>(m_view.call(SCI_TEXTHEIGHT, 0));
	RECT editorRect{};
	::GetWindowRect(source.hwnd(), &editorRect);
	const int border = 2 * ::GetSystemMetrics(SM_CXBORDER);
	int width = std::max(Dpi::scale(320, dpi), static_cast<int>((editorRect.right - editorRect.left) * static_cast<long long>(request.widthPercent) / 100));
	const int height = lines * lineHeight + m_captionHeight + border;

	// Left of the marker bar, centered on the mouse, inside the monitor
	MONITORINFO monitor{ sizeof(MONITORINFO) };
	::GetMonitorInfoW(::MonitorFromPoint(request.anchor, MONITOR_DEFAULTTONEAREST), &monitor);
	const RECT& work = monitor.rcWork;
	width = std::min<int>(width, work.right - work.left);
	const int gap = Dpi::scale(6, dpi);
	int x = request.anchor.x - width - gap;
	if (x < work.left)
		x = request.anchor.x + gap;
	x = std::clamp<int>(x, work.left, std::max<int>(work.left, work.right - width));
	int y = request.anchor.y - height / 2;
	y = std::clamp<int>(y, work.top, std::max<int>(work.top, work.bottom - height));

	::SetWindowPos(m_window, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_NOREDRAW);
	RECT client{};
	::GetClientRect(m_window, &client);
	::MoveWindow(m_viewWindow, 0, m_captionHeight, client.right, std::max<int>(0, client.bottom - m_captionHeight), FALSE);

	// Content: the preview view has no folds and no wrapping, so a document
	// line is a display line.
	const Occurrence& occurrence = request.occurrence;
	const Sci_Position line = m_view.lineFromPosition(occurrence.start);
	m_view.call(SCI_SETSEL, static_cast<uptr_t>(occurrence.start), occurrence.end);
	m_view.call(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(std::max<Sci_Position>(0, line - request.contextLines)));
	m_view.call(SCI_SETXOFFSET, 0);
	const int textRight = client.right;
	const int matchRight = static_cast<int>(m_view.call(SCI_POINTXFROMPOSITION, 0, occurrence.end));
	if (matchRight > textRight - Dpi::scale(16, dpi)) {
		const int matchLeft = static_cast<int>(m_view.call(SCI_POINTXFROMPOSITION, 0, occurrence.start));
		m_view.call(SCI_SETXOFFSET, static_cast<uptr_t>(std::max(0, matchLeft - numberWidth - (textRight - numberWidth) / 3)));
	}

	m_caption = L"Line " + withSeparators(static_cast<size_t>(line) + 1) + L"    " + withSeparators(request.number) +
		L" of " + withSeparators(request.total) + (request.totalIsLimited ? L"+" : L"");

	::InvalidateRect(m_window, nullptr, TRUE);
	::InvalidateRect(m_viewWindow, nullptr, FALSE);
	if (!m_visible) {
		::ShowWindow(m_window, SW_SHOWNOACTIVATE);
		m_visible = true;
	}
	::RedrawWindow(m_window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void PreviewWindow::paintCaption(HDC dc) const {
	RECT client{};
	::GetClientRect(m_window, &client);
	RECT caption{ 0, 0, client.right, m_captionHeight };
	HBRUSH brush = ::CreateSolidBrush(m_captionBack);
	::FillRect(dc, &caption, brush);
	::DeleteObject(brush);

	HGDIOBJ old = ::SelectObject(dc, m_font);
	::SetBkMode(dc, TRANSPARENT);
	::SetTextColor(dc, m_captionText);
	caption.left += Dpi::scale(6, m_fontDpi);
	::DrawTextW(dc, m_caption.c_str(), static_cast<int>(m_caption.size()), &caption, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
	::SelectObject(dc, old);
}

LRESULT CALLBACK PreviewWindow::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_NCCREATE) {
		auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
		::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
	}
	auto* self = reinterpret_cast<PreviewWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));

	switch (message) {
	case WM_NCHITTEST:
		return HTTRANSPARENT;   // the mouse goes to whatever is below
	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT: {
		PAINTSTRUCT paint{};
		HDC dc = ::BeginPaint(hwnd, &paint);
		if (self)
			self->paintCaption(dc);
		::EndPaint(hwnd, &paint);
		return 0;
	}
	case WM_NOTIFY:
	case WM_COMMAND:
		return 0;   // notifications of the preview view
	}
	return ::DefWindowProcW(hwnd, message, wParam, lParam);
}
