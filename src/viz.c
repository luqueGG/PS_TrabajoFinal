#include "viz.h"

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

size_t viz_termios_size(void) { return sizeof(struct termios); }

int viz_enable_raw_mode(void *saved_termios) {
    struct termios *saved = (struct termios *)saved_termios;
    if (tcgetattr(STDIN_FILENO, saved) != 0) return -1;

    struct termios raw = *saved;
    raw.c_lflag &= ~(unsigned)(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(unsigned)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(unsigned)(OPOST);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0; /* lecturas no bloqueantes a nivel de polling externo */

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return -1;
    return 0;
}

int viz_disable_raw_mode(void *saved_termios) {
    struct termios *saved = (struct termios *)saved_termios;
    return tcsetattr(STDIN_FILENO, TCSAFLUSH, saved);
}

void viz_compute_layout(int term_rows, int term_cols, ps_rect_t *out) {
    /* columnas: tsk | con+cli | shl, en proporción aprox 1:2:1 */
    int left_w = term_cols / 4;
    int right_w = term_cols / 4;
    int mid_w = term_cols - left_w - right_w;
    if (left_w < 10) left_w = 10;
    if (right_w < 10) right_w = 10;
    if (mid_w < 20) mid_w = 20;

    /* filas del medio: con arriba, cli abajo, proporción 2:1 */
    int con_h = (term_rows * 2) / 3;
    int cli_h = term_rows - con_h;
    if (con_h < 4) con_h = term_rows > 4 ? term_rows - 2 : 2;
    if (cli_h < 2) cli_h = term_rows - con_h > 0 ? term_rows - con_h : 1;

    out[PANEL_TSK] = (ps_rect_t){.row = 1, .col = 1, .height = term_rows, .width = left_w};
    out[PANEL_SHL] = (ps_rect_t){
        .row = 1, .col = left_w + mid_w + 1, .height = term_rows, .width = right_w};
    out[PANEL_CON] = (ps_rect_t){.row = 1, .col = left_w + 1, .height = con_h, .width = mid_w};
    out[PANEL_CLI] = (ps_rect_t){
        .row = con_h + 1, .col = left_w + 1, .height = cli_h, .width = mid_w};
}

void viz_clear_screen(void) {
    /* \x1b[2J borra todo, \x1b[H mueve el cursor a home */
    fputs("\x1b[2J\x1b[H", stdout);
}

void viz_move_cursor(int row, int col) {
    printf("\x1b[%d;%dH", row, col);
}

static void viz_put_horizontal(int row, int col, int width, char left, char fill, char right) {
    viz_move_cursor(row, col);
    putchar(left);
    for (int i = 0; i < width - 2; i++) putchar(fill);
    if (width >= 2) putchar(right);
}

void viz_draw_panel_frame(const ps_rect_t *r, const char *title, int focused) {
    /* esquina superior con título embebido */
    viz_put_horizontal(r->row, r->col, r->width, '+', '-', '+');

    viz_move_cursor(r->row, r->col + 2);
    if (focused) printf("[*%s*]", title); else printf("[%s]", title);

    /* bordes laterales */
    for (int i = 1; i < r->height - 1; i++) {
        viz_move_cursor(r->row + i, r->col);
        putchar('|');
        viz_move_cursor(r->row + i, r->col + r->width - 1);
        putchar('|');
    }

    viz_put_horizontal(r->row + r->height - 1, r->col, r->width, '+', '-', '+');
    fflush(stdout);
}

void viz_draw_panel_line(const ps_rect_t *r, int rel_row, const char *text) {
    int inner_row = r->row + 1 + rel_row;
    if (inner_row >= r->row + r->height - 1) return; /* fuera del panel */

    int inner_width = r->width - 2;
    if (inner_width <= 0) return;

    char buf[PS_LINE_MAX];
    size_t len = strlen(text);
    if (len > (size_t)inner_width) len = (size_t)inner_width;
    memcpy(buf, text, len);
    /* relleno con espacios para "limpiar" lo que había antes en esa línea */
    for (int i = (int)len; i < inner_width; i++) buf[i] = ' ';
    buf[inner_width] = '\0';

    viz_move_cursor(inner_row, r->col + 1);
    fputs(buf, stdout);
    fflush(stdout);
}
