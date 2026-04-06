#include "buffer.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 4096
#define GROWTH_FACTOR 2
#define MAX_BUFFER_SIZE (100 * 1024 * 1024) /* 100 MB limit for v1 */

bool buffer_init(GapBuffer *buf, int initial_capacity) {
    if (initial_capacity <= 0) {
        initial_capacity = INITIAL_CAPACITY;
    }
    
    buf->data = (char *)malloc(initial_capacity);
    if (!buf->data) {
        return false;
    }
    
    buf->size = 0;
    buf->capacity = initial_capacity;
    buf->gap_start = 0;
    buf->gap_length = initial_capacity;
    
    return true;
}

void buffer_free(GapBuffer *buf) {
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
    buf->gap_start = 0;
    buf->gap_length = 0;
}

bool buffer_ensure_space(GapBuffer *buf, int needed) {
    if (buf->gap_length >= needed) {
        return true;
    }

    // Need to grow buffer
    // Check for integer overflow before multiplying
    if (buf->capacity > MAX_BUFFER_SIZE / GROWTH_FACTOR) {
        // Would overflow or exceed limit
        return false;
    }

    int new_capacity = buf->capacity * GROWTH_FACTOR;
    while (new_capacity - buf->size < needed) {
        // Check for overflow before next multiply
        if (new_capacity > MAX_BUFFER_SIZE / GROWTH_FACTOR) {
            /* One final step to reach needed */
            int final_capacity = buf->size + needed + 1024;
            if (final_capacity > MAX_BUFFER_SIZE) return false;
            new_capacity = final_capacity;
            break;
        }
        new_capacity *= GROWTH_FACTOR;
    }
    
    char *new_data = (char *)realloc(buf->data, new_capacity);
    if (!new_data) {
        return false;
    }
    
    // Move data after gap to new position
    int after_gap_start = buf->gap_start + buf->gap_length;
    int after_gap_size = buf->capacity - after_gap_start;
    
    if (after_gap_size > 0) {
        memmove(new_data + new_capacity - after_gap_size, 
                new_data + after_gap_start, 
                after_gap_size);
    }
    
    buf->data = new_data;
    buf->gap_length = new_capacity - buf->size;
    buf->capacity = new_capacity;
    
    return true;
}

void buffer_move_gap(GapBuffer *buf, int pos) {
    if (pos == buf->gap_start) {
        return;
    }
    
    int current_gap_end = buf->gap_start + buf->gap_length;
    
    if (pos < buf->gap_start) {
        // Moving gap left: shift data right
        int move_size = buf->gap_start - pos;
        memmove(buf->data + pos + buf->gap_length, 
                buf->data + pos, 
                move_size);
    } else {
        // Moving gap right: shift data left
        int move_size = pos - buf->gap_start;
        memmove(buf->data + buf->gap_start, 
                buf->data + current_gap_end, 
                move_size);
    }
    
    buf->gap_start = pos;
}

bool buffer_insert(GapBuffer *buf, TextPos pos, const char *text, int length) {
    if (!text || length <= 0) {
        return true;
    }
    
    #ifdef DEV_BUILD
    log_action("BUFFER_INSERT", "pos: %lld, len: %d", pos, length);
    #endif
    
    if (pos < 0 || pos > buf->size) {
        return false;
    }
    
    if (!buffer_ensure_space(buf, length)) {
        return false;
    }
    
    // Move gap to insertion position
    buffer_move_gap(buf, pos);
    
    // Copy text into gap
    memcpy(buf->data + buf->gap_start, text, length);
    
    // Update gap
    buf->gap_start += length;
    buf->gap_length -= length;
    buf->size += length;
    
    return true;
}

bool buffer_delete(GapBuffer *buf, TextPos pos, int length) {
    if (length <= 0) {
        return true;
    }
    
    #ifdef DEV_BUILD
    log_action("BUFFER_DELETE", "pos: %lld, len: %d", pos, length);
    #endif
    
    if (pos < 0 || pos + length > buf->size) {
        return false;
    }
    
    // Move gap to deletion position
    buffer_move_gap(buf, pos);
    
    // Expand gap to cover deleted text
    buf->gap_length += length;
    buf->size -= length;
    
    return true;
}

char *buffer_get_text(GapBuffer *buf, TextPos start, TextPos end) {
    if (start < 0 || end > buf->size || start > end) {
        return NULL;
    }
    
    int length = end - start;
    char *result = (char *)malloc(length + 1);
    if (!result) {
        return NULL;
    }
    
    // Need to handle gap
    if (start < buf->gap_start && end > buf->gap_start) {
        // Text spans gap
        int before_gap = buf->gap_start - start;
        int after_gap = end - buf->gap_start;
        
        memcpy(result, buf->data + start, before_gap);
        memcpy(result + before_gap, 
               buf->data + buf->gap_start + buf->gap_length, 
               after_gap);
    } else if (start >= buf->gap_start) {
        // Text is after gap
        memcpy(result, 
               buf->data + start + buf->gap_length, 
               length);
    } else {
        // Text is before gap
        memcpy(result, buf->data + start, length);
    }
    
    result[length] = '\0';
    return result;
}

char buffer_get_char(GapBuffer *buf, TextPos pos) {
    if (pos < 0 || pos >= buf->size) {
        return 0;
    }
    
    if (pos < buf->gap_start) {
        return buf->data[pos];
    } else {
        return buf->data[pos + buf->gap_length];
    }
}

int buffer_length(GapBuffer *buf) {
    return buf->size;
}

LineCol buffer_pos_to_linecol(GapBuffer *buf, TextPos pos) {
    LineCol lc = {0, 0};
    
    if (pos < 0 || pos > buf->size) {
        return lc;
    }
    
    int current_pos = 0;
    int line = 0;
    int col = 0;
    
    while (current_pos < pos) {
        char c = buffer_get_char(buf, current_pos);
        if (c == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
        current_pos++;
    }
    
    lc.line = line;
    lc.col = col;
    return lc;
}

TextPos buffer_linecol_to_pos(GapBuffer *buf, LineCol lc) {
    int current_pos = 0;
    int line = 0;
    int col = 0;
    
    while (current_pos < buf->size) {
        if (line == lc.line && col == lc.col) {
            return current_pos;
        }
        
        char c = buffer_get_char(buf, current_pos);
        if (c == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
        current_pos++;
    }
    
    // If position not found, return end
    return buf->size;
}

int buffer_line_count(GapBuffer *buf) {
    if (buf->size == 0) {
        return 1;
    }
    
    int count = 1;
    for (int i = 0; i < buf->size; i++) {
        if (buffer_get_char(buf, i) == '\n') {
            count++;
        }
    }
    
    return count;
}

TextPos buffer_line_start(GapBuffer *buf, int line) {
    if (line <= 0) {
        return 0;
    }
    
    int current_line = 0;
    for (int i = 0; i < buf->size; i++) {
        if (buffer_get_char(buf, i) == '\n') {
            current_line++;
            if (current_line == line) {
                return i + 1;
            }
        }
    }
    
    return buf->size;
}

TextPos buffer_line_end(GapBuffer *buf, int line) {
    TextPos start = buffer_line_start(buf, line);
    
    for (int i = start; i < buf->size; i++) {
        if (buffer_get_char(buf, i) == '\n') {
            return i;
        }
    }
    
    return buf->size;
}

int buffer_line_length(GapBuffer *buf, int line) {
    TextPos start = buffer_line_start(buf, line);
    TextPos end = buffer_line_end(buf, line);
    return end - start;
}
