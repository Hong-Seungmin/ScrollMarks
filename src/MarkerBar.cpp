// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "MarkerBar.h"

#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>

#include "Dpi.h"
#include "Plugin.h"

namespace {

constexpr UINT_PTR kSubclassId = 0x534D4252;   // "SMBR"
constexpr Sci_Position kMaximumTextBytes = 1024;
constexpr UINT kSelectionDelay = 15;           // coalesces bursts of selection changes
constexpr UINT kEditDelay = 250;               // waits for a pause in typing
constexpr UINT kSearchInterval = 1;            // next slice as soon as the message queue is idle
constexpr double kSearchBudget = 6.0;          // milliseconds of searching per slice

} // namespace

bool MarkerBar::Layout::operator==(const Layout& other) const {
	return EqualRect(&bar, &other.bar) && trackTop == other.trackTop && trackLength == other.trackLength &&
		thumbLength == other.thumbLength && rangeMin == other.rangeMin && rangeMax == other.rangeMax && page == other.page;
}

// ---------------------------------------------------------------------------
// Lifetime

void MarkerBar::attach(Plugin& plugin, HWND scintilla, int index) {
	if (!scintilla)
		return;
	m_plugin = &plugin;
	m_index = index;
	m_editor.attach(scintilla);
	m_document = m_editor.document();
	::SetWindowSubclass(scintilla, &MarkerBar::subclassProc, kSubclassId, reinterpret_cast<DWORD_PTR>(this));
	refreshFrame();
}

