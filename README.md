# ScrollMarks

A Notepad++ plugin that marks every occurrence of the selected text next to
the vertical scrollbar. Click a marker to jump there, hover it to preview the
lines around it.

[한국어](README.ko.md)

![Markers next to the scrollbar and the hover preview](docs/images/preview.png)

## Features

- **Occurrence markers.** Select a word and every occurrence in the whole
  document gets a marker in a thin bar on the right of the scrollbar. The
  occurrence you selected has its own color and spans the full width of the
  bar, so you always see where you are.
- **Markers line up with the scrollbar.** A marker sits where the top of the
  scrollbar thumb is when its line is at the top of the screen. The marker of
  every line on screen is therefore always inside the thumb, at any zoom level,
  with folded code, with *Enable scrolling beyond last line*, and in long
  documents where Windows enlarges the thumb to its minimum size.
- **Marker size follows the document.** A marker is as tall as one line on
  the scrollbar track, never thinner than a minimum you choose. Short
  documents get tall markers, long documents get thin ones that stay apart.
- **Click to go there.** Clicking a marker selects that occurrence and
  centers its line. Folded lines are unfolded. Clicking an empty spot scrolls
  to that spot.
- **Hover to preview.** Hovering a marker shows the line and three lines above
  and below, with the editor's own fonts, syntax colors and highlights, plus
  the line number and *n of m*.
- **Follows Notepad++.** Match case, whole word, smart highlighting on/off,
  the large file restriction, colors, dark mode and the hover delay all come
  from Notepad++ and Windows unless you change them.

## Lightweight by design

ScrollMarks is written in C++ against the Notepad++ and Scintilla APIs. It
does not use a script engine, .NET or any extra runtime.

- The search runs in slices of a few milliseconds, so typing and scrolling
  never wait for it. With every line of a 21 MB, 400,000 line file matching,
  Notepad++ answered within 13 ms at all times in testing.
- Nothing is searched or drawn while you scroll. Scrolling only moves the
  scrollbar thumb, the markers stay where they are.
- Edits re-run the search only after a 250 ms pause in typing.
- The number of markers is capped (100,000 by default) to keep memory small.
- The search target and flags of Notepad++ are restored after every slice, so
  other features and plugins are not affected.

## Installation

### Plugin Admin

Once ScrollMarks is listed: **Plugins > Plugins Admin**, search for
*ScrollMarks*, tick it and press **Install**.

### Manual

1. Download the zip for your Notepad++ from the
   [releases](https://github.com/Hong-Seungmin/ScrollMarks/releases):
   `x64` for 64-bit, `x86` for 32-bit, `arm64` for ARM.
   **?** > **About Notepad++** shows which one you have.
2. Close Notepad++.
3. Extract the zip into `plugins\ScrollMarks\` in the Notepad++ folder, so
   that you get `plugins\ScrollMarks\ScrollMarks.dll`.
4. Start Notepad++.

Notepad++ 8.3 or later is required (64-bit Scintilla positions). Tested with
Notepad++ 8.9.3 on Windows 11.

## Usage

- Select a word, or double-click it. Markers appear at once.
- **Plugins > ScrollMarks > Show Markers** turns the bar on and off.
- **Plugins > ScrollMarks > Settings...** opens the settings.

## Settings

Settings are stored in `plugins\Config\ScrollMarks.ini`. *auto* means the value
comes from Notepad++ or Windows.

| Setting | Default | auto means |
|---|---|---|
| Show markers | on | |
| Only while Notepad++ smart highlighting is enabled | on | *Preferences > Highlighting > Smart Highlighting* |
| Match case | auto | the smart highlighting option, or the Find dialog when *Use Find dialog settings* is on |
| Whole word only | auto | same as above |
| Minimum length | 1 character | |
| Maximum markers | 100,000 | 0 removes the limit |
| Mark the word at the caret when nothing is selected | off | |
| Mark large files even when Notepad++ restricts smart highlighting | off | *Preferences > Performance > Large File Restriction* |
| Bar width | auto | half the width of the scrollbar |
| Minimum marker height | 2 px | |
| Occurrence color | auto | the smart highlighting color of the current theme |
| Current occurrence color | auto | the caret color of the current theme |
| Background color | auto | the Notepad++ dark mode background, or the Windows button face color |
| Clicking a marker selects the occurrence | on | |
| Clicking a marker centers its line | on | |
| Clicking an empty spot scrolls there | on | |
| Show a preview when hovering a marker | on | |
| Context lines | 3 | lines above and below |
| Hover delay | auto | the Windows mouse hover time |
| Preview width | 60 % | of the editor width |

Pixel values are at 100 % scaling and grow with the display scaling.

Notepad++ saves its preferences when it exits. Changes in the Notepad++
Preferences dialog therefore reach ScrollMarks after a restart of Notepad++.

## Notes

- The bar takes a few pixels from the right of the text area, next to the
  scrollbar. Other plugins that draw a bar there show up next to
  it. Turn one of them off if you only want one.
- With word wrap, the marker of an occurrence in a wrapped line is drawn at
  the first row of that line.
- Multiple and rectangular selections are not marked.

## Building

Requirements: Visual Studio 2022 or its Build Tools with the C++ workload
(toolset v143) and a Windows 10/11 SDK.

```
msbuild ScrollMarks.vcxproj -p:Configuration=Release -p:Platform=x64
msbuild ScrollMarks.vcxproj -p:Configuration=Release -p:Platform=Win32
msbuild ScrollMarks.vcxproj -p:Configuration=Release -p:Platform=ARM64
pwsh scripts/package.ps1
```

The DLLs go to `bin\<platform>\Release\`, the Plugin Admin zips to `dist\`.

Pushing a tag such as `v1.0.0` builds all three architectures on GitHub
Actions and creates a draft release with the zips and their SHA-256 hashes.
The tag must match the version in `src/Version.h`.

## License

GNU General Public License v3.0 or later. See [LICENSE](LICENSE).

The files in `npp/` come from the
[Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus) and
[Scintilla](https://www.scintilla.org/) projects under their own licenses.
