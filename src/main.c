/* PSAdmin - herramienta de administración de Linux en C
 *
 * Integra:
 *   shl (PSShell)    - explorador de archivos + respaldo automático
 *   tsk (AdminTasks) - procesos, stats, señales, árbol
 *   cli (PSCLI)      - consola embebida real (PTY + bash)
 *   con (PSCon)      - análisis de scripts bash (variables/ciclos)
 *   viz (Visualizer) - raw ANSI + layout de 4 paneles
 *
 * Controles:
 *   Tab        - rotar foco entre paneles (tsk -> con -> shl -> cli -> tsk)
 *   j/k o flechas arriba/abajo - moverse dentro del panel con foco
 *                (tsk: selecciona proceso, shl: selecciona archivo/dir)
 *   Enter      - en shl: entrar a directorio seleccionado / en tsk: ver árbol
 *   a          - en shl: analizar el archivo bash seleccionado (-> panel con)
 *   b          - en shl: respaldar (backup) la entrada seleccionada
 *   k (en tsk) - SIGTERM al proceso seleccionado   [tecla 'x' para no chocar con navegación]
 *   x          - en tsk: terminar proceso (SIGTERM)
 *   X          - en tsk: forzar terminación (SIGKILL)
 *   s          - en tsk: suspender (SIGSTOP)
 *   r          - en tsk: reanudar (SIGCONT)
 *   /          - en tsk: buscar por nombre
 *   q          - salir
 *   (cuando el foco está en cli, todo el teclado se reenvía al bash real)
 */

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "cli.h"
#include "common.h"
#include "ccon.h"
#include "shl.h"
#include "tsk.h"
#include "viz.h"

typedef struct {
    shl_state_t shl;
    tsk_state_list_t tsk;
    cli_state_t cli;
    con_result_t con;
    char con_source_name[PS_NAME_MAX]; /* nombre del último script analizado */
    char status_msg[PS_LINE_MAX];      /* mensaje de feedback (errores, backups, etc) */
    ps_panel_id_t focus;
    ps_rect_t panels[PANEL_COUNT];
    char search_buf[PS_NAME_MAX];
    int searching; /* 1 mientras se está escribiendo una búsqueda en tsk */
} app_state_t;

static volatile int g_resized = 0;

static void on_sigwinch(int sig) {
    (void)sig;
    g_resized = 1;
}

static void get_term_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    } else {
        *rows = 24;
        *cols = 80;
    }
}

static const char *panel_title(ps_panel_id_t id) {
    switch (id) {
        case PANEL_TSK: return "tsk";
        case PANEL_CON: return "con";
        case PANEL_SHL: return "shl";
        case PANEL_CLI: return "cli";
        default: return "?";
    }
}

/* --- render de cada panel --- */

static void render_tsk(app_state_t *app) {
    ps_rect_t *r = &app->panels[PANEL_TSK];
    viz_draw_panel_frame(r, "tsk", app->focus == PANEL_TSK);

    tsk_sysinfo_t info;
    tsk_sysinfo(&info);

    char line[PS_LINE_MAX];
    snprintf(line, sizeof(line), "CPU:%5.1f%% MEM:%ld/%ldMB", info.cpu_usage_percent,
              (info.mem_total_kb - info.mem_available_kb) / 1024, info.mem_total_kb / 1024);
    viz_draw_panel_line(r, 0, line);
    viz_draw_panel_line(r, 1, "----------------------");

    int visible_rows = r->height - 4; /* header(2) + bordes */
    int start = 0;
    if ((int)app->tsk.selected >= visible_rows) start = app->tsk.selected - visible_rows + 1;

    for (int i = 0; i < visible_rows; i++) {
        size_t idx = (size_t)(start + i);
        if (idx >= app->tsk.count) {
            viz_draw_panel_line(r, 2 + i, "");
            continue;
        }
        tsk_proc_t *p = &app->tsk.procs[idx];
        char mark = ((int)idx == app->tsk.selected) ? '>' : ' ';
        char state_c = '?';
        switch (p->state) {
            case TSK_RUNNING: state_c = 'R'; break;
            case TSK_SLEEPING: state_c = 'S'; break;
            case TSK_STOPPED: state_c = 'T'; break;
            case TSK_ZOMBIE: state_c = 'Z'; break;
            default: state_c = '?'; break;
        }
        snprintf(line, sizeof(line), "%c%6d %c %5.1f%% %s", mark, p->pid, state_c,
                  p->cpu_percent, p->comm);
        viz_draw_panel_line(r, 2 + i, line);
    }
}

