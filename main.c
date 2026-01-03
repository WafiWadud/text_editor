#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char **lines;
  size_t *line_len;
  int num_lines;
  int capacity;
} Buffer;

typedef struct {
  int cx, cy;
  int rowoff, coloff;
} Cursor;

typedef struct {
  Buffer buffer;
  Cursor cursor;
  const char *filename;
} Editor;

static void buffer_init(Buffer *buf, int initial_capacity) {
  buf->lines = malloc(initial_capacity * sizeof(char *));
  buf->line_len = malloc(initial_capacity * sizeof(size_t));
  buf->num_lines = 0;
  buf->capacity = initial_capacity;
}

static void buffer_ensure_capacity(Buffer *buf, int required) {
  if (required <= buf->capacity)
    return;

  int new_capacity = buf->capacity * 2;
  while (new_capacity < required)
    new_capacity *= 2;

  buf->lines = realloc(buf->lines, new_capacity * sizeof(char *));
  buf->line_len = realloc(buf->line_len, new_capacity * sizeof(size_t));
  buf->capacity = new_capacity;
}

static void buffer_free(Buffer *buf) {
  for (int i = 0; i < buf->num_lines; i++) {
    free(buf->lines[i]);
  }
  free(buf->lines);
  free(buf->line_len);
}

static int save_buffer(const Editor *ed) {
  FILE *f = fopen(ed->filename, "w");
  if (!f)
    return 0;

  for (int i = 0; i < ed->buffer.num_lines; i++) {
    if (fputs(ed->buffer.lines[i], f) == EOF) {
      fclose(f);
      return 0;
    }
    if (fputc('\n', f) == EOF) {
      fclose(f);
      return 0;
    }
  }

  if (fclose(f) != 0)
    return 0;

  return 1;
}

static void delete_line(Buffer *buf, int at) {
  free(buf->lines[at]);

  memmove(&buf->lines[at], &buf->lines[at + 1],
          (buf->num_lines - at - 1) * sizeof(char *));
  memmove(&buf->line_len[at], &buf->line_len[at + 1],
          (buf->num_lines - at - 1) * sizeof(size_t));

  buf->num_lines--;
}

static void show_message(const char *msg) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  move(rows - 1, 0);
  clrtoeol();
  attron(A_REVERSE);
  mvprintw(rows - 1, 0, " %s ", msg);
  attroff(A_REVERSE);
  refresh();
}

static void redraw(const Editor *ed) {
  clear();

  for (int i = 0; i < LINES && (i + ed->cursor.rowoff) < ed->buffer.num_lines;
       i++) {
    char *line = ed->buffer.lines[i + ed->cursor.rowoff];

    if ((int)ed->buffer.line_len[i + ed->cursor.rowoff] > ed->cursor.coloff) {
      mvprintw(i, 0, "%.*s", COLS, &line[ed->cursor.coloff]);
    }
  }

  move(ed->cursor.cy - ed->cursor.rowoff, ed->cursor.cx - ed->cursor.coloff);
  refresh();
}

static void clamp_cursor(Editor *ed) {
  Cursor *c = &ed->cursor;
  const Buffer *buf = &ed->buffer;

  if (c->cy < 0)
    c->cy = 0;
  if (c->cy >= buf->num_lines)
    c->cy = buf->num_lines - 1;

  if (c->cx < 0)
    c->cx = 0;
  if (c->cx > (int)buf->line_len[c->cy])
    c->cx = buf->line_len[c->cy];

  /* Vertical scrolling */
  if (c->cy < c->rowoff)
    c->rowoff = c->cy;
  if (c->cy >= c->rowoff + LINES)
    c->rowoff = c->cy - LINES + 1;

  /* Horizontal scrolling */
  if (c->cx < c->coloff)
    c->coloff = c->cx;
  if (c->cx >= c->coloff + COLS)
    c->coloff = c->cx - COLS + 1;
}

static void insert_char(Editor *ed, int ch) {
  Buffer *buf = &ed->buffer;
  Cursor *c = &ed->cursor;

  char *line = buf->lines[c->cy];
  line = realloc(line, buf->line_len[c->cy] + 2);
  memmove(&line[c->cx + 1], &line[c->cx], buf->line_len[c->cy] - c->cx + 1);
  line[c->cx++] = ch;
  buf->lines[c->cy] = line;
  buf->line_len[c->cy]++;
}

static void backspace(Editor *ed) {
  Buffer *buf = &ed->buffer;
  Cursor *c = &ed->cursor;

  if (c->cx > 0) {
    /* Normal backspace inside line */
    char *line = buf->lines[c->cy];
    memmove(&line[c->cx - 1], &line[c->cx], buf->line_len[c->cy] - c->cx + 1);
    buf->line_len[c->cy]--;
    c->cx--;
    return;
  }

  /* cx == 0 → merge with previous line */
  if (c->cy == 0)
    return;

  int prev_len = buf->line_len[c->cy - 1];

  buf->lines[c->cy - 1] =
      realloc(buf->lines[c->cy - 1], prev_len + buf->line_len[c->cy] + 1);

  memcpy(&buf->lines[c->cy - 1][prev_len], buf->lines[c->cy],
         buf->line_len[c->cy] + 1);

  buf->line_len[c->cy - 1] += buf->line_len[c->cy];

  delete_line(buf, c->cy);

  c->cy--;
  c->cx = prev_len;
}

