set (datafiles
tTable_2.data_v0/table.dat
tTable_2.data_v0/table.f0
tTable_2.data_v0/table.f0i0
tTable_2.data_v0/table.f1
tTable_2.data_v0/table.f2
tTable_2.data_v0/table.info
tTable_2.data_v1/table.dat
tTable_2.data_v1/table.f0
tTable_2.data_v1/table.f0i0
tTable_2.data_v1/table.f1
tTable_2.data_v1/table.f2
tTable_2.data_v1/table.info
)

foreach (file ${datafiles})
    configure_file (${CMAKE_CURRENT_SOURCE_DIR}/${file} ${CMAKE_CURRENT_BINARY_DIR}/${file} COPYONLY)
endforeach (file)


set (tests
ascii2Table
tArrayColumnSlices
tArrayColumnCellSlices
tColumnsIndex
tColumnsIndexArray
tConcatRows
tConcatTable
tConcatTable2
tConcatTable3
tMemoryTable
tNullTable
tReadAsciiTable
tReadAsciiTable2
tRefRows
tRefTable
tRowCopier
tScalarRecordColumn
tTable
tTableAccess
tTableCopy
tTableCopyPerf
tTableDesc
tTableDescHyper
tTableInfo
tTableIter
tTableKeywords
tTableLock
tTableLockSync
tTableLockSync_2
tTableRecord
tTableRow
tTableTrace
tBaseColumnPromotions
tBaseTableCoverage
tRefTableCoverage
tColumnSetCoverage
tPlainTableCoverage
tTableProxy
tTableProxyAdvanced
tTableUtil
tTableVector
tTable_1
tTable_2
tTable_3
tTable_4
tVeryBigTable
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
