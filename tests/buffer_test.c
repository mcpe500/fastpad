#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/buffer.h"
#include "../src/core_types.h"

// Helper untuk verifikasi isi buffer
void assert_buffer_content(GapBuffer *buf, const char *expected) {
    TextPos len = strlen(expected);
    char *actual = buffer_get_text(buf, 0, len);
    if (strcmp(actual, expected) != 0) {
        printf("FAILED: Expected '%s', got '%s'\\n", expected, actual);
        free(actual);
        exit(1);
    }
    free(actual);
}

void test_insert_start() {
    printf("Testing Insert Start... ");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    buffer_insert(&buf, 0, "World", 5);
    buffer_insert(&buf, 0, "Hello ", 6);
    assert_buffer_content(&buf, "Hello World");
    buffer_free(&buf);
    printf("PASSED\\n");
}

void test_insert_middle() {
    printf("Testing Insert Middle... ");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    buffer_insert(&buf, 0, "Hello World", 11);
    buffer_insert(&buf, 6, "Beautiful ", 10);
    assert_buffer_content(&buf, "Hello Beautiful World");
    buffer_free(&buf);
    printf("PASSED\\n");
}

void test_insert_end() {
    printf("Testing Insert End... ");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    buffer_insert(&buf, 0, "Hello", 5);
    buffer_insert(&buf, 5, " World", 6);
    assert_buffer_content(&buf, "Hello World");
    buffer_free(&buf);
    printf("PASSED\\n");
}

void test_delete_range() {
    printf("Testing Delete Range... ");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    buffer_insert(&buf, 0, "Hello Beautiful World", 21);
    buffer_delete(&buf, 6, 10); // Delete "Beautiful "
    assert_buffer_content(&buf, "Hello World");
    buffer_free(&buf);
    printf("PASSED\\n");
}

void test_buffer_expansion() {
    printf("Testing Buffer Expansion (Stress)... ");
    GapBuffer buf;
    buffer_init(&buf, 10); // Very small initial capacity
    
    char large_text[2000];
    memset(large_text, 'A', 1999);
    large_text[1999] = '\\0';
    
    buffer_insert(&buf, 0, large_text, 1999);
    if (buf.size != 1999) {
        printf("FAILED: Size mismatch after expansion\\n");
        exit(1);
    }
    
    buffer_free(&buf);
    printf("PASSED\\n");
}

int main() {
    printf("--- FastPad Core Logic Regression Suite ---\\n");
    test_insert_start();
    test_insert_middle();
    test_insert_end();
    test_delete_range();
    test_buffer_expansion();
    printf("-------------------------------------------\\n");
    printf("ALL TESTS PASSED! Logic is stable.\\n");
    return 0;
}
