# FastPad

A tiny, fast, native Windows text editor written in C.

## Features

- Plain text editing with UTF-8 support
- Open, Save, Save As
- Cut, Copy, Paste
- Undo/Redo
- Find text
- Status bar with line/column info
- Word wrap toggle
- Low memory usage
- Instant startup

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+N | New file |
| Ctrl+O | Open file |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+F | Find |
| Ctrl+A | Select All |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+X | Cut |
| Ctrl+C | Copy |
| Ctrl+V | Paste |
| Arrows | Move caret |
| Home/End | Go to line start/end |
| Ctrl+Home/End | Go to file start/end |
| PageUp/PageDown | Scroll page |

## Building

### Prerequisites

- Windows 10/11
- C compiler (MSVC, MinGW-w64, or clang)
- Make (optional, but recommended)

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
   src/fileio.c src/render.c src/search.c ^
   /link user32.lib gdi32.lib comdlg32.lib shell32.lib ^
   /SUBSYSTEM:WINDOWS
```

### Native Windows Build (MinGW-w64)

```cmd
gcc -Os -s -ffunction-sections -fdata-sections ^
    -Wl,--gc-sections -mwindows ^
    src/main.c src/app.c src/editor.c src/buffer.c ^
    src/fileio.c src/render.c src/search.c ^
    -o FastPad.exe ^
    -luser32 -lgdi32 -lcomdlg32 -lkernel32 -lshell32
```

## Architecture

```
src/
├── main.c      - Entry point (WinMain)
├── app.c       - App state, window lifecycle, menus
├── editor.c    - Caret, selection, editing logic
├── buffer.c    - Gap buffer text storage
├── fileio.c    - File open/save, UTF-8 handling
├── render.c    - GDI text rendering
└── search.c    - Find text functionality
```

## Design Goals

- **Fast startup** - Opens in under a second
- **Low memory** - Minimal private working set
- **Small binary** - Under a few hundred KB
- **Responsive** - No typing lag
- **Simple** - Easy to understand and maintain

## License

Public domain / MIT
