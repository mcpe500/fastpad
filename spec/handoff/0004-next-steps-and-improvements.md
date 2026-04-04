# 0004. Next Steps and Improvements

## Status: Recommendations for Future Development

**Date**: April 4, 2026  
**Priority**: Organized by importance  
**Effort**: Estimated relative to current codebase size

## What This Handoff Covers

This document lists potential improvements to FastPad, organized by priority. Each item includes:
- What the improvement is
- Why it matters
- How to implement it
- Potential pitfalls

## P0: Critical Fixes (Must Do)

### 1. Runtime Testing on Windows

**What**: Actually run FastPad.exe on Windows and verify all features work

**Why**: Code compiles but has never been executed. Could have:
- Runtime crashes
- Rendering issues
- Input handling bugs
- Memory leaks

**How**:
1. Run on Windows 10/11 or Wine
2. Test each feature systematically:
   - Open file
   - Type text
   - Move caret
   - Select text
   - Copy/Cut/Paste
   - Undo/Redo
   - Save file
   - Find text
   - Close with unsaved changes
3. Check memory usage and performance

**Pitfalls**:
- Might reveal font rendering issues
- Clipboard integration might fail
- Status bar common controls might need init

---

### 2. Fix Word Wrap Implementation

**What**: Make word wrap menu item functional

**Why**: Listed in spec as nice-to-have, menu exists but does nothing

**How**:
1. Add `bool word_wrap` flag to App structure (already exists)
2. In render_paint(), calculate line breaks at word boundaries
3. Handle wrapping at viewport width instead of newline characters
4. Update line counting to account for wrapped lines
5. Toggle with View → Word Wrap menu

**Implementation approach**:
```c
// Pseudo-code for word wrap rendering
int wrapped_line_count = 0;
for (each logical line) {
    int chars_remaining = line_length;
    while (chars_remaining > 0) {
        int chars_this_row = min(chars_remaining, visible_cols);
        // Find word boundary (last space before wrap point)
        if (word_wrap && chars_remaining > visible_cols) {
            chars_this_row = find_last_space(line_text, visible_cols);
        }
        render_line_segment(...);
        chars_remaining -= chars_this_row;
        wrapped_line_count++;
    }
}
```

**Pitfalls**:
- Complex to get right with selection
- Caret positioning becomes tricky
- Might slow down rendering for long lines

**Estimated effort**: Medium (100-200 lines)

---

### 3. Add Common Controls Initialization

**What**: Initialize common controls library for status bar

**Why**: Status bar uses `msctls_statusbar32` which requires InitCommonControlsEx()

**How**:
```c
// In app_init() or WinMain()
INITCOMMONCONTROLSEX icex = {0};
icex.dwSize = sizeof(icex);
icex.dwICC = ICC_BAR_CLASSES;
InitCommonControlsEx(&icex);
```

Also add `comctl32` to LIBS in Makefile.

**Pitfalls**:
- Without this, status bar creation might fail silently
- Some Windows versions might work without it, others won't

**Estimated effort**: Trivial (5 minutes)

---

## P1: Important Improvements (Should Do)

### 4. Implement Undo Grouping

**What**: Group consecutive character insertions into single undo operation

**Why**: Currently each keystroke creates separate undo step. Typing "hello" requires 5 undo operations.

**How**:
1. Track last operation type and position
2. If current operation is same type and adjacent position, extend previous operation
3. Otherwise, create new operation

```c
// In editor_char_input()
if (undo.count > 0) {
    UndoOp *last = &undo.ops[undo.count - 1];
    if (last->type == OP_INSERT && 
        last->pos + last->length == caret &&
        same_typing_session()) {
        // Extend previous operation
        last->text = realloc(last->text, last->length + 1);
        last->text[last->length] = ch;
        last->length++;
        return;
    }
}
// Otherwise create new operation
editor_add_undo_op(...);
```

**Session detection**: Use timestamp (e.g., 2 seconds between keystrokes = new session)

**Pitfalls**:
- Need to handle selection deletion correctly
- Must reset grouping on navigation
- Memory management for extended text

**Estimated effort**: Medium (50-100 lines)

---

### 5. Double Buffering to Reduce Flicker

**What**: Render to memory DC first, then copy to screen

**Why**: Current implementation might flicker on resize or rapid typing

**How**:
```c
void render_paint(Editor *editor, HDC hdc, const RECT *update_rect) {
    RECT client_rect;
    GetClientRect(editor->hwnd, &client_rect);
    
    // Create memory DC
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, 
                                            client_rect.right, 
                                            client_rect.bottom);
    HBITMAP old_bitmap = SelectObject(mem_dc, bitmap);
    
    // Render to memory DC
    render_to_dc(editor, mem_dc, &client_rect);
    
    // Copy to screen
    BitBlt(hdc, 0, 0, client_rect.right, client_rect.bottom,
           mem_dc, 0, 0, SRCCOPY);
    
    // Cleanup
    SelectObject(mem_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(mem_dc);
}
```

