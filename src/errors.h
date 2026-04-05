/*
 * FastPad Error Messages
 * 
 * Centralized location for all user-visible messages.
 * To translate FastPad, edit the strings below.
 */

#ifndef ERRORS_H
#define ERRORS_H

/* Error messages */
#define ERR_FAILED_OPEN_FILE "Failed to open file."
#define ERR_FAILED_READ_FILE_SIZE "Failed to read file size."
#define ERR_FAILED_READ_FILE "Failed to read file."
#define ERR_OUT_OF_MEMORY "Out of memory."
#define ERR_FAILED_CREATE_FILE "Failed to create file."
#define ERR_FAILED_WRITE_FILE "Failed to write file."
#define ERR_FAILED_GET_TEXT "Failed to get text."
#define ERR_FAILED_INIT_TABS "Failed to initialize tabs."
#define ERR_FAILED_REGISTER_CLASS "Failed to register window class."
#define ERR_FAILED_CREATE_WINDOW "Failed to create window."
#define ERR_FAILED_FIND_DIALOG "Failed to create find dialog."
#define ERR_MAX_TABS_REACHED "Maximum number of tabs reached."

/* Informational messages */
#define MSG_TEXT_NOT_FOUND "Text not found."
#define MSG_LARGE_FILE \
    "This file is large (%.1f MB) and might be slow to edit.\n" \
    "Continue anyway?"
#define MSG_UNSAVED_CHANGES \
    "Save changes to \"%s\"?"
#define MSG_UNSAVED_ALL_TABS \
    "Some tabs have unsaved changes. Save all and exit?"
#define MSG_WORD_WRAP_NOT_IMPLEMENTED \
    "Word wrap is not yet implemented.\n" \
    "Long lines will extend beyond the visible area."
#define DLG_ERROR "Error"
#define DLG_FASTPAD "FastPad"
#define DLG_FIND "Find"
#define DLG_ABOUT "About FastPad"

/* About dialog text */
#define MSG_ABOUT_TEXT \
    "FastPad v1.0\n" \
    "A tiny, fast, native text editor."

/* File size threshold for large file warning (10 MB) */
#define LARGE_FILE_THRESHOLD (10 * 1024 * 1024)

#endif /* ERRORS_H */
