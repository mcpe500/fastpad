#include "theme.h"
#include <stdio.h>
#include <string.h>

// Preset themes
static const Theme presets[THEME_PRESET_COUNT] = {
    // Classic Light
    {
        THEME_CLASSIC_LIGHT,
        "Classic Light",
        false,
        {
            // Editor
            RGB(255, 255, 255),   // editor_bg
            RGB(0, 0, 0),          // editor_text
            RGB(0, 0, 0),          // editor_caret
            
            // Selection
            RGB(0, 120, 215),      // selection_bg
            RGB(255, 255, 255),    // selection_text
            
            // Margin
            RGB(240, 240, 240),    // margin_bg
            RGB(128, 128, 128),    // margin_text
            
            // Tab
            RGB(230, 230, 230),    // tab_active_bg
            RGB(200, 200, 200),    // tab_inactive_bg
            RGB(0, 0, 0),          // tab_text
            
            // Status bar
            RGB(240, 240, 240),    // statusbar_bg
            RGB(0, 0, 0),          // statusbar_text
            
            // Menu
            RGB(240, 240, 240),    // menu_bg
            RGB(0, 0, 0),          // menu_text
        },
        { "Consolas", 14, 100, false }
    },
    
    // Classic Dark
    {
        THEME_CLASSIC_DARK,
        "Classic Dark",
        true,
        {
            // Editor
            RGB(30, 30, 30),       // editor_bg
            RGB(200, 200, 200),    // editor_text
            RGB(200, 200, 200),    // editor_caret
            
            // Selection
            RGB(0, 100, 180),      // selection_bg
            RGB(255, 255, 255),    // selection_text
            
            // Margin
            RGB(40, 40, 40),       // margin_bg
            RGB(100, 100, 100),    // margin_text
            
            // Tab
            RGB(50, 50, 50),       // tab_active_bg
            RGB(35, 35, 35),       // tab_inactive_bg
            RGB(200, 200, 200),    // tab_text
            
            // Status bar
            RGB(35, 35, 35),       // statusbar_bg
            RGB(200, 200, 200),    // statusbar_text
            
            // Menu
            RGB(45, 45, 45),       // menu_bg
            RGB(200, 200, 200),    // menu_text
        },
        { "Consolas", 14, 100, false }
    },
    
    // Monokai
    {
        THEME_MONOKAI,
        "Monokai",
        true,
        {
            // Editor
            RGB(39, 41, 46),       // editor_bg
            RGB(248, 248, 242),   // editor_text
            RGB(248, 248, 242),   // editor_caret
            
            // Selection
            RGB(102, 102, 102),   // selection_bg
            RGB(248, 248, 242),   // selection_text
            
            // Margin
            RGB(55, 55, 55),       // margin_bg
            RGB(128, 128, 128),    // margin_text
            
            // Tab
            RGB(55, 55, 55),       // tab_active_bg
            RGB(35, 35, 35),       // tab_inactive_bg
            RGB(248, 248, 242),   // tab_text
            
            // Status bar
            RGB(50, 50, 50),       // statusbar_bg
            RGB(248, 248, 242),   // statusbar_text
            
            // Menu
            RGB(45, 45, 45),       // menu_bg
            RGB(248, 248, 242),   // menu_text
        },
        { "Consolas", 14, 100, false }
    },
    
    // Solarized Light
    {
        THEME_SOLARIZED_LIGHT,
        "Solarized Light",
        false,
        {
            // Editor
            RGB(253, 246, 227),    // editor_bg (base3)
            RGB(101, 123, 131),    // editor_text (base01)
            RGB(220, 50, 47),       // editor_caret (red)
            
            // Selection
            RGB(181, 137, 0),      // selection_bg (yellow)
            RGB(253, 246, 227),    // selection_text
            
            // Margin
            RGB(238, 232, 213),    // margin_bg (base2)
            RGB(147, 161, 161),    // margin_text (base1)
            
            // Tab
            RGB(238, 232, 213),    // tab_active_bg
            RGB(253, 246, 227),    // tab_inactive_bg
            RGB(101, 123, 131),    // tab_text
            
            // Status bar
            RGB(238, 232, 213),    // statusbar_bg
            RGB(101, 123, 131),    // statusbar_text
            
            // Menu
            RGB(238, 232, 213),    // menu_bg
            RGB(101, 123, 131),    // menu_text
        },
        { "Consolas", 14, 100, false }
    },
    
    // Solarized Dark
    {
        THEME_SOLARIZED_DARK,
        "Solarized Dark",
        true,
        {
            // Editor
            RGB(0, 43, 54),         // editor_bg (base03)
            RGB(131, 148, 150),    // editor_text (base0)
            RGB(220, 50, 47),       // editor_caret (red)
            
            // Selection
            RGB(7, 54, 66),         // selection_bg (base02)
            RGB(131, 148, 150),    // selection_text
            
            // Margin
            RGB(7, 54, 66),         // margin_bg
            RGB(88, 110, 117),     // margin_text (base01)
            
            // Tab
            RGB(7, 54, 66),         // tab_active_bg
            RGB(0, 43, 54),         // tab_inactive_bg
            RGB(131, 148, 150),    // tab_text
            
            // Status bar
            RGB(7, 54, 66),         // statusbar_bg
            RGB(131, 148, 150),    // statusbar_text
            
            // Menu
            RGB(7, 54, 66),         // menu_bg
            RGB(131, 148, 150),    // menu_text
        },
        { "Consolas", 14, 100, false }
    },
    
    // Dracula
    {
        THEME_DRACULA,
        "Dracula",
        true,
        {
            // Editor
            RGB(40, 42, 54),        // editor_bg
            RGB(248, 248, 242),   // editor_text
            RGB(248, 248, 242),   // editor_caret
            
            // Selection
            RGB(68, 71, 90),       // selection_bg
            RGB(248, 248, 242),   // selection_text
            
            // Margin
            RGB(55, 55, 65),       // margin_bg
            RGB(128, 128, 128),    // margin_text
            
            // Tab
            RGB(55, 55, 65),       // tab_active_bg
            RGB(40, 42, 54),        // tab_inactive_bg
            RGB(248, 248, 242),   // tab_text
            
            // Status bar
            RGB(40, 42, 54),        // statusbar_bg
            RGB(248, 248, 242),   // statusbar_text
            
            // Menu
            RGB(50, 50, 60),       // menu_bg
            RGB(248, 248, 242),   // menu_text
        },
        { "Consolas", 14, 100, false }
    }
};

