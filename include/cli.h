#ifndef PSADMIN_CLI_H
#define PSADMIN_CLI_H

#include <sys/types.h>
#include "common.h"

/* Buffer circular simple de salida del PTY, para que viz pueda dibujar
 * "lo último que imprimió bash" sin que cli dependa del renderer. */
typedef struct {
    char data[8192];
    size_t len; /* bytes válidos en data, siempre <= sizeof(data) */
} cli_outbuf_t;

typedef struct {
    int master_fd;   /* extremo maestro del PTY, el padre lee/escribe aquí */
    pid_t child_pid;  /* pid del bash hijo */
    cli_outbuf_t out;
    int running;      /* 0 si el hijo terminó */
} cli_state_t;

/* --- ciclo de vida --- */

/* Crea el PTY, hace fork y ejecuta /bin/bash interactivo en el hijo.
 * Retorna 0 en éxito. El padre queda con master_fd listo para leer/escribir. */
int cli_spawn(cli_state_t *st);

/* Cierra el master_fd y espera (no bloqueante si el hijo ya murió) */
void cli_close(cli_state_t *st);

/* --- E/S --- */

/* Escribe bytes hacia el PTY (lo que el usuario tipeó) */
ssize_t cli_write_input(cli_state_t *st, const char *buf, size_t len);

/* Lee lo disponible del PTY sin bloquear y lo apila en st->out
 * (descartando lo más viejo si se llena el buffer). Retorna bytes leídos,
 * 0 si no había nada, -1 en error real. Marca st->running=0 si el hijo
 * cerró el extremo (EOF). */
ssize_t cli_poll_output(cli_state_t *st);

/* --- lógica pura testeable --- */

/* Agrega 'len' bytes de 'data' al buffer circular 'out', recortando el
 * contenido más antiguo si no entra. Función pura, sin syscalls. */
void cli_outbuf_append(cli_outbuf_t *out, const char *data, size_t len);

#endif /* PSADMIN_CLI_H */
