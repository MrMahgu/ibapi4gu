file(REMOVE_RECURSE "${IBAPI4GU_TEST_DIR}")
file(MAKE_DIRECTORY "${IBAPI4GU_TEST_DIR}")
set(_input "${IBAPI4GU_TEST_DIR}/Decimal.cpp")
file(WRITE "${_input}" "unreviewed vendor source\n")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DIBAPI4GU_SOURCE_DIR=${IBAPI4GU_SOURCE_DIR}"
    "-DIBAPI4GU_INPUT=${_input}"
    "-DIBAPI4GU_OUTPUT=${IBAPI4GU_TEST_DIR}/patched.cpp"
    -P "${IBAPI4GU_SOURCE_DIR}/tests/cmake/invoke_patch.cmake"
  RESULT_VARIABLE _result
  OUTPUT_QUIET
  ERROR_QUIET
)
if(_result EQUAL 0)
  message(FATAL_ERROR "The Decimal.cpp source hash guard accepted unknown source.")
endif()
