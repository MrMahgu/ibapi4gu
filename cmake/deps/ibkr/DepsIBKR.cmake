include_guard(GLOBAL)
include(ExternalProject)

set(IBKR_TWSAPI_VERSION "1044.01" CACHE STRING "IBKR TWS API version (e.g. 1044.01)")
set(IBKR_FETCH_TWSAPI  OFF        CACHE BOOL   "Allow fetching/extracting/building IBKR TWS API during build")

# Default MSI everywhere; on WSL/Linux, you may opt into ZIP:
set(FORCE_UNIX_SOURCE OFF CACHE BOOL "On WSL/Linux: use macunix zip instead of MSI (default MSI everywhere)")

set(IBKR_TWSAPI_WIN32_URL
  "https://interactivebrokers.github.io/downloads/TWS%20API%20Install%20${IBKR_TWSAPI_VERSION}.msi"
  CACHE STRING "IBKR TWS API MSI URL (default for all platforms)"
)

set(IBKR_TWSAPI_UNIX_URL
  "https://interactivebrokers.github.io/downloads/twsapi_macunix.${IBKR_TWSAPI_VERSION}.zip"
  CACHE STRING "IBKR TWS API zip URL (WSL/Linux only when FORCE_UNIX_SOURCE=ON)"
)

set(IBKR_TWSAPI_SHA256_MSI "" CACHE STRING "Optional SHA256 for MSI (hex)")
set(IBKR_TWSAPI_SHA256_ZIP "" CACHE STRING "Optional SHA256 for ZIP (hex)")

set(IBKR_IBAPI_EXTRACT_ROOT "" CACHE PATH "Path to already-extracted IBKR TWS API root; skips download/extract")

set(_ibkr_is_unix (UNIX AND NOT APPLE AND NOT WIN32))

set(_ibkr_kind "msi")
set(_ibkr_url  "${IBKR_TWSAPI_WIN32_URL}")

if(_ibkr_is_unix AND FORCE_UNIX_SOURCE)
  set(_ibkr_kind "zip")
  set(_ibkr_url  "${IBKR_TWSAPI_UNIX_URL}")
endif()

message(STATUS "IBKR: version=${IBKR_TWSAPI_VERSION} kind=${_ibkr_kind} FORCE_UNIX_SOURCE=${FORCE_UNIX_SOURCE}")

# MSI layout uses "client/protobuf"; zip (macunix) uses "client/protobufUnix".
set(_ibkr_client_protobuf_dir "protobuf")
if(_ibkr_kind STREQUAL "zip")
  set(_ibkr_client_protobuf_dir "protobufUnix")
endif()
if(NOT DEFINED IBKR_CLIENT_PROTOBUF_DIR OR IBKR_CLIENT_PROTOBUF_DIR STREQUAL "")
  set(IBKR_CLIENT_PROTOBUF_DIR "${_ibkr_client_protobuf_dir}" CACHE STRING
      "IBKR C++ client protobuf dir (protobuf or protobufUnix)")
endif()

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
    "${_prefix}/include/client/${IBKR_CLIENT_PROTOBUF_DIR}"
    "${_prefix}/include/client/include"
    "${_prefix}/include/client/include/${IBKR_CLIENT_PROTOBUF_DIR}"
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

set(_ibkr_url_hash "")
if(_ibkr_kind STREQUAL "msi" AND IBKR_TWSAPI_SHA256_MSI)
  set(_ibkr_url_hash "URL_HASH SHA256=${IBKR_TWSAPI_SHA256_MSI}")
elseif(_ibkr_kind STREQUAL "zip" AND IBKR_TWSAPI_SHA256_ZIP)
  set(_ibkr_url_hash "URL_HASH SHA256=${IBKR_TWSAPI_SHA256_ZIP}")
endif()

set(_ibkr_build_cmd   "${CMAKE_COMMAND}" --build "${_ibkr_build}")
set(_ibkr_install_cmd "${CMAKE_COMMAND}" --build "${_ibkr_build}" --target install)

