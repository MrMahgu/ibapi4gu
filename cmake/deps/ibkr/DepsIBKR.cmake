include_guard(GLOBAL)
include(ExternalProject)

set(IBKR_TWSAPI_VERSION "1051.01" CACHE STRING "IBKR TWS API version (e.g. 1051.01)")
set(IBKR_FETCH_TWSAPI  OFF        CACHE BOOL   "Allow fetching/extracting/building IBKR TWS API during build")

set(IBKR_TWSAPI_URL "" CACHE STRING "Optional IBKR TWS API zip URL override")
set(IBKR_TWSAPI_SHA256 "" CACHE STRING "Optional SHA256 for the IBKR TWS API zip (hex)")

set(IBKR_IBAPI_EXTRACT_ROOT "" CACHE PATH "Path to already-extracted IBKR TWS API root; skips download/extract")

string(REPLACE "." "_" _ibkr_twsapi_archive_version "${IBKR_TWSAPI_VERSION}")
if(IBKR_TWSAPI_URL)
  set(_ibkr_url "${IBKR_TWSAPI_URL}")
else()
  set(_ibkr_url
    "https://interactivebrokers.github.io/downloads/twsapi_${_ibkr_twsapi_archive_version}.zip")
endif()

message(STATUS "IBKR: version=${IBKR_TWSAPI_VERSION} url=${_ibkr_url}")

set(_ibkr_prefix  "${CMAKE_BINARY_DIR}/_deps/ibkr_twsapi-${IBKR_TWSAPI_VERSION}")
set(_ibkr_dl      "${_ibkr_prefix}/dl")
set(_ibkr_extract "${_ibkr_prefix}/extract")  # vendor payload extracted here
set(_ibkr_build   "${_ibkr_prefix}/build")    # wrapper build dir
set(_ibkr_install "${_ibkr_prefix}/install")  # wrapper install prefix

set(_ibkr_wrapper_src "${CMAKE_CURRENT_LIST_DIR}/ibkr_build")
set(_ibkr_ep_name "ibkr_twsapi")

file(MAKE_DIRECTORY "${_ibkr_dl}")

if(WIN32)
  set(_ibkr_out_lib "${_ibkr_install}/lib/ibkr_ibapi.lib")
else()
  set(_ibkr_out_lib "${_ibkr_install}/lib/libibkr_ibapi.a")
endif()

