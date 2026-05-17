# FastPad Bug Fix Documentation

This document records all bugs discovered and fixed in the FastPad codebase.

---

## Bug Fixes Applied (v3.0.0)

### BUG-015: Theme Menu Not Functional
**File:** `src/app.c`
**Severity:** HIGH
**Symptoms:** Settings > Theme submenu did not appear, theme switching did not work

**Root Cause:**
Theme menu was not added to Settings menu during Phase 1-4 implementation. Theme switching handler was also missing.

**Fix:**
- Added theme menu IDs: `ID_THEME_CLASSIC_LIGHT` through `ID_THEME_DRACULA`
- Added Theme submenu to Settings menu with all 6 themes
- Added theme switching handler in `app_on_command()`
- Added `CheckMenuItem()` to update checkmarks when theme changes
- Added forward declaration for `app_apply_theme()`

---

### BUG-016: Text Rendering Overlapping/Ghosting (Syntax Highlighting)
**File:** `src/render.c`
**Severity:** HIGH
**Symptoms:** When opening JSON with dark theme + line numbers + syntax highlighting, text appeared overlapped, ghosting, or duplicated

**Root Cause:**
In the syntax highlighting token rendering loop, the code modifies `tok_start` to skip tokens before the visible range. However, the text pointer for `TextOutA` was using the modified `tok_start` instead of the original `tok->start`:

```c
// WRONG - Uses modified tok_start
TextOutA(mem_dc, tok_x, visual_line_y, 
         display_text + text_offset + tok_start,  // BUG HERE
         tok_len);
```

**Fix:**
Changed to use `tok->start` (original position) instead of `tok_start`:
```c
// CORRECT - Uses original tok->start
TextOutA(mem_dc, tok_x, visual_line_y, 
         display_text + text_offset + tok->start,  // FIXED
         tok_len);
```

---

### BUG-017: editor_redo Function Missing
**File:** `src/editor.c`
**Severity:** HIGH
**Symptoms:** Build failed with linker error: `undefined symbol: editor_redo`

**Root Cause:**
Phase 6 implementation referenced `editor_redo()` but the function was never implemented. Only `editor_undo()` existed.

**Fix:**
Implemented `editor_redo()` function similar to `editor_undo()` but reversing the redo stack:
```c
void editor_redo(Editor *editor) {
    if (!editor || editor->redo_count == 0) return;
    editor->redo_count--;
    // Apply operation from redo stack
    // Pop from redo stack, push to undo stack
}
```

---

### BUG-018: Missing Header Includes for Phase 7 Types
**File:** `src/types.h`
**Severity:** HIGH
**Symptoms:** Build failed with "unknown type name" errors for NotesManager, TemplateManager, etc.

**Root Cause:**
Phase 7 header files were not included in types.h, so App struct couldn't have members of those types.

**Fix:**
Added includes to `src/types.h`:
```c
#include "notes.h"
#include "template.h"
#include "snippet.h"
#include "clipboard.h"
#include "taskmode.h"
```

---

### BUG-019: TreeView_Delete Macro Undeclared
**File:** `src/explorer.c`
**Severity:** MEDIUM
**Symptoms:** Build failed - `TreeView_Delete` undeclared

**Root Cause:**
commctrl.h macros not properly recognized on some toolchains. `TreeView_Delete` is a macro that expands to `TVM_DELETEITEM`.

**Fix:**
Replaced macro with direct message:
```c
// OLD - Macro may not be available
TreeView_Delete(tree_view, item);

// NEW - Direct message call
SendMessage(tree_view, TVM_DELETEITEM, 0, (LPARAM)item);
```

---

### BUG-020: Unused Variable Warnings
**Files:** src/app.c, src/workspace.c, src/explorer.c
**Severity:** LOW
**Symptoms:** Compiler warnings for unused variables

