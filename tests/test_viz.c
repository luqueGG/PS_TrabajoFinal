#include <check.h>

#include "viz.h"

START_TEST(test_viz_layout_covers_full_width) {
    ps_rect_t panels[PANEL_COUNT];
    viz_compute_layout(40, 120, panels);

    /* tsk empieza en col 1 */
    ck_assert_int_eq(panels[PANEL_TSK].col, 1);
    /* con empieza justo después de tsk */
    ck_assert_int_eq(panels[PANEL_CON].col, panels[PANEL_TSK].col + panels[PANEL_TSK].width);
    /* shl empieza justo después de con */
    ck_assert_int_eq(panels[PANEL_SHL].col, panels[PANEL_CON].col + panels[PANEL_CON].width);
    /* el ancho total cubierto coincide con term_cols */
    int total = panels[PANEL_TSK].width + panels[PANEL_CON].width + panels[PANEL_SHL].width;
    ck_assert_int_eq(total, 120);
}
END_TEST

START_TEST(test_viz_layout_cli_below_con_same_column) {
    ps_rect_t panels[PANEL_COUNT];
    viz_compute_layout(40, 120, panels);

    ck_assert_int_eq(panels[PANEL_CLI].col, panels[PANEL_CON].col);
    ck_assert_int_eq(panels[PANEL_CLI].width, panels[PANEL_CON].width);
    ck_assert_int_eq(panels[PANEL_CLI].row, panels[PANEL_CON].row + panels[PANEL_CON].height);
}
END_TEST

START_TEST(test_viz_layout_tsk_and_shl_span_full_height) {
    ps_rect_t panels[PANEL_COUNT];
    viz_compute_layout(40, 120, panels);

    ck_assert_int_eq(panels[PANEL_TSK].height, 40);
    ck_assert_int_eq(panels[PANEL_SHL].height, 40);
}
END_TEST

START_TEST(test_viz_layout_handles_small_terminal_without_negatives) {
    ps_rect_t panels[PANEL_COUNT];
    viz_compute_layout(10, 40, panels);
    for (int i = 0; i < PANEL_COUNT; i++) {
        ck_assert(panels[i].width > 0);
        ck_assert(panels[i].height > 0);
        ck_assert(panels[i].row > 0);
        ck_assert(panels[i].col > 0);
    }
}
END_TEST

Suite *viz_suite(void) {
    Suite *s = suite_create("viz");
    TCase *tc = tcase_create("core");
    tcase_add_test(tc, test_viz_layout_covers_full_width);
    tcase_add_test(tc, test_viz_layout_cli_below_con_same_column);
    tcase_add_test(tc, test_viz_layout_tsk_and_shl_span_full_height);
    tcase_add_test(tc, test_viz_layout_handles_small_terminal_without_negatives);
    suite_add_tcase(s, tc);
    return s;
}
