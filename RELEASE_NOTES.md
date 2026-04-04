# Release Notes Template

Copy this content when creating the GitHub Release.

---

## 🚀 FastPad v1.0.0

**A tiny, fast, native Windows text editor written in C.**

FastPad is designed to be the opposite of bloated editors. It starts instantly, uses minimal memory, and stays out of your way while you edit text.

### ✨ Features

- ✅ **Plain text editing** with UTF-8 support
- ✅ **File operations**: Open, Save, Save As
- ✅ **Clipboard**: Cut, Copy, Paste (with proper UTF-8/UTF-16 conversion)
- ✅ **Undo/Redo** with bounded history (1000 operations)
- ✅ **Find text** with case-sensitive option
- ✅ **Status bar** showing line, column, and modified state
- ✅ **Full keyboard shortcuts** (see below)
- ✅ **Unsaved changes prompt** before closing
- ✅ **32 KB binary** - zero dependencies!

### ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New file |
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
| `Arrows` | Move caret |
| `Home/End` | Go to line start/end |
| `Ctrl+Home/End` | Go to file start/end |
| `PageUp/PageDown` | Scroll page |
| `Shift+Arrows` | Extend selection |

### 📦 Download

- **FastPad.exe** (32 KB) - Windows 64-bit (x86-64)
- No installer required
- No runtime dependencies - just double-click and run!

### 🖥️ System Requirements

- Windows 10/11 (64-bit)
- Also works on Wine (Linux/macOS)

### 🔨 Build from Source

If you want to build from source:

```bash
# Prerequisites: llvm-mingw or MinGW-w64
make

# Output: build/FastPad.exe
```

See [README.md](https://github.com/mcpe500/fastpad/blob/main/README.md) for detailed build instructions.

### 📋 What's NOT in FastPad (By Design)

FastPad is NOT trying to be:
- ❌ An IDE
- ❌ A browser-based editor
- ❌ A plugin platform
- ❌ A rich text editor
- ❌ A collaboration tool

FastPad focuses on doing one thing well: **editing plain text, fast**.

### 🐛 Known Limitations

- Word wrap menu exists but not yet functional
- No syntax highlighting (by design)
- No tabs (single file per window)
- Font is hardcoded to Consolas

### 📝 SHA256 Checksum

```
dced33f85973b9a3ee7e6d7157b55fa6a0a07141b4fa7eb55a1ea7d09cdf85a6  FastPad.exe
```

### 📚 Documentation

- [README](https://github.com/mcpe500/fastpad/blob/main/README.md) - Usage and build instructions
- [Spec](https://github.com/mcpe500/fastpad/blob/main/spec/001-spec.md) - Product requirements
- [Handoff Docs](https://github.com/mcpe500/fastpad/tree/main/spec/handoff) - Technical documentation

---

**Full Changelog**: https://github.com/mcpe500/fastpad/compare/v1.0.0