void MarkerBar::detach() {
	if (!attached())
		return;
	stopTimers();
	m_plugin->preview().hideFor(hwnd());
	const HWND scintilla = hwnd();
	::RemoveWindowSubclass(scintilla, &MarkerBar::subclassProc, kSubclassId);
	m_editor.attach(nullptr);
	::SetWindowPos(scintilla, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	m_search.reset();
	m_lines.clear();
	m_rows.clear();
	m_layoutValid = false;
}

void MarkerBar::refreshFrame() {
	m_layoutValid = false;
	if (attached())
		::SetWindowPos(hwnd(), nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool MarkerBar::shown() const {
	return attached() && m_plugin->settings().enabled;
}

bool MarkerBar::active() const {
	const Settings& settings = m_plugin->settings();
	const NppPreferences& preferences = m_plugin->preferences();
	if (!settings.enabled)
		return false;
	if (settings.followSmartHighlighting && !preferences.smartHighlighting)
		return false;
	if (!settings.markLargeFiles && preferences.largeFileRestriction && !preferences.largeFileAllowsSmartHighlighting &&
		m_editor.length() >= preferences.largeFileBytes)
		return false;
	return true;
}

UINT MarkerBar::dpi() const {
	return Dpi::forWindow(hwnd());
}

int MarkerBar::barWidth() const {
	const UINT currentDpi = dpi();
	const NumberChoice& width = m_plugin->settings().barWidth;
	const int pixels = width.automatic ? Dpi::systemMetric(SM_CXVSCROLL, currentDpi) / 2 : Dpi::scale(width.value, currentDpi);
	return std::clamp(pixels, 2, Dpi::scale(64, currentDpi));
}

// ---------------------------------------------------------------------------
// Geometry

RECT MarkerBar::barRect() const {
	// WM_NCCALCSIZE takes the width of the bar from the client area. Windows
	// then puts the vertical scrollbar right next to the smaller client area,
	// so the reserved strip ends up on the right of the scrollbar.
	RECT window{};
	RECT client{};
	::GetWindowRect(hwnd(), &window);
	::GetClientRect(hwnd(), &client);
	POINT origin{ 0, 0 };
	::ClientToScreen(hwnd(), &origin);
	const LONG_PTR style = ::GetWindowLongPtrW(hwnd(), GWL_STYLE);

	int left = (origin.x - window.left) + client.right;
	SCROLLBARINFO vertical{ sizeof(SCROLLBARINFO) };
	if ((style & WS_VSCROLL) && ::GetScrollBarInfo(hwnd(), OBJID_VSCROLL, &vertical) &&
		!(vertical.rgstate[0] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN)))
		left = vertical.rcScrollBar.right - window.left;

	// Reach down next to the horizontal scrollbar too, so no gap is left
	const int top = origin.y - window.top;
	int bottom = top + client.bottom;
	SCROLLBARINFO horizontal{ sizeof(SCROLLBARINFO) };
	if ((style & WS_HSCROLL) && ::GetScrollBarInfo(hwnd(), OBJID_HSCROLL, &horizontal) &&
		!(horizontal.rgstate[0] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN)))
		bottom = horizontal.rcScrollBar.bottom - window.top;

	return RECT{ left, top, left + barWidth(), bottom };
}

bool MarkerBar::barContains(POINT screen) const {
	RECT window{};
	::GetWindowRect(hwnd(), &window);
	const RECT bar = barRect();
	const POINT point{ screen.x - window.left, screen.y - window.top };
	return ::PtInRect(&bar, point) != FALSE;
}

MarkerBar::Layout MarkerBar::computeLayout() const {
	Layout layout;
	layout.bar = barRect();

	RECT window{};
	::GetWindowRect(hwnd(), &window);

	SCROLLBARINFO scrollbar{ sizeof(SCROLLBARINFO) };
	const bool hasScrollbar = (::GetWindowLongPtrW(hwnd(), GWL_STYLE) & WS_VSCROLL) &&
		::GetScrollBarInfo(hwnd(), OBJID_VSCROLL, &scrollbar) &&
		!(scrollbar.rgstate[0] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN));

	if (hasScrollbar) {
		// The track is the scrollbar without its arrow buttons
		layout.trackTop = (scrollbar.rcScrollBar.top - window.top) + scrollbar.dxyLineButton;
		layout.trackLength = (scrollbar.rcScrollBar.bottom - scrollbar.rcScrollBar.top) - 2 * scrollbar.dxyLineButton;
		if (!(scrollbar.rgstate[0] & STATE_SYSTEM_UNAVAILABLE))
			layout.thumbLength = scrollbar.xyThumbBottom - scrollbar.xyThumbTop;
	} else {
		layout.trackTop = layout.bar.top;
		layout.trackLength = layout.bar.bottom - layout.bar.top;
	}

	SCROLLINFO info{ sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE };
	if (hasScrollbar && ::GetScrollInfo(hwnd(), SB_VERT, &info)) {
		layout.rangeMin = info.nMin;
		layout.rangeMax = info.nMax;
		layout.page = static_cast<int>(info.nPage);
	} else {
		// Same numbers Scintilla gives the scrollbar
		const Sci_Position lines = m_editor.lineCount();
		const Sci_Position onScreen = m_editor.call(SCI_LINESONSCREEN);
		Sci_Position displayed = lines > 0 ? m_editor.displayLineFromDocLine(lines - 1) + m_editor.call(SCI_WRAPCOUNT, lines - 1) : 1;
		if (!m_editor.call(SCI_GETENDATLASTLINE))
			displayed += onScreen - 1;
		layout.rangeMin = 0;
		layout.rangeMax = static_cast<int>(std::max<Sci_Position>(displayed, onScreen) - 1);
		layout.page = static_cast<int>(onScreen);
	}
	layout.trackLength = std::max(layout.trackLength, 0);
	return layout;
}

double MarkerBar::pixelsPerLine() const {
	// Windows places the thumb top at trackTop + position * (track - thumb) / (count - page),
	// where position is the first display line on screen.
	const Layout& layout = m_layout;
	const double count = static_cast<double>(layout.rangeMax) - layout.rangeMin + 1;
	const double page = std::max(1, layout.page);
	if (layout.trackLength <= 0 || count <= 0)
		return 0;
	if (layout.thumbLength > 0 && count > page)
		return (layout.trackLength - layout.thumbLength) / (count - page);
	// Nothing to scroll: lines sit where they are on screen
	return layout.trackLength / std::max(count, page);
}

