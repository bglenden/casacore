# Define the table files used in tests.
set (datafiles
../../Tables/test/tTable_2.data_v0/table.dat
../../Tables/test/tTable_2.data_v0/table.f0
../../Tables/test/tTable_2.data_v0/table.f0i0
../../Tables/test/tTable_2.data_v0/table.f1
../../Tables/test/tTable_2.data_v0/table.f2
../../Tables/test/tTable_2.data_v0/table.info
)

# Copying is not needed, thus outcommented.
#foreach (file ${datafiles})
#    configure_file (${CMAKE_CURRENT_SOURCE_DIR}/${file} ${CMAKE_CURRENT_BINARY_DIR}/${file} COPYONLY)
#endforeach (file)


set (tests
tExprGroup
tExprGroupArray
tExprNode
tExprNodeSet
tExprNodeSetElem
tExprNodeSetOpt
tExprUnitNode
tExprNodeUDF
tMArray
tMArrayMath
tMArrayUtil
tRecordExpr
tRecordGram
tRecordGramTable
tTableExprData
tTableGram
tTableGramError
tTableGramFunc
tTaQLNode
)

# Only test scripts, no test programs.
set (testscripts
ttaql
tTableGramAlttab
tTableGramCretab
tTableGramGroupAggr
tTableGramGroupAggrAll
tTableGramGroupAggrMaskAll
tTableGramJoin
tTableGramMasked
tTableGramNull
tTableGramUpdate
)

# Some test sources include a test .h file.
include_directories ( . )

foreach (test ${tests})
    add_executable (${test} ${test}.cc)
    add_pch_support(${test})
    target_link_libraries (${test} casa_tables)
    set(_use_assay FALSE)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test}.run"
            OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test}.out"
            OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test}.in")
        set(_use_assay TRUE)
    endif()
    file(GLOB _test_sidecars
        "${CMAKE_CURRENT_SOURCE_DIR}/${test}.in_*"
        "${CMAKE_CURRENT_SOURCE_DIR}/${test}.out_*"
        "${CMAKE_CURRENT_SOURCE_DIR}/${test}.run_*")
    if (_test_sidecars)
        set(_use_assay TRUE)
    endif()

    if (_use_assay)
        add_test (${test} ${CMAKE_SOURCE_DIR}/cmake/cmake_assay ./${test})
    else()
        add_test (NAME ${test} COMMAND ${test})
    endif()
    add_dependencies(check ${test})
endforeach (test)

foreach (test ${testscripts})
    add_test (${test} ${CMAKE_SOURCE_DIR}/cmake/cmake_assay ./${test})
endforeach (test)
