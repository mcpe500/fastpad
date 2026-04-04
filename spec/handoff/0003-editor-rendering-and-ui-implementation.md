# 0003. Editor Rendering and UI Implementation

## Modules: render.c, editor.c, app.c

**Status**: Complete and compiled successfully  
**Lines of code**: ~1400 combined  
**Dependencies**: Win32 API, GDI, Common Controls

## What This Handoff Covers

This document explains the rendering pipeline, editor state management, and UI implementation. Critical for understanding:

1. How text is displayed on screen
2. How user input is processed
3. How the window and controls are managed
4. How to extend or fix rendering issues

## Rendering Pipeline (render.c)

### Overview

Rendering uses GDI (Graphics Device Interface) for text output. The pipeline is:

```
WM_PAINT → render_paint() → Select font → Fill background → Draw lines → Draw caret
```

### Initialization

```c
bool render_init(Editor *editor);
```

Creates monospace font:
- **Font face**: Consolas (fallback: system fixed font)
- **Height**: 16 pixels
- **Width**: 8 pixels (average)
- **Style**: Normal weight, no italic/bold

**Note**: Hardcoded values. Could be made configurable later.

### Metrics Calculation

```c
void render_calc_metrics(Editor *editor, HDC hdc);
```

Uses `GetTextMetrics()` to get actual font dimensions from DC. Important for:
- Line height (tmHeight)
- Character width (tmAveCharWidth)
- Accurate text placement

### Resize Handling

```c
void render_resize(Editor *editor, int width, int height);
```

Calculates:
- `visible_lines = height / line_height`
- `visible_cols = width / char_width`
- Clamps scroll position to valid range

### Paint Function

```c
void render_paint(Editor *editor, HDC hdc, const RECT *update_rect);
```

**Steps:**

1. **Select font** into DC
2. **Fill background** with white brush
3. **Calculate visible line range** from scroll_y
4. **For each visible line:**
   - Get line text from buffer
   - Check if line has selection
   - Draw selection background (blue) if needed
   - Draw text (black or white on selection)
5. **Draw caret** if window has focus

**Optimization**: Only renders visible lines, not entire buffer.

### Selection Painting

Selection is drawn with:
- **Background**: RGB(0, 120, 215) - Windows blue
- **Text**: RGB(255, 255, 255) - White

**Algorithm:**
1. Get selection start/end (normalized: start <= end)
2. Convert positions to line/column
3. For each line in selection range:
   - Calculate pixel coordinates
   - Fill rectangle with selection brush

### Caret Rendering

Uses Windows caret API:
- `CreateCaret()` - Create caret (1px width, line_height height)
- `SetCaretPos()` - Position caret
- `ShowCaret()` / `HideCaret()` - Visibility

**Visibility rules:**
- Show when window has focus
- Hide when window loses focus or caret off-screen

## Editor State (editor.c)

### Editor Structure

```c
typedef struct {
    GapBuffer buffer;      // Text storage
    Selection selection;   // Selection range (start, end)
    TextPos caret;         // Current caret position
    bool modified;         // File has unsaved changes
    UndoHistory undo;      // Undo stack
    UndoHistory redo;      // Redo stack
    Viewport viewport;     // Scroll position, visible area
    HWND hwnd;             // Window handle
    HFONT font;            // Font handle
    int font_height;       // Font height in pixels
    int char_width;        // Average char width in pixels
} Editor;
```

### Caret Movement

Handled in `editor_key_down()`:

| Key | Behavior |
|-----|----------|
| Left | Move left 1 char (or word if Ctrl) |
| Right | Move right 1 char (or word if Ctrl) |
| Up | Move to same column on previous line |
| Down | Move to same column on next line |
| Home | Go to line start (or file start if Ctrl) |
| End | Go to line end (or file end if Ctrl) |
| PageUp | Scroll up one page |
| PageDown | Scroll down one page |

**With Shift**: Extends selection instead of clearing it.

### Text Input

```c
void editor_char_input(Editor *editor, char ch);
```

**Behavior:**
1. If selection exists, delete it first (with undo record)
2. Insert character at caret
3. Add to undo history
4. Move caret forward
5. Clear selection
6. Mark as modified
7. Scroll to caret
8. Invalidate window (trigger redraw)

### Selection Logic

**Types of selection:**
1. **Keyboard**: Arrow keys with Shift
2. **Mouse**: Click (clear) or drag (extend)

