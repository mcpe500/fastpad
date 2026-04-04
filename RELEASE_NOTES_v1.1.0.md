## 🚀 FastPad v1.1.0

**Multi-tab support is here!** Edit multiple files simultaneously with FastPad's new tabbed interface.

### 🆕 What's New

#### Multi-Tab Editing
- ✅ **Up to 20 tabs** - Each with its own independent editor
- ✅ **New Tab** (Ctrl+T) - Open a fresh editing space
- ✅ **Close Tab** (Ctrl+W) - Close current tab
- ✅ **Switch Tabs** (Ctrl+Tab / Ctrl+Shift+Tab) - Navigate quickly
- ✅ **Click to Switch** - Click tab titles to switch
- ✅ **Independent Undo/Redo** - Each tab maintains its own history
- ✅ **Tab Bar UI** - Modern SysTabControl32 common control

#### Improved UI/UX
- Status bar shows tab position: `Tab 2/5 | Ln 10, Col 5`
- Window title reflects active tab's filename and modified state
- Auto-create first tab on startup
- Auto-create new tab when closing the last one
- Proper save prompt for each tab's unsaved changes

### 🐛 Bug Fixes

#### Critical: Backspace/Delete Not Working
- **Fixed**: Backspace now properly removes characters
- **Fixed**: Delete key works correctly
- **Root Cause**: Operations were executed in wrong order
- **Fix**: Save deleted text → Delete from buffer → Add to undo → Update caret

### ✨ Core Features

- ✅ Plain text editing with UTF-8 support
- ✅ Open, Save, Save As
- ✅ Clipboard: Cut, Copy, Paste
- ✅ Undo/Redo with bounded history
- ✅ Find text with case-sensitive option
- ✅ Status bar with line, column, and modified state
- ✅ Full keyboard shortcuts
- ✅ Unsaved changes prompt
- ✅ **37 KB binary** - zero dependencies!

### ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New file |
| `Ctrl+T` | **New Tab** 🆕 |
| `Ctrl+W` | **Close Tab** 🆕 |
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
| `Ctrl+Tab` | **Next Tab** 🆕 |
| `Ctrl+Shift+Tab` | **Previous Tab** 🆕 |
| `Arrows` | Move caret |
| `Shift+Arrows` | Extend selection |
| `Home/End` | Go to line start/end |
| `Ctrl+Home/End` | Go to file start/end |

### 🧪 Testing

This release includes comprehensive unit tests:

- ✅ **20 tests** covering core functionality
- ✅ **Backspace/Delete bug fix verified**
- ✅ **10K stress test** passed (no memory leaks)
- ✅ **Edge cases** handled correctly
- ✅ **Memory safety** verified (all allocations freed)

Run tests with: `make test`

### 📦 Download

- **FastPad.exe** (37 KB) - Windows 64-bit (x86-64)
- No installer required
- No runtime dependencies - just double-click and run!

### 🖥️ System Requirements

- Windows 10/11 (64-bit)
- Also works on Wine (Linux/macOS)

### 🔨 Build from Source

```bash
# Prerequisites: llvm-mingw or MinGW-w64
make

# Run tests
make test

# Output: build/FastPad.exe
```

See [README.md](https://github.com/mcpe500/fastpad/blob/main/README.md) for detailed build instructions.

### 🐛 Known Limitations

- Word wrap menu exists but not yet functional
- No syntax highlighting (by design)
- No tabs (single file per window) - wait, we have tabs now! 🎉
- Font is hardcoded to Consolas

### 📝 SHA256 Checksum

```
<sha256sum build/FastPad.exe>
```

### 📚 Documentation

- [README](https://github.com/mcpe500/fastpad/blob/main/README.md) - Usage and build instructions
- [Spec](https://github.com/mcpe500/fastpad/blob/main/spec/001-spec.md) - Product requirements
- [Handoff Docs](https://github.com/mcpe500/fastpad/tree/main/spec/handoff) - Technical documentation

---

**Full Changelog**: https://github.com/mcpe500/fastpad/compare/v1.0.0...v1.1.0
