/*
 * FastPad Plugin System Implementation
 * 
 * Provides a simple plugin/extension system with hooks.
 */

#include "plugin.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

// Global state
static Plugin g_plugins[MAX_PLUGINS];
static int g_plugin_count = 0;
static char g_plugins_dir[MAX_PATH] = {0};

// Hook callbacks storage
#define MAX_HOOK_CALLBACKS 16
static struct {
    PluginHookFunc callbacks[MAX_HOOK_CALLBACKS];
    int callback_count;
} g_hooks[HOOK_COUNT];

// ============================================================================
// Initialization
// ============================================================================

bool plugin_init(void) {
    char appdata[MAX_PATH];
    
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) != S_OK) {
        log_error("Failed to get AppData folder path");
        return false;
    }
    
    snprintf(g_plugins_dir, MAX_PATH, "%s\\FastPad\\plugins", appdata);
    
    // Create plugins directory if it doesn't exist
    CreateDirectoryA(g_plugins_dir, NULL);
    
    // Initialize hook arrays
    for (int i = 0; i < HOOK_COUNT; i++) {
        g_hooks[i].callback_count = 0;
    }
    
    // Initialize plugin array
    memset(g_plugins, 0, sizeof(g_plugins));
    g_plugin_count = 0;
    
    log_info("Plugin system initialized: %s", g_plugins_dir);
    
    // Auto-discover and load plugins
    plugin_load_all();
    
    return true;
}

void plugin_shutdown(void) {
    // Unload all plugins
    for (int i = 0; i < g_plugin_count; i++) {
        if (g_plugins[i].module) {
            FreeLibrary(g_plugins[i].module);
            g_plugins[i].module = NULL;
        }
        g_plugins[i].state = PLUGIN_STATE_UNLOADED;
    }
    
    g_plugin_count = 0;
    log_info("Plugin system shut down");
}

const char* plugin_get_directory(void) {
    return g_plugins_dir;
}

bool plugin_ensure_dir(void) {
    if (g_plugins_dir[0] == '\0') {
        return false;
    }
    
    // Create directory tree if it doesn't exist
    char dir[MAX_PATH];
    strncpy(dir, g_plugins_dir, MAX_PATH - 1);
    dir[MAX_PATH - 1] = '\0';
    
    for (char *p = dir + 3; *p; p++) {
        if (*p == '\\') {
            *p = '\0';
            CreateDirectoryA(dir, NULL);
            *p = '\\';
        }
    }
    
    return true;
}

// ============================================================================
// Plugin Loading
// ============================================================================

bool plugin_load_all(void) {
    if (!plugin_ensure_dir()) {
        return false;
    }
    
    // Find all subdirectories in plugins folder
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*", g_plugins_dir);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    do {
        // Skip . and ..
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }
        
        // Check if it's a directory
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char plugin_path[MAX_PATH];
            snprintf(plugin_path, MAX_PATH, "%s\\%s", g_plugins_dir, fd.cFileName);
            
            // Try to load the plugin
            plugin_load(plugin_path);
        }
        
    } while (FindNextFileA(hFind, &fd));
    
    FindClose(hFind);
    
    log_info("Loaded %d plugins", g_plugin_count);
    return true;
}

bool plugin_load(const char *plugin_dir) {
    if (!plugin_dir) {
        return false;
    }
    
    if (g_plugin_count >= MAX_PLUGINS) {
        log_error("Maximum plugin count reached (%d)", MAX_PLUGINS);
        return false;
    }
    
    // Load manifest
    PluginManifest manifest;
    if (!plugin_load_manifest(plugin_dir, &manifest)) {
        log_error("Failed to load plugin manifest from: %s", plugin_dir);
        return false;
    }
    
    // Check if plugin already loaded
    Plugin *existing = plugin_get(manifest.id);
    if (existing) {
        log_info("Plugin already loaded: %s", manifest.id);
        return true;
    }
    
    // Build path to DLL
    char dll_path[MAX_PATH];
    snprintf(dll_path, MAX_PATH, "%s\\%s.dll", plugin_dir, manifest.id);
    
    // Load the DLL
    HMODULE module = LoadLibraryA(dll_path);
    if (!module) {
        DWORD error = GetLastError();
        (void)error; // Suppress unused warning
        log_error("Failed to load plugin DLL %s (error %lu)", dll_path, error);
        
        // Add plugin with error state
        Plugin *p = &g_plugins[g_plugin_count++];
        memcpy(&p->manifest, &manifest, sizeof(PluginManifest));
        p->state = PLUGIN_STATE_ERROR;
        p->module = NULL;
        return false;
    }
    
    // Get entry point function
    PluginInitFunc entrypoint = (PluginInitFunc)GetProcAddress(module, manifest.entrypoint);
    if (!entrypoint) {
        log_error("Plugin entry point '%s' not found in %s", manifest.entrypoint, dll_path);
        FreeLibrary(module);
        
        Plugin *p = &g_plugins[g_plugin_count++];
        memcpy(&p->manifest, &manifest, sizeof(PluginManifest));
        p->state = PLUGIN_STATE_ERROR;
        p->module = NULL;
        return false;
    }
    
    // Call entry point
    if (!entrypoint()) {
        log_error("Plugin entry point returned failure for: %s", manifest.id);
        FreeLibrary(module);
        
        Plugin *p = &g_plugins[g_plugin_count++];
        memcpy(&p->manifest, &manifest, sizeof(PluginManifest));
        p->state = PLUGIN_STATE_ERROR;
        p->module = NULL;
        return false;
    }
    
    // Success - add to plugin list
    Plugin *p = &g_plugins[g_plugin_count++];
    memcpy(&p->manifest, &manifest, sizeof(PluginManifest));
    p->state = PLUGIN_STATE_LOADED;
    p->module = module;
    p->instance_data = NULL;
    GetSystemTimeAsFileTime(&p->load_time);
    
    log_info("Loaded plugin: %s v%s by %s", manifest.name, manifest.version, manifest.author);
    
    return true;
}