int MarkerBar::markerHeight() const {
	// One display line on the track, but never thinner than the minimum
	const int minimum = Dpi::scale(m_plugin->settings().minimumMarkerHeight, dpi());
	return std::max(minimum, static_cast<int>(std::lround(m_scale)));
}

// ---------------------------------------------------------------------------
// Window messages

LRESULT CALLBACK MarkerBar::subclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data) {
	auto* self = reinterpret_cast<MarkerBar*>(data);

	switch (message) {
	case WM_NCCALCSIZE: {
		const LRESULT result = ::DefSubclassProc(hwnd, message, wParam, lParam);
		if (self->shown()) {
			// For both values of wParam the first RECT is the client rectangle
			RECT* client = reinterpret_cast<RECT*>(lParam);
			const int width = self->barWidth();
			if (client->right - client->left > 2 * width)
				client->right -= width;
		}
		self->m_layoutValid = false;
		return result;
	}

	case WM_NCPAINT: {
		const LRESULT result = ::DefSubclassProc(hwnd, message, wParam, lParam);
		if (self->shown())
			self->paintNow();
		return result;
	}

	case WM_NCHITTEST:
		if (self->shown() && self->barContains(POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }))
			return HTBORDER;
		break;

	case WM_SETCURSOR:
		if (self->shown() && LOWORD(lParam) == HTBORDER) {
			POINT cursor{};
			::GetCursorPos(&cursor);
			if (self->barContains(cursor)) {
				::SetCursor(::LoadCursorW(nullptr, self->occurrenceAt(cursor.y) >= 0 ? IDC_HAND : IDC_ARROW));
				return TRUE;
			}
		}
		break;

	case WM_NCMOUSEMOVE:
		if (self->shown()) {
			const POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if (self->barContains(point)) {
				self->onMouseMove(point);
				return 0;
			}
			self->onMouseLeave();
		}
		break;

	case WM_NCMOUSELEAVE:
		self->m_trackingMouse = false;
		self->onMouseLeave();
		break;

	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONDBLCLK:
		if (self->shown()) {
			const POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			if (self->barContains(point)) {
				self->onClick(point);
				return 0;
			}
		}
		break;

	case WM_PAINT: {
		const LRESULT result = ::DefSubclassProc(hwnd, message, wParam, lParam);
		self->onPainted();
		return result;
	}

	case WM_DPICHANGED_AFTERPARENT: {
		const LRESULT result = ::DefSubclassProc(hwnd, message, wParam, lParam);
		self->refreshFrame();
		return result;
	}

	case WM_NCDESTROY:
		self->stopTimers();
		::RemoveWindowSubclass(hwnd, &MarkerBar::subclassProc, kSubclassId);
		self->m_editor.attach(nullptr);
		break;
	}

	return ::DefSubclassProc(hwnd, message, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Selection and search

void MarkerBar::onSelectionChanged() {
	if (!shown() || m_editPending)
		return;   // a pending edit re-evaluates the selection anyway

	const Sci_Position start = m_editor.call(SCI_GETSELECTIONSTART);
	const Sci_Position end = m_editor.call(SCI_GETSELECTIONEND);
	const Sci_Position caret = m_editor.call(SCI_GETCURRENTPOS);
	const bool caretMatters = start == end && m_plugin->settings().useWordAtCaret;
	if (start == m_selectionStart && end == m_selectionEnd && (!caretMatters || caret == m_caret) && !m_textChanged)
		return;

	m_plugin->setTimer(m_index, EvaluateTimer, kSelectionDelay);
}

void MarkerBar::onTextChanged() {
	m_textChanged = true;
	m_plugin->preview().hideFor(hwnd());
	if (m_search.empty())
		return;   // nothing marked, the next selection change starts a new search
	m_editPending = true;
	m_plugin->setTimer(m_index, EvaluateTimer, kEditDelay);
}

void MarkerBar::onDocumentMaybeSwitched() {
	if (!attached())
		return;
	const sptr_t document = m_editor.document();
	if (document == m_document)
		return;
	m_document = document;
	m_textChanged = false;
	m_editPending = false;
	m_selectionStart = m_selectionEnd = m_caret = -1;
	clear();
	evaluateSelection();
}

void MarkerBar::onSettingsChanged() {
	if (!attached())
		return;
	refreshFrame();
	clear();
	m_selectionStart = m_selectionEnd = m_caret = -1;
	if (shown())
		evaluateSelection();
}

void MarkerBar::onAppearanceChanged() {
	if (shown())
		paintNow();
}

void MarkerBar::onTimer(TimerKind kind) {
	switch (kind) {
	case EvaluateTimer:
		m_plugin->killTimer(m_index, EvaluateTimer);
		m_editPending = false;
		evaluateSelection();
		break;
	case SearchTimer:
		continueSearch();
		break;
	case HoverTimer:
		m_plugin->killTimer(m_index, HoverTimer);
		if (m_hoverIndex >= 0)
			showPreview();
		break;
	}
}

void MarkerBar::stopTimers() {
	if (!m_plugin)
		return;
	m_plugin->killTimer(m_index, EvaluateTimer);
	m_plugin->killTimer(m_index, SearchTimer);
	m_plugin->killTimer(m_index, HoverTimer);
	m_editPending = false;
}

void MarkerBar::evaluateSelection() {
	if (!shown())
		return;

	m_selectionStart = m_editor.call(SCI_GETSELECTIONSTART);
	m_selectionEnd = m_editor.call(SCI_GETSELECTIONEND);
	m_caret = m_editor.call(SCI_GETCURRENTPOS);

	std::string text;
	Sci_Position start = 0;
	if (!active() || !readSearchText(text, start)) {
		clear();
		m_textChanged = false;
		return;
	}

	const int flags = m_plugin->searchFlags();
	if (!m_textChanged && text == m_search.text() && flags == m_search.flags()) {
		setCurrent(start);   // same text, only the selected occurrence moved
		return;
	}
	beginSearch(std::move(text), flags, start);
}

bool MarkerBar::readSearchText(std::string& text, Sci_Position& start) const {
	const Settings& settings = m_plugin->settings();
	if (m_editor.call(SCI_GETSELECTIONS) > 1)
		return false;   // multiple or rectangular selection

	Sci_Position from = m_selectionStart;
	Sci_Position to = m_selectionEnd;
	if (from == to) {
		if (!settings.useWordAtCaret)
			return false;
		from = m_editor.call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(m_caret), 1);
		to = m_editor.call(SCI_WORDENDPOSITION, static_cast<uptr_t>(m_caret), 1);
		if (from == to)
			return false;
	}

	if (to - from > kMaximumTextBytes)
		return false;
	if (m_editor.lineFromPosition(from) != m_editor.lineFromPosition(to))
		return false;

	if (m_plugin->searchFlags() & SCFIND_WHOLEWORD) {
		// Like Notepad++, whole word matching only marks a selection that is one word
		if (!m_editor.call(SCI_ISRANGEWORD, static_cast<uptr_t>(from), to))
			return false;
		if (m_editor.call(SCI_WORDENDPOSITION, static_cast<uptr_t>(from), 1) != to)
			return false;
	}

	if (m_editor.call(SCI_COUNTCHARACTERS, static_cast<uptr_t>(from), to) < settings.minimumLength)
		return false;

	text = m_editor.text(from, to);
	start = from;
	return !text.empty();
}

