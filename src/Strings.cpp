// ScrollMarks - occurrence markers next to the Notepad++ scrollbar
// Copyright (C) 2026 Hong-Seungmin
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See the LICENSE file for details.

#include "Strings.h"

#include <cwctype>
#include <string>

#include "Notepad_plus_msgs.h"
#include "NppPreferences.h"

namespace {

struct Entry {
	Text id;
	const wchar_t* english;
	const wchar_t* korean;
};

const Entry kEntries[] = {
	{ Text::MenuShowMarkers, L"Show Markers", L"마커 표시" },
	{ Text::MenuSettings, L"Settings...", L"설정..." },
	{ Text::MenuNext, L"Next Occurrence", L"다음 위치로 이동" },
	{ Text::MenuPrevious, L"Previous Occurrence", L"이전 위치로 이동" },
	{ Text::MenuAbout, L"About ScrollMarks", L"ScrollMarks 정보" },
	{ Text::AboutTitle, L"About ScrollMarks", L"ScrollMarks 정보" },
	{ Text::AboutBody,
		L"Marks every occurrence of the selected text, bookmarks, changes and search marks next to the scrollbar.\n"
		L"Click a marker to go there, hover it to preview the lines around it.",
		L"선택한 텍스트와 같은 위치, 북마크, 변경 내역, 찾기 표시를 스크롤바 옆에 표시합니다.\n"
		L"마커를 클릭하면 그 위치로 가고, 마우스를 올리면 주변 줄을 미리 봅니다." },

	{ Text::DialogTitle, L"ScrollMarks Settings", L"ScrollMarks 설정" },
	{ Text::TabGeneral, L"General", L"일반" },
	{ Text::TabOccurrences, L"Occurrences", L"같은 단어" },
	{ Text::TabOtherMarks, L"Other marks", L"다른 표시" },
	{ Text::TabCaretLine, L"Caret line", L"커서 위치선" },
	{ Text::TabNavigation, L"Click and keys", L"클릭과 이동" },
	{ Text::HoverGroup, L"Hover preview", L"마우스오버 미리보기" },
	{ Text::ShowPreview, L"Show a preview when hovering a marker", L"마커에 마우스를 올리면 미리보기 표시" },
	{ Text::ContextLines, L"Context lines:", L"주변 줄 수:" },
	{ Text::AboveAndBelow, L"above and below", L"위아래 각각" },
	{ Text::HoverDelay, L"Hover delay:", L"표시 지연:" },
	{ Text::Milliseconds, L"ms", L"ms" },
	{ Text::PreviewWidth, L"Preview width:", L"미리보기 폭:" },
	{ Text::PercentOfEditor, L"% of the editor", L"% (에디터 폭 기준)" },
	{ Text::SampleGroup, L"Sample", L"견본" },
	{ Text::ResetDefaults, L"R&eset to defaults", L"기본값으로(&E)" },
	{ Text::Ok, L"OK", L"확인" },
	{ Text::Cancel, L"Cancel", L"취소" },
	{ Text::Automatic, L"Automatic", L"자동" },
	{ Text::Pixels, L"px", L"px" },
	{ Text::Percent, L"%", L"%" },
	{ Text::ColumnColor, L"Color", L"색" },
	{ Text::ColumnPosition, L"Position", L"위치" },
	{ Text::ColumnWidth, L"Width", L"폭" },
	{ Text::Left, L"Left", L"왼쪽" },
	{ Text::Center, L"Center", L"가운데" },
	{ Text::Right, L"Right", L"오른쪽" },
	{ Text::LikeNotepadOn, L"Like Notepad++ (on)", L"Notepad++ 따르기 (켬)" },
	{ Text::LikeNotepadOff, L"Like Notepad++ (off)", L"Notepad++ 따르기 (끔)" },
	{ Text::On, L"On", L"켬" },
	{ Text::Off, L"Off", L"끔" },

	{ Text::ShowMarkers, L"&Show markers next to the scrollbar", L"스크롤바 옆에 마커 표시(&S)" },
	{ Text::Language, L"Language:", L"언어:" },
	{ Text::LanguageAuto, L"Like Notepad++", L"Notepad++ 따르기" },
	{ Text::BarWidth, L"Bar width:", L"막대 폭:" },
	{ Text::MinimumMarkerHeight, L"Minimum marker height:", L"마커 최소 높이:" },
	{ Text::Background, L"Background:", L"배경:" },
	{ Text::PriorityNote,
		L"Markers are drawn from the lowest priority to the highest: change history, bookmarks, other indicators, "
		L"style tokens, find marks, occurrences, the selected occurrence and the caret line. Higher ones are drawn "
		L"on top and narrower by default, so the ones below stay visible around them.",
		L"마커는 우선순위가 낮은 것부터 그립니다: 변경 내역, 북마크, 기타 인디케이터, 스타일 토큰, 찾기 표시, "
		L"같은 단어, 선택한 위치, 커서 위치선. 높은 것이 위에 그려지고 기본 폭이 더 좁아서, 겹쳐도 아래 것이 "
		L"둘레로 보입니다." },

	{ Text::MarkOccurrences, L"&Mark every occurrence of the selected text", L"선택한 텍스트와 같은 위치 모두 표시(&M)" },
	{ Text::FollowSmartHighlighting, L"Only while Notepad++ smart highlighting is enabled", L"Notepad++ 스마트 강조가 켜져 있을 때만" },
	{ Text::MatchCase, L"Match case:", L"대소문자 구분:" },
	{ Text::WholeWord, L"Whole word only:", L"단어 단위로만:" },
	{ Text::MinimumLength, L"Minimum length:", L"최소 길이:" },
	{ Text::Characters, L"characters", L"글자" },
	{ Text::MaximumMarkers, L"Maximum markers:", L"최대 마커 수:" },
	{ Text::NoLimit, L"0 = no limit", L"0 = 제한 없음" },
	{ Text::WordAtCaret, L"Mark the word at the caret when nothing is selected", L"선택이 없으면 커서 위치의 단어 표시" },
	{ Text::LargeFiles, L"Mark large files even when Notepad++ restricts smart highlighting", L"Notepad++ 가 제한하는 대용량 파일도 표시" },
	{ Text::OccurrencesRow, L"Occurrences:", L"같은 단어:" },
	{ Text::CurrentRow, L"Selected occurrence:", L"선택한 위치:" },
	{ Text::WholeWordWarning,
		L"Note: Notepad++ smart highlighting still matches whole words only. Turn off \"Match whole word only\" in "
		L"Settings > Preferences > Highlighting > Smart Highlighting too, so the editor and the markers agree.",
		L"주의: Notepad++ 스마트 강조는 계속 단어 단위로만 강조합니다. 설정 > 환경설정 > 강조 표시 > 스마트 강조의 "
		L"\"모든 단어 일치\" 도 꺼야 에디터 강조와 마커가 일치합니다." },
	{ Text::WholeWordWarningFind,
		L"Note: Notepad++ smart highlighting follows the Find dialog, which matches whole words only. Turn off "
		L"\"Match whole word only\" in the Find dialog too, so the editor and the markers agree.",
		L"주의: Notepad++ 스마트 강조는 찾기 대화상자 설정을 따르며 단어 단위로만 강조합니다. 찾기 대화상자의 "
		L"\"단어 완전 일치\" 도 꺼야 에디터 강조와 마커가 일치합니다." },

	{ Text::ChangeHistory, L"Change history", L"변경 내역" },
	{ Text::HistoryModified, L"Modified", L"수정됨" },
	{ Text::HistorySaved, L"Saved", L"저장됨" },
	{ Text::HistoryReverted, L"Reverted", L"원래대로" },
	{ Text::HistoryRevertedModified, L"Back to modified", L"수정본으로 되돌림" },
	{ Text::Bookmarks, L"Bookmarks", L"북마크" },
	{ Text::FindMarks, L"Find mark results", L"찾기 표시 결과" },
	{ Text::StyleTokens, L"Style tokens", L"스타일 토큰" },
	{ Text::OtherIndicators, L"Other indicators", L"기타 인디케이터" },
	{ Text::IndicatorNumbers, L"Numbers:", L"번호:" },
	{ Text::IndicatorNumbersHint, L"Scintilla indicator numbers, e.g. 9, 19", L"Scintilla 인디케이터 번호, 예: 9, 19" },

	{ Text::ShowCaretLine, L"Show a &line at the caret position", L"커서 위치에 선 표시(&L)" },
	{ Text::Color, L"Color:", L"색:" },
	{ Text::Thickness, L"Thickness:", L"두께:" },
	{ Text::Position, L"Position:", L"위치:" },
	{ Text::Width, L"Width:", L"폭:" },

	{ Text::MoveCaretOnClick, L"Clicking a marker also moves the caret there", L"마커를 클릭하면 커서도 그 위치로 이동" },
	{ Text::CenterOnClick, L"Center the line on click", L"클릭한 줄을 화면 가운데로" },
	{ Text::FlashLine, L"Briefly highlight the line after a click", L"클릭으로 이동한 줄을 잠깐 강조" },
	{ Text::Duration, L"Duration:", L"시간:" },
	{ Text::ScrollOnEmptyClick, L"Clicking an empty spot scrolls there", L"빈 곳을 클릭하면 그 지점으로 스크롤" },
	{ Text::JumpGroup, L"Previous / next occurrence", L"이전·다음 위치로 이동" },
	{ Text::WrapAround, L"Continue from the other end of the document", L"문서 끝에 닿으면 반대쪽 끝에서 이어서 이동" },
	{ Text::CenterOnJump, L"Center the line after moving", L"이동한 줄을 화면 가운데로" },
	{ Text::ShortcutNote,
		L"Assign keys in Settings > Shortcut Mapper > Plugin commands. No keys are assigned by default.",
		L"단축키는 설정 > 단축키 관리자 > 플러그인 명령에서 지정합니다. 기본으로 지정된 키는 없습니다." },

	{ Text::CaptionLine, L"Line", L"줄" },
	{ Text::KindOccurrence, L"Occurrence", L"같은 단어" },
	{ Text::KindBookmark, L"Bookmark", L"북마크" },
	{ Text::KindFindMark, L"Find mark", L"찾기 표시" },
	{ Text::KindStyleToken, L"Style token", L"스타일 토큰" },
	{ Text::KindIndicator, L"Indicator", L"인디케이터" },
};

bool g_korean = false;
const wchar_t* g_table[2][static_cast<size_t>(Text::Count)] = {};

void buildTable() {
	static bool built = false;
	if (built)
		return;
	for (const Entry& entry : kEntries) {
		g_table[0][static_cast<size_t>(entry.id)] = entry.english;
		g_table[1][static_cast<size_t>(entry.id)] = entry.korean;
	}
	built = true;
}

std::wstring lower(std::wstring value) {
	for (wchar_t& c : value)
		c = static_cast<wchar_t>(std::towlower(c));
	return value;
}

} // namespace

