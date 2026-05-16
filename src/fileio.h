#ifndef FILEIO_H
#define FILEIO_H

#include "types.h"

// Load file into buffer
// Returns true on success, false on failure
// Shows error message box on failure
bool file_load(HWND hwnd, const char *filename, GapBuffer *buffer);

// Save buffer to file with specified encoding and line ending
// Returns true on success, false on failure
// Shows error message box on failure
bool file_save_with_options(HWND hwnd, const char *filename, GapBuffer *buffer, 
                           EncodingType encoding, LineEndingType line_ending);

// Save buffer to file (using UTF-8 with BOM and CRLF)
// Returns true on success, false on failure
// Shows error message box on failure
bool file_save(HWND hwnd, const char *filename, GapBuffer *buffer);

// Show open file dialog
// Returns true if file was selected and loaded
bool file_open_dialog(HWND hwnd, char *out_filename, int out_size, GapBuffer *buffer);

// Show save file dialog
// Returns true if file was selected and saved
bool file_save_dialog(HWND hwnd, char *out_filename, int out_size, GapBuffer *buffer);

// Detect encoding from file content
EncodingType file_detect_encoding(const char *data, int length);

// Convert buffer content to specified encoding
char* file_convert_encoding(const char *input, int input_len, int *output_len, 
                           EncodingType from_enc, EncodingType to_enc);

// Normalize line endings to specified type
// Returns newly allocated string (caller must free)
char *file_normalize_line_endings_ex(const char *text, int length, LineEndingType target_ending);

// Detect if file has BOM
bool file_has_bom(const char *data, int length);

// Auto-save functions
void file_get_auto_save_path(App *app, const char *filename, char *out_path, int out_size);
bool file_auto_save(App *app, const char *filename, GapBuffer *buffer);
bool file_restore_auto_save(const char *filename, GapBuffer *buffer);

#endif // FILEIO_H
