# FastPad Spec

## Overview

FastPad is a tiny, native Windows text editor built in C with the raw Win32 API.

The goal is not to compete with VS Code, Notepad++, or modern IDEs. The goal is to build a text editor that:

- starts instantly
- feels snappy while typing
- uses very little memory
- has a tiny binary
- is simple enough to understand and maintain

FastPad should feel better than a bloated editor because it does less, not because it does more.

## Product Goals

1. **Native and small**
   - No Electron
   - No browser engine
   - No .NET runtime requirement
   - No large third-party GUI toolkit

2. **Fast startup**
   - Window should appear effectively instantly on a normal machine

3. **Low idle memory**
   - Minimize private working set and allocations
   - "Under 1 MB total process memory" is not a hard requirement because modern Windows process overhead makes that unrealistic
   - Instead target:
     - tiny executable size
     - very low private memory
     - no useless background work

4. **Good editing feel**
   - No typing lag
   - Smooth cursor movement
   - Clean redraw behavior
   - Fast open/save for normal text files

5. **Simplicity first**
   - Prefer fewer features with better quality
   - Avoid complexity that does not clearly improve responsiveness or usability

## Non-Goals

FastPad is **not** trying to include:

- plugins
- syntax highlighting
- file tree/sidebar
- minimap
- autosave daemon
- telemetry
- collaboration
- markdown preview
- rich text editing
- web rendering
- extension marketplace

These may be considered later, but are explicitly out of scope for v1.

**Note:** Tab support was added in v1.1 for basic multi-file editing, but advanced tab features remain out of scope.

## Target Platform

Primary target:

- Windows 10/11 x64

Build environment may be:

- Windows native build using MSVC or clang
- Linux cross-compile using `mingw-w64`

## Language and Tooling

- Language: **C**
- UI: **raw Win32 API**
- Rendering: **GDI** for v1
- Build: simple Makefile or shell script
- No new package dependencies

## Core Features for v1

FastPad v1 must support:

1. **Create and edit plain text**
2. **Open existing text files**
3. **Save current file**
4. **Save As**
5. **Basic keyboard input**
6. **Arrow key navigation**
7. **Backspace/Delete**
8. **Mouse caret placement**
9. **Text selection**
10. **Copy/Cut/Paste**
11. **Undo/Redo** (single-stack or simple bounded history is acceptable)
12. **Vertical scrolling**
13. **Find dialog**
14. **Status bar**
    - line
    - column
    - modified state
15. **UTF-8 file I/O**
    - internally UTF-8 or UTF-16 is acceptable
    - file format on disk should support UTF-8 cleanly

## Multi-Tab Editing (v1.1+)

FastPad v1.1 added basic multi-tab support:

- **New tab**: Ctrl+T or File > New Tab
- **Close tab**: Ctrl+W or File > Close Tab
- **Switch tabs**: Ctrl+Tab / Ctrl+Shift+Tab
- **Tab bar**: Shows all open tabs at top of editor area
- **Unsaved changes handling**: Prompts to save all tabs on close
- **Shared editor surface**: All tabs share the main window's editor surface for simplicity
- **Proper shutdown**: Clean exit without hanging, even with unsaved changes

## Nice-to-Have Features

Allowed if they remain simple:

- Ctrl+G go to line
- recent files
- word wrap toggle
- show/hide status bar
- large file warning
- read-only detection

Do not add these until core editing is solid.

## Memory and Performance Targets

These are targets, not absolute promises.

### Startup
- The app should open in well under one second on a normal machine

### Typing
- No visible lag during normal typing in medium-size text files

### File size
- Prefer executable size under a few hundred KB stripped
- Under 1 MB is desirable for the binary if practical

### Memory
- Keep memory low and proportional to file size
- Avoid large caches and heavyweight structures
- Avoid background threads unless clearly necessary

### Redraw efficiency
- Redraw only what changed when practical
- Render only visible lines

## Architecture

## High-Level Modules

### `main.c`
- application entry point
- WinMain
- message loop
- top-level initialization

### `app.c`
- global app state
- menu commands
- window lifecycle
- file/open/save workflow

### `editor.c`
- caret logic
- selection logic
- keyboard and mouse editing behavior
- viewport state

### `buffer.c`
- text storage
- insertion/deletion
- line indexing helpers
- file-size-sensitive operations

### `render.c`
- font setup
- measuring text
- line rendering
- selection painting
- caret rendering coordination

