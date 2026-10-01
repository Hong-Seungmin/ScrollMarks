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

// Notepad++ preferences that ScrollMarks follows by default. They are read from
// Notepad++'s config.xml, which Notepad++ writes when it exits, so changes made
// in the Preferences dialog are picked up after the next restart (or when the
// ScrollMarks settings dialog is opened, if Notepad++ has saved them by then).
// The defaults below are Notepad++'s own defaults.
struct NppPreferences {
	bool smartHighlighting = true;
	bool matchCase = false;
	bool wholeWord = true;

	bool largeFileRestriction = true;
	bool largeFileAllowsSmartHighlighting = false;
	long long largeFileBytes = 200LL * 1024 * 1024;

	bool found = false;          // config.xml was found and read
	std::wstring configFile;

	static NppPreferences load(HWND npp);
};