function(_ibkr_define_imported_target _prefix)
  if(NOT TARGET IBKR::ibapi)
    add_library(IBKR::ibapi STATIC IMPORTED GLOBAL)
  endif()

  if(WIN32)
    set(_lib "${_prefix}/lib/ibkr_ibapi.lib")
  else()
    set(_lib "${_prefix}/lib/libibkr_ibapi.a")
  endif()

  set(_ibkr_inc_dirs
    "${_prefix}/include"
    "${_prefix}/include/client"
    "${_prefix}/include/client/protobuf"
    "${_prefix}/include/client/include"
    "${_prefix}/include/client/include/protobuf"
  )

  set_target_properties(IBKR::ibapi PROPERTIES
    IMPORTED_LOCATION "${_lib}"
    INTERFACE_INCLUDE_DIRECTORIES "${_ibkr_inc_dirs}"
  )

  if(CMAKE_GENERATOR_MULTI_CONFIG)
    foreach(cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
      string(TOUPPER "${cfg}" cfg_u)
      set_target_properties(IBKR::ibapi PROPERTIES
        "IMPORTED_LOCATION_${cfg_u}" "${_lib}"
      )
    endforeach()
  else()
    set_target_properties(IBKR::ibapi PROPERTIES
      IMPORTED_LOCATION "${_lib}"
    )
  endif()

  if(TARGET protobuf::libprotobuf)
    target_link_libraries(IBKR::ibapi INTERFACE protobuf::libprotobuf)
  endif()
  if(TARGET protobuf::libprotobuf-lite)
    target_link_libraries(IBKR::ibapi INTERFACE protobuf::libprotobuf-lite)
  endif()
  if(TARGET intelrdfp::bid)
    target_link_libraries(IBKR::ibapi INTERFACE intelrdfp::bid)
  endif()
  set_property(TARGET IBKR::ibapi APPEND PROPERTY
    INTERFACE_COMPILE_DEFINITIONS "IBKR_TWSAPI_VERSION=\"${IBKR_TWSAPI_VERSION}\""
  )
endfunction()

if(IBKR_IBAPI_EXTRACT_ROOT)
  set(_ibkr_extract "${IBKR_IBAPI_EXTRACT_ROOT}")
else()
  if(NOT IBKR_FETCH_TWSAPI)
    message(FATAL_ERROR
      "IBKR_FETCH_TWSAPI=OFF and IBKR_IBAPI_EXTRACT_ROOT is empty.\n"
      "Either set -DIBKR_FETCH_TWSAPI=ON (build-time vendor fetch/extract)\n"
      "or provide -DIBKR_IBAPI_EXTRACT_ROOT=/path/to/extracted/root.")
  endif()
endif()

set(_ibkr_url_hash)
if(IBKR_TWSAPI_SHA256)
  set(_ibkr_url_hash URL_HASH "SHA256=${IBKR_TWSAPI_SHA256}")
endif()

set(_ibkr_build_cmd   "${CMAKE_COMMAND}" --build "${_ibkr_build}")
set(_ibkr_install_cmd "${CMAKE_COMMAND}" --build "${_ibkr_build}" --target install)

if(CMAKE_GENERATOR_MULTI_CONFIG)
  list(APPEND _ibkr_build_cmd   --config "$<CONFIG>")
  list(APPEND _ibkr_install_cmd --config "$<CONFIG>")
endif()

if(NOT IBKR_IBAPI_EXTRACT_ROOT)
  set(_ibkr_ep_cmake_args
    "-DCMAKE_INSTALL_PREFIX:PATH=${_ibkr_install}"
    "-DIBKR_EXTRACT_ROOT:PATH=${_ibkr_extract}"
    "-DIBKR_VERBOSE_LAYOUT:BOOL=OFF"
  )
  if(MSVC AND DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE)
    list(APPEND _ibkr_ep_cmake_args
      "-DCMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}"
    )
  endif()
  if(NOT CMAKE_GENERATOR_MULTI_CONFIG AND CMAKE_BUILD_TYPE)
    list(APPEND _ibkr_ep_cmake_args
      "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
    )
  endif()
  if(MSVC AND CMAKE_MSVC_RUNTIME_LIBRARY)
    list(APPEND _ibkr_ep_cmake_args
      "-DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=${CMAKE_MSVC_RUNTIME_LIBRARY}"
    )
  endif()
  if(DEFINED protobuf_SOURCE_DIR)
    list(APPEND _ibkr_ep_cmake_args
      "-DIBKR_PROTOBUF_INCLUDE_DIR:PATH=${protobuf_SOURCE_DIR}/src"
      "-DIBKR_ABSEIL_INCLUDE_DIR:PATH=${protobuf_SOURCE_DIR}/third_party/abseil-cpp"
    )
  endif()

  ExternalProject_Add(${_ibkr_ep_name}
    PREFIX        "${_ibkr_prefix}"
    DOWNLOAD_DIR  "${_ibkr_dl}"
    SOURCE_DIR    "${_ibkr_wrapper_src}"  # wrapper CMakeLists.txt lives here
    BINARY_DIR    "${_ibkr_build}"
    INSTALL_DIR   "${_ibkr_install}"

    URL           "${_ibkr_url}"
    DOWNLOAD_NAME "ibkr_twsapi.zip"
    DOWNLOAD_NO_EXTRACT 1

    ${_ibkr_url_hash}

    UPDATE_COMMAND ""

    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_ARGS
      ${_ibkr_ep_cmake_args}

    BUILD_COMMAND   ${_ibkr_build_cmd}
    INSTALL_COMMAND ${_ibkr_install_cmd}

    BUILD_BYPRODUCTS "${_ibkr_out_lib}"
  )

  ExternalProject_Add_Step(${_ibkr_ep_name} extract_vendor
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_ibkr_extract}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${_ibkr_extract}"
        ${CMAKE_COMMAND} -E tar xvf "<DOWNLOADED_FILE>" --format=zip
    DEPENDEES download
    DEPENDERS configure
    ALWAYS FALSE
  )

  _ibkr_define_imported_target("${_ibkr_install}")
  add_dependencies(IBKR::ibapi ${_ibkr_ep_name})

else()
  # Local wrapper build path (configure-time)
  if(DEFINED protobuf_SOURCE_DIR)
    set(IBKR_PROTOBUF_INCLUDE_DIR "${protobuf_SOURCE_DIR}/src" CACHE PATH "Protobuf include dir")
    set(IBKR_ABSEIL_INCLUDE_DIR "${protobuf_SOURCE_DIR}/third_party/abseil-cpp" CACHE PATH "Abseil include dir")
  endif()
  add_subdirectory("${_ibkr_wrapper_src}" "${CMAKE_BINARY_DIR}/_deps/ibkr_ibapi_local_build" EXCLUDE_FROM_ALL)

  if(NOT TARGET ibkr_ibapi)
    message(FATAL_ERROR "Expected wrapper target 'ibkr_ibapi' from ${_ibkr_wrapper_src} but it was not defined.")
  endif()

  if(NOT TARGET IBKR::ibapi)
    add_library(IBKR::ibapi ALIAS ibkr_ibapi)
  endif()

endif()

if(TARGET ibapi_stack_deps_ibkr)
  target_link_libraries(ibapi_stack_deps_ibkr INTERFACE IBKR::ibapi)
endif()
