#include "editor.h"
#include "buffer.h"
#include "render.h"
#include <stdlib.h>
#include <string.h>

#define UNDO_MAX_OPS 1000
#define UNDO_MAX_TEXT 10000

bool editor_init(Editor *editor, HWND hwnd) {
    memset(editor, 0, sizeof(Editor));
    
    if (!buffer_init(&editor->buffer, 4096)) {
        return false;
    }
    
    editor->hwnd = hwnd;
    editor->caret = 0;
    editor->selection.start = 0;
    editor->selection.end = 0;
    editor->modified = false;
    
    // Initialize undo/redo
    editor->undo.ops = (UndoOp *)malloc(sizeof(UndoOp) * UNDO_MAX_OPS);
    editor->undo.count = 0;
    editor->undo.capacity = UNDO_MAX_OPS;
    editor->undo.current = 0;
    editor->undo.max_ops = UNDO_MAX_OPS;
    
    editor->redo.ops = (UndoOp *)malloc(sizeof(UndoOp) * UNDO_MAX_OPS);
    editor->redo.count = 0;
    editor->redo.capacity = UNDO_MAX_OPS;
    editor->redo.current = 0;
    editor->redo.max_ops = UNDO_MAX_OPS;
    
    return true;
}

void editor_free(Editor *editor) {
    buffer_free(&editor->buffer);
    
    // Free undo history
    for (int i = 0; i < editor->undo.count; i++) {
        if (editor->undo.ops[i].text) {
            free(editor->undo.ops[i].text);
        }
    }
    if (editor->undo.ops) {
        free(editor->undo.ops);
    }
    
    // Free redo history
    for (int i = 0; i < editor->redo.count; i++) {
        if (editor->redo.ops[i].text) {
            free(editor->redo.ops[i].text);
        }
    }
    if (editor->redo.ops) {
        free(editor->redo.ops);
    }
    
    render_free(editor);
}

static void editor_add_undo_op(Editor *editor, UndoOpType type, TextPos pos, const char *text, int length) {
    // Clear redo history when new operation is added
    for (int i = 0; i < editor->redo.count; i++) {
        if (editor->redo.ops[i].text) {
            free(editor->redo.ops[i].text);
            editor->redo.ops[i].text = NULL;
        }
    }
    editor->redo.count = 0;
    editor->redo.current = 0;

    // Add to undo history
    if (editor->undo.count >= editor->undo.max_ops) {
        // Remove oldest operation - free text BEFORE shifting
        if (editor->undo.ops[0].text) {
            free(editor->undo.ops[0].text);
            editor->undo.ops[0].text = NULL;
        }

        for (int i = 1; i < editor->undo.count; i++) {
            editor->undo.ops[i-1] = editor->undo.ops[i];
        }
        editor->undo.count--;
    }

    UndoOp *op = &editor->undo.ops[editor->undo.count];
    op->type = type;
    op->pos = pos;
    op->length = length;
    op->text = (char *)malloc(length + 1);

    if (op->text && text) {
        memcpy(op->text, text, length);
        op->text[length] = '\0';
    } else if (!op->text) {
        // malloc failed - don't count this operation
        op->length = 0;
        op->type = OP_DELETE;
        op->pos = 0;
        return;
    }

    editor->undo.count++;
    editor->undo.current = editor->undo.count;
}

void editor_set_modified(Editor *editor, bool modified) {
    editor->modified = modified;
}

bool editor_has_selection(Editor *editor) {
    return editor->selection.start != editor->selection.end;
}

void editor_get_selection(Editor *editor, TextPos *out_start, TextPos *out_end) {
    TextPos start = editor->selection.start;
    TextPos end = editor->selection.end;
    
    if (start > end) {
        TextPos temp = start;
        start = end;
        end = temp;
    }
    
    if (out_start) *out_start = start;
    if (out_end) *out_end = end;
}

