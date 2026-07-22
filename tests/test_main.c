#include <check.h>
#include <stdlib.h>

Suite *shl_suite(void);
Suite *tsk_suite(void);
Suite *con_suite(void);
Suite *cli_suite(void);
Suite *viz_suite(void);

int main(void) {
    int failed = 0;

    Suite *suites[] = {
        shl_suite(), tsk_suite(), con_suite(), cli_suite(), viz_suite(),
    };
    size_t n = sizeof(suites) / sizeof(suites[0]);

    for (size_t i = 0; i < n; i++) {
        SRunner *runner = srunner_create(suites[i]);
        srunner_run_all(runner, CK_NORMAL);
        failed += srunner_ntests_failed(runner);
        srunner_free(runner);
    }

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
