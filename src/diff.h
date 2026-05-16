/*
 * FastPad Diff/Compare Module
 * 
 * Provides diff and compare functionality for two text buffers.
 */

#ifndef DIFF_H
#define DIFF_H

#include "core_types.h"
#include <windows.h>

// Diff operation types
typedef enum {
    DIFF_OP_EQUAL,    // Lines are the same
    DIFF_OP_INSERT,   // Line added
    DIFF_OP_DELETE,   // Line removed
    DIFF_OP_CHANGE    // Line modified
} DiffOpType;

// Result of comparing a single line
typedef struct {
    DiffOpType op;
    int line1;  // Line number in first text (-1 for insert)
    int line2;  // Line number in second text (-1 for delete)
    char *text; // The line text content
} DiffLine;

// Output structure containing diff results
typedef struct {
    DiffLine *lines;
    int count;
    int capacity;
} DiffOutput;

// Maximum diff output lines
#define DIFF_MAX_LINES 100000

// ============================================================================
// Diff Functions
// ============================================================================

// Compare two text buffers and return diff output
// Caller must free the returned DiffOutput with diff_free()
DiffOutput* diff_buffers(const char *text1, int len1, const char *text2, int len2);

// Compare two files and return diff output
// Returns NULL on error; caller must free with diff_free()
DiffOutput* diff_files(const char *filepath1, const char *filepath2);

// Compute line-based diff between two texts
void diff_compute(const char *text1, int len1, const char *text2, int len2, DiffOutput *output);

// Free diff output
void diff_free(DiffOutput *output);

// Get diff statistics
int diff_get_change_count(const DiffOutput *output);
int diff_get_insert_count(const DiffOutput *output);
int diff_get_delete_count(const DiffOutput *output);

// Check if diff output indicates identical content
bool diff_is_identical(const DiffOutput *output);

// ============================================================================
// Diff Viewer Window
// ============================================================================

// Show diff viewer window comparing two buffers
bool diff_show_viewer(HWND parent, const char *text1, const char *text2, 
                      const char *label1, const char *label2);

// Show diff viewer comparing current tab with file on disk
bool diff_compare_with_disk(void *app, int tab_index);

// Show diff viewer comparing two tabs
bool diff_compare_tabs(void *app, int tab1_index, int tab2_index);

// Diff viewer window procedure
LRESULT CALLBACK DiffViewerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Create diff viewer window
HWND diff_viewer_create(HWND parent, DiffOutput *output, const char *label1, const char *label2);

#endif // DIFF_H