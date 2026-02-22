set (tests
dLogging
dLogging2
tLoggerHolder
tLogging
)

foreach (test ${tests})
    add_executable (${test} ${test}.cc)
    add_pch_support(${test})
    target_link_libraries (${test} casa_tables)
    add_test (NAME ${test} COMMAND ${test})
    add_dependencies(check ${test})
endforeach (test)
