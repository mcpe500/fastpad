#include "editor.h"
#include "buffer.h"
#include "render.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <shlwapi.h>

#define UNDO_MAX_OPS 1000
#define UNDO_MAX_TEXT 10000

// Helper: generate a simple hash for a filename to create a unique history file
static void get_history_filename(const char *filename, char *out_path) {
    char temp_path[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_path);
    
    unsigned long hash = 5381;
    int c;
    const char *str = filename;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    
    sprintf(out_path, "%sfastpad_undo_%lx.bin", temp_path, hash);
}

bool editor_save_history(Editor *editor, const char *filename) {
    if (!filename || editor->undo.count == 0) return false;
    
    char path[MAX_PATH];
    get_history_filename(filename, path);
    
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    
    fwrite(&editor->undo.count, sizeof(int), 1, f);
    fwrite(&editor->undo.current, sizeof(int), 1, f);
    
    for (int i = 0; i < editor->undo.count; i++) {
        UndoOp *op = &editor->undo.ops[i];
        fwrite(&op->type, sizeof(UndoOpType), 1, f);
        fwrite(&op->pos, sizeof(TextPos), 1, f);
        fwrite(&op->length, sizeof(int), 1, f);
        fwrite(&op->replace_len, sizeof(int), 1, f);
        fwrite(op->text, 1, op->length, f);
    }
    
    fclose(f);
    return true;
}

bool editor_load_history(Editor *editor, const char *filename) {
    if (!filename) return false;
    
    char path[MAX_PATH];
    get_history_filename(filename, path);
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    int count, current;
    if (fread(&count, sizeof(int), 1, f) != 1) { fclose(f); return false; }
    if (fread(&current, sizeof(int), 1, f) != 1) { fclose(f); return false; }
    
    if (count > UNDO_MAX_OPS) count = UNDO_MAX_OPS;
    
    // Free existing history if any
    for (int i = 0; i < editor->undo.count; i++) {
        if (editor->undo.ops[i].text) free(editor->undo.ops[i].text);
    }
    
    editor->undo.count = 0;
    editor->undo.current = current;
    
    for (int i = 0; i < count; i++) {
        UndoOp op;
        if (fread(&op.type, sizeof(UndoOpType), 1, f) != 1) break;
        if (fread(&op.pos, sizeof(TextPos), 1, f) != 1) break;
        if (fread(&op.length, sizeof(int), 1, f) != 1) break;
        if (fread(&op.replace_len, sizeof(int), 1, f) != 1) break;
        
        op.text = (char *)malloc(op.length + 1);
        if (op.text) {
            fread(op.text, 1, op.length, f);
            op.text[op.length] = '\0';
            
            editor->undo.ops[editor->undo.count++] = op;
        }
    }
    
    fclose(f);
    return true;
}

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
    
    // Initialize multi-cursor (none active by default)
    editor->cursor_count = 0;
    
    // Initialize column selection
    editor->col_selection.active = false;
    editor->col_selection.start = 0;
    editor->col_selection.end = 0;
    editor->col_selection.start_col = 0;
    editor->col_selection.end_col = 0;
    
    // Initialize bracket highlighting
    editor->bracket_match = -1;
    editor->bracket_type = 0;
    
    // Initialize folding
    editor->fold_count = 0;
    
    // Initialize autocomplete
    editor->autocomplete.visible = false;
    editor->autocomplete.count = 0;
    editor->autocomplete.selected_index = -1;
    
    // Initialize bracket auto-close
    editor->auto_close_brackets = true;
    
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
    // Check if we can group this operation with the previous one
    if (editor->undo.count > 0) {
        UndoOp *last = &editor->undo.ops[editor->undo.count - 1];
        
        if (type == OP_INSERT && last->type == OP_INSERT && last->pos + last->length == pos) {
            // Group consecutive insertions
            char *new_text = (char *)realloc(last->text, last->length + length + 1);
            if (new_text) {
                last->text = new_text;
                memcpy(last->text + last->length, text, length);
                last->text[last->length + length] = '\0';
                last->length += length;
                // Redo is already cleared by the first insertion
                return;
            }
        } else if (type == OP_DELETE && last->type == OP_DELETE && last->pos == pos + length) {
            // Group consecutive deletions (backspacing)
            char *new_text = (char *)realloc(last->text, last->length + length + 1);
            if (new_text) {
                last->text = new_text;
                // Shift existing text right, insert new text at the beginning
                memmove(last->text + length, last->text, last->length);
                memcpy(last->text, text, length);
                last->text[last->length + length] = '\0';
                last->pos = pos;
                last->length += length;
                return;
            }
        } else if (type == OP_DELETE && last->type == OP_DELETE && last->pos + last->length == pos) {
            // Group consecutive deletions (Delete key)
            char *new_text = (char *)realloc(last->text, last->length + length + 1);
            if (new_text) {
                last->text = new_text;
                memcpy(last->text + last->length, text, length);
                last->text[last->length + length] = '\0';
                last->length += length;
                return;
            }
        }
    }

    // Clear redo history when new (ungrouped) operation is added
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
    if (!modified) {
        // Document is saved - record current undo position as save point
        editor->undo_count_at_save = editor->undo.current;
    }
}