void editor_clear_selection(Editor *editor) {
    editor->selection.start = editor->caret;
    editor->selection.end = editor->caret;
    InvalidateRect(editor->hwnd, NULL, FALSE);
}

void editor_select_all(Editor *editor) {
    editor->selection.start = 0;
    editor->selection.end = editor->buffer.size;
    editor->caret = editor->buffer.size;
    editor_scroll_to_caret(editor);
    InvalidateRect(editor->hwnd, NULL, FALSE);
}

void editor_char_input(Editor *editor, char ch) {
    // Delete selection if present
    if (editor_has_selection(editor)) {
        TextPos start, end;
        editor_get_selection(editor, &start, &end);
        
        char *deleted_text = buffer_get_text(&editor->buffer, start, end);
        if (deleted_text) {
            int deleted_length = end - start;
            editor_add_undo_op(editor, OP_DELETE, start, deleted_text, deleted_length);
            free(deleted_text);
        }
        
        buffer_delete(&editor->buffer, start, end - start);
        editor->caret = start;
        editor->selection.start = start;
        editor->selection.end = start;
    }
    
    // Insert character
    if (buffer_insert(&editor->buffer, editor->caret, &ch, 1)) {
        editor_add_undo_op(editor, OP_INSERT, editor->caret, &ch, 1);
        editor->caret++;
        editor_clear_selection(editor);
        editor_set_modified(editor, true);
    }
    
    editor_scroll_to_caret(editor);
    InvalidateRect(editor->hwnd, NULL, FALSE);
}

