/*
 * FastPad Settings Export/Import System
 * 
 * Handles exporting and importing all application settings to/from JSON.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "core_types.h"
#include <windows.h>

// Settings categories for selective export/import
typedef enum {
    SETTINGS_CATEGORY_ALL = 0,
    SETTINGS_CATEGORY_THEME = 1,
    SETTINGS_CATEGORY_FONT = 2,
    SETTINGS_CATEGORY_SHORTCUTS = 3,
    SETTINGS_CATEGORY_RECENT = 4,
    SETTINGS_CATEGORY_WINDOW = 5,
    SETTINGS_CATEGORY_TABS = 6
} SettingsCategory;

// Settings export result
typedef struct {
    bool success;
    char *data;          // JSON string (caller must free)
    size_t data_size;    // Size of data
    char error[256];     // Error message if failed
} SettingsExportResult;

// ============================================================================
// Settings Export/Import Functions
// ============================================================================

// Export all settings to JSON
SettingsExportResult* settings_export(SettingsCategory category);

// Export settings to a file
bool settings_export_to_file(const char *filepath, SettingsCategory category);

// Import settings from JSON data
bool settings_import(const char *json_data, SettingsCategory category);

// Import settings from a file
bool settings_import_from_file(const char *filepath, SettingsCategory category);

// Free an export result
void settings_export_result_free(SettingsExportResult *result);

// ============================================================================
// Individual Settings Access
// ============================================================================

// Get current theme ID
const char* settings_get_theme_id(void);

// Set theme from ID
bool settings_set_theme(const char *theme_id);

// Get font settings
bool settings_get_font(char *font_family, int *font_size, bool *bold);

// Set font settings
bool settings_set_font(const char *font_family, int font_size, bool bold);

// Get window position and size
bool settings_get_window_pos(int *x, int *y, int *width, int *height);

// Set window position and size
bool settings_set_window_pos(int x, int y, int width, int height);

// Get shortcut count
int settings_get_shortcut_count(void);

// Get a specific shortcut
bool settings_get_shortcut(int index, int *key, int *modifiers, char *action);

// Set a specific shortcut
bool settings_set_shortcut(int index, int key, int modifiers, const char *action);

// ============================================================================
// Internal Functions
// ============================================================================

// Get the settings file path
const char* settings_get_filepath(void);

// Parse JSON and apply settings
bool settings_parse_json(const char *json, SettingsCategory category);

// Write settings to JSON file
bool settings_write_json(const char *json, const char *filepath);

// Read settings from JSON file
char* settings_read_json(const char *filepath);

#endif // SETTINGS_H