// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "Settings.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <iterator>

namespace {

class IniFile {
public:
	explicit IniFile(const std::wstring& path) : m_path(path) {}

	std::wstring read(const wchar_t* section, const wchar_t* key) const {
		wchar_t buffer[128] = {};
		::GetPrivateProfileStringW(section, key, L"", buffer, static_cast<DWORD>(std::size(buffer)), m_path.c_str());
		std::wstring value(buffer);
		value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](wchar_t c) { return !std::iswspace(c); }));
		value.erase(std::find_if(value.rbegin(), value.rend(), [](wchar_t c) { return !std::iswspace(c); }).base(), value.end());
		std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
		return value;
	}

	void write(const wchar_t* section, const wchar_t* key, const std::wstring& value) const {
		::WritePrivateProfileStringW(section, key, value.c_str(), m_path.c_str());
	}

	bool readBool(const wchar_t* section, const wchar_t* key, bool fallback) const {
		const std::wstring value = read(section, key);
		if (value == L"1" || value == L"yes" || value == L"true" || value == L"on")
			return true;
		if (value == L"0" || value == L"no" || value == L"false" || value == L"off")
			return false;
		return fallback;
	}

	int readInt(const wchar_t* section, const wchar_t* key, int fallback) const {
		const std::wstring value = read(section, key);
		if (value.empty())
			return fallback;
		wchar_t* end = nullptr;
		const long number = std::wcstol(value.c_str(), &end, 10);
		return (end && *end == L'\0') ? static_cast<int>(number) : fallback;
	}

	Choice readChoice(const wchar_t* section, const wchar_t* key, Choice fallback) const {
		const std::wstring value = read(section, key);
		if (value == L"auto" || value == L"notepad++")
			return Choice::FollowNotepad;
		if (value == L"1" || value == L"on" || value == L"yes")
			return Choice::On;
		if (value == L"0" || value == L"off" || value == L"no")
			return Choice::Off;
		return fallback;
	}

	NumberChoice readNumber(const wchar_t* section, const wchar_t* key, NumberChoice fallback) const {
		const std::wstring value = read(section, key);
		if (value == L"auto")
			return NumberChoice{ true, fallback.value };
		wchar_t* end = nullptr;
		const long number = std::wcstol(value.c_str(), &end, 10);
		if (!value.empty() && end && *end == L'\0')
			return NumberChoice{ false, static_cast<int>(number) };
		return fallback;
	}

	ColorChoice readColor(const wchar_t* section, const wchar_t* key, ColorChoice fallback) const {
		const std::wstring value = read(section, key);
		if (value == L"auto")
			return ColorChoice{ true, fallback.color };
		if (value.size() == 7 && value[0] == L'#') {
			wchar_t* end = nullptr;
			const unsigned long rgb = std::wcstoul(value.c_str() + 1, &end, 16);
			if (end && *end == L'\0')
				return ColorChoice{ false, RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF) };
		}
		return fallback;
	}

	void writeBool(const wchar_t* section, const wchar_t* key, bool value) const { write(section, key, value ? L"1" : L"0"); }
	void writeInt(const wchar_t* section, const wchar_t* key, int value) const { write(section, key, std::to_wstring(value)); }

	void writeChoice(const wchar_t* section, const wchar_t* key, Choice value) const {
		write(section, key, value == Choice::FollowNotepad ? L"auto" : value == Choice::On ? L"on" : L"off");
	}

	void writeNumber(const wchar_t* section, const wchar_t* key, const NumberChoice& value) const {
		write(section, key, value.automatic ? std::wstring(L"auto") : std::to_wstring(value.value));
	}

	void writeColor(const wchar_t* section, const wchar_t* key, const ColorChoice& value) const {
		if (value.automatic) {
			write(section, key, L"auto");
			return;
		}
		wchar_t buffer[8];
		swprintf_s(buffer, L"#%02X%02X%02X", GetRValue(value.color), GetGValue(value.color), GetBValue(value.color));
		write(section, key, buffer);
	}

private:
	std::wstring m_path;
};

} // namespace

