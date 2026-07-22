#ifndef PSADMIN_TSK_H
#define PSADMIN_TSK_H

#include <sys/types.h>
#include "common.h"

typedef enum {
    TSK_RUNNING = 0,
    TSK_SLEEPING,
    TSK_STOPPED,
    TSK_ZOMBIE,
    TSK_OTHER
} tsk_state_t;

typedef struct {
    pid_t pid;
    pid_t ppid;
    char comm[PS_NAME_MAX];
    tsk_state_t state;
    unsigned long utime;     /* jiffies en modo usuario, de /proc/[pid]/stat */
    unsigned long stime;     /* jiffies en modo kernel */
    long rss_kb;              /* memoria residente en KB */
    double cpu_percent;       /* calculado entre dos muestras */
} tsk_proc_t;

typedef struct {
    tsk_proc_t *procs;
    size_t count;
    size_t capacity;
    int selected;
    unsigned long long last_total_jiffies; /* para % cpu */
} tsk_state_list_t;

typedef struct {
    double cpu_usage_percent; /* uso global del sistema, 0-100 */
    long mem_total_kb;
    long mem_free_kb;
    long mem_available_kb;
} tsk_sysinfo_t;

/* --- ciclo de vida --- */
int tsk_init(tsk_state_list_t *st);
void tsk_free(tsk_state_list_t *st);

/* --- listado --- */
/* Relee /proc y repuebla st->procs. Si había una muestra previa, calcula
 * cpu_percent por proceso usando la diferencia de jiffies. */
int tsk_refresh(tsk_state_list_t *st);

/* Filtra por subcadena en comm (case-insensitive). matches debe tener espacio
 * para al menos st->count punteros. Devuelve la cantidad de coincidencias. */
size_t tsk_search(const tsk_state_list_t *st, const char *needle,
                   const tsk_proc_t **matches, size_t max_matches);

/* --- info global del sistema --- */
int tsk_sysinfo(tsk_sysinfo_t *info);

/* --- acciones --- */
int tsk_kill(pid_t pid);        /* SIGTERM */
int tsk_force_kill(pid_t pid);  /* SIGKILL */
int tsk_suspend(pid_t pid);     /* SIGSTOP */
int tsk_resume(pid_t pid);      /* SIGCONT */

/* --- navegación lógica --- */
void tsk_move_selection(tsk_state_list_t *st, int delta);

/* --- árbol de procesos ---
 * Dado un pid raíz (0 = pid 1 / init), recorre st->procs y escribe en out[]
 * los pids en orden de pre-order DFS junto con su profundidad (depth[]).
 * Retorna la cantidad de nodos escritos. out/depth deben tener tamaño
 * st->count como cota superior. */
size_t tsk_build_tree(const tsk_state_list_t *st, pid_t root,
                       pid_t *out, int *depth, size_t max_out);

/* --- parsing puro (testeable sin /proc real) ---
 * Parsea una línea estilo /proc/[pid]/stat (campo a campo, ya separada por
 * espacios salvo el comm entre paréntesis) y llena los campos relevantes. */
int tsk_parse_stat_line(const char *line, tsk_proc_t *out);

#endif /* PSADMIN_TSK_H */
