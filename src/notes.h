#ifndef NOTES_H
#define NOTES_H

#include "core_types.h"
#include <windows.h>

#define MAX_NOTES 100
#define MAX_NOTE_TITLE 128
#define MAX_NOTE_CONTENT 65536

typedef struct {
    char id[32];
    char title[MAX_NOTE_TITLE];
    char content[MAX_NOTE_CONTENT];
    FILETIME created;
    FILETIME modified;
    bool pinned;
} Note;

typedef struct {
    Note notes[MAX_NOTES];
    int count;
    char data_dir[MAX_PATH];
} NotesManager;

bool notes_init(NotesManager *mgr, HWND parent);
void notes_free(NotesManager *mgr);
bool notes_save(NotesManager *mgr);
bool notes_load(NotesManager *mgr);
bool notes_add(NotesManager *mgr, const char *title, const char *content);
bool notes_remove(NotesManager *mgr, int index);
bool notes_update(NotesManager *mgr, int index, const char *title, const char *content);
bool notes_toggle_pin(NotesManager *mgr, int index);
int notes_find_by_id(NotesManager *mgr, const char *id);

#endif // NOTES_H