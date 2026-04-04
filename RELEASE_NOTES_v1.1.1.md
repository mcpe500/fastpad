## 🚀 FastPad v1.1.1

**Critical rendering fix** — text now appears when typing!

### 🐛 Bug Fixed

#### Text Not Appearing When Typing
- **Issue**: Characters were being inserted into the buffer but not displayed on screen
- **Symptom**: Typing did nothing visually, delete/backspace seemed broken
- **Root Cause**: Double coordinate adjustment between `app_on_paint()` and `render_paint()`
  - `app_on_paint()` used `SetViewportOrgEx` to offset drawing for tab bar
  - `render_paint()` ALSO manually adjusted `client_rect.bottom -= tab_height`
  - Result: All rendering was offset twice, pushing text off-screen

#### Caret at Wrong Position
- Same double offset issue caused caret to appear at incorrect Y position

### ✅ Fix

**render.c**:
- Removed `extern App g_app` declaration
- Removed `client_rect.bottom -= g_app.tab_mgr.height` adjustment
- Removed `+ g_app.tab_mgr.height` from caret Y position
- Let `SetViewportOrgEx` in `app_on_paint()` handle ALL offset in one place

**render_paint()** now renders from (0,0) — viewport origin handles the rest.

### ✨ Features (from v1.1.0)

- ✅ Multi-tab support (up to 20 tabs)
- ✅ Each tab has independent editor with undo/redo
- ✅ New Tab (Ctrl+T), Close Tab (Ctrl+W)
- ✅ Switch tabs (Ctrl+Tab / Ctrl+Shift+Tab)
- ✅ Click tab titles to switch
- ✅ Status bar shows tab position
- ✅ Window title reflects active tab

### ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New file |
| `Ctrl+T` | New Tab |
| `Ctrl+W` | Close Tab |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+F` | Find |
| `Ctrl+A` | Select All |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+X` | Cut |
| `Ctrl+C` | Copy |
| `Ctrl+V` | Paste |
| `Ctrl+Tab` | Next Tab |
| `Ctrl+Shift+Tab` | Previous Tab |
| `Arrows` | Move caret |
| `Shift+Arrows` | Extend selection |
| `Home/End` | Go to line start/end |
| `Ctrl+Home/End` | Go to file start/end |

### 📦 Download

- **FastPad.exe** (36 KB) - Windows 64-bit (x86-64)
- No installer required
- No runtime dependencies - just double-click and run!

### 🖥️ System Requirements

- Windows 10/11 (64-bit)
- Also works on Wine (Linux/macOS)

### 🔨 Build from Source

```bash
make
```

See [README.md](https://github.com/mcpe500/fastpad/blob/main/README.md) for detailed build instructions.

### 📝 SHA256 Checksum

```
<sha256sum build/FastPad.exe>
```

---

**Full Changelog**: https://github.com/mcpe500/fastpad/compare/v1.1.0...v1.1.1
