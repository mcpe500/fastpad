# FastPad

A tiny, fast, native Windows text editor written in C.

## Features

### Core Editing
- Plain text editing with multi-encoding support (UTF-8, UTF-8 BOM, ANSI, UTF-16 LE/BE)
- Open, Save, Save As
- Multi-tab editing
- Cut, Copy, Paste
- Undo/Redo
- Find and Replace

### Productivity
- **Auto Save** - Automatically saves every 3 seconds when buffer is dirty
- **Session Restore** - Reopens last session on startup
- **Recent Files** - Quick access to recently opened files (up to 20)
- **Line Numbers** - Toggle gutter with line numbers
- **Word Wrap** - Toggle word wrap on/off

### Customization
- **6 Preset Themes** - Classic Light/Dark, Monokai, Solarized Light/Dark, Dracula
- **Custom Font** - Adjustable font family, size, and weight
- **Zoom** - Ctrl+Plus/Minus/0 for zoom in/out/reset (50%-200%)
- **Custom Shortcuts** - Export/import keyboard shortcuts
- **Encoding Selector** - Per-tab encoding selection
- **Line Ending Selector** - Per-tab line ending (CRLF/LF/CR)

### Developer Tools
- **Syntax Highlighting** - 12 languages (JSON, XML, HTML, CSS, JS, Python, Markdown, INI, YAML, SQL, C, C++)
- **JSON Formatter** - Format, Minify, Validate JSON
- **Split View** - Horizontal or vertical split for side-by-side editing

### Advanced
- **Backup/Version History** - Create and restore from backups
- **Plugin System** - Load custom plugins with hooks
- **Settings Export/Import** - Share settings between machines

### Status Bar
Shows: Tab X/Y | Ln X, Col Y | Words: N | Chars: N | Encoding | Line Ending | Saved Status

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+N | New file |
| Ctrl+O | Open file |
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
| Ctrl+Home/End | Go to file start/end |
| Arrows | Move caret |
| Home/End | Go to line start/end |
| PageUp/PageDown | Scroll page |

## Building

### Prerequisites

- Windows 10/11 or Linux with MinGW-w64
- C compiler (MSVC, MinGW-w64, or clang)
- Make

### Linux Cross-Compile (MinGW-w64)

```bash
# Install toolchain
sudo apt update
sudo apt install -y gcc-mingw-w64-x86-64 make

# Build
make

# Output: build/FastPad.exe
```

### Native Windows Build (MSVC)

```cmd
# Using Developer Command Prompt for VS
cl /O2 /Fe:FastPad.exe ^
   src/main.c src/app.c src/editor.c src/buffer.c ^
   src/fileio.c src/render.c src/search.c src/tab_manager.c ^
   src/log.c src/theme.c src/backup.c src/plugin.c src/settings.c ^
   /link user32.lib gdi32.lib comdlg32.lib shell32.lib comctl32.lib ^
   /SUBSYSTEM:WINDOWS
```

### Native Windows Build (MinGW-w64)

```cmd
gcc -Os -s -ffunction-sections -fdata-sections ^
    -Wl,--gc-sections -mwindows ^
    src/main.c src/app.c src/editor.c src/buffer.c ^
    src/fileio.c src/render.c src/search.c src/tab_manager.c ^
    src/log.c src/theme.c src/backup.c src/plugin.c src/settings.c ^
    -o FastPad.exe ^
    -luser32 -lgdi32 -lcomdlg32 -lkernel32 -lshell32 -lcomctl32
```

## Architecture

```
src/
├── main.c          - Entry point (WinMain)
├── app.c           - App state, window lifecycle, menus
├── editor.c        - Caret, selection, editing logic
├── buffer.c        - Gap buffer text storage
├── fileio.c        - File open/save, encoding handling
├── render.c        - GDI text rendering
├── search.c        - Find text functionality
├── tab_manager.c   - Multi-tab management
├── theme.c         - Theme and color management
├── log.c           - Logging functionality
├── backup.c         - Backup/version history
├── plugin.c        - Plugin system
└── settings.c      - Settings export/import
```

## Data Storage

All user data is stored in `%APPDATA%\FastPad\`:

- `theme.json` - Theme and font settings
- `shortcuts.json` - Custom keyboard shortcuts
- `recent_files.json` - Recent files list
- `session.json` - Last session state
- `autosave\` - Auto-saved temporary files
- `backups\` - Version history backups
- `plugins\` - Installed plugins

## Design Goals

- **Fast startup** - Opens in under a second
- **Low memory** - Minimal private working set
- **Small binary** - ~50KB with all features
- **Responsive** - No typing lag
- **Simple** - Easy to understand and maintain

## License

MIT