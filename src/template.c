#include "template.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

static bool template_ensure_dir(TemplateManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    return CreateDirectoryA(mgr->data_dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool template_init(TemplateManager *mgr, HWND parent) {
    (void)parent; // unused
    memset(mgr, 0, sizeof(TemplateManager));
    
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        snprintf(mgr->data_dir, MAX_PATH, "%s\\FastPad\\templates", appdata);
    } else {
        return false;
    }
    
    template_ensure_dir(mgr);
    template_load(mgr);
    return true;
}

void template_free(TemplateManager *mgr) {
    template_save(mgr);
}

bool template_save(TemplateManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\templates.dat", mgr->data_dir);
    
    FILE *f = fopen(filepath, "wb");
    if (!f) return false;
    
    fwrite(&mgr->count, sizeof(int), 1, f);
    for (int i = 0; i < mgr->count; i++) {
        Template *t = &mgr->templates[i];
        fwrite(t->name, sizeof(t->name), 1, f);
        fwrite(t->content, sizeof(t->content), 1, f);
        fwrite(t->extension, sizeof(t->extension), 1, f);
        fwrite(&t->created, sizeof(FILETIME), 1, f);
    }
    
    fclose(f);
    return true;
}

bool template_load(TemplateManager *mgr) {
    if (mgr->data_dir[0] == '\0') return false;
    
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\templates.dat", mgr->data_dir);
    
    FILE *f = fopen(filepath, "rb");
    if (!f) return true;
    
    if (fread(&mgr->count, sizeof(int), 1, f) != 1) {
        fclose(f);
        return false;
    }
    
    if (mgr->count > MAX_TEMPLATES) mgr->count = MAX_TEMPLATES;
    
    for (int i = 0; i < mgr->count; i++) {
        Template *t = &mgr->templates[i];
        fread(t->name, sizeof(t->name), 1, f);
        fread(t->content, sizeof(t->content), 1, f);
        fread(t->extension, sizeof(t->extension), 1, f);
        fread(&t->created, sizeof(FILETIME), 1, f);
    }
    
    fclose(f);
    return true;
}

bool template_add(TemplateManager *mgr, const char *name, const char *content, const char *extension) {
    if (mgr->count >= MAX_TEMPLATES) return false;
    
    Template *t = &mgr->templates[mgr->count];
    memset(t, 0, sizeof(Template));
    
    strncpy(t->name, name, MAX_TEMPLATE_NAME - 1);
    strncpy(t->content, content, MAX_TEMPLATE_CONTENT - 1);
    strncpy(t->extension, extension, MAX_TEMPLATE_EXT - 1);
    
    GetSystemTimeAsFileTime(&t->created);
    mgr->count++;
    template_save(mgr);
    return true;
}

bool template_remove(TemplateManager *mgr, int index) {
    if (index < 0 || index >= mgr->count) return false;
    
    for (int i = index; i < mgr->count - 1; i++) {
        mgr->templates[i] = mgr->templates[i + 1];
    }
    
    mgr->count--;
    template_save(mgr);
    return true;
}

bool template_update(TemplateManager *mgr, int index, const char *name, const char *content, const char *extension) {
    if (index < 0 || index >= mgr->count) return false;
    
    Template *t = &mgr->templates[index];
    strncpy(t->name, name, MAX_TEMPLATE_NAME - 1);
    strncpy(t->content, content, MAX_TEMPLATE_CONTENT - 1);
    strncpy(t->extension, extension, MAX_TEMPLATE_EXT - 1);
    
    template_save(mgr);
    return true;
}

const char* template_get_content(TemplateManager *mgr, int index) {
    if (index < 0 || index >= mgr->count) return NULL;
    return mgr->templates[index].content;
}

int template_find_by_name(TemplateManager *mgr, const char *name) {
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->templates[i].name, name) == 0) return i;
    }
    return -1;
}