void MarkerBar::beginSearch(std::string text, int flags, Sci_Position currentStart) {
	m_textChanged = false;
	m_plugin->preview().hideFor(hwnd());
	m_search.begin(std::move(text), flags, static_cast<size_t>(m_plugin->settings().maximumMarkers), m_editor.length());
	m_lines.clear();
	m_lastDocLine = -1;
	m_current = -1;
	m_currentStart = currentStart;
	m_hoverIndex = -1;
	continueSearch();
}

void MarkerBar::continueSearch() {
	const bool finished = m_search.resume(m_editor, kSearchBudget);
	updateCurrentIndex();
	rebuildRows();
	paintNow();
	if (finished)
		m_plugin->killTimer(m_index, SearchTimer);
	else
		m_plugin->setTimer(m_index, SearchTimer, kSearchInterval);
}

void MarkerBar::clear() {
	m_plugin->killTimer(m_index, SearchTimer);
	m_plugin->killTimer(m_index, HoverTimer);
	m_plugin->preview().hideFor(hwnd());
	const bool hadMarkers = !m_search.empty() || !m_lines.empty();
	m_search.reset();
	m_lines.clear();
	m_lastDocLine = -1;
	m_current = -1;
	m_currentStart = -1;
	m_hoverIndex = -1;
	if (hadMarkers) {
		rebuildRows();
		paintNow();
	}
}

