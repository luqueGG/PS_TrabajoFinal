#include <check.h>
#include <string.h>

#include "ccon.h"

START_TEST(test_con_detects_simple_variable) {
    con_result_t res;
    con_analyze_buffer("X=1\necho hola\n", &res);
    ck_assert_uint_eq(res.count, 1);
    ck_assert_int_eq(res.items[0].kind, CON_FIND_VARIABLE);
    ck_assert_str_eq(res.items[0].text, "X");
    ck_assert_int_eq(res.items[0].line_no, 1);
    con_result_free(&res);
}
END_TEST

START_TEST(test_con_ignores_comparison_not_assignment) {
    con_result_t res;
    con_analyze_buffer("if [ \"$x\" == \"1\" ]; then\n  echo ok\nfi\n", &res);
    /* no debe detectar "x" como variable asignada acá */
    for (size_t i = 0; i < res.count; i++) {
        ck_assert_int_ne(res.items[i].kind, CON_FIND_VARIABLE);
    }
    con_result_free(&res);
}
END_TEST

START_TEST(test_con_detects_export_local_readonly) {
    con_result_t res;
    con_analyze_buffer("export FOO=bar\nlocal baz=1\nreadonly QUX=2\n", &res);
    ck_assert_uint_eq(res.count, 3);
    ck_assert_str_eq(res.items[0].text, "FOO");
    ck_assert_str_eq(res.items[1].text, "baz");
    ck_assert_str_eq(res.items[2].text, "QUX");
    con_result_free(&res);
}
END_TEST

START_TEST(test_con_detects_for_while_until_loops) {
    con_result_t res;
    con_analyze_buffer(
        "for i in 1 2 3; do\n"
        "  echo $i\n"
        "done\n"
        "while true; do\n"
        "  break\n"
        "done\n"
        "until false; do\n"
        "  break\n"
        "done\n",
        &res);

    int loop_count = 0;
    for (size_t i = 0; i < res.count; i++) {
        if (res.items[i].kind == CON_FIND_LOOP) loop_count++;
    }
    ck_assert_int_eq(loop_count, 3);
    con_result_free(&res);
}
END_TEST

START_TEST(test_con_ignores_comments_and_empty_lines) {
    con_result_t res;
    con_analyze_buffer("# X=1 esto es un comentario\n\nY=2\n", &res);
    ck_assert_uint_eq(res.count, 1);
    ck_assert_str_eq(res.items[0].text, "Y");
    con_result_free(&res);
}
END_TEST

START_TEST(test_con_array_assignment_detected) {
    con_result_t res;
    con_analyze_buffer("ARR[0]=valor\n", &res);
    ck_assert_uint_eq(res.count, 1);
    ck_assert_str_eq(res.items[0].text, "ARR");
    con_result_free(&res);
}
END_TEST

START_TEST(test_con_for_double_paren_arithmetic) {
    con_result_t res;
    con_analyze_buffer("for ((i=0; i<10; i++)); do\n  echo $i\ndone\n", &res);
    int loop_count = 0;
    for (size_t i = 0; i < res.count; i++) {
        if (res.items[i].kind == CON_FIND_LOOP) loop_count++;
    }
    ck_assert_int_eq(loop_count, 1);
    con_result_free(&res);
}
END_TEST

START_TEST(test_con_kind_label_strings) {
    ck_assert_str_eq(con_kind_label(CON_FIND_VARIABLE), "variable");
    ck_assert_str_eq(con_kind_label(CON_FIND_LOOP), "ciclo");
}
END_TEST

Suite *con_suite(void) {
    Suite *s = suite_create("con");
    TCase *tc = tcase_create("core");
    tcase_add_test(tc, test_con_detects_simple_variable);
    tcase_add_test(tc, test_con_ignores_comparison_not_assignment);
    tcase_add_test(tc, test_con_detects_export_local_readonly);
    tcase_add_test(tc, test_con_detects_for_while_until_loops);
    tcase_add_test(tc, test_con_ignores_comments_and_empty_lines);
    tcase_add_test(tc, test_con_array_assignment_detected);
    tcase_add_test(tc, test_con_for_double_paren_arithmetic);
    tcase_add_test(tc, test_con_kind_label_strings);
    suite_add_tcase(s, tc);
    return s;
}
