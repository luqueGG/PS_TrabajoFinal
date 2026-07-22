#ifndef PSADMIN_COMMON_H
#define PSADMIN_COMMON_H

#include <stddef.h>

/* Tamaño máximo de rutas y buffers de texto usados en todo el proyecto */
#define PS_PATH_MAX 4096
#define PS_LINE_MAX 1024
#define PS_NAME_MAX 256

/* Geometría de un panel en la pantalla (coordenadas 1-based, estilo terminal) */
typedef struct {
    int row;    /* fila superior izquierda */
    int col;    /* columna superior izquierda */
    int height; /* alto en filas */
    int width;  /* ancho en columnas */
} ps_rect_t;

/* Identificadores de los 4 paneles, usados para foco/navegación */
typedef enum {
    PANEL_TSK = 0,
    PANEL_CON = 1,
    PANEL_SHL = 2,
    PANEL_CLI = 3,
    PANEL_COUNT
} ps_panel_id_t;

#endif /* PSADMIN_COMMON_H */
