#include "clipboard.h"
#include <stdio.h>
#include <string.h>

bool clipboard_init(ClipboardHistory *mgr) {
    memset(mgr, 0, sizeof(ClipboardHistory));
    mgr->max_items = MAX_CLIPBOARD_ITEMS;
    mgr->next_is_our_change = false;
    return true;
}

void clipboard_free(ClipboardHistory *mgr) {
    (void)mgr; // unused
    // Nothing to free - items are inline
}

bool clipboard_add(ClipboardHistory *mgr, const char *content, int length) {
    if (mgr->count >= mgr->max_items) {
        // Remove oldest
        for (int i = 0; i < mgr->count - 1; i++) {
            mgr->items[i] = mgr->items[i + 1];
        }
        mgr->count--;
    }
    
    ClipboardItem *item = &mgr->items[mgr->count];
    memset(item, 0, sizeof(ClipboardItem));
    
    if (length >= MAX_CLIPBOARD_CONTENT) {
        length = MAX_CLIPBOARD_CONTENT - 1;
    }
    
    memcpy(item->content, content, length);
    item->content[length] = '\0';
    item->length = length;
    item->is_text = true;
    
    GetSystemTimeAsFileTime(&item->copied);
    mgr->count++;
    
    return true;
}

bool clipboard_remove(ClipboardHistory *mgr, int index) {
    if (index < 0 || index >= mgr->count) return false;
    
    for (int i = index; i < mgr->count - 1; i++) {
        mgr->items[i] = mgr->items[i + 1];
    }
    
    mgr->count--;
    return true;
}

bool clipboard_clear(ClipboardHistory *mgr) {
    mgr->count = 0;
    return true;
}

const char* clipboard_get_item(ClipboardHistory *mgr, int index, int *length) {
    if (index < 0 || index >= mgr->count) return NULL;
    
    ClipboardItem *item = &mgr->items[index];
    if (length) *length = item->length;
    return item->content;
}

int clipboard_get_count(ClipboardHistory *mgr) {
    return mgr->count;
}

bool clipboard_set_viewer(ClipboardHistory *mgr, HWND hwnd) {
    mgr->viewer_hwnd = hwnd;
    return true;
}

bool clipboard_is_our_change(ClipboardHistory *mgr) {
    return mgr->next_is_our_change;
}

void clipboard_mark_our_change(ClipboardHistory *mgr, bool ours) {
    mgr->next_is_our_change = ours;
}

LRESULT clipboard_handle_message(ClipboardHistory *mgr, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)mgr;
    (void)hwnd;
    (void)msg;
    (void)wParam;
    (void)lParam;
    // Note: Real clipboard monitoring requires SetClipboardViewer
    // For this implementation we handle WM_DRAWCLIPBOARD
    // This is a simplified version
    return 0;
}