void MarkerBar::setCurrent(Sci_Position start) {
	const long long previous = m_current;
	m_currentStart = start;
	updateCurrentIndex();
	if (m_current != previous) {
		rebuildRows();
		paintNow();
	}
}

void MarkerBar::updateCurrentIndex() {
	const auto& occurrences = m_search.occurrences();
	const auto it = std::lower_bound(occurrences.begin(), occurrences.end(), m_currentStart,
		[](const Occurrence& occurrence, Sci_Position position) { return occurrence.start < position; });
	m_current = (it != occurrences.end() && it->start == m_currentStart) ? static_cast<long long>(it - occurrences.begin()) : -1;
}

// ---------------------------------------------------------------------------
// Drawing

void MarkerBar::onPainted() {
	if (!shown())
		return;
	onDocumentMaybeSwitched();

	// Folding, wrapping, zooming and resizing all change the scrollbar.
	// Scrolling only moves the thumb, which is not part of the layout.
	const Layout layout = computeLayout();
	if (m_layoutValid && layout == m_layout)
		return;
	const bool linesMoved = !m_layoutValid || layout.rangeMin != m_layout.rangeMin || layout.rangeMax != m_layout.rangeMax;
	m_layout = layout;
	m_layoutValid = true;
	if (linesMoved) {
		m_lines.clear();
		m_lastDocLine = -1;
	}
	rebuildRows();
	paintNow();
}

void MarkerBar::updateLines() {
	const auto& occurrences = m_search.occurrences();
	if (m_lines.size() > occurrences.size()) {
		m_lines.clear();
		m_lastDocLine = -1;
	}
	m_lines.reserve(occurrences.size());
	for (size_t i = m_lines.size(); i < occurrences.size(); ++i) {
		const Sci_Position line = m_editor.lineFromPosition(occurrences[i].start);
		if (line != m_lastDocLine) {
			m_lastDocLine = line;
			m_lastDisplayLine = m_editor.displayLineFromDocLine(line);
		}
		m_lines.push_back(m_lastDisplayLine);
	}
}

void MarkerBar::rebuildRows() {
	if (!m_layoutValid) {
		m_layout = computeLayout();
		m_layoutValid = true;
	}
	m_scale = pixelsPerLine();
	m_rows.assign(static_cast<size_t>(m_layout.trackLength), RowEmpty);
	if (m_search.occurrences().empty() || m_rows.empty() || m_scale <= 0)
		return;

	updateLines();
	const int height = markerHeight();
	const int length = static_cast<int>(m_rows.size());
	auto mark = [&](size_t index, Row kind) {
		// Rounded like Windows rounds the thumb position
		const int top = static_cast<int>(std::lround(static_cast<double>(m_lines[index]) * m_scale));
		const int bottom = std::min(length, top + height);
		for (int y = std::max(0, top); y < bottom; ++y) {
			if (m_rows[static_cast<size_t>(y)] < kind)
				m_rows[static_cast<size_t>(y)] = kind;
		}
	};
	for (size_t i = 0; i < m_lines.size(); ++i)
		mark(i, RowOccurrence);
	if (m_current >= 0 && static_cast<size_t>(m_current) < m_lines.size())
		mark(static_cast<size_t>(m_current), RowCurrent);
}

void MarkerBar::paintNow() {
	if (!shown())
		return;
	HDC dc = ::GetWindowDC(hwnd());
	if (!dc)
		return;
	paint(dc);
	::ReleaseDC(hwnd(), dc);
}

