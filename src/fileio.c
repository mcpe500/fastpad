#include "fileio.h"
#include "buffer.h"
#include "app.h"
#include "errors.h"
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool file_has_bom(const char *data, int length) {
    if (length >= 3) {
        return (unsigned char)data[0] == 0xEF &&
               (unsigned char)data[1] == 0xBB &&
               (unsigned char)data[2] == 0xBF;
    }
    return false;
}

char *file_normalize_line_endings(const char *text, int length) {
    // Count how many LFs need CR added
    int cr_count = 0;
    for (int i = 0; i < length; i++) {
        if (text[i] == '\n' && (i == 0 || text[i-1] != '\r')) {
            cr_count++;
        }
    }
    
    if (cr_count == 0) {
        // Already normalized
        char *result = (char *)malloc(length + 1);
        if (result) {
            memcpy(result, text, length);
            result[length] = '\0';
        }
        return result;
    }
    
    // Allocate new buffer with space for CRs
    int new_length = length + cr_count;
    char *result = (char *)malloc(new_length + 1);
    if (!result) {
        return NULL;
    }
    
    int j = 0;
    for (int i = 0; i < length; i++) {
        if (text[i] == '\n' && (i == 0 || text[i-1] != '\r')) {
            result[j++] = '\r';
        }
        result[j++] = text[i];
    }
    result[j] = '\0';
    
    return result;
}

bool file_load(HWND hwnd, const char *filename, GapBuffer *buffer) {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    char *fileData = NULL;
    bool success = false;

    hFile = CreateFileA(
        filename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxA(hwnd, ERR_FAILED_OPEN_FILE, DLG_ERROR, MB_ICONERROR);
        goto cleanup;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        MessageBoxA(hwnd, ERR_FAILED_READ_FILE_SIZE, DLG_ERROR, MB_ICONERROR);
        goto cleanup;
    }

    // Handle empty files
    if (fileSize == 0) {
        // Properly reinitialize buffer for empty file
        buffer_free(buffer);
        if (!buffer_init(buffer, 4096)) {
            MessageBoxA(hwnd, ERR_OUT_OF_MEMORY, DLG_ERROR, MB_ICONERROR);
            goto cleanup;
        }
        success = true;
        goto cleanup;
    }

    // Large file warning (BUG #025 fix)
    if (fileSize > LARGE_FILE_THRESHOLD) {
        double size_mb = (double)fileSize / (1024.0 * 1024.0);
        char msg[512];
        snprintf(msg, sizeof(msg), MSG_LARGE_FILE, size_mb);
        int result = MessageBoxA(hwnd, msg, DLG_FASTPAD, MB_YESNO | MB_ICONWARNING);
        if (result == IDNO) {
            goto cleanup; /* User chose not to open */
        }
    }

    fileData = (char *)malloc(fileSize);
    if (!fileData) {
        MessageBoxA(hwnd, ERR_OUT_OF_MEMORY, DLG_ERROR, MB_ICONERROR);
        goto cleanup;
    }

    DWORD bytesRead;
    if (!ReadFile(hFile, fileData, fileSize, &bytesRead, NULL)) {
        MessageBoxA(hwnd, ERR_FAILED_READ_FILE, DLG_ERROR, MB_ICONERROR);
        goto cleanup;
    }

    // Skip BOM if present
    int offset = 0;
    if (file_has_bom(fileData, bytesRead)) {
        offset = 3;
    }

    // Convert CRLF to LF for internal storage
    int lf_count = 0;
    for (DWORD i = offset; i < bytesRead; i++) {
        if (fileData[i] == '\r' && i + 1 < bytesRead && fileData[i+1] == '\n') {
            lf_count++;
        }
    }

    int text_length = (int)bytesRead - (int)offset - lf_count;

    // Initialize buffer with text
    buffer_free(buffer);
    if (!buffer_init(buffer, text_length + 1024)) {
        MessageBoxA(hwnd, ERR_OUT_OF_MEMORY, DLG_ERROR, MB_ICONERROR);
        goto cleanup;
    }

    // Copy text with normalized line endings using gap buffer API
    // FIX: Use proper buffer_insert() calls to maintain gap buffer invariants
    // instead of direct memory manipulation
    TextPos insert_pos = 0;
    for (DWORD i = offset; i < bytesRead; i++) {
        if (fileData[i] == '\r' && i + 1 < bytesRead && fileData[i+1] == '\n') {
            char nl = '\n';
            buffer_insert(buffer, insert_pos, &nl, 1);
            insert_pos++;
            i++; // Skip LF
        } else {
            buffer_insert(buffer, insert_pos, &fileData[i], 1);
            insert_pos++;
        }
    }

    success = true;

cleanup:
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
    if (fileData) {
        free(fileData);
    }
    return success;
}

