#include <check.h>
#include <string.h>

#include "cli.h"

START_TEST(test_cli_outbuf_append_simple) {
    cli_outbuf_t buf;
    memset(&buf, 0, sizeof(buf));
    cli_outbuf_append(&buf, "hola", 4);
    ck_assert_uint_eq(buf.len, 4);
    ck_assert_int_eq(memcmp(buf.data, "hola", 4), 0);
}
END_TEST

START_TEST(test_cli_outbuf_append_accumulates) {
    cli_outbuf_t buf;
    memset(&buf, 0, sizeof(buf));
    cli_outbuf_append(&buf, "abc", 3);
    cli_outbuf_append(&buf, "def", 3);
    ck_assert_uint_eq(buf.len, 6);
    ck_assert_int_eq(memcmp(buf.data, "abcdef", 6), 0);
}
END_TEST

START_TEST(test_cli_outbuf_append_drops_oldest_when_full) {
    cli_outbuf_t buf;
    memset(&buf, 0, sizeof(buf));
    /* llenamos el buffer completo con 'a' */
    char fill[8192];
    memset(fill, 'a', sizeof(fill));
    cli_outbuf_append(&buf, fill, sizeof(fill));
    ck_assert_uint_eq(buf.len, sizeof(fill));

    /* agregamos algo más: debe descartar del frente para hacer lugar */
    cli_outbuf_append(&buf, "ZZZZ", 4);
    ck_assert_uint_eq(buf.len, sizeof(fill)); /* sigue lleno */
    /* las últimas 4 posiciones deben ser "ZZZZ" */
    ck_assert_int_eq(memcmp(buf.data + buf.len - 4, "ZZZZ", 4), 0);
}
END_TEST

START_TEST(test_cli_outbuf_append_larger_than_capacity) {
    cli_outbuf_t buf;
    memset(&buf, 0, sizeof(buf));
    char huge[9000];
    memset(huge, 'x', sizeof(huge) - 5);
    memcpy(huge + sizeof(huge) - 5, "TAIL1", 5); /* últimos 5 bytes distintivos, ojo tamaño */
    cli_outbuf_append(&buf, huge, sizeof(huge));
    ck_assert_uint_eq(buf.len, sizeof(buf.data));
    /* el contenido debe ser la "cola" del dato original */
    ck_assert_int_eq(memcmp(buf.data + buf.len - 5, "TAIL1", 5), 0);
}
END_TEST

Suite *cli_suite(void) {
    Suite *s = suite_create("cli");
    TCase *tc = tcase_create("core");
    tcase_add_test(tc, test_cli_outbuf_append_simple);
    tcase_add_test(tc, test_cli_outbuf_append_accumulates);
    tcase_add_test(tc, test_cli_outbuf_append_drops_oldest_when_full);
    tcase_add_test(tc, test_cli_outbuf_append_larger_than_capacity);
    suite_add_tcase(s, tc);
    return s;
}
