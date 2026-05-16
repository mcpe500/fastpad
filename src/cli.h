/*
 * FastPad CLI Support Module
 * 
 * Provides command-line interface support for portable mode and CLI operations.
 */

#ifndef CLI_H
#define CLI_H

#include "core_types.h"
#include <stdbool.h>

// CLI argument structure
typedef struct {
    const char *program_name;
    const char **files;        // Array of file paths to open
    int file_count;
    bool portable_mode;        // Run in portable mode (no %APPDATA% writes)
    const char *settings_file;// Custom settings file path
    bool no_splash;           // Skip splash screen
    bool new_instance;        // Force new instance
    bool help_requested;      // User requested help
    bool version_requested;   // User requested version
} CLIArgs;

// ============================================================================
// CLI Parsing
// ============================================================================

// Parse command line arguments
// Returns false if parsing failed (e.g., unknown option)
bool cli_parse_args(int argc, char **argv, CLIArgs *out_args);

// Free CLI arguments
void cli_free_args(CLIArgs *args);

// Get the executable directory (for portable mode)
const char* cli_get_exe_dir(void);

// Get the portable data directory
// Returns NULL if not in portable mode
const char* cli_get_portable_dir(void);

// Check if running in portable mode
bool cli_is_portable_mode(const CLIArgs *args);

// ============================================================================
// Portable Mode
// ============================================================================

// Initialize portable mode if enabled
// Returns true if portable mode is active
bool cli_init_portable_mode(const CLIArgs *args);

// Get the portable settings path
// Returns path to settings.json in portable directory, or NULL
const char* cli_get_portable_settings_path(void);

// Get the portable backups directory path
const char* cli_get_portable_backups_path(void);

// Get the portable plugins directory path
const char* cli_get_portable_plugins_path(void);

// Get the portable autosave directory path
const char* cli_get_portable_autosave_path(void);

// ============================================================================
// CLI Help/Version Output
// ============================================================================

// Print help text to stdout
void cli_print_help(const char *program_name);

// Print version info to stdout
void cli_print_version(void);

// ============================================================================
// CLI Integration with App
// ============================================================================

// Process CLI args and load files into app
// Returns number of files successfully opened
int cli_load_files_to_app(const CLIArgs *args, void *app_handle);

// Check if a single instance is required and if another instance is running
bool cli_check_single_instance(const CLIArgs *args);

#endif // CLI_H