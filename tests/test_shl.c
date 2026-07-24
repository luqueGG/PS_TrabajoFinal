#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "shl.h"

static char g_tmp_root[PS_PATH_MAX];

static void setup_tmp_tree(void) {
    snprintf(g_tmp_root, sizeof(g_tmp_root), "/tmp/psadmin_test_shl_%d", getpid());
    mkdir(g_tmp_root, 0755);

    char p[PS_PATH_MAX];
    snprintf(p, sizeof(p), "%s/zzz_dir", g_tmp_root);
    mkdir(p, 0755);
    snprintf(p, sizeof(p), "%s/aaa_file.txt", g_tmp_root);
    FILE *f = fopen(p, "w");
    fputs("hola\n", f);
    fclose(f);
    snprintf(p, sizeof(p), "%s/bbb_dir", g_tmp_root);
    mkdir(p, 0755);
}

START_TEST(test_shl_init_and_reload_lists_entries) {
    setup_tmp_tree();
    shl_state_t st;
    int rc = shl_init(&st, g_tmp_root);
    ck_assert_int_eq(rc, 0);
    /* esperamos 4 entradas: ".." (siempre presente salvo en "/"), aaa_file.txt, bbb_dir, zzz_dir */
    ck_assert_uint_eq(st.count, 4);
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_dirs_sorted_before_files) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    /* orden esperado: "..", "bbb_dir", "zzz_dir" (dirs, alfabético; ".." < letras en ASCII),
     * luego "aaa_file.txt" (archivo, al final) */
    ck_assert_int_eq(st.entries[0].type, SHL_ENTRY_DIR);
    ck_assert_int_eq(st.entries[1].type, SHL_ENTRY_DIR);
    ck_assert_int_eq(st.entries[2].type, SHL_ENTRY_DIR);
    ck_assert_int_eq(st.entries[3].type, SHL_ENTRY_FILE);
    ck_assert_str_eq(st.entries[0].name, "..");
    ck_assert_str_eq(st.entries[1].name, "bbb_dir");
    ck_assert_str_eq(st.entries[2].name, "zzz_dir");
    ck_assert_str_eq(st.entries[3].name, "aaa_file.txt");
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_move_selection_clamps) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    shl_move_selection(&st, -5);
    ck_assert_int_eq(st.selected, 0);
    shl_move_selection(&st, 1000);
    ck_assert_int_eq(st.selected, (int)st.count - 1);
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_selected_path_builds_full_path) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    st.selected = 0;
    char out[PS_PATH_MAX];
    int rc = shl_selected_path(&st, out, sizeof(out));
    ck_assert_int_eq(rc, 0);
    char expected[PS_PATH_MAX];
    snprintf(expected, sizeof(expected), "%s/%s", g_tmp_root, st.entries[0].name);
    ck_assert_str_eq(out, expected);
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_chdir_into_subdir_and_back) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    int rc = shl_chdir(&st, "bbb_dir");
    ck_assert_int_eq(rc, 0);
    /* subdir vacío, pero ".." siempre aparece */
    ck_assert_uint_eq(st.count, 1);
    ck_assert_str_eq(st.entries[0].name, "..");

    rc = shl_chdir(&st, "..");
    ck_assert_int_eq(rc, 0);
    ck_assert_uint_eq(st.count, 4);
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_build_backup_name_format) {
    char out[256];
    /* timestamp fijo: 2024-01-02 03:04:05 UTC aprox (depende de TZ local,
     * por eso sólo verificamos el patrón general, no el valor exacto) */
    time_t fixed = 1704165845; /* 2024-01-02 03:04:05 UTC */
    shl_build_backup_name("script.sh", fixed, out, sizeof(out));
    ck_assert_int_eq(strncmp(out, "script.sh_", 10), 0);
    ck_assert_ptr_nonnull(strstr(out, ".tar.gz"));
}
END_TEST

START_TEST(test_shl_backup_selected_creates_tar) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    /* seleccionamos el archivo (índice 3, después de "..", "bbb_dir", "zzz_dir") */
    st.selected = 3;
    ck_assert_str_eq(st.entries[3].name, "aaa_file.txt");

    char dest_dir[PS_PATH_MAX];
    snprintf(dest_dir, sizeof(dest_dir), "%s_backups", g_tmp_root);
    mkdir(dest_dir, 0755);

    char backup_path[PS_PATH_MAX];
    int rc = shl_backup_selected(&st, dest_dir, backup_path, sizeof(backup_path));
    ck_assert_int_eq(rc, 0);

    struct stat sb;
    ck_assert_int_eq(stat(backup_path, &sb), 0);
    ck_assert(sb.st_size > 0);

    shl_free(&st);
}
END_TEST

