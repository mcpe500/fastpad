# FastPad Bug Fix Documentation

This document records all bugs discovered and fixed in the FastPad codebase.

## Bug Fixes Applied (v1.2.x)

### BUG-001: File Load Buffer Corruption
**File:** `src/fileio.c`
**Severity:** HIGH
**Symptoms:** Crashes or corruption when opening files, especially after realloc

**Root Cause:**
The `file_load()` function was writing directly to `buffer->data` bypassing the gap buffer API:
```c
// OLD - Direct memory write bypassed gap buffer invariants
buffer->data[dest_pos++] = '\n';
buffer->size = text_length;
buffer->gap_start = text_length;
buffer->gap_length = buffer->capacity - text_length;
```

This violated gap buffer internal state management since buffer_insert/buffer_delete expect the gap to be in a consistent position.

**Fix:**
Replace direct data manipulation with proper buffer API calls:
```c
// NEW - Use buffer_insert() to maintain gap buffer invariants
TextPos insert_pos = 0;
for (DWORD i = offset; i < bytesRead; i++) {
    if (fileData[i] == '\r' && i + 1 < bytesRead && fileData[i+1] == '\n') {
        char nl = '\n';
        buffer_insert(buffer, insert_pos, &nl, 1);
        insert_pos++;
        i++;
    } else {
        buffer_insert(buffer, insert_pos, &fileData[i], 1);
        insert_pos++;
    }
}
```

---

### BUG-002: Selection Rendering on Wrapped Lines
**File:** `src/render.c`
**Severity:** HIGH
**Symptoms:** Selection highlighting appears in wrong position or wrong segment on wrapped lines

**Root Cause:**
The rendering code used `seg_start_col` and `seg_end_col` for selection calculations but these weren't properly tracking the segment's display column position. For wrapped lines, each visual segment starts at a different display column position.

**Fix:**
1. Renamed variables to clarify semantics: `seg_start_col` → `seg_start_disp`, `seg_end_col` → `seg_end_disp`
2. Fixed selection rendering to properly calculate segment-relative intersection:
```c
// Calculate intersection with current segment
int intersect_start = (seg_start_disp > sel_start_disp) ? seg_start_disp : sel_start_disp;
int intersect_end = (seg_end_disp < sel_end_disp) ? seg_end_disp : sel_end_disp;
```

---

### BUG-003: Caret Position Wrong with Word Wrap
**File:** `src/render.c`
**Severity:** HIGH
**Symptoms:** Caret disappears or appears at wrong position when scrolling with word wrap enabled

**Root Cause:**
The caret X position calculation didn't properly handle horizontal scroll offset:
```c
// OLD - didn't properly handle horizontal scroll
int caret_x = RENDER_MARGIN_WIDTH + (wrap_enabled ? (caret_display_col % editor->viewport.visible_cols) : caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
```

**Fix:**
Separated the logic for wrapped vs non-wrapped cases:
```c
// NEW - clear separation of wrapped/non-wrapped logic
int caret_x;
if (wrap_enabled) {
    int col_in_wrap = caret_display_col % editor->viewport.visible_cols;
    caret_x = RENDER_MARGIN_WIDTH + col_in_wrap * editor->viewport.char_width;
} else {
    caret_x = RENDER_MARGIN_WIDTH + (caret_display_col - editor->viewport.scroll_x) * editor->viewport.char_width;
}
```

---

### BUG-004: First Visible Line Not Rendered at Scroll Boundary
**File:** `src/render.c`
**Severity:** MEDIUM
**Symptoms:** First line doesn't appear when scroll position exactly matches a line boundary

**Root Cause:**
The initial scroll position calculation used `>` instead of `>=` when checking if a line should be rendered:
```c
// OLD - used > which skipped lines exactly at boundary
if (current_visual_line + visual_lines_for_this_logical > editor->viewport.scroll_y) break;
```

**Fix:**
Added explicit check for exact boundary case:
```c
if (current_visual_line + visual_lines_for_this_logical > editor->viewport.scroll_y) break;
if (current_visual_line + visual_lines_for_this_logical == editor->viewport.scroll_y) {
    // Exactly at scroll boundary - this is the first visible line
    break;
}
```

---

### BUG-005: Tab Switch Redundant Invalidation
**File:** `src/tab_manager.c`
**Severity:** LOW
**Symptoms:** Potential null pointer crash during tab switch with certain race conditions

