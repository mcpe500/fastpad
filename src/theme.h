#ifndef THEME_H
#define THEME_H

#include <windows.h>
#include "core_types.h"

// Theme color palette
typedef struct {
    // Editor colors
    COLORREF editor_bg;        // Editor background
    COLORREF editor_text;       // Normal text color
    COLORREF editor_caret;      // Caret color
    
    // Selection colors
    COLORREF selection_bg;      // Selection background
    COLORREF selection_text;    // Selected text color
    
    // Line number margin
    COLORREF margin_bg;         // Margin/line number background
    COLORREF margin_text;       // Line number text color
    
    // Tab colors
    COLORREF tab_active_bg;     // Active tab background
    COLORREF tab_inactive_bg;   // Inactive tab background
    COLORREF tab_text;          // Tab text color
    
    // Status bar
    COLORREF statusbar_bg;      // Status bar background
    COLORREF statusbar_text;    // Status bar text color
    
    // Menu
    COLORREF menu_bg;           // Menu background
    COLORREF menu_text;         // Menu text color
    
} ThemeColors;

// Font settings
typedef struct {
    char font_family[64];       // Font name (e.g., "Consolas", "JetBrains Mono")
    int font_size;              // Font size in pixels
    int line_height;            // Line height multiplier (e.g., 150 = 1.5)
    bool bold;                  // Bold style
    
} FontSettings;

// Full theme definition
typedef struct {
    const char *id;            // Theme ID (e.g., "classic_light")
    const char *name;          // Display name (e.g., "Classic Light")
    bool is_dark;              // Dark mode flag
    ThemeColors colors;         // Color palette
    FontSettings font;         // Font settings
    
} Theme;

// Total number of preset themes
#define THEME_PRESET_COUNT 6

// Preset theme IDs
#define THEME_CLASSIC_LIGHT  "classic_light"
#define THEME_CLASSIC_DARK   "classic_dark"
#define THEME_MONOKAI        "monokai"
#define THEME_SOLARIZED_LIGHT "solarized_light"
#define THEME_SOLARIZED_DARK  "solarized_dark"
#define THEME_DRACULA        "dracula"

// Get all preset themes
const Theme* theme_get_presets(void);
const Theme* theme_get_by_id(const char *id);

// Apply theme to app (update g_app settings)
void theme_apply(const Theme *theme);

// Load/save theme settings from config file
void theme_load_settings(const char *config_path);
void theme_save_settings(const char *config_path);

// Parse color from hex string (e.g., "#1E1E1E")
COLORREF theme_parse_color(const char *hex);

// Convert color to hex string
void theme_color_to_hex(COLORREF color, char *out, size_t out_size);

#endif // THEME_H