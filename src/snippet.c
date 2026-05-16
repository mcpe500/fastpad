#include "snippet.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

static bool snippet_ensure_dir(SnippetManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    return CreateDirectoryA(mgr->data_dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool snippet_init(SnippetManager *mgr, HWND parent) {
    (void)parent; // unused
    memset(mgr, 0, sizeof(SnippetManager));
    
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        snprintf(mgr->data_dir, MAX_PATH, "%s\\FastPad\\snippets", appdata);
    } else {
        return false;
    }
    
    snippet_ensure_dir(mgr);
    snippet_load(mgr);
    return true;
}

void snippet_free(SnippetManager *mgr) {
    snippet_save(mgr);
}

bool snippet_save(SnippetManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\snippets.dat", mgr->data_dir);
    
    FILE *f = fopen(filepath, "wb");
    if (!f) return false;
    
    fwrite(&mgr->count, sizeof(int), 1, f);
    for (int i = 0; i < mgr->count; i++) {
        Snippet *s = &mgr->snippets[i];
        fwrite(s->trigger, sizeof(s->trigger), 1, f);
        fwrite(s->content, sizeof(s->content), 1, f);
        fwrite(s->description, sizeof(s->description), 1, f);
        fwrite(s->language, sizeof(s->language), 1, f);
        fwrite(&s->created, sizeof(FILETIME), 1, f);
    }
    
    fclose(f);
    return true;
}

bool snippet_load(SnippetManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\snippets.dat", mgr->data_dir);
    
    FILE *f = fopen(filepath, "rb");
    if (!f) return true;
    
    if (fread(&mgr->count, sizeof(int), 1, f) != 1) {
        fclose(f);
        return false;
    }
    
    if (mgr->count > MAX_SNIPPETS) mgr->count = MAX_SNIPPETS;
    
    for (int i = 0; i < mgr->count; i++) {
        Snippet *s = &mgr->snippets[i];
        fread(s->trigger, sizeof(s->trigger), 1, f);
        fread(s->content, sizeof(s->content), 1, f);
        fread(s->description, sizeof(s->description), 1, f);
        fread(s->language, sizeof(s->language), 1, f);
        fread(&s->created, sizeof(FILETIME), 1, f);
    }
    
    fclose(f);
    return true;
}

bool snippet_add(SnippetManager *mgr, const char *trigger, const char *content, const char *description, const char *language) {
    if (mgr->count >= MAX_SNIPPETS) return false;
    
    Snippet *s = &mgr->snippets[mgr->count];
    memset(s, 0, sizeof(Snippet));
    
    strncpy(s->trigger, trigger, MAX_SNIPPET_TRIGGER - 1);
    strncpy(s->content, content, MAX_SNIPPET_CONTENT - 1);
    strncpy(s->description, description, MAX_SNIPPET_DESCRIPTION - 1);
    strncpy(s->language, language, 31);
    
    GetSystemTimeAsFileTime(&s->created);
    mgr->count++;
    snippet_save(mgr);
    return true;
}

bool snippet_remove(SnippetManager *mgr, int index) {
    if (index < 0 || index >= mgr->count) return false;
    
    for (int i = index; i < mgr->count - 1; i++) {
        mgr->snippets[i] = mgr->snippets[i + 1];
    }
    
    mgr->count--;
    snippet_save(mgr);
    return true;
}

bool snippet_update(SnippetManager *mgr, int index, const char *trigger, const char *content, const char *description, const char *language) {
    if (index < 0 || index >= mgr->count) return false;
    
    Snippet *s = &mgr->snippets[index];
    strncpy(s->trigger, trigger, MAX_SNIPPET_TRIGGER - 1);
    strncpy(s->content, content, MAX_SNIPPET_CONTENT - 1);
    strncpy(s->description, description, MAX_SNIPPET_DESCRIPTION - 1);
    strncpy(s->language, language, 31);
    
    snippet_save(mgr);
    return true;
}

const char* snippet_get_content(SnippetManager *mgr, int index) {
    if (index < 0 || index >= mgr->count) return NULL;
    return mgr->snippets[index].content;
}

int snippet_find_by_trigger(SnippetManager *mgr, const char *trigger) {
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->snippets[i].trigger, trigger) == 0) return i;
    }
    return -1;
}

int snippet_find_by_language(SnippetManager *mgr, const char *language) {
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->snippets[i].language, language) == 0) return i;
    }
    return -1;
}