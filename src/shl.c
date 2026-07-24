#include "shl.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>

static int shl_grow(shl_state_t *st) {
    size_t new_cap = st->capacity == 0 ? 64 : st->capacity * 2;
    shl_entry_t *tmp = realloc(st->entries, new_cap * sizeof(shl_entry_t));
    if (!tmp) return -1;
    st->entries = tmp;
    st->capacity = new_cap;
    return 0;
}

static shl_entry_type_t shl_type_from_mode(mode_t m) {
    if (S_ISDIR(m)) return SHL_ENTRY_DIR;
    if (S_ISLNK(m)) return SHL_ENTRY_LINK;
    if (S_ISREG(m)) return SHL_ENTRY_FILE;
    return SHL_ENTRY_OTHER;
}

/* Comparador: directorios antes que archivos, luego orden alfabético.
 * Expuesto sin "static" implícito vía qsort callback. */
static int shl_cmp_entries(const void *a, const void *b) {
    const shl_entry_t *ea = (const shl_entry_t *)a;
    const shl_entry_t *eb = (const shl_entry_t *)b;
    int da = (ea->type == SHL_ENTRY_DIR) ? 0 : 1;
    int db = (eb->type == SHL_ENTRY_DIR) ? 0 : 1;
    if (da != db) return da - db;
    return strcmp(ea->name, eb->name);
}

int shl_init(shl_state_t *st, const char *start_path) {
    memset(st, 0, sizeof(*st));
    const char *path = start_path;
    if (!path) {
        path = getenv("HOME");
        if (!path) {
            struct passwd *pw = getpwuid(getuid());
            path = pw ? pw->pw_dir : "/";
        }
    }
    strncpy(st->cwd, path, sizeof(st->cwd) - 1);
    st->selected = 0;
    return shl_reload(st);
}

void shl_free(shl_state_t *st) {
    free(st->entries);
    st->entries = NULL;
    st->count = 0;
    st->capacity = 0;
}

int shl_reload(shl_state_t *st) {
    DIR *d = opendir(st->cwd);
    if (!d) return -1;

    st->count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0) continue;
        /* mostramos ".." salvo en "/" */
        if (strcmp(de->d_name, "..") == 0 && strcmp(st->cwd, "/") == 0) continue;

        if (st->count >= st->capacity) {
            if (shl_grow(st) != 0) {
                closedir(d);
                return -1;
            }
        }

        shl_entry_t *e = &st->entries[st->count];
        memset(e, 0, sizeof(*e));
        strncpy(e->name, de->d_name, sizeof(e->name) - 1);

        char full[PS_PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", st->cwd, de->d_name);
        struct stat sb;
        if (lstat(full, &sb) == 0) {
            e->type = shl_type_from_mode(sb.st_mode);
            e->size = sb.st_size;
            e->mode = sb.st_mode;
            e->mtime = sb.st_mtime;
        } else {
            e->type = SHL_ENTRY_OTHER;
        }
        st->count++;
    }
    closedir(d);

    qsort(st->entries, st->count, sizeof(shl_entry_t), shl_cmp_entries);
    if (st->selected >= (int)st->count) st->selected = st->count > 0 ? (int)st->count - 1 : 0;
    if (st->selected < 0) st->selected = 0;
    return 0;
}

int shl_chdir(shl_state_t *st, const char *target) {
    char next[PS_PATH_MAX];
    if (strcmp(target, "..") == 0) {
        strncpy(next, st->cwd, sizeof(next) - 1);
        next[sizeof(next) - 1] = '\0';
        char *slash = strrchr(next, '/');
        if (slash && slash != next) {
            *slash = '\0';
        } else if (slash == next) {
            next[1] = '\0'; /* raíz */
        }
    } else {
        snprintf(next, sizeof(next), "%s/%s", st->cwd, target);
    }

    DIR *d = opendir(next);
    if (!d) return -1;
    closedir(d);

    strncpy(st->cwd, next, sizeof(st->cwd) - 1);
    st->cwd[sizeof(st->cwd) - 1] = '\0';
    st->selected = 0;
    return shl_reload(st);
}

int shl_selected_path(const shl_state_t *st, char *out, size_t out_sz) {
    if (st->count == 0 || st->selected < 0 || st->selected >= (int)st->count) return -1;
    snprintf(out, out_sz, "%s/%s", st->cwd, st->entries[st->selected].name);
    return 0;
}

void shl_move_selection(shl_state_t *st, int delta) {
    if (st->count == 0) {
        st->selected = 0;
        return;
    }
    int next = st->selected + delta;
    if (next < 0) next = 0;
    if (next >= (int)st->count) next = (int)st->count - 1;
    st->selected = next;
}

static int shl_remove_recursive(const char *path) {
    struct stat sb;
    if (lstat(path, &sb) != 0) return -1;

    if (!S_ISDIR(sb.st_mode)) {
        return unlink(path) == 0 ? 0 : -1;
    }

    DIR *d = opendir(path);
    if (!d) return -1;

    int rc = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        char child[PS_PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
        if (shl_remove_recursive(child) != 0) rc = -1;
    }
    closedir(d);

    if (rmdir(path) != 0) rc = -1;
    return rc;
}

int shl_delete_selected(shl_state_t *st) {
    if (st->count == 0 || st->selected < 0 || st->selected >= (int)st->count) return -1;

    const shl_entry_t *e = &st->entries[st->selected];
    if (strcmp(e->name, "..") == 0) return -1;

    char path[PS_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", st->cwd, e->name);

    if (shl_remove_recursive(path) != 0) return -1;

    return shl_reload(st);
}

void shl_build_backup_name(const char *base_name, time_t now, char *out, size_t out_sz) {
    struct tm tmv;
    localtime_r(&now, &tmv);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tmv);
    snprintf(out, out_sz, "%s_%s.tar.gz", base_name, stamp);
}

int shl_backup_selected(const shl_state_t *st, const char *dest_dir,
                         char *out_backup_path, size_t out_sz) {
    char src_path[PS_PATH_MAX];
    if (shl_selected_path(st, src_path, sizeof(src_path)) != 0) return -1;

    char dest[PS_PATH_MAX];
    if (dest_dir) {
        strncpy(dest, dest_dir, sizeof(dest) - 1);
        dest[sizeof(dest) - 1] = '\0';
    } else {
        const char *home = getenv("HOME");
        snprintf(dest, sizeof(dest), "%s/.psadmin_backups", home ? home : "/tmp");
    }

    /* crea el directorio destino si no existe (equivalente a mkdir -p de 1 nivel) */
    struct stat sb;
    if (stat(dest, &sb) != 0) {
        if (mkdir(dest, 0755) != 0 && errno != EEXIST) return -1;
    }

    const char *base_name = st->entries[st->selected].name;
    char backup_name[PS_NAME_MAX + 32];
    shl_build_backup_name(base_name, time(NULL), backup_name, sizeof(backup_name));

    char full_backup_path[PS_PATH_MAX];
    snprintf(full_backup_path, sizeof(full_backup_path), "%s/%s", dest, backup_name);

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* hijo: tar -czf <dest> -C <cwd> <nombre> */
        execlp("tar", "tar", "-czf", full_backup_path, "-C", st->cwd, base_name, NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return -1;

    if (out_backup_path) {
        strncpy(out_backup_path, full_backup_path, out_sz - 1);
        out_backup_path[out_sz - 1] = '\0';
    }
    return 0;
}
