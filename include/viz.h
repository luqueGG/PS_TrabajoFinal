#ifndef PSADMIN_VIZ_H
#define PSADMIN_VIZ_H

#include "common.h"

/* --- terminal raw mode --- */

/* Pone la terminal en raw mode (sin buffering de línea, sin eco) y guarda
 * el estado original en *saved para poder restaurarlo. Retorna 0 en éxito. */
int viz_enable_raw_mode(void *saved_termios);

/* Restaura la terminal al estado guardado por viz_enable_raw_mode */
int viz_disable_raw_mode(void *saved_termios);

/* Tamaño en bytes que el caller debe reservar para "saved_termios"
 * (se evita exponer <termios.h> en el header para no acoplar a todos
 * los módulos que lo incluyan). */
size_t viz_termios_size(void);

/* --- layout --- */

/* Calcula los 4 rectángulos (tsk, con, shl, cli) dado el tamaño de
 * terminal term_rows x term_cols, replicando el layout ascii del diseño:
 *
 *  +--------+-----------------+--------+
 *  |  tsk   |       con       |  shl   |
 *  |        |-----------------|        |
 *  |        |       cli       |        |
 *  +--------+-----------------+--------+
 *
 * out debe tener tamaño PANEL_COUNT (definido en common.h), indexado por
 * ps_panel_id_t. Función pura, sin tocar la terminal real: testeable. */
void viz_compute_layout(int term_rows, int term_cols, ps_rect_t *out);

/* --- primitivas de dibujo (usan stdout directamente con ANSI) --- */

/* Limpia toda la pantalla y mueve el cursor a home */
void viz_clear_screen(void);

/* Dibuja el borde + título de un panel en su rect. */
void viz_draw_panel_frame(const ps_rect_t *r, const char *title, int focused);

/* Escribe texto dentro del área interior de un panel (recortando si no
 * entra), en la fila relativa 'rel_row' (0-based, dentro del contenido). */
void viz_draw_panel_line(const ps_rect_t *r, int rel_row, const char *text);

/* Mueve el cursor del terminal a una posición absoluta (1-based) */
void viz_move_cursor(int row, int col);

#endif /* PSADMIN_VIZ_H */
