/*
 * FastPad CLI Support Module Implementation
 * 
 * Provides command-line interface support for portable mode and CLI operations.
 */

#include "cli.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

// Portable mode directory (set at startup)
static char g_portable_dir[512] = {0};
static bool g_portable_mode = false;

// ============================================================================
// CLI Parsing
// ============================================================================

bool cli_parse_args(int argc, char **argv, CLIArgs *out_args) {
    if (!out_args) {
        return false;
    }
    
    memset(out_args, 0, sizeof(CLIArgs));
    
    if (argc <= 0 || !argv) {
        return true;  // No args is valid
    }
    
    out_args->program_name = argv[0];
    
    // Allocate array for files (max 32)
    out_args->files = (const char**)malloc(sizeof(char*) * 32);
    if (!out_args->files) {
        return false;
    }
    out_args->file_count = 0;
    
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        
        // Check for flags
        if (arg[0] == '-' || arg[0] == '/') {
            if (strcmp(arg + 1, "h") == 0 || strcmp(arg + 1, "help") == 0 || 
                strcmp(arg + 1, "?") == 0) {
                out_args->help_requested = true;
            } else if (strcmp(arg + 1, "v") == 0 || strcmp(arg + 1, "version") == 0) {
                out_args->version_requested = true;
            } else if (strcmp(arg + 1, "p") == 0 || strcmp(arg + 1, "portable") == 0) {
                out_args->portable_mode = true;
            } else if (strcmp(arg + 1, "n") == 0 || strcmp(arg + 1, "new-instance") == 0) {
                out_args->new_instance = true;
            } else if (strcmp(arg + 1, "s") == 0 || strcmp(arg + 1, "settings") == 0) {
                if (i + 1 < argc) {
                    out_args->settings_file = argv[++i];
                }
            } else if (strcmp(arg + 1, "ns") == 0 || strcmp(arg + 1, "no-splash") == 0) {
                out_args->no_splash = true;
            } else {
                // Unknown option
                fprintf(stderr, "Unknown option: %s\n", arg);
                free((void*)out_args->files);
                memset(out_args, 0, sizeof(CLIArgs));
                return false;
            }
        } else {
            // File path
            if (out_args->file_count < 32) {
                out_args->files[out_args->file_count++] = arg;
            }
        }
    }
    
    return true;
}

void cli_free_args(CLIArgs *args) {
    if (!args) return;
    
    if (args->files) {
        free((void*)args->files);
        args->files = NULL;
    }
    
    memset(args, 0, sizeof(CLIArgs));
}

// ============================================================================
// Portable Mode
// ============================================================================

bool cli_init_portable_mode(const CLIArgs *args) {
    if (!args) {
        return false;
    }
    
    if (!args->portable_mode) {
        g_portable_mode = false;
        return false;
    }
    
    // Get executable directory
    const char *exe_dir = cli_get_exe_dir();
    if (!exe_dir) {
        return false;
    }
    
    // Check for "data" subdirectory to confirm portable mode
    snprintf(g_portable_dir, sizeof(g_portable_dir), "%s" PATH_SEP "data", exe_dir);
    
    // Create data directory if it doesn't exist
#ifdef _WIN32
    CreateDirectoryA(g_portable_dir, NULL);
#else
    mkdir(g_portable_dir, 0755);
#endif
    
    g_portable_mode = true;
    
    log_info("Portable mode enabled: %s", g_portable_dir);
    
    return true;
}

bool cli_is_portable_mode(const CLIArgs *args) {
    (void)args;
    return g_portable_mode;
}

const char* cli_get_portable_dir(void) {
    if (!g_portable_mode) {
        return NULL;
    }
    return g_portable_dir;
}

const char* cli_get_portable_settings_path(void) {
    if (!g_portable_mode) {
        return NULL;
    }
    
    static char path[512];
    snprintf(path, sizeof(path), "%s" PATH_SEP "settings.json", g_portable_dir);
    return path;
}