bool file_save(HWND hwnd, const char *filename, GapBuffer *buffer) {
    // Get full text
    char *text = buffer_get_text(buffer, 0, buffer->size);
    if (!text) {
        MessageBoxA(hwnd, ERR_FAILED_GET_TEXT, DLG_ERROR, MB_ICONERROR);
        return false;
    }
    
    // Normalize line endings to CRLF for file
    char *normalized = file_normalize_line_endings(text, buffer->size);
    free(text);
    
    if (!normalized) {
        MessageBoxA(hwnd, ERR_OUT_OF_MEMORY, DLG_ERROR, MB_ICONERROR);
        return false;
    }
    
    int normalized_length = (int)strlen(normalized);
    
    HANDLE hFile = CreateFileA(
        filename,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        free(normalized);
        MessageBoxA(hwnd, ERR_FAILED_CREATE_FILE, DLG_ERROR, MB_ICONERROR);
        return false;
    }
    
    // Write UTF-8 BOM
    DWORD bytesWritten;
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    if (!WriteFile(hFile, bom, 3, &bytesWritten, NULL)) {
        free(normalized);
        CloseHandle(hFile);
        MessageBoxA(hwnd, ERR_FAILED_WRITE_FILE, DLG_ERROR, MB_ICONERROR);
        return false;
    }
    
    // Write text
    if (!WriteFile(hFile, normalized, normalized_length, &bytesWritten, NULL)) {
        free(normalized);
        CloseHandle(hFile);
        MessageBoxA(hwnd, ERR_FAILED_WRITE_FILE, DLG_ERROR, MB_ICONERROR);
        return false;
    }
    
    free(normalized);
    CloseHandle(hFile);
    return true;
}

bool file_open_dialog(HWND hwnd, char *out_filename, int out_size, GapBuffer *buffer) {
    OPENFILENAMEA ofn = {0};
    char filename[MAX_PATH] = {0};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    if (GetOpenFileNameA(&ofn)) {
        if (file_load(hwnd, filename, buffer)) {
            strncpy(out_filename, filename, out_size);
            return true;
        }
    }
    
    return false;
}

