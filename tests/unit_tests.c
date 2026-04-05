/*
 * FastPad Unit Test Suite
 * 
 * Run with: gcc tests/unit_tests.c -o test && ./test
 * 
 * Tests cover:
 * - Buffer operations (insert, delete, get_text)
 * - Editor operations (typing, backspace, undo/redo)
 * - Edge cases and boundary conditions
 * - Stress tests (10K operations)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// Mock Windows types
typedef void* HWND;
typedef void* HDC;
typedef void* HFONT;
typedef unsigned int UINT;
typedef unsigned int WPARAM;
typedef long LPARAM;

#define VK_BACK 8
#define VK_TAB 9
#define VK_RETURN 13
#define VK_DELETE 46

// Minimal types for standalone testing
typedef int64_t TextPos;
typedef struct { int line; int col; } LineCol;
typedef struct { TextPos start, end; } Selection;
typedef enum { OP_INSERT, OP_DELETE, OP_REPLACE } UndoOpType;

typedef struct {
    char *data;
    int size;
    int capacity;
    int gap_start;
    int gap_length;
} GapBuffer;

typedef struct {
    UndoOpType type;
    TextPos pos;
    char *text;
    int length;
    int replace_len;
} UndoOp;

typedef struct {
    UndoOp *ops;
    int count;
    int capacity;
    int current;
    int max_ops;
} UndoHistory;

typedef struct {
    int scroll_y, scroll_x;
    int visible_lines, visible_cols;
    int line_height, char_width;
} Viewport;

typedef struct {
    GapBuffer buffer;
    Selection selection;
    TextPos caret;
    bool modified;
    int undo_count_at_save;
    UndoHistory undo;
    UndoHistory redo;
    Viewport viewport;
    HWND hwnd;
    HFONT font;
    int font_height;
    int char_width;
    int tab_index;
} Editor;

#define UNDO_MAX_OPS 1000
#define UNDO_MAX_TEXT 10000
#define INITIAL_CAPACITY 4096
#define GROWTH_FACTOR 2

// Mock Windows functions
void InvalidateRect(void *a, void *b, int c) { (void)a; (void)b; (void)c; }
void ShowCaret(void *a) { (void)a; }
void HideCaret(void *a) { (void)a; }
void CreateCaret(void *a, void *b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
void SetCaretPos(int x, int y) { (void)x; (void)y; }
long GetAsyncKeyState(int k) { (void)k; return 0; }

// Forward declarations
static void editor_add_undo_op_replace(Editor *editor, TextPos pos, const char *text, int text_len, int replace_len);

// Include buffer implementation inline
bool buffer_init(GapBuffer *buf, int initial_capacity);
void buffer_free(GapBuffer *buf);
bool buffer_insert(GapBuffer *buf, TextPos pos, const char *text, int length);
bool buffer_delete(GapBuffer *buf, TextPos pos, int length);
char *buffer_get_text(GapBuffer *buf, TextPos start, TextPos end);
char buffer_get_char(GapBuffer *buf, TextPos pos);
int buffer_line_count(GapBuffer *buf);
TextPos buffer_line_start(GapBuffer *buf, int line);
TextPos buffer_line_end(GapBuffer *buf, int line);

void buffer_move_gap(GapBuffer *buf, int pos) {
    if (pos == buf->gap_start) return;
    int current_gap_end = buf->gap_start + buf->gap_length;
    if (pos < buf->gap_start) {
        int move_size = buf->gap_start - pos;
        memmove(buf->data + pos + buf->gap_length, buf->data + pos, move_size);
    } else {
        int move_size = pos - buf->gap_start;
        memmove(buf->data + buf->gap_start, buf->data + current_gap_end, move_size);
    }
    buf->gap_start = pos;
}

bool buffer_ensure_space(GapBuffer *buf, int needed) {
    if (buf->gap_length >= needed) return true;
    if (buf->capacity > 100 * 1024 * 1024 / GROWTH_FACTOR) return false;
    int new_capacity = buf->capacity * GROWTH_FACTOR;
    while (new_capacity - buf->size < needed) {
        if (new_capacity > 100 * 1024 * 1024 / GROWTH_FACTOR) {
            int final = buf->size + needed + 1024;
            if (final > 100 * 1024 * 1024) return false;
            new_capacity = final;
            break;
        }
        new_capacity *= GROWTH_FACTOR;
    }
    char *nd = realloc(buf->data, new_capacity);
    if (!nd) return false;
    int after = buf->gap_start + buf->gap_length;
    int sz = buf->capacity - after;
    if (sz > 0) memmove(nd + new_capacity - sz, nd + after, sz);
    buf->data = nd;
    buf->gap_length = new_capacity - buf->size;
    buf->capacity = new_capacity;
    return true;
}

bool buffer_init(GapBuffer *buf, int initial_capacity) {
    if (initial_capacity <= 0) initial_capacity = INITIAL_CAPACITY;
    buf->data = malloc(initial_capacity);
    if (!buf->data) return false;
    buf->size = 0;
    buf->capacity = initial_capacity;
    buf->gap_start = 0;
    buf->gap_length = initial_capacity;
    return true;
}

void buffer_free(GapBuffer *buf) {
    if (buf->data) free(buf->data);
    buf->data = NULL;
    buf->size = buf->capacity = buf->gap_start = buf->gap_length = 0;
}

bool buffer_insert(GapBuffer *buf, TextPos pos, const char *text, int length) {
    if (!text || length <= 0 || pos < 0 || pos > buf->size) return false;
    if (!buffer_ensure_space(buf, length)) return false;
    buffer_move_gap(buf, pos);
    memcpy(buf->data + buf->gap_start, text, length);
    buf->gap_start += length;
    buf->gap_length -= length;
    buf->size += length;
    return true;
}

bool buffer_delete(GapBuffer *buf, TextPos pos, int length) {
    if (length <= 0 || pos < 0 || pos + length > buf->size) return false;
    buffer_move_gap(buf, pos);
    buf->gap_length += length;
    buf->size -= length;
    return true;
}

char buffer_get_char(GapBuffer *buf, TextPos pos) {
    if (pos < 0 || pos >= buf->size) return 0;
    return (pos < buf->gap_start) ? buf->data[pos] : buf->data[pos + buf->gap_length];
}

char *buffer_get_text(GapBuffer *buf, TextPos start, TextPos end) {
    if (start < 0 || end > buf->size || start > end) return NULL;
    int len = end - start;
    char *r = malloc(len + 1);
    if (!r) return NULL;
    if (start < buf->gap_start && end > buf->gap_start) {
        int bg = buf->gap_start - start;
        int ag = end - buf->gap_start;
        memcpy(r, buf->data + start, bg);
        memcpy(r + bg, buf->data + buf->gap_start + buf->gap_length, ag);
    } else if (start >= buf->gap_start) {
        memcpy(r, buf->data + start + buf->gap_length, len);
    } else {
        memcpy(r, buf->data + start, len);
    }
    r[len] = '\0';
    return r;
}

int buffer_line_count(GapBuffer *buf) {
    if (buf->size == 0) return 1;
    int c = 1;
    for (int i = 0; i < buf->size; i++)
        if (buffer_get_char(buf, i) == '\n') c++;
    return c;
}

TextPos buffer_line_start(GapBuffer *buf, int line) {
    if (line <= 0) return 0;
    int cl = 0;
    for (int i = 0; i < buf->size; i++) {
        if (buffer_get_char(buf, i) == '\n') {
            cl++;
            if (cl == line) return i + 1;
        }
    }
    return buf->size;
}

TextPos buffer_line_end(GapBuffer *buf, int line) {
    TextPos s = buffer_line_start(buf, line);
    for (int i = s; i < buf->size; i++)
        if (buffer_get_char(buf, i) == '\n') return i;
    return buf->size;
}

// Include editor functions needed for tests
static void editor_add_undo_op(Editor *e, UndoOpType type, TextPos pos, const char *text, int length) {
    for (int i = 0; i < e->redo.count; i++) {
        if (e->redo.ops[i].text) { free(e->redo.ops[i].text); e->redo.ops[i].text = NULL; }
    }
    e->redo.count = 0;
    e->redo.current = 0;
    if (e->undo.count >= e->undo.max_ops) {
        if (e->undo.ops[0].text) { free(e->undo.ops[0].text); e->undo.ops[0].text = NULL; }
        for (int i = 1; i < e->undo.count; i++) e->undo.ops[i-1] = e->undo.ops[i];
        e->undo.count--;
    }
    UndoOp *op = &e->undo.ops[e->undo.count];
    op->type = type;
    op->pos = pos;
    op->length = length;
    op->text = malloc(length + 1);
    if (op->text && text) { memcpy(op->text, text, length); op->text[length] = '\0'; }
    e->undo.count++;
    e->undo.current = e->undo.count;
}

bool editor_init(Editor *e, HWND hwnd) {
    memset(e, 0, sizeof(Editor));
    if (!buffer_init(&e->buffer, 4096)) return false;
    e->hwnd = hwnd;
    e->undo.ops = malloc(sizeof(UndoOp) * UNDO_MAX_OPS);
    e->undo.count = 0;
    e->undo.max_ops = UNDO_MAX_OPS;
    e->redo.ops = malloc(sizeof(UndoOp) * UNDO_MAX_OPS);
    e->redo.count = 0;
    e->redo.max_ops = UNDO_MAX_OPS;
    return true;
}

void editor_free(Editor *e) {
    buffer_free(&e->buffer);
    for (int i = 0; i < e->undo.count; i++) if (e->undo.ops[i].text) free(e->undo.ops[i].text);
    free(e->undo.ops);
    for (int i = 0; i < e->redo.count; i++) if (e->redo.ops[i].text) free(e->redo.ops[i].text);
    free(e->redo.ops);
}

static void editor_delete_selection(Editor *e) {
    TextPos s = e->selection.start, e2 = e->selection.end;
    if (s > e2) { TextPos t = s; s = e2; e2 = t; }
    char *t = buffer_get_text(&e->buffer, s, e2);
    if (t) { editor_add_undo_op(e, OP_DELETE, s, t, e2 - s); free(t); }
    buffer_delete(&e->buffer, s, e2 - s);
    e->caret = s;
    e->selection.start = e->selection.end = s;
}

void editor_char_input(Editor *e, char ch) {
    if (e->selection.start != e->selection.end) {
        editor_delete_selection(e);
    }
    if (buffer_insert(&e->buffer, e->caret, &ch, 1)) {
        editor_add_undo_op(e, OP_INSERT, e->caret, &ch, 1);
        e->caret++;
    }
    e->modified = true;
}

void editor_key_down(Editor *e, int key) {
    if (key == VK_BACK) {
        if (e->selection.start != e->selection.end) {
            editor_delete_selection(e);
        } else if (e->caret > 0) {
            char c = buffer_get_char(&e->buffer, e->caret - 1);
            buffer_delete(&e->buffer, e->caret - 1, 1);
            editor_add_undo_op(e, OP_DELETE, e->caret - 1, &c, 1);
            e->caret--;
            e->selection.start = e->selection.end = e->caret;
        }
    } else if (key == VK_RETURN) {
        char n = '\n';
        if (e->selection.start != e->selection.end) editor_delete_selection(e);
        if (buffer_insert(&e->buffer, e->caret, &n, 1)) {
            editor_add_undo_op(e, OP_INSERT, e->caret, &n, 1);
            e->caret++;
            e->selection.start = e->selection.end = e->caret;
        }
        e->modified = true;
    }
}

bool editor_undo(Editor *e) {
    if (e->undo.current <= 0) return false;
    e->undo.current--;
    UndoOp *op = &e->undo.ops[e->undo.current];
    if (e->redo.count >= e->redo.max_ops) {
        if (e->redo.ops[0].text) { free(e->redo.ops[0].text); e->redo.ops[0].text = NULL; }
        for (int i = 1; i < e->redo.count; i++) e->redo.ops[i-1] = e->redo.ops[i];
        e->redo.count--;
    }
    UndoOp *ro = &e->redo.ops[e->redo.count];
    ro->type = op->type;
    ro->pos = op->pos;
    ro->length = op->length;
    ro->replace_len = op->replace_len;
    ro->text = malloc(op->length + 1);
    if (ro->text) { memcpy(ro->text, op->text, op->length); ro->text[op->length] = '\0'; }
    e->redo.count++;
    e->redo.current = e->redo.count;
    if (op->type == OP_INSERT) {
        buffer_delete(&e->buffer, op->pos, op->length);
        e->caret = op->pos;
    } else if (op->type == OP_DELETE) {
        buffer_insert(&e->buffer, op->pos, op->text, op->length);
        e->caret = op->pos + op->length;
    } else if (op->type == OP_REPLACE) {
        buffer_delete(&e->buffer, op->pos, op->replace_len);
        buffer_insert(&e->buffer, op->pos, op->text, op->length);
        e->caret = op->pos + op->length;
    }
    if (e->undo.current == e->undo_count_at_save) e->modified = false;
    return true;
}

bool editor_redo(Editor *e) {
    if (e->redo.current <= 0) return false;
    e->redo.current--;
    UndoOp *op = &e->redo.ops[e->redo.current];
    if (e->undo.count >= e->undo.max_ops) {
        if (e->undo.ops[0].text) { free(e->undo.ops[0].text); e->undo.ops[0].text = NULL; }
        for (int i = 1; i < e->undo.count; i++) e->undo.ops[i-1] = e->undo.ops[i];
        e->undo.count--;
    }
    UndoOp *uo = &e->undo.ops[e->undo.count];
    uo->type = op->type;
    uo->pos = op->pos;
    uo->length = op->length;
    uo->replace_len = op->replace_len;
    uo->text = malloc(op->length + 1);
    if (uo->text) { memcpy(uo->text, op->text, op->length); uo->text[op->length] = '\0'; }
    e->undo.count++;
    e->undo.current = e->undo.count;
    if (op->type == OP_INSERT) {
        buffer_insert(&e->buffer, op->pos, op->text, op->length);
        e->caret = op->pos + op->length;
    } else if (op->type == OP_DELETE) {
        buffer_delete(&e->buffer, op->pos, op->length);
        e->caret = op->pos;
    } else if (op->type == OP_REPLACE) {
        buffer_delete(&e->buffer, op->pos, op->replace_len);
        buffer_insert(&e->buffer, op->pos, op->text, op->length);
        e->caret = op->pos + op->length;
    }
    return true;
}

void editor_select_all(Editor *e) {
    e->selection.start = 0;
    e->selection.end = e->buffer.size;
    e->caret = e->buffer.size;
}

// Test framework
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond) do { \
    tests_run++; \
    if (!(cond)) { \
        tests_failed++; \
        printf("  ✗ FAILED line %d: %s\n", __LINE__, #cond); \
        return; \
    } else { tests_passed++; } \
} while(0)

#define ASSERT_INT(a, b) do { \
    tests_run++; \
    if ((a) != (b)) { \
        tests_failed++; \
        printf("  ✗ FAILED line %d: expected %d, got %d\n", __LINE__, (int)(b), (int)(a)); \
        return; \
    } else { tests_passed++; } \
} while(0)

#define ASSERT_STR(a, b) do { \
    tests_run++; \
    if (strcmp((a), (b)) != 0) { \
        tests_failed++; \
        printf("  ✗ FAILED line %d: expected \"%s\", got \"%s\"\n", __LINE__, (b), (a)); \
        return; \
    } else { tests_passed++; } \
} while(0)

static HWND mock_hwnd = (HWND)0x1234;

// Define TEST macro
#define TEST(name) void name(void)

// Tests
TEST(test_buffer_init) {
    printf("  ▶ buffer_init\n");
    GapBuffer buf;
    ASSERT(buffer_init(&buf, 1024));
    ASSERT_INT(buf.size, 0);
    ASSERT_INT(buf.capacity, 1024);
    buffer_free(&buf);
}

TEST(test_buffer_insert_simple) {
    printf("  ▶ buffer_insert simple\n");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    ASSERT(buffer_insert(&buf, 0, "Hello", 5));
    ASSERT_INT(buf.size, 5);
    char *t = buffer_get_text(&buf, 0, 5);
    ASSERT_STR(t, "Hello");
    free(t);
    buffer_free(&buf);
}

TEST(test_buffer_delete_end) {
    printf("  ▶ buffer_delete end\n");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    buffer_insert(&buf, 0, "Hello", 5);
    ASSERT(buffer_delete(&buf, 4, 1));
    ASSERT_INT(buf.size, 4);
    char *t = buffer_get_text(&buf, 0, 4);
    ASSERT_STR(t, "Hell");
    free(t);
    buffer_free(&buf);
}

TEST(test_buffer_delete_middle) {
    printf("  ▶ buffer_delete middle\n");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    buffer_insert(&buf, 0, "Hello World", 11);
    ASSERT(buffer_delete(&buf, 5, 6));
    char *t = buffer_get_text(&buf, 0, 5);
    ASSERT_STR(t, "Hello");
    free(t);
    buffer_free(&buf);
}

TEST(test_buffer_get_char) {
    printf("  ▶ buffer_get_char\n");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    buffer_insert(&buf, 0, "Hello", 5);
    ASSERT_INT(buffer_get_char(&buf, 0), 'H');
    ASSERT_INT(buffer_get_char(&buf, 1), 'e');
    ASSERT_INT(buffer_get_char(&buf, 4), 'o');
    ASSERT_INT(buffer_get_char(&buf, 5), 0);
    buffer_free(&buf);
}

TEST(test_buffer_line_count) {
    printf("  ▶ buffer_line_count\n");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    ASSERT_INT(buffer_line_count(&buf), 1);
    buffer_insert(&buf, 0, "Hello\nWorld\nFoo", 15);
    ASSERT_INT(buffer_line_count(&buf), 3);
    buffer_free(&buf);
}

TEST(test_buffer_stress_10k) {
    printf("  ▶ buffer_stress 10K ops\n");
    GapBuffer buf;
    buffer_init(&buf, 4096);
    for (int i = 0; i < 10000; i++) {
        char c = 'A' + (i % 26);
        buffer_insert(&buf, buf.size, &c, 1);
    }
    ASSERT_INT(buf.size, 10000);
    for (int i = 0; i < 10000; i++) {
        buffer_delete(&buf, 0, 1);
    }
    ASSERT_INT(buf.size, 0);
    buffer_free(&buf);
}

TEST(test_editor_init) {
    printf("  ▶ editor_init\n");
    Editor e;
    ASSERT(editor_init(&e, mock_hwnd));
    ASSERT_INT(e.caret, 0);
    ASSERT(!e.modified);
    editor_free(&e);
}

TEST(test_editor_char_input) {
    printf("  ▶ editor_char_input\n");
    Editor e;
    editor_init(&e, mock_hwnd);
    editor_char_input(&e, 'H');
    editor_char_input(&e, 'e');
    editor_char_input(&e, 'l');
    editor_char_input(&e, 'l');
    editor_char_input(&e, 'o');
    char *t = buffer_get_text(&e.buffer, 0, e.buffer.size);
    ASSERT_STR(t, "Hello");
    free(t);
    ASSERT_INT(e.caret, 5);
    editor_free(&e);
}

TEST(test_editor_backspace) {
    printf("  ▶ editor_backspace\n");
    Editor e;
    editor_init(&e, mock_hwnd);
    editor_char_input(&e, 'H');
    editor_char_input(&e, 'i');
    ASSERT_INT(e.caret, 2);
    editor_key_down(&e, VK_BACK);
    ASSERT_INT(e.caret, 1);
    ASSERT_INT(e.buffer.size, 1);
    char *t = buffer_get_text(&e.buffer, 0, 1);
    ASSERT_STR(t, "H");
    free(t);
    editor_free(&e);
}

TEST(test_editor_undo_single) {
    printf("  ▶ editor_undo single\n");
    Editor e;
    editor_init(&e, mock_hwnd);
    editor_char_input(&e, 'A');
    editor_char_input(&e, 'B');
    ASSERT_INT(e.buffer.size, 2);
    editor_undo(&e);
    ASSERT_INT(e.buffer.size, 1);
    char *t = buffer_get_text(&e.buffer, 0, 1);
    ASSERT_STR(t, "A");
    free(t);
    editor_free(&e);
}

TEST(test_editor_undo_redo_cycle) {
    printf("  ▶ editor_undo/redo cycle\n");
    Editor e;
    editor_init(&e, mock_hwnd);
    editor_char_input(&e, 'A');
    editor_char_input(&e, 'B');
    editor_char_input(&e, 'C');
    ASSERT_INT(e.buffer.size, 3);
    editor_undo(&e);
    ASSERT_INT(e.buffer.size, 2);
    editor_undo(&e);
    ASSERT_INT(e.buffer.size, 1);
    editor_redo(&e);
    ASSERT_INT(e.buffer.size, 2);
    editor_redo(&e);
    ASSERT_INT(e.buffer.size, 3);
    char *t = buffer_get_text(&e.buffer, 0, 3);
    ASSERT_STR(t, "ABC");
    free(t);
    editor_free(&e);
}

TEST(test_editor_select_all) {
    printf("  ▶ editor_select_all\n");
    Editor e;
    editor_init(&e, mock_hwnd);
    editor_char_input(&e, 'T');
    editor_char_input(&e, 'e');
    editor_char_input(&e, 's');
    editor_char_input(&e, 't');
    editor_select_all(&e);
    ASSERT(e.selection.start != e.selection.end);
    TextPos s, e2;
    s = e.selection.start; e2 = e.selection.end;
    if (s > e2) { TextPos t = s; s = e2; e2 = t; }
    ASSERT_INT(s, 0);
    ASSERT_INT(e2, 4);
    editor_free(&e);
}

TEST(test_editor_stress_1000_ops) {
    printf("  ▶ editor_stress 1000 ops\n");
    Editor e;
    editor_init(&e, mock_hwnd);
    for (int i = 0; i < 1000; i++) {
        char c = 'A' + (i % 26);
        editor_char_input(&e, c);
    }
    ASSERT_INT(e.buffer.size, 1000);
    for (int i = 0; i < 1000; i++) editor_undo(&e);
    ASSERT_INT(e.buffer.size, 0);
    for (int i = 0; i < 1000; i++) editor_redo(&e);
    ASSERT_INT(e.buffer.size, 1000);
    editor_free(&e);
}

int main(void) {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║   FastPad Unit Test Suite                ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");
    
    printf("┌─ Buffer Tests ─────────────────────────┐\n");
    test_buffer_init();
    test_buffer_insert_simple();
    test_buffer_delete_end();
    test_buffer_delete_middle();
    test_buffer_get_char();
    test_buffer_line_count();
    test_buffer_stress_10k();
    printf("└──────────────────────────────────────────┘\n\n");
    
    printf("┌─ Editor Tests ─────────────────────────┐\n");
    test_editor_init();
    test_editor_char_input();
    test_editor_backspace();
    test_editor_undo_single();
    test_editor_undo_redo_cycle();
    test_editor_select_all();
    test_editor_stress_1000_ops();
    printf("└──────────────────────────────────────────┘\n\n");
    
    printf("╔══════════════════════════════════════════╗\n");
    printf("║  Total:%-3d Passed:%-3d Failed:%-3d    ║\n", tests_run, tests_passed, tests_failed);
    printf("╚══════════════════════════════════════════╝\n");
    printf(tests_failed == 0 ? "\n✅ ALL TESTS PASSED!\n" : "\n❌ %d FAILED!\n", tests_failed);
    
    return tests_failed;
}

