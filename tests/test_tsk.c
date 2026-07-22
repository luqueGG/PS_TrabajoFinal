#include <check.h>
#include <string.h>

#include "tsk.h"

START_TEST(test_tsk_parse_stat_line_basic) {
    /* línea típica de /proc/[pid]/stat: pid (comm) state ppid ... utime stime ... */
    const char *line =
        "1234 (bash) S 1 1234 1234 0 -1 4194304 100 0 0 0 50 20 0 0 20 0 1 0 1000 "
        "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0";
    tsk_proc_t p;
    int rc = tsk_parse_stat_line(line, &p);
    ck_assert_int_eq(rc, 0);
    ck_assert_int_eq(p.pid, 1234);
    ck_assert_str_eq(p.comm, "bash");
    ck_assert_int_eq(p.state, TSK_SLEEPING);
    ck_assert_int_eq(p.ppid, 1);
    ck_assert_uint_eq(p.utime, 50);
    ck_assert_uint_eq(p.stime, 20);
}
END_TEST

START_TEST(test_tsk_parse_stat_line_comm_with_spaces) {
    /* comandos con espacios o paréntesis dentro de (comm), p.ej. "(kworker/0:1)" */
    const char *line =
        "99 (some weird name) R 1 99 99 0 -1 0 0 0 0 0 5 5 0 0 20 0 1 0 1000 "
        "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0";
    tsk_proc_t p;
    int rc = tsk_parse_stat_line(line, &p);
    ck_assert_int_eq(rc, 0);
    ck_assert_str_eq(p.comm, "some weird name");
    ck_assert_int_eq(p.state, TSK_RUNNING);
}
END_TEST

START_TEST(test_tsk_parse_stat_line_invalid) {
    tsk_proc_t p;
    int rc = tsk_parse_stat_line("esto no tiene parentesis", &p);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_tsk_move_selection_clamps) {
    tsk_state_list_t st;
    memset(&st, 0, sizeof(st));
    st.count = 5;
    st.selected = 2;
    tsk_move_selection(&st, -10);
    ck_assert_int_eq(st.selected, 0);
    tsk_move_selection(&st, 100);
    ck_assert_int_eq(st.selected, 4);
}
END_TEST

START_TEST(test_tsk_move_selection_empty_list) {
    tsk_state_list_t st;
    memset(&st, 0, sizeof(st));
    st.count = 0;
    tsk_move_selection(&st, 3);
    ck_assert_int_eq(st.selected, 0);
}
END_TEST

START_TEST(test_tsk_build_tree_simple_chain) {
    tsk_state_list_t st;
    memset(&st, 0, sizeof(st));
    tsk_proc_t procs[4];
    memset(procs, 0, sizeof(procs));
    procs[0] = (tsk_proc_t){.pid = 1, .ppid = 0};
    procs[1] = (tsk_proc_t){.pid = 2, .ppid = 1};
    procs[2] = (tsk_proc_t){.pid = 3, .ppid = 2};
    procs[3] = (tsk_proc_t){.pid = 4, .ppid = 1};
    st.procs = procs;
    st.count = 4;

    pid_t out[8];
    int depth[8];
    size_t n = tsk_build_tree(&st, 1, out, depth, 8);

    /* DFS pre-order esperado: 1 (d0), 2 (d1), 3 (d2), 4 (d1) */
    ck_assert_uint_eq(n, 4);
    ck_assert_int_eq(out[0], 1); ck_assert_int_eq(depth[0], 0);
    ck_assert_int_eq(out[1], 2); ck_assert_int_eq(depth[1], 1);
    ck_assert_int_eq(out[2], 3); ck_assert_int_eq(depth[2], 2);
    ck_assert_int_eq(out[3], 4); ck_assert_int_eq(depth[3], 1);
}
END_TEST

START_TEST(test_tsk_search_case_insensitive) {
    tsk_state_list_t st;
    memset(&st, 0, sizeof(st));
    tsk_proc_t procs[3];
    memset(procs, 0, sizeof(procs));
    strcpy(procs[0].comm, "Bash");
    strcpy(procs[1].comm, "vim");
    strcpy(procs[2].comm, "bashrc-helper");
    st.procs = procs;
    st.count = 3;

    const tsk_proc_t *matches[3];
    size_t n = tsk_search(&st, "BASH", matches, 3);
    ck_assert_uint_eq(n, 2);
}
END_TEST

Suite *tsk_suite(void) {
    Suite *s = suite_create("tsk");
    TCase *tc = tcase_create("core");
    tcase_add_test(tc, test_tsk_parse_stat_line_basic);
    tcase_add_test(tc, test_tsk_parse_stat_line_comm_with_spaces);
    tcase_add_test(tc, test_tsk_parse_stat_line_invalid);
    tcase_add_test(tc, test_tsk_move_selection_clamps);
    tcase_add_test(tc, test_tsk_move_selection_empty_list);
    tcase_add_test(tc, test_tsk_build_tree_simple_chain);
    tcase_add_test(tc, test_tsk_search_case_insensitive);
    suite_add_tcase(s, tc);
    return s;
}
