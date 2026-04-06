#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/buffer.h"
#include "../src/types.h"

void test_basic_insert() {
    printf("Testing basic insert... ");
    GapBuffer buf;
    buffer_init(&buf, 4096);
    
    // Simulating the crash scenario: insert first character
    if (!buffer_insert(&buf, 0, "A", 1)) {
        printf("FAILED: buffer_insert returned false\\n");
        exit(1);
    }
    
    if (buffer_get_char(&buf, 0) != 'A') {
        printf("FAILED: char at 0 is not 'A'\\n");
        exit(1);
    }
    
    if (buf.size != 1) {
        printf("FAILED: size is %d, expected 1\\n", buf.size);
        exit(1);
    }
    
    buffer_free(&buf);
    printf("PASSED\\n");
}

void test_multiple_inserts() {
    printf("Testing multiple inserts... ");
    GapBuffer buf;
    buffer_init(&buf, 4096);
    
    buffer_insert(&buf, 0, "Hello", 5);
    buffer_insert(&buf, 5, " World", 6);
    
    char *text = buffer_get_text(&buf, 0, 11);
    if (strcmp(text, "Hello World") != 0) {
        printf("FAILED: text is %s, expected 'Hello World'\\n", text);
        free(text);
        exit(1);
    }
    free(text);
    
    buffer_free(&buf);
    printf("PASSED\\n");
}

int main() {
    printf("--- FastPad Buffer Logic Unit Tests ---\\n");
    test_basic_insert();
    test_multiple_inserts();
    printf("All tests passed successfully!\\n");
    return 0;
}