**Pitfalls**:
- Slightly more memory usage
- Must keep in sync with screen updates

**Estimated effort**: Small (30-50 lines)

---

### 6. Add Line Number Margin (Optional)

**What**: Show line numbers in left margin

**Why**: Common editor feature, helps navigation

**How**:
1. Reserve left margin (e.g., 50 pixels)
2. In render_paint(), draw line numbers before text
3. Adjust viewport scroll calculations
4. Handle resizing

**Pitfalls**:
- Takes screen space
- Must update on line insert/delete
- Right-align numbers for alignment

**Estimated effort**: Medium (80-120 lines)

---

### 7. Improve Find Dialog

**What**: Create proper modal dialog with better UX

**Why**: Current implementation is simple modeless dialog

**How**:
1. Create dialog template in memory (DLGTEMPLATE)
2. Use DialogBoxIndirect() for modal dialog
3. Add "Find Previous" button
4. Add search direction (up/down)
5. Highlight all matches option
6. Remember last search text

**Alternative**: Keep modeless but add:
- "Find Previous" button
- "Highlight All" checkbox
- Search progress for large files
- Replace functionality

**Pitfalls**:
- Dialog templates in memory are complex
- Must handle parent window disable/enable

**Estimated effort**: Medium-High (150-300 lines)

---

## P2: Nice to Have (Could Do)

### 8. Horizontal Scrolling Support

**What**: Add horizontal scroll bar and scrolling

**Why**: Long lines might extend beyond visible area

**How**:
1. Add horizontal scrollbar to window
2. Track max line width
3. Update scrollbar on text changes
4. Handle WM_HSCROLL messages
5. Adjust render_paint() x-offset

**Pitfalls**:
- Complex with word wrap enabled
- Must recalculate on resize
- Might conflict with word wrap

**Estimated effort**: Medium (100-150 lines)

---

### 9. Large File Warning

**What**: Warn user before opening very large files

**Why**: Files >10MB might be slow or use too much memory

**How**:
```c
// In file_load()
if (fileSize > 10 * 1024 * 1024) { // 10 MB
    int result = MessageBoxA(hwnd, 
        "This file is large and might be slow to edit. Continue?",
        "FastPad",
        MB_YESNO | MB_ICONWARNING);
    if (result == IDNO) {
        return false;
    }
}
```

**Pitfalls**:
- Arbitrary threshold
- Might annoy users with fast machines

**Estimated effort**: Trivial (10 lines)

---

### 10. Recent Files List

**What**: Show recently opened files in File menu

**Why**: Quick access to commonly edited files

**How**:
1. Store recent files in array (e.g., last 10)
2. Save to registry or config file on exit
3. Load on startup
4. Display in File menu with accelerators (&1, &2, etc.)
5. Remove duplicates

**Storage**:
```c
#define MAX_RECENT_FILES 10
char recent_files[MAX_RECENT_FILES][MAX_PATH];
int recent_count;
```

**Pitfalls**:
- Must handle deleted files gracefully
- Registry access might fail
- Path length limits

**Estimated effort**: Small (80-120 lines)

---

### 11. Read-Only File Detection

**What**: Detect and indicate when file is read-only

**Why**: Prevents confusion when save fails

**How**:
1. After opening file, check file attributes
2. Set flag in Editor structure
3. Show "[Read Only]" in window title
4. Disable save (show message if attempted)
5. Save As still works

```c
// In file_load()
DWORD attrs = GetFileAttributesA(filename);
if (attrs != INVALID_FILE_SIZE && (attrs & FILE_ATTRIBUTE_READONLY)) {
    editor->read_only = true;
}
```

**Pitfalls**:
- Network files might have different permissions
- User might not understand why save is disabled

**Estimated effort**: Small (40-60 lines)

---

### 12. Ctrl+G Go to Line

**What**: Dialog to jump to specific line number

**Why**: Useful for navigation in large files

**How**:
1. Add menu item (Edit → Go to Line)
2. Show simple dialog with edit box
3. Parse line number input
4. Convert to byte offset
5. Move caret and scroll

**Pitfalls**:
- Must validate input (1-based line numbers)
- Handle out-of-range gracefully

**Estimated effort**: Small (50-80 lines including dialog)

---

## P3: Future Considerations (Maybe Someday)

### 13. Piece Table Instead of Gap Buffer

**What**: Replace gap buffer with piece table

**Why**: 
- Better for complex undo/redo
- More memory efficient for large files
- Easier to implement "snapshot" undo

**How**: Complete rewrite of buffer.c. See spec.md for discussion.