bool plugin_unload(const char *plugin_id) {
    if (!plugin_id) {
        return false;
    }
    
    for (int i = 0; i < g_plugin_count; i++) {
        if (strcmp(g_plugins[i].manifest.id, plugin_id) == 0) {
            if (g_plugins[i].module) {
                FreeLibrary(g_plugins[i].module);
                g_plugins[i].module = NULL;
            }
            g_plugins[i].state = PLUGIN_STATE_UNLOADED;
            
            // Shift remaining plugins
            for (int j = i; j < g_plugin_count - 1; j++) {
                g_plugins[j] = g_plugins[j + 1];
            }
            g_plugin_count--;
            
            log_info("Unloaded plugin: %s", plugin_id);
            return true;
        }
    }
    
    return false;
}

bool plugin_reload(const char *plugin_id) {
    if (!plugin_unload(plugin_id)) {
        return false;
    }
    
    // Re-discover and load
    return plugin_load_all();
}

// ============================================================================
// Plugin Access
// ============================================================================

Plugin* plugin_get(const char *plugin_id) {
    if (!plugin_id) {
        return NULL;
    }
    
    for (int i = 0; i < g_plugin_count; i++) {
        if (strcmp(g_plugins[i].manifest.id, plugin_id) == 0) {
            return &g_plugins[i];
        }
    }
    
    return NULL;
}

Plugin* plugin_get_all(int *count) {
    if (count) {
        *count = g_plugin_count;
    }
    return g_plugins;
}

bool plugin_set_enabled(const char *plugin_id, bool enabled) {
    Plugin *p = plugin_get(plugin_id);
    if (!p) {
        return false;
    }
    
    p->manifest.enabled = enabled;
    
    if (!enabled && p->state == PLUGIN_STATE_LOADED) {
        // Unload if currently loaded
        if (p->module) {
            FreeLibrary(p->module);
            p->module = NULL;
        }
        p->state = PLUGIN_STATE_DISABLED;
    }
    
    return true;
}

bool plugin_is_enabled(const char *plugin_id) {
    Plugin *p = plugin_get(plugin_id);
    if (!p) {
        return false;
    }
    return p->manifest.enabled;
}

// ============================================================================
// Plugin Manifest
// ============================================================================

bool plugin_load_manifest(const char *plugin_dir, PluginManifest *manifest) {
    if (!plugin_dir || !manifest) {
        return false;
    }
    
    char manifest_path[MAX_PATH];
    snprintf(manifest_path, MAX_PATH, "%s\\manifest.json", plugin_dir);
    
    FILE *f = fopen(manifest_path, "r");
    if (!f) {
        return false;
    }
    
    // Initialize with defaults
    memset(manifest, 0, sizeof(PluginManifest));
    manifest->enabled = true;
    
    // Read file content
    char buffer[8192];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (bytes_read == 0) {
        return false;
    }
    buffer[bytes_read] = '\0';
    
    // Simple JSON parsing - extract key fields
    // Note: This is a minimal parser for plugin manifests only
    
    char *json = buffer;
    char *key;
    char *value;
    
    while ((key = strstr(json, "\"")) != NULL) {
        key++;
        char *key_end = strstr(key, "\"");
        if (!key_end) break;
        
        size_t key_len = key_end - key;
        
        // Find colon
        char *colon = key_end + 1;
        while (*colon == ' ' || *colon == ':') colon++;
        
        // Find value (string starts with ")
        if (*colon != '"') {
            json = colon;
            continue;
        }
        colon++;
        
        value = colon;
        char *value_end = strstr(value, "\"");
        if (!value_end) {
            json = value;
            continue;
        }
        
        size_t value_len = value_end - value;
        
        // Copy based on key
        if (strncmp(key, "id", key_len) == 0) {
            if (value_len >= MAX_PLUGIN_NAME) value_len = MAX_PLUGIN_NAME - 1;
            strncpy(manifest->id, value, value_len);
            manifest->id[value_len] = '\0';
        } else if (strncmp(key, "name", key_len) == 0) {
            if (value_len >= MAX_PLUGIN_NAME) value_len = MAX_PLUGIN_NAME - 1;
            strncpy(manifest->name, value, value_len);
            manifest->name[value_len] = '\0';
        } else if (strncmp(key, "version", key_len) == 0) {
            if (value_len >= 32) value_len = 31;
            strncpy(manifest->version, value, value_len);
            manifest->version[value_len] = '\0';
        } else if (strncmp(key, "author", key_len) == 0) {
            if (value_len >= 128) value_len = 127;
            strncpy(manifest->author, value, value_len);
            manifest->author[value_len] = '\0';
        } else if (strncmp(key, "description", key_len) == 0) {
            if (value_len >= 512) value_len = 511;
            strncpy(manifest->description, value, value_len);
            manifest->description[value_len] = '\0';
        } else if (strncmp(key, "entrypoint", key_len) == 0) {
            if (value_len >= 128) value_len = 127;
            strncpy(manifest->entrypoint, value, value_len);
            manifest->entrypoint[value_len] = '\0';
        }
        
        json = value_end + 1;
    }
    
    // Set defaults if not found
    if (manifest->entrypoint[0] == '\0') {
        strncpy(manifest->entrypoint, "PluginInit", MAX_PLUGIN_NAME - 1);
    }
    
    return manifest->id[0] != '\0';
}

