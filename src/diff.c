/*
 * FastPad Diff/Compare Module Implementation
 * 
 * Provides diff and compare functionality for two text buffers.
 */

#include "diff.h"
#include "buffer.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// strdup is not standard C, provide our own
static char* fastpad_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *d = (char*)malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

// LCS (Longest Common Subsequence) based diff algorithm
// Using a simplified Myer algorithm for line-by-line comparison

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Forward declarations
static void diff_lcs(const char **lines1, int count1, const char **lines2, int count2,
                     int *lcs_matrix);
static void diff_backtrack(const char **lines1, const char **lines2,
                           int *lcs_matrix, int count1, int count2,
                           DiffOutput *output);

// Split text into lines
static char** split_lines(const char *text, int len, int *out_count) {
    if (!text || len <= 0) {
        *out_count = 0;
        return NULL;
    }
    
    // Count lines
    int line_count = 1;
    for (int i = 0; i < len; i++) {
        if (text[i] == '\n') line_count++;
    }
    
    char **lines = (char**)malloc(sizeof(char*) * line_count);
    if (!lines) {
        *out_count = 0;
        return NULL;
    }
    
    // Split text into lines
    int line_idx = 0;
    int line_start = 0;
    
    for (int i = 0; i <= len; i++) {
        if (i == len || text[i] == '\n') {
            int line_len = i - line_start;
            // Remove trailing \r if present
            if (line_len > 0 && text[i - 1] == '\r') {
                line_len--;
            }
            
            lines[line_idx] = (char*)malloc(line_len + 1);
            if (lines[line_idx]) {
                memcpy(lines[line_idx], text + line_start, line_len);
                lines[line_idx][line_len] = '\0';
            }
            line_start = i + 1;
            line_idx++;
        }
    }
    
    *out_count = line_idx;
    return lines;
}

// Free split lines
static void free_lines(char **lines, int count) {
    if (!lines) return;
    for (int i = 0; i < count; i++) {
        if (lines[i]) free(lines[i]);
    }
    free(lines);
}

// Calculate LCS matrix for two sequences
static void diff_lcs(const char **lines1, int count1, const char **lines2, int count2,
                     int *lcs_matrix) {
    // Initialize matrix
    for (int i = 0; i <= count1; i++) {
        for (int j = 0; j <= count2; j++) {
            if (i == 0 || j == 0) {
                lcs_matrix[i * (count2 + 1) + j] = 0;
            } else if (strcmp(lines1[i - 1], lines2[j - 1]) == 0) {
                lcs_matrix[i * (count2 + 1) + j] = lcs_matrix[(i - 1) * (count2 + 1) + (j - 1)] + 1;
            } else {
                int up = lcs_matrix[(i - 1) * (count2 + 1) + j];
                int left = lcs_matrix[i * (count2 + 1) + (j - 1)];
                lcs_matrix[i * (count2 + 1) + j] = MAX(up, left);
            }
        }
    }
}

// Backtrack through LCS matrix to produce diff output
static void diff_backtrack(const char **lines1, const char **lines2,
                           int *lcs_matrix, int count1, int count2,
                           DiffOutput *output) {
    output->count = 0;
    output->identical = 1;
    
    int i = count1;
    int j = count2;
    
    // First pass: count operations to allocate
    int op_count = 0;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && strcmp(lines1[i - 1], lines2[j - 1]) == 0) {
            op_count++;
            i--;
            j--;
        } else if (j > 0 && (i == 0 || lcs_matrix[i * (count2 + 1) + (j - 1)] >= lcs_matrix[(i - 1) * (count2 + 1) + j])) {
            op_count++;
            j--;
        } else if (i > 0) {
            op_count++;
            i--;
        }
    }
    
    // Allocate results (reverse order)
    output->results = (DiffResult*)malloc(sizeof(DiffResult) * op_count);
    if (!output->results) return;
    output->capacity = op_count;
    
    // Reset and backtrack again to fill results
    i = count1;
    j = count2;
    int result_idx = op_count - 1;
    
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && strcmp(lines1[i - 1], lines2[j - 1]) == 0) {
            DiffResult *r = &output->results[result_idx];
            r->op = DIFF_OP_EQUAL;
            r->line1 = i - 1;
            r->line2 = j - 1;
            r->text = NULL;
            i--;
            j--;
            result_idx--;
        } else if (j > 0 && (i == 0 || lcs_matrix[i * (count2 + 1) + (j - 1)] >= lcs_matrix[(i - 1) * (count2 + 1) + j])) {
            DiffResult *r = &output->results[result_idx];
            r->op = DIFF_OP_INSERT;
            r->line1 = -1;
            r->line2 = j - 1;
            r->text = lines2[j - 1] ? fastpad_strdup(lines2[j - 1]) : NULL;
            output->identical = 0;
            j--;
            result_idx--;
        } else if (i > 0) {
            DiffResult *r = &output->results[result_idx];
            r->op = DIFF_OP_DELETE;
            r->line1 = i - 1;
            r->line2 = -1;
            r->text = lines1[i - 1] ? fastpad_strdup(lines1[i - 1]) : NULL;
            output->identical = 0;
            i--;
            result_idx--;
        }
    }
    
    output->count = op_count;
}