bool file_save_dialog(HWND hwnd, char *out_filename, int out_size, GapBuffer *buffer) {
    OPENFILENAMEA ofn = {0};
    char filename[MAX_PATH] = {0};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileNameA(&ofn)) {
        if (file_save(hwnd, filename, buffer)) {
            strncpy(out_filename, filename, out_size);
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// Auto-save functions
// ============================================================================

void file_get_auto_save_path(App *app, const char *filename, char *out_path, int out_size) {
    if (!app || !filename || !out_path) return;
    
    // Create hash from filename for safe path
    unsigned int hash = 0;
    for (const char *p = filename; *p; p++) {
        hash = hash * 31 + (unsigned char)*p;
    }
    
    if (app->auto_save_dir[0]) {
        snprintf(out_path, out_size, "%s\\%u.fpad", app->auto_save_dir, hash);
    } else {
        // Fallback to temp directory
        char temp_path[MAX_PATH];
        GetTempPathA(MAX_PATH, temp_path);
        snprintf(out_path, out_size, "%sFastPad_%u.fpad", temp_path, hash);
    }
}

bool file_auto_save(App *app, const char *filename, GapBuffer *buffer) {
    if (!app || !filename || !buffer) return false;
    
    char auto_path[MAX_PATH];
    file_get_auto_save_path(app, filename, auto_path, MAX_PATH);
    
    // Get full text
    char *text = buffer_get_text(buffer, 0, buffer->size);
    if (!text) return false;
    
    // Normalize line endings to CRLF
    char *normalized = file_normalize_line_endings(text, buffer->size);
    free(text);
    
    if (!normalized) return false;
    
    int normalized_length = (int)strlen(normalized);
    
    // Ensure directory exists
    if (app->auto_save_dir[0]) {
        CreateDirectoryA(app->auto_save_dir, NULL);
    }
    
    HANDLE hFile = CreateFileA(
        auto_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        free(normalized);
        return false;
    }
    
    // Write UTF-8 BOM
    DWORD bytesWritten;
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    if (!WriteFile(hFile, bom, 3, &bytesWritten, NULL)) {
        free(normalized);
        CloseHandle(hFile);
        return false;
    }
    
    // Write text
    if (!WriteFile(hFile, normalized, normalized_length, &bytesWritten, NULL)) {
        free(normalized);
        CloseHandle(hFile);
        return false;
    }
    
    free(normalized);
    CloseHandle(hFile);
    return true;
}

bool file_restore_auto_save(const char *filename, GapBuffer *buffer) {
    (void)filename;
    (void)buffer;
    // Implementation would check for auto-save file and restore
    return false;
}

// ============================================================================
// Encoding Detection and Conversion
// ============================================================================

EncodingType file_detect_encoding(const char *data, int length) {
    if (!data || length <= 0) {
        return ENCODING_UTF8;
    }
    
    // Check for UTF-16 BOM
    if (length >= 2) {
        if ((unsigned char)data[0] == 0xFF && (unsigned char)data[1] == 0xFE) {
            return ENCODING_UTF16LE;
        }
        if ((unsigned char)data[0] == 0xFE && (unsigned char)data[1] == 0xFF) {
            return ENCODING_UTF16BE;
        }
    }
    
    // Check for UTF-8 BOM
    if (file_has_bom(data, length)) {
        return ENCODING_UTF8_BOM;
    }
    
    // Check for valid UTF-8 sequences
    bool is_valid_utf8 = true;
    int i = 0;
    while (i < length) {
        unsigned char c = (unsigned char)data[i];
        
        // Check for ASCII
        if (c <= 0x7F) {
            i++;
            continue;
        }
        
        // Check for valid 2-byte UTF-8 sequence
        if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= length || (data[i + 1] & 0xC0) != 0x80) {
                is_valid_utf8 = false;
                break;
            }
            i += 2;
            continue;
        }
        
        // Check for valid 3-byte UTF-8 sequence
        if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= length || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) {
                is_valid_utf8 = false;
                break;
            }
            i += 3;
            continue;
        }
        
        // Check for valid 4-byte UTF-8 sequence
        if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= length || (data[i + 1] & 0xC0) != 0x80 || 
                (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0) != 0x80) {
                is_valid_utf8 = false;
                break;
            }
            i += 4;
            continue;
        }
        
        // Invalid UTF-8 start byte
        is_valid_utf8 = false;
        break;
    }
    
    if (is_valid_utf8) {
        return ENCODING_UTF8;
    }
    
    return ENCODING_ANSI;
}

char* file_convert_encoding(const char *input, int input_len, int *output_len, 
                           EncodingType from_enc, EncodingType to_enc) {
    (void)from_enc;
    (void)to_enc;
    if (!input || input_len <= 0) {
        if (output_len) *output_len = 0;
        return NULL;
    }
    
    // For now, just do simple pass-through for same encoding
    // Full conversion would require code page tables or iconv
    
    char *output = (char *)malloc(input_len + 1);
    if (!output) {
        if (output_len) *output_len = 0;
        return NULL;
    }
    
    memcpy(output, input, input_len);
    output[input_len] = '\0';
    
    if (output_len) *output_len = input_len;
    return output;
}