if(CMAKE_GENERATOR_MULTI_CONFIG)
  list(APPEND _ibkr_build_cmd   --config "$<CONFIG>")
  list(APPEND _ibkr_install_cmd --config "$<CONFIG>")
endif()

if(NOT IBKR_IBAPI_EXTRACT_ROOT)

  # Non-Windows MSI extraction requires msiextract
  if((NOT WIN32) AND (_ibkr_kind STREQUAL "msi"))
    find_program(_ibkr_msiextract NAMES msiextract)
    if(NOT _ibkr_msiextract)
      message(FATAL_ERROR
        "IBKR MSI extraction on WSL/Linux requires 'msiextract' (msitools).\n"
        "Install msitools OR set -DFORCE_UNIX_SOURCE=ON to use the zip on WSL/Linux.")
    endif()
  endif()

  if(_ibkr_kind STREQUAL "msi")
    set(_ibkr_download_name "ibkr_twsapi.msi")
  else()
    set(_ibkr_download_name "ibkr_twsapi.zip")
  endif()

  set(_ibkr_ep_cmake_args
    "-DCMAKE_INSTALL_PREFIX:PATH=${_ibkr_install}"
    "-DIBKR_EXTRACT_ROOT:PATH=${_ibkr_extract}"
    "-DIBKR_VERBOSE_LAYOUT:BOOL=OFF"
    "-DIBKR_CLIENT_PROTOBUF_DIR:STRING=${IBKR_CLIENT_PROTOBUF_DIR}"
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
    DOWNLOAD_NAME "${_ibkr_download_name}"
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

  # extraction step (msiexec wants native paths)
  if(_ibkr_kind STREQUAL "msi")
    if(WIN32)
      file(TO_NATIVE_PATH "${_ibkr_dl}/ibkr_twsapi.msi" _ibkr_msi_native)
      file(TO_NATIVE_PATH "${_ibkr_extract}"            _ibkr_extract_native)

      ExternalProject_Add_Step(${_ibkr_ep_name} extract_vendor
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_ibkr_extract}"
        COMMAND ${CMAKE_COMMAND} -E chdir "${_ibkr_extract}"
            msiexec /a "${_ibkr_msi_native}" /qn "TARGETDIR=${_ibkr_extract_native}"
        DEPENDEES download
        DEPENDERS configure
        ALWAYS FALSE
      )
    else()
      ExternalProject_Add_Step(${_ibkr_ep_name} extract_vendor
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_ibkr_extract}"
        COMMAND ${CMAKE_COMMAND} -E chdir "${_ibkr_extract}"
           "${_ibkr_msiextract}" "${_ibkr_dl}/ibkr_twsapi.msi"
        WORKING_DIRECTORY "${_ibkr_prefix}"
        DEPENDEES download
        DEPENDERS configure
        ALWAYS FALSE
      )
    endif()
  else()
    ExternalProject_Add_Step(${_ibkr_ep_name} extract_vendor
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_ibkr_extract}"
      COMMAND ${CMAKE_COMMAND} -E chdir "${_ibkr_extract}"
          ${CMAKE_COMMAND} -E tar xvf "${_ibkr_dl}/ibkr_twsapi.zip" --format=zip
      DEPENDEES download
      DEPENDERS configure
      ALWAYS FALSE
    )
  endif()

  
  _ibkr_define_imported_target("${_ibkr_install}")
  add_dependencies(IBKR::ibapi ${_ibkr_ep_name})

else()
  # Local wrapper build path (configure-time)
  if(NOT DEFINED IBKR_CLIENT_PROTOBUF_DIR OR IBKR_CLIENT_PROTOBUF_DIR STREQUAL "")
    set(IBKR_CLIENT_PROTOBUF_DIR "${_ibkr_client_protobuf_dir}" CACHE STRING
        "IBKR C++ client protobuf dir (protobuf or protobufUnix)")
  endif()
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