void editor_key_down(Editor *editor, int key) {
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    
    switch (key) {
        case VK_LEFT:
            if (ctrl) {
                // Move to previous word
                while (editor->caret > 0) {
                    char c = buffer_get_char(&editor->buffer, editor->caret - 1);
                    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                        editor->caret--;
                    } else {
                        break;
                    }
                }
                while (editor->caret > 0) {
                    char c = buffer_get_char(&editor->buffer, editor->caret - 1);
                    if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                        editor->caret--;
                    } else {
                        break;
                    }
                }
            } else if (editor->caret > 0) {
                editor->caret--;
            }
            
            if (!shift) {
                editor_clear_selection(editor);
            } else {
                editor->selection.end = editor->caret;
            }
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
            
        case VK_RIGHT:
            if (ctrl) {
                // Move to next word
                while (editor->caret < editor->buffer.size) {
                    char c = buffer_get_char(&editor->buffer, editor->caret);
                    if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                        editor->caret++;
                    } else {
                        break;
                    }
                }
                while (editor->caret < editor->buffer.size) {
                    char c = buffer_get_char(&editor->buffer, editor->caret);
                    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                        editor->caret++;
                    } else {
                        break;
                    }
                }
            } else if (editor->caret < editor->buffer.size) {
                editor->caret++;
            }
            
            if (!shift) {
                editor_clear_selection(editor);
            } else {
                editor->selection.end = editor->caret;
            }
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
            
        case VK_UP: {
            LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
            if (lc.line > 0) {
                TextPos line_start = buffer_line_start(&editor->buffer, lc.line - 1);
                TextPos line_end = buffer_line_end(&editor->buffer, lc.line - 1);
                int line_len = line_end - line_start;
                
                if (lc.col > line_len) {
                    editor->caret = line_end;
                } else {
                    editor->caret = line_start + lc.col;
                }
            } else {
                editor->caret = 0;
            }
            
            if (!shift) {
                editor_clear_selection(editor);
            } else {
                editor->selection.end = editor->caret;
            }
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
        }
            
        case VK_DOWN: {
            LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
            int total_lines = buffer_line_count(&editor->buffer);
            
            if (lc.line < total_lines - 1) {
                TextPos line_start = buffer_line_start(&editor->buffer, lc.line + 1);
                TextPos line_end = buffer_line_end(&editor->buffer, lc.line + 1);
                int line_len = line_end - line_start;
                
                if (lc.col > line_len) {
                    editor->caret = line_end;
                } else {
                    editor->caret = line_start + lc.col;
                }
            } else {
                editor->caret = editor->buffer.size;
            }
            
            if (!shift) {
                editor_clear_selection(editor);
            } else {
                editor->selection.end = editor->caret;
            }
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
        }
            
        case VK_HOME:
            if (ctrl) {
                editor->caret = 0;
            } else {
                LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
                editor->caret = buffer_line_start(&editor->buffer, lc.line);
            }
            
            if (!shift) {
                editor_clear_selection(editor);
            } else {
                editor->selection.end = editor->caret;
            }
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
            
        case VK_END:
            if (ctrl) {
                editor->caret = editor->buffer.size;
            } else {
                LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
                editor->caret = buffer_line_end(&editor->buffer, lc.line);
            }
            
            if (!shift) {
                editor_clear_selection(editor);
            } else {
                editor->selection.end = editor->caret;
            }
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
            
        case VK_PRIOR: // Page Up
            editor->caret -= editor->viewport.visible_cols * editor->viewport.line_height;
            if (editor->caret < 0) {
                editor->caret = 0;
            }
            
            if (!shift) {
                editor_clear_selection(editor);
            } else {
                editor->selection.end = editor->caret;
            }
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
            
        case VK_NEXT: // Page Down
            editor->caret += editor->viewport.visible_cols * editor->viewport.line_height;
            if (editor->caret > editor->buffer.size) {
                editor->caret = editor->buffer.size;
            }
            
            if (!shift) {
                editor_clear_selection(editor);
            } else {
                editor->selection.end = editor->caret;
            }
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
            
        case VK_BACK: {
            if (editor_has_selection(editor)) {
                TextPos start, end;
                editor_get_selection(editor, &start, &end);
                
                // Save deleted text for undo BEFORE deleting
                char *deleted_text = buffer_get_text(&editor->buffer, start, end);
                int deleted_length = end - start;
                
                buffer_delete(&editor->buffer, start, deleted_length);
                
                if (deleted_text) {
                    editor_add_undo_op(editor, OP_DELETE, start, deleted_text, deleted_length);
                    free(deleted_text);
                }
                
                editor->caret = start;
                editor->selection.start = start;
                editor->selection.end = start;
            } else if (editor->caret > 0) {
                // Save character BEFORE deleting from buffer
                char deleted = buffer_get_char(&editor->buffer, editor->caret - 1);
                
                // Delete the character
                buffer_delete(&editor->buffer, editor->caret - 1, 1);
                
                // Add undo (delete operation means we can undo by re-inserting)
                editor_add_undo_op(editor, OP_DELETE, editor->caret - 1, &deleted, 1);
                
                // Move caret back
                editor->caret--;
                editor->selection.start = editor->caret;
                editor->selection.end = editor->caret;
            }
            
            editor_set_modified(editor, true);
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
        }
            
        case VK_DELETE: {
            if (editor_has_selection(editor)) {
                TextPos start, end;
                editor_get_selection(editor, &start, &end);
                
                // Save deleted text for undo BEFORE deleting
                char *deleted_text = buffer_get_text(&editor->buffer, start, end);
                int deleted_length = end - start;
                
                buffer_delete(&editor->buffer, start, deleted_length);
                
                if (deleted_text) {
                    editor_add_undo_op(editor, OP_DELETE, start, deleted_text, deleted_length);
                    free(deleted_text);
                }
                
                editor->caret = start;
                editor->selection.start = start;
                editor->selection.end = start;
            } else if (editor->caret < editor->buffer.size) {
                // Save character BEFORE deleting from buffer
                char deleted = buffer_get_char(&editor->buffer, editor->caret);
                
                // Delete the character
                buffer_delete(&editor->buffer, editor->caret, 1);
                
                // Add undo
                editor_add_undo_op(editor, OP_DELETE, editor->caret, &deleted, 1);
                
                // Selection stays at caret (doesn't move on delete)
                editor->selection.start = editor->caret;
                editor->selection.end = editor->caret;
            }
            
            editor_set_modified(editor, true);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
        }
            
        case VK_RETURN: {
            char newline = '\n';
            
            if (editor_has_selection(editor)) {
                TextPos start, end;
                editor_get_selection(editor, &start, &end);
                
                char *deleted_text = buffer_get_text(&editor->buffer, start, end);
                if (deleted_text) {
                    int deleted_length = end - start;
                    editor_add_undo_op(editor, OP_DELETE, start, deleted_text, deleted_length);
                    free(deleted_text);
                }
                
                buffer_delete(&editor->buffer, start, end - start);
                editor->caret = start;
                editor->selection.start = start;
                editor->selection.end = start;
            }
            
            if (buffer_insert(&editor->buffer, editor->caret, &newline, 1)) {
                editor_add_undo_op(editor, OP_INSERT, editor->caret, &newline, 1);
                editor->caret++;
                editor_clear_selection(editor);
                editor_set_modified(editor, true);
            }
            
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
        }
            
        case VK_TAB: {
            char tab = '\t';
            
            if (editor_has_selection(editor)) {
                TextPos start, end;
                editor_get_selection(editor, &start, &end);
                
                char *deleted_text = buffer_get_text(&editor->buffer, start, end);
                if (deleted_text) {
                    int deleted_length = end - start;
                    editor_add_undo_op(editor, OP_DELETE, start, deleted_text, deleted_length);
                    free(deleted_text);
                }
                
                buffer_delete(&editor->buffer, start, end - start);
                editor->caret = start;
                editor->selection.start = start;
                editor->selection.end = start;
            }
            
            if (buffer_insert(&editor->buffer, editor->caret, &tab, 1)) {
                editor_add_undo_op(editor, OP_INSERT, editor->caret, &tab, 1);
                editor->caret++;
                editor_clear_selection(editor);
                editor_set_modified(editor, true);
            }
            
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
        }
    }
}

