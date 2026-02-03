/**
 * @file editor.h
 * @brief A simple terminal-based text editor using ncurses
 *
 * This text editor supports basic editing operations including:
 * - Character insertion and deletion
 * - Line navigation with arrow keys
 * - Save functionality (Ctrl+S)
 * - Multi-line text management
 * - Automatic scrolling and viewport management
 */

#ifndef EDITOR_H
#define EDITOR_H

#include <stddef.h>

/**
 * @struct Buffer
 * @brief Manages the text content
 *
 * Stores all lines of the document with dynamic capacity expansion.
 * Each line is null-terminated and its length is tracked separately.
 *
 * @member lines Array of pointers to individual lines
 * @member line_len Array of lengths for each line
 * @member num_lines Number of lines currently in the buffer
 * @member capacity Maximum number of lines the buffer can hold
 */
typedef struct {
  char **lines;
  size_t *line_len;
  int num_lines;
  int capacity;
} Buffer;

/**
 * @struct Cursor
 * @brief Tracks cursor position and viewport offset
 *
 * Maintains both absolute cursor position (cx, cy) and the viewport offset
 * to support scrolling when the file is larger than the terminal.
 *
 * @member cx Cursor X position (column)
 * @member cy Cursor Y position (row/line number)
 * @member rowoff Row offset for vertical scrolling
 * @member coloff Column offset for horizontal scrolling
 */
typedef struct {
  int cx, cy;
  int rowoff, coloff;
} Cursor;

/**
 * @struct Editor
 * @brief Main editor state
 *
 * Combines the buffer and cursor state with the filename reference.
 *
 * @member buffer The text content buffer
 * @member cursor Current cursor position and viewport state
 * @member filename Path to the open file
 */
typedef struct {
  Buffer buffer;
  Cursor cursor;
  const char *filename;
} Editor;

/**
 * @brief Initializes a buffer with a given capacity
 *
 * Allocates memory for the lines and line_len arrays and sets the initial
 * state.
 *
 * @param buf Pointer to the buffer to initialize
 * @param initial_capacity Initial number of lines the buffer can hold
 */
static void buffer_init(Buffer *buf, int initial_capacity);

/**
 * @brief Ensures the buffer has enough capacity for a required number of lines
 *
 * Dynamically expands the buffer capacity by doubling if necessary.
 * Uses exponential growth to minimize reallocation overhead.
 *
 * @param buf Pointer to the buffer
 * @param required Minimum number of lines needed
 */
static void buffer_ensure_capacity(Buffer *buf, int required);

/**
 * @brief Frees all memory associated with a buffer
 *
 * Deallocates each line string, then the lines and line_len arrays.
 *
 * @param buf Pointer to the buffer to free
 */
static void buffer_free(Buffer *buf);

/**
 * @brief Saves the buffer contents to the file
 *
 * Writes all lines from the buffer to the associated filename,
 * with each line followed by a newline character.
 *
 * @param ed Pointer to the editor state
 * @return 1 on success, 0 on failure (file I/O error)
 */
static int save_buffer(const Editor *ed);

/**
 * @brief Deletes a line from the buffer
 *
 * Removes the line at the specified index, shifting all subsequent lines up.
 * Frees the memory of the deleted line.
 *
 * @param buf Pointer to the buffer
 * @param at Index of the line to delete
 */
static void delete_line(Buffer *buf, int at);

/**
 * @brief Displays a message on the bottom status line
 *
 * Shows a message in reverse video on the last line of the terminal.
 * Useful for displaying status updates like "File saved successfully".
 *
 * @param msg The message string to display
 */
static void show_message(const char *msg);

/**
 * @brief Redraws the editor viewport
 *
 * Clears the screen and renders visible lines based on the current viewport
 * offset. Handles both horizontal and vertical scrolling, positioning the
 * cursor correctly.
 *
 * @param ed Pointer to the editor state
 */
static void redraw(const Editor *ed);

/**
 * @brief Constrains cursor position within valid bounds and adjusts viewport
 *
 * Ensures the cursor position is within the buffer and viewport limits.
 * Adjusts the viewport offset (rowoff, coloff) to keep the cursor visible
 * on screen by automatically scrolling when necessary.
 *
 * @param ed Pointer to the editor state
 */
static void clamp_cursor(Editor *ed);

/**
 * @brief Inserts a character at the cursor position
 *
 * Inserts a character at the current cursor location, expanding the line as
 * needed. Handles memory reallocation and updates line length tracking.
 *
 * @param ed Pointer to the editor state
 * @param ch The character to insert, as an integer
 */
static void insert_char(Editor *ed, int ch);

/**
 * @brief Handles backspace (backward delete) operation
 *
 * Deletes the character before the cursor. If the cursor is at the beginning of
 * a line, merges the current line with the previous line.
 *
 * @param ed Pointer to the editor state
 */
static void backspace(Editor *ed);

/**
 * @brief Handles delete (forward delete) operation
 *
 * Deletes the character at the cursor. If the cursor is at the end of a line,
 * merges the current line with the next line.
 *
 * @param ed Pointer to the editor state
 */
static void delete_at_cursor(Editor *ed);

/**
 * @brief Inserts a newline at the cursor position, splitting the line
 *
 * Splits the current line at the cursor position:
 * - The content before the cursor remains on the current line
 * - The content after the cursor moves to a new line below
 * - The cursor moves to the beginning of the new line
 *
 * @param ed Pointer to the editor state
 */
static void insert_newline(Editor *ed);

/**
 * @brief Loads a file into the editor buffer
 *
 * Opens and reads the file line by line, storing each line in the buffer.
 * Initializes the cursor and viewport to the beginning of the file.
 * If the file is empty, creates a single empty line.
 *
 * @param ed Pointer to the editor state
 * @param filename Path to the file to load
 * @return 1 on success, 0 on failure (file not found or I/O error)
 */
static int load_file(Editor *ed, const char *filename);

/**
 * @brief Main entry point for the text editor
 *
 * Initializes ncurses, loads the file, and runs the main event loop.
 * Handles all user input and coordinates editor operations.
 *
 * Usage: ./editor <filename>
 *
 * Key bindings:
 * - Arrow keys: Move cursor
 * - Ctrl+S / Ctrl+W: Save file
 * - Backspace / Delete: Delete characters
 * - Enter: Insert newline
 * - Printable characters: Insert character
 * - Esc: Exit editor
 *
 * @param argc Number of command line arguments
 * @param argv Command line arguments (expects filename)
 * @return 0 on success, 1 on failure
 */
int main(int argc, char *argv[]);

#endif /* EDITOR_H */