static void render_shl(app_state_t *app) {
    ps_rect_t *r = &app->panels[PANEL_SHL];
    viz_draw_panel_frame(r, "shl", app->focus == PANEL_SHL);

    viz_draw_panel_line(r, 0, app->shl.cwd);
    viz_draw_panel_line(r, 1, "----------------------");

    int visible_rows = r->height - 4;
    int start = 0;
    if (app->shl.selected >= visible_rows) start = app->shl.selected - visible_rows + 1;

    char line[PS_LINE_MAX];
    for (int i = 0; i < visible_rows; i++) {
        int idx = start + i;
        if (idx >= (int)app->shl.count) {
            viz_draw_panel_line(r, 2 + i, "");
            continue;
        }
        shl_entry_t *e = &app->shl.entries[idx];
        char mark = (idx == app->shl.selected) ? '>' : ' ';
        char type_c = (e->type == SHL_ENTRY_DIR) ? '/' : ' ';
        snprintf(line, sizeof(line), "%c%s%c", mark, e->name, type_c);
        viz_draw_panel_line(r, 2 + i, line);
    }
}

static void render_con(app_state_t *app) {
    ps_rect_t *r = &app->panels[PANEL_CON];
    viz_draw_panel_frame(r, "con", app->focus == PANEL_CON);

    char header[PS_LINE_MAX];
    if (app->con_source_name[0]) {
        snprintf(header, sizeof(header), "analisis: %s (%zu hallazgos)",
                  app->con_source_name, app->con.count);
    } else {
        snprintf(header, sizeof(header), "[shl] selecciona un .sh y presiona 'a'");
    }
    viz_draw_panel_line(r, 0, header);
    viz_draw_panel_line(r, 1, "----------------------");

    int visible_rows = r->height - 4;
    char line[PS_LINE_MAX];
    for (int i = 0; i < visible_rows; i++) {
        if ((size_t)i >= app->con.count) {
            viz_draw_panel_line(r, 2 + i, "");
            continue;
        }
        con_finding_t *f = &app->con.items[i];
        snprintf(line, sizeof(line), "L%-4d [%s] %s", f->line_no,
                  con_kind_label(f->kind), f->text);
        viz_draw_panel_line(r, 2 + i, line);
    }
}

static void render_cli_static_frame(app_state_t *app) {
    /* Solo el marco; el contenido real del PTY se vuelca aparte porque
     * trae sus propios códigos ANSI (colores de bash, prompt, etc.) y no
     * conviene re-procesarlo línea por línea como los otros paneles. */
    ps_rect_t *r = &app->panels[PANEL_CLI];
    viz_draw_panel_frame(r, "cli", app->focus == PANEL_CLI);
}

/* Vuelca el buffer de salida del PTY dentro del área interior del panel cli,
 * tal cual (pass-through de ANSI), respetando los márgenes del panel. */
static void render_cli_content(app_state_t *app) {
    ps_rect_t *r = &app->panels[PANEL_CLI];
    if (app->cli.out.len == 0) return;

    /* Posicionamos el cursor en la esquina interior y dejamos que el
     * propio bash maneje saltos de línea / colores; para no salirnos del
     * panel hacia la derecha, esto es "best effort" en raw ANSI puro. */
    viz_move_cursor(r->row + 1, r->col + 1);
    fwrite(app->cli.out.data, 1, app->cli.out.len, stdout);
    fflush(stdout);
}

static void render_status_bar(app_state_t *app, int term_rows, int term_cols) {
    viz_move_cursor(term_rows, 1);
    char line[PS_LINE_MAX];
    if (app->searching) {
        snprintf(line, sizeof(line), "/%s", app->search_buf);
    } else if (app->status_msg[0]) {
        snprintf(line, sizeof(line), "%s", app->status_msg);
    } else {
        snprintf(line, sizeof(line),
                  "Tab: cambiar panel | j/k: mover | q: salir | (shl) a:analizar b:backup Enter:abrir | (tsk) x/X:kill s:stop r:cont /:buscar");
    }
    int len = (int)strlen(line);
    if (len > term_cols) len = term_cols;
    fwrite(line, 1, (size_t)len, stdout);
    for (int i = len; i < term_cols; i++) putchar(' ');
    fflush(stdout);
}