void editor_mouse_click(Editor *editor, int x, int y, bool shift) {
    int line = render_y_to_line(editor, y);
    int col = x / editor->viewport.char_width + editor->viewport.scroll_x;
    
    TextPos pos = buffer_linecol_to_pos(&editor->buffer, (LineCol){line, col});
    
    // Clamp to valid position
    if (pos > editor->buffer.size) {
        pos = editor->buffer.size;
    }
    
    editor->caret = pos;
    
    if (shift) {
        editor->selection.end = pos;
    } else {
        editor_clear_selection(editor);
    }
    
    editor_scroll_to_caret(editor);
    InvalidateRect(editor->hwnd, NULL, FALSE);
}

void editor_mouse_drag(Editor *editor, int x, int y) {
    int line = render_y_to_line(editor, y);
    int col = x / editor->viewport.char_width + editor->viewport.scroll_x;
    
    TextPos pos = buffer_linecol_to_pos(&editor->buffer, (LineCol){line, col});
    
    // Clamp to valid position
    if (pos > editor->buffer.size) {
        pos = editor->buffer.size;
    }
    
    editor->caret = pos;
    editor->selection.end = pos;
    
    InvalidateRect(editor->hwnd, NULL, FALSE);
}

bool editor_copy(Editor *editor) {
    if (!editor_has_selection(editor)) {
        return false;
    }
    
    TextPos start, end;
    editor_get_selection(editor, &start, &end);
    
    char *text = buffer_get_text(&editor->buffer, start, end);
    if (!text) {
        return false;
    }
    
    int length = end - start;
    
    // Convert to UTF-16 for clipboard
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text, length, NULL, 0);
    if (wlen == 0) {
        free(text);
        return false;
    }
    
    wchar_t *wtext = (wchar_t *)malloc((wlen + 1) * sizeof(wchar_t));
    if (!wtext) {
        free(text);
        return false;
    }
    
    MultiByteToWideChar(CP_UTF8, 0, text, length, wtext, wlen);
    wtext[wlen] = L'\0';
    free(text);
    
    // Set clipboard data
    if (OpenClipboard(editor->hwnd)) {
        EmptyClipboard();
        
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t *mem = (wchar_t *)GlobalLock(hMem);
            memcpy(mem, wtext, (wlen + 1) * sizeof(wchar_t));
            GlobalUnlock(hMem);
            
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        
        CloseClipboard();
    }
    
    free(wtext);
    return true;
}

