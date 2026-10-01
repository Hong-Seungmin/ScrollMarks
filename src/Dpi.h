// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#pragma once

#include <windows.h>

// DPI helpers that also work on Windows versions without per-monitor DPI APIs.
namespace Dpi {

inline UINT forWindow(HWND hwnd) {
	using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
	static const auto getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
		::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
	if (getDpiForWindow && hwnd) {
		const UINT dpi = getDpiForWindow(hwnd);
		if (dpi)
			return dpi;
	}
	HDC dc = ::GetDC(nullptr);
	const int dpi = dc ? ::GetDeviceCaps(dc, LOGPIXELSY) : 96;
	if (dc)
		::ReleaseDC(nullptr, dc);
	return dpi > 0 ? static_cast<UINT>(dpi) : 96;
}

inline int scale(int value, UINT dpi) {
	return ::MulDiv(value, static_cast<int>(dpi), 96);
}

inline int systemMetric(int index, UINT dpi) {
	using GetSystemMetricsForDpiFn = int(WINAPI*)(int, UINT);
	static const auto getSystemMetricsForDpi = reinterpret_cast<GetSystemMetricsForDpiFn>(
		::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi"));
	if (getSystemMetricsForDpi)
		return getSystemMetricsForDpi(index, dpi);
	return ::MulDiv(::GetSystemMetrics(index), static_cast<int>(dpi), static_cast<int>(forWindow(nullptr)));
}

} // namespace Dpi
