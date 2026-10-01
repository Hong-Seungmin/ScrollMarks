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
#include "Strings.h"

namespace {

constexpr UINT_PTR kSubclassId = 0x534D4252;   // "SMBR"
constexpr Sci_Position kMaximumTextBytes = 1024;
constexpr UINT kSelectionDelay = 15;           // coalesces bursts of selection changes
constexpr UINT kEditDelay = 250;               // waits for a pause in typing
constexpr UINT kIndicatorDelay = 150;          // indicators change in bursts too
constexpr UINT kMarkerDelay = 100;
constexpr UINT kSliceInterval = 1;             // next slice as soon as the message queue is idle
constexpr double kSliceBudget = 6.0;           // milliseconds of work per slice

std::wstring withSeparators(size_t value) {
	std::wstring digits = std::to_wstring(value);
	for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
		digits.insert(static_cast<size_t>(i), L",");
	return digits;
}

// Layers a click or hover can hit, from the top
constexpr Layer kHitOrder[] = {
	LayerCurrent, LayerOccurrences, LayerFindMarks, LayerStyleTokens, LayerOtherIndicators, LayerBookmarks, LayerChangeHistory,
};

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
	m_scan.clear();
	m_lines.clear();
	for (auto& rows : m_rows)
		rows.clear();
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

bool MarkerBar::occurrencesActive() const {
	const Settings& settings = m_plugin->settings();
	const NppPreferences& preferences = m_plugin->preferences();
	if (!settings.enabled || !settings.occurrences.enabled)
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
	const int pixels = width.automatic ? Dpi::systemMetric(SM_CXVSCROLL, currentDpi) : Dpi::scale(width.value, currentDpi);
	return std::clamp(pixels, 2, Dpi::scale(64, currentDpi));
}

void MarkerBar::stopTimers() {
	if (!m_plugin)
		return;
	for (TimerKind kind : { EvaluateTimer, SearchTimer, HoverTimer, ScanDelayTimer, ScanTimer })
		m_plugin->killTimer(m_index, kind);
	m_editPending = false;
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
				::SetCursor(::LoadCursorW(nullptr, self->hitAt(cursor).valid() ? IDC_HAND : IDC_ARROW));
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
// Events

void MarkerBar::onSelectionChanged() {
	if (!shown())
		return;
	updateCaretLine();
	if (m_editPending)
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
	m_plugin->preview().hideFor(hwnd());
	if (!shown())
		return;
	scheduleScan(MarkScan::All, kEditDelay);
	if (m_search.empty())
		return;   // nothing marked, the next selection change starts a new search
	// The occurrence positions are stale until the search runs again
	m_textChanged = true;
	m_editPending = true;
	m_plugin->setTimer(m_index, EvaluateTimer, kEditDelay);
}

void MarkerBar::onIndicatorsChanged() {
	const Settings& settings = m_plugin->settings();
	if (shown() && (settings.findMarks.enabled || settings.styleTokens.enabled || settings.otherIndicators.enabled))
		scheduleScan(MarkScan::Indicators, kIndicatorDelay);
}

void MarkerBar::onMarkersChanged() {
	if (shown() && m_plugin->settings().bookmarks.enabled)
		scheduleScan(MarkScan::Bookmarks, kMarkerDelay);
}

void MarkerBar::onSaved() {
	if (shown() && m_plugin->settings().changeHistory.enabled)
		scheduleScan(MarkScan::History, kMarkerDelay);
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
	m_selectionStart = m_selectionEnd = m_caret = m_caretDocLine = -1;
	m_scan.clear();
	invalidateLines();
	clearOccurrences();
	if (!shown())
		return;
	m_pendingScan = MarkScan::All;
	startScan();
	updateCaretLine();
	evaluateSelection();
}

void MarkerBar::onSettingsChanged() {
	if (!attached())
		return;
	stopTimers();
	refreshFrame();
	m_scan.clear();
	invalidateLines();
	clearOccurrences();
	m_selectionStart = m_selectionEnd = m_caret = m_caretDocLine = -1;
	if (!shown())
		return;
	m_pendingScan = MarkScan::All;
	startScan();
	updateCaretLine();
	evaluateSelection();
	rebuildRows();
	paintNow();
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
		if (m_hover.valid())
			showPreview();
		break;
	case ScanDelayTimer:
		m_plugin->killTimer(m_index, ScanDelayTimer);
		startScan();
		break;
	case ScanTimer:
		continueScan();
		break;
	}
}

// ---------------------------------------------------------------------------
// Occurrences

void MarkerBar::evaluateSelection() {
	if (!shown())
		return;

	m_selectionStart = m_editor.call(SCI_GETSELECTIONSTART);
	m_selectionEnd = m_editor.call(SCI_GETSELECTIONEND);
	m_caret = m_editor.call(SCI_GETCURRENTPOS);

	std::string text;
	Sci_Position start = 0;
	if (!occurrencesActive() || !readSearchText(text, start)) {
		clearOccurrences();
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
	m_hover = Hit();
	continueSearch();
}

void MarkerBar::continueSearch() {
	const bool finished = m_search.resume(m_editor, kSliceBudget);
	updateCurrentIndex();
	rebuildRows();
	paintNow();
	if (finished)
		m_plugin->killTimer(m_index, SearchTimer);
	else
		m_plugin->setTimer(m_index, SearchTimer, kSliceInterval);
}

void MarkerBar::clearOccurrences() {
	m_plugin->killTimer(m_index, SearchTimer);
	m_plugin->killTimer(m_index, HoverTimer);
	m_plugin->preview().hideFor(hwnd());
	const bool hadMarkers = !m_search.empty() || !m_lines.empty();
	m_search.reset();
	m_lines.clear();
	m_lastDocLine = -1;
	m_current = -1;
	m_currentStart = -1;
	m_hover = Hit();
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

void MarkerBar::updateCaretLine() {
	if (!m_plugin->settings().caretLine.enabled)
		return;
	const Sci_Position line = m_editor.lineFromPosition(m_editor.call(SCI_GETCURRENTPOS));
	if (line == m_caretDocLine)
		return;
	m_caretDocLine = line;
	if (!m_layoutValid)
		return;   // the next paint builds every row
	rebuildCaretRow();
	paintNow();
}

// ---------------------------------------------------------------------------
// Other marks

void MarkerBar::scheduleScan(unsigned sources, UINT delay) {
	m_pendingScan |= sources;
	m_plugin->setTimer(m_index, ScanDelayTimer, delay);
}

void MarkerBar::startScan() {
	if (!shown() || !m_pendingScan)
		return;
	const Settings& settings = m_plugin->settings();
	MarkScan::Request request;
	request.history = settings.changeHistory.enabled;
	request.bookmarks = settings.bookmarks.enabled;
	request.bookmarkMarker = m_plugin->bookmarkMarker();
	request.findMarks = settings.findMarks.enabled;
	request.styleTokens = settings.styleTokens.enabled;
	if (settings.otherIndicators.enabled)
		request.otherIndicators = settings.otherIndicatorNumbers();
	m_otherIndicators = request.otherIndicators;

	m_scan.begin(m_pendingScan, request, m_editor);
	m_pendingScan = 0;
	continueScan();
}

void MarkerBar::continueScan() {
	if (m_scan.resume(m_editor, kSliceBudget)) {
		m_plugin->killTimer(m_index, ScanTimer);
		for (bool& valid : m_markLinesValid)
			valid = false;
		rebuildRows();
		paintNow();
	} else {
		m_plugin->setTimer(m_index, ScanTimer, kSliceInterval);
	}
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
	if (linesMoved)
		invalidateLines();
	rebuildRows();
	paintNow();
}

void MarkerBar::invalidateLines() {
	m_lines.clear();
	m_lastDocLine = -1;
	for (bool& valid : m_markLinesValid)
		valid = false;
}

void MarkerBar::updateOccurrenceLines() {
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

void MarkerBar::updateMarkLines(Layer layer) {
	const std::vector<Mark>& marks = m_scan.marks(layer);
	std::vector<Sci_Position>& lines = m_markLines[layer];
	if (m_markLinesValid[layer] && lines.size() == marks.size())
		return;
	lines.clear();
	lines.reserve(marks.size());
	Sci_Position lastLine = -1;
	Sci_Position lastDisplay = 0;
	for (const Mark& mark : marks) {
		if (mark.line != lastLine) {
			lastLine = mark.line;
			lastDisplay = m_editor.displayLineFromDocLine(mark.line);
		}
		lines.push_back(lastDisplay);
	}
	m_markLinesValid[layer] = true;
}

void MarkerBar::fillRows(Layer layer, Sci_Position displayLine, int height, unsigned char value) {
	std::vector<unsigned char>& rows = m_rows[static_cast<size_t>(layer)];
	const int length = static_cast<int>(rows.size());
	// Rounded like Windows rounds the thumb position
	const int top = static_cast<int>(std::lround(static_cast<double>(displayLine) * m_scale));
	const int bottom = std::min(length, top + height);
	for (int y = std::max(0, top); y < bottom; ++y) {
		unsigned char& row = rows[static_cast<size_t>(y)];
		if (row == 0)
			row = value;
	}
}

void MarkerBar::rebuildRows() {
	if (!m_layoutValid) {
		m_layout = computeLayout();
		m_layoutValid = true;
	}
	m_scale = pixelsPerLine();
	const size_t length = static_cast<size_t>(m_layout.trackLength);
	for (auto& rows : m_rows)
		rows.assign(length, 0);
	if (length == 0 || m_scale <= 0)
		return;

	const int height = markerHeight();

	if (!m_search.occurrences().empty()) {
		updateOccurrenceLines();
		for (Sci_Position line : m_lines)
			fillRows(LayerOccurrences, line, height, 1);
		if (m_current >= 0 && static_cast<size_t>(m_current) < m_lines.size())
			fillRows(LayerCurrent, m_lines[static_cast<size_t>(m_current)], height, 1);
	}

	for (Layer layer : { LayerChangeHistory, LayerBookmarks, LayerOtherIndicators, LayerStyleTokens, LayerFindMarks }) {
		const std::vector<Mark>& marks = m_scan.marks(layer);
		if (marks.empty())
			continue;
		updateMarkLines(layer);
		const std::vector<Sci_Position>& lines = m_markLines[layer];
		for (size_t i = 0; i < marks.size(); ++i)
			fillRows(layer, lines[i], height, marks[i].value);
	}

	rebuildCaretRow();
}

void MarkerBar::rebuildCaretRow() {
	std::vector<unsigned char>& rows = m_rows[LayerCaretLine];
	std::fill(rows.begin(), rows.end(), static_cast<unsigned char>(0));
	if (!m_plugin->settings().caretLine.enabled || rows.empty() || m_scale <= 0)
		return;
	if (m_caretDocLine < 0)
		m_caretDocLine = m_editor.lineFromPosition(m_editor.call(SCI_GETCURRENTPOS));

	// The selected occurrence already shows where the caret is
	const auto& occurrences = m_search.occurrences();
	if (m_current >= 0 && static_cast<size_t>(m_current) < occurrences.size() &&
		m_editor.lineFromPosition(occurrences[static_cast<size_t>(m_current)].start) == m_caretDocLine)
		return;

	// Centered on the row a marker of that line would use
	const int thickness = Dpi::scale(m_plugin->settings().caretLineThickness, dpi());
	const Sci_Position display = m_editor.displayLineFromDocLine(m_caretDocLine);
	const int top = static_cast<int>(std::lround(static_cast<double>(display) * m_scale)) + (markerHeight() - thickness) / 2;
	const int length = static_cast<int>(rows.size());
	for (int y = std::max(0, top); y < std::min(length, top + thickness); ++y)
		rows[static_cast<size_t>(y)] = 1;
}

void MarkerBar::paintNow() {
	if (!shown())
		return;
	if (!m_layoutValid)
		rebuildRows();
	HDC dc = ::GetWindowDC(hwnd());
	if (!dc)
		return;
	const RECT& bar = m_layout.bar;
	const BarStyle style = m_plugin->barStyle(m_editor);
	paintBar(dc, bar.left, bar.top, bar.right - bar.left, bar.bottom - bar.top, style, m_rows, m_layout.trackTop - bar.top);
	::ReleaseDC(hwnd(), dc);
}

// ---------------------------------------------------------------------------
// Mouse

long long MarkerBar::nearest(const std::vector<Sci_Position>& lines, double y, double tolerance) const {
	if (lines.empty())
		return -1;
	const double height = markerHeight();
	const double line = (y - height / 2) / m_scale;
	const auto target = static_cast<Sci_Position>(std::ceil(line));
	const auto after = std::lower_bound(lines.begin(), lines.end(), target);

	long long best = -1;
	double bestDistance = tolerance;
	auto consider = [&](std::vector<Sci_Position>::const_iterator candidate) {
		// The first mark on that display line
		const auto first = std::lower_bound(lines.begin(), candidate + 1, *candidate);
		const double center = std::lround(static_cast<double>(*first) * m_scale) + height / 2;
		const double distance = std::fabs(center - y);
		if (distance <= bestDistance) {
			bestDistance = distance;
			best = first - lines.begin();
		}
	};
	if (after != lines.end())
		consider(after);
	if (after != lines.begin())
		consider(after - 1);
	return best;
}

MarkerBar::Hit MarkerBar::hitAt(POINT screen) const {
	if (!m_layoutValid || m_scale <= 0)
		return Hit();

	RECT window{};
	::GetWindowRect(hwnd(), &window);
	const double y = static_cast<double>(screen.y - window.top - m_layout.trackTop);
	const int x = screen.x - window.left - m_layout.bar.left;
	const int width = m_layout.bar.right - m_layout.bar.left;
	const double tolerance = markerHeight() / 2.0 + Dpi::scale(3, dpi());
	const BarStyle style = m_plugin->barStyle(m_editor);

	// First the layers under the mouse horizontally, then any layer
	for (int pass = 0; pass < 2; ++pass) {
		for (Layer layer : kHitOrder) {
			const LayerStyle& layerStyle = style.layers[layer];
			if (!layerStyle.enabled)
				continue;
			if (pass == 0) {
				int left = 0;
				int right = 0;
				layerSpan(layerStyle, width, left, right);
				if (x < left - 1 || x > right)
					continue;
			}

			if (layer == LayerCurrent) {
				if (m_current < 0 || static_cast<size_t>(m_current) >= m_lines.size())
					continue;
				const std::vector<Sci_Position> single{ m_lines[static_cast<size_t>(m_current)] };
				if (nearest(single, y, tolerance) >= 0)
					return Hit{ LayerCurrent, static_cast<size_t>(m_current) };
				continue;
			}

			const std::vector<Sci_Position>& lines = layer == LayerOccurrences ? m_lines : m_markLines[layer];
			const long long index = nearest(lines, y, tolerance);
			if (index >= 0)
				return Hit{ layer, static_cast<size_t>(index) };
		}
	}
	return Hit();
}

Mark MarkerBar::markOf(const Hit& hit) const {
	if (hit.layer == LayerOccurrences || hit.layer == LayerCurrent) {
		const Occurrence& occurrence = m_search.occurrences()[hit.index];
		return Mark{ occurrence.start, occurrence.end, m_editor.lineFromPosition(occurrence.start), 1 };
	}
	return m_scan.marks(hit.layer)[hit.index];
}

std::wstring MarkerBar::captionOf(const Hit& hit, Sci_Position line) const {
	std::wstring kind;
	size_t total = 0;
	bool limited = false;
	switch (hit.layer) {
	case LayerOccurrences:
	case LayerCurrent:
		kind = tr(Text::KindOccurrence);
		total = m_search.occurrences().size();
		limited = m_search.truncated();
		break;
	case LayerBookmarks:
		kind = tr(Text::KindBookmark);
		break;
	case LayerFindMarks:
		kind = tr(Text::KindFindMark);
		break;
	case LayerChangeHistory: {
		static const Text states[kHistoryStates] = { Text::HistoryModified, Text::HistorySaved, Text::HistoryReverted, Text::HistoryRevertedModified };
		const int value = std::clamp<int>(m_scan.marks(hit.layer)[hit.index].value, 1, kHistoryStates);
		kind = tr(states[value - 1]);
		break;
	}
	case LayerStyleTokens:
		kind = std::wstring(tr(Text::KindStyleToken)) + L" " + std::to_wstring(m_scan.marks(hit.layer)[hit.index].value);
		break;
	case LayerOtherIndicators: {
		const size_t value = m_scan.marks(hit.layer)[hit.index].value;
		kind = tr(Text::KindIndicator);
		if (value >= 1 && value <= m_otherIndicators.size())
			kind += L" " + std::to_wstring(m_otherIndicators[value - 1]);
		break;
	}
	default:
		break;
	}
	if (total == 0)
		total = m_scan.marks(hit.layer).size();

	return std::wstring(tr(Text::CaptionLine)) + L" " + withSeparators(static_cast<size_t>(line) + 1) + L"    " + kind + L"  " +
		withSeparators(hit.index + 1) + L" / " + withSeparators(total) + (limited ? L"+" : L"");
}

void MarkerBar::onMouseMove(POINT screen) {
	if (!m_trackingMouse) {
		TRACKMOUSEEVENT track{ sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT, hwnd(), 0 };
		m_trackingMouse = ::TrackMouseEvent(&track) != FALSE;
	}

	m_hoverPoint = screen;
	const Hit hit = hitAt(screen);
	PreviewWindow& preview = m_plugin->preview();
	if (hit == m_hover) {
		if (hit.valid() && preview.visibleFor(hwnd()))
			showPreview();   // follow the mouse vertically
		return;
	}

	m_hover = hit;
	if (!hit.valid() || !m_plugin->settings().previewEnabled) {
		m_plugin->killTimer(m_index, HoverTimer);
		preview.hideFor(hwnd());
	} else if (preview.visibleFor(hwnd())) {
		showPreview();   // already previewing: switch at once
	} else {
		m_plugin->setTimer(m_index, HoverTimer, m_plugin->previewDelay());
	}
}

void MarkerBar::onMouseLeave() {
	if (!m_hover.valid() && !m_plugin->preview().visibleFor(hwnd()))
		return;
	m_hover = Hit();
	m_plugin->killTimer(m_index, HoverTimer);
	m_plugin->preview().hideFor(hwnd());
}

void MarkerBar::onClick(POINT screen) {
	m_plugin->killTimer(m_index, HoverTimer);
	m_plugin->preview().hideFor(hwnd());
	m_hover = Hit();

	const Hit hit = hitAt(screen);
	if (hit.valid())
		goTo(hit);
	else if (m_plugin->settings().scrollOnEmptyClick)
		scrollTo(screen.y);
	::SetFocus(hwnd());
}

void MarkerBar::scrollToLine(Sci_Position docLine, bool center) {
	const Sci_Position display = m_editor.displayLineFromDocLine(docLine);
	const Sci_Position onScreen = m_editor.call(SCI_LINESONSCREEN);
	const Sci_Position first = m_editor.call(SCI_GETFIRSTVISIBLELINE);
	Sci_Position target = first;
	if (center)
		target = display - onScreen / 2;
	else if (display < first)
		target = display;
	else if (display >= first + onScreen)
		target = display - onScreen + 1;
	m_editor.call(SCI_SETFIRSTVISIBLELINE, static_cast<uptr_t>(std::max<Sci_Position>(0, target)));
}

void MarkerBar::goTo(const Hit& hit) {
	const bool occurrence = hit.layer == LayerOccurrences || hit.layer == LayerCurrent;
	if (occurrence && m_textChanged)
		return;   // positions are being refreshed after an edit
	const Mark mark = markOf(hit);
	if (mark.end > m_editor.length() || mark.line >= m_editor.lineCount())
		return;

	const Settings& settings = m_plugin->settings();
	m_editor.call(SCI_ENSUREVISIBLE, static_cast<uptr_t>(mark.line));   // unfold

	if (settings.moveCaretOnClick) {
		if (mark.end > mark.start)
			m_editor.call(SCI_SETSEL, static_cast<uptr_t>(mark.start), mark.end);
		else
			m_editor.call(SCI_SETEMPTYSELECTION, static_cast<uptr_t>(mark.start));
	}
	scrollToLine(mark.line, settings.centerOnClick);
	if (settings.flashLine)
		m_plugin->flashLine(m_editor, mark.line);
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
	if (!m_hover.valid())
		return;
	const bool occurrence = m_hover.layer == LayerOccurrences || m_hover.layer == LayerCurrent;
	if (occurrence && m_textChanged)
		return;   // positions are being refreshed after an edit
	const size_t count = occurrence ? m_search.occurrences().size() : m_scan.marks(m_hover.layer).size();
	if (m_hover.index >= count)
		return;

	const Settings& settings = m_plugin->settings();
	RECT window{};
	::GetWindowRect(hwnd(), &window);

	const Mark mark = markOf(m_hover);
	PreviewWindow::Request request;
	request.occurrence = Occurrence{ mark.start, mark.end };
	request.caption = captionOf(m_hover, mark.line);

	// Occurrences of the searched text in the lines the preview shows: Notepad++
	// only highlights the lines on screen, so the preview highlights them itself
	const auto& occurrences = m_search.occurrences();
	if (!occurrences.empty() && !m_textChanged) {
		const Sci_Position lines = m_editor.lineCount();
		const Sci_Position firstLine = std::max<Sci_Position>(0, mark.line - settings.previewContextLines);
		const Sci_Position lastLine = std::min<Sci_Position>(lines - 1, mark.line + settings.previewContextLines);
		const Sci_Position from = m_editor.call(SCI_POSITIONFROMLINE, static_cast<uptr_t>(firstLine));
		const Sci_Position to = m_editor.call(SCI_GETLINEENDPOSITION, static_cast<uptr_t>(lastLine));
		auto it = std::lower_bound(occurrences.begin(), occurrences.end(), from,
			[](const Occurrence& occurrence, Sci_Position position) { return occurrence.start < position; });
		for (; it != occurrences.end() && it->start < to && request.highlights.size() < 1000; ++it)
			request.highlights.push_back(*it);
	}
	request.contextLines = settings.previewContextLines;
	request.widthPercent = settings.previewWidthPercent;
	request.anchor = POINT{ window.left + m_layout.bar.left, m_hoverPoint.y };
	m_plugin->preview().show(m_editor, request);
}

// ---------------------------------------------------------------------------
// Previous / next occurrence

bool MarkerBar::jump(bool forward) {
	if (!attached())
		return false;

	Sci_Position from = m_editor.call(SCI_GETSELECTIONSTART);
	Sci_Position to = m_editor.call(SCI_GETSELECTIONEND);
	if (m_editor.call(SCI_GETSELECTIONS) > 1)
		return false;
	if (from == to) {
		const Sci_Position caret = m_editor.call(SCI_GETCURRENTPOS);
		from = m_editor.call(SCI_WORDSTARTPOSITION, static_cast<uptr_t>(caret), 1);
		to = m_editor.call(SCI_WORDENDPOSITION, static_cast<uptr_t>(caret), 1);
	}
	if (from == to || to - from > kMaximumTextBytes || m_editor.lineFromPosition(from) != m_editor.lineFromPosition(to))
		return false;

	const std::string text = m_editor.text(from, to);
	int flags = m_plugin->searchFlags();
	const bool oneWord = m_editor.call(SCI_ISRANGEWORD, static_cast<uptr_t>(from), to) &&
		m_editor.call(SCI_WORDENDPOSITION, static_cast<uptr_t>(from), 1) == to;
	if (!oneWord)
		flags &= ~SCFIND_WHOLEWORD;

	// The target and the search flags are shared with Notepad++: restore them
	const sptr_t savedStart = m_editor.call(SCI_GETTARGETSTART);
	const sptr_t savedEnd = m_editor.call(SCI_GETTARGETEND);
	const sptr_t savedFlags = m_editor.call(SCI_GETSEARCHFLAGS);
	m_editor.call(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(flags));

	const Sci_Position length = m_editor.length();
	auto search = [&](Sci_Position start, Sci_Position end) {
		// A target that ends before it starts is searched backwards
		m_editor.call(SCI_SETTARGETRANGE, static_cast<uptr_t>(start), end);
		return m_editor.call(SCI_SEARCHINTARGET, text.size(), text.c_str());
	};
	Sci_Position found = forward ? search(to, length) : search(from, 0);
	if (found < 0 && m_plugin->settings().wrapAround)
		found = forward ? search(0, from) : search(length, to);
	const Sci_Position foundEnd = found >= 0 ? m_editor.call(SCI_GETTARGETEND) : -1;

	m_editor.call(SCI_SETSEARCHFLAGS, static_cast<uptr_t>(savedFlags));
	m_editor.call(SCI_SETTARGETRANGE, static_cast<uptr_t>(savedStart), savedEnd);
	if (found < 0)
		return false;

	const Sci_Position line = m_editor.lineFromPosition(found);
	m_editor.call(SCI_ENSUREVISIBLE, static_cast<uptr_t>(line));
	m_editor.call(SCI_SETSEL, static_cast<uptr_t>(found), foundEnd);
	if (m_plugin->settings().centerOnJump)
		scrollToLine(line, true);
	else
		m_editor.call(SCI_SCROLLRANGE, static_cast<uptr_t>(foundEnd), found);
	return true;
}