START_TEST(test_shl_delete_selected_file) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    st.selected = 3; /* "aaa_file.txt", ver test_shl_dirs_sorted_before_files */
    ck_assert_str_eq(st.entries[3].name, "aaa_file.txt");

    char path[PS_PATH_MAX];
    snprintf(path, sizeof(path), "%s/aaa_file.txt", g_tmp_root);

    int rc = shl_delete_selected(&st);
    ck_assert_int_eq(rc, 0);

    struct stat sb;
    ck_assert_int_ne(stat(path, &sb), 0);
    ck_assert_uint_eq(st.count, 3); /* "..", bbb_dir, zzz_dir */
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_delete_selected_dir_recursive) {
    setup_tmp_tree();
    /* metemos contenido dentro de bbb_dir para probar el borrado recursivo */
    char nested[PS_PATH_MAX];
    snprintf(nested, sizeof(nested), "%s/bbb_dir/inner.txt", g_tmp_root);
    FILE *f = fopen(nested, "w");
    fputs("x\n", f);
    fclose(f);

    shl_state_t st;
    shl_init(&st, g_tmp_root);
    st.selected = 1; /* "bbb_dir" */
    ck_assert_str_eq(st.entries[1].name, "bbb_dir");

    char dir_path[PS_PATH_MAX];
    snprintf(dir_path, sizeof(dir_path), "%s/bbb_dir", g_tmp_root);

    int rc = shl_delete_selected(&st);
    ck_assert_int_eq(rc, 0);

    struct stat sb;
    ck_assert_int_ne(stat(dir_path, &sb), 0);
    ck_assert_uint_eq(st.count, 3); /* "..", aaa_file.txt, zzz_dir */
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_delete_dotdot_fails) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    st.selected = 0; /* ".." */
    ck_assert_str_eq(st.entries[0].name, "..");

    int rc = shl_delete_selected(&st);
    ck_assert_int_ne(rc, 0);
    ck_assert_uint_eq(st.count, 4); /* nada cambió */
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_clip_copy_to_subdir) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    st.selected = 3; /* "aaa_file.txt" */
    ck_assert_str_eq(st.entries[3].name, "aaa_file.txt");

    int rc = shl_clip_set(&st, 0 /* copiar */);
    ck_assert_int_eq(rc, 0);

    rc = shl_chdir(&st, "bbb_dir");
    ck_assert_int_eq(rc, 0);

    rc = shl_clip_paste(&st);
    ck_assert_int_eq(rc, 0);

    char src[PS_PATH_MAX], dst[PS_PATH_MAX];
    snprintf(src, sizeof(src), "%s/aaa_file.txt", g_tmp_root);
    snprintf(dst, sizeof(dst), "%s/bbb_dir/aaa_file.txt", g_tmp_root);

    struct stat sb;
    ck_assert_int_eq(stat(src, &sb), 0); /* original sigue existiendo (copia) */
    ck_assert_int_eq(stat(dst, &sb), 0); /* copia llegó al destino */
    ck_assert_str_eq(st.clip_path, "");  /* portapapeles se limpia tras pegar */
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_clip_move_to_subdir) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    st.selected = 3; /* "aaa_file.txt" */

    int rc = shl_clip_set(&st, 1 /* mover */);
    ck_assert_int_eq(rc, 0);

    rc = shl_chdir(&st, "zzz_dir");
    ck_assert_int_eq(rc, 0);

    rc = shl_clip_paste(&st);
    ck_assert_int_eq(rc, 0);

    char src[PS_PATH_MAX], dst[PS_PATH_MAX];
    snprintf(src, sizeof(src), "%s/aaa_file.txt", g_tmp_root);
    snprintf(dst, sizeof(dst), "%s/zzz_dir/aaa_file.txt", g_tmp_root);

    struct stat sb;
    ck_assert_int_ne(stat(src, &sb), 0); /* el original ya no está (se movió) */
    ck_assert_int_eq(stat(dst, &sb), 0); /* llegó al destino */
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_clip_set_on_dotdot_fails) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    st.selected = 0; /* ".." */
    int rc = shl_clip_set(&st, 0);
    ck_assert_int_ne(rc, 0);
    ck_assert_str_eq(st.clip_path, "");
    shl_free(&st);
}
END_TEST

START_TEST(test_shl_clip_paste_without_mark_fails) {
    setup_tmp_tree();
    shl_state_t st;
    shl_init(&st, g_tmp_root);
    int rc = shl_clip_paste(&st);
    ck_assert_int_ne(rc, 0);
    shl_free(&st);
}
END_TEST

Suite *shl_suite(void) {
    Suite *s = suite_create("shl");
    TCase *tc = tcase_create("core");
    tcase_add_test(tc, test_shl_init_and_reload_lists_entries);
    tcase_add_test(tc, test_shl_dirs_sorted_before_files);
    tcase_add_test(tc, test_shl_move_selection_clamps);
    tcase_add_test(tc, test_shl_selected_path_builds_full_path);
    tcase_add_test(tc, test_shl_chdir_into_subdir_and_back);
    tcase_add_test(tc, test_shl_build_backup_name_format);
    tcase_add_test(tc, test_shl_backup_selected_creates_tar);
    tcase_add_test(tc, test_shl_delete_selected_file);
    tcase_add_test(tc, test_shl_delete_selected_dir_recursive);
    tcase_add_test(tc, test_shl_delete_dotdot_fails);
    tcase_add_test(tc, test_shl_clip_copy_to_subdir);
    tcase_add_test(tc, test_shl_clip_move_to_subdir);
    tcase_add_test(tc, test_shl_clip_set_on_dotdot_fails);
    tcase_add_test(tc, test_shl_clip_paste_without_mark_fails);
    suite_add_tcase(s, tc);
    return s;
}
