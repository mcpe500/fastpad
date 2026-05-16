#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include "core_types.h"
#include <windows.h>

#define MAX_CLIPBOARD_ITEMS 50
#define MAX_CLIPBOARD_CONTENT 65536

typedef struct {
    char content[MAX_CLIPBOARD_CONTENT];
    int length;
    FILETIME copied;
    bool is_text;
} ClipboardItem;

typedef struct {
    ClipboardItem items[MAX_CLIPBOARD_ITEMS];
    int count;
    int max_items;
    HWND viewer_hwnd;
    bool next_is_our_change;
} ClipboardHistory;

bool clipboard_init(ClipboardHistory *mgr);
void clipboard_free(ClipboardHistory *mgr);
bool clipboard_add(ClipboardHistory *mgr, const char *content, int length);
bool clipboard_remove(ClipboardHistory *mgr, int index);
bool clipboard_clear(ClipboardHistory *mgr);
const char* clipboard_get_item(ClipboardHistory *mgr, int index, int *length);
int clipboard_get_count(ClipboardHistory *mgr);
bool clipboard_set_viewer(ClipboardHistory *mgr, HWND hwnd);
bool clipboard_is_our_change(ClipboardHistory *mgr);
void clipboard_mark_our_change(ClipboardHistory *mgr, bool ours);
LRESULT clipboard_handle_message(ClipboardHistory *mgr, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // CLIPBOARD_H