namespace Strings {

void setKorean(bool korean) {
	g_korean = korean;
}

bool korean() {
	return g_korean;
}

const wchar_t* get(Text text) {
	buildTable();
	const wchar_t* value = g_table[g_korean ? 1 : 0][static_cast<size_t>(text)];
	if (!value)
		value = g_table[0][static_cast<size_t>(text)];
	return value ? value : L"";
}

bool notepadIsKorean(HWND npp) {
	// Notepad++ 8.6.8+ reports the language file once it is running
	const int length = static_cast<int>(::SendMessage(npp, NPPM_GETNATIVELANGFILENAME, 0, 0));
	if (length > 0) {
		std::string name(static_cast<size_t>(length) + 1, '\0');
		::SendMessage(npp, NPPM_GETNATIVELANGFILENAME, name.size(), reinterpret_cast<LPARAM>(name.data()));
		return name.find("korean") != std::string::npos;
	}

	// Earlier, or while Notepad++ is still starting: nativeLang.xml in the settings
	// directory is a copy of the active language file.
	const NppPreferences preferences = NppPreferences::load(npp);
	if (preferences.configFile.empty())
		return false;
	std::wstring path = preferences.configFile;
	path.replace(path.size() - wcslen(L"config.xml"), wcslen(L"config.xml"), L"nativeLang.xml");
	HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return false;
	char buffer[2048] = {};
	DWORD read = 0;
	::ReadFile(file, buffer, sizeof(buffer) - 1, &read, nullptr);
	::CloseHandle(file);
	const std::string head(buffer, read);
	const size_t tag = head.find("<Native-Langue");
	if (tag == std::string::npos)
		return false;
	const std::string element = head.substr(tag, head.find('>', tag) - tag);
	std::wstring wide(element.begin(), element.end());
	return lower(wide).find(L"korean") != std::wstring::npos;
}

} // namespace Strings