// Helper: Add a replace operation to undo history
// Replaces `replace_len` chars at `pos` with `text`
static void editor_add_undo_op_replace(Editor *editor, TextPos pos, const char *text, int text_len, int replace_len) {
    // Clear redo history
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
    op->type = OP_REPLACE;
    op->pos = pos;
    op->length = text_len;
    op->replace_len = replace_len;
    op->text = (char *)malloc(text_len + 1);
    if (op->text) {
        memcpy(op->text, text, text_len);
        op->text[text_len] = '\0';
    }

    editor->undo.count++;
    editor->undo.current = editor->undo.count;
}

// BUG #021 Fix: Extract selection deletion into helper function
// Deletes current selection and logs it as OP_DELETE undo operation
static void editor_delete_selection(Editor *editor) {
    TextPos start, end;
    editor_get_selection(editor, &start, &end);
    int sel_length = end - start;

    char *deleted_text = buffer_get_text(&editor->buffer, start, end);
    if (deleted_text) {
        editor_add_undo_op(editor, OP_DELETE, start, deleted_text, sel_length);
        free(deleted_text);
    }

    buffer_delete(&editor->buffer, start, sel_length);
    editor->caret = start;
    editor->selection.start = start;
    editor->selection.end = start;
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
    // FIX: Ensure caret is scrolled into view after clearing selection
    // Previously this was missing, causing caret to disappear after shift+arrow
    editor_scroll_to_caret(editor);
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
    #ifdef DEV_BUILD
    log_action("EDITOR_INPUT", "char: %c", ch);
    #endif
    // BUG #014 Fix: When typing with a selection, use OP_REPLACE instead of
    // separate DELETE + INSERT ops. This allows single undo to restore original text.
    if (editor_has_selection(editor)) {
        TextPos start, end;
        editor_get_selection(editor, &start, &end);
        int sel_length = end - start;

        // Save original text for undo
        char *original_text = buffer_get_text(&editor->buffer, start, end);

        // Delete selection and insert character
        buffer_delete(&editor->buffer, start, sel_length);
        buffer_insert(&editor->buffer, start, &ch, 1);

        // Create single OP_REPLACE operation (undo will restore original text in one step)
        if (original_text) {
            editor_add_undo_op_replace(editor, start, original_text, 1, sel_length);
            free(original_text);
        } else {
            editor_add_undo_op(editor, OP_INSERT, start, &ch, 1);
        }

        editor->caret = start + 1;
        editor->selection.start = editor->caret;
        editor->selection.end = editor->caret;
    } else {
        // No selection: normal insert
        if (buffer_insert(&editor->buffer, editor->caret, &ch, 1)) {
            editor_add_undo_op(editor, OP_INSERT, editor->caret, &ch, 1);
            editor->caret++;
        }
    }

    editor_set_modified(editor, true);
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
            
        case VK_PRIOR: { // Page Up
            int lines_to_move = editor->viewport.visible_lines;
            if (lines_to_move < 1) lines_to_move = 1;
            
            LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
            int new_line = lc.line - lines_to_move;
            if (new_line < 0) new_line = 0;
            
            TextPos new_pos = buffer_line_start(&editor->buffer, new_line);
            // Try to maintain column position
            TextPos line_end = buffer_line_end(&editor->buffer, new_line);
            int max_col = line_end - new_pos;
            if (lc.col > max_col) {
                new_pos = line_end;
            } else {
                new_pos += lc.col;
            }
            
            editor->caret = new_pos;
            if (!shift) {
                editor_clear_selection(editor);
            } else {
                editor->selection.end = editor->caret;
            }
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
        }

        case VK_NEXT: { // Page Down
            int lines_to_move = editor->viewport.visible_lines;
            if (lines_to_move < 1) lines_to_move = 1;
            
            LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
            int total_lines = buffer_line_count(&editor->buffer);
            int new_line = lc.line + lines_to_move;
            if (new_line >= total_lines) new_line = total_lines - 1;
            
            TextPos new_pos = buffer_line_start(&editor->buffer, new_line);
            // Try to maintain column position
            TextPos line_end = buffer_line_end(&editor->buffer, new_line);
            int max_col = line_end - new_pos;
            if (lc.col > max_col) {
                new_pos = line_end;
            } else {
                new_pos += lc.col;
            }
            
            editor->caret = new_pos;
            if (!shift) {
                editor_clear_selection(editor);
            } else {
                editor->selection.end = editor->caret;
            }
            editor_scroll_to_caret(editor);
            InvalidateRect(editor->hwnd, NULL, FALSE);
            break;
        }
            
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
                editor_delete_selection(editor);
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
                editor_delete_selection(editor);
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
                editor_delete_selection(editor);
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
    if (x < RENDER_MARGIN_WIDTH) return; // Ignore clicks in margin
    int adj_x = x - RENDER_MARGIN_WIDTH;
    int line = render_y_to_line(editor, y);
    int col = adj_x / editor->viewport.char_width + editor->viewport.scroll_x;
    
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
    if (x < RENDER_MARGIN_WIDTH) return; // Ignore drags in margin
    int adj_x = x - RENDER_MARGIN_WIDTH;
    int line = render_y_to_line(editor, y);
    int col = adj_x / editor->viewport.char_width + editor->viewport.scroll_x;
    
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

int editor_get_word_count(Editor *editor) {
    if (!editor) return 0;
    
    char *text = buffer_get_text(&editor->buffer, 0, editor->buffer.size);
    if (!text) return 0;
    
    int count = 0;
    bool in_word = false;
    
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        // Word boundary: whitespace or newline character
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            in_word = false;
        } else {
            if (!in_word) {
                count++;
                in_word = true;
            }
        }
    }
    
    free(text);
    return count;
}

