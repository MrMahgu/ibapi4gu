include_guard(GLOBAL)
include(FetchContent)

function(_ibapi4gu_read_protobuf_numeric_version include_directory output_variable)
  set(_common_header "${include_directory}/google/protobuf/stubs/common.h")
  if(NOT EXISTS "${_common_header}")
    set(${output_variable} "" PARENT_SCOPE)
    return()
  endif()

  file(STRINGS "${_common_header}" _version_lines
    REGEX "GOOGLE_PROTOBUF_VERSION [0-9]+"
  )
  if(NOT _version_lines)
    set(${output_variable} "" PARENT_SCOPE)
    return()
  endif()
  list(GET _version_lines 0 _version_line)
  string(REGEX REPLACE ".*GOOGLE_PROTOBUF_VERSION ([0-9]+).*" "\\1"
    _numeric_version "${_version_line}"
  )
  set(${output_variable} "${_numeric_version}" PARENT_SCOPE)
endfunction()

function(_ibapi4gu_validate_existing_protobuf)
  set(_detected_version "")

  get_target_property(_include_directories protobuf::libprotobuf
    INTERFACE_INCLUDE_DIRECTORIES
  )
  foreach(_include_directory IN LISTS _include_directories)
    string(REGEX REPLACE "^\\$<BUILD_INTERFACE:(.*)>$" "\\1"
      _include_directory "${_include_directory}"
    )
    if(_include_directory MATCHES "^\\$<")
      continue()
    endif()
    _ibapi4gu_read_protobuf_numeric_version(
      "${_include_directory}" _detected_version
    )
    if(_detected_version)
      break()
    endif()
  endforeach()

  # Imported targets can conceal their include path behind complex generator
  # expressions. Fall back to package metadata only when the header itself was
  # not readable.
  if(NOT _detected_version)
    foreach(_version_variable IN ITEMS protobuf_VERSION_STRING Protobuf_VERSION)
      if(DEFINED ${_version_variable}
         AND "${${_version_variable}}" STREQUAL "${IBAPI4GU_REQUIRED_PROTOBUF_VERSION}")
        set(_detected_version "${IBAPI4GU_REQUIRED_PROTOBUF_NUMERIC_VERSION}")
        break()
      endif()
    endforeach()
  endif()

  if(NOT "${_detected_version}"
     STREQUAL "${IBAPI4GU_REQUIRED_PROTOBUF_NUMERIC_VERSION}")
    message(FATAL_ERROR
      "ibapi4gu requires Protobuf ${IBAPI4GU_REQUIRED_PROTOBUF_VERSION} "
      "(${IBAPI4GU_REQUIRED_PROTOBUF_NUMERIC_VERSION}) for the selected IBKR "
      "distribution, but an existing protobuf::libprotobuf target has an "
      "unknown or different version. Protobuf C++ generated code and runtime "
      "versions must match exactly."
    )
  endif()
endfunction()

if(TARGET protobuf::libprotobuf)
  _ibapi4gu_validate_existing_protobuf()
  return()
endif()

# Normal variables deliberately shadow any parent cache entries while the
# vendored dependencies configure, without rewriting the parent's cache.
set(protobuf_BUILD_TESTS OFF)
set(protobuf_BUILD_CONFORMANCE OFF)
set(protobuf_BUILD_EXAMPLES OFF)
set(protobuf_BUILD_SHARED_LIBS OFF)
set(protobuf_BUILD_PROTOBUF_BINARIES ON)
set(protobuf_BUILD_PROTOC_BINARIES OFF)
set(protobuf_BUILD_LIBPROTOC OFF)
set(protobuf_BUILD_LIBUPB OFF)
set(protobuf_WITH_ZLIB OFF)
set(protobuf_INSTALL OFF)
set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_BUILD_TESTING OFF)
set(ABSL_ENABLE_INSTALL OFF)

if(IBAPI4GU_REQUIRED_PROTOBUF_VERSION STREQUAL "5.29.5")
  FetchContent_Declare(
    abseil
    URL "${IBAPI4GU_ABSEIL_URL}"
    URL_HASH "SHA256=${IBAPI4GU_ABSEIL_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(abseil)

  FetchContent_Declare(
    protobuf
    URL "${IBAPI4GU_PROTOBUF_5_29_5_URL}"
    URL_HASH "SHA256=${IBAPI4GU_PROTOBUF_5_29_5_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(protobuf)
elseif(IBAPI4GU_REQUIRED_PROTOBUF_VERSION STREQUAL "3.12.4")
  FetchContent_Declare(
    protobuf
    URL "${IBAPI4GU_PROTOBUF_3_12_4_URL}"
    URL_HASH "SHA256=${IBAPI4GU_PROTOBUF_3_12_4_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR cmake
    EXCLUDE_FROM_ALL
  )

  # Protobuf 3.12.4 declares policy compatibility with CMake 3.1. CMake 4
  # removed compatibility below 3.5, so raise only this third-party policy
  # floor while leaving the dependency source untouched. Its policy floor
  # also predates CMP0077, so explicitly make option() honor our scoped
  # normal variables instead of changing the parent project's cache.
  block(SCOPE_FOR VARIABLES POLICIES)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(CMAKE_WARN_DEPRECATED OFF)
    FetchContent_MakeAvailable(protobuf)
  endblock()
else()
  message(FATAL_ERROR
    "No pinned source artifact for Protobuf ${IBAPI4GU_REQUIRED_PROTOBUF_VERSION}."
  )
endif()

# The old-Protobuf policy block deliberately scopes variables. Recover the
# populated paths in this directory for validation and local-source tests.
FetchContent_GetProperties(protobuf)

if(NOT TARGET protobuf::libprotobuf AND TARGET libprotobuf)
  add_library(protobuf::libprotobuf ALIAS libprotobuf)
endif()
if(NOT TARGET protobuf::libprotobuf)
  message(FATAL_ERROR "The pinned Protobuf build did not define protobuf::libprotobuf.")
endif()

if(TARGET libprotobuf)
  target_compile_options(libprotobuf PRIVATE
    "$<$<CXX_COMPILER_ID:GNU>:-Wno-deprecated-declarations>"
    "$<$<CXX_COMPILER_ID:GNU>:-Wno-stringop-overread>"
    "$<$<CXX_COMPILER_ID:GNU>:-Wno-stringop-overflow>"
    "$<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-deprecated-declarations>"
  )
endif()

_ibapi4gu_read_protobuf_numeric_version(
  "${protobuf_SOURCE_DIR}/src" _ibapi4gu_built_protobuf_version
)
if(NOT "${_ibapi4gu_built_protobuf_version}"
   STREQUAL "${IBAPI4GU_REQUIRED_PROTOBUF_NUMERIC_VERSION}")
  message(FATAL_ERROR
    "Pinned Protobuf source reports ${_ibapi4gu_built_protobuf_version}; expected "
    "${IBAPI4GU_REQUIRED_PROTOBUF_NUMERIC_VERSION}."
  )
endif()
