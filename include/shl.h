#ifndef PSADMIN_SHL_H
#define PSADMIN_SHL_H

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "common.h"

/* Tipo de entrada de directorio, simplificado para la UI */
typedef enum {
    SHL_ENTRY_FILE = 0,
    SHL_ENTRY_DIR,
    SHL_ENTRY_LINK,
    SHL_ENTRY_OTHER
} shl_entry_type_t;

typedef struct {
    char name[PS_NAME_MAX];
    shl_entry_type_t type;
    off_t size;
    mode_t mode;
    time_t mtime;
} shl_entry_t;

typedef struct {
    char cwd[PS_PATH_MAX];
    shl_entry_t *entries;
    size_t count;
    size_t capacity;
    int selected; /* índice del cursor dentro de entries */

    /* "portapapeles" de un solo slot para copiar/mover */
    char clip_path[PS_PATH_MAX]; /* ruta absoluta marcada, vacío si no hay nada */
    int clip_is_move;             /* 1 = mover al pegar, 0 = copiar */
} shl_state_t;

/* --- Ciclo de vida --- */

/* Inicializa el estado del explorador en start_path (NULL = $HOME) */
int shl_init(shl_state_t *st, const char *start_path);

/* Libera memoria reservada por shl_init / shl_reload */
void shl_free(shl_state_t *st);

/* --- Listado --- */

/* Relee el directorio actual (st->cwd) y repuebla st->entries.
 * Ordena: directorios primero, luego archivos, ambos alfabéticamente. */
int shl_reload(shl_state_t *st);

/* Cambia de directorio relativo a st->cwd (soporta "..") y recarga */
int shl_chdir(shl_state_t *st, const char *target);

/* Devuelve la ruta absoluta de la entrada actualmente seleccionada.
 * out debe tener al menos PS_PATH_MAX bytes. Retorna 0 en éxito. */
int shl_selected_path(const shl_state_t *st, char *out, size_t out_sz);

/* --- Selección / navegación lógica (sin dibujar) --- */
void shl_move_selection(shl_state_t *st, int delta);

/* --- Eliminar ---
 * Elimina la entrada actualmente seleccionada. Si es un directorio, borra su
 * contenido recursivamente. No permite eliminar "..". Recarga st->entries
 * tras eliminar con éxito. Retorna 0 en éxito, -1 en error. */
int shl_delete_selected(shl_state_t *st);

/* --- Copiar / mover ---
 * shl_clip_set marca la entrada seleccionada (no permite "..") para una
 * operación posterior de copiar (is_move=0) o mover (is_move=1). Retorna 0
 * en éxito, -1 si no hay nada válido seleccionado.
 *
 * shl_clip_paste copia/mueve lo marcado hacia st->cwd (directorio actual),
 * usando el mismo nombre base del origen. Recarga st->entries tras pegar.
 * Retorna 0 en éxito, -1 si no había nada marcado o falló la operación. */
int shl_clip_set(shl_state_t *st, int is_move);
int shl_clip_paste(shl_state_t *st);

/* --- Búsqueda ---
 * Filtra st->entries por subcadena en el nombre (case-insensitive), análogo
 * a tsk_search. matches debe tener espacio para al menos max_matches
 * punteros. Retorna la cantidad de coincidencias. */
size_t shl_search(const shl_state_t *st, const char *needle,
                   const shl_entry_t **matches, size_t max_matches);

/* --- Estadísticas de archivo ---
 * Formatea en una sola línea: nombre, tipo, permisos estilo "ls -l" y
 * tamaño legible (B/KB/MB/...) y fecha de modificación de una entrada.
 * Función pura (no toca el filesystem), testeable con un shl_entry_t
 * construido a mano. */
void shl_format_entry_stats(const shl_entry_t *e, char *out, size_t out_sz);

/* --- Respaldo automático ---
 * Comprime (tar+gzip) la entrada seleccionada (archivo o directorio) y la
 * deposita en dest_dir con un nombre con timestamp:
 *   <nombre>_YYYYmmdd_HHMMSS.tar.gz
 * Si dest_dir es NULL usa "<HOME>/.psadmin_backups".
 * Retorna 0 en éxito, -1 en error (ver errno / stderr del subproceso). */
int shl_backup_selected(const shl_state_t *st, const char *dest_dir,
                         char *out_backup_path, size_t out_sz);

/* Construye el nombre de archivo de respaldo (lógica pura, testeable) sin
 * tocar el filesystem. base_name es el nombre original (sin ruta). */
void shl_build_backup_name(const char *base_name, time_t now,
                            char *out, size_t out_sz);

#endif /* PSADMIN_SHL_H */
