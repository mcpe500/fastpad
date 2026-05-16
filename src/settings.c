/*
 * FastPad Settings Export/Import System Implementation
 * 
 * Handles exporting and importing all application settings to/from JSON.
 */

#include "settings.h"
#include "log.h"
#include "theme.h"
#include "app.h"
#include "core_types.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

// Global settings file path
static char g_settings_path[MAX_PATH] = {0};

// ============================================================================
// Initialization
// ============================================================================

const char* settings_get_filepath(void) {
    if (g_settings_path[0] == '\0') {
        char appdata[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
            snprintf(g_settings_path, MAX_PATH, "%s\\FastPad\\settings.json", appdata);
            
            // Ensure directory exists
            char dir[MAX_PATH];
            strncpy(dir, g_settings_path, MAX_PATH - 1);
            dir[MAX_PATH - 1] = '\0';
            char *p = strrchr(dir, '\\');
            if (p) {
                *p = '\0';
                CreateDirectoryA(dir, NULL);
                *p = '\\';
            }
        }
    }
    return g_settings_path;
}

// ============================================================================
// Settings Export
// ============================================================================

// Helper to safely append formatted string to buffer
static size_t append_format(char **buffer, size_t *offset, size_t *capacity, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    size_t needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    
    if (*offset + needed + 1 > *capacity) {
        size_t new_cap = (*capacity) * 2;
        if (new_cap < *offset + needed + 256) {
            new_cap = *offset + needed + 256;
        }
        char *new_buf = (char *)realloc(*buffer, new_cap);
        if (!new_buf) {
            return 0;
        }
        *buffer = new_buf;
        *capacity = new_cap;
    }
    
    va_start(args, fmt);
    size_t written = vsnprintf(*buffer + *offset, *capacity - *offset, fmt, args);
    va_end(args);
    
    *offset += written;
    return written;
}

