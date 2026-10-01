// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

// Entry points called by Notepad++.

#include <windows.h>

#include "Plugin.h"
#include "Version.h"

BOOL APIENTRY DllMain(HINSTANCE module, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		::DisableThreadLibraryCalls(module);
		Plugin::instance().setModule(module);
	}
	return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData data) {
	Plugin::instance().setInfo(data);
}

extern "C" __declspec(dllexport) const wchar_t* getName() {
	return SCROLLMARKS_NAME;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* count) {
	return Plugin::instance().commands(count);
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notification) {
	// Never let an unexpected error take Notepad++ down
	try {
		Plugin::instance().notify(notification);
	} catch (...) {
	}
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) {
	return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode() {
	return TRUE;
}
