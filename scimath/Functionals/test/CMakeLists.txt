set (tests
dFunction
dGaussianND
tChebyshev
tCombiFunction
tCompoundFunction
tConstantND
tFuncExpression
tFunctionHolder
tFunctionOrder
tFunctionWrapper
tGaussian1D
tGaussian2D
tGaussian3D
tGaussianND
tHyperPlane
tInterpolate1D
tPoisson
tPolynomial
tPowerLogarithmicPolynomial
tSampledFunctional
tSimButterworthBandpass
tSinusoid1D
tSPolynomial
)

foreach (test ${tests})
    add_executable (${test} ${test}.cc)
    add_pch_support(${test})
    target_link_libraries (${test} casa_scimath)
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