int editor_get_char_count(Editor *editor) {
    if (!editor) return 0;
    return editor->buffer.size;
}

void editor_scroll_to_caret(Editor *editor) {
    LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);

    // Scroll vertically
    if (lc.line < editor->viewport.scroll_y) {
        editor->viewport.scroll_y = lc.line;
    } else if (lc.line >= editor->viewport.scroll_y + editor->viewport.visible_lines) {
        editor->viewport.scroll_y = lc.line - editor->viewport.visible_lines + 1;
    }

    // Scroll horizontally using display column (tabs expanded)
    int caret_display_col = get_caret_display_col(editor);
    if (caret_display_col < editor->viewport.scroll_x) {
        editor->viewport.scroll_x = caret_display_col;
    } else if (caret_display_col >= editor->viewport.scroll_x + editor->viewport.visible_cols) {
        editor->viewport.scroll_x = caret_display_col - editor->viewport.visible_cols + 1;
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
    redo_op->type = op->type;  // Same type for redo
    redo_op->pos = op->pos;
    redo_op->length = op->length;
    redo_op->replace_len = op->replace_len;
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
    } else if (op->type == OP_DELETE) {
        // Undo delete = insert
        buffer_insert(&editor->buffer, op->pos, op->text, op->length);
        editor->caret = op->pos + op->length;
    } else if (op->type == OP_REPLACE) {
        // Undo replace: delete replacement text, restore original text
        buffer_delete(&editor->buffer, op->pos, op->replace_len);
        buffer_insert(&editor->buffer, op->pos, op->text, op->length);
        editor->caret = op->pos + op->length;
    }

    // Check if we're back at save point
    if (editor->undo.current == editor->undo_count_at_save) {
        editor->modified = false;
    }

    editor_clear_selection(editor);
    editor_scroll_to_caret(editor);
    InvalidateRect(editor->hwnd, NULL, FALSE);

    return true;
}

