# FastPad Specification

## Overview

FastPad is a tiny, native Windows text editor built in C with the raw Win32 API.

The goal is not to compete with VS Code, Notepad++, or modern IDEs. The goal is to be a fast, minimal, self-contained editor that runs anywhere with zero dependencies.

## Version History

### v3.0.0 - Complete Feature Pack
**Date:** May 2026

All phases implemented:

#### Phase 1 — Basic Productivity ✓
- **Auto Save**: Every 3 seconds when buffer is dirty
- **Session Restore**: Restore tabs on startup
- **Recent Files**: File > Recent Files (max 20, pin/unpin, clear)
- **Search/Replace**: Find, Replace, Replace All, Match case, Whole word, Highlight all
- **Line Numbers**: View > Line Numbers toggle
- **Unsaved Indicator**: Tab shows `*` if buffer dirty

#### Phase 2 — Customization ✓
- **Zoom**: Ctrl+Plus/Minus/0 (50-200%)
- **Custom Shortcuts**: Load/save from JSON
- **Encoding Selector**: UTF-8, UTF-8 BOM, ANSI, UTF-16 LE/BE
- **Line Ending Selector**: CRLF, LF, CR
- **Status Bar**: Tab count, line/col, word/char count, encoding, line ending, save state

#### Phase 3 — Developer Features ✓
- **Syntax Highlighting**: 12 languages (JSON, XML, HTML, CSS, JS, Python, Markdown, INI, YAML, SQL, C, C++)
- **JSON Formatter**: Format, Minify, Validate
- **Split View**: Horizontal/Vertical, shared buffer, independent scroll

#### Phase 4 — Advanced ✓
- **Backup/Version History**: %APPDATA%/FastPad/backups/, max 10 versions per file
- **Plugin System**: %APPDATA%/FastPad/plugins/, manifest.json, hook system
- **Settings Export/Import**: JSON format

#### Phase 5 — Workspace & Navigation ✓
- **Open Folder**: File > Open Folder
- **Sidebar File Explorer**: Tree view, file icons, click to open
- **Global Search**: Search in all files, results with preview
- **File Operations**: New, Rename, Delete, Duplicate, Reveal in Explorer

#### Phase 6 — Advanced Editing ✓
- **Multi-Cursor**: Alt+Click, Ctrl+D, Ctrl+Alt+Up/Down
- **Column Selection**: Alt+drag
- **Bracket Highlight**: Matching bracket highlighting, auto-close
- **Code Folding**: JSON objects, code blocks, markdown sections
- **Auto-Complete**: Words from document, popup suggestion

#### Phase 7 — Productivity ✓
- **Notes Manager**: Quick notes (Ctrl+Shift+N), notes list, pin, search
- **Template System**: File > New From Template (Daily Notes, Meeting Notes, Bug Report, etc.)
- **Snippet System**: Triggers like "bugrep"+Tab expands to template
- **Clipboard History**: Ctrl+Shift+V, last 20 items, pin, search
- **Task Mode**: Auto-detect `- [ ]` and `- [x]`, click to toggle

#### Phase 8 — Power User (Partial) ✓
- **File Watcher**: Detect external file changes
- **Read-Only Mode**: Toggle via View menu or Ctrl+R
- **CLI Support**: `--line` option to jump to line
- **Portable Mode**: Detect portable installation

### Phase 8 Not Implemented (Incompatible Header)
- **Diff/Compare Tool**: Not compiled due to header incompatibility with current codebase

---

## Features

### Core Editing
- Plain text editing with multi-encoding support
- Open, Save, Save As
- Multiple tabs
- Undo/Redo with history persistence
- Find and Replace

### User Interface
- Native Windows UI (no Electron, no webview)
- Tab bar with close buttons
- Status bar (line, column, word count, encoding, line ending, modified state)
- Split view for side-by-side editing
- Sidebar file explorer (workspace mode)

### Customization
- 6 preset themes (Classic Light, Classic Dark, Monokai, Solarized Light, Solarized Dark, Dracula)
- Custom font selection
- Zoom (50-200%)
- Word wrap toggle
- Line numbers toggle
- Status bar toggle

### Developer Features
- Syntax highlighting for 12 languages
- JSON formatter (format, minify, validate)
- Code folding
- Bracket matching
- Auto-complete

### Productivity
- Auto-save
- Session restore
- Recent files
- Quick notes
- Clipboard history
- Task/checkbox mode

### File Management
- Workspace/folder mode with file explorer
- Global search in folder
- File operations (new, rename, delete, duplicate)
- File watcher for external changes

---

## Menu Structure

