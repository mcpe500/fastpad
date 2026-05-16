#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "core_types.h"
#include <windows.h>

#define MAX_TEMPLATES 50
#define MAX_TEMPLATE_NAME 128
#define MAX_TEMPLATE_CONTENT 32768
#define MAX_TEMPLATE_EXT 32

typedef struct {
    char name[MAX_TEMPLATE_NAME];
    char content[MAX_TEMPLATE_CONTENT];
    char extension[MAX_TEMPLATE_EXT];
    FILETIME created;
} Template;

typedef struct {
    Template templates[MAX_TEMPLATES];
    int count;
    char data_dir[MAX_PATH];
} TemplateManager;

bool template_init(TemplateManager *mgr, HWND parent);
void template_free(TemplateManager *mgr);
bool template_save(TemplateManager *mgr);
bool template_load(TemplateManager *mgr);
bool template_add(TemplateManager *mgr, const char *name, const char *content, const char *extension);
bool template_remove(TemplateManager *mgr, int index);
bool template_update(TemplateManager *mgr, int index, const char *name, const char *content, const char *extension);
const char* template_get_content(TemplateManager *mgr, int index);
int template_find_by_name(TemplateManager *mgr, const char *name);

#endif // TEMPLATE_H