#include "fileio.h"
#include "buffer.h"
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

    // Copy text with normalized line endings
    int dest_pos = 0;
    for (DWORD i = offset; i < bytesRead; i++) {
        if (fileData[i] == '\r' && i + 1 < bytesRead && fileData[i+1] == '\n') {
            buffer->data[dest_pos++] = '\n';
            i++; // Skip LF
        } else {
            buffer->data[dest_pos++] = fileData[i];
        }
    }

    buffer->size = text_length;
    buffer->gap_start = text_length;
    buffer->gap_length = buffer->capacity - text_length;

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