char *file_normalize_line_endings_ex(const char *text, int length, LineEndingType target_ending) {
    if (!text || length <= 0) {
        char *result = (char *)malloc(1);
        if (result) result[0] = '\0';
        return result;
    }
    
    // First, count how many line endings need conversion
    int lf_count = 0;
    int cr_count = 0;
    bool last_was_cr = false;
    
    for (int i = 0; i < length; i++) {
        if (text[i] == '\n') {
            if (!last_was_cr) {
                lf_count++;
            }
        } else if (text[i] == '\r') {
            cr_count++;
        }
        last_was_cr = (text[i] == '\r');
    }
    
    // Calculate output size
    int output_size = length;
    if (target_ending == LINE_ENDING_CRLF) {
        // Need to add CR before LF where missing
        output_size += lf_count;
    } else if (target_ending == LINE_ENDING_LF) {
        // Need to remove CR before LF
        output_size -= cr_count;
    } else if (target_ending == LINE_ENDING_CR) {
        // Need to remove LF
        output_size -= lf_count;
    }
    
    char *result = (char *)malloc(output_size + 1);
    if (!result) return NULL;
    
    int j = 0;
    last_was_cr = false;
    
    for (int i = 0; i < length; i++) {
        if (text[i] == '\r') {
            if (target_ending == LINE_ENDING_CRLF) {
                result[j++] = '\r';
                result[j++] = '\n';
            } else if (target_ending == LINE_ENDING_LF) {
                // Skip CR, LF will be handled next if present
                // Actually, output CR only
                result[j++] = '\r';
            } else {
                // LINE_ENDING_CR: keep CR only
                result[j++] = '\r';
            }
            last_was_cr = true;
        } else if (text[i] == '\n') {
            if (target_ending == LINE_ENDING_CRLF) {
                if (!last_was_cr) {
                    result[j++] = '\r';
                }
                result[j++] = '\n';
            } else if (target_ending == LINE_ENDING_LF) {
                result[j++] = '\n';
            } else {
                // LINE_ENDING_CR: skip LF
            }
            last_was_cr = false;
        } else {
            result[j++] = text[i];
            last_was_cr = false;
        }
    }
    
    result[j] = '\0';
    
    return result;
}

bool file_save_with_options(HWND hwnd, const char *filename, GapBuffer *buffer, 
                           EncodingType encoding, LineEndingType line_ending) {
    // Get full text
    char *text = buffer_get_text(buffer, 0, buffer->size);
    if (!text) {
        MessageBoxA(hwnd, ERR_FAILED_GET_TEXT, DLG_ERROR, MB_ICONERROR);
        return false;
    }
    
    // Normalize line endings
    char *normalized = file_normalize_line_endings_ex(text, buffer->size, line_ending);
    free(text);
    
    if (!normalized) {
        MessageBoxA(hwnd, ERR_OUT_OF_MEMORY, DLG_ERROR, MB_ICONERROR);
        return false;
    }
    
    int normalized_length = (int)strlen(normalized);
    
    HANDLE hFile = CreateFileA(
        filename,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        free(normalized);
        MessageBoxA(hwnd, ERR_FAILED_CREATE_FILE, DLG_ERROR, MB_ICONERROR);
        return false;
    }
    
    DWORD bytesWritten;
    
    // Write BOM if needed
    if (encoding == ENCODING_UTF8_BOM) {
        unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        if (!WriteFile(hFile, bom, 3, &bytesWritten, NULL)) {
            free(normalized);
            CloseHandle(hFile);
            MessageBoxA(hwnd, ERR_FAILED_WRITE_FILE, DLG_ERROR, MB_ICONERROR);
            return false;
        }
    } else if (encoding == ENCODING_UTF16LE) {
        unsigned char bom[] = {0xFF, 0xFE};
        if (!WriteFile(hFile, bom, 2, &bytesWritten, NULL)) {
            free(normalized);
            CloseHandle(hFile);
            MessageBoxA(hwnd, ERR_FAILED_WRITE_FILE, DLG_ERROR, MB_ICONERROR);
            return false;
        }
    } else if (encoding == ENCODING_UTF16BE) {
        unsigned char bom[] = {0xFE, 0xFF};
        if (!WriteFile(hFile, bom, 2, &bytesWritten, NULL)) {
            free(normalized);
            CloseHandle(hFile);
            MessageBoxA(hwnd, ERR_FAILED_WRITE_FILE, DLG_ERROR, MB_ICONERROR);
            return false;
        }
    }
    
    // Write text
    if (!WriteFile(hFile, normalized, normalized_length, &bytesWritten, NULL)) {
        free(normalized);
        CloseHandle(hFile);
        MessageBoxA(hwnd, ERR_FAILED_WRITE_FILE, DLG_ERROR, MB_ICONERROR);
        return false;
    }
    
    free(normalized);
    CloseHandle(hFile);
    return true;
}
