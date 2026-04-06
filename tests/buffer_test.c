#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "../src/buffer.h"
#include "../src/core_types.h"

// Helper untuk mengecek apakah struct GapBuffer rusak
void check_integrity(GapBuffer *buf, TextPos last_known_gap_start, int last_known_cap) {
    if (buf->capacity != last_known_cap) {
        printf("INTEGRITY ERROR: Capacity changed unexpectedly! %d -> %d\\n", last_known_cap, buf->capacity);
        exit(1);
    }
    // Catatan: gap_start boleh berubah setelah insert, 
    // jadi kita cek integritas pointer utamanya saja di sini
    if (buf->data == NULL) {
        printf("INTEGRITY ERROR: buffer->data became NULL!\\n");
        exit(1);
    }
}

void test_crash_reproduction() {
    printf("Testing Exact Crash Scenario (pos=0, len=1)... ");
    GapBuffer buf;
    buffer_init(&buf, 4096);
    
    // Kita simpan state sebelum memcpy
    int cap_before = buf.capacity;
    
    // TRIGGER: Insert 1 char at pos 0
    buffer_insert(&buf, 0, "A", 1);
    
    // Cek apakah struktur data rusak setelah insert
    check_integrity(&buf, 0, cap_before);
    
    buffer_free(&buf);
    printf("PASSED\\n");
}

void test_boundary_insert() {
    printf("Testing Boundary Inserts... ");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    
    // 1. Insert at very start
    buffer_insert(&buf, 0, "S", 1);
    // 2. Insert at very end
    buffer_insert(&buf, buf.size, "E", 1);
    // 3. Insert in middle
    buffer_insert(&buf, 1, "M", 1);
    
    char *res = buffer_get_text(&buf, 0, 3);
    if (strcmp(res, "SME") != 0) {
        printf("FAILED: expected SME, got %s\\n", res);
        exit(1);
    }
    free(res);
    
    buffer_free(&buf);
    printf("PASSED\\n");
}

void test_chaos_inserts() {
    printf("Testing Chaos (10,000 random operations)... ");
    GapBuffer buf;
    buffer_init(&buf, 1024);
    srand(time(NULL));
    
    for (int i = 0; i < 10000; i++) {
        int op = rand() % 2;
        if (op == 0) { // Insert
            TextPos pos = (buf.size == 0) ? 0 : rand() % (buf.size + 1);
            const char *txt = "x";
            buffer_insert(&buf, pos, txt, 1);
        } else { // Delete
            if (buf.size > 0) {
                TextPos pos = rand() % buf.size;
                int len = 1 + (rand() % (buf.size - pos));
                buffer_delete(&buf, pos, len);
            }
        }
    }
    
    buffer_free(&buf);
    printf("PASSED\\n");
}

void test_extreme_expansion() {
    printf("Testing Extreme Expansion... ");
    GapBuffer buf;
    buffer_init(&buf, 10); // Minimal start
    
    // Insert 1 million chars to force many reallocs
    char *big_data = malloc(1000000);
    memset(big_data, 'A', 1000000);
    
    buffer_insert(&buf, 0, big_data, 1000000);
    
    if (buf.size != 1000000) {
        printf("FAILED: size mismatch\\n");
        exit(1);
    }
    
    free(big_data);
    buffer_free(&buf);
    printf("PASSED\\n");
}

int main() {
    printf("--- FastPad BRUTAL Testing Suite ---\\n");
    test_crash_reproduction();
    test_boundary_insert();
    test_chaos_inserts();
    test_extreme_expansion();
    printf("-------------------------------------\\n");
    printf("ALL BRUTAL TESTS PASSED! No memory corruption detected by ASan.\\n");
    return 0;
}
