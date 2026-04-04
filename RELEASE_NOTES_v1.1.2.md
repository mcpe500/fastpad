# Release Notes - FastPad v1.1.2

## 🐛 Bug Fix Release

This release focuses on fixing critical bugs related to tab management, rendering, and focus handling. No new features, just stability improvements.

### Fixed Issues

#### 1. **Application hang on close** (Critical)
- **Problem**: Window would hang when closing with multiple tabs open
- **Root cause**: 
  - Close button (X) only checked unsaved changes for active tab
  - `WM_DESTROY` called `tab_manager_close_tab()` which would auto-create new tabs
  - Infinite loop during shutdown
- **Fix**:
  - `WM_CLOSE` now checks ALL tabs for unsaved changes using `tab_manager_check_unsaved()`
  - Added `shutting_down` flag to prevent auto-creating tabs during shutdown
  - `WM_DESTROY` now frees resources directly without going through close flow
  - `tab_manager_free()` is now "resources only" - no prompts, no switching, no new tabs

#### 2. **Selection rendering artifacts** (High)
- **Problem**: Text appeared invisible/white when selected, ghost characters after backspace/delete
- **Root cause**: Entire line was drawn in white color when ANY part was selected
- **Fix**: 
  - Lines now drawn in 3 segments: before selection (black), selected (white on blue), after selection (black)
  - Only the actual selected portion uses white text
  - Prevents ghost characters and ensures readability

#### 3. **Find dialog focus issues** (Medium)
- **Problem**: After Find Next, keyboard shortcuts would go to dialog instead of editor
- **Root cause**: `SetFocus(g_find_edit)` kept focus in dialog after search
- **Fix**:
  - Find Next now returns focus to parent window
  - Dialog WM_DESTROY restores focus to editor
  - Consistent focus management throughout

#### 4. **Tab switching inconsistencies** (Medium)
- **Problem**: New tab sometimes couldn't type, Ctrl+A appeared not to work
- **Root cause**: Focus not properly set after tab switch, caret state ambiguous
- **Fix**:
  - Tab switch now: sets focus, hides stale caret, updates statusbar, forces full redraw
  - Ensures clean state after every tab change

### Technical Changes

#### `src/types.h`
- Added `shutting_down` flag to `App` struct

#### `src/app.c`
- `WM_CLOSE`: Now uses `tab_manager_check_unsaved()` instead of `app_check_unsaved()`
- `WM_CLOSE`: Sets `app->shutting_down = true` before `DestroyWindow()`
- `WM_DESTROY`: Simplified to just call `tab_manager_free()` (no tab-by-tab close)
- All `tab_manager_close_tab()` calls now pass `app->shutting_down` parameter

#### `src/tab_manager.c`
- `tab_manager_free()`: Now frees editor resources directly, no prompts/switching/new tabs
- `tab_manager_close_tab()`: Added `is_shutting_down` parameter
  - Skips save prompts during shutdown
  - Prevents auto-creating tabs when count reaches 0 during shutdown
- `tab_manager_switch_tab()`: 
  - Now hides caret during switch
  - Updates statusbar
  - Forces full window redraw

#### `src/search.c`
- Find Next button: Changed `SetFocus(g_find_edit)` to `SetFocus(g_find_parent)`
- `WM_DESTROY`: Now restores focus to parent window before clearing globals

#### `src/render.c`
- **Major rewrite**: Selection now drawn in 3 segments per line
  - Segment 1: Before selection (black text)
  - Segment 2: Selected portion (white text, blue background)
  - Segment 3: After selection (black text)
- Removed unused `render_draw_line()` helper function
- Prevents entire-line-turning-white bug

### Build

- ✅ Clean build with no warnings
- ✅ All existing functionality preserved
- ✅ Binary size unchanged

### Upgrade Recommendation

**Recommended for all users**, especially those experiencing:
- Hangs when closing the application
- Invisible text during selection
- Focus issues with Find dialog
- Problems typing in new tabs

### SHA256 Checksum

*(To be filled after build)*

---

**Full Changelog**: https://github.com/mcpe500/fastpad/compare/v1.1.1...v1.1.2