void MarkerBar::paint(HDC dc) {
	if (!m_layoutValid)
		rebuildRows();

	const RECT& bar = m_layout.bar;
	const int width = bar.right - bar.left;
	const int height = bar.bottom - bar.top;
	if (width <= 0 || height <= 0)
		return;

	const BarColors colors = m_plugin->barColors(m_editor);

	// Draw off screen and copy in one go to avoid flicker
	HDC memory = ::CreateCompatibleDC(dc);
	HBITMAP bitmap = ::CreateCompatibleBitmap(dc, width, height);
	if (!memory || !bitmap) {
		if (bitmap)
			::DeleteObject(bitmap);
		if (memory)
			::DeleteDC(memory);
		return;
	}
	HGDIOBJ oldBitmap = ::SelectObject(memory, bitmap);

	HBRUSH background = ::CreateSolidBrush(colors.background);
	HBRUSH occurrence = ::CreateSolidBrush(colors.occurrence);
	HBRUSH current = ::CreateSolidBrush(colors.current);

	const RECT all{ 0, 0, width, height };
	::FillRect(memory, &all, background);

	// Other occurrences are a bit narrower than the current one, so the two
	// differ in shape as well as in color.
	const int inset = width >= 6 ? width / 5 : 0;
	const int offset = m_layout.trackTop - bar.top;
	const size_t rows = m_rows.size();
	for (size_t y = 0; y < rows;) {
		const unsigned char kind = m_rows[y];
		if (kind == RowEmpty) {
			++y;
			continue;
		}
		const size_t first = y;
		while (y < rows && m_rows[y] == kind)
			++y;
		RECT marker = kind == RowCurrent
			? RECT{ 0, offset + static_cast<int>(first), width, offset + static_cast<int>(y) }
			: RECT{ inset, offset + static_cast<int>(first), width - inset, offset + static_cast<int>(y) };
		::FillRect(memory, &marker, kind == RowCurrent ? current : occurrence);
	}

	::BitBlt(dc, bar.left, bar.top, width, height, memory, 0, 0, SRCCOPY);

	::DeleteObject(background);
	::DeleteObject(occurrence);
	::DeleteObject(current);
	::SelectObject(memory, oldBitmap);
	::DeleteObject(bitmap);
	::DeleteDC(memory);
}

// ---------------------------------------------------------------------------
// Mouse

int MarkerBar::occurrenceAt(int screenY) const {
	if (m_lines.empty() || m_scale <= 0 || !m_layoutValid)
		return -1;

	RECT window{};
	::GetWindowRect(hwnd(), &window);
	const double y = static_cast<double>(screenY - window.top - m_layout.trackTop);
	const double height = markerHeight();
	const double tolerance = height / 2 + Dpi::scale(3, dpi());

	// Display line whose marker is centered at y
	const double line = (y - height / 2) / m_scale;
	const auto target = static_cast<Sci_Position>(std::ceil(line));
	const auto after = std::lower_bound(m_lines.begin(), m_lines.end(), target);

	long long best = -1;
	double bestDistance = tolerance;
	auto consider = [&](std::vector<Sci_Position>::const_iterator candidate) {
		// Use the first occurrence on that display line
		const auto first = std::lower_bound(m_lines.begin(), candidate + 1, *candidate);
		const double center = static_cast<double>(*first) * m_scale + height / 2;
		const double distance = std::fabs(center - y);
		const long long index = first - m_lines.begin();
		if (distance < bestDistance || (distance == bestDistance && index == m_current)) {
			bestDistance = distance;
			best = index;
		}
	};
	if (after != m_lines.end())
		consider(after);
	if (after != m_lines.begin())
		consider(after - 1);
	return static_cast<int>(best);
}

