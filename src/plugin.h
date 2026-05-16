/*
 * FastPad Plugin System
 * 
 * Provides a simple plugin/extension system with hooks.
 */

#ifndef PLUGIN_H
#define PLUGIN_H

#include "core_types.h"
#include <windows.h>

// Maximum number of loaded plugins
#define MAX_PLUGINS 32

// Maximum plugin name length
#define MAX_PLUGIN_NAME 128

// Plugin state
typedef enum {
    PLUGIN_STATE_UNLOADED,   // Plugin not loaded
    PLUGIN_STATE_LOADED,     // Plugin loaded successfully
    PLUGIN_STATE_ERROR,      // Plugin load failed
    PLUGIN_STATE_DISABLED    // Plugin disabled by user
} PluginState;

// Plugin manifest (from manifest.json)
typedef struct {
    char id[MAX_PLUGIN_NAME];       // Unique plugin identifier
    char name[MAX_PLUGIN_NAME];     // Display name
    char version[32];              // Version string (e.g., "1.0.0")
    char author[128];              // Author name
    char description[512];         // Plugin description
    char entrypoint[128];          // Entry point function name
    char dependency[MAX_PLUGIN_NAME]; // Required plugin ID (optional)
    bool enabled;                  // Whether plugin is enabled
} PluginManifest;

// Plugin handle structure
typedef struct {
    PluginManifest manifest;        // Plugin metadata
    PluginState state;              // Current state
    HMODULE module;                 // Loaded module handle
    void *instance_data;           // Plugin-specific instance data
    FILETIME load_time;            // When plugin was loaded
} Plugin;

// Plugin hook types
typedef enum {
    HOOK_FILE_OPEN,        // Called when a file is opened
    HOOK_FILE_SAVE,        // Called when a file is saved
    HOOK_FILE_CLOSE,       // Called when a file is closed
    HOOK_EDITOR_INIT,      // Called when editor is initialized
    HOOK_EDITOR_KEYDOWN,   // Called on keydown in editor
    HOOK_EDITOR_CHAR,      // Called on char input in editor
    HOOK_MENU_BUILD,       // Called when menu is being built
    HOOK_APP_INIT,         // Called when app initializes
    HOOK_APP_EXIT,         // Called when app exits
    HOOK_COUNT             // Total number of hooks
} PluginHookType;

// Plugin hook callback function signature
typedef void (*PluginHookFunc)(int hook_id, void *param);

// Plugin initialization function signature
typedef bool (*PluginInitFunc)(void);

// ============================================================================
// Plugin System Functions
// ============================================================================

// Initialize the plugin system
bool plugin_init(void);

// Shutdown the plugin system and unload all plugins
void plugin_shutdown(void);

// Load a plugin from a directory
bool plugin_load(const char *plugin_dir);

// Unload a specific plugin
bool plugin_unload(const char *plugin_id);

// Load all plugins from the plugins directory
bool plugin_load_all(void);

// Get the plugins directory path
const char* plugin_get_directory(void);
Plugin* plugin_get(const char *plugin_id);

// Get all loaded plugins
Plugin* plugin_get_all(int *count);

// Enable or disable a plugin
bool plugin_set_enabled(const char *plugin_id, bool enabled);

// Check if a plugin is enabled
bool plugin_is_enabled(const char *plugin_id);

// Get the plugins directory path
const char* plugin_get_directory(void);

// Reload a plugin (unload then load)
bool plugin_reload(const char *plugin_id);

// ============================================================================
// Plugin Hooks
// ============================================================================

// Register a hook callback for a specific hook type
bool plugin_register_hook(PluginHookType type, PluginHookFunc callback);

// Unregister a hook callback
bool plugin_unregister_hook(PluginHookType type, PluginHookFunc callback);

// Call all hooks for a specific type
void plugin_call_hooks(PluginHookType type, void *param);

// ============================================================================
// Plugin Discovery
// ============================================================================

// Discover all plugins in the plugins directory
int plugin_discover(char **plugin_ids, int max_count);

// Get manifest for a plugin without loading it
bool plugin_get_manifest(const char *plugin_dir, PluginManifest *manifest);

// ============================================================================
// Internal Functions
// ============================================================================

// Load plugin manifest from manifest.json
bool plugin_load_manifest(const char *plugin_dir, PluginManifest *manifest);

// Call a specific plugin's entry point
bool plugin_call_entrypoint(Plugin *plugin);

// Convert hook type to string for logging
const char* plugin_hook_to_string(PluginHookType type);

#endif // PLUGIN_H