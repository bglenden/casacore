set (tests
dM1950_2000
dMeasure
tEarthField
tEarthMagneticMachine
tMBaseline
tMDirection
tMEarthMagnetic
tMFrequency
tMeasComet
tMeasIERS
tMeasJPL
tMeasMath
tMeasure
tMeasuresProxy
tMeasureHolder
tMuvw
tParAngleMachine
tQuality
tRecordTransformable
tStokes
tUVWMachine
tVelocityMachine
)

foreach (test ${tests})
    add_executable (${test} ${test}.cc)
    add_pch_support(${test})
    target_link_libraries (${test} casa_measures)
    add_test (${test} ${CMAKE_SOURCE_DIR}/cmake/cmake_assay ./${test})
    add_dependencies(check ${test})
endforeach (test)

find_package(Boost COMPONENTS unit_test_framework)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIR})

    add_executable (measurestest test.cc tCyclicPtr.cc)
    add_pch_support(measurestest)
    if(TARGET Boost::unit_test_framework)
        target_link_libraries(measurestest casa_measures Boost::unit_test_framework)
    else()
        target_link_libraries(measurestest casa_measures ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
    endif()
    add_test (NAME measurestest COMMAND measurestest)
    add_dependencies(check measurestest)
endif(Boost_FOUND)

if (SOFA_FOUND)
    add_executable (tIAU2000 SofaTest.cc tIAU2000.cc)
    target_link_libraries (tIAU2000 casa_measures ${SOFA_LIBRARY})
    add_test (NAME tIAU2000 COMMAND tIAU2000)
    add_dependencies(check tIAU2000)
endif (SOFA_FOUND)