static void full_render(app_state_t *app, int term_rows, int term_cols) {
    viz_clear_screen();
    render_tsk(app);
    render_con(app);
    render_shl(app);
    render_cli_static_frame(app);
    render_cli_content(app);
    render_status_bar(app, term_rows, term_cols);
}

/* --- acciones de alto nivel disparadas por teclado --- */

static void action_analyze_selected(app_state_t *app) {
    char path[PS_PATH_MAX];
    if (shl_selected_path(&app->shl, path, sizeof(path)) != 0) {
        snprintf(app->status_msg, sizeof(app->status_msg), "shl: nada seleccionado");
        return;
    }
    con_result_free(&app->con);
    if (con_analyze_file(path, &app->con) != 0) {
        snprintf(app->status_msg, sizeof(app->status_msg), "con: no se pudo abrir %s", path);
        return;
    }
    strncpy(app->con_source_name, app->shl.entries[app->shl.selected].name,
             sizeof(app->con_source_name) - 1);
    snprintf(app->status_msg, sizeof(app->status_msg), "con: analizado %s", path);
}

static void action_backup_selected(app_state_t *app) {
    char out_path[PS_PATH_MAX];
    if (shl_backup_selected(&app->shl, NULL, out_path, sizeof(out_path)) == 0) {
        snprintf(app->status_msg, sizeof(app->status_msg), "shl: backup creado en %s", out_path);
    } else {
        snprintf(app->status_msg, sizeof(app->status_msg), "shl: error al crear backup");
    }
}

static void action_shl_enter(app_state_t *app) {
    if (app->shl.count == 0) return;
    shl_entry_t *e = &app->shl.entries[app->shl.selected];
    if (e->type == SHL_ENTRY_DIR) {
        shl_chdir(&app->shl, e->name);
    }
}

static void action_tsk_signal(app_state_t *app, char key) {
    if (app->tsk.count == 0) return;
    pid_t pid = app->tsk.procs[app->tsk.selected].pid;
    int rc = -1;
    const char *what = "";
    switch (key) {
        case 'x': rc = tsk_kill(pid); what = "SIGTERM"; break;
        case 'X': rc = tsk_force_kill(pid); what = "SIGKILL"; break;
        case 's': rc = tsk_suspend(pid); what = "SIGSTOP"; break;
        case 'r': rc = tsk_resume(pid); what = "SIGCONT"; break;
        default: return;
    }
    if (rc == 0) {
        snprintf(app->status_msg, sizeof(app->status_msg), "tsk: %s -> pid %d ok", what, pid);
    } else {
        snprintf(app->status_msg, sizeof(app->status_msg),
                  "tsk: %s -> pid %d FALLO (permiso?)", what, pid);
    }
}

/* --- manejo de teclado --- */

/* Lee hasta 'maxlen' bytes ya disponibles en stdin (no bloqueante) y los
 * coloca en buf. Retorna la cantidad leída (puede ser 0). */
static ssize_t read_stdin_nonblock(char *buf, size_t maxlen) {
    return read(STDIN_FILENO, buf, maxlen);
}

static void handle_key_global_panel(app_state_t *app, char c) {
    if (app->searching) {
        if (c == '\n' || c == '\r') {
            const tsk_proc_t *matches[64];
            size_t n = tsk_search(&app->tsk, app->search_buf, matches, 64);
            if (n > 0) {
                for (size_t i = 0; i < app->tsk.count; i++) {
                    if (&app->tsk.procs[i] == matches[0]) { app->tsk.selected = (int)i; break; }
                }
                snprintf(app->status_msg, sizeof(app->status_msg), "tsk: %zu coincidencia(s)", n);
            } else {
                snprintf(app->status_msg, sizeof(app->status_msg), "tsk: sin coincidencias");
            }
            app->searching = 0;
        } else if (c == 27) {
            app->searching = 0;
        } else if (c == 127 || c == 8) {
            size_t l = strlen(app->search_buf);
            if (l > 0) app->search_buf[l - 1] = '\0';
        } else if (c >= 32 && c < 127) {
            size_t l = strlen(app->search_buf);
            if (l + 1 < sizeof(app->search_buf)) {
                app->search_buf[l] = c;
                app->search_buf[l + 1] = '\0';
            }
        }
        return;
    }

    switch (app->focus) {
        case PANEL_TSK:
            if (c == 'j') tsk_move_selection(&app->tsk, 1);
            else if (c == 'k') tsk_move_selection(&app->tsk, -1);
            else if (c == 'x' || c == 'X' || c == 's' || c == 'r') action_tsk_signal(app, c);
            else if (c == '/') { app->searching = 1; app->search_buf[0] = '\0'; }
            break;
        case PANEL_SHL:
            if (c == 'j') shl_move_selection(&app->shl, 1);
            else if (c == 'k') shl_move_selection(&app->shl, -1);
            else if (c == '\n' || c == '\r') action_shl_enter(app);
            else if (c == 'a') action_analyze_selected(app);
            else if (c == 'b') action_backup_selected(app);
            break;
        case PANEL_CON:
            /* por ahora sólo lectura; espacio para scroll futuro */
            break;
        case PANEL_CLI:
            /* se maneja aparte: todo byte se reenvía al PTY */
            break;
        default:
            break;
    }
}

