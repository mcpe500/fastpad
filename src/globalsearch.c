/*
 * FastPad Global Search
 * Search across all files in workspace
 */

#include "globalsearch.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <process.h>

// ============================================================================
// Search Thread
// ============================================================================

static unsigned int __stdcall search_thread_func(void *param) {
    GlobalSearch *search = (GlobalSearch *)param;
    if (!search) return 0;
    
    const char *search_path = search->search_path;
    const char *query = search->search_text;
    
    if (!search_path || !query) {
        search->searching = false;
        return 0;
    }
    
    // Allocate results buffer
    search->results.capacity = 1000;
    search->results.count = 0;
    search->results.results = (SearchResult*)malloc(sizeof(SearchResult) * search->results.capacity);
    
    if (!search->results.results) {
        search->searching = false;
        return 0;
    }
    
    memset(search->results.results, 0, sizeof(SearchResult) * search->results.capacity);
    
    // Search recursively
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, MAX_PATH, "%s\\*", search_path);
    
    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(search_pattern, &find_data);
    
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (search->cancelled) break;
            
            const char *name = find_data.cFileName;
            
            // Skip . and ..
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
            
            // Build full path
            char full_path[MAX_PATH];
            snprintf(full_path, MAX_PATH, "%s\\%s", search_path, name);
            
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // Recurse into subdirectory
                if (!search->include_hidden && (name[0] == '.')) continue;
                
                strncpy(search->search_path, full_path, MAX_PATH - 1);
                search_thread_func(param); // Recursive call
            } else {
                // Search in file
                FILE *f = fopen(full_path, "r");
                if (!f) continue;
                
                char line[1024];
                int line_num = 0;
                
                while (fgets(line, sizeof(line), f) && !search->cancelled) {
                    line_num++;
                    
                    // Simple search - check if query appears in line
                    const char *found = strstr(line, query);
                    if (found) {
                        // Check whole word match if needed
                        if (search->whole_word) {
                            bool is_word = false;
                            const char *start = found - 1;
                            const char *end = found + strlen(query);
                            
                            if (start < line || *start == ' ' || *start == '\t' || *start == '(' || *start == ')' || *start == '{' || *start == '}' || *start == '[' || *start == ']' || *start == ',' || *start == ';' || *start == ':') {
                                if (*end == '\0' || *end == ' ' || *end == '\t' || *end == '(' || *end == ')' || *end == '{' || *end == '}' || *end == '[' || *end == ']' || *end == ',' || *end == ';' || *end == ':') {
                                    is_word = true;
                                }
                            }
                            if (!is_word) continue;
                        }
                        
                        // Grow buffer if needed
                        if (search->results.count >= search->results.capacity) {
                            search->results.capacity += 500;
                            SearchResult *new_results = (SearchResult*)realloc(
                                search->results.results,
                                sizeof(SearchResult) * search->results.capacity
                            );
                            if (new_results) {
                                search->results.results = new_results;
                            } else {
                                break;
                            }
                        }
                        
                        // Add result
                        SearchResult *r = &search->results.results[search->results.count];
                        strncpy(r->filepath, full_path, MAX_PATH - 1);
                        strncpy(r->filename, name, MAX_PATH - 1);
                        r->line_number = line_num;
                        r->column = (int)(found - line);
                        
                        // Create preview (up to 100 chars around match)
                        int preview_len = (int)strlen(line);
                        if (preview_len > 100) {
                            int match_pos = (int)(found - line);
                            int start = match_pos > 30 ? match_pos - 30 : 0;
                            int len = preview_len - start;
                            if (len > 100) len = 100;
                            
                            // Trim preview
                            if (start > 0) {
                                strncpy(r->preview, "...", 3);
                                strncpy(r->preview + 3, line + start, len - 3);
                                r->preview[len] = '\0';
                            } else {
                                strncpy(r->preview, line, 100);
                                r->preview[100] = '\0';
                            }
                        } else {
                            strncpy(r->preview, line, 255);
                            r->preview[255] = '\0';
                        }
                        
                        // Trim newlines
                        char *p = r->preview;
                        while (*p) {
                            if (*p == '\r' || *p == '\n') *p = ' ';
                            p++;
                        }
                        
                        r->match_start = r->column;
                        r->match_end = r->column + (int)strlen(query);
                        
                        search->results.count++;
                    }
                }
                
                fclose(f);
            }
            
        } while (FindNextFileA(find, &find_data) && !search->cancelled);
        
        FindClose(find);
    }
    
    search->searching = false;
    
    // Update UI (post message to main thread)
    // Using WM_USER for search complete notification
    PostMessage(search->parent_hwnd, WM_USER + 1, 0, 0);
    
    return 0;
}

// ============================================================================
// Global Search Implementation
// ============================================================================

bool globalsearch_init(GlobalSearch *search, HWND parent) {
    if (!search) return false;
    
    memset(search, 0, sizeof(GlobalSearch));
    search->parent_hwnd = parent;
    search->case_sensitive = false;
    search->whole_word = false;
    search->regex = false;
    search->include_hidden = false;
    
    return true;
}

void globalsearch_free(GlobalSearch *search) {
    if (!search) return;
    
    if (search->results.results) {
        free(search->results.results);
    }
    
    if (search->search_thread) {
        CloseHandle(search->search_thread);
    }
}

void globalsearch_show(GlobalSearch *search, const char *initial_path) {
    if (!search) return;
    
    search->parent_hwnd = GetActiveWindow();
    
    if (initial_path) {
        strncpy(search->search_path, initial_path, MAX_PATH - 1);
    }
}

