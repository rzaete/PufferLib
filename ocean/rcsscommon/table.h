#ifndef TABLE_H
#define TABLE_H

#include <stdbool.h>
#include <stdio.h>

#include "raylib.h"

#ifndef TABLE_MAX_COLS
#define TABLE_MAX_COLS 32
#endif

#define TABLE_CELL_LEN 32
#define TABLE_LABEL_LEN 64
#define TABLE_CELL_PAD 8
#define TABLE_COL_PAD 20
#define TABLE_MIN_LABEL_W 160
#define TABLE_MIN_COL_W 60

typedef struct {
    char text[TABLE_CELL_LEN];
    Color color;
} TableCell;

typedef struct {
    char label[TABLE_LABEL_LEN];
    Color label_color;
    TableCell cells[TABLE_MAX_COLS];
} TableRow;

typedef struct {
    Font font;
    int font_size;
    int ncols;
    const TableRow *header;
    const TableRow *rows;
    int nrows;
} Table;

static inline int table_ncols(const Table *t) {
    int n = t->ncols;
    if (n > TABLE_MAX_COLS) n = TABLE_MAX_COLS;
    if (n < 0) n = 0;
    return n;
}

static void table_text(Font font, const char *s, int x, int y, int size, Color color) {
    if (font.texture.id == 0) {
        DrawText(s, x, y, size, color);
        return;
    }
    DrawTextEx(font, s, (Vector2){(float)x, (float)y}, (float)size, 0.0f, color);
}

static int table_text_width(Font font, const char *s, int size) {
    if (font.texture.id == 0)
        return MeasureText(s, size);
    return (int)MeasureTextEx(font, s, (float)size, 0.0f).x;
}

static void table_row_init(TableRow *row, const char *label, Color color, int ncols) {
    snprintf(row->label, sizeof(row->label), "%s", label);
    row->label_color = color;
    (void)ncols;
    for (int i = 0; i < TABLE_MAX_COLS; i++) {
        row->cells[i].text[0] = '\0';
        row->cells[i].color = color;
    }
}

static void table_row_floats(TableRow *row, const char *label, Color color,
                             const char *fmt, const float *vals, int ncols) {
    table_row_init(row, label, color, ncols);
    int n = ncols;
    if (n > TABLE_MAX_COLS) n = TABLE_MAX_COLS;
    for (int i = 0; i < n; i++)
        snprintf(row->cells[i].text, sizeof(row->cells[i].text), fmt, vals[i]);
}

static void table_row_ints(TableRow *row, const char *label, Color color,
                           const int *vals, int ncols) {
    table_row_init(row, label, color, ncols);
    int n = ncols;
    if (n > TABLE_MAX_COLS) n = TABLE_MAX_COLS;
    for (int i = 0; i < n; i++)
        snprintf(row->cells[i].text, sizeof(row->cells[i].text), "%d", vals[i]);
}

static void table_row_bools(TableRow *row, const char *label, Color off, Color on,
                            const bool *vals, int ncols) {
    table_row_init(row, label, off, ncols);
    int n = ncols;
    if (n > TABLE_MAX_COLS) n = TABLE_MAX_COLS;
    for (int i = 0; i < n; i++) {
        snprintf(row->cells[i].text, sizeof(row->cells[i].text), "%s", vals[i] ? "true" : "false");
        row->cells[i].color = vals[i] ? on : off;
    }
}

static int table_line_h(const Table *t) {
    return t->font_size + 2;
}

static int table_label_w(const Table *t) {
    int max_w = 0;
    int size = t->font_size;
    if (t->header) {
        int w = table_text_width(t->font, t->header->label, size);
        if (w > max_w) max_w = w;
    }
    for (int i = 0; i < t->nrows; i++) {
        int w = table_text_width(t->font, t->rows[i].label, size);
        if (w > max_w) max_w = w;
    }
    int label_w = max_w + TABLE_COL_PAD;
    if (label_w < TABLE_MIN_LABEL_W) label_w = TABLE_MIN_LABEL_W;
    return label_w;
}

static int table_col_w(const Table *t) {
    int max_w = 0;
    int size = t->font_size;
    int ncols = table_ncols(t);
    if (t->header) {
        for (int c = 0; c < ncols; c++) {
            int w = table_text_width(t->font, t->header->cells[c].text, size);
            if (w > max_w) max_w = w;
        }
    }
    for (int i = 0; i < t->nrows; i++) {
        for (int c = 0; c < ncols; c++) {
            int w = table_text_width(t->font, t->rows[i].cells[c].text, size);
            if (w > max_w) max_w = w;
        }
    }
    int col_w = max_w + TABLE_COL_PAD;
    if (col_w < TABLE_MIN_COL_W) col_w = TABLE_MIN_COL_W;
    return col_w;
}

static int table_width(const Table *t) {
    return table_label_w(t) + table_ncols(t) * table_col_w(t);
}

static int table_height(const Table *t) {
    return (t->nrows + (t->header ? 1 : 0)) * table_line_h(t);
}

static void table_draw_row(const Table *t, const TableRow *row, int x, int y,
                           int label_w, int col_w, int line_h, int width,
                           int row_i, bool is_header) {
    if (is_header)
        DrawRectangle(x, y, width, line_h, (Color){255, 255, 255, 22});
    else if (row_i % 2 == 0)
        DrawRectangle(x, y, width, line_h, (Color){255, 255, 255, 8});

    table_text(t->font, row->label, x, y, t->font_size, row->label_color);

    unsigned char div_a = is_header ? 50 : 35;
    unsigned char grid_a = is_header ? 30 : 18;
    DrawLine(x + label_w, y, x + label_w, y + line_h, (Color){255, 255, 255, div_a});

    int ncols = table_ncols(t);
    for (int c = 0; c < ncols; c++) {
        int col_x = x + label_w + c * col_w;
        table_text(t->font, row->cells[c].text, col_x + TABLE_CELL_PAD, y, t->font_size,
                   row->cells[c].color);
        DrawLine(col_x + col_w, y, col_x + col_w, y + line_h, (Color){255, 255, 255, grid_a});
    }
}

static void table_draw(int x, int y, const Table *t) {
    if (!t || table_ncols(t) < 1) return;
    int line_h = table_line_h(t);
    int label_w = table_label_w(t);
    int col_w = table_col_w(t);
    int width = label_w + table_ncols(t) * col_w;

    if (t->header) {
        table_draw_row(t, t->header, x, y, label_w, col_w, line_h, width, 0, true);
        y += line_h;
    }
    for (int i = 0; i < t->nrows; i++) {
        table_draw_row(t, &t->rows[i], x, y, label_w, col_w, line_h, width, i, false);
        y += line_h;
    }
}

#endif