SettingsExportResult* settings_export(SettingsCategory category) {
    SettingsExportResult *result = (SettingsExportResult *)malloc(sizeof(SettingsExportResult));
    if (!result) {
        return NULL;
    }
    memset(result, 0, sizeof(SettingsExportResult));
    
    // Estimate size (generous buffer)
    size_t capacity = 16384;
    char *json = (char *)malloc(capacity);
    if (!json) {
        strncpy(result->error, "Out of memory", sizeof(result->error) - 1);
        free(result);
        return NULL;
    }
    
    size_t offset = 0;
    
    append_format(&json, &offset, &capacity, "{\n");
    append_format(&json, &offset, &capacity, "  \"version\": \"1.0\",\n");
    append_format(&json, &offset, &capacity, "  \"category\": %d,\n", (int)category);
    append_format(&json, &offset, &capacity, "  \"settings\": {\n");
    
    // Export based on category
    if (category == SETTINGS_CATEGORY_ALL || category == SETTINGS_CATEGORY_THEME) {
        const char *theme_id = g_app.current_theme ? g_app.current_theme->id : "classic_light";
        append_format(&json, &offset, &capacity, "    \"theme\": \"%s\",\n", theme_id);
    }
    
    if (category == SETTINGS_CATEGORY_ALL || category == SETTINGS_CATEGORY_FONT) {
        append_format(&json, &offset, &capacity, "    \"font_family\": \"%s\",\n", g_app.font_settings.font_family);
        append_format(&json, &offset, &capacity, "    \"font_size\": %d,\n", g_app.font_settings.font_size);
        append_format(&json, &offset, &capacity, "    \"font_bold\": %s,\n", g_app.font_settings.bold ? "true" : "false");
        append_format(&json, &offset, &capacity, "    \"line_height\": %d,\n", g_app.font_settings.line_height);
    }
    
    if (category == SETTINGS_CATEGORY_ALL || category == SETTINGS_CATEGORY_WINDOW) {
        // Get window rect
        RECT rect;
        if (GetWindowRect(g_app.hwnd, &rect)) {
            append_format(&json, &offset, &capacity, "    \"window_x\": %d,\n", (int)rect.left);
            append_format(&json, &offset, &capacity, "    \"window_y\": %d,\n", (int)rect.top);
            append_format(&json, &offset, &capacity, "    \"window_width\": %d,\n", (int)(rect.right - rect.left));
            append_format(&json, &offset, &capacity, "    \"window_height\": %d,\n", (int)(rect.bottom - rect.top));
        }
    }
    
    if (category == SETTINGS_CATEGORY_ALL || category == SETTINGS_CATEGORY_SHORTCUTS) {
        append_format(&json, &offset, &capacity, "    \"shortcuts\": [\n");
        for (int i = 0; i < g_app.shortcut_count; i++) {
            append_format(&json, &offset, &capacity, "      { \"key\": %d, \"modifiers\": %d, \"action\": \"%s\" }%s\n",
                   g_app.shortcuts[i].key,
                   g_app.shortcuts[i].modifiers,
                   g_app.shortcuts[i].action,
                   i < g_app.shortcut_count - 1 ? "," : "");
        }
        append_format(&json, &offset, &capacity, "    ],\n");
    }
    
    if (category == SETTINGS_CATEGORY_ALL || category == SETTINGS_CATEGORY_RECENT) {
        append_format(&json, &offset, &capacity, "    \"recent_files\": [\n");
        for (int i = 0; i < g_app.recent_count; i++) {
            append_format(&json, &offset, &capacity, "      \"%s\"%s\n",
                   g_app.recent_files[i],
                   i < g_app.recent_count - 1 ? "," : "");
        }
        append_format(&json, &offset, &capacity, "    ],\n");
    }
    
    if (category == SETTINGS_CATEGORY_ALL || category == SETTINGS_CATEGORY_TABS) {
        append_format(&json, &offset, &capacity, "    \"word_wrap\": %s,\n", g_app.word_wrap ? "true" : "false");
        append_format(&json, &offset, &capacity, "    \"show_statusbar\": %s,\n", g_app.show_statusbar ? "true" : "false");
        append_format(&json, &offset, &capacity, "    \"show_line_numbers\": %s,\n", g_app.show_line_numbers ? "true" : "false");
        append_format(&json, &offset, &capacity, "    \"highlight_enabled\": %s,\n", g_app.highlight_enabled ? "true" : "false");
        append_format(&json, &offset, &capacity, "    \"auto_save_enabled\": %s,\n", g_app.auto_save_enabled ? "true" : "false");
        append_format(&json, &offset, &capacity, "    \"split_mode\": %d,\n", g_app.split_mode);
        append_format(&json, &offset, &capacity, "    \"split_ratio\": %f,\n", (double)g_app.split_ratio);
    }
    
    // Remove trailing comma and close JSON
    if (offset > 2 && json[offset - 2] == ',') {
        json[offset - 2] = '\n';
        offset -= 2;
    }
    
    append_format(&json, &offset, &capacity, "  }\n");
    append_format(&json, &offset, &capacity, "}\n");
    
    result->success = true;
    result->data = json;
    result->data_size = offset;
    
    return result;
}

bool settings_export_to_file(const char *filepath, SettingsCategory category) {
    if (!filepath) {
        filepath = settings_get_filepath();
    }
    
    SettingsExportResult *result = settings_export(category);
    if (!result || !result->success) {
        if (result) settings_export_result_free(result);
        return false;
    }
    
    FILE *f = fopen(filepath, "w");
    if (!f) {
        strncpy(result->error, "Failed to open file for writing", sizeof(result->error) - 1);
        settings_export_result_free(result);
        return false;
    }
    
    size_t written = fwrite(result->data, 1, result->data_size, f);
    fclose(f);
    
    if (written != result->data_size) {
        settings_export_result_free(result);
        return false;
    }
    
    log_info("Settings exported to: %s", filepath);
    settings_export_result_free(result);
    return true;
}

void settings_export_result_free(SettingsExportResult *result) {
    if (result) {
        if (result->data) {
            free(result->data);
        }
        free(result);
    }
}

// ============================================================================
// Settings Import
// ============================================================================

// Helper function to extract string value
static const char* extract_string(const char *json, const char *key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    
    const char *pos = strstr(json, pattern);
    if (!pos) return NULL;
    
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '"') pos++;
    
    static char value[256];
    size_t i = 0;
    while (*pos && *pos != '"' && *pos != ',' && *pos != '}' && i < sizeof(value) - 1) {
        value[i++] = *pos++;
    }
    value[i] = '\0';
    
    return value;
}