bool editor_redo(Editor *editor) {
    if (editor->redo.count <= 0 || editor->redo.current >= editor->redo.count) {
        return false;
    }

    UndoOp *op = &editor->redo.ops[editor->redo.current];
    editor->redo.current++;

    // Add to undo history
    if (editor->undo.count >= editor->undo.max_ops) {
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
    undo_op->type = op->type;
    undo_op->pos = op->pos;
    undo_op->length = op->length;
    undo_op->replace_len = op->replace_len;
    undo_op->text = (char *)malloc(op->length + 1);
    if (undo_op->text) {
        memcpy(undo_op->text, op->text, op->length);
        undo_op->text[op->length] = '\0';
    }
    editor->undo.count++;
    editor->undo.current = editor->undo.count;

    // Execute operation
    if (op->type == OP_INSERT) {
        buffer_insert(&editor->buffer, op->pos, op->text, op->length);
        editor->caret = op->pos + op->length;
    } else if (op->type == OP_DELETE) {
        buffer_delete(&editor->buffer, op->pos, op->length);
        editor->caret = op->pos;
    } else if (op->type == OP_REPLACE) {
        buffer_delete(&editor->buffer, op->pos, op->replace_len);
        buffer_insert(&editor->buffer, op->pos, op->text, op->length);
        editor->caret = op->pos + op->length;
    }

    if (editor->undo.current > editor->undo_count_at_save) {
        editor->modified = true;
    }

    editor_clear_selection(editor);
    editor_scroll_to_caret(editor);
    InvalidateRect(editor->hwnd, NULL, FALSE);

    return true;
}

// ============================================================================
// Multi-Cursor Editing
// ============================================================================

void editor_add_cursor(Editor *editor, int x, int y) {
    if (x < RENDER_MARGIN_WIDTH) return;
    if (editor->cursor_count >= MAX_CURSORS) return;
    
    int adj_x = x - RENDER_MARGIN_WIDTH;
    int line = render_y_to_line(editor, y);
    int col = adj_x / editor->viewport.char_width + editor->viewport.scroll_x;
    
    TextPos pos = buffer_linecol_to_pos(&editor->buffer, (LineCol){line, col});
    if (pos > editor->buffer.size) pos = editor->buffer.size;
    
    // Check if this position already has a cursor (excluding primary)
    for (int i = 0; i < editor->cursor_count; i++) {
        if (editor->cursors[i].position == pos) return;
    }
    
    editor->cursors[editor->cursor_count].position = pos;
    editor->cursors[editor->cursor_count].display_col = col;
    editor->cursors[editor->cursor_count].line = line;
    editor->cursor_count++;
    
    InvalidateRect(editor->hwnd, NULL, FALSE);
}

void editor_add_cursor_at_line(Editor *editor, int line, int col) {
    if (editor->cursor_count >= MAX_CURSORS) return;
    
    TextPos pos = buffer_linecol_to_pos(&editor->buffer, (LineCol){line, col});
    if (pos > editor->buffer.size) pos = editor->buffer.size;
    
    for (int i = 0; i < editor->cursor_count; i++) {
        if (editor->cursors[i].position == pos) return;
    }
    
    editor->cursors[editor->cursor_count].position = pos;
    editor->cursors[editor->cursor_count].display_col = col;
    editor->cursors[editor->cursor_count].line = line;
    editor->cursor_count++;
    
    InvalidateRect(editor->hwnd, NULL, FALSE);
}

void editor_select_next_occurrence(Editor *editor) {
    // Get current word at caret
    LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
    TextPos line_start = buffer_line_start(&editor->buffer, lc.line);
    TextPos line_end = buffer_line_end(&editor->buffer, lc.line);
    
    // Find word boundaries
    TextPos pos = editor->caret;
    
    // Find start of word
    while (pos > line_start) {
        char c = buffer_get_char(&editor->buffer, pos - 1);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        pos--;
    }
    TextPos word_start = pos;
    
    // Find end of word
    pos = editor->caret;
    while (pos < line_end) {
        char c = buffer_get_char(&editor->buffer, pos);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        pos++;
    }
    TextPos word_end = pos;
    
    if (word_end <= word_start) return;
    
    // Get word text
    int word_len = (int)(word_end - word_start);
    char *word = buffer_get_text(&editor->buffer, word_start, word_end);
    if (!word || word_len <= 0) return;
    
    // Search for next occurrence
    TextPos search_start = editor->caret + 1;
    TextPos found_pos = -1;
    
    for (TextPos p = search_start; p < editor->buffer.size - word_len + 1; p++) {
        bool match = true;
        for (int i = 0; i < word_len; i++) {
            if (buffer_get_char(&editor->buffer, p + i) != word[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            found_pos = p;
            break;
        }
    }
    
    free(word);
    
    if (found_pos != -1) {
        LineCol found_lc = buffer_pos_to_linecol(&editor->buffer, found_pos);
        editor_add_cursor_at_line(editor, found_lc.line, found_lc.col);
    }
}

void editor_remove_cursor(Editor *editor, int index) {
    if (index < 0 || index >= editor->cursor_count) return;
    
    for (int i = index; i < editor->cursor_count - 1; i++) {
        editor->cursors[i] = editor->cursors[i + 1];
    }
    editor->cursor_count--;
    
    InvalidateRect(editor->hwnd, NULL, FALSE);
}

void editor_clear_extra_cursors(Editor *editor) {
    editor->cursor_count = 0;
    InvalidateRect(editor->hwnd, NULL, FALSE);
}

bool editor_has_multiple_cursors(Editor *editor) {
    return editor->cursor_count > 0;
}

// ============================================================================
// Column Selection Mode
// ============================================================================

void editor_start_column_selection(Editor *editor, int x, int y) {
    if (x < RENDER_MARGIN_WIDTH) return;
    
    int adj_x = x - RENDER_MARGIN_WIDTH;
    int line = render_y_to_line(editor, y);
    int col = adj_x / editor->viewport.char_width + editor->viewport.scroll_x;
    
    TextPos pos = buffer_linecol_to_pos(&editor->buffer, (LineCol){line, col});
    if (pos > editor->buffer.size) pos = editor->buffer.size;
    
    editor->col_selection.active = true;
    editor->col_selection.start = pos;
    editor->col_selection.end = pos;
    editor->col_selection.start_col = col;
    editor->col_selection.end_col = col;
}

void editor_update_column_selection(Editor *editor, int x, int y) {
    if (!editor->col_selection.active) return;
    
    int adj_x = x - RENDER_MARGIN_WIDTH;
    int line = render_y_to_line(editor, y);
    int col = adj_x / editor->viewport.char_width + editor->viewport.scroll_x;
    
    TextPos pos = buffer_linecol_to_pos(&editor->buffer, (LineCol){line, col});
    if (pos > editor->buffer.size) pos = editor->buffer.size;
    
    editor->col_selection.end = pos;
    editor->col_selection.end_col = col;
    
    InvalidateRect(editor->hwnd, NULL, FALSE);
}

void editor_end_column_selection(Editor *editor) {
    editor->col_selection.active = false;
}

bool editor_is_column_selection_active(Editor *editor) {
    return editor->col_selection.active;
}

// ============================================================================
// Bracket Pair Highlighting
// ============================================================================

static int get_bracket_type(char c) {
    switch (c) {
        case '(': case ')': return 1;
        case '{': case '}': return 2;
        case '[': case ']': return 3;
        case '<': case '>': return 4;
        case '"': return 5;
        case '\'': return 6;
        default: return 0;
    }
}

static char get_matching_bracket(char bracket) {
    switch (bracket) {
        case '(': return ')';
        case ')': return '(';
        case '{': return '}';
        case '}': return '{';
        case '[': return ']';
        case ']': return '[';
        case '<': return '>';
        case '>': return '<';
        case '"': return '"';
        case '\'': return '\'';
        default: return '\0';
    }
}

void editor_update_bracket_match(Editor *editor) {
    editor->bracket_match = -1;
    editor->bracket_type = 0;
    
    if (editor->caret <= 0 || editor->caret >= editor->buffer.size) return;
    
    char c = buffer_get_char(&editor->buffer, editor->caret);
    char prev_c = buffer_get_char(&editor->buffer, editor->caret - 1);
    
    int type = get_bracket_type(c);
    int prev_type = get_bracket_type(prev_c);
    
    if (type == 0 && prev_type == 0) return;
    
    // Determine which bracket to check (prefer char before caret for opening)
    char bracket = '\0';
    bool is_opening = false;
    
    if (prev_type != 0 && prev_c != c) {
        // Use previous character as it might be an opening bracket
        bracket = prev_c;
        is_opening = (prev_c == '(' || prev_c == '{' || prev_c == '[' || prev_c == '<');
    } else if (type != 0) {
        bracket = c;
        is_opening = (c == '(' || c == '{' || c == '[' || c == '<');
    }
    
    if (bracket == '\0') return;
    
    editor->bracket_type = get_bracket_type(bracket);
    char match = get_matching_bracket(bracket);
    
    // Search for matching bracket
    int depth = 1;
    TextPos start = is_opening ? editor->caret : editor->caret - 1;
    TextPos dir = is_opening ? 1 : -1;
    
    for (TextPos p = start + dir; p >= 0 && p < editor->buffer.size; p += dir) {
        char pc = buffer_get_char(&editor->buffer, p);
        
        if (pc == bracket) {
            depth++;
        } else if (pc == match) {
            depth--;
            if (depth == 0) {
                editor->bracket_match = p;
                return;
            }
        }
    }
}

TextPos editor_get_bracket_match(Editor *editor) {
    return editor->bracket_match;
}

int editor_get_bracket_type(Editor *editor) {
    return editor->bracket_type;
}

// ============================================================================
// Code Folding
// ============================================================================

void editor_detect_folds(Editor *editor) {
    editor->fold_count = 0;
    
    int line_count = buffer_line_count(&editor->buffer);
    
    for (int line = 0; line < line_count - 1 && editor->fold_count < MAX_FOLDS; line++) {
        TextPos line_start = buffer_line_start(&editor->buffer, line);
        TextPos line_end = buffer_line_end(&editor->buffer, line);
        int line_len = (int)(line_end - line_start);
        
        if (line_len <= 0) continue;
        
        char *line_text = buffer_get_text(&editor->buffer, line_start, line_end);
        if (!line_text) continue;
        
        // Skip leading whitespace
        int i = 0;
        while (i < line_len && (line_text[i] == ' ' || line_text[i] == '\t')) i++;
        
        if (i >= line_len) {
            free(line_text);
            continue;
        }
        
        // Detect foldable regions
        bool is_foldable = false;
        char fold_type = '\0';
        int start_line = line;
        int end_line = line;
        
        // Check for JSON objects/arrays
        if (line_text[i] == '{' || line_text[i] == '[') {
            char open = line_text[i];
            char close = (open == '{') ? '}' : ']';
            int depth = 1;
            
            for (int search_line = line + 1; search_line < line_count && editor->fold_count < MAX_FOLDS; search_line++) {
                TextPos sl_start = buffer_line_start(&editor->buffer, search_line);
                TextPos sl_end = buffer_line_end(&editor->buffer, search_line);
                char *sl_text = buffer_get_text(&editor->buffer, sl_start, sl_end);
                
                if (sl_text) {
                    for (int j = 0; j < (int)(sl_end - sl_start); j++) {
                        if (sl_text[j] == open) depth++;
                        else if (sl_text[j] == close) depth--;
                        
                        if (depth == 0) {
                            end_line = search_line;
                            is_foldable = true;
                            fold_type = 'j';
                            free(sl_text);
                            break;
                        }
                    }
                    free(sl_text);
                }
                
                if (depth == 0) break;
            }
        }
        // Check for markdown headers
        else if (line_text[i] == '#') {
            int level = 0;
            while (i < line_len && line_text[i] == '#' && level < 6) {
                level++;
                i++;
            }
            
            if (level > 0 && level < 6) {
                // Find next heading of same or higher level
                for (int search_line = line + 1; search_line < line_count; search_line++) {
                    TextPos sl_start = buffer_line_start(&editor->buffer, search_line);
                    TextPos sl_end = buffer_line_end(&editor->buffer, search_line);
                    char *sl_text = buffer_get_text(&editor->buffer, sl_start, sl_end);
                    
                    if (sl_text) {
                        int si = 0;
                        while (si < (int)(sl_end - sl_start) && (sl_text[si] == ' ' || sl_text[si] == '\t')) si++;
                        
                        if (si < (int)(sl_end - sl_start) && sl_text[si] == '#') {
                            int next_level = 0;
                            int si2 = si;
                            while (si2 < (int)(sl_end - sl_start) && sl_text[si2] == '#' && next_level < 6) {
                                next_level++;
                                si2++;
                            }
                            
                            if (next_level <= level) {
                                end_line = search_line - 1;
                                is_foldable = true;
                                fold_type = 'm';
                                free(sl_text);
                                break;
                            }
                        }
                        free(sl_text);
                    }
                }
            }
        }
        // Check for code blocks (braces)
        else if (line_text[i] == '{') {
            int depth = 1;
            for (int search_line = line + 1; search_line < line_count && editor->fold_count < MAX_FOLDS; search_line++) {
                TextPos sl_start = buffer_line_start(&editor->buffer, search_line);
                TextPos sl_end = buffer_line_end(&editor->buffer, search_line);
                char *sl_text = buffer_get_text(&editor->buffer, sl_start, sl_end);
                
                if (sl_text) {
                    // Skip leading whitespace
                    int si = 0;
                    while (si < (int)(sl_end - sl_start) && (sl_text[si] == ' ' || sl_text[si] == '\t')) si++;
                    
                    for (int j = si; j < (int)(sl_end - sl_start); j++) {
                        if (sl_text[j] == '{') depth++;
                        else if (sl_text[j] == '}') depth--;
                        
                        if (depth == 0) {
                            end_line = search_line;
                            is_foldable = true;
                            fold_type = 'c';
                            free(sl_text);
                            break;
                        }
                    }
                    free(sl_text);
                }
                
                if (depth == 0) break;
            }
        }
        
        if (is_foldable && end_line > start_line + 1) {
            editor->folds[editor->fold_count].start_line = start_line;
            editor->folds[editor->fold_count].end_line = end_line;
            editor->folds[editor->fold_count].collapsed = false;
            editor->folds[editor->fold_count].type = fold_type;
            editor->fold_count++;
        }
        
        free(line_text);
    }
}

void editor_toggle_fold(Editor *editor, int line) {
    for (int i = 0; i < editor->fold_count; i++) {
        if (editor->folds[i].start_line == line) {
            editor->folds[i].collapsed = !editor->folds[i].collapsed;
            InvalidateRect(editor->hwnd, NULL, FALSE);
            return;
        }
        // Also toggle if line is within the fold region
        if (line > editor->folds[i].start_line && line <= editor->folds[i].end_line) {
            editor->folds[i].collapsed = !editor->folds[i].collapsed;
            InvalidateRect(editor->hwnd, NULL, FALSE);
            return;
        }
    }
}

bool editor_is_line_folded(Editor *editor, int line) {
    for (int i = 0; i < editor->fold_count; i++) {
        if (line >= editor->folds[i].start_line && line <= editor->folds[i].end_line) {
            return editor->folds[i].collapsed;
        }
    }
    return false;
}

int editor_get_fold_end_line(Editor *editor, int line) {
    for (int i = 0; i < editor->fold_count; i++) {
        if (line >= editor->folds[i].start_line && line <= editor->folds[i].end_line) {
            return editor->folds[i].collapsed ? editor->folds[i].start_line : -1;
        }
    }
    return -1;
}

// ============================================================================
// Auto-Complete
// ============================================================================

void editor_update_autocomplete(Editor *editor) {
    editor->autocomplete.count = 0;
    
    // Only trigger after 3+ characters typed
    LineCol lc = buffer_pos_to_linecol(&editor->buffer, editor->caret);
    if (lc.col < 3) return;
    
    // Get current word
    TextPos line_start = buffer_line_start(&editor->buffer, lc.line);
    TextPos word_start = editor->caret;
    
    while (word_start > line_start) {
        char c = buffer_get_char(&editor->buffer, word_start - 1);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        word_start--;
    }
    
    int prefix_len = (int)(editor->caret - word_start);
    if (prefix_len < 3) return;
    
    // Collect words from document
    char **words = (char **)malloc(sizeof(char *) * MAX_AUTOCOMPLETE_WORDS);
    int word_count = 0;
    
    for (TextPos p = 0; p < editor->buffer.size && word_count < MAX_AUTOCOMPLETE_WORDS - 1; p++) {
        // Skip whitespace
        while (p < editor->buffer.size) {
            char c = buffer_get_char(&editor->buffer, p);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p++;
            else break;
        }
        
        if (p >= editor->buffer.size) break;
        
        // Collect word
        TextPos w_start = p;
        while (p < editor->buffer.size) {
            char c = buffer_get_char(&editor->buffer, p);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
            p++;
        }
        int w_len = (int)(p - w_start);
        
        if (w_len >= 3) {
            char *word = buffer_get_text(&editor->buffer, w_start, p);
            if (word) {
                // Check if word starts with prefix
                bool match = true;
                for (int i = 0; i < prefix_len && word[i]; i++) {
                    if (word[i] != buffer_get_char(&editor->buffer, word_start + i)) {
                        match = false;
                        break;
                    }
                }
                
                if (match) {
                    // Check if we already have this word
                    bool exists = false;
                    for (int i = 0; i < word_count; i++) {
                        if (strcmp(words[i], word) == 0) {
                            exists = true;
                            break;
                        }
                    }
                    
                    if (!exists && word_count < MAX_AUTOCOMPLETE_WORDS) {
                        words[word_count] = word;
                        word_count++;
                    } else {
                        free(word);
                    }
                } else {
                    free(word);
                }
            }
        }
    }
    
    // Copy to autocomplete structure (limit to MAX_SUGGESTIONS)
    editor->autocomplete.count = (word_count > MAX_SUGGESTIONS) ? MAX_SUGGESTIONS : word_count;
    editor->autocomplete.selected_index = 0;
    editor->autocomplete.trigger_pos = editor->caret - prefix_len;
    
    for (int i = 0; i < editor->autocomplete.count; i++) {
        strncpy(editor->autocomplete.words[i].word, words[i], 63);
        editor->autocomplete.words[i].word[63] = '\0';
        editor->autocomplete.words[i].frequency = 1;
        free(words[i]);
    }
    
    free(words);
}

void editor_show_autocomplete(Editor *editor) {
    if (editor->autocomplete.count > 0) {
        editor->autocomplete.visible = true;
    }
}

void editor_hide_autocomplete(Editor *editor) {
    editor->autocomplete.visible = false;
}

bool editor_autocomplete_is_visible(Editor *editor) {
    return editor->autocomplete.visible;
}

void editor_autocomplete_select(Editor *editor, int index) {
    if (index >= 0 && index < editor->autocomplete.count) {
        editor->autocomplete.selected_index = index;
    }
}

void editor_autocomplete_nav_next(Editor *editor) {
    if (editor->autocomplete.count > 0) {
        editor->autocomplete.selected_index++;
        if (editor->autocomplete.selected_index >= editor->autocomplete.count) {
            editor->autocomplete.selected_index = 0;
        }
    }
}

void editor_autocomplete_nav_prev(Editor *editor) {
    if (editor->autocomplete.count > 0) {
        editor->autocomplete.selected_index--;
        if (editor->autocomplete.selected_index < 0) {
            editor->autocomplete.selected_index = editor->autocomplete.count - 1;
        }
    }
}

const char* editor_autocomplete_get_selected(Editor *editor) {
    if (editor->autocomplete.selected_index >= 0 && 
        editor->autocomplete.selected_index < editor->autocomplete.count) {
        return editor->autocomplete.words[editor->autocomplete.selected_index].word;
    }
    return NULL;
}

// ============================================================================
// Bracket Auto-Close
// ============================================================================

void editor_auto_close_bracket(Editor *editor, char bracket) {
    char pair[2] = {'\0', '\0'};
    
    switch (bracket) {
        case '(': strcpy(pair, ")"); break;
        case '{': strcpy(pair, "}"); break;
        case '[': strcpy(pair, "]"); break;
        case '<': strcpy(pair, ">"); break;
        case '"': strcpy(pair, "\""); break;
        case '\'': strcpy(pair, "'"); break;
        default: return;
    }
    
    // Insert closing bracket
    if (buffer_insert(&editor->buffer, editor->caret, pair, 1)) {
        editor_add_undo_op(editor, OP_INSERT, editor->caret, pair, 1);
        editor->caret++;
    }
}