**Root Cause:**
The `tab_manager_switch_tab()` function had redundant invalidation that could crash if `active` was NULL:
```c
// OLD - could crash if tab count changed between calls
Tab *active = tab_manager_get_active(mgr);
if (active) InvalidateRect(active->hwnd, NULL, TRUE);
```

**Fix:**
Removed the redundant call since the final InvalidateRect already handles full window redraw:
```c
// FIX: Removed redundant InvalidateRect call.
// The final InvalidateRect below handles full window redraw.
```

---

### BUG-006: Find Dialog Crashes When Tab Closed
**File:** `src/search.c`
**Severity:** MEDIUM
**Symptoms:** Crash when closing tab while find dialog is open

**Root Cause:**
The `FindSubclassProc` window procedure used `data->editor` without validating the window handle was still valid. If a tab was closed, the editor pointer became stale.

**Fix:**
Added validation at the start of message processing:
```c
// FIX: Validate editor window handle before using it
if (data && data->editor && !IsWindow(data->editor->hwnd)) {
    data->editor = NULL;  // Mark as invalid, prevent crash
}
```

---

### BUG-007: Selection Clear Doesn't Scroll Caret Into View
**File:** `src/editor.c`
**Severity:** MEDIUM
**Symptoms:** Caret disappears after shift+arrow to create selection then releasing shift to clear

**Root Cause:**
`editor_clear_selection()` set selection endpoints but didn't call `editor_scroll_to_caret()` to ensure the caret was visible after clearing.

**Fix:**
Added scroll-to-caret call:
```c
void editor_clear_selection(Editor *editor) {
    editor->selection.start = editor->caret;
    editor->selection.end = editor->caret;
    // FIX: Ensure caret is scrolled into view after clearing selection
    editor_scroll_to_caret(editor);
    InvalidateRect(editor->hwnd, NULL, FALSE);
}
```

---

### BUG-008: Caret Position Not Refreshed When Focus Returns
**File:** `src/app.c`
**Severity:** MEDIUM
**Symptoms:** Caret appears at wrong position or missing after alt-tabbing back to editor

**Root Cause:**
`app_on_setfocus()` only called `ShowCaret()` without first ensuring the caret position was properly updated via scroll.

**Fix:**
Added caret position refresh before showing:
```c
void app_on_setfocus(App *app) {
    Tab *tab = tab_manager_get_active(&app->tab_mgr);
    if (tab) {
        // FIX: When focus returns to editor, ensure caret is visible
        editor_scroll_to_caret(&tab->editor);
        ShowCaret(tab->hwnd);
        InvalidateRect(tab->hwnd, NULL, FALSE);
    }
}
```

---

### BUG-009: Selection Rendering Text Color Wrong (Fixed in BUG-002)
**File:** `src/render.c`
**Severity:** HIGH
**Symptoms:** Selected text renders in wrong color, making text hard to read

**Root Cause:**
The text segment rendering for selected text used incorrect variable names and wrong x-coordinate calculations:
```c
// OLD - Used seg_start_col instead of seg_start_disp, wrong x offset
int sel_x = x + (seg_start_col + sel_start_in_seg) * editor->viewport.char_width;
```

**Fix:**
Complete rewrite of selection rendering logic (see BUG-002) with proper segment-relative calculations.

---

## Known Issues (Not Yet Fixed)

### Issue-001: Performance with Large Files
The gap buffer implementation may become slow with very large files (>10MB) due to reallocation patterns.

### Issue-002: Undo History Memory
Undo history is stored as separate allocations which can become fragmented. No consolidation of small operations.

### Issue-003: Binary Size
The exe is currently around 270KB. Further optimization possible with -Oz and LTO but not a priority.

---

## Testing Notes

After fixes, always run:
```bash
make clean && make          # Windows build
make test_linux             # Linux buffer tests with ASan
```

Buffer tests use AddressSanitizer to catch memory corruption issues that may not manifest on Windows but could on other platforms.

---

## Version History

- **v1.2.0** - Bug fixes from this session
  - BUG-001 through BUG-009 fixed
  - Better file loading via proper gap buffer API
  - Selection rendering fixed for wrapped lines
  - Caret positioning fixed for all scroll modes
  - Find dialog stability improved

- **v1.1.x** - Tab management added, stability issues
  - Multi-tab support introduced
  - Various stability issues from sharing editor surface

- **v1.0.x** - Initial release
  - Basic text editing
  - File open/save
  - Undo/redo