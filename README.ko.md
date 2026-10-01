# ScrollMarks

선택한 텍스트와 같은 텍스트가 문서 어디에 있는지 세로 스크롤바 옆에
표시하는 Notepad++ 플러그인입니다. 마커를 클릭하면 그 위치로 이동하고,
마우스를 올리면 주변 줄을 미리 볼 수 있습니다.

[English](README.md)

![스크롤바 옆 마커와 미리보기](docs/images/preview.png)

## 기능

- **위치 마커.** 단어를 선택하면 문서 전체에서 같은 단어마다 스크롤바
  오른쪽의 좁은 막대에 마커가 찍힙니다. 지금 선택한 위치는 다른 색으로
  막대 폭 전체에 그려져서 내 위치를 바로 알 수 있습니다.
- **스크롤바와 정확히 맞는 위치.** 마커는 그 줄이 화면 맨 위에 올 때의
  thumb 윗변 위치에 그려집니다. 그래서 화면에 보이는 줄의 마커는 항상
  thumb 안에 있습니다. 확대/축소, 접힌 코드, *마지막 줄 너머로 스크롤*
  설정, Windows 가 thumb 를 최소 크기로 키우는 긴 문서에서도 같습니다.
- **문서에 맞는 마커 두께.** 마커 높이는 스크롤바 트랙에서 한 줄이
  차지하는 높이이고, 설정한 최소값보다 얇아지지 않습니다. 짧은 문서는
  두껍게, 긴 문서는 얇고 서로 떨어지게 그려집니다.
- **클릭으로 이동.** 마커를 클릭하면 그 위치를 선택하고 줄을 화면 가운데로
  가져옵니다. 접힌 줄은 펼칩니다. 빈 곳을 클릭하면 그 지점으로
  스크롤합니다.
- **마우스오버 미리보기.** 마커에 마우스를 올리면 그 줄과 위아래 3줄을
  에디터와 같은 글꼴, 구문 색, 하이라이트로 보여 주고, 줄 번호와
  *n of m* 을 표시합니다.
- **Notepad++ 설정을 따름.** 대소문자 구분, 단어 단위, 스마트 하이라이트
  켜기/끄기, 대용량 파일 제한, 색상, 다크 모드, 마우스오버 지연 시간을
  바꾸지 않으면 Notepad++ 와 Windows 설정을 그대로 씁니다.

## 가벼움

ScrollMarks 는 Notepad++ 와 Scintilla API 만 쓰는 C++ 플러그인입니다.
스크립트 엔진, .NET, 별도 런타임을 쓰지 않습니다.

- 검색은 몇 밀리초 단위로 나눠 실행되어 입력과 스크롤을 기다리게 하지
  않습니다. 테스트에서 2,100만 바이트, 40만 줄 파일의 모든 줄이 일치하는
  경우에도 Notepad++ 는 항상 13ms 안에 응답했습니다.
- 스크롤할 때는 검색도 그리기도 하지 않습니다. 스크롤은 thumb 만 움직이고
  마커는 그대로입니다.
- 편집하면 입력이 250ms 멈춘 뒤에만 다시 검색합니다.
- 마커 수에 상한(기본 10만 개)이 있어 메모리를 적게 씁니다.
- 검색 후 Notepad++ 의 검색 대상과 플래그를 매번 원래대로 되돌리므로 다른
  기능과 플러그인에 영향을 주지 않습니다.

## 설치

### Plugin Admin

목록에 등록된 뒤에는 **플러그인 > 플러그인 관리자** 에서 *ScrollMarks* 를
찾아 체크하고 **설치** 를 누릅니다.

### 수동 설치