// Helper to extract int value
static int extract_int(const char *json, const char *key, int default_value) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    
    const char *pos = strstr(json, pattern);
    if (!pos) return default_value;
    
    pos += strlen(pattern);
    while (*pos == ' ') pos++;
    
    // Skip to first digit or minus
    while (*pos && *pos != '-' && (*pos < '0' || *pos > '9')) {
        if (*pos == '\0' || *pos == ',' || *pos == '}') return default_value;
        pos++;
    }
    
    int value = 0;
    bool negative = false;
    if (*pos == '-') {
        negative = true;
        pos++;
    }
    
    while (*pos >= '0' && *pos <= '9') {
        value = value * 10 + (*pos - '0');
        pos++;
    }
    
    return negative ? -value : value;
}

// Helper to extract bool value
static bool extract_bool(const char *json, const char *key, bool default_value) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    
    const char *pos = strstr(json, pattern);
    if (!pos) return default_value;
    
    pos += strlen(pattern);
    while (*pos == ' ') pos++;
    
    if (strncmp(pos, "true", 4) == 0) return true;
    if (strncmp(pos, "false", 5) == 0) return false;
    
    return default_value;
}

bool settings_import(const char *json_data, SettingsCategory category) {
    (void)category;
    
    if (!json_data) {
        return false;
    }
    
    // Simple JSON parsing
    const char *json = json_data;
    
    // Find "settings" object
    const char *settings_start = strstr(json, "\"settings\"");
    if (!settings_start) {
        log_error("Invalid settings JSON: missing 'settings' object");
        return false;
    }
    
    // Find opening brace of settings object
    settings_start = strchr(settings_start, '{');
    if (!settings_start) {
        return false;
    }
    
    // Apply settings based on category
    if (category == SETTINGS_CATEGORY_ALL || category == SETTINGS_CATEGORY_THEME) {
        const char *theme_id = extract_string(settings_start, "theme");
        if (theme_id && theme_id[0] != '\0') {
            const Theme *theme = theme_get_by_id(theme_id);
            if (theme) {
                // Apply theme via app function
                g_app.current_theme = (Theme *)theme;
                log_info("Imported theme: %s", theme_id);
            }
        }
    }
    
    if (category == SETTINGS_CATEGORY_ALL || category == SETTINGS_CATEGORY_FONT) {
        const char *font_family = extract_string(settings_start, "font_family");
        int font_size = extract_int(settings_start, "font_size", 14);
        bool font_bold = extract_bool(settings_start, "font_bold", false);
        
        if (font_family && font_family[0] != '\0') {
            strncpy(g_app.font_settings.font_family, font_family, sizeof(g_app.font_settings.font_family) - 1);
            g_app.font_settings.font_family[sizeof(g_app.font_settings.font_family) - 1] = '\0';
        }
        g_app.font_settings.font_size = font_size;
        g_app.font_settings.bold = font_bold;
        
        int line_height = extract_int(settings_start, "line_height", 100);
        g_app.font_settings.line_height = line_height;
        
        log_info("Imported font settings: %s %d", g_app.font_settings.font_family, font_size);
    }
    
    if (category == SETTINGS_CATEGORY_ALL || category == SETTINGS_CATEGORY_WINDOW) {
        int x = extract_int(settings_start, "window_x", -1);
        int y = extract_int(settings_start, "window_y", -1);
        int width = extract_int(settings_start, "window_width", -1);
        int height = extract_int(settings_start, "window_height", -1);
        
        if (x != -1 && y != -1 && width != -1 && height != -1) {
            SetWindowPos(g_app.hwnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
            log_info("Imported window position: %d,%d %dx%d", x, y, width, height);
        }
    }
    
    log_info("Settings imported successfully");
    return true;
}

bool settings_import_from_file(const char *filepath, SettingsCategory category) {
    if (!filepath) {
        filepath = settings_get_filepath();
    }
    
    FILE *f = fopen(filepath, "r");
    if (!f) {
        log_error("Failed to open settings file: %s", filepath);
        return false;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return false;
    }
    
    char *json = (char *)malloc((size_t)size + 1);
    if (!json) {
        fclose(f);
        return false;
    }
    
    size_t read = fread(json, 1, (size_t)size, f);
    fclose(f);
    
    if (read != (size_t)size) {
        free(json);
        return false;
    }
    json[size] = '\0';
    
    bool success = settings_import(json, category);
    free(json);
    
    return success;
}

// ============================================================================
// Individual Settings Access
// ============================================================================

const char* settings_get_theme_id(void) {
    return g_app.current_theme ? g_app.current_theme->id : "classic_light";
}

bool settings_set_theme(const char *theme_id) {
    const Theme *theme = theme_get_by_id(theme_id);
    if (!theme) {
        return false;
    }
    
    g_app.current_theme = (Theme *)theme;
    log_info("Theme changed to: %s", theme_id);
    return true;
}

bool settings_get_font(char *font_family, int *out_font_size, bool *out_bold) {
    if (!font_family) return false;
    
    strncpy(font_family, g_app.font_settings.font_family, 63);
    font_family[63] = '\0';
    
    if (out_font_size) *out_font_size = g_app.font_settings.font_size;
    if (out_bold) *out_bold = g_app.font_settings.bold;
    
    return true;
}

bool settings_set_font(const char *font_family, int font_size, bool bold) {
    if (!font_family) return false;
    
    strncpy(g_app.font_settings.font_family, font_family, sizeof(g_app.font_settings.font_family) - 1);
    g_app.font_settings.font_family[sizeof(g_app.font_settings.font_family) - 1] = '\0';
    g_app.font_settings.font_size = font_size;
    g_app.font_settings.bold = bold;
    
    return true;
}

bool settings_get_window_pos(int *x, int *y, int *width, int *height) {
    RECT rect;
    if (!GetWindowRect(g_app.hwnd, &rect)) {
        return false;
    }
    
    if (x) *x = (int)rect.left;
    if (y) *y = (int)rect.top;
    if (width) *width = (int)(rect.right - rect.left);
    if (height) *height = (int)(rect.bottom - rect.top);
    
    return true;
}

bool settings_set_window_pos(int x, int y, int width, int height) {
    return SetWindowPos(g_app.hwnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE) != 0;
}

int settings_get_shortcut_count(void) {
    return g_app.shortcut_count;
}

bool settings_get_shortcut(int index, int *key, int *modifiers, char *action) {
    if (index < 0 || index >= g_app.shortcut_count) {
        return false;
    }
    
    if (key) *key = g_app.shortcuts[index].key;
    if (modifiers) *modifiers = g_app.shortcuts[index].modifiers;
    if (action) {
        strncpy(action, g_app.shortcuts[index].action, 31);
        action[31] = '\0';
    }
    
    return true;
}

bool settings_set_shortcut(int index, int key, int modifiers, const char *action) {
    if (index < 0 || index >= MAX_SHORTCUTS || !action) {
        return false;
    }
    
    g_app.shortcuts[index].key = key;
    g_app.shortcuts[index].modifiers = modifiers;
    strncpy(g_app.shortcuts[index].action, action, 31);
    g_app.shortcuts[index].action[31] = '\0';
    
    if (index >= g_app.shortcut_count) {
        g_app.shortcut_count = index + 1;
    }
    
    return true;
}

// ============================================================================
// Internal Functions
// ============================================================================

bool settings_parse_json(const char *json, SettingsCategory category) {
    return settings_import(json, category);
}

bool settings_write_json(const char *json, const char *filepath) {
    if (!json || !filepath) {
        return false;
    }
    
    FILE *f = fopen(filepath, "w");
    if (!f) {
        return false;
    }
    
    size_t len = strlen(json);
    size_t written = fwrite(json, 1, len, f);
    fclose(f);
    
    return written == len;
}

char* settings_read_json(const char *filepath) {
    if (!filepath) {
        filepath = settings_get_filepath();
    }
    
    FILE *f = fopen(filepath, "r");
    if (!f) {
        return NULL;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    
    char *json = (char *)malloc((size_t)size + 1);
    if (!json) {
        fclose(f);
        return NULL;
    }
    
    size_t read = fread(json, 1, (size_t)size, f);
    fclose(f);
    
    if (read != (size_t)size) {
        free(json);
        return NULL;
    }
    
    json[size] = '\0';
    return json;
}