# 0001. Project Overview and Architecture

## Project: FastPad v1
**Status**: Core implementation complete  
**Date**: April 4, 2026  
**Repository**: https://github.com/mcpe500/fastpad

## What is FastPad

FastPad is a tiny, fast, native Windows text editor written in C using raw Win32 API.

### Product Goals
- Instant startup (< 1 second)
- Low memory usage (proportional to file size)
- Small binary (32 KB achieved)
- Responsive editing (no typing lag)
- Simple, maintainable code

### What FastPad is NOT
- Not an IDE
- Not a browser-based editor
- Not a plugin platform
- Not a rich text editor
- Not a collaboration tool

## Architecture Overview

### Module Structure

```
src/
├── main.c          - Entry point (WinMain, message loop)
├── app.c           - Application state, window lifecycle, menus
├── editor.c        - Caret, selection, editing, undo/redo
├── buffer.c        - Gap buffer text storage
├── fileio.c        - File I/O, UTF-8 handling, dialogs
├── render.c        - GDI text rendering
└── search.c        - Find text functionality
```

### Key Data Structures

#### GapBuffer (buffer.c)
- Efficient text storage for local editing
- Gap moves to insertion/deletion point
- O(1) insertion/deletion near caret
- Current capacity: starts at 4KB, grows 2x

#### Editor (editor.c)
- Manages caret position
- Selection range (start/end positions)
- Undo/Redo history (bounded: 1000 ops max)
- Viewport state (scroll position, visible area)

#### App (app.c)
- Top-level window management
- Menu bar (File, Edit, View, Help)
- Status bar (line/column, modified state)
- File lifecycle (new, open, save, save as)

### Rendering Strategy
- GDI-based rendering
- Single monospace font (Consolas, fallback: system fixed)
- Renders only visible lines
- Selection painting with blue background
- White text on selected regions

### Memory Strategy
- No background threads
- No caches
- Allocate on demand
- Free unused resources
- Gap buffer grows/shrinks as needed

## Build System

### Toolchain
- **Cross-compile**: `x86_64-w64-mingw32-clang` (llvm-mingw on Termux)
- **Alternative**: MSVC or MinGW-w64 on Windows
- **Build command**: `make`
- **Output**: `build/FastPad.exe` (32 KB)

### Build Command
```bash
x86_64-w64-mingw32-clang \
  -Os -s -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -mwindows \
  src/main.c src/app.c src/editor.c src/buffer.c \
  src/fileio.c src/render.c src/search.c \
  -o build/FastPad.exe \
  -luser32 -lgdi32 -lcomdlg32 -lkernel32 -lshell32
```

## Current Feature Status

### ✅ Implemented (v1 Complete)
- [x] Create/edit plain text
- [x] Open files (UTF-8 with BOM support)
- [x] Save files
- [x] Save As
- [x] Text insertion/deletion
- [x] Caret movement (arrows, Home/End, Ctrl+Home/End, PageUp/Down)
- [x] Mouse caret placement
- [x] Text selection (keyboard + mouse drag)
- [x] Copy/Cut/Paste (with UTF-8/UTF-16 conversion)
- [x] Undo/Redo (bounded history, 1000 ops)
- [x] Find (search dialog with match case)
- [x] Status bar (line, column, modified state)
- [x] All keyboard shortcuts (see README.md)
- [x] Unsaved changes prompt
- [x] Window title with filename and modified marker
- [x] Menu bar (File, Edit, View, Help)

### 🚧 Nice-to-Have (Not Yet Implemented)
- [ ] Ctrl+G (Go to line)
- [ ] Recent files
- [ ] Word wrap (menu item exists, not functional)
- [ ] Show/hide status bar (menu item exists, functional)
- [ ] Large file warning
- [ ] Read-only detection

### ❌ Out of Scope (v1)
- Tabs
- Plugins
- Syntax highlighting
- File tree/sidebar
- Minimap
- Autosave daemon
- Telemetry
- Collaboration
- Markdown preview
- Rich text editing

## File Handling

### Supported
- Plain text files
- UTF-8 files (with or without BOM)
- Empty files
- Large files (limited by available memory)

### Line Endings
- Internally: LF (`\n`)
- On disk: CRLF (`\r\n`) with UTF-8 BOM
- Conversion happens on load/save

### Encoding
- Read: UTF-8 (BOM detection and skip)
- Write: UTF-8 with BOM
- Clipboard: UTF-16 (Windows native)

## Known Issues / Limitations

1. **Word wrap**: Menu item exists but not functional
2. **Horizontal scrolling**: Basic implementation, may need refinement
3. **Large files**: No specific optimization yet, may be slow on very large files
4. **Find dialog**: Simple implementation, could be improved with proper modal dialog
5. **Font selection**: Hardcoded to Consolas, no user configuration yet
6. **No syntax highlighting**: By design (plain text editor)

## Next Steps (if continuing development)

See `0004-next-steps-and-improvements.md` for detailed recommendations.

## Quick Start for New Developers

1. Read `spec/001-spec.md` for requirements
2. Read `spec/program/program1.md` for implementation rules
3. Review source files in order:
   - `src/types.h` - Core data structures
   - `src/main.c` - Entry point
   - `src/app.c` - Window and app lifecycle
   - `src/buffer.c` - Text storage (gap buffer)
   - `src/editor.c` - Editing logic
   - `src/render.c` - Display
   - `src/fileio.c` - File operations
   - `src/search.c` - Find functionality

4. Build: `make`
5. Test: Run on Windows or Wine