void MarkerBar::onMouseMove(POINT screen) {
	if (!m_trackingMouse) {
		TRACKMOUSEEVENT track{ sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT, hwnd(), 0 };
		m_trackingMouse = ::TrackMouseEvent(&track) != FALSE;
	}

	m_hoverPoint = screen;
	const long long index = occurrenceAt(screen.y);
	PreviewWindow& preview = m_plugin->preview();
	if (index == m_hoverIndex) {
		if (index >= 0 && preview.visibleFor(hwnd()))
			showPreview();   // follow the mouse vertically
		return;
	}

	m_hoverIndex = index;
	if (index < 0 || !m_plugin->settings().previewEnabled) {
		m_plugin->killTimer(m_index, HoverTimer);
		preview.hideFor(hwnd());
	} else if (preview.visibleFor(hwnd())) {
		showPreview();   // already previewing: switch at once
	} else {
		m_plugin->setTimer(m_index, HoverTimer, m_plugin->previewDelay());
	}
}

void MarkerBar::onMouseLeave() {
	if (m_hoverIndex < 0 && !m_plugin->preview().visibleFor(hwnd()))
		return;
	m_hoverIndex = -1;
	m_plugin->killTimer(m_index, HoverTimer);
	m_plugin->preview().hideFor(hwnd());
}

void MarkerBar::onClick(POINT screen) {
	m_plugin->killTimer(m_index, HoverTimer);
	m_plugin->preview().hideFor(hwnd());
	m_hoverIndex = -1;

	const int index = occurrenceAt(screen.y);
	if (index >= 0)
		goTo(static_cast<size_t>(index));
	else if (m_plugin->settings().scrollOnEmptyClick)
		scrollTo(screen.y);
	::SetFocus(hwnd());
}

void MarkerBar::goTo(size_t index) {
	const auto& occurrences = m_search.occurrences();
	if (index >= occurrences.size())
		return;
	const Occurrence occurrence = occurrences[index];
	if (m_textChanged || occurrence.end > m_editor.length())
		return;   // positions are being refreshed after an edit

	const Settings& settings = m_plugin->settings();
	const Sci_Position line = m_editor.lineFromPosition(occurrence.start);
	m_editor.call(SCI_ENSUREVISIBLE, static_cast<uptr_t>(line));   // unfold

	if (settings.selectOnClick) {
		m_editor.call(SCI_SETSEL, static_cast<uptr_t>(occurrence.start), occurrence.end);
		m_selectionStart = occurrence.start;
		m_selectionEnd = occurrence.end;
		m_caret = occurrence.end;
		setCurrent(occurrence.start);
	}

	if (settings.centerOnClick) {
		const Sci_Position displayLine = m_editor.displayLineFromDocLine(line);
		const Sci_Position onScreen = m_editor.call(SCI_LINESONSCREEN);
		m_editor.call(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(std::max<Sci_Position>(0, displayLine - onScreen / 2)));
	} else {
		m_editor.call(SCI_SCROLLRANGE, static_cast<uptr_t>(occurrence.end), occurrence.start);
	}
}

void MarkerBar::scrollTo(int screenY) {
	if (!m_layoutValid || m_scale <= 0)
		return;
	RECT window{};
	::GetWindowRect(hwnd(), &window);
	const double y = static_cast<double>(screenY - window.top - m_layout.trackTop);
	const auto displayLine = static_cast<Sci_Position>(y / m_scale);
	const Sci_Position onScreen = m_editor.call(SCI_LINESONSCREEN);
	m_editor.call(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(std::max<Sci_Position>(0, displayLine - onScreen / 2)));
}

void MarkerBar::showPreview() {
	const auto& occurrences = m_search.occurrences();
	if (m_hoverIndex < 0 || static_cast<size_t>(m_hoverIndex) >= occurrences.size() || m_textChanged)
		return;

	const Settings& settings = m_plugin->settings();
	RECT window{};
	::GetWindowRect(hwnd(), &window);

	PreviewWindow::Request request;
	request.occurrence = occurrences[static_cast<size_t>(m_hoverIndex)];
	request.number = static_cast<size_t>(m_hoverIndex) + 1;
	request.total = occurrences.size();
	request.totalIsLimited = m_search.truncated();
	request.contextLines = settings.previewContextLines;
	request.widthPercent = settings.previewWidthPercent;
	request.anchor = POINT{ window.left + m_layout.bar.left, m_hoverPoint.y };
	m_plugin->preview().show(m_editor, request);
}
