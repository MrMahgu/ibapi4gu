file(REMOVE_RECURSE "${IBAPI4GU_TEST_BINARY_DIR}")
set(_superproject_source "${IBAPI4GU_TEST_BINARY_DIR}-source")
file(REMOVE_RECURSE "${_superproject_source}")
file(MAKE_DIRECTORY "${_superproject_source}")

set(_superproject_cmake [=[
cmake_minimum_required(VERSION 3.28.3)
project(ibapi4gu_parent_smoke LANGUAGES C CXX)

set(protobuf_BUILD_TESTS ON CACHE BOOL "parent sentinel")
set(protobuf_INSTALL ON CACHE BOOL "parent sentinel")

add_subdirectory("@IBAPI4GU_SOURCE_DIR@" ibapi4gu)

if(NOT TARGET ibapi4gu::twsapi)
  message(FATAL_ERROR "ibapi4gu::twsapi was not defined for the parent project")
endif()

get_property(_parent_protobuf_tests CACHE protobuf_BUILD_TESTS PROPERTY VALUE)
get_property(_parent_protobuf_install CACHE protobuf_INSTALL PROPERTY VALUE)
if(NOT _parent_protobuf_tests STREQUAL "ON"
   OR NOT _parent_protobuf_install STREQUAL "ON")
  message(FATAL_ERROR "ibapi4gu changed the parent Protobuf cache options")
endif()

add_executable(parent_consumer main.cpp)
target_link_libraries(parent_consumer PRIVATE ibapi4gu::twsapi)
]=])
string(CONFIGURE "${_superproject_cmake}" _superproject_cmake @ONLY)
file(WRITE "${_superproject_source}/CMakeLists.txt" "${_superproject_cmake}")
file(WRITE "${_superproject_source}/main.cpp"
  "#include <client/EClientSocket.h>\nint main() { EClientSocket client(nullptr); }\n"
)

set(_configure_command
  "${CMAKE_COMMAND}"
  -S "${_superproject_source}"
  -B "${IBAPI4GU_TEST_BINARY_DIR}"
  -G "${IBAPI4GU_GENERATOR}"
  "-DIBAPI4GU_IBKR_DISTRIBUTION=${IBAPI4GU_DISTRIBUTION}"
  "-DIBAPI4GU_IBKR_SOURCE_DIR=${IBAPI4GU_IBKR_SOURCE}"
  "-DFETCHCONTENT_SOURCE_DIR_PROTOBUF=${IBAPI4GU_PROTOBUF_SOURCE}"
  "-DFETCHCONTENT_SOURCE_DIR_INTELRDFP=${IBAPI4GU_INTELRDFP_SOURCE}"
  -DIBAPI4GU_BUILD_EXAMPLES=OFF
  -DBUILD_TESTING=OFF
)
if(IBAPI4GU_ABSEIL_SOURCE)
  list(APPEND _configure_command
    "-DFETCHCONTENT_SOURCE_DIR_ABSEIL=${IBAPI4GU_ABSEIL_SOURCE}"
  )
endif()
if(IBAPI4GU_GENERATOR_PLATFORM)
  list(APPEND _configure_command -A "${IBAPI4GU_GENERATOR_PLATFORM}")
endif()
if(IBAPI4GU_GENERATOR_TOOLSET)
  list(APPEND _configure_command -T "${IBAPI4GU_GENERATOR_TOOLSET}")
endif()
if(NOT IBAPI4GU_GENERATOR MATCHES "Visual Studio|Xcode")
  list(APPEND _configure_command
    "-DCMAKE_C_COMPILER=${IBAPI4GU_C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${IBAPI4GU_CXX_COMPILER}"
  )
endif()

execute_process(
  COMMAND ${_configure_command}
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _output
  ERROR_VARIABLE _error
)
if(NOT _result EQUAL 0)
  message(FATAL_ERROR
    "Local-source configuration failed (${_result}).\n${_output}\n${_error}"
  )
endif()