**Selection representation:**
- `selection.start` and `selection.end` are byte offsets
- Order doesn't matter (can be start > end)
- Normalized when used (swap if needed)

### Clipboard Operations

#### Copy

```c
bool editor_copy(Editor *editor);
```

1. Get selected text from buffer
2. Convert UTF-8 to UTF-16
3. Open clipboard
4. Set CF_UNICODETEXT data
5. Close clipboard

#### Cut

Same as copy + delete selection.

#### Paste

```c
bool editor_paste(Editor *editor);
```

1. Open clipboard
2. Get CF_UNICODETEXT data
3. Convert UTF-16 to UTF-8
4. Delete selection if present
5. Insert text at caret
6. Close clipboard

**Important**: All clipboard data is UTF-16 (Windows native).

### Undo/Redo

#### Data Structure

```c
typedef struct {
    UndoOp *ops;         // Array of operations
    int count;           // Number of operations
    int capacity;        // Allocated capacity
    int current;         // Current position (for redo)
    int max_ops;         // Maximum operations to keep (1000)
} UndoHistory;
```

#### Operation Types

```c
typedef enum {
    OP_INSERT,   // Text was inserted
    OP_DELETE    // Text was deleted
} UndoOpType;
```

Each operation stores:
- Type (insert/delete)
- Position where it occurred
- Text that was inserted/deleted
- Length of text

#### Undo Algorithm

```c
bool editor_undo(Editor *editor);
```

1. Get previous operation from undo stack
2. Create inverse operation in redo stack
3. Execute inverse:
   - Undo insert → delete text
   - Undo delete → insert text
4. Move caret to operation position
5. Clear selection
6. Redraw

#### Redo Algorithm

```c
bool editor_redo(Editor *editor);
```

1. Get next operation from redo stack
2. Move it back to undo stack
3. Execute operation:
   - Redo insert → insert text
   - Redo delete → delete text
4. Move caret
5. Redraw

**Bounded history**: Max 1000 operations. Oldest operations are dropped.

**Limitation**: Each keystroke creates separate operation. Could be improved to group typing.

## Window Management (app.c)

### Window Creation

```c
HWND app_create_window(App *app, HINSTANCE hInstance);
```

**Window class:**
- Name: "FastPadWindowClass"
- Style: CS_HREDRAW | CS_VREDRAW (redraw on resize)
- Background: COLOR_WINDOW + 1
- Icon: Standard application icon

**Window style:**
- WS_OVERLAPPEDWINDOW (standard window with title bar, borders, sysmenu)
- Size: 800x600 (default)
- Position: CW_USEDEFAULT (let Windows choose)

### Menu Structure

Created in `app_create_window()`:

```
File          Edit          View          Help
├─ New        ├─ Undo       ├─ Word Wrap  └─ About
├─ Open...    ├─ Redo       └─ Status Bar
├─ Save       ├─ ---
├─ Save As... ├─ Cut
├─ ---        ├─ Copy
└─ Exit       ├─ Paste
              ├─ ---
              ├─ Select All
              └─ Find...
```

**Menu accelerators:**
- Displayed as tab-separated text (e.g., "Save\tCtrl+S")
- Actual handling in WM_KEYDOWN

### Status Bar

Created as `msctls_statusbar32` common control.

**Parts:** 3 sections (1/3, 2/3, rest)

**Display:**
- Part 0: "Ln X, Col Y  Modified"
- Parts 1-2: Empty (reserved for future use)

**Update trigger:** On paint, resize, caret movement

### Window Procedure

```c
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
```

**Message handling:**

| Message | Handler | Purpose |
|---------|---------|---------|
| WM_CREATE | Inline | Initialize editor, status bar |
| WM_SIZE | app_on_size | Resize editor/status bar |
| WM_PAINT | app_on_paint | Render editor |
| WM_CHAR | app_on_char | Text input |
| WM_KEYDOWN | app_on_keydown | Special keys, shortcuts |
| WM_LBUTTONDOWN | app_on_lbuttondown | Caret placement |
| WM_MOUSEMOVE | app_on_mousemove | Selection dragging |
| WM_LBUTTONUP | app_on_lbuttonup | End selection |
| WM_SETFOCUS | app_on_setfocus | Show caret |
| WM_KILLFOCUS | Inline | Hide caret |
| WM_MOUSEWHEEL | app_on_mousewheel | Scroll with mouse wheel |
| WM_COMMAND | app_on_command | Menu items |
| WM_CLOSE | app_check_unsaved | Prompt before close |
| WM_DESTROY | app_free | Cleanup |

