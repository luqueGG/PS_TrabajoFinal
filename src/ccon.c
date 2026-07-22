#include "ccon.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void con_result_init(con_result_t *res) {
    memset(res, 0, sizeof(*res));
}

void con_result_free(con_result_t *res) {
    free(res->items);
    res->items = NULL;
    res->count = 0;
    res->capacity = 0;
}

int con_result_add(con_result_t *res, con_find_kind_t kind, int line_no,
                    const char *text) {
    if (res->count >= res->capacity) {
        size_t new_cap = res->capacity == 0 ? 32 : res->capacity * 2;
        con_finding_t *tmp = realloc(res->items, new_cap * sizeof(con_finding_t));
        if (!tmp) return -1;
        res->items = tmp;
        res->capacity = new_cap;
    }
    con_finding_t *f = &res->items[res->count++];
    f->kind = kind;
    f->line_no = line_no;
    strncpy(f->text, text, sizeof(f->text) - 1);
    f->text[sizeof(f->text) - 1] = '\0';
    return 0;
}

const char *con_kind_label(con_find_kind_t kind) {
    switch (kind) {
        case CON_FIND_VARIABLE: return "variable";
        case CON_FIND_LOOP: return "ciclo";
        default: return "desconocido";
    }
}

static int con_is_identifier_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/* ¿La línea arranca con una keyword de control que NO es asignación?
 * Evita falsos positivos como "if [ x = 1 ]" siendo leído como variable x. */
static int con_starts_with_control_kw(const char *line) {
    static const char *kws[] = {"if", "then", "else", "elif", "fi", "case",
                                 "esac", "function", "do", "done", NULL};
    for (int i = 0; kws[i]; i++) {
        size_t len = strlen(kws[i]);
        if (strncmp(line, kws[i], len) == 0 &&
            (line[len] == '\0' || isspace((unsigned char)line[len]) || line[len] == ';')) {
            return 1;
        }
    }
    return 0;
}

void con_analyze_variables(const char *trimmed_line, int line_no, con_result_t *result) {
    if (trimmed_line[0] == '#' || trimmed_line[0] == '\0') return;
    if (con_starts_with_control_kw(trimmed_line)) return;

    /* buscamos un identificador válido seguido directamente de '=' (sin
     * espacio antes), y el '=' no debe ser parte de "==", "!=", "<=", ">=" */
    const char *p = trimmed_line;

    /* opcional: "export VAR=valor" / "local VAR=valor" / "readonly VAR=valor" */
    static const char *prefixes[] = {"export ", "local ", "readonly ", "declare ", NULL};
    for (int i = 0; prefixes[i]; i++) {
        size_t len = strlen(prefixes[i]);
        if (strncmp(p, prefixes[i], len) == 0) {
            p += len;
            while (isspace((unsigned char)*p)) p++;
            break;
        }
    }

    if (!isalpha((unsigned char)*p) && *p != '_') return;

    const char *start = p;
    while (con_is_identifier_char(*p)) p++;
    size_t name_len = (size_t)(p - start);
    if (name_len == 0) return;

    /* Soporta arreglos: VAR[idx]=valor -> saltamos el [idx] opcional */
    if (*p == '[') {
        const char *close = strchr(p, ']');
        if (close) p = close + 1;
    }

    if (*p != '=') return;
    if (*(p + 1) == '=') return; /* es "==", comparación, no asignación */
    /* evita "!=" "<=" ">=" mirando el char justo antes del '=' ya consumido
     * por start..p, que por construcción es parte del identificador, así
     * que no aplica aquí; el caso real a evitar es cuando p mismo es '=' de
     * comparación encadenada del tipo "x ==" ya cubierto arriba. */

    char name[PS_NAME_MAX];
    size_t copy_len = name_len < sizeof(name) - 1 ? name_len : sizeof(name) - 1;
    memcpy(name, start, copy_len);
    name[copy_len] = '\0';

    con_result_add(result, CON_FIND_VARIABLE, line_no, name);
}

void con_analyze_loops(const char *trimmed_line, int line_no, con_result_t *result) {
    if (trimmed_line[0] == '#' || trimmed_line[0] == '\0') return;

    const char *keywords[] = {"for", "while", "until", NULL};
    for (int i = 0; keywords[i]; i++) {
        size_t len = strlen(keywords[i]);
        if (strncmp(trimmed_line, keywords[i], len) == 0 &&
            (trimmed_line[len] == '\0' || isspace((unsigned char)trimmed_line[len]) ||
             trimmed_line[len] == '(')) {
            /* texto descriptivo: la línea completa (recortada a un tamaño razonable) */
            con_result_add(result, CON_FIND_LOOP, line_no, trimmed_line);
            return;
        }
    }
}

/* Tabla de analizadores registrados. Para sumar uno nuevo: escribir la
 * función y agregar una entrada aquí. */
static const con_analyzer_fn con_analyzer_table[] = {
    con_analyze_variables,
    con_analyze_loops,
};
static const size_t con_analyzer_count =
    sizeof(con_analyzer_table) / sizeof(con_analyzer_table[0]);

static const char *con_skip_leading_space(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

void con_analyze_buffer(const char *buffer, con_result_t *out) {
    con_result_init(out);

    char line[PS_LINE_MAX];
    int line_no = 0;
    size_t i = 0, len = strlen(buffer);
    size_t li = 0;

    while (i <= len) {
        char c = buffer[i];
        if (c == '\n' || c == '\0') {
            line[li] = '\0';
            line_no++;
            const char *trimmed = con_skip_leading_space(line);
            /* quitar espacio final */
            char clean[PS_LINE_MAX];
            strncpy(clean, trimmed, sizeof(clean) - 1);
            clean[sizeof(clean) - 1] = '\0';
            size_t clen = strlen(clean);
            while (clen > 0 && isspace((unsigned char)clean[clen - 1])) clean[--clen] = '\0';

            for (size_t a = 0; a < con_analyzer_count; a++) {
                con_analyzer_table[a](clean, line_no, out);
            }
            li = 0;
            if (c == '\0') break;
        } else {
            if (li < sizeof(line) - 1) line[li++] = c;
        }
        i++;
    }
}

int con_analyze_file(const char *path, con_result_t *out) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char *buffer = NULL;
    size_t cap = 0, len = 0;
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (len + n + 1 > cap) {
            size_t new_cap = (cap == 0 ? 8192 : cap * 2);
            while (new_cap < len + n + 1) new_cap *= 2;
            char *tmp = realloc(buffer, new_cap);
            if (!tmp) { free(buffer); fclose(f); return -1; }
            buffer = tmp;
            cap = new_cap;
        }
        memcpy(buffer + len, chunk, n);
        len += n;
    }
    fclose(f);

    if (!buffer) {
        char empty[1] = {0};
        con_analyze_buffer(empty, out);
        return 0;
    }
    buffer[len] = '\0';
    con_analyze_buffer(buffer, out);
    free(buffer);
    return 0;
}
