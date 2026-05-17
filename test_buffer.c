// Quick test for buffer insert/delete
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef int TextPos;

typedef struct {
    char *data;
    TextPos size;
    TextPos capacity;
    TextPos gap_start;
    TextPos gap_length;
} GapBuffer;

void buffer_init(GapBuffer *buf) {
    buf->capacity = 64;
    buf->data = malloc(buf->capacity);
    buf->size = 0;
    buf->gap_start = 0;
    buf->gap_length = buf->capacity;
}

char buffer_get_char(GapBuffer *buf, TextPos pos) {
    if (pos < 0 || pos > buf->size) return 0;
    if (pos < buf->gap_start) return buf->data[pos];
    return buf->data[pos + buf->gap_length];
}

void buffer_insert(GapBuffer *buf, TextPos pos, const char *text, int len) {
    if (len <= 0 || !text) return;
    while (buf->gap_length < len) {
        TextPos new_cap = buf->capacity * 2;
        char *new_data = malloc(new_cap);
        memcpy(new_data, buf->data, buf->gap_start);
        memcpy(new_data + buf->gap_start, buf->data + buf->gap_start + buf->gap_length, 
               buf->size - buf->gap_start);
        free(buf->data);
        buf->data = new_data;
        buf->gap_length = new_cap - buf->size;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + pos, text, len);
    buf->gap_start += len;
    buf->size += len;
}

int main() {
    GapBuffer buf;
    buffer_init(&buf);
    
    // Insert "hello world"
    char *text1 = "hello world";
    buffer_insert(&buf, 0, text1, strlen(text1));
    
    printf("After 'hello world':\n");
    for (int i = 0; i < buf.size; i++) {
        char c = buffer_get_char(&buf, i);
        printf("%c", c);
    }
    printf("\n");
    printf("size=%d capacity=%d gap_start=%d gap_len=%d\n", 
           buf.size, buf.capacity, buf.gap_start, buf.gap_length);
    
    // Insert space at position 5
    char space = ' ';
    buffer_insert(&buf, 5, &space, 1);
    
    printf("\nAfter inserting space at pos 5:\n");
    for (int i = 0; i < buf.size; i++) {
        char c = buffer_get_char(&buf, i);
        printf("%c", c);
    }
    printf("\n");
    printf("size=%d capacity=%d gap_start=%d gap_len=%d\n", 
           buf.size, buf.capacity, buf.gap_start, buf.gap_length);
    
    free(buf.data);
    return 0;
}