bool editor_cut(Editor *editor) {
    if (!editor_copy(editor)) {
        return false;
    }
    
    TextPos start, end;
    editor_get_selection(editor, &start, &end);
    
    char *deleted_text = buffer_get_text(&editor->buffer, start, end);
    if (deleted_text) {
        int deleted_length = end - start;
        editor_add_undo_op(editor, OP_DELETE, start, deleted_text, deleted_length);
        free(deleted_text);
    }
    
    buffer_delete(&editor->buffer, start, end - start);
    editor->caret = start;
    editor->selection.start = start;
    editor->selection.end = start;
    
    editor_set_modified(editor, true);
    InvalidateRect(editor->hwnd, NULL, FALSE);
    
    return true;
}

bool editor_paste(Editor *editor) {
    if (!OpenClipboard(editor->hwnd)) {
        return false;
    }
    
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) {
        CloseClipboard();
        return false;
    }
    
    wchar_t *wtext = (wchar_t *)GlobalLock(hData);
    if (!wtext) {
        CloseClipboard();
        return false;
    }
    
    int wlen = (int)wcslen(wtext);
    int len = WideCharToMultiByte(CP_UTF8, 0, wtext, wlen, NULL, 0, NULL, NULL);
    
    char *text = (char *)malloc(len + 1);
    if (!text) {
        GlobalUnlock(hData);
        CloseClipboard();
        return false;
    }
    
    WideCharToMultiByte(CP_UTF8, 0, wtext, wlen, text, len, NULL, NULL);
    text[len] = '\0';
    
    GlobalUnlock(hData);
    CloseClipboard();
    
    // Delete selection if present
    if (editor_has_selection(editor)) {
        TextPos start, end;
        editor_get_selection(editor, &start, &end);
        
        char *deleted_text = buffer_get_text(&editor->buffer, start, end);
        if (deleted_text) {
            int deleted_length = end - start;
            editor_add_undo_op(editor, OP_DELETE, start, deleted_text, deleted_length);
            free(deleted_text);
        }
        
        buffer_delete(&editor->buffer, start, end - start);
        editor->caret = start;
        editor->selection.start = start;
        editor->selection.end = start;
    }
    
    // Insert text
    if (buffer_insert(&editor->buffer, editor->caret, text, len)) {
        editor_add_undo_op(editor, OP_INSERT, editor->caret, text, len);
        editor->caret += len;
        editor_clear_selection(editor);
        editor_set_modified(editor, true);
    }
    
    free(text);
    editor_scroll_to_caret(editor);
    InvalidateRect(editor->hwnd, NULL, FALSE);
    
    return true;
}

void editor_get_linecol(Editor *editor, int *line, int *col) {
    LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
    if (line) *line = lc.line + 1; // 1-based for display
    if (col) *col = lc.col + 1;    // 1-based for display
}

void editor_scroll_to_caret(Editor *editor) {
    LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
    
    // Scroll vertically
    if (lc.line < editor->viewport.scroll_y) {
        editor->viewport.scroll_y = lc.line;
    } else if (lc.line >= editor->viewport.scroll_y + editor->viewport.visible_lines) {
        editor->viewport.scroll_y = lc.line - editor->viewport.visible_lines + 1;
    }
    
    // Scroll horizontally
    if (lc.col < editor->viewport.scroll_x) {
        editor->viewport.scroll_x = lc.col;
    } else if (lc.col >= editor->viewport.scroll_x + editor->viewport.visible_cols) {
        editor->viewport.scroll_x = lc.col - editor->viewport.visible_cols + 1;
    }
}