static void delete_at_cursor(Editor *ed) {
  Buffer *buf = &ed->buffer;
  Cursor *c = &ed->cursor;

  if (c->cx < (int)buf->line_len[c->cy]) {
    /* Normal delete inside line */
    memmove(&buf->lines[c->cy][c->cx], &buf->lines[c->cy][c->cx + 1],
            buf->line_len[c->cy] - c->cx);
    buf->line_len[c->cy]--;
    return;
  }

  /* cx == end of line → merge with next line */
  if (c->cy + 1 >= buf->num_lines)
    return;

  buf->lines[c->cy] = realloc(
      buf->lines[c->cy], buf->line_len[c->cy] + buf->line_len[c->cy + 1] + 1);

  memcpy(&buf->lines[c->cy][buf->line_len[c->cy]], buf->lines[c->cy + 1],
         buf->line_len[c->cy + 1] + 1);

  buf->line_len[c->cy] += buf->line_len[c->cy + 1];

  delete_line(buf, c->cy + 1);
}

static void insert_newline(Editor *ed) {
  Buffer *buf = &ed->buffer;
  Cursor *c = &ed->cursor;

  buffer_ensure_capacity(buf, buf->num_lines + 1);

  char *line = buf->lines[c->cy];

  /* Right-hand side after cursor */
  char *right = strdup(&line[c->cx]);

  /* Truncate current line at cursor */
  line[c->cx] = '\0';
  buf->lines[c->cy] = realloc(line, c->cx + 1);
  buf->line_len[c->cy] = c->cx;

  /* Make room for new line */
  memmove(&buf->lines[c->cy + 2], &buf->lines[c->cy + 1],
          (buf->num_lines - c->cy - 1) * sizeof(char *));
  memmove(&buf->line_len[c->cy + 2], &buf->line_len[c->cy + 1],
          (buf->num_lines - c->cy - 1) * sizeof(size_t));

  /* Insert new line */
  buf->lines[c->cy + 1] = right;
  buf->line_len[c->cy + 1] = strlen(right);
  buf->num_lines++;

  /* Move cursor */
  c->cy++;
  c->cx = 0;
}

static int load_file(Editor *ed, const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file)
    return 0;

  ed->filename = filename;
  buffer_init(&ed->buffer, 256);
  ed->cursor.cx = ed->cursor.cy = 0;
  ed->cursor.rowoff = ed->cursor.coloff = 0;

  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  while ((read = getline(&line, &len, file)) != -1) {
    buffer_ensure_capacity(&ed->buffer, ed->buffer.num_lines + 1);

    line[strcspn(line, "\n")] = '\0';
    ed->buffer.lines[ed->buffer.num_lines] = strdup(line);
    ed->buffer.line_len[ed->buffer.num_lines] =
        strlen(ed->buffer.lines[ed->buffer.num_lines]);
    ed->buffer.num_lines++;
  }

  if (ed->buffer.num_lines == 0) {
    buffer_ensure_capacity(&ed->buffer, 1);
    ed->buffer.lines[0] = strdup("");
    ed->buffer.line_len[0] = 0;
    ed->buffer.num_lines = 1;
  }

  free(line);
  fclose(file);
  return 1;
}

int main(int argc, char *argv[]) {
  if (argc < 2)
    return 1;

  Editor ed = {0};
  if (!load_file(&ed, argv[1]))
    return 1;

  initscr();
  raw(); /* Use raw() instead of cbreak() to capture all control characters */
  noecho();
  keypad(stdscr, TRUE);

  redraw(&ed);

  int ch;
  while ((ch = getch()) != 27) {
    switch (ch) {
    case 19: /* Ctrl+S */
    case 23: /* Ctrl+W - alternative save key */
      if (save_buffer(&ed)) {
        show_message("File saved successfully");
      } else {
        show_message("ERROR: Failed to save file");
      }
      napms(1000); /* Show message for 1 second */
      break;
    case KEY_UP:
      ed.cursor.cy--;
      break;
    case KEY_DOWN:
      ed.cursor.cy++;
      break;
    case KEY_LEFT:
      ed.cursor.cx--;
      break;
    case KEY_RIGHT:
      ed.cursor.cx++;
      break;
    case KEY_BACKSPACE:
    case 127:
      backspace(&ed);
      break;
    case KEY_DC:
      delete_at_cursor(&ed);
      break;
    case '\n':
    case KEY_ENTER:
      insert_newline(&ed);
      break;
    default:
      if (ch >= 32 && ch <= 126)
        insert_char(&ed, ch);
      break;
    }

    clamp_cursor(&ed);
    redraw(&ed);
  }

  endwin();
  buffer_free(&ed.buffer);
  return 0;
}
