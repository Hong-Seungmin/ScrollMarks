// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "NppPreferences.h"

#include <shlobj.h>
#include <cctype>
#include <cstdlib>
#include <map>

#include "Notepad_plus_msgs.h"

namespace {

struct XmlElement {
	std::map<std::string, std::string> attributes;
	std::string text;

	std::string attribute(const char* name) const {
		const auto it = attributes.find(name);
		return it == attributes.end() ? std::string() : it->second;
	}
};

bool isSpace(char c) {
	return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string trim(const std::string& value) {
	size_t begin = 0;
	size_t end = value.size();
	while (begin < end && isSpace(value[begin]))
		++begin;
	while (end > begin && isSpace(value[end - 1]))
		--end;
	return value.substr(begin, end - begin);
}

// Finds the first <tag ...> element, optionally with name="nameAttribute".
// This is not a general XML parser; it only has to understand config.xml.
bool findElement(const std::string& xml, const std::string& tag, const char* nameAttribute, XmlElement& out) {
	const std::string open = "<" + tag;
	size_t position = 0;
	while ((position = xml.find(open, position)) != std::string::npos) {
		size_t p = position + open.size();
		if (p >= xml.size() || !(isSpace(xml[p]) || xml[p] == '/' || xml[p] == '>')) {
			position = p;
			continue;
		}

		XmlElement element;
		bool selfClosing = false;
		bool closed = false;
		while (p < xml.size() && !closed) {
			const char c = xml[p];
			if (isSpace(c)) {
				++p;
			} else if (c == '/') {
				selfClosing = true;
				++p;
			} else if (c == '>') {
				closed = true;
				++p;
			} else {
				const size_t keyBegin = p;
				while (p < xml.size() && xml[p] != '=' && xml[p] != '>' && xml[p] != '/' && !isSpace(xml[p]))
					++p;
				const std::string key = xml.substr(keyBegin, p - keyBegin);
				while (p < xml.size() && isSpace(xml[p]))
					++p;
				if (p < xml.size() && xml[p] == '=') {
					++p;
					while (p < xml.size() && isSpace(xml[p]))
						++p;
					if (p < xml.size() && (xml[p] == '"' || xml[p] == '\'')) {
						const char quote = xml[p++];
						const size_t valueEnd = xml.find(quote, p);
						if (valueEnd == std::string::npos)
							return false;
						element.attributes[key] = xml.substr(p, valueEnd - p);
						p = valueEnd + 1;
					}
				} else if (key.empty()) {
					++p;
				}
			}
		}

		if (closed && !selfClosing) {
			const size_t textEnd = xml.find('<', p);
			if (textEnd != std::string::npos)
				element.text = trim(xml.substr(p, textEnd - p));
		}

		if (!nameAttribute || element.attribute("name") == nameAttribute) {
			out = std::move(element);
			return true;
		}
		position = p;
	}
	return false;
}

bool isYes(const std::string& value, bool fallback) {
	if (value == "yes" || value == "true" || value == "1")
		return true;
	if (value == "no" || value == "false" || value == "0")
		return false;
	return fallback;
}

std::wstring settingsDirectory(HWND npp) {
	// Notepad++ 8.6.8 and later report the active settings directory, which
	// covers -settingsDir, cloud settings, AppData and portable installations.
	const int length = static_cast<int>(::SendMessage(npp, NPPM_GETNPPSETTINGSDIRPATH, 0, 0));
	if (length > 0) {
		std::wstring path(static_cast<size_t>(length) + 1, L'\0');
		if (::SendMessage(npp, NPPM_GETNPPSETTINGSDIRPATH, path.size(), reinterpret_cast<LPARAM>(path.data())) > 0) {
			path.resize(wcslen(path.c_str()));
			return path;
		}
	}

	// Older versions: portable when doLocalConf.xml is next to notepad++.exe
	wchar_t nppDirectory[MAX_PATH] = {};
	::SendMessage(npp, NPPM_GETNPPDIRECTORY, MAX_PATH, reinterpret_cast<LPARAM>(nppDirectory));
	const std::wstring localMarker = std::wstring(nppDirectory) + L"\\doLocalConf.xml";
	if (::GetFileAttributesW(localMarker.c_str()) != INVALID_FILE_ATTRIBUTES)
		return nppDirectory;

	PWSTR appData = nullptr;
	std::wstring path;
	if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData)))
		path = std::wstring(appData) + L"\\Notepad++";
	::CoTaskMemFree(appData);
	return path;
}

bool readFile(const std::wstring& path, std::string& content) {
	HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return false;

	LARGE_INTEGER size{};
	bool ok = ::GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < 64LL * 1024 * 1024;
	if (ok) {
		content.resize(static_cast<size_t>(size.QuadPart));
		DWORD read = 0;
		ok = ::ReadFile(file, content.data(), static_cast<DWORD>(content.size()), &read, nullptr) && read == content.size();
	}
	::CloseHandle(file);
	return ok;
}

} // namespace

NppPreferences NppPreferences::load(HWND npp) {
	NppPreferences preferences;

	const std::wstring directory = settingsDirectory(npp);
	if (directory.empty())
		return preferences;

	preferences.configFile = directory + L"\\config.xml";
	std::string xml;
	if (!readFile(preferences.configFile, xml))
		return preferences;
	preferences.found = true;

	// <GUIConfig name="SmartHighLight" matchCase="no" wholeWordOnly="yes" useFindSettings="no" onAnotherView="no">yes</GUIConfig>
	XmlElement smart;
	if (findElement(xml, "GUIConfig", "SmartHighLight", smart)) {
		preferences.smartHighlighting = isYes(smart.text, preferences.smartHighlighting);
		preferences.matchCase = isYes(smart.attribute("matchCase"), preferences.matchCase);
		preferences.wholeWord = isYes(smart.attribute("wholeWordOnly"), preferences.wholeWord);

		// "Use Find dialog settings" makes smart highlighting follow the Find dialog
		XmlElement find;
		preferences.followsFindDialog = isYes(smart.attribute("useFindSettings"), false);
		if (preferences.followsFindDialog && findElement(xml, "FindHistory", nullptr, find)) {
			preferences.matchCase = isYes(find.attribute("matchCase"), preferences.matchCase);
			preferences.wholeWord = isYes(find.attribute("matchWord"), preferences.wholeWord);
		}
	}

	// <GUIConfig name="largeFileRestriction" fileSizeMB="200" isEnabled="yes" allowSmartHilite="no" ... />
	XmlElement largeFile;
	if (findElement(xml, "GUIConfig", "largeFileRestriction", largeFile)) {
		preferences.largeFileRestriction = isYes(largeFile.attribute("isEnabled"), preferences.largeFileRestriction);
		preferences.largeFileAllowsSmartHighlighting = isYes(largeFile.attribute("allowSmartHilite"), preferences.largeFileAllowsSmartHighlighting);
		const std::string megabytes = largeFile.attribute("fileSizeMB");
		if (!megabytes.empty()) {
			const long long value = std::strtoll(megabytes.c_str(), nullptr, 10);
			if (value > 0)
				preferences.largeFileBytes = value * 1024 * 1024;
		}
	}

	return preferences;
}