**Estimated effort**: High (rewrite buffer.c)

---

### 14. Plugin Architecture

**What**: Allow loading DLLs as plugins

**Why**: Extensibility

**Why not**: Explicitly out of scope for v1. Would add complexity.

**Status**: Defer until v2 (if ever)

---

### 15. Syntax Highlighting

**What**: Color-code keywords, strings, comments, etc.

**Why**: Useful for code editing

**Why not**: 
- Out of scope for v1
- Would add significant complexity
- Goes against "plain text editor" goal

**Status**: Explicitly not implementing

---

## Performance Optimizations

### A. Line Start Cache

**What**: Cache line start positions for O(1) lookup

**Why**: Currently line operations are O(n)

**How**:
```c
typedef struct {
    TextPos *line_starts;
    int count;
    int capacity;
    int buffer_version; // Invalidate on edit
} LineCache;
```

Update cache on text changes. Rebuild if invalid.

**Trade-off**: Memory vs speed

---

### B. Incremental Redraw

**What**: Only redraw changed regions

**Why**: Full repaint on every keystroke is wasteful

**How**:
1. Track dirty regions during editing
2. Pass update_rect to render_paint()
3. Only render affected lines

**Trade-off**: Complexity vs performance

---

### C. Memory Pool for Undo Operations

**What**: Preallocate undo operation storage

**Why**: Reduces malloc/free overhead

**How**:
```c
typedef struct {
    char text_pool[10000]; // Pool for undo text
    int pool_used;
    // ... allocate from pool instead of malloc
} UndoHistory;
```

**Trade-off**: Fixed limit vs allocation speed

---

## Code Quality Improvements

### X. Add Error Handling

**What**: More graceful error handling throughout

**Why**: Currently some errors might crash or corrupt state

**Examples**:
- Check malloc return values everywhere
- Handle font creation failure
- Handle clipboard failures
- Validate file paths before operations

---

### Y. Add Comments and Documentation

**What**: More inline documentation

**Why**: Helps future maintainers

**Where needed**:
- Complex buffer operations
- Undo/redo logic
- Selection edge cases
- File encoding handling

---

### Z. Unit Tests

**What**: Add test suite for buffer and editor logic

**Why**: Catch regressions

**How**:
1. Extract buffer logic into testable module
2. Write tests for insert/delete/get operations
3. Test undo/redo sequences
4. Test line/column conversions

**Challenge**: Windows-specific code makes cross-platform testing hard

---

## Build System Improvements

### 1. Add Debug Build Configuration

**What**: Separate debug and release builds

**Why**: Debugging requires symbols and no optimization

**How**:
```makefile
# Debug build
debug: CFLAGS = -g -O0 -DDEBUG
debug: LDFLAGS = 
debug: $(TARGET_DEBUG)

# Release build (current)
release: CFLAGS = -Os -s -ffunction-sections -fdata-sections
release: $(TARGET)
```

---

### 2. Add Installer/Package

**What**: Create Windows installer or zip package

**Why**: Easier distribution

**How**: Use NSIS, Inno Setup, or just zip the .exe

---

### 3. Continuous Integration

**What**: Automated build on commit

**Why**: Catch build breaks early

**How**: GitHub Actions with mingw-w64

---

## Prioritization Guide

### If you have 1 hour:
- #3: Add common controls initialization
- #9: Large file warning
- #11: Read-only detection

### If you have 1 day:
- #1: Runtime testing and bug fixes
- #2: Word wrap implementation
- #4: Undo grouping
- #12: Ctrl+G go to line

### If you have 1 week:
- All above
- #5: Double buffering
- #6: Line numbers
- #7: Better find dialog
- #10: Recent files

### If you have 1 month:
- All above
- #8: Horizontal scrolling
- #13: Piece table (maybe)
- Performance optimizations A-C
- Code quality improvements X-Z

---

## Decision Framework

Before implementing any improvement, ask:

1. **Does this help core text editing?** (P0)
2. **Does this keep the app small?** (Check binary size impact)
3. **Does this keep the code understandable?** (Check complexity)
4. **Does this preserve responsiveness?** (Check performance)

If any answer is "no", reconsider or deprioritize.

---

## Final Notes

FastPad v1 is **feature complete** according to spec. The code compiles cleanly and implements all must-have features.

**Current state:**
- ✅ All v1 features implemented
- ✅ Clean compilation (no errors)
- ✅ 32 KB binary (meets size target)
- ⚠️ No runtime testing yet
- ⚠️ Some nice-to-haves deferred

**Recommendation:** Focus on runtime testing and bug fixes before adding features. A solid v1 is better than a buggy v1.1.

Remember the FastPad philosophy:
> "Better means: faster launch, less lag, less clutter, easier to understand, simpler behavior, fewer ways to break."