### `fileio.c`
- open/save/load
- UTF-8 decoding/encoding
- line ending normalization

### `search.c`
- find text
- next/previous match
- modeless find dialog with proper focus handling

### `tab_manager.c`
- tab creation, deletion, and switching
- tab control (SysTabControl32) management
- unsaved changes detection across all tabs
- shutdown-aware tab closing to prevent hangs

## Text Storage

Preferred choices:

1. **Gap buffer** for simplest implementation
2. **Piece table** if undo/edit behavior becomes cleaner

For v1, **gap buffer is preferred** unless the implementation becomes awkward.

Requirements for the text buffer:

- efficient insertion/deletion near caret
- support for large enough files for normal note-taking and coding
- line-based navigation helpers
- no quadratic behavior during normal use

## UI Layout

Main window contains:

- menu bar
- tab bar (shows all open tabs)
- editor client area (shared by all tabs)
- optional status bar

No ribbon, no custom chrome, no unnecessary panels.

### Tab Bar Behavior

- Tab bar is positioned at the top of the client area
- All tabs share the same editor surface (child window per tab is deferred to future versions)
- Tab switching updates focus, caret, and redraw the editor area
- Tab bar shows tab titles and allows clicking to switch tabs

## Menus

### File
- New
- New Tab
- Close Tab
- Open
- Save
- Save As
- Exit

### Edit
- Undo
- Redo
- Cut
- Copy
- Paste
- Select All
- Find

### View
- Word Wrap
- Status Bar

### Help
- About

## Input and Editing Behavior

### Keyboard shortcuts
- Ctrl+N = New
- Ctrl+O = Open
- Ctrl+S = Save
- Ctrl+Shift+S = Save As
- Ctrl+T = New Tab
- Ctrl+W = Close Tab
- Ctrl+Tab = Next Tab
- Ctrl+Shift+Tab = Previous Tab
- Ctrl+F = Find
- Ctrl+A = Select All
- Ctrl+Z = Undo
- Ctrl+Y = Redo
- Ctrl+X = Cut
- Ctrl+C = Copy
- Ctrl+V = Paste

### Navigation
- arrows move caret
- Home/End move within line
- Ctrl+Home/Ctrl+End go to file start/end
- PageUp/PageDown scroll and move caret
- mouse click places caret
- drag selects text

### Line endings
- normalize internally
- preserve or consistently write `\n`/CRLF based on policy chosen for v1
- behavior must be documented in code comments

## File Handling

Supported:
- plain text files
- UTF-8 files
- empty files
- reasonably large text files

Not required:
- binary file editing
- rich text formats
- Word docs
- PDFs

Behavior:
- warn on unsaved changes before destructive actions
- update window title with filename and modified marker

## Error Handling

The app must fail simply and clearly.

Examples:
- open failure -> message box
- save failure -> message box
- invalid decode -> either best-effort fallback or clear error
- out-of-memory -> fail gracefully if practical

No silent data loss.

## Rendering Rules

- use a single monospace default font for v1
- keep rendering logic simple
- avoid flicker
- support resize correctly
- horizontal scrolling may be omitted in v1 if word wrap is enabled by default, but no hidden broken behavior

### Selection Rendering

Selection must be drawn properly to avoid visual artifacts:

- lines are drawn in 3 segments when partially selected: before selection (black), selected portion (white on blue), after selection (black)
- do NOT draw entire line in white color just because part of it is selected
- selection background is drawn first, then text on top
- this prevents ghost characters and ensures text remains readable during selection

## Code Quality Rules

- small files with clear responsibilities
- avoid macros unless useful
- avoid giant global mutable state blocks where possible
- comment the tricky parts, especially buffer logic
- prefer simple structs and functions over abstraction-heavy patterns
- no premature generalization

## Build and Distribution

The repo should support:

### Native Windows build
- MSVC or clang

### Linux -> Windows cross-build
- `mingw-w64`

The release artifact should be:

- a single `FastPad.exe`
- no runtime installer required if possible

## Suggested Build Command

Example Linux cross-compile command:

```sh
x86_64-w64-mingw32-gcc \
  -Os -s \
  -ffunction-sections -fdata-sections \
  -Wl,--gc-sections \
  -mwindows \
  src/main.c src/app.c src/editor.c src/buffer.c src/fileio.c src/render.c src/search.c \
  -o FastPad.exe \
  -luser32 -lgdi32 -lcomdlg32 -lkernel32 -lshell32
```
