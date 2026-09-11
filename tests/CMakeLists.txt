add_executable(ibapi4gu_decimal_test decimal_test.cpp)
target_link_libraries(ibapi4gu_decimal_test PRIVATE
  ibapi4gu::twsapi
  ibapi4gu_project_options
  ibapi4gu_project_warnings
)
add_test(NAME decimal-regressions COMMAND ibapi4gu_decimal_test)

add_executable(ibapi4gu_smoke_test smoke_test.cpp)
target_link_libraries(ibapi4gu_smoke_test PRIVATE
  ibapi4gu::twsapi
  ibapi4gu_project_options
  ibapi4gu_project_warnings
)
target_compile_definitions(ibapi4gu_smoke_test PRIVATE
  IBAPI4GU_EXPECTED_PROTOBUF_VERSION=${IBAPI4GU_REQUIRED_PROTOBUF_NUMERIC_VERSION}
)
add_test(NAME public-target-smoke COMMAND ibapi4gu_smoke_test)

add_test(NAME decimal-patch-guard
  COMMAND "${CMAKE_COMMAND}"
    "-DIBAPI4GU_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DIBAPI4GU_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/patch-guard"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch_guard.cmake"
)

set(_local_source_test_arguments
  "-DIBAPI4GU_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
  "-DIBAPI4GU_TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/local-source-build"
  "-DIBAPI4GU_GENERATOR=${CMAKE_GENERATOR}"
  "-DIBAPI4GU_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
  "-DIBAPI4GU_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
  "-DIBAPI4GU_C_COMPILER=${CMAKE_C_COMPILER}"
  "-DIBAPI4GU_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
  "-DIBAPI4GU_DISTRIBUTION=${IBAPI4GU_IBKR_DISTRIBUTION}"
  "-DIBAPI4GU_IBKR_SOURCE=${IBAPI4GU_RESOLVED_IBKR_SOURCE_DIR}"
  "-DIBAPI4GU_PROTOBUF_SOURCE=${protobuf_SOURCE_DIR}"
  "-DIBAPI4GU_INTELRDFP_SOURCE=${intelrdfp_SOURCE_DIR}"
)
if(DEFINED abseil_SOURCE_DIR)
  list(APPEND _local_source_test_arguments
    "-DIBAPI4GU_ABSEIL_SOURCE=${abseil_SOURCE_DIR}"
  )
endif()
add_test(NAME local-source-configure
  COMMAND "${CMAKE_COMMAND}"
    ${_local_source_test_arguments}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/local_source_configure.cmake"
)
set_tests_properties(local-source-configure PROPERTIES RUN_SERIAL TRUE)

set_property(TARGET ibapi4gu_decimal_test ibapi4gu_smoke_test
  PROPERTY FOLDER "tests"
)
