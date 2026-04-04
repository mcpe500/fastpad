#ifndef FILEIO_H
#define FILEIO_H

#include "types.h"

// Load file into buffer
// Returns true on success, false on failure
// Shows error message box on failure
bool file_load(HWND hwnd, const char *filename, GapBuffer *buffer);

// Save buffer to file
// Returns true on success, false on failure
// Shows error message box on failure
bool file_save(HWND hwnd, const char *filename, GapBuffer *buffer);

// Show open file dialog
// Returns true if file was selected and loaded
bool file_open_dialog(HWND hwnd, char *out_filename, int out_size, GapBuffer *buffer);

// Show save file dialog
// Returns true if file was selected and saved
bool file_save_dialog(HWND hwnd, char *out_filename, int out_size, GapBuffer *buffer);

// Normalize line endings to CRLF
// Returns newly allocated string (caller must free)
char *file_normalize_line_endings(const char *text, int length);

// Detect if file has BOM
bool file_has_bom(const char *data, int length);

#endif // FILEIO_H
