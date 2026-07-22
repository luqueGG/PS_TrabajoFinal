#include "cli.h"

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

void cli_outbuf_append(cli_outbuf_t *out, const char *data, size_t len) {
    if (len >= sizeof(out->data)) {
        /* el dato nuevo ya es más grande que el buffer: nos quedamos con
         * la cola (lo más reciente) */
        data += (len - sizeof(out->data));
        len = sizeof(out->data);
        memcpy(out->data, data, len);
        out->len = len;
        return;
    }

    size_t space = sizeof(out->data) - out->len;
    if (len > space) {
        /* descartamos del frente lo necesario para hacer lugar */
        size_t to_drop = len - space;
        memmove(out->data, out->data + to_drop, out->len - to_drop);
        out->len -= to_drop;
    }
    memcpy(out->data + out->len, data, len);
    out->len += len;
}

int cli_spawn(cli_state_t *st) {
    memset(st, 0, sizeof(*st));

    pid_t pid = forkpty(&st->master_fd, NULL, NULL, NULL);
    if (pid < 0) return -1;

    if (pid == 0) {
        /* hijo: se convierte en bash interactivo */
        setenv("TERM", "xterm-256color", 1);
        execlp("bash", "bash", "--noprofile", "--norc", "-i", NULL);
        _exit(127);
    }

    st->child_pid = pid;
    st->running = 1;

    /* master_fd en modo no bloqueante para poder hacer poll desde el loop
     * principal sin trabarse */
    int flags = fcntl(st->master_fd, F_GETFL, 0);
    fcntl(st->master_fd, F_SETFL, flags | O_NONBLOCK);

    return 0;
}

void cli_close(cli_state_t *st) {
    if (st->master_fd >= 0) {
        close(st->master_fd);
        st->master_fd = -1;
    }
    if (st->child_pid > 0) {
        kill(st->child_pid, SIGHUP);
        int status;
        waitpid(st->child_pid, &status, WNOHANG);
        st->child_pid = -1;
    }
    st->running = 0;
}

ssize_t cli_write_input(cli_state_t *st, const char *buf, size_t len) {
    if (!st->running || st->master_fd < 0) return -1;
    return write(st->master_fd, buf, len);
}

ssize_t cli_poll_output(cli_state_t *st) {
    if (!st->running || st->master_fd < 0) return -1;

    char chunk[4096];
    ssize_t n = read(st->master_fd, chunk, sizeof(chunk));
    if (n > 0) {
        cli_outbuf_append(&st->out, chunk, (size_t)n);
        return n;
    }
    if (n == 0) {
        st->running = 0;
        return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0; /* nada disponible, no es error */
    }
    if (errno == EIO) {
        /* típico cuando el hijo (bash) terminó y cerró el PTY */
        st->running = 0;
        return 0;
    }
    return -1;
}