int main(void) {
    app_state_t app;
    memset(&app, 0, sizeof(app));
    app.focus = PANEL_TSK;

    if (shl_init(&app.shl, NULL) != 0) {
        fprintf(stderr, "psadmin: no se pudo inicializar shl (HOME inaccesible)\n");
        return 1;
    }
    if (tsk_init(&app.tsk) != 0) {
        fprintf(stderr, "psadmin: no se pudo inicializar tsk (/proc inaccesible)\n");
        return 1;
    }
    con_result_init(&app.con);
    if (cli_spawn(&app.cli) != 0) {
        fprintf(stderr, "psadmin: no se pudo crear el PTY para bash\n");
        return 1;
    }

    char saved_termios[256]; /* >= sizeof(struct termios) en cualquier libc común */
    if (viz_termios_size() > sizeof(saved_termios)) {
        fprintf(stderr, "psadmin: buffer de termios insuficiente\n");
        return 1;
    }
    if (viz_enable_raw_mode(saved_termios) != 0) {
        fprintf(stderr, "psadmin: no se pudo poner la terminal en raw mode\n");
        return 1;
    }

    signal(SIGWINCH, on_sigwinch);

    int term_rows, term_cols;
    get_term_size(&term_rows, &term_cols);
    viz_compute_layout(term_rows, term_cols, app.panels);
    full_render(&app, term_rows, term_cols);

    time_t last_refresh = time(NULL);
    int quit = 0;

    while (!quit) {
        if (g_resized) {
            g_resized = 0;
            get_term_size(&term_rows, &term_cols);
            viz_compute_layout(term_rows, term_cols, app.panels);
            full_render(&app, term_rows, term_cols);
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        int maxfd = STDIN_FILENO;
        if (app.cli.running && app.cli.master_fd >= 0) {
            FD_SET(app.cli.master_fd, &rfds);
            if (app.cli.master_fd > maxfd) maxfd = app.cli.master_fd;
        }

        struct timeval tv = {.tv_sec = 0, .tv_usec = 200000}; /* 200ms tick */
        int rv = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        int need_redraw = 0;

        if (rv > 0 && FD_ISSET(STDIN_FILENO, &rfds)) {
            char buf[256];
            ssize_t n = read_stdin_nonblock(buf, sizeof(buf));
            for (ssize_t i = 0; i < n; i++) {
                char c = buf[i];
                if (app.focus == PANEL_CLI) {
                    cli_write_input(&app.cli, &c, 1);
                    continue;
                }
                if (c == '\t') {
                    app.focus = (ps_panel_id_t)((app.focus + 1) % PANEL_COUNT);
                    need_redraw = 1;
                } else if (c == 'q' && !app.searching) {
                    quit = 1;
                } else {
                    handle_key_global_panel(&app, c);
                    need_redraw = 1;
                }
            }
        }

        if (rv > 0 && app.cli.running && FD_ISSET(app.cli.master_fd, &rfds)) {
            cli_poll_output(&app.cli);
            need_redraw = 1;
        }

        /* refresco periódico de stats (cada ~1s) */
        time_t now = time(NULL);
        if (now != last_refresh) {
            tsk_refresh(&app.tsk);
            last_refresh = now;
            need_redraw = 1;
        }

        if (need_redraw && !quit) {
            full_render(&app, term_rows, term_cols);
        }
    }

    viz_disable_raw_mode(saved_termios);
    viz_clear_screen();
    viz_move_cursor(1, 1);

    cli_close(&app.cli);
    con_result_free(&app.con);
    tsk_free(&app.tsk);
    shl_free(&app.shl);

    printf("psadmin: saliendo.\n");
    return 0;
}
