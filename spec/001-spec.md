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

## Version History

| Version | Release Date | Features |
|---------|--------------|----------|
| v1.0.x | - | Initial release: basic text editing, file open/save, undo/redo |
| v1.1.x | - | Multi-tab support, tab bar, shared editor surface |
| v1.2.x | - | Bug fixes: buffer corruption, selection rendering, caret positioning |
| v1.3.0 | - | Theme support: 6 preset themes, custom font settings |
| v2.0.0 | - | Full feature release: auto-save, recent files, search+, line numbers, zoom, encoding, syntax highlighting, split view, backup, plugin system |

---

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

---

## Target Platform

Primary target:

- Windows 10/11 x64

Build environment may be:

- Windows native build using MSVC or clang
- Linux cross-compile using `mingw-w64`

---

## Language and Tooling

- Language: **C**
- UI: **raw Win32 API**
- Rendering: **GDI** for v1
- Build: simple Makefile or shell script
- No new package dependencies

---

## Core Features

### v1.0 - Basic Editing

FastPad v1.0 must support:

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
13. **Find dialog**
14. **Status bar** (line, column, modified state)
15. **UTF-8 file I/O**

### v1.1 - Multi-Tab Editing

- **New tab**: Ctrl+T or File > New Tab
- **Close tab**: Ctrl+W or File > Close Tab
- **Switch tabs**: Ctrl+Tab / Ctrl+Shift+Tab
- **Tab bar**: Shows all open tabs at top of editor area
- **Unsaved changes handling**: Prompts to save all tabs on close
- **Shared editor surface**: All tabs share the main window's editor surface for simplicity
- **Proper shutdown**: Clean exit without hanging, even with unsaved changes

### v1.3 - Theme Support

**Preset Themes (6):**
1. Classic Light - Traditional light background
2. Classic Dark - Traditional dark background
3. Monokai - Popular syntax theme
4. Solarized Light - Low-contrast light theme
5. Solarized Dark - Low-contrast dark theme
6. Dracula - Purple accent dark theme

**Font Settings:**
- Font family (system fonts)
- Font size (8-72)
- Font weight (regular, bold)

**Theme Persistence:**
- Settings stored in `%APPDATA%\FastPad\theme.json`

---

## v2.0 - Feature Pack

### Auto Save & Session Restore

**Auto Save:**
- Triggers every 3 seconds when buffer is dirty
- Saves to `%APPDATA%\FastPad\autosave\`
- Does not change the file's original path
- Status indicator in status bar: "Saved", "Unsaved", "Auto-saved"

**Session Restore:**
- On startup, restores tabs from last session
- Stores session data in `%APPDATA%\FastPad\session.json`
- Includes: open files, cursor positions, scroll positions, active tab

**Data Directory:**
- `%APPDATA%\FastPad\` - main config directory
  - `autosave\` - auto-saved temporary files
  - `session.json` - last session state
  - `theme.json` - theme settings
  - `shortcuts.json` - custom keyboard shortcuts
  - `recent_files.json` - recent files list

### Recent Files

**Features:**
- Menu: File > Recent Files
- Maximum 20 entries
- Pin/unpin favorite files
- Clear recent files option
- Reopen last session option
- Shows full path on hover

### Search and Replace (Enhanced)

**Find Dialog:**
- Find text input
- Match case toggle
- Whole word toggle
- Direction (up/down)
- Highlight all matches
- Count total matches
- Navigate: Find Next, Find Previous

**Replace Dialog:**
- Find and Replace text inputs
- Replace single occurrence
- Replace all occurrences
- Confirmation before Replace All

### Line Numbers

**Features:**
- Toggle via View > Line Numbers
- Displayed in gutter on left side
- Current line number highlighted
- Aligned with text content
- Updates on scroll and edit

### Unsaved Tab Indicator

**Features:**
- Tab title shows `*` prefix when buffer is dirty
- Tab title shows file name (or "Untitled" if new)
- Status bar shows "Unsaved" or "Saved" indicator

### Zoom In/Out

**Controls:**
- `Ctrl+Plus` - Zoom In (increase font size by 2pt)
- `Ctrl+Minus` - Zoom Out (decrease font size by 2pt)
- `Ctrl+0` - Reset Zoom (restore to 100%)
- Menu: View > Zoom submenu

**Zoom Range:**
- Minimum: 50% (zoom_level = 50)
- Maximum: 200% (zoom_level = 200)
- Default: 100% (zoom_level = 100)

**Behavior:**
- Zoom is temporary (session only)
- Affects only the editor font size
- Status bar shows current zoom level

### Custom Shortcuts

**Features:**
- Load/save shortcuts from `%APPDATA%\FastPad\shortcuts.json`
- Default shortcuts initialized on first run
- Menu: Settings > Shortcuts

**Default Shortcuts:**
| Shortcut | Action |
|----------|--------|
| Ctrl+N | New |
| Ctrl+O | Open |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+T | New Tab |
| Ctrl+W | Close Tab |
| Ctrl+Tab | Next Tab |
| Ctrl+Shift+Tab | Previous Tab |
| Ctrl+F | Find |
| Ctrl+H | Replace |
| Ctrl+A | Select All |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+Home | Go to Start |
| Ctrl+End | Go to End |
| Ctrl+= | Zoom In |
| Ctrl+- | Zoom Out |
| Ctrl+0 | Reset Zoom |

### Encoding Selector

**Supported Encodings:**
- UTF-8 (default)
- UTF-8 with BOM
- ANSI (system default code page)
- UTF-16 LE (Little Endian)
- UTF-16 BE (Big Endian)

**Menu:** File > Encoding

**Behavior:**
- Encoding is per-tab
- Changing encoding does not convert the file
- Status bar shows current encoding

### Line Ending Selector

**Supported Line Endings:**
- CRLF (Windows default: `\r\n`)
- LF (Unix default: `\n`)
- CR (Old Mac: `\r`)

**Menu:** File > Line Ending

**Behavior:**
- Line ending is per-tab
- Status bar shows current line ending type

### Enhanced Status Bar

**Information Displayed:**
```
Tab X/Y | Ln X, Col Y | Words: N | Chars: N | UTF-8 | CRLF | Saved
```

- **Tab X/Y**: Current tab number and total tabs
- **Ln X, Col Y**: Current line and column
- **Words: N**: Word count in current buffer
- **Chars: N**: Character count in current buffer
- **UTF-8/UTF-16/ANSI**: Current encoding
- **CRLF/LF/CR**: Current line ending
- **Saved/Unsaved/Auto-saved**: Save status

### Syntax Highlighting

**Supported Languages (12):**
1. JSON
2. XML
3. HTML
4. CSS
5. JavaScript
6. Python
7. Markdown
8. INI/Config
9. YAML
10. SQL
11. C
12. C++

**Token Types:**
- Keywords (language-specific reserved words)
- Strings (single/double quoted)
- Comments (single-line `//` and multi-line `/* */`)
- Numbers
- Operators
- Properties/Variables

