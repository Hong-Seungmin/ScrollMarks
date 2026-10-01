# Changelog

## 1.1.0

- Markers for change history, bookmarks, find mark results, the five style
  tokens and any other Scintilla indicator numbers.
- A caret line marker.
- Every kind of marker has its own switch, color, position (left, center,
  right) and width. Higher priorities are drawn on top and narrower, so
  overlapping markers stay visible.
- Clicking a marker scrolls there and briefly highlights the line. The caret
  only moves when *Clicking a marker also moves the caret there* is on.
- Previous Occurrence and Next Occurrence commands, without default keys.
- Hovering any kind of marker shows the preview, with the kind of marker in
  the caption. Every occurrence of the searched text in the preview is
  highlighted, also in lines Notepad++ has not highlighted because they are
  off screen.
- Automatic colors: a palette with light and dark variants, and theme colors
  adjusted for contrast with the bar.
- Tabbed settings dialog with the hover preview settings and a live sample of
  the bar below the tabs.
- English and Korean user interface, following the Notepad++ language.
- A note on the Occurrences tab when *Whole word only* is off here but still on
  in Notepad++, which would make the editor highlight fewer places than the bar.
- The automatic bar width is now the scrollbar width, to make room for lanes.

## 1.0.0

First release.

- Markers for every occurrence of the selected text in a bar next to the
  vertical scrollbar, with a separate color for the selected occurrence.
- Marker positions follow the scrollbar thumb, including folding, zoom,
  scrolling beyond the last line and the minimum thumb size.
- Click a marker to select the occurrence and center its line. Click an empty
  spot to scroll there.
- Hover a marker to preview the surrounding lines with the editor's styles.
- Defaults follow Notepad++: smart highlighting options, large file
  restriction, theme colors and dark mode.
- Settings dialog, stored in `plugins\Config\ScrollMarks.ini`.
- Builds for x64, x86 and ARM64.