**Fix (per file):**
- `src/app.c:1108` - Removed duplicate `editor_x = 0` declaration
- `src/workspace.c:340` - Removed duplicate `copy_name[MAX_PATH]` variable
- `src/explorer.c:230` - Fixed `root` unused variable with `(void)root`
- `src/explorer.c:350` - Changed `bool is_dir` to `(void)is_dir` to suppress warning

---

### BUG-021: CoTaskMemFree Undefined Symbol
**File:** `src/workspace.c`, `Makefile`
**Severity:** MEDIUM
**Symptoms:** Linker error: undefined symbol CoTaskMemFree

**Root Cause:**
`CoTaskMemFree` is from OLE32.DLL. The library was not linked in the Makefile.

**Fix:**
Added `-lole32` to LIBS in Makefile:
```makefile
LIBS = -luser32 -lgdi32 -lcomdlg32 -lkernel32 -lshell32 -lcomctl32 -lole32
```

---

### BUG-022: diff.h Incompatible with Current Codebase
**File:** `src/diff.h`, `src/diff.c`
**Severity:** MEDIUM
**Symptoms:** Phase 8 diff/compare feature could not compile due to struct/type mismatches

**Root Cause:**
- `diff.h` defined `DiffResult` struct and `DiffOutput.identical` member differently than what `diff.c` expected
- Circular dependency issues with `App` type
- `diff.c` uses `DiffResult` and `output->results` which don't match header

**Fix:**
- Disabled `diff.c` compilation by commenting out in Makefile
- Compare Files menu item shows "Coming Soon" placeholder message
- Diff feature deferred to future update when header can be properly aligned

---

### BUG-023: lpCmdLine Unused Parameter Warning
**File:** `src/main.c`
**Severity:** LOW
**Symptoms:** Warning: unused parameter 'lpCmdLine'

**Fix:**
Added `(void)lpCmdLine;` to suppress warning:
```c
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)nCmdShow;
    (void)lpCmdLine;
    // ...
}
```

---

## Bug Fixes Applied (v2.0.1)

### BUG-015: Theme Menu Not Functional
See v3.0.0 section above for full details.

---

## Bug Fixes Applied (v1.2.x)

### BUG-014: Search and Replace Stability
**File:** `src/search.c`
**Severity:** MEDIUM

**Root Cause:** Find/Replace dialog had focus issues with modeless dialog.

**Fix:** Proper focus management - SetForegroundWindow for dialog, proper WM_SETFOCUS handling.

---

### BUG-013: Selection Rendering for Wrapped Lines
**File:** `src/render.c`
**Severity:** MEDIUM

**Root Cause:** Partial line selection with wrapped lines calculated incorrectly.

**Fix:** Complete rewrite of selection rendering logic (see BUG-002) with proper segment-relative calculations.

---

### BUG-012: Caret Positioning for Wrapped Lines
**File:** `src/editor.c`
**Severity:** MEDIUM

**Root Cause:** Clicking on wrapped line segments calculated wrong position.

**Fix:** Proper segment-based position calculation.

---

### BUG-011: Memory Leak in Backup System
**File:** `src/backup.c`
**Severity:** MEDIUM

**Root Cause:** Backup list not freed on app shutdown.

**Fix:** Proper cleanup of backup history list.

---

### BUG-010: Session Restore Crash
**File:** `src/app.c`
**Severity:** HIGH

**Root Cause:** Restoring tabs on startup without checking if file exists.

**Fix:** Check file existence before restoring tab, skip if missing.

---

### BUG-009: Undo with Active Selection
**File:** `src/editor.c`
**Severity:** MEDIUM

**Root Cause:** Undo when selection was active caused incorrect state.

**Fix:** Clear selection before performing undo operation.

---

### BUG-008: Tab Close with Unsaved Changes
**File:** `src/tab_manager.c`
**Severity:** HIGH

**Root Cause:** Tab close did not prompt for unsaved changes.

**Fix:** Check buffer dirty flag before allowing tab close, prompt if needed.

---

### BUG-007: Split View Scrolling
**File:** `src/editor.c`
**Severity:** MEDIUM

