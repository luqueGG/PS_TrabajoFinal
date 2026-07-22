#ifndef PSADMIN_CON_H
#define PSADMIN_CON_H

#include "common.h"

/* Categoría de un hallazgo. Pensado para crecer (CON_FIND_CONDITIONAL,
 * CON_FIND_FUNCTION, etc.) sin romper el código existente. */
typedef enum {
    CON_FIND_VARIABLE = 0,
    CON_FIND_LOOP,
    CON_FIND_COUNT /* mantener al final */
} con_find_kind_t;

typedef struct {
    con_find_kind_t kind;
    int line_no;            /* 1-based */
    char text[PS_LINE_MAX];  /* fragmento relevante (nombre de var, o tipo+condición de ciclo) */
} con_finding_t;

typedef struct {
    con_finding_t *items;
    size_t count;
    size_t capacity;
} con_result_t;

/* --- API de un "analizador" extensible ---
 * Cada analizador recibe una línea ya trimeada (sin espacios al inicio) y,
 * si encuentra algo, agrega un con_finding_t al resultado.
 * Para agregar un analizador nuevo (ifs, funciones, etc.) basta con:
 *   1) sumar un valor a con_find_kind_t
 *   2) escribir una función con esta firma
 *   3) registrarla en con_analyzer_table[] (con.c)
 */
typedef void (*con_analyzer_fn)(const char *trimmed_line, int line_no,
                                 con_result_t *result);

/* --- ciclo de vida del resultado --- */
void con_result_init(con_result_t *res);
void con_result_free(con_result_t *res);
int con_result_add(con_result_t *res, con_find_kind_t kind, int line_no,
                    const char *text);

/* --- entrada principal --- */
/* Analiza el archivo en path corriendo todos los analizadores registrados
 * sobre cada línea. Retorna 0 en éxito, -1 si no se pudo abrir el archivo. */
int con_analyze_file(const char *path, con_result_t *out);

/* Variante pura para tests: analiza un buffer ya cargado en memoria
 * (líneas separadas por '\n'), sin tocar el filesystem. */
void con_analyze_buffer(const char *buffer, con_result_t *out);

/* --- analizadores individuales (expuestos para poder testear cada uno) --- */

/* Detecta asignaciones de variable estilo bash: NOMBRE=valor (sin espacios
 * alrededor del '='), ignorando comparaciones (==), comentarios y líneas
 * que empiezan con palabras clave de control. */
void con_analyze_variables(const char *trimmed_line, int line_no, con_result_t *result);

/* Detecta inicios de ciclo: for / while / until (incluye "for ((...))" y
 * "for x in ..."). */
void con_analyze_loops(const char *trimmed_line, int line_no, con_result_t *result);

/* Devuelve una cadena legible para humanos para un con_find_kind_t */
const char *con_kind_label(con_find_kind_t kind);

#endif /* PSADMIN_CON_H */