// Compute diff between two texts
void diff_compute(const char *text1, int len1, const char *text2, int len2,
                  DiffOutput *output) {
    memset(output, 0, sizeof(DiffOutput));
    
    // Split into lines
    int count1 = 0, count2 = 0;
    char **lines1 = split_lines(text1, len1, &count1);
    char **lines2 = split_lines(text2, len2, &count2);
    
    if (!lines1 && !lines2) {
        output->identical = 1;
        return;
    }
    
    // Handle edge cases
    if (count1 == 0 && count2 == 0) {
        output->identical = 1;
        return;
    }
    
    if (count1 == 0) {
        output->results = (DiffResult*)malloc(sizeof(DiffResult) * count2);
        if (output->results) {
            output->capacity = count2;
            for (int i = 0; i < count2; i++) {
                output->results[i].op = DIFF_OP_INSERT;
                output->results[i].line1 = -1;
                output->results[i].line2 = i;
                output->results[i].text = lines2[i] ? fastpad_strdup(lines2[i]) : NULL;
            }
            output->count = count2;
        }
        output->identical = 0;
        free_lines(lines2, count2);
        return;
    }
    
    if (count2 == 0) {
        output->results = (DiffResult*)malloc(sizeof(DiffResult) * count1);
        if (output->results) {
            output->capacity = count1;
            for (int i = 0; i < count1; i++) {
                output->results[i].op = DIFF_OP_DELETE;
                output->results[i].line1 = i;
                output->results[i].line2 = -1;
                output->results[i].text = lines1[i] ? fastpad_strdup(lines1[i]) : NULL;
            }
            output->count = count1;
        }
        output->identical = 0;
        free_lines(lines1, count1);
        return;
    }
    
    // Allocate LCS matrix
    int *lcs_matrix = (int*)malloc(sizeof(int) * (count1 + 1) * (count2 + 1));
    if (!lcs_matrix) {
        free_lines(lines1, count1);
        free_lines(lines2, count2);
        return;
    }
    
    // Compute LCS
    diff_lcs((const char**)lines1, count1, (const char**)lines2, count2, lcs_matrix);
    
    // Backtrack to produce diff
    diff_backtrack((const char**)lines1, (const char**)lines2, lcs_matrix, count1, count2, output);
    
    // Cleanup
    free(lcs_matrix);
    free_lines(lines1, count1);
    free_lines(lines2, count2);
}

// Compare two text buffers
DiffOutput* diff_buffers(const char *text1, int len1, const char *text2, int len2) {
    DiffOutput *output = (DiffOutput*)malloc(sizeof(DiffOutput));
    if (!output) return NULL;
    
    diff_compute(text1, len1, text2, len2, output);
    
    return output;
}

// Compare two files
DiffOutput* diff_files(const char *filepath1, const char *filepath2) {
    DiffOutput *output = (DiffOutput*)malloc(sizeof(DiffOutput));
    if (!output) return NULL;
    memset(output, 0, sizeof(DiffOutput));
    
    // Read first file
    FILE *f1 = fopen(filepath1, "rb");
    if (!f1) {
        free(output);
        return NULL;
    }
    
    fseek(f1, 0, SEEK_END);
    int len1 = ftell(f1);
    fseek(f1, 0, SEEK_SET);
    
    char *text1 = (char*)malloc(len1 + 1);
    if (!text1) {
        fclose(f1);
        free(output);
        return NULL;
    }
    fread(text1, 1, len1, f1);
    text1[len1] = '\0';
    fclose(f1);
    
    // Read second file
    FILE *f2 = fopen(filepath2, "rb");
    if (!f2) {
        free(text1);
        free(output);
        return NULL;
    }
    
    fseek(f2, 0, SEEK_END);
    int len2 = ftell(f2);
    fseek(f2, 0, SEEK_SET);
    
    char *text2 = (char*)malloc(len2 + 1);
    if (!text2) {
        free(text1);
        fclose(f2);
        free(output);
        return NULL;
    }
    fread(text2, 1, len2, f2);
    text2[len2] = '\0';
    fclose(f2);
    
    // Compute diff
    diff_compute(text1, len1, text2, len2, output);
    
    free(text1);
    free(text2);
    
    return output;
}

// Free diff output
void diff_free(DiffOutput *output) {
    if (!output) return;
    if (output->results) {
        for (int i = 0; i < output->count; i++) {
            if (output->results[i].text) {
                free(output->results[i].text);
            }
        }
        free(output->results);
    }
    memset(output, 0, sizeof(DiffOutput));
}

