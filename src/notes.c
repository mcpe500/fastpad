#include "notes.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

static bool notes_ensure_dir(NotesManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    return CreateDirectoryA(mgr->data_dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool notes_init(NotesManager *mgr, HWND parent) {
    (void)parent; // unused
    memset(mgr, 0, sizeof(NotesManager));
    
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        snprintf(mgr->data_dir, MAX_PATH, "%s\\FastPad\\notes", appdata);
    } else {
        return false;
    }
    
    notes_ensure_dir(mgr);
    notes_load(mgr);
    return true;
}

void notes_free(NotesManager *mgr) {
    notes_save(mgr);
}

bool notes_save(NotesManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\notes.dat", mgr->data_dir);
    
    FILE *f = fopen(filepath, "wb");
    if (!f) return false;
    
    fwrite(&mgr->count, sizeof(int), 1, f);
    for (int i = 0; i < mgr->count; i++) {
        Note *n = &mgr->notes[i];
        fwrite(n->id, sizeof(n->id), 1, f);
        fwrite(n->title, sizeof(n->title), 1, f);
        fwrite(n->content, sizeof(n->content), 1, f);
        fwrite(&n->created, sizeof(FILETIME), 1, f);
        fwrite(&n->modified, sizeof(FILETIME), 1, f);
        fwrite(&n->pinned, sizeof(bool), 1, f);
    }
    
    fclose(f);
    return true;
}

bool notes_load(NotesManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\notes.dat", mgr->data_dir);
    
    FILE *f = fopen(filepath, "rb");
    if (!f) return true; // No file yet is OK
    
    if (fread(&mgr->count, sizeof(int), 1, f) != 1) {
        fclose(f);
        return false;
    }
    
    if (mgr->count > MAX_NOTES) mgr->count = MAX_NOTES;
    
    for (int i = 0; i < mgr->count; i++) {
        Note *n = &mgr->notes[i];
        fread(n->id, sizeof(n->id), 1, f);
        fread(n->title, sizeof(n->title), 1, f);
        fread(n->content, sizeof(n->content), 1, f);
        fread(&n->created, sizeof(FILETIME), 1, f);
        fread(&n->modified, sizeof(FILETIME), 1, f);
        fread(&n->pinned, sizeof(bool), 1, f);
    }
    
    fclose(f);
    return true;
}

bool notes_add(NotesManager *mgr, const char *title, const char *content) {
    if (mgr->count >= MAX_NOTES) return false;
    
    Note *n = &mgr->notes[mgr->count];
    memset(n, 0, sizeof(Note));
    
    // Generate ID from timestamp
    SYSTEMTIME st;
    GetSystemTime(&st);
    snprintf(n->id, sizeof(n->id), "note_%04d%02d%02d%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    
    strncpy(n->title, title, MAX_NOTE_TITLE - 1);
    strncpy(n->content, content, MAX_NOTE_CONTENT - 1);
    
    GetSystemTimeAsFileTime(&n->created);
    n->modified = n->created;
    n->pinned = false;
    
    mgr->count++;
    notes_save(mgr);
    return true;
}

bool notes_remove(NotesManager *mgr, int index) {
    if (index < 0 || index >= mgr->count) return false;
    
    // Shift remaining notes
    for (int i = index; i < mgr->count - 1; i++) {
        mgr->notes[i] = mgr->notes[i + 1];
    }
    
    mgr->count--;
    notes_save(mgr);
    return true;
}

bool notes_update(NotesManager *mgr, int index, const char *title, const char *content) {
    if (index < 0 || index >= mgr->count) return false;
    
    Note *n = &mgr->notes[index];
    strncpy(n->title, title, MAX_NOTE_TITLE - 1);
    strncpy(n->content, content, MAX_NOTE_CONTENT - 1);
    
    GetSystemTimeAsFileTime(&n->modified);
    notes_save(mgr);
    return true;
}

bool notes_toggle_pin(NotesManager *mgr, int index) {
    if (index < 0 || index >= mgr->count) return false;
    
    mgr->notes[index].pinned = !mgr->notes[index].pinned;
    notes_save(mgr);
    return true;
}

int notes_find_by_id(NotesManager *mgr, const char *id) {
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->notes[i].id, id) == 0) return i;
    }
    return -1;
}