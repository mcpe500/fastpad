# FastPad Specification

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

- extension marketplace
- rich text editing
- web rendering

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

## Core Features (v1.0+)

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
11. **Undo/Redo** (bounded history)
12. **Vertical scrolling**
13. **Horizontal scrolling** (when word wrap disabled)
14. **Find dialog** (modeless, with find next/previous)
15. **Status bar** showing:
    - current line and column
    - modified state
    - file encoding
    - line ending style
16. **UTF-8 file I/O** with encoding detection

## Multi-Tab Editing (v1.1+)

FastPad v1.1 added basic multi-tab support:

- **New tab**: Ctrl+T or File > New Tab
- **Close tab**: Ctrl+W or File > Close Tab
- **Switch tabs**: Ctrl+Tab / Ctrl+Shift+Tab
- **Tab bar**: Shows all open tabs at top of editor area
- **Unsaved changes handling**: Prompts to save all tabs on close
- **Shared editor surface**: All tabs share the main window's editor surface for simplicity
- **Proper shutdown**: Clean exit without hanging, even with unsaved changes

## Extended Features (v1.2+)

The following features were added in v1.2 and later:

### Split View
- **Split horizontal**: Ctrl+Shift+H or View > Split Horizontal
- **Split vertical**: Ctrl+Shift+V or View > Split Vertical
- **Close split**: Ctrl+Shift+W or View > Close Split
- **Cycle focus**: F6 cycles between split panes

### Syntax Highlighting
FastPad supports syntax highlighting for common file types:

| Language   | Extensions        |
|------------|-------------------|
| C          | .c, .h            |
| C++        | .cpp, .cc, .cxx   |
| Java       | .java             |
| JavaScript | .js               |
| Python     | .py               |
| HTML       | .html, .htm       |
| CSS        | .css              |
| XML        | .xml              |
| JSON       | .json             |
| YAML       | .yml, .yaml       |
| SQL        | .sql              |
| Markdown   | .md               |
| INI        | .ini, .cfg        |

Colors follow a theme (see Theme System below).

### Theme System
FastPad supports color themes for the editor interface.

**Built-in themes:**
- **Dark**: Dark background with light text
- **Light**: Light background with dark text (default)
- **High Contrast**: Maximum contrast for accessibility

**Theme configuration:**
- Editor background and text colors
- Selection background and text colors
- Caret color
- Line number margin colors
- Tab colors (active/inactive)
- Status bar colors
- Menu colors

### Session Management
- **Auto-save**: Automatic backup of unsaved changes to temp files
- **Session restore**: On startup, reopens previously open files
- **Session data stored**: Per-tab cursor position, scroll position, filename

### Recent Files
- Tracks recently opened files (up to 20)
- Accessible via File > Recent submenu
- Persisted across sessions

### Zoom
- **Zoom in**: Ctrl+Plus or Ctrl+Mouse wheel up
- **Zoom out**: Ctrl+Minus or Ctrl+Mouse wheel down
- **Reset zoom**: Ctrl+0
- Range: 50% to 200%

### Custom Shortcuts
Users can configure keyboard shortcuts for menu actions:
- Stored in settings file as JSON
- Import/export via Settings menu

