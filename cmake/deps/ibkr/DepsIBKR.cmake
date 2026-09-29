include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/PatchDecimal.cmake)

if(APPLE)
  message(FATAL_ERROR "ibapi4gu currently supports x86-64 Linux/WSL and Windows, not macOS.")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _ibapi4gu_system_processor)
if(NOT _ibapi4gu_system_processor MATCHES "^(amd64|x86_64|x64)$")
  message(FATAL_ERROR
    "ibapi4gu currently supports x86-64 only; CMAKE_SYSTEM_PROCESSOR is "
    "'${CMAKE_SYSTEM_PROCESSOR}'."
  )
endif()

set(IBAPI4GU_IBKR_SOURCE_DIR "" CACHE PATH
  "Path to an already-extracted IBKR ${IBAPI4GU_IBKR_VERSION} source tree"
)
set(_ibapi4gu_ibkr_archive_name "twsapi-${IBAPI4GU_IBKR_VERSION}.zip")

if(IBAPI4GU_IBKR_SOURCE_DIR)
  get_filename_component(_ibapi4gu_ibkr_source_root
    "${IBAPI4GU_IBKR_SOURCE_DIR}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  if(NOT IS_DIRECTORY "${_ibapi4gu_ibkr_source_root}")
    message(FATAL_ERROR
      "IBAPI4GU_IBKR_SOURCE_DIR is not a directory: "
      "${_ibapi4gu_ibkr_source_root}"
    )
  endif()
else()
  set(_ibapi4gu_ibkr_download_directory
    "${CMAKE_BINARY_DIR}/_deps/ibapi4gu-downloads"
  )
  set(_ibapi4gu_ibkr_archive
    "${_ibapi4gu_ibkr_download_directory}/${_ibapi4gu_ibkr_archive_name}"
  )
  string(SUBSTRING "${IBAPI4GU_IBKR_SHA256}" 0 12 _ibapi4gu_ibkr_hash_prefix)
  set(_ibapi4gu_ibkr_source_root
    "${CMAKE_BINARY_DIR}/_deps/ibkr-${IBAPI4GU_IBKR_VERSION}-${_ibapi4gu_ibkr_hash_prefix}"
  )
  set(_ibapi4gu_ibkr_stamp "${_ibapi4gu_ibkr_source_root}/.ibapi4gu-extracted")

  file(MAKE_DIRECTORY "${_ibapi4gu_ibkr_download_directory}")
  file(DOWNLOAD
    "${IBAPI4GU_IBKR_URL}"
    "${_ibapi4gu_ibkr_archive}"
    EXPECTED_HASH "SHA256=${IBAPI4GU_IBKR_SHA256}"
    TLS_VERIFY ON
    INACTIVITY_TIMEOUT 30
    TIMEOUT 300
    STATUS _ibapi4gu_download_status
    SHOW_PROGRESS
  )
  list(GET _ibapi4gu_download_status 0 _ibapi4gu_download_result)
  list(GET _ibapi4gu_download_status 1 _ibapi4gu_download_message)
  if(NOT _ibapi4gu_download_result EQUAL 0)
    message(FATAL_ERROR
      "Failed to download the pinned IBKR artifact: "
      "${_ibapi4gu_download_message}"
    )
  endif()

  if(NOT EXISTS "${_ibapi4gu_ibkr_stamp}")
    file(REMOVE_RECURSE "${_ibapi4gu_ibkr_source_root}")
    file(MAKE_DIRECTORY "${_ibapi4gu_ibkr_source_root}")

    file(ARCHIVE_EXTRACT
      INPUT "${_ibapi4gu_ibkr_archive}"
      DESTINATION "${_ibapi4gu_ibkr_source_root}"
    )

    file(WRITE "${_ibapi4gu_ibkr_stamp}"
      "${IBAPI4GU_IBKR_VERSION} ${IBAPI4GU_IBKR_SHA256}\n"
    )
  endif()
endif()

file(GLOB_RECURSE _ibapi4gu_socket_anchors LIST_DIRECTORIES FALSE
  "${_ibapi4gu_ibkr_source_root}/*/EClientSocket.cpp"
)
list(FILTER _ibapi4gu_socket_anchors INCLUDE REGEX
  "/(cppclient|CppClient)/client/EClientSocket\\.cpp$"
)
list(LENGTH _ibapi4gu_socket_anchors _ibapi4gu_anchor_count)
if(NOT _ibapi4gu_anchor_count EQUAL 1)
  message(FATAL_ERROR
    "Expected exactly one IBKR C++ client under ${_ibapi4gu_ibkr_source_root}; "
    "found ${_ibapi4gu_anchor_count}: ${_ibapi4gu_socket_anchors}"
  )
endif()

list(GET _ibapi4gu_socket_anchors 0 _ibapi4gu_socket_anchor)
get_filename_component(_ibapi4gu_client_directory
  "${_ibapi4gu_socket_anchor}" DIRECTORY
)
get_filename_component(_ibapi4gu_cppclient_directory
  "${_ibapi4gu_client_directory}" DIRECTORY
)
set(_ibapi4gu_generated_directory
  "${_ibapi4gu_client_directory}/${IBAPI4GU_IBKR_PROTOBUF_DIR}"
)

set(_ibapi4gu_marker_header "${_ibapi4gu_generated_directory}/OrderBound.pb.h")
if(NOT EXISTS "${_ibapi4gu_marker_header}")
  message(FATAL_ERROR "IBKR generated Protobuf header not found: ${_ibapi4gu_marker_header}")
endif()
file(READ "${_ibapi4gu_marker_header}" _ibapi4gu_marker_contents)
string(FIND "${_ibapi4gu_marker_contents}" "${IBAPI4GU_IBKR_PROTOBUF_MARKER}"
  _ibapi4gu_marker_position
)
if(_ibapi4gu_marker_position EQUAL -1)
  message(FATAL_ERROR
    "The selected IBKR source does not contain the expected Protobuf "
    "${IBAPI4GU_REQUIRED_PROTOBUF_VERSION} generation marker."
  )
endif()

file(GLOB _ibapi4gu_client_sources LIST_DIRECTORIES FALSE
  "${_ibapi4gu_client_directory}/*.cpp"
)
file(GLOB _ibapi4gu_generated_sources LIST_DIRECTORIES FALSE
  "${_ibapi4gu_generated_directory}/*.cc"
  "${_ibapi4gu_generated_directory}/*.cpp"
)
if(NOT _ibapi4gu_client_sources OR NOT _ibapi4gu_generated_sources)
  message(FATAL_ERROR "The reviewed IBKR client source layout is incomplete.")
endif()

set(_ibapi4gu_decimal_source "${_ibapi4gu_client_directory}/Decimal.cpp")
list(REMOVE_ITEM _ibapi4gu_client_sources "${_ibapi4gu_decimal_source}")
set(_ibapi4gu_patched_decimal
  "${CMAKE_CURRENT_BINARY_DIR}/generated/ibkr-${IBAPI4GU_IBKR_VERSION}/Decimal.cpp"
)
ibapi4gu_create_patched_decimal(
  "${_ibapi4gu_decimal_source}"
  "${_ibapi4gu_patched_decimal}"
  "${IBAPI4GU_IBKR_DECIMAL_SHA256}"
)

add_library(ibapi4gu_twsapi STATIC
  ${_ibapi4gu_client_sources}
  ${_ibapi4gu_generated_sources}
  "${_ibapi4gu_patched_decimal}"
)
add_library(ibapi4gu::twsapi ALIAS ibapi4gu_twsapi)

set(_ibapi4gu_public_include_directories
  "${_ibapi4gu_cppclient_directory}"
  "${_ibapi4gu_client_directory}"
  "${_ibapi4gu_generated_directory}"
)
foreach(_optional_include_directory IN ITEMS
    "${_ibapi4gu_client_directory}/include"
    "${_ibapi4gu_cppclient_directory}/shared")
  if(IS_DIRECTORY "${_optional_include_directory}")
    list(APPEND _ibapi4gu_public_include_directories
      "${_optional_include_directory}"
    )
  endif()
endforeach()

target_include_directories(ibapi4gu_twsapi SYSTEM PUBLIC
  ${_ibapi4gu_public_include_directories}
)
target_compile_features(ibapi4gu_twsapi PUBLIC cxx_std_20)
target_compile_definitions(ibapi4gu_twsapi
  PUBLIC IBAPI4GU_TWSAPI_VERSION="${IBAPI4GU_IBKR_VERSION}"
  PRIVATE
    $<$<BOOL:${WIN32}>:NOMINMAX>
    $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN>
)
target_compile_options(ibapi4gu_twsapi PRIVATE
  "$<$<CXX_COMPILER_ID:GNU>:-Wno-missing-requires>"
  "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-deprecated-declarations>"
)
target_link_libraries(ibapi4gu_twsapi
  PUBLIC protobuf::libprotobuf
  PRIVATE ibapi4gu_intelrdfp
)

if(WIN32)
  target_link_libraries(ibapi4gu_twsapi PRIVATE ws2_32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(ibapi4gu_twsapi PRIVATE Threads::Threads)
endif()

set_target_properties(ibapi4gu_twsapi PROPERTIES
  OUTPUT_NAME ibapi4gu_twsapi
  FOLDER "dependencies/ibkr"
)

set(IBAPI4GU_RESOLVED_IBKR_SOURCE_DIR "${_ibapi4gu_ibkr_source_root}"
  CACHE INTERNAL "Resolved IBKR source root"
)

message(STATUS
  "ibapi4gu: IBKR ${IBAPI4GU_IBKR_VERSION}, "
  "Protobuf ${IBAPI4GU_REQUIRED_PROTOBUF_VERSION}, Intel RDFP ${IBAPI4GU_INTEL_RDFP_VERSION}"
)