1. [릴리스](https://github.com/Hong-Seungmin/ScrollMarks/releases) 에서
   Notepad++ 에 맞는 zip 을 받습니다. 64비트는 `x64`, 32비트는 `x86`,
   ARM 은 `arm64` 입니다. **?** > **Notepad++ 정보** 에서 확인할 수 있습니다.
2. Notepad++ 를 종료합니다.
3. zip 을 Notepad++ 폴더의 `plugins\ScrollMarks\` 에 풀어
   `plugins\ScrollMarks\ScrollMarks.dll` 이 되게 합니다.
4. Notepad++ 를 실행합니다.

Notepad++ 8.3 이상이 필요합니다. Windows 11 의 Notepad++ 8.9.3 에서
테스트했습니다.

## 사용법

- 단어를 선택하거나 더블클릭하면 바로 마커가 나타납니다.
- **플러그인 > ScrollMarks > Show Markers** 로 막대를 켜고 끕니다.
- **플러그인 > ScrollMarks > Settings...** 에서 설정을 바꿉니다.

## 설정

설정은 `plugins\Config\ScrollMarks.ini` 에 저장됩니다. *auto* 는 Notepad++
나 Windows 의 값을 따른다는 뜻입니다.

| 설정 | 기본값 | auto 일 때 따르는 값 |
|---|---|---|
| 마커 표시 | 켬 | |
| Notepad++ 스마트 하이라이트가 켜져 있을 때만 | 켬 | *환경 설정 > 강조 > 스마트 강조* |
| 대소문자 구분 | auto | 스마트 강조 옵션, *찾기 대화 상자 설정 사용* 이 켜져 있으면 찾기 창 |
| 단어 단위 | auto | 위와 같음 |
| 최소 길이 | 1글자 | |
| 최대 마커 수 | 100,000 | 0 이면 제한 없음 |
| 선택이 없을 때 커서 위치의 단어 표시 | 끔 | |
| Notepad++ 가 제한하는 대용량 파일도 표시 | 끔 | *환경 설정 > 성능 > 대용량 파일 제한* |
| 막대 폭 | auto | 스크롤바 폭의 절반 |
| 마커 최소 높이 | 2px | |
| 같은 텍스트 마커 색 | auto | 현재 테마의 스마트 강조 색 |
| 선택한 위치 마커 색 | auto | 현재 테마의 커서 색 |
| 배경 색 | auto | Notepad++ 다크 모드 배경, 또는 Windows 버튼 면 색 |
| 마커 클릭 시 그 위치 선택 | 켬 | |
| 마커 클릭 시 줄을 가운데로 | 켬 | |
| 빈 곳 클릭 시 그 지점으로 스크롤 | 켬 | |
| 마우스오버 미리보기 | 켬 | |
| 미리보기 주변 줄 수 | 3 | 위아래 각각 |
| 마우스오버 지연 | auto | Windows 마우스 호버 시간 |
| 미리보기 폭 | 60% | 에디터 폭 기준 |

픽셀 값은 100% 배율 기준이며 디스플레이 배율에 맞춰 커집니다.

Notepad++ 는 종료할 때 환경 설정을 저장합니다. 그래서 Notepad++ 환경
설정에서 바꾼 값은 Notepad++ 를 다시 시작한 뒤 ScrollMarks 에 반영됩니다.

## 참고

- 막대는 스크롤바 옆, 텍스트 영역 오른쪽의 몇 픽셀을 씁니다. 같은 자리에 막대를
  그리는 다른 플러그인이 있으면 나란히 보입니다. 하나만 원하면 한쪽을 끄세요.
- 자동 줄바꿈 상태에서 줄바꿈된 줄의 마커는 그 줄의 첫 행 위치에
  그려집니다.
- 다중 선택과 사각형 선택은 표시하지 않습니다.

## 빌드

C++ 워크로드(v143 툴셋)가 포함된 Visual Studio 2022 또는 Build Tools 와
Windows 10/11 SDK 가 필요합니다.

```
msbuild ScrollMarks.vcxproj -p:Configuration=Release -p:Platform=x64
msbuild ScrollMarks.vcxproj -p:Configuration=Release -p:Platform=Win32
msbuild ScrollMarks.vcxproj -p:Configuration=Release -p:Platform=ARM64
pwsh scripts/package.ps1
```

DLL 은 `bin\<platform>\Release\`, Plugin Admin 용 zip 은 `dist\` 에
만들어집니다.

`v1.0.0` 같은 태그를 push 하면 GitHub Actions 가 세 아키텍처를 빌드하고
zip 과 SHA-256 해시가 담긴 draft 릴리스를 만듭니다. 태그는
`src/Version.h` 의 버전과 같아야 합니다.

## 라이선스

GNU General Public License v3.0 이상. [LICENSE](LICENSE) 를 참고하세요.

`npp/` 의 파일은 [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus)
와 [Scintilla](https://www.scintilla.org/) 프로젝트의 것으로 각자의
라이선스를 따릅니다.