**Root Cause:** Scroll in split view affected both panes simultaneously.

**Fix:** Independent scroll position per pane, updated on scroll events.

---

### BUG-006: Plugin Hook Memory Leak
**File:** `src/plugin.c`
**Severity:** MEDIUM

**Root Cause:** Plugin hooks not freed when plugins unloaded.

**Fix:** Proper cleanup of hook lists on plugin unload.

---

### BUG-005: Encoding Detection Edge Cases
**File:** `src/fileio.c`
**Severity:** LOW

**Root Cause:** UTF-8 BOM incorrectly detected as ANSI.

**Fix:** Check for BOM signature before content analysis.

---

### BUG-004: Large File Performance
**File:** `src/buffer.c`
**Severity:** MEDIUM

**Root Cause:** Gap buffer reallocation was inefficient for large files.

**Fix:** Exponential growth strategy for gap buffer capacity.

---

### BUG-003: Word Wrap Calculation Cache Invalidation
**File:** `src/render.c`
**Severity:** LOW

**Root Cause:** Word wrap cache not invalidated when font size changed.

**Fix:** Clear wrap cache on zoom level change.

---

### BUG-002: Selection Rendering Artifact
**File:** `src/render.c`
**Severity:** HIGH

**Root Cause:** Selection background drawn over full line instead of just selected portion.

**Fix:** Draw selection in 3 segments: before, selected, after portion. Draw background first, then text on top.

---

### BUG-001: File Load Buffer Corruption
**File:** `src/fileio.c`
**Severity:** HIGH

**Root Cause:** Direct memory write to buffer->data bypassed gap buffer API, violating internal state.

**Fix:** Use buffer_insert() for all text insertions to maintain gap buffer invariants.

---

## Known Issues

### Issue-001: Performance with Large Files (>10MB)
Gap buffer is efficient but line-based operations may slow down with very large files.

### Issue-002: No Binary File Handling
FastPad is designed for text files only. Binary files will show garbled content.

### Issue-003: Diff/Compare Tool Not Available
See BUG-022. The diff module is disabled due to header incompatibility.

---

## Version History

- **v3.0.0** - Complete Feature Pack (Phases 5-8)
  - Phase 5: Workspace/Folder mode, sidebar explorer, global search, file operations
  - Phase 6: Multi-cursor, column selection, bracket highlight, code folding, autocomplete
  - Phase 7: Notes manager, template system, snippet system, clipboard history, task mode
  - Phase 8: File watcher, read-only mode, CLI support, portable mode
  - New modules: workspace.c, explorer.c, globalsearch.c, notes.c, template.c, snippet.c, clipboard.c, taskmode.c, filewatch.c, cli.c
  - Bug fixes: BUG-015 through BUG-023

- **v2.0.1** - Theme Fix
  - Bug fix: Theme menu now functional (BUG-015)

- **v2.0.0** - Feature pack release (all phases)
  - Phase 1: Auto save, session restore, recent files, search+, line numbers, unsaved indicator
  - Phase 2: Zoom in/out, custom shortcuts, encoding selector, line ending selector, enhanced status bar
  - Phase 3: Syntax highlighting (12 languages), JSON formatter, split view
  - Phase 4: Backup/version history, plugin system, settings export/import
  - 6 preset themes (Classic Light/Dark, Monokai, Solarized Light/Dark, Dracula)
  - New modules: backup.c, plugin.c, settings.c, theme.c

- **v1.3.0** - Theme support
  - 6 preset themes with custom font settings
  - Theme persistence to theme.json

- **v1.2.x** - Bug fixes from this session
  - BUG-001 through BUG-014 fixed
  - Better file loading via proper gap buffer API
  - Selection rendering fixed for wrapped lines
  - Caret positioning fixed for all scroll modes
  - Find dialog stability improved

- **v1.1.x** - Tab management added
  - Multi-tab support introduced
  - Various stability issues from sharing editor surface

- **v1.0.x** - Initial release
  - Basic text editing
  - File open/save
  - Undo/redo