void editor_resize(Editor *editor) {
    RECT rect;
    GetClientRect(editor->hwnd, &rect);
    render_resize(editor, rect.right, rect.bottom);
}

bool editor_undo(Editor *editor) {
    if (editor->undo.current <= 0) {
        return false;
    }

    editor->undo.current--;
    UndoOp *op = &editor->undo.ops[editor->undo.current];

    // Add to redo history
    if (editor->redo.count >= editor->redo.max_ops) {
        // Free oldest redo op text before shifting
        if (editor->redo.ops[0].text) {
            free(editor->redo.ops[0].text);
            editor->redo.ops[0].text = NULL;
        }
        for (int i = 1; i < editor->redo.count; i++) {
            editor->redo.ops[i-1] = editor->redo.ops[i];
        }
        editor->redo.count--;
    }

    UndoOp *redo_op = &editor->redo.ops[editor->redo.count];
    redo_op->type = (op->type == OP_INSERT) ? OP_DELETE : OP_INSERT;
    redo_op->pos = op->pos;
    redo_op->length = op->length;
    redo_op->text = (char *)malloc(op->length + 1);
    if (redo_op->text) {
        memcpy(redo_op->text, op->text, op->length);
        redo_op->text[op->length] = '\0';
    }
    editor->redo.count++;
    editor->redo.current = editor->redo.count;

    // Execute inverse operation
    if (op->type == OP_INSERT) {
        // Undo insert = delete
        buffer_delete(&editor->buffer, op->pos, op->length);
        editor->caret = op->pos;
    } else {
        // Undo delete = insert
        buffer_insert(&editor->buffer, op->pos, op->text, op->length);
        editor->caret = op->pos + op->length;
    }

    editor_clear_selection(editor);
    editor_scroll_to_caret(editor);
    InvalidateRect(editor->hwnd, NULL, FALSE);

    return true;
}

bool editor_redo(Editor *editor) {
    if (editor->redo.current <= 0) {
        return false;
    }

    editor->redo.current--;
    UndoOp *op = &editor->redo.ops[editor->redo.current];

    // Add back to undo history
    if (editor->undo.count >= editor->undo.max_ops) {
        // Free oldest undo op text before shifting
        if (editor->undo.ops[0].text) {
            free(editor->undo.ops[0].text);
            editor->undo.ops[0].text = NULL;
        }
        for (int i = 1; i < editor->undo.count; i++) {
            editor->undo.ops[i-1] = editor->undo.ops[i];
        }
        editor->undo.count--;
    }

    UndoOp *undo_op = &editor->undo.ops[editor->undo.count];
    undo_op->type = (op->type == OP_INSERT) ? OP_DELETE : OP_INSERT;
    undo_op->pos = op->pos;
    undo_op->length = op->length;
    undo_op->text = (char *)malloc(op->length + 1);
    if (undo_op->text) {
        memcpy(undo_op->text, op->text, op->length);
        undo_op->text[op->length] = '\0';
    }
    editor->undo.count++;
    editor->undo.current = editor->undo.count;

    // Execute the redo operation (keep text in redo stack for future redo)
    if (op->type == OP_INSERT) {
        buffer_insert(&editor->buffer, op->pos, op->text, op->length);
        editor->caret = op->pos + op->length;
    } else {
        buffer_delete(&editor->buffer, op->pos, op->length);
        editor->caret = op->pos;
    }

    editor_clear_selection(editor);
    editor_scroll_to_caret(editor);
    InvalidateRect(editor->hwnd, NULL, FALSE);

    return true;
}