### File
- New (Ctrl+N)
- New Tab (Ctrl+T)
- Open... (Ctrl+O)
- Open Folder...
- Save (Ctrl+S)
- Save As... (Ctrl+Shift+S)
- ---

- Encoding > (UTF-8, UTF-8 BOM, ANSI, UTF-16 LE, UTF-16 BE)
- Line Ending > (CRLF, LF, CR)
- ---

- Recent Files >
- Close Tab (Ctrl+W)
- Exit (Alt+F4)

### Edit
- Undo (Ctrl+Z)
- Redo (Ctrl+Y)
- ---
- Cut (Ctrl+X)
- Copy (Ctrl+C)
- Paste (Ctrl+V)
- ---
- Select All (Ctrl+A)
- ---
- Find... (Ctrl+F)
- Replace... (Ctrl+H)
- Find Next (F3)
- ---
- Format JSON
- Minify JSON
- Validate JSON

### View
- Line Numbers
- Word Wrap
- Status Bar
- ---
- Theme > (submenu with 6 themes)
- ---
- Zoom In (Ctrl+Plus)
- Zoom Out (Ctrl+Minus)
- Reset Zoom (Ctrl+0)
- ---
- Split View > (None, Horizontal, Vertical)
- ---
- Read-Only Mode
- ---
- JSON Formatter

### Tools
- Compare Files... (placeholder - coming soon)
- Keyboard Shortcuts...
- ---
- Export Settings...
- Import Settings...

### Help
- About FastPad

---

## Data Storage

All user data stored in: `%APPDATA%/FastPad/`

```
%APPDATA%/FastPad/
├── fastpad.ini           # Window position, size
├── session.json          # Open tabs, cursor positions
├── settings.json         # All settings
├── shortcuts.json        # Custom keyboard shortcuts
├── recent.txt            # Recent files list
├── themes/
│   └── custom.json       # Custom theme colors
├── backups/              # File version history
│   └── /
│       ├── v1.txt
│       └── v2.txt
├── plugins/              # Plugin DLLs
│   └── <plugin>/
│       ├── manifest.json
│       └── plugin.dll
├── notes/                # Quick notes
│   └── <note_id>.txt
├── templates/            # User templates
│   └── <name>.txt
├── snippets.json        # Snippet definitions
├── tasks.json           # Task data
└── workspaces/          # Recent workspaces
    └── <folder>.json
```

---

## Architecture

### Core Modules

| File | Purpose |
|------|---------|
| main.c | Entry point, WinMain |
| app.c | Main application logic, window proc |
| editor.c | Editor state, text input handling |
| buffer.c | Gap buffer, text storage |
| render.c | Text rendering, syntax highlighting |
| tab_manager.c | Tab management |
| fileio.c | File I/O, encoding conversion |
| search.c | Find/Replace dialog and logic |
| theme.c | Theme definitions and application |

### Extended Modules (Phase 4+)

| File | Purpose |
|------|---------|
| backup.c | Version history |
| plugin.c | Plugin system |
| settings.c | Settings import/export |
| workspace.c | Workspace management |
| explorer.c | Sidebar file explorer |
| globalsearch.c | Search in files |
| notes.c | Quick notes manager |
| template.c | Template system |
| snippet.c | Snippet expansion |
| clipboard.c | Clipboard history |
| taskmode.c | Task/checkbox mode |
| filewatch.c | File watcher |
| cli.c | Command line support |

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+N | New file |
| Ctrl+T | New tab |
| Ctrl+O | Open file |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+W | Close tab |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+X | Cut |
| Ctrl+C | Copy |
| Ctrl+V | Paste |
| Ctrl+A | Select All |
| Ctrl+F | Find |
| Ctrl+H | Replace |
| F3 | Find Next |
| Ctrl+Plus | Zoom In |
| Ctrl+Minus | Zoom Out |
| Ctrl+0 | Reset Zoom |
| Ctrl+Tab | Next Tab |
| Ctrl+Shift+Tab | Previous Tab |
| Ctrl+R | Toggle Read-Only |
| Ctrl+Shift+N | Quick Note |
| Ctrl+Shift+V | Clipboard History |

---

## Build

### Requirements
- llvm-mingw (x86_64-w64-mingw32-clang)
- Windows SDK

### Build Command
```bash
make clean && make
```

### Output
- `build/FastPad.exe` - Main executable

---

## Planned Features

- Diff/Compare Tool (Phase 8 - deferred due to header incompatibility)
- Cloud Sync
- Plugin API expansion
- More syntax highlighting languages
- Macro support
- Custom themes UI