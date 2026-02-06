include_guard(GLOBAL)
include(FetchContent)

set(IBAPI_PROTOBUF_VERSION "5.29.5" CACHE STRING "Protobuf version")

if(NOT DEFINED protobuf_BUILD_TESTS)
  set(protobuf_BUILD_TESTS OFF CACHE BOOL "Disable Protobuf tests")
endif()

if(NOT DEFINED protobuf_BUILD_SHARED_LIBS)
  set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "Static Protobuf libs")
endif()

if(NOT DEFINED protobuf_WITH_ZLIB)
  set(protobuf_WITH_ZLIB OFF CACHE BOOL "Disable zlib in Protobuf")
endif()

if(NOT DEFINED protobuf_INSTALL)
  set(protobuf_INSTALL OFF CACHE BOOL "Disable Protobuf install rules")
endif()

if(NOT DEFINED protobuf_BUILD_LIBPROTOC)
  set(protobuf_BUILD_LIBPROTOC OFF CACHE BOOL "Disable libprotoc")
endif()

if(NOT DEFINED protobuf_BUILD_PROTOC_BINARIES)
  set(protobuf_BUILD_PROTOC_BINARIES OFF CACHE BOOL "Disable protoc binary")
endif()

if(NOT DEFINED protobuf_ABSL_PROVIDER)
  set(protobuf_ABSL_PROVIDER "module" CACHE STRING "Abseil provider for Protobuf")
endif()

if(NOT DEFINED ABSL_PROPAGATE_CXX_STD)
  set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "Let Abseil propagate C++ standard")
endif()

if(NOT DEFINED FETCHCONTENT_BASE_DIR)
  set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "FetchContent base dir")
endif()

FetchContent_Declare(
  protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
  GIT_TAG        v${IBAPI_PROTOBUF_VERSION}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
  USES_TERMINAL_DOWNLOAD TRUE
)

FetchContent_GetProperties(protobuf)
if(NOT protobuf_POPULATED)
  message(STATUS "Git: Fetching protobuf v${IBAPI_PROTOBUF_VERSION}")
  FetchContent_MakeAvailable(protobuf)  
endif()

# Protobuf layout differs across versions:
# - newer: CMakeLists.txt at repo root
# - older: CMakeLists.txt under cmake/
set(_protobuf_cmake_dir "${protobuf_SOURCE_DIR}")
if(NOT EXISTS "${_protobuf_cmake_dir}/CMakeLists.txt"
   AND EXISTS "${protobuf_SOURCE_DIR}/cmake/CMakeLists.txt")
  set(_protobuf_cmake_dir "${protobuf_SOURCE_DIR}/cmake")
endif()
if(NOT EXISTS "${_protobuf_cmake_dir}/CMakeLists.txt")
  message(FATAL_ERROR "Protobuf CMakeLists.txt not found in ${protobuf_SOURCE_DIR} or ${protobuf_SOURCE_DIR}/cmake.")
endif()

if(NOT TARGET libprotobuf AND NOT TARGET protobuf::libprotobuf)
  add_subdirectory("${_protobuf_cmake_dir}" "${protobuf_BINARY_DIR}" EXCLUDE_FROM_ALL)
endif()

# Attach protobuf targets to the deps bucket of chicken
set(_ibapi_pb_targets "")
  foreach(t IN ITEMS
    protobuf::libprotobuf
    protobuf::libprotobuf-lite
    libprotobuf
    libprotobuf-lite
  )
  if(TARGET ${t})
    list(APPEND _ibapi_pb_targets ${t})
  endif()
endforeach()

if(NOT _ibapi_pb_targets)
  message(FATAL_ERROR "Protobuf fetched but protobuf:: targets not found; check Protobuf CMake export names for v${IBAPI_PROTOBUF_VERSION}.")
endif()

target_link_libraries(ibapi_stack_deps_protobuf INTERFACE ${_ibapi_pb_targets})
