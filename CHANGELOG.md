# Changelog

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