const Theme* theme_get_presets(void) {
    return presets;
}

const Theme* theme_get_by_id(const char *id) {
    if (!id) return &presets[0];
    for (int i = 0; i < THEME_PRESET_COUNT; i++) {
        if (strcmp(presets[i].id, id) == 0) {
            return &presets[i];
        }
    }
    return &presets[0]; // Default to classic light
}

void theme_apply(const Theme *theme) {
    if (!theme) return;
    
    // This will be called to apply theme colors to g_app
    // The actual implementation is in app.c
    extern void app_apply_theme(const Theme *theme);
    app_apply_theme(theme);
}

COLORREF theme_parse_color(const char *hex) {
    if (!hex || hex[0] != '#') return RGB(0, 0, 0);
    if (strlen(hex) < 7) return RGB(0, 0, 0);
    
    int r, g, b;
    if (sscanf(hex + 1, "%2x%2x%2x", &r, &g, &b) == 3) {
        return RGB(r, g, b);
    }
    return RGB(0, 0, 0);
}

void theme_color_to_hex(COLORREF color, char *out, size_t out_size) {
    snprintf(out, out_size, "#%02X%02X%02X", 
             GetRValue(color), 
             GetGValue(color), 
             GetBValue(color));
}

void theme_load_settings(const char *config_path) {
    (void)config_path;
    // Will load theme settings from config file
    // Implementation in app.c
}

void theme_save_settings(const char *config_path) {
    (void)config_path;
    // Will save theme settings to config file
    // Implementation in app.c
}