#include "tsk.h"

#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tsk_grow(tsk_state_list_t *st) {
    size_t new_cap = st->capacity == 0 ? 128 : st->capacity * 2;
    tsk_proc_t *tmp = realloc(st->procs, new_cap * sizeof(tsk_proc_t));
    if (!tmp) return -1;
    st->procs = tmp;
    st->capacity = new_cap;
    return 0;
}

int tsk_init(tsk_state_list_t *st) {
    memset(st, 0, sizeof(*st));
    return tsk_refresh(st);
}

void tsk_free(tsk_state_list_t *st) {
    free(st->procs);
    st->procs = NULL;
    st->count = 0;
    st->capacity = 0;
}

static tsk_state_t tsk_state_from_char(char c) {
    switch (c) {
        case 'R': return TSK_RUNNING;
        case 'S': case 'D': return TSK_SLEEPING;
        case 'T': case 't': return TSK_STOPPED;
        case 'Z': return TSK_ZOMBIE;
        default: return TSK_OTHER;
    }
}

/* Parsea una línea completa de /proc/[pid]/stat. El campo comm está entre
 * paréntesis y puede contener espacios, por eso se busca '(' y el último ')'. */
int tsk_parse_stat_line(const char *line, tsk_proc_t *out) {
    if (!line || !out) return -1;
    memset(out, 0, sizeof(*out));

    const char *open_paren = strchr(line, '(');
    const char *close_paren = strrchr(line, ')');
    if (!open_paren || !close_paren || close_paren < open_paren) return -1;

    /* pid: todo lo anterior a '(' */
    out->pid = (pid_t)atol(line);

    size_t comm_len = (size_t)(close_paren - open_paren - 1);
    if (comm_len >= sizeof(out->comm)) comm_len = sizeof(out->comm) - 1;
    memcpy(out->comm, open_paren + 1, comm_len);
    out->comm[comm_len] = '\0';

    /* resto de campos después de ") ", separados por espacio.
     * Campo 1 (post-comm) = state, 2 = ppid, ... 13=utime, 14=stime */
    char rest[PS_LINE_MAX];
    strncpy(rest, close_paren + 2, sizeof(rest) - 1);
    rest[sizeof(rest) - 1] = '\0';

    char *tok = strtok(rest, " ");
    int field = 1; /* arrancamos en "state" = campo lógico 3 del /proc/stat completo */
    char state_c = 'O';
    pid_t ppid = 0;
    unsigned long utime = 0, stime = 0;

    while (tok) {
        switch (field) {
            case 1: state_c = tok[0]; break;
            case 2: ppid = (pid_t)atol(tok); break;
            case 12: utime = strtoul(tok, NULL, 10); break;
            case 13: stime = strtoul(tok, NULL, 10); break;
            default: break;
        }
        tok = strtok(NULL, " ");
        field++;
        if (field > 13) break;
    }

    out->state = tsk_state_from_char(state_c);
    out->ppid = ppid;
    out->utime = utime;
    out->stime = stime;
    return 0;
}

static long tsk_read_rss_kb(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[PS_LINE_MAX];
    long rss = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &rss);
            break;
        }
    }
    fclose(f);
    return rss;
}

static unsigned long long tsk_read_total_jiffies(void) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;
    char label[16];
    unsigned long long user, nice, system_, idle, iowait, irq, softirq, steal;
    unsigned long long total = 0;
    if (fscanf(f, "%15s %llu %llu %llu %llu %llu %llu %llu %llu", label, &user,
               &nice, &system_, &idle, &iowait, &irq, &softirq, &steal) == 9) {
        total = user + nice + system_ + idle + iowait + irq + softirq + steal;
    }
    fclose(f);
    return total;
}

int tsk_refresh(tsk_state_list_t *st) {
    /* guardamos muestra previa para calcular %cpu por delta de jiffies */
    tsk_proc_t *prev = st->procs;
    size_t prev_count = st->count;
    unsigned long long prev_total = st->last_total_jiffies;

    tsk_proc_t *new_procs = NULL;
    size_t new_count = 0;
    size_t new_cap = 0;

    DIR *d = opendir("/proc");
    if (!d) return -1;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!isdigit((unsigned char)de->d_name[0])) continue;

        char path[64];
        snprintf(path, sizeof(path), "/proc/%s/stat", de->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char line[PS_LINE_MAX];
        if (!fgets(line, sizeof(line), f)) {
            fclose(f);
            continue;
        }
        fclose(f);

        tsk_proc_t p;
        if (tsk_parse_stat_line(line, &p) != 0) continue;
        p.rss_kb = tsk_read_rss_kb(p.pid);

        if (new_count >= new_cap) {
            size_t cap2 = new_cap == 0 ? 128 : new_cap * 2;
            tsk_proc_t *tmp = realloc(new_procs, cap2 * sizeof(tsk_proc_t));
            if (!tmp) { closedir(d); free(new_procs); return -1; }
            new_procs = tmp;
            new_cap = cap2;
        }
        new_procs[new_count++] = p;
    }
    closedir(d);

    unsigned long long total = tsk_read_total_jiffies();
    unsigned long long total_delta = (prev_total > 0 && total > prev_total)
                                          ? (total - prev_total) : 0;

    if (total_delta > 0 && prev) {
        for (size_t i = 0; i < new_count; i++) {
            for (size_t j = 0; j < prev_count; j++) {
                if (prev[j].pid == new_procs[i].pid) {
                    unsigned long du = new_procs[i].utime - prev[j].utime;
                    unsigned long ds = new_procs[i].stime - prev[j].stime;
                    new_procs[i].cpu_percent =
                        100.0 * (double)(du + ds) / (double)total_delta;
                    break;
                }
            }
        }
    }

    free(st->procs);
    st->procs = new_procs;
    st->count = new_count;
    st->capacity = new_cap;
    st->last_total_jiffies = total;
    if (st->selected >= (int)st->count) st->selected = st->count > 0 ? (int)st->count - 1 : 0;
    if (st->selected < 0) st->selected = 0;
    return 0;
}