// Get diff statistics
int diff_get_change_count(const DiffOutput *output) {
    if (!output) return 0;
    int count = 0;
    for (int i = 0; i < output->count; i++) {
        if (output->results[i].op != DIFF_OP_EQUAL) {
            count++;
        }
    }
    return count;
}

int diff_get_insert_count(const DiffOutput *output) {
    if (!output) return 0;
    int count = 0;
    for (int i = 0; i < output->count; i++) {
        if (output->results[i].op == DIFF_OP_INSERT) {
            count++;
        }
    }
    return count;
}

int diff_get_delete_count(const DiffOutput *output) {
    if (!output) return 0;
    int count = 0;
    for (int i = 0; i < output->count; i++) {
        if (output->results[i].op == DIFF_OP_DELETE) {
            count++;
        }
    }
    return count;
}

bool diff_is_identical(const DiffOutput *output) {
    if (!output) return true;
    return output->identical == 1;
}

// ============================================================================
// Diff Viewer Window (Windows-specific)
// ============================================================================

#ifdef WINDOWS

#include "app.h"

#define ID_DIFF_LIST 3001
#define ID_DIFF_CLOSE 3002
#define ID_DIFF_COPY 3003

static HWND g_diff_hwnd = NULL;
static DiffOutput *g_diff_output = NULL;

// Diff viewer window procedure
LRESULT CALLBACK DiffViewerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
            DiffOutput *output = (DiffOutput*)cs->lpCreateParams;
            g_diff_output = output;
            g_diff_hwnd = hwnd;
            
            // Create header
            CreateWindowExA(0, "STATIC", "Diff Results", 
                          WS_CHILD | WS_VISIBLE | SS_CENTER,
                          10, 10, 760, 30, hwnd, NULL, 
                          (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            
            // Create listbox for diff results
            HWND listbox = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_HASSTRINGS,
                10, 50, 760, 500, hwnd, (HMENU)ID_DIFF_LIST,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            
            // Populate listbox
            if (output && output->results) {
                for (int i = 0; i < output->count; i++) {
                    DiffResult *r = &output->results[i];
                    char line[1024];
                    
                    switch (r->op) {
                        case DIFF_OP_EQUAL:
                            snprintf(line, sizeof(line), "  %4d: %s", r->line1 + 1, r->text ? r->text : "");
                            break;
                        case DIFF_OP_INSERT:
                            snprintf(line, sizeof(line), "+ %4d: +%s", r->line2 + 1, r->text ? r->text : "");
                            break;
                        case DIFF_OP_DELETE:
                            snprintf(line, sizeof(line), "- %4d: -%s", r->line1 + 1, r->text ? r->text : "");
                            break;
                        case DIFF_OP_REPLACE:
                            snprintf(line, sizeof(line), "~ %4d<->%d: ~%s", r->line1 + 1, r->line2 + 1, r->text ? r->text : "");
                            break;
                    }
                    SendMessageA(listbox, LB_ADDSTRING, 0, (LPARAM)line);
                }
            }
            
            // Create close button
            CreateWindowExA(0, "BUTTON", "Close",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                700, 560, 80, 30, hwnd, (HMENU)ID_DIFF_CLOSE,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            
            break;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_DIFF_CLOSE) {
                DestroyWindow(hwnd);
            }
            break;
            
        case WM_DESTROY:
            g_diff_hwnd = NULL;
            g_diff_output = NULL;
            break;
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Create diff viewer window
HWND diff_viewer_create(HWND parent, DiffOutput *output, const char *label1, const char *label2) {
    (void)label1;
    (void)label2;
    
    HINSTANCE hInstance = GetModuleHandle(NULL);
    
    // Register window class
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = DiffViewerProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "FastPadDiffViewer";
    
    RegisterClassExA(&wc);
    
    // Create window
    HWND hwnd = CreateWindowExA(
        0, "FastPadDiffViewer", "FastPad - Diff Viewer",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 650,
        parent, NULL, hInstance, output
    );
    
    return hwnd;
}

// Show diff viewer comparing two buffers
bool diff_show_viewer(HWND parent, const char *text1, const char *text2,
                      const char *label1, const char *label2) {
    DiffOutput *output = diff_buffers(text1, strlen(text1), text2, strlen(text2));
    if (!output) return false;
    
    diff_viewer_create(parent, output, label1, label2);
    
    // Transfer ownership - viewer will free
    return true;
}

// Show diff viewer comparing current tab with file on disk
bool diff_compare_with_disk(App *app, int tab_index) {
    (void)app;
    (void)tab_index;
    // Implementation would load file from disk and compare with buffer
    return false;
}

// Show diff viewer comparing two tabs
bool diff_compare_tabs(App *app, int tab1_index, int tab2_index) {
    (void)app;
    (void)tab1_index;
    (void)tab2_index;
    // Implementation would compare two tab buffers
    return false;
}

#endif // WINDOWS