void globalsearch_hide(GlobalSearch *search) {
    if (!search) return;
    globalsearch_cancel(search);
}

bool globalsearch_is_visible(const GlobalSearch *search) {
    return search && search->hwnd && IsWindowVisible(search->hwnd);
}

void globalsearch_start(GlobalSearch *search, const char *path, const char *query, bool replace_mode) {
    if (!search || !path || !query) return;
    
    // Cancel any ongoing search
    globalsearch_cancel(search);
    
    // Setup search
    strncpy(search->search_path, path, MAX_PATH - 1);
    strncpy(search->search_text, query, MAX_PATH - 1);
    search->search_text[MAX_PATH - 1] = '\0';
    search->cancelled = false;
    search->searching = true;
    (void)replace_mode;
    
    // Clear previous results
    if (search->results.results) {
        free(search->results.results);
    }
    search->results.results = NULL;
    search->results.count = 0;
    search->results.capacity = 0;
    
    // Start search thread
    search->search_thread = (HANDLE)_beginthreadex(
        NULL, 0, search_thread_func, search, 0, NULL
    );
    
    log_info("Global search started: \"%s\" in %s", query, path);
}

void globalsearch_cancel(GlobalSearch *search) {
    if (!search) return;
    
    search->cancelled = true;
    
    if (search->search_thread) {
        WaitForSingleObject(search->search_thread, 1000);
        CloseHandle(search->search_thread);
        search->search_thread = NULL;
    }
    
    search->searching = false;
}

void globalsearch_clear_results(GlobalSearch *search) {
    if (!search) return;
    
    if (search->results.results) {
        free(search->results.results);
    }
    search->results.results = NULL;
    search->results.count = 0;
    search->results.capacity = 0;
}

int globalsearch_get_result_count(const GlobalSearch *search) {
    return search ? search->results.count : 0;
}

const SearchResult* globalsearch_get_result(const GlobalSearch *search, int index) {
    if (!search || index < 0 || index >= search->results.count) return NULL;
    return &search->results.results[index];
}

bool globalsearch_replace_one(GlobalSearch *search, int result_index, const char *replace_with) {
    if (!search || result_index < 0 || result_index >= search->results.count) return false;
    
    SearchResult *r = &search->results.results[result_index];
    
    // Read file
    FILE *f = fopen(r->filepath, "r");
    if (!f) return false;
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        return false;
    }
    
    fread(content, 1, size, f);
    fclose(f);
    content[size] = '\0';
    
    // Find and replace on specific line
    char *line_start = content;
    int current_line = 1;
    
    while (*line_start && current_line < r->line_number) {
        line_start = strchr(line_start, '\n');
        if (!line_start) break;
        line_start++;
        current_line++;
    }
    
    if (current_line != r->line_number) {
        free(content);
        return false;
    }
    
    // Find end of line
    char *line_end = strchr(line_start, '\n');
    if (line_end) *line_end = '\0';
    
    // Check if query still exists at column
    if (strncmp(line_start + r->column, r->preview + r->match_start, 
                r->match_end - r->match_start) != 0) {
        free(content);
        return false;
    }
    
    // Perform replacement
    char *new_line = (char*)malloc(strlen(line_start) + strlen(replace_with) + 1);
    if (!new_line) {
        free(content);
        return false;
    }
    
    strncpy(new_line, line_start, r->column);
    new_line[r->column] = '\0';
    strcat(new_line, replace_with);
    strcat(new_line, line_start + r->match_end);
    
    // Rebuild file content
    *line_start = '\0';
    if (line_end) *line_end = '\n';
    
    FILE *out = fopen(r->filepath, "w");
    if (!out) {
        free(new_line);
        free(content);
        return false;
    }
    
    fprintf(out, "%s%s%s", content, new_line, line_end ? line_end + 1 : "");
    fclose(out);
    
    free(new_line);
    free(content);
    
    log_info("Replaced in %s:%d", r->filepath, r->line_number);
    return true;
}

int globalsearch_replace_all(GlobalSearch *search, const char *replace_with) {
    if (!search || !replace_with) return 0;
    
    int replaced = 0;
    for (int i = 0; i < search->results.count; i++) {
        if (globalsearch_replace_one(search, i, replace_with)) {
            replaced++;
        }
    }
    
    log_info("Replaced %d occurrences", replaced);
    return replaced;
}

void globalsearch_on_size(GlobalSearch *search, int width, int height) {
    (void)search;
    (void)width;
    (void)height;
}

bool globalsearch_on_command(GlobalSearch *search, WPARAM wParam, LPARAM lParam) {
    (void)search;
    (void)wParam;
    (void)lParam;
    return false;
}

bool globalsearch_on_notify(GlobalSearch *search, LPARAM lParam) {
    (void)search;
    (void)lParam;
    return false;
}

INT_PTR CALLBACK globalsearch_dlg_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)hwnd;
    (void)msg;
    (void)wParam;
    (void)lParam;
    return FALSE;
}

void globalsearch_jump_to_result(const SearchResult *result, void *user_data) {
    (void)result;
    (void)user_data;
    // This would send a message to open the file at the specific location
    // Implementation depends on app integration
}

void globalsearch_set_jump_callback(GlobalSearch *search, SearchJumpCallback callback, void *user_data) {
    if (!search) return;
    (void)callback;
    (void)user_data;
}