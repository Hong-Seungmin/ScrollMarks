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

#include "Scintilla.h"

// Thin wrapper around one Scintilla window. Calls go through Scintilla's
// direct function, which is a plain function call instead of a window message.
class Editor {
public:
	Editor() = default;
	explicit Editor(HWND hwnd) { attach(hwnd); }

	void attach(HWND hwnd) {
		m_hwnd = hwnd;
		m_pointer = 0;
		m_directStatus = nullptr;
		m_direct = nullptr;
		if (!hwnd)
			return;
		m_pointer = static_cast<sptr_t>(::SendMessage(hwnd, SCI_GETDIRECTPOINTER, 0, 0));
		m_directStatus = reinterpret_cast<SciFnDirectStatus>(::SendMessage(hwnd, SCI_GETDIRECTSTATUSFUNCTION, 0, 0));
		if (!m_directStatus)
			m_direct = reinterpret_cast<SciFnDirect>(::SendMessage(hwnd, SCI_GETDIRECTFUNCTION, 0, 0));
	}

	HWND hwnd() const { return m_hwnd; }

	sptr_t call(unsigned int message, uptr_t wParam = 0, sptr_t lParam = 0) const {
		if (m_directStatus && m_pointer) {
			int status = 0;
			return m_directStatus(m_pointer, message, wParam, lParam, &status);
		}
		if (m_direct && m_pointer)
			return m_direct(m_pointer, message, wParam, lParam);
		return m_hwnd ? static_cast<sptr_t>(::SendMessage(m_hwnd, message, wParam, lParam)) : 0;
	}

	template <typename T>
	sptr_t call(unsigned int message, uptr_t wParam, T* pointer) const {
		return call(message, wParam, reinterpret_cast<sptr_t>(pointer));
	}

	Sci_Position length() const { return call(SCI_GETLENGTH); }
	Sci_Position lineCount() const { return call(SCI_GETLINECOUNT); }
	Sci_Position lineFromPosition(Sci_Position position) const { return call(SCI_LINEFROMPOSITION, position); }
	Sci_Position displayLineFromDocLine(Sci_Position line) const { return call(SCI_VISIBLEFROMDOCLINE, line); }
	sptr_t document() const { return call(SCI_GETDOCPOINTER); }

	// Bytes of the document between start and end.
	std::string text(Sci_Position start, Sci_Position end) const {
		std::string result;
		if (end <= start)
			return result;
		result.assign(static_cast<size_t>(end - start) + 1, '\0');
		Sci_TextRangeFull range{ { start, end }, result.data() };
		const sptr_t copied = call(SCI_GETTEXTRANGEFULL, 0, &range);
		if (copied > 0) {
			result.resize(static_cast<size_t>(copied));
			return result;
		}
		// Scintilla before 5.3 has no SCI_GETTEXTRANGEFULL
		const char* pointer = reinterpret_cast<const char*>(call(SCI_GETRANGEPOINTER, start, end - start));
		if (pointer)
			result.assign(pointer, static_cast<size_t>(end - start));
		else
			result.clear();
		return result;
	}

private:
	HWND m_hwnd = nullptr;
	sptr_t m_pointer = 0;
	SciFnDirectStatus m_directStatus = nullptr;
	SciFnDirect m_direct = nullptr;
};