### Backup System
- **Auto-backup**: Creates timestamped backup copies of files before saving
- **Version history**: Keeps up to 10 backup versions per file
- **Restore**: Choose which version to restore from
- Backup directory: `%APPDATA%\FastPad\backups\`

### Plugin System
FastPad has a plugin system for extensibility:

**Plugin structure:**
- Each plugin in its own directory under `plugins/`
- `manifest.json` describes the plugin
- Plugins can register hooks for events

**Supported hooks:**
- `HOOK_FILE_OPEN` - Called when a file is opened
- `HOOK_FILE_SAVE` - Called when a file is saved
- `HOOK_FILE_CLOSE` - Called when a file is closed
- `HOOK_EDITOR_INIT` - Called when editor is initialized
- `HOOK_EDITOR_KEYDOWN` - Called on keydown in editor
- `HOOK_EDITOR_CHAR` - Called on char input in editor
- `HOOK_MENU_BUILD` - Called when menu is being built
- `HOOK_APP_INIT` - Called when app initializes
- `HOOK_APP_EXIT` - Called when app exits

### Settings Export/Import
All settings can be exported to JSON and imported:
- Theme settings
- Font settings
- Window position and size
- Keyboard shortcuts
- Recent files list
- Tab configuration

### JSON Tools
Under Tools menu:
- **Format JSON**: Pretty-print JSON content
- **Minify JSON**: Remove whitespace from JSON
- **Validate JSON**: Check if content is valid JSON

### File Encoding Support
- UTF-8 (without BOM)
- UTF-8 with BOM
- ANSI (system default)
- UTF-16 LE/BE

### Line Ending Support
- Windows (CRLF)
- Unix (LF)
- Old Mac (CR)

## Nice-to-Have Features

Allowed if they remain simple:

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
- text storage (gap buffer implementation)
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
- JSON formatting/minifying/validation

### `tab_manager.c`
- tab creation, deletion, and switching
- tab control (SysTabControl32) management
- unsaved changes detection across all tabs
- shutdown-aware tab closing to prevent hangs

### `theme.c`
- theme definitions and color management
- built-in theme implementations

### `backup.c`
- automatic backup creation
- version history management
- backup restore functionality

### `plugin.c`
- plugin loading and initialization
- plugin hook management
- manifest parsing

### `settings.c`
- settings export to JSON
- settings import from JSON
- individual settings accessors

## Text Storage

**Gap buffer** implementation for efficient text editing.

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
- Recent Files (submenu)
- Encoding (submenu: UTF-8, UTF-8 BOM, ANSI, UTF-16)
- Line Ending (submenu: Windows CRLF, Unix LF, Old Mac CR)
- Exit

### Edit
- Undo
- Redo
- Cut
- Copy
- Paste
- Select All
- Find
- Find Next
- Replace

### View
- Word Wrap
- Status Bar
- Line Numbers
- Zoom In
- Zoom Out
- Zoom Reset
- Split Horizontal
- Split Vertical
- Close Split
- Syntax Highlighting

### Tools
- Format JSON
- Minify JSON
- Validate JSON
- Create Backup
- Restore Backup
- Plugins

### Settings
- Customize Shortcuts
- Export Settings
- Import Settings

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
- Ctrl+H = Replace
- Ctrl+A = Select All
- Ctrl+Z = Undo
- Ctrl+Y = Redo
- Ctrl+X = Cut
- Ctrl+C = Copy
- Ctrl+V = Paste
- Ctrl+Plus = Zoom In
- Ctrl+Minus = Zoom Out
- Ctrl+0 = Reset Zoom
- F6 = Cycle split focus

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
- horizontal scrolling when word wrap is disabled

### Selection Rendering

Selection must be drawn properly to avoid visual artifacts:

- lines are drawn in 3 segments when partially selected: before selection (normal color), selected portion (white on blue), after selection (normal color)
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

## Bug Tracking

Known bugs and their fixes are documented in `spec/BUG_FIXES.md`.

When fixing bugs:
1. Document the bug in BUG_FIXES.md with symptoms, root cause, and fix
2. Add FIX comment in code at the bug location
3. Verify fix compiles and passes tests
4. Update version history in BUG_FIXES.md

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

## File Structure

```
fastpad/
├── src/
│   ├── main.c          # Entry point
│   ├── app.c           # Application state and window management
│   ├── app.h
│   ├── editor.c        # Editor operations (caret, selection, input)
│   ├── editor.h
│   ├── buffer.c        # Gap buffer implementation
│   ├── buffer.h
│   ├── render.c        # GDI rendering
│   ├── render.h
│   ├── fileio.c        # File I/O with encoding support
│   ├── fileio.h
│   ├── search.c        # Find/replace dialogs and JSON tools
│   ├── search.h
│   ├── tab_manager.c   # Multi-tab management
│   ├── tab_manager.h
│   ├── theme.c         # Theme definitions
│   ├── theme.h
│   ├── backup.c        # Backup system
│   ├── backup.h
│   ├── plugin.c        # Plugin system
│   ├── plugin.h
│   ├── settings.c      # Settings export/import
│   ├── settings.h
│   ├── types.h         # Main type definitions
│   ├── core_types.h   # Core types (GapBuffer, Selection, etc.)
│   ├── log.c          # Logging (dev builds only)
│   ├── log.h
│   ├── errors.h       # Error messages
│   └── Makefile       # Build system
├── spec/
│   ├── 001-spec.md     # This file
│   └── BUG_FIXES.md    # Bug tracking
├── plugins/            # Plugin directories
├── FastPad.exe         # Build output
└── README.md
```

## Version History

- **v1.2.x** - Extended feature set
  - Split view support
  - Syntax highlighting for 14 languages
  - Theme system with dark/light modes
  - Plugin system with 9 hook types
  - Backup system with version history
  - Settings export/import (JSON)
  - Session management and auto-save
  - Recent files tracking
  - Zoom (50-200%)
  - Custom keyboard shortcuts
  - JSON formatting/minifying/validation
  - File encoding detection
  - Multiple line ending styles
  - Bug fixes from BUG_FIXES.md

- **v1.1.x** - Tab management added
  - Multi-tab support introduced
  - Various stability issues from sharing editor surface

- **v1.0.x** - Initial release
  - Basic text editing
  - File open/save
  - Undo/redo