**Features:**
- Language auto-detection from file extension
- Manual language selection via Tools > Language
- Highlight colors from current theme

### JSON Formatter

**Menu:** Tools > Format JSON

**Operations:**
- **Format**: Pretty-print JSON with proper indentation
- **Minify**: Remove all whitespace from JSON
- **Validate**: Check if content is valid JSON

**Features:**
- 2-space indentation for formatting
- Error messages for invalid JSON
- Operates on entire buffer content

### Split View

**Menu:** View > Split

**Options:**
- Split Horizontal (side by side)
- Split Vertical (top and bottom)
- Close Split

**Behavior:**
- Both viewports share the same buffer
- Each viewport has independent scroll position
- Caret is shared (active in one viewport at a time)
- Synchronized updates when content changes

### Backup/Version History

**Backup System:**
- Menu: Tools > Backup
- Creates timestamped backup copies
- Backup directory: `%APPDATA%\FastPad\backups\`
- Maximum 10 backup versions per file
- Backup file naming: `filename.timestamp.bak`

**Features:**
- Create Backup: Manually create a backup of current file
- Restore from Backup: Open backup and replace current content
- Automatic cleanup of old backups beyond limit
- Backup includes: file content, original path, timestamp

### Plugin/Extension System

**Plugin Directory:** `%APPDATA%\FastPad\plugins\`

**Plugin Structure:**
```
plugins/
  plugin_name/
    manifest.json    # Plugin metadata
    plugin.dll      # Compiled plugin module