void Settings::load(const std::wstring& file) {
	const IniFile ini(file);
	const Settings defaults;

	enabled = ini.readBool(L"General", L"Enabled", defaults.enabled);

	followSmartHighlighting = ini.readBool(L"Matching", L"FollowSmartHighlighting", defaults.followSmartHighlighting);
	matchCase = ini.readChoice(L"Matching", L"MatchCase", defaults.matchCase);
	wholeWord = ini.readChoice(L"Matching", L"WholeWord", defaults.wholeWord);
	useWordAtCaret = ini.readBool(L"Matching", L"UseWordAtCaret", defaults.useWordAtCaret);
	markLargeFiles = ini.readBool(L"Matching", L"MarkLargeFiles", defaults.markLargeFiles);
	minimumLength = ini.readInt(L"Matching", L"MinimumLength", defaults.minimumLength);
	maximumMarkers = ini.readInt(L"Matching", L"MaximumMarkers", defaults.maximumMarkers);

	barWidth = ini.readNumber(L"Markers", L"BarWidth", defaults.barWidth);
	minimumMarkerHeight = ini.readInt(L"Markers", L"MinimumMarkerHeight", defaults.minimumMarkerHeight);
	occurrenceColor = ini.readColor(L"Markers", L"OccurrenceColor", defaults.occurrenceColor);
	currentColor = ini.readColor(L"Markers", L"CurrentColor", defaults.currentColor);
	backgroundColor = ini.readColor(L"Markers", L"BackgroundColor", defaults.backgroundColor);

	selectOnClick = ini.readBool(L"Navigation", L"SelectOnClick", defaults.selectOnClick);
	centerOnClick = ini.readBool(L"Navigation", L"CenterOnClick", defaults.centerOnClick);
	scrollOnEmptyClick = ini.readBool(L"Navigation", L"ScrollOnEmptyClick", defaults.scrollOnEmptyClick);

	previewEnabled = ini.readBool(L"Preview", L"Enabled", defaults.previewEnabled);
	previewContextLines = ini.readInt(L"Preview", L"ContextLines", defaults.previewContextLines);
	previewDelay = ini.readNumber(L"Preview", L"Delay", defaults.previewDelay);
	previewWidthPercent = ini.readInt(L"Preview", L"WidthPercent", defaults.previewWidthPercent);

	clamp();
}

void Settings::save(const std::wstring& file) const {
	const IniFile ini(file);

	ini.writeBool(L"General", L"Enabled", enabled);

	ini.writeBool(L"Matching", L"FollowSmartHighlighting", followSmartHighlighting);
	ini.writeChoice(L"Matching", L"MatchCase", matchCase);
	ini.writeChoice(L"Matching", L"WholeWord", wholeWord);
	ini.writeBool(L"Matching", L"UseWordAtCaret", useWordAtCaret);
	ini.writeBool(L"Matching", L"MarkLargeFiles", markLargeFiles);
	ini.writeInt(L"Matching", L"MinimumLength", minimumLength);
	ini.writeInt(L"Matching", L"MaximumMarkers", maximumMarkers);

	ini.writeNumber(L"Markers", L"BarWidth", barWidth);
	ini.writeInt(L"Markers", L"MinimumMarkerHeight", minimumMarkerHeight);
	ini.writeColor(L"Markers", L"OccurrenceColor", occurrenceColor);
	ini.writeColor(L"Markers", L"CurrentColor", currentColor);
	ini.writeColor(L"Markers", L"BackgroundColor", backgroundColor);

	ini.writeBool(L"Navigation", L"SelectOnClick", selectOnClick);
	ini.writeBool(L"Navigation", L"CenterOnClick", centerOnClick);
	ini.writeBool(L"Navigation", L"ScrollOnEmptyClick", scrollOnEmptyClick);

	ini.writeBool(L"Preview", L"Enabled", previewEnabled);
	ini.writeInt(L"Preview", L"ContextLines", previewContextLines);
	ini.writeNumber(L"Preview", L"Delay", previewDelay);
	ini.writeInt(L"Preview", L"WidthPercent", previewWidthPercent);
}

void Settings::clamp() {
	minimumLength = std::clamp(minimumLength, 1, 1000);
	maximumMarkers = std::clamp(maximumMarkers, 0, 10000000);
	if (!barWidth.automatic)
		barWidth.value = std::clamp(barWidth.value, 2, 64);
	minimumMarkerHeight = std::clamp(minimumMarkerHeight, 1, 32);
	previewContextLines = std::clamp(previewContextLines, 0, 20);
	if (!previewDelay.automatic)
		previewDelay.value = std::clamp(previewDelay.value, 0, 5000);
	previewWidthPercent = std::clamp(previewWidthPercent, 20, 100);
}
