#foreach (files
#antenna.params
#feed.params
#field.params
#obs.params
#range-obs.params
#selector-obs.params
#spw.params
#)
#    configure_file (${CMAKE_CURRENT_SOURCE_DIR}/${files} ${CMAKE_CURRENT_BINARY_DIR}#/${files} COPYONLY)
#endforeach (files)

set (tests
tMeasurementSet
tMSColumns
tMSDataDescBuffer
tMSFieldBuffer
tMSFieldEphem
tMSIter
tMSMainBuffer
tMSPolBuffer
tStokesConverter
)

foreach (test ${tests})
	add_executable (${test} ${test}.cc)
	add_pch_support(${test})
	target_link_libraries (${test} casa_ms)
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
