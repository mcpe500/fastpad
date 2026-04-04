/*
 * FastPad Unit Tests - Standalone Version
 * 
 * Tests buffer and editor logic without Windows dependencies.
 * Run with: make test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Minimal types needed for testing
typedef void* HWND;
#define VK_BACK 8
#define VK_TAB 9
#define VK_RETURN 13
#define VK_DELETE 46
#define VK_LEFT 37
#define VK_RIGHT 39
#define VK_UP 38
#define VK_DOWN 40
#define VK_HOME 36
#define VK_END 35

// Mock Windows functions
void InvalidateRect(HWND hwnd, void* rect, int erase) { (void)hwnd; (void)rect; (void)erase; }
void ShowCaret(HWND hwnd) { (void)hwnd; }
void HideCaret(HWND hwnd) { (void)hwnd; }
void CreateCaret(HWND hwnd, void* bm, int w, int h) { (void)hwnd; (void)bm; (void)w; (void)h; }
void SetCaretPos(int x, int y) { (void)x; (void)y; }
long GetAsyncKeyState(int key) { (void)key; return 0; }

// Include buffer implementation
#define GAP_BUFFER_IMPLEMENTATION
typedef int64_t TextPos;

typedef struct {
    int line;
    int col;
} LineCol;

typedef struct {
    TextPos start;
    TextPos end;
} Selection;

typedef struct {
    char *data;
    int size;
    int capacity;
    int gap_start;
    int gap_length;
} GapBuffer;

typedef enum {
    OP_INSERT,
    OP_DELETE
} UndoOpType;

typedef struct {
    UndoOpType type;
    TextPos pos;
    char *text;
    int length;
} UndoOp;

typedef struct {
    UndoOp *ops;
    int count;
    int capacity;
    int current;
    int max_ops;
} UndoHistory;

typedef struct {
    int scroll_y;
    int scroll_x;
    int visible_lines;
    int visible_cols;
    int line_height;
    int char_width;
} Viewport;

// Forward declarations
bool buffer_init(GapBuffer *buf, int initial_capacity);
void buffer_free(GapBuffer *buf);
bool buffer_insert(GapBuffer *buf, TextPos pos, const char *text, int length);
bool buffer_delete(GapBuffer *buf, TextPos pos, int length);
char *buffer_get_text(GapBuffer *buf, TextPos start, TextPos end);
char buffer_get_char(GapBuffer *buf, TextPos pos);
int buffer_length(GapBuffer *buf);
void buffer_move_gap(GapBuffer *buf, int pos);
bool buffer_ensure_space(GapBuffer *buf, int needed);
LineCol buffer_pos_to_linecol(GapBuffer *buf, TextPos pos);
TextPos buffer_linecol_to_pos(GapBuffer *buf, LineCol lc);
int buffer_line_count(GapBuffer *buf);
TextPos buffer_line_start(GapBuffer *buf, int line);
TextPos buffer_line_end(GapBuffer *buf, int line);
int buffer_line_length(GapBuffer *buf, int line);

// Include buffer.c implementation inline
#include "../src/buffer.c"

// Test framework
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void name(void)
#define ASSERT(cond) do { \
    tests_run++; \
    if (!(cond)) { \
        tests_failed++; \
        printf("  ❌ FAILED at line %d: %s\n", __LINE__, #cond); \
        return; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    tests_run++; \
    if (strcmp((a), (b)) != 0) { \
        tests_failed++; \
        printf("  ❌ FAILED at line %d: expected \"%s\", got \"%s\"\n", \
               __LINE__, (b), (a)); \
        return; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_INT_EQ(a, b) do { \
    tests_run++; \
    if ((a) != (b)) { \
        tests_failed++; \
        printf("  ❌ FAILED at line %d: expected %d, got %d\n", \
               __LINE__, (b), (a)); \
        return; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define PRINT_TEST(name) printf("\n▶ %s\n", name)

// ============================================================
// Buffer Tests
// ============================================================

TEST(test_buffer_init) {
    PRINT_TEST("Buffer Initialization");
    
    GapBuffer buf;
    bool result = buffer_init(&buf, 1024);
    
    ASSERT(result == true);
    ASSERT_INT_EQ(buf.size, 0);
    ASSERT_INT_EQ(buf.capacity, 1024);
    ASSERT_INT_EQ(buf.gap_start, 0);
    ASSERT_INT_EQ(buf.gap_length, 1024);
    
    buffer_free(&buf);
    printf("  ✓ Buffer initialized correctly\n");
}

TEST(test_buffer_insert_SIMPLE) {
    PRINT_TEST("Simple Insert");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    const char *text = "Hello";
    bool result = buffer_insert(&buf, 0, text, 5);
    
    ASSERT(result == true);
    ASSERT_INT_EQ(buf.size, 5);
    ASSERT_INT_EQ(buf.gap_start, 5);
    
    char *result_text = buffer_get_text(&buf, 0, 5);
    ASSERT_STR_EQ(result_text, "Hello");
    free(result_text);
    
    buffer_free(&buf);
    printf("  ✓ Simple insert works\n");
}

TEST(test_buffer_INSERT_AT_END) {
    PRINT_TEST("Insert at End");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello", 5);
    buffer_insert(&buf, 5, " World", 6);
    
    ASSERT_INT_EQ(buf.size, 11);
    
    char *result = buffer_get_text(&buf, 0, 11);
    ASSERT_STR_EQ(result, "Hello World");
    free(result);
    
    buffer_free(&buf);
    printf("  ✓ Insert at end works\n");
}

TEST(test_buffer_INSERT_AT_BEGINNING) {
    PRINT_TEST("Insert at Beginning");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "World", 5);
    buffer_insert(&buf, 0, "Hello ", 6);
    
    char *result = buffer_get_text(&buf, 0, 11);
    ASSERT_STR_EQ(result, "Hello World");
    free(result);
    
    buffer_free(&buf);
    printf("  ✓ Insert at beginning works\n");
}

TEST(test_buffer_INSERT_IN_MIDDLE) {
    PRINT_TEST("Insert in Middle");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Helo", 4);
    buffer_insert(&buf, 3, "l", 1);
    
    char *result = buffer_get_text(&buf, 0, 5);
    ASSERT_STR_EQ(result, "Hello");
    free(result);
    
    buffer_free(&buf);
    printf("  ✓ Insert in middle works\n");
}

TEST(test_buffer_DELETE_FROM_END) {
    PRINT_TEST("Delete from End");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello", 5);
    bool result = buffer_delete(&buf, 4, 1);
    
    ASSERT(result == true);
    ASSERT_INT_EQ(buf.size, 4);
    
    char *text = buffer_get_text(&buf, 0, 4);
    ASSERT_STR_EQ(text, "Hell");
    free(text);
    
    buffer_free(&buf);
    printf("  ✓ Delete from end works\n");
}

TEST(test_buffer_DELETE_FROM_BEGINNING) {
    PRINT_TEST("Delete from Beginning");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello", 5);
    buffer_delete(&buf, 0, 1);
    
    char *text = buffer_get_text(&buf, 0, 4);
    ASSERT_STR_EQ(text, "ello");
    free(text);
    
    buffer_free(&buf);
    printf("  ✓ Delete from beginning works\n");
}

TEST(test_buffer_DELETE_FROM_MIDDLE) {
    PRINT_TEST("Delete from Middle");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello", 5);
    buffer_delete(&buf, 2, 1);
    
    char *text = buffer_get_text(&buf, 0, 4);
    ASSERT_STR_EQ(text, "Helo");
    free(text);
    
    buffer_free(&buf);
    printf("  ✓ Delete from middle works\n");
}

TEST(test_buffer_DELETE_ALL) {
    PRINT_TEST("Delete All Text");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello", 5);
    buffer_delete(&buf, 0, 5);
    
    ASSERT_INT_EQ(buf.size, 0);
    ASSERT_INT_EQ(buf.gap_start, 0);
    ASSERT_INT_EQ(buf.gap_length, 1024);
    
    buffer_free(&buf);
    printf("  ✓ Delete all text works\n");
}

TEST(test_buffer_DELETE_RANGE) {
    PRINT_TEST("Delete Range");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello World", 11);
    buffer_delete(&buf, 5, 6);
    
    char *text = buffer_get_text(&buf, 0, 5);
    ASSERT_STR_EQ(text, "Hello");
    free(text);
    
    buffer_free(&buf);
    printf("  ✓ Delete range works\n");
}

TEST(test_buffer_GET_CHAR) {
    PRINT_TEST("Get Character at Position");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello", 5);
    
    ASSERT_INT_EQ(buffer_get_char(&buf, 0), 'H');
    ASSERT_INT_EQ(buffer_get_char(&buf, 1), 'e');
    ASSERT_INT_EQ(buffer_get_char(&buf, 4), 'o');
    ASSERT_INT_EQ(buffer_get_char(&buf, 5), 0);
    
    buffer_free(&buf);
    printf("  ✓ Get char works\n");
}

TEST(test_buffer_LINE_COL_CONVERSION) {
    PRINT_TEST("Line/Column Conversion");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello\nWorld\nFoo", 15);
    
    LineCol lc = buffer_pos_to_linecol(&buf, 0);
    ASSERT_INT_EQ(lc.line, 0);
    ASSERT_INT_EQ(lc.col, 0);
    
    lc = buffer_pos_to_linecol(&buf, 6);
    ASSERT_INT_EQ(lc.line, 1);
    ASSERT_INT_EQ(lc.col, 0);
    
    lc = buffer_pos_to_linecol(&buf, 7);
    ASSERT_INT_EQ(lc.line, 1);
    ASSERT_INT_EQ(lc.col, 1);
    
    lc = buffer_pos_to_linecol(&buf, 12);
    ASSERT_INT_EQ(lc.line, 2);
    ASSERT_INT_EQ(lc.col, 0);
    
    TextPos pos = buffer_linecol_to_pos(&buf, (LineCol){1, 0});
    ASSERT_INT_EQ(pos, 6);
    
    pos = buffer_linecol_to_pos(&buf, (LineCol){2, 2});
    ASSERT_INT_EQ(pos, 14);
    
    buffer_free(&buf);
    printf("  ✓ Line/col conversion works\n");
}

TEST(test_buffer_LINE_COUNT) {
    PRINT_TEST("Line Count");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    ASSERT_INT_EQ(buffer_line_count(&buf), 1);
    
    buffer_insert(&buf, 0, "Hello", 5);
    ASSERT_INT_EQ(buffer_line_count(&buf), 1);
    
    buffer_insert(&buf, 5, "\n", 1);
    ASSERT_INT_EQ(buffer_line_count(&buf), 2);
    
    buffer_insert(&buf, 6, "World", 5);
    ASSERT_INT_EQ(buffer_line_count(&buf), 2);
    
    buffer_insert(&buf, 11, "\n", 1);
    ASSERT_INT_EQ(buffer_line_count(&buf), 3);
    
    buffer_free(&buf);
    printf("  ✓ Line count works\n");
}

TEST(test_buffer_LINE_START_END) {
    PRINT_TEST("Line Start/End Positions");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello\nWorld\nFoo", 15);
    
    ASSERT_INT_EQ(buffer_line_start(&buf, 0), 0);
    ASSERT_INT_EQ(buffer_line_end(&buf, 0), 5);
    ASSERT_INT_EQ(buffer_line_length(&buf, 0), 5);
    
    ASSERT_INT_EQ(buffer_line_start(&buf, 1), 6);
    ASSERT_INT_EQ(buffer_line_end(&buf, 1), 11);
    ASSERT_INT_EQ(buffer_line_length(&buf, 1), 5);
    
    ASSERT_INT_EQ(buffer_line_start(&buf, 2), 12);
    ASSERT_INT_EQ(buffer_line_end(&buf, 2), 15);
    ASSERT_INT_EQ(buffer_line_length(&buf, 2), 3);
    
    buffer_free(&buf);
    printf("  ✓ Line start/end works\n");
}

TEST(test_buffer_GROWTH) {
    PRINT_TEST("Buffer Growth");
    
    GapBuffer buf;
    buffer_init(&buf, 10);
    
    char text[100];
    memset(text, 'A', 99);
    text[99] = '\0';
    
    bool result = buffer_insert(&buf, 0, text, 99);
    ASSERT(result == true);
    ASSERT_INT_EQ(buf.size, 99);
    ASSERT(buf.capacity >= 99);
    
    char *result_text = buffer_get_text(&buf, 0, 99);
    ASSERT(memcmp(result_text, text, 99) == 0);
    free(result_text);
    
    buffer_free(&buf);
    printf("  ✓ Buffer growth works\n");
}

TEST(test_buffer_INSERT_DELETE_CYCLE) {
    PRINT_TEST("Insert/Delete Cycle");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello", 5);
    buffer_delete(&buf, 0, 5);
    buffer_insert(&buf, 0, "World", 5);
    
    char *text = buffer_get_text(&buf, 0, 5);
    ASSERT_STR_EQ(text, "World");
    free(text);
    
    buffer_free(&buf);
    printf("  ✓ Insert/delete cycle works\n");
}

TEST(test_buffer_DELETE_INVALID_POSITIONS) {
    PRINT_TEST("Delete at Invalid Positions");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello", 5);
    
    bool result = buffer_delete(&buf, -1, 1);
    ASSERT(result == false);
    
    result = buffer_delete(&buf, 10, 1);
    ASSERT(result == false);
    
    result = buffer_delete(&buf, 0, 100);
    ASSERT(result == false);
    
    ASSERT_INT_EQ(buf.size, 5);
    
    buffer_free(&buf);
    printf("  ✓ Invalid delete handled correctly\n");
}

TEST(test_buffer_TEXT_WITH_NEWLINES) {
    PRINT_TEST("Text with Newlines");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    const char *text = "Line 1\nLine 2\nLine 3";
    buffer_insert(&buf, 0, text, 20);
    
    char *result = buffer_get_text(&buf, 0, 20);
    ASSERT_STR_EQ(result, text);
    free(result);
    
    ASSERT_INT_EQ(buffer_line_count(&buf), 3);
    
    buffer_free(&buf);
    printf("  ✓ Text with newlines works\n");
}

// ============================================================
// Stress Tests
// ============================================================

TEST(test_stress_RAPID_INSERT_DELETE) {
    PRINT_TEST("Stress: Rapid Insert/Delete (1000 ops)");
    
    GapBuffer buf;
    buffer_init(&buf, 4096);
    
    for (int i = 0; i < 1000; i++) {
        char ch = 'A' + (i % 26);
        buffer_insert(&buf, buf.size, &ch, 1);
    }
    
    ASSERT_INT_EQ(buf.size, 1000);
    
    for (int i = 0; i < 1000; i++) {
        buffer_delete(&buf, buf.size - 1, 1);
    }
    
    ASSERT_INT_EQ(buf.size, 0);
    
    buffer_free(&buf);
    printf("  ✓ 1000 insert/delete cycle passed\n");
}

TEST(test_stress_ALTERNATING_OPS) {
    PRINT_TEST("Stress: Alternating Insert/Delete (500 cycles)");
    
    GapBuffer buf;
    buffer_init(&buf, 4096);
    
    for (int i = 0; i < 500; i++) {
        buffer_insert(&buf, buf.size, "X", 1);
        buffer_delete(&buf, buf.size - 1, 1);
    }
    
    ASSERT_INT_EQ(buf.size, 0);
    
    buffer_free(&buf);
    printf("  ✓ 500 alternating ops passed\n");
}

TEST(test_stress_LARGE_TEXT) {
    PRINT_TEST("Stress: Large Text Operations (10KB)");
    
    GapBuffer buf;
    buffer_init(&buf, 4096);
    
    char *large_text = malloc(10000);
    memset(large_text, 'A', 10000);
    
    buffer_insert(&buf, 0, large_text, 10000);
    ASSERT_INT_EQ(buf.size, 10000);
    
    char *result = buffer_get_text(&buf, 0, 10000);
    for (int i = 0; i < 10000; i++) {
        if (result[i] != 'A') {
            printf("  ❌ FAILED: Character mismatch at %d\n", i);
            tests_failed++;
            free(result);
            free(large_text);
            buffer_free(&buf);
            return;
        }
    }
    free(result);
    
    for (int i = 0; i < 10; i++) {
        buffer_delete(&buf, 0, 1000);
    }
    
    ASSERT_INT_EQ(buf.size, 0);
    
    free(large_text);
    buffer_free(&buf);
    printf("  ✓ 10KB text operations passed\n");
}

TEST(test_edge_EMPTY_OPERATIONS) {
    PRINT_TEST("Edge Case: Empty Buffer Operations");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_delete(&buf, 0, 1);
    buffer_delete(&buf, -1, 1);
    buffer_delete(&buf, 100, 1);
    
    ASSERT_INT_EQ(buf.size, 0);
    
    buffer_free(&buf);
    printf("  ✓ Empty operations handled\n");
}

TEST(test_edge_MULTI_NEWLINES) {
    PRINT_TEST("Edge Case: Multiple Consecutive Newlines");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "\n\n\n\n\n", 5);
    
    ASSERT_INT_EQ(buffer_line_count(&buf), 6);
    ASSERT_INT_EQ(buffer_line_start(&buf, 3), 3);
    ASSERT_INT_EQ(buffer_line_end(&buf, 3), 3);
    ASSERT_INT_EQ(buffer_line_length(&buf, 3), 0);
    
    buffer_free(&buf);
    printf("  ✓ Multiple newlines handled\n");
}

// ============================================================
// Memory Leak Tests
// ============================================================

TEST(test_memory_GET_TEXT_FREE) {
    PRINT_TEST("Memory: buffer_get_text Must Be Freed (1000 calls)");
    
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    buffer_insert(&buf, 0, "Hello", 5);
    
    for (int i = 0; i < 1000; i++) {
        char *text = buffer_get_text(&buf, 0, 5);
        if (!text) {
            printf("  ❌ FAILED: buffer_get_text returned NULL\n");
            tests_failed++;
            buffer_free(&buf);
            return;
        }
        free(text);
    }
    
    buffer_free(&buf);
    printf("  ✓ 1000 get_text calls properly freed\n");
}

TEST(test_memory_BUFFER_FREE) {
    PRINT_TEST("Memory: Buffer Free Complete (100 buffers)");
    
    for (int i = 0; i < 100; i++) {
        GapBuffer buf;
        buffer_init(&buf, 4096);
        buffer_insert(&buf, 0, "Test data", 9);
        buffer_free(&buf);
    }
    
    printf("  ✓ 100 buffers created and freed\n");
}

TEST(test_memory_INSERT_DELETE_10K) {
    PRINT_TEST("Memory: 10K Insert/Delete Operations");
    
    GapBuffer buf;
    buffer_init(&buf, 4096);
    
    for (int i = 0; i < 10000; i++) {
        char ch = 'A' + (i % 26);
        buffer_insert(&buf, buf.size, &ch, 1);
    }
    
    ASSERT_INT_EQ(buf.size, 10000);
    
    for (int i = 0; i < 10000; i++) {
        buffer_delete(&buf, 0, 1);
    }
    
    ASSERT_INT_EQ(buf.size, 0);
    
    buffer_free(&buf);
    printf("  ✓ 10K insert/delete operations completed\n");
}

// ============================================================
// Test Runner
// ============================================================

int main(void) {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║     FastPad Unit Test Suite              ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    
    printf("\n┌─ Buffer Tests ─────────────────────────┐\n");
    test_buffer_init();
    test_buffer_INSERT_SIMPLE();
    test_buffer_INSERT_AT_END();
    test_buffer_INSERT_AT_BEGINNING();
    test_buffer_INSERT_IN_MIDDLE();
    test_buffer_DELETE_FROM_END();
    test_buffer_DELETE_FROM_BEGINNING();
    test_buffer_DELETE_FROM_MIDDLE();
    test_buffer_DELETE_ALL();
    test_buffer_DELETE_RANGE();
    test_buffer_GET_CHAR();
    test_buffer_LINE_COL_CONVERSION();
    test_buffer_LINE_COUNT();
    test_buffer_LINE_START_END();
    test_buffer_GROWTH();
    test_buffer_INSERT_DELETE_CYCLE();
    test_buffer_DELETE_INVALID_POSITIONS();
    test_buffer_TEXT_WITH_NEWLINES();
    printf("└──────────────────────────────────────────┘\n");
    
    printf("\n┌─ Stress Tests ─────────────────────────┐\n");
    test_stress_RAPID_INSERT_DELETE();
    test_stress_ALTERNATING_OPS();
    test_stress_LARGE_TEXT();
    printf("└──────────────────────────────────────────┘\n");
    
    printf("\n┌─ Edge Cases ───────────────────────────┐\n");
    test_edge_EMPTY_OPERATIONS();
    test_edge_MULTI_NEWLINES();
    printf("└──────────────────────────────────────────┘\n");
    
    printf("\n┌─ Memory Tests ─────────────────────────┐\n");
    test_memory_GET_TEXT_FREE();
    test_memory_BUFFER_FREE();
    test_memory_INSERT_DELETE_10K();
    printf("└──────────────────────────────────────────┘\n");
    
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║           Test Summary                     ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Total:  %-33d ║\n", tests_run);
    printf("║  Passed: %-33d ║\n", tests_passed);
    printf("║  Failed: %-33d ║\n", tests_failed);
    printf("╚══════════════════════════════════════════╝\n");
    
    if (tests_failed == 0) {
        printf("\n✅ All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ %d test(s) failed!\n", tests_failed);
        return 1;
    }
}