const char* cli_get_portable_backups_path(void) {
    if (!g_portable_mode) {
        return NULL;
    }
    
    static char path[512];
    snprintf(path, sizeof(path), "%s" PATH_SEP "backups", g_portable_dir);
    
#ifdef _WIN32
    CreateDirectoryA(path, NULL);
#else
    mkdir(path, 0755);
#endif
    
    return path;
}

const char* cli_get_portable_plugins_path(void) {
    if (!g_portable_mode) {
        return NULL;
    }
    
    static char path[512];
    snprintf(path, sizeof(path), "%s" PATH_SEP "plugins", g_portable_dir);
    
#ifdef _WIN32
    CreateDirectoryA(path, NULL);
#else
    mkdir(path, 0755);
#endif
    
    return path;
}

const char* cli_get_portable_autosave_path(void) {
    if (!g_portable_mode) {
        return NULL;
    }
    
    static char path[512];
    snprintf(path, sizeof(path), "%s" PATH_SEP "autosave", g_portable_dir);
    
#ifdef _WIN32
    CreateDirectoryA(path, NULL);
#else
    mkdir(path, 0755);
#endif
    
    return path;
}

// ============================================================================
// CLI Help/Version Output
// ============================================================================

void cli_print_help(const char *program_name) {
    if (!program_name) {
        program_name = "FastPad";
    }
    
    printf("FastPad - A tiny, fast, native Windows text editor\n\n");
    printf("Usage: %s [OPTIONS] [FILES...]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help         Show this help message\n");
    printf("  -v, --version      Show version information\n");
    printf("  -p, --portable     Run in portable mode (store data next to exe)\n");
    printf("  -s, --settings F   Use custom settings file F\n");
    printf("  -n, --new-instance Force a new instance (don't reuse existing)\n");
    printf("  -ns, --no-splash   Skip the splash screen\n");
    printf("\nExamples:\n");
    printf("  %s file.txt                    Open file.txt\n", program_name);
    printf("  %s -p file1.txt file2.txt     Run in portable mode\n", program_name);
    printf("  %s --portable                 Start in portable mode\n", program_name);
}

void cli_print_version(void) {
    printf("FastPad v2.0.1\n");
    printf("A tiny, fast, native Windows text editor\n");
    printf("Built with raw Win32 API and minimal dependencies\n");
}

// ============================================================================
// CLI Integration
// ============================================================================

const char* cli_get_exe_dir(void) {
    static char exe_dir[512] = {0};
    
    if (exe_dir[0] != '\0') {
        return exe_dir;
    }
    
#ifdef _WIN32
    // Get executable path
    char exe_path[512];
    if (GetModuleFileNameA(NULL, exe_path, sizeof(exe_path)) == 0) {
        return NULL;
    }
    
    // Find last backslash
    char *last_bs = strrchr(exe_path, '\\');
    if (last_bs) {
        *last_bs = '\0';
    }
    
    strncpy(exe_dir, exe_path, sizeof(exe_dir) - 1);
    exe_dir[sizeof(exe_dir) - 1] = '\0';
#else
    // On Linux, use /proc/self/exe
    char link_path[512];
    ssize_t len = readlink("/proc/self/exe", link_path, sizeof(link_path) - 1);
    if (len > 0) {
        link_path[len] = '\0';
        char *last_bs = strrchr(link_path, '/');
        if (last_bs) {
            *last_bs = '\0';
        }
        strncpy(exe_dir, link_path, sizeof(exe_dir) - 1);
        exe_dir[sizeof(exe_dir) - 1] = '\0';
    } else {
        // Fallback to current directory
        getcwd(exe_dir, sizeof(exe_dir) - 1);
    }
#endif
    
    return exe_dir;
}

int cli_load_files_to_app(const CLIArgs *args, void *app_handle) {
    (void)args;
    (void)app_handle;
    // Would integrate with the App structure to open files
    // Return number of files successfully opened
    return 0;
}

bool cli_check_single_instance(const CLIArgs *args) {
    (void)args;
    // Would check for existing FastPad instance using named mutex or window enumeration
    // Return true if this should be a new instance
    return true;
}