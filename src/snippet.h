#ifndef SNIPPET_H
#define SNIPPET_H

#include "core_types.h"
#include <windows.h>

#define MAX_SNIPPETS 100
#define MAX_SNIPPET_TRIGGER 64
#define MAX_SNIPPET_CONTENT 16384
#define MAX_SNIPPET_DESCRIPTION 256

typedef struct {
    char trigger[MAX_SNIPPET_TRIGGER];
    char content[MAX_SNIPPET_CONTENT];
    char description[MAX_SNIPPET_DESCRIPTION];
    char language[32];
    FILETIME created;
} Snippet;

typedef struct {
    Snippet snippets[MAX_SNIPPETS];
    int count;
    char data_dir[MAX_PATH];
} SnippetManager;

bool snippet_init(SnippetManager *mgr, HWND parent);
void snippet_free(SnippetManager *mgr);
bool snippet_save(SnippetManager *mgr);
bool snippet_load(SnippetManager *mgr);
bool snippet_add(SnippetManager *mgr, const char *trigger, const char *content, const char *description, const char *language);
bool snippet_remove(SnippetManager *mgr, int index);
bool snippet_update(SnippetManager *mgr, int index, const char *trigger, const char *content, const char *description, const char *language);
const char* snippet_get_content(SnippetManager *mgr, int index);
int snippet_find_by_trigger(SnippetManager *mgr, const char *trigger);
int snippet_find_by_language(SnippetManager *mgr, const char *language);

#endif // SNIPPET_H