bool plugin_get_manifest(const char *plugin_dir, PluginManifest *manifest) {
    return plugin_load_manifest(plugin_dir, manifest);
}

// ============================================================================
// Hooks System
// ============================================================================

bool plugin_register_hook(PluginHookType type, PluginHookFunc callback) {
    if (type < 0 || type >= HOOK_COUNT || !callback) {
        return false;
    }
    
    if (g_hooks[type].callback_count >= MAX_HOOK_CALLBACKS) {
        log_error("Maximum hook callbacks reached for hook type %d", type);
        return false;
    }
    
    g_hooks[type].callbacks[g_hooks[type].callback_count++] = callback;
    
    return true;
}

bool plugin_unregister_hook(PluginHookType type, PluginHookFunc callback) {
    if (type < 0 || type >= HOOK_COUNT || !callback) {
        return false;
    }
    
    for (int i = 0; i < g_hooks[type].callback_count; i++) {
        if (g_hooks[type].callbacks[i] == callback) {
            // Shift remaining callbacks
            for (int j = i; j < g_hooks[type].callback_count - 1; j++) {
                g_hooks[type].callbacks[j] = g_hooks[type].callbacks[j + 1];
            }
            g_hooks[type].callback_count--;
            return true;
        }
    }
    
    return false;
}

void plugin_call_hooks(PluginHookType type, void *param) {
    if (type < 0 || type >= HOOK_COUNT) {
        return;
    }
    
    for (int i = 0; i < g_hooks[type].callback_count; i++) {
        g_hooks[type].callbacks[i]((int)type, param);
    }
}

const char* plugin_hook_to_string(PluginHookType type) {
    switch (type) {
        case HOOK_FILE_OPEN: return "FILE_OPEN";
        case HOOK_FILE_SAVE: return "FILE_SAVE";
        case HOOK_FILE_CLOSE: return "FILE_CLOSE";
        case HOOK_EDITOR_INIT: return "EDITOR_INIT";
        case HOOK_EDITOR_KEYDOWN: return "EDITOR_KEYDOWN";
        case HOOK_EDITOR_CHAR: return "EDITOR_CHAR";
        case HOOK_MENU_BUILD: return "MENU_BUILD";
        case HOOK_APP_INIT: return "APP_INIT";
        case HOOK_APP_EXIT: return "APP_EXIT";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Plugin Discovery
// ============================================================================

int plugin_discover(char **plugin_ids, int max_count) {
    if (!plugin_ids || max_count <= 0) {
        return 0;
    }
    
    int count = 0;
    
    if (!plugin_ensure_dir()) {
        return 0;
    }
    
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*", g_plugins_dir);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }
        
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char plugin_path[MAX_PATH];
            snprintf(plugin_path, MAX_PATH, "%s\\%s", g_plugins_dir, fd.cFileName);
            
            PluginManifest manifest;
            if (plugin_load_manifest(plugin_path, &manifest)) {
                if (count < max_count) {
                    strncpy(plugin_ids[count], manifest.id, MAX_PLUGIN_NAME - 1);
                    plugin_ids[count][MAX_PLUGIN_NAME - 1] = '\0';
                    count++;
                }
            }
        }
        
    } while (FindNextFileA(hFind, &fd));
    
    FindClose(hFind);
    
    return count;
}

bool plugin_call_entrypoint(Plugin *plugin) {
    if (!plugin || !plugin->module) {
        return false;
    }
    
    PluginInitFunc entrypoint = (PluginInitFunc)GetProcAddress(
        plugin->module, plugin->manifest.entrypoint);
    
    if (!entrypoint) {
        return false;
    }
    
    return entrypoint();
}