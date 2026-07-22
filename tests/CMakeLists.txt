add_executable(psadmin_tests
    test_main.c
    test_shl.c
    test_tsk.c
    test_con.c
    test_cli.c
    test_viz.c
)

target_include_directories(psadmin_tests PRIVATE ${CHECK_INCLUDE_DIRS})
target_link_directories(psadmin_tests PRIVATE ${CHECK_LIBRARY_DIRS})
target_link_libraries(psadmin_tests PRIVATE psadmin_core ${CHECK_LIBRARIES})
target_compile_options(psadmin_tests PRIVATE ${CHECK_CFLAGS_OTHER})

add_test(NAME psadmin_unit_tests COMMAND psadmin_tests)