size_t tsk_search(const tsk_state_list_t *st, const char *needle,
                   const tsk_proc_t **matches, size_t max_matches) {
    size_t found = 0;
    char lower_needle[PS_NAME_MAX];
    size_t i;
    for (i = 0; needle[i] && i < sizeof(lower_needle) - 1; i++)
        lower_needle[i] = (char)tolower((unsigned char)needle[i]);
    lower_needle[i] = '\0';

    for (size_t k = 0; k < st->count && found < max_matches; k++) {
        char lower_comm[PS_NAME_MAX];
        size_t j;
        for (j = 0; st->procs[k].comm[j] && j < sizeof(lower_comm) - 1; j++)
            lower_comm[j] = (char)tolower((unsigned char)st->procs[k].comm[j]);
        lower_comm[j] = '\0';

        if (strstr(lower_comm, lower_needle) != NULL) {
            matches[found++] = &st->procs[k];
        }
    }
    return found;
}

int tsk_sysinfo(tsk_sysinfo_t *info) {
    memset(info, 0, sizeof(*info));
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    char key[64];
    long value;
    char unit[16];
    while (fscanf(f, "%63s %ld %15s", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) info->mem_total_kb = value;
        else if (strcmp(key, "MemFree:") == 0) info->mem_free_kb = value;
        else if (strcmp(key, "MemAvailable:") == 0) info->mem_available_kb = value;
    }
    fclose(f);

    static unsigned long long prev_idle = 0, prev_total = 0;
    FILE *fs = fopen("/proc/stat", "r");
    if (fs) {
        char label[16];
        unsigned long long user, nice, system_, idle, iowait, irq, softirq, steal;
        if (fscanf(fs, "%15s %llu %llu %llu %llu %llu %llu %llu %llu", label,
                   &user, &nice, &system_, &idle, &iowait, &irq, &softirq,
                   &steal) == 9) {
            unsigned long long total = user + nice + system_ + idle + iowait +
                                        irq + softirq + steal;
            unsigned long long idle_all = idle + iowait;
            if (prev_total > 0 && total > prev_total) {
                unsigned long long dt = total - prev_total;
                unsigned long long di = idle_all - prev_idle;
                info->cpu_usage_percent = 100.0 * (double)(dt - di) / (double)dt;
            }
            prev_total = total;
            prev_idle = idle_all;
        }
        fclose(fs);
    }
    return 0;
}

int tsk_kill(pid_t pid) { return kill(pid, SIGTERM); }
int tsk_force_kill(pid_t pid) { return kill(pid, SIGKILL); }
int tsk_suspend(pid_t pid) { return kill(pid, SIGSTOP); }
int tsk_resume(pid_t pid) { return kill(pid, SIGCONT); }

void tsk_move_selection(tsk_state_list_t *st, int delta) {
    if (st->count == 0) { st->selected = 0; return; }
    int next = st->selected + delta;
    if (next < 0) next = 0;
    if (next >= (int)st->count) next = (int)st->count - 1;
    st->selected = next;
}

static void tsk_tree_visit(const tsk_state_list_t *st, pid_t pid, int depth,
                            pid_t *out, int *depth_out, size_t *n, size_t max_out) {
    if (*n >= max_out) return;
    out[*n] = pid;
    depth_out[*n] = depth;
    (*n)++;
    for (size_t i = 0; i < st->count; i++) {
        if (st->procs[i].ppid == pid && st->procs[i].pid != pid) {
            tsk_tree_visit(st, st->procs[i].pid, depth + 1, out, depth_out, n, max_out);
        }
    }
}

size_t tsk_build_tree(const tsk_state_list_t *st, pid_t root,
                       pid_t *out, int *depth, size_t max_out) {
    size_t n = 0;
    if (root == 0) root = 1;
    tsk_tree_visit(st, root, 0, out, depth, &n, max_out);
    return n;
}
