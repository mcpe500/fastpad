#ifndef GLOBALSEARCH_H
#define GLOBALSEARCH_H

#include <windows.h>
#include <stdbool.h>

// Search result entry
typedef struct {
    char filepath[MAX_PATH];
    char filename[MAX_PATH];
    int line_number;
    int column;
    char preview[256];
    int match_start;
    int match_end;
} SearchResult;

// Search results list
typedef struct {
    SearchResult *results;
    int count;
    int capacity;
} SearchResults;

// Search state
typedef struct {
    HWND hwnd;
    HWND parent_hwnd;
    HWND list_view;
    HWND search_edit;
    HWND replace_edit;
    HWND progress_bar;
    HFONT font;
    bool searching;
    bool cancelled;
    HANDLE search_thread;
    SearchResults results;
    char search_text[MAX_PATH];
    char replace_text[MAX_PATH];
    char search_path[MAX_PATH];
    bool case_sensitive;
    bool whole_word;
    bool regex;
    bool include_hidden;
} GlobalSearch;

// Initialize global search
bool globalsearch_init(GlobalSearch *search, HWND parent);

// Free global search resources
void globalsearch_free(GlobalSearch *search);

// Show search dialog
void globalsearch_show(GlobalSearch *search, const char *initial_path);

// Hide search dialog
void globalsearch_hide(GlobalSearch *search);

// Is search dialog visible
bool globalsearch_is_visible(const GlobalSearch *search);

// Start search operation
void globalsearch_start(GlobalSearch *search, const char *path, const char *query, bool replace_mode);

// Cancel ongoing search
void globalsearch_cancel(GlobalSearch *search);

// Clear results
void globalsearch_clear_results(GlobalSearch *search);

// Get result count
int globalsearch_get_result_count(const GlobalSearch *search);

// Get result by index
const SearchResult* globalsearch_get_result(const GlobalSearch *search, int index);

// Replace single result
bool globalsearch_replace_one(GlobalSearch *search, int result_index, const char *replace_with);

// Replace all results
int globalsearch_replace_all(GlobalSearch *search, const char *replace_with);

// Handle WM_SIZE
void globalsearch_on_size(GlobalSearch *search, int width, int height);

// Handle WM_COMMAND from search controls
bool globalsearch_on_command(GlobalSearch *search, WPARAM wParam, LPARAM lParam);

// Handle list view notifications
bool globalsearch_on_notify(GlobalSearch *search, LPARAM lParam);

// Handle dialog messages
INT_PTR globalsearch_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Jump to result (open file at location)
void globalsearch_jump_to_result(const SearchResult *result, void *user_data);

// Search callback for opening file at location
typedef void (*SearchJumpCallback)(const char *filepath, int line, int col, void *user_data);

// Set jump callback
void globalsearch_set_jump_callback(GlobalSearch *search, SearchJumpCallback callback, void *user_data);

#endif // GLOBALSEARCH_H
