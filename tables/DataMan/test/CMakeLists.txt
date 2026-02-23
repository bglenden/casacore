set (tests
dRetypedArrayEngine
dVSCEngine
dVirtColEng
nISMBucket
tBitFlagsEngine
tCompressComplex
tCompressFloat
tDataManInfo
tExternalStMan
tExternalStManNew
tForwardCol
tForwardColRow
tIncrementalStMan
tMappedArrayEngine
tMemoryStMan
tScaledArrayEngine
tScaledComplexData
tSSMAddRemove
tSSMStringHandler
tStandardStMan
tStArrayFile
tStMan
tStMan1
tStManColumnDispatch
tStorageManagerCoverage
tStManAll
tTiledBool
tTiledCellStM_1
tTiledCellStMan
tTiledColumnStMan
tTiledDataStM_1
tTiledDataStMan
tTiledEmpty
tTiledFileAccess
tTiledShapeStM_1
tTiledShapeStM_2
tTiledShapeStMan
tTiledStMan
tTSMShape
tVirtColEng
tVirtualTaQLColumn
tVACEngine
tVSCEngine
)

if (USE_ADIOS2)
    list(APPEND tests tAdios2StMan)
endif()

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