## Input Handling

### Keyboard Shortcuts

Handled in `app_on_keydown()`:

```c
bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
```

**Ctrl shortcuts:**
- Ctrl+N: New file
- Ctrl+O: Open file
- Ctrl+S: Save
- Ctrl+Shift+S: Save As
- Ctrl+F: Find
- Ctrl+A: Select All
- Ctrl+Z: Undo
- Ctrl+Y: Redo
- Ctrl+X: Cut
- Ctrl+C: Copy
- Ctrl+V: Paste

**Other keys:**
- Passed to `editor_key_down()` for editing behavior

### Mouse Input

**Left click:**
- Places caret at click position
- With Shift: extends selection
- Clears existing selection (without Shift)

**Mouse drag:**
- Extends selection from click point
- Continuous update during drag

**Mouse wheel:**
- Scrolls vertically by lines
- Delta-based (WHEEL_DELTA = 120)

## Find Dialog (search.c)

### Implementation

Simple modeless dialog created with `CreateWindowEx()`.

**Controls:**
- Edit box: Search text input
- "Find Next" button: Search forward
- "Match case" checkbox: Case-sensitive toggle
- "Close" button: Dismiss dialog

**Behavior:**
- Searches from current caret position
- Wraps around to beginning if not found
- Selects found text and scrolls to it
- Can be reopened (brings to front if already open)

**Limitation**: Not a proper modal dialog. Could be improved with DialogBox and resource template.

## File Lifecycle (app.c)

### New File

```c
void app_file_new(App *app);
```

1. Check for unsaved changes (prompt if needed)
2. Free and reinitialize buffer
3. Reset caret and selection
4. Clear modified flag
5. Set filename to "Untitled"
6. Update window title
7. Redraw

### Open File

```c
void app_file_open(App *app);
```

1. Check for unsaved changes
2. Show Open dialog (GetOpenFileName)
3. Load file into buffer (file_load)
4. Reset caret and selection
5. Store full path in app->filename
6. Clear modified flag
7. Update window title
8. Redraw

### Save File

```c
void app_file_save(App *app);
```

If filename is "Untitled":
- Show Save As dialog

Else:
- Save to current file path
- Clear modified flag
- Update window title

### Save As

```c
void app_file_save_as(App *app);
```

1. Show Save As dialog (GetSaveFileName)
2. Save file
3. Update filename
4. Clear modified flag
5. Update title

### Unsaved Changes Prompt

```c
bool app_check_unsaved(App *app);
```

Shows MessageBox with Yes/No/Cancel:
- **Yes**: Save file, return true if save succeeded
- **No**: Discard changes, return true
- **Cancel**: Don't close, return false

## Window Title

Format:
- Modified: `*filename - FastPad`
- Clean: `filename - FastPad`

Basename extracted from full path (after last backslash).

## Known Issues

### Rendering
1. **No horizontal scroll bar**: Only vertical scrolling
2. **Flicker possible**: No double buffering yet
3. **Font not configurable**: Hardcoded to Consolas
4. **No word wrap**: Text can extend beyond visible width

### Editor
1. **Undo groups by operation**: Each keystroke is separate undo step
2. **No column mode selection**: Only linear selection
3. **No drag-and-drop**: File opening only via menu

### Performance
1. **Line operations are O(n)**: No line cache
2. **Selection redraws all lines**: Could optimize to visible only
3. **No dirty region tracking**: Always full repaint

## Future Improvements

See `0004-next-steps-and-improvements.md` for:
- Double buffering to reduce flicker
- Word wrap implementation
- Configurable fonts
- Undo grouping
- Line number margin
- Horizontal scrolling
- Better find dialog with proper resources

## Testing Notes

- ✅ Compilation successful (zero errors)
- ✅ Menu structure verified
- ✅ Keyboard shortcuts all wired up
- ✅ Clipboard integration uses correct format (UTF-16)
- ⚠️ No runtime testing yet (requires Windows environment)

**Potential issues:**
1. Font might not render correctly on all systems
2. Selection might have off-by-one errors
3. Caret visibility logic might have edge cases
4. Mouse drag selection might need debouncing

## Related Files

- `src/render.c` - GDI rendering
- `src/render.h` - Render function declarations
- `src/editor.c` - Editor logic
- `src/editor.h` - Editor function declarations
- `src/app.c` - Window management
- `src/app.h` - App function declarations
- `src/search.c` - Find dialog