```

**Manifest Schema:**
```json
{
  "id": "plugin_name",
  "name": "Plugin Display Name",
  "version": "1.0.0",
  "description": "What the plugin does",
  "author": "Author Name",
  "entrypoint": "plugin_init",
  "hooks": ["HOOK_FILE_OPEN", "HOOK_FILE_SAVE"]
}
```

**Available Hooks:**
- `HOOK_FILE_OPEN` - Called after file is opened
- `HOOK_FILE_SAVE` - Called after file is saved
- `HOOK_EDITOR_INIT` - Called when editor is initialized
- `HOOK_APP_INIT` - Called when app is initialized
- `HOOK_APP_SHUTDOWN` - Called when app is shutting down
- `HOOK_BUFFER_CHANGED` - Called when buffer content changes

**Plugin States:**
- `PLUGIN_STATE_LOADED` - Successfully loaded
- `PLUGIN_STATE_ERROR` - Failed to load
- `PLUGIN_STATE_DISABLED` - Manually disabled

### Settings Export/Import

**Menu:** Settings > Export Settings / Import Settings

**Exported Settings Include:**
- Theme selection and customizations
- Font settings (family, size, weight)
- Custom keyboard shortcuts
- Recent files list
- Window size and position
- All preference settings

**Export Format:** JSON file (`%APPDATA%\FastPad\settings_export.json`)

---

## Architecture

### High-Level Modules

| Module | Responsibility |
|--------|----------------|
| `main.c` | Application entry point, WinMain, message loop, top-level initialization |
| `app.c` | Global app state, menu commands, window lifecycle, file/open/save workflow |
| `editor.c` | Caret logic, selection logic, keyboard and mouse editing behavior, viewport state |
| `buffer.c` | Text storage via gap buffer, insertion/deletion, line indexing helpers |
| `render.c` | Font setup, text measuring, line rendering, selection painting, caret rendering |
| `fileio.c` | Open/save/load, encoding conversion, line ending normalization |
| `search.c` | Find text, next/previous match, modeless find dialog |
| `tab_manager.c` | Tab creation, deletion, switching, SysTabControl32 management |
| `theme.c` | Theme definitions, color management, font settings |
| `log.c` | Logging functionality |
| `backup.c` | Backup creation, listing, restore, cleanup |
| `plugin.c` | Plugin discovery, loading, hook management |
| `settings.c` | Settings export/import, JSON serialization |

### Header Files

| Header | Purpose |
|--------|---------|
| `types.h` | Core type definitions (App, Editor, Tab, GapBuffer, etc.) |
| `app.h` | App state management, window procedures |
| `editor.h` | Editor operations, caret, selection |
| `buffer.h` | Gap buffer implementation |
| `render.h` | Rendering constants, colors |
| `fileio.h` | File I/O operations, encoding types |
| `search.h` | Search dialog, match finding |
| `tab_manager.h` | Tab management API |
| `theme.h` | Theme structs, color definitions |
| `log.h` | Logging macros and functions |
| `backup.h` | Backup system API |
| `plugin.h` | Plugin system API, hook definitions |
| `settings.h` | Settings API |

### Data Storage

| File | Location | Purpose |
|------|----------|---------|
| `theme.json` | `%APPDATA%\FastPad\` | Theme and font settings |
| `shortcuts.json` | `%APPDATA%\FastPad\` | Custom keyboard shortcuts |
| `recent_files.json` | `%APPDATA%\FastPad\` | Recent files list |
| `session.json` | `%APPDATA%\FastPad\` | Last session state |
| `settings_export.json` | User-chosen | Exported settings |
| `autosave\` | `%APPDATA%\FastPad\` | Auto-saved temporary files |
| `backups\` | `%APPDATA%\FastPad\` | Version history backups |
| `plugins\` | `%APPDATA%\FastPad\` | Installed plugins |

---

## UI Layout

Main window contains:

- Menu bar
- Tab bar (shows all open tabs)
- Editor client area (shared by all tabs)
- Status bar (toggleable via View > Status Bar)

No ribbon, no custom chrome, no unnecessary panels.

### Tab Bar Behavior

- Tab bar is positioned at the top of the client area
- All tabs share the same editor surface (child window per tab is deferred)
- Tab switching updates focus, caret, and redraw the editor area
- Tab bar shows tab titles with `*` prefix for unsaved changes
- Unsaved indicator and file path shown

---

## Menus

### File
- New (Ctrl+N)
- New Tab (Ctrl+T)
- Close Tab (Ctrl+W)
- Open (Ctrl+O)
- Save (Ctrl+S)
- Save As (Ctrl+Shift+S)
- **Encoding** (submenu)
  - UTF-8
  - UTF-8 with BOM
  - ANSI
  - UTF-16 LE
  - UTF-16 BE
- **Line Ending** (submenu)
  - CRLF
  - LF
  - CR
- **Recent Files** (submenu)
  - File entries (clickable)
  - Pin/Unpin option
  - Clear Recent Files
  - Separator
  - Reopen Last Session
- Exit

### Edit
- Undo (Ctrl+Z)
- Redo (Ctrl+Y)
- Cut (Ctrl+X)
- Copy (Ctrl+C)
- Paste (Ctrl+V)
- Select All (Ctrl+A)
- Find (Ctrl+F)
- **Find/Replace** (submenu)
  - Find (Ctrl+F)
  - Replace (Ctrl+H)
- Go to Line (Ctrl+G)

### View
- **Zoom** (submenu)
  - Zoom In (Ctrl+Plus)
  - Zoom Out (Ctrl+Minus)
  - Reset Zoom (Ctrl+0)
  - Separator
  - 50%
  - 75%
  - 100%
  - 125%
  - 150%
  - 200%
- **Split** (submenu)
  - Split Horizontal
  - Split Vertical
  - Close Split
- Word Wrap
- **Line Numbers**
- Status Bar

### Tools
- **Backup** (submenu)
  - Create Backup
  - Restore from Backup...
- **Plugins** (submenu)
  - Manage Plugins...
  - Reload Plugins
- **Format** (submenu)
  - Format JSON
  - Minify JSON
  - Validate JSON
- **Language** (submenu) - List of supported languages

### Settings
- **Shortcuts**
  - Customize Shortcuts...
  - Reset to Defaults
- **Export Settings...**
- **Import Settings...**
- **Theme** (submenu) - List of preset themes

### Help
- About

---

## Input and Editing Behavior

### Keyboard Shortcuts (v2.0 Complete List)

| Shortcut | Action |
|----------|--------|
| Ctrl+N | New |
| Ctrl+O | Open |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+T | New Tab |
| Ctrl+W | Close Tab |
| Ctrl+Tab | Next Tab |
| Ctrl+Shift+Tab | Previous Tab |
| Ctrl+F | Find |
| Ctrl+H | Replace |
| Ctrl+A | Select All |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+X | Cut |
| Ctrl+C | Copy |
| Ctrl+V | Paste |
| Ctrl+G | Go to Line |
| Ctrl+= | Zoom In |
| Ctrl+- | Zoom Out |
| Ctrl+0 | Reset Zoom |
| Ctrl+Home | Go to file start |
| Ctrl+End | Go to file end |
| Arrows | Move caret |
| Home/End | Move within line |
| PageUp/PageDown | Scroll page |

### Navigation
- arrows move caret
- Home/End move within line
- Ctrl+Home/Ctrl+End go to file start/end
- PageUp/PageDown scroll and move caret
- mouse click places caret
- drag selects text

### Line Endings
- Internally normalize all line endings to `\n`
- Write back with selected line ending type (CRLF/LF/CR)
- Behavior is configurable per-tab

---

## File Handling

**Supported:**
- Plain text files
- UTF-8 files (with and without BOM)
- UTF-16 LE/BE files
- ANSI files (system code page)
- Empty files
- Reasonably large text files

**Not supported:**
- Binary file editing
- Rich text formats
- Word docs
- PDFs

**Behavior:**
- Warn on unsaved changes before destructive actions
- Update window title with filename and modified marker
- Auto-detect encoding on file open

---

## Error Handling

The app must fail simply and clearly.

Examples:
- Open failure -> message box
- Save failure -> message box
- Invalid decode -> either best-effort fallback or clear error
- Out-of-memory -> fail gracefully if practical
- Invalid JSON (formatter) -> show error message

No silent data loss.

---

## Rendering Rules

- use a single monospace default font for v1
- keep rendering logic simple
- avoid flicker
- support resize correctly
- horizontal scrolling when word wrap is disabled

### Selection Rendering

Selection must be drawn properly to avoid visual artifacts:

- Lines are drawn in 3 segments when partially selected: before selection (normal color), selected portion (white on blue), after selection (normal color)
- Do NOT draw entire line in selection color just because part of it is selected
- Selection background is drawn first, then text on top
- This prevents ghost characters and ensures text remains readable during selection

---

## Code Quality Rules

- Small files with clear responsibilities
- Avoid macros unless useful
- Avoid giant global mutable state blocks where possible
- Comment the tricky parts, especially buffer logic
- Prefer simple structs and functions over abstraction-heavy patterns
- No premature generalization

---

## Build and Distribution

### Linux -> Windows Cross-Compile

```sh
x86_64-w64-mingw32-gcc \
  -Os -s \
  -ffunction-sections -fdata-sections \
  -Wl,--gc-sections \
  -mwindows \
  src/main.c src/app.c src/editor.c src/buffer.c src/fileio.c \
  src/render.c src/search.c src/tab_manager.c src/log.c \
  src/theme.c src/backup.c src/plugin.c src/settings.c \
  -o FastPad.exe \
  -luser32 -lgdi32 -lcomdlg32 -lkernel32 -lshell32 -lcomctl32
```

### Release Artifact

- A single `FastPad.exe`
- No runtime installer required
- Target size: ~50KB (with all v2.0 features)

---

## Bug Tracking

Known bugs and their fixes are documented in `spec/BUG_FIXES.md`.

When fixing bugs:
1. Document the bug in BUG_FIXES.md with symptoms, root cause, and fix
2. Add FIX comment in code at the bug location
3. Verify fix compiles and passes tests
4. Update version history in BUG_FIXES.md

---

## Testing

After any changes, always run:

```bash
make clean && make   # Must compile with zero warnings
```

---

## Non-Goals (What FastPad is NOT)

FastPad does NOT include:

- File tree/sidebar
- Minimap
- Telemetry
- Collaboration
- Markdown preview
- Rich text editing
- Web rendering
- Extension marketplace

These remain out of scope for v2.0.