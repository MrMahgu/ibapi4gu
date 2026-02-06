cmake_minimum_required(VERSION 3.28.3)

project(ibkr_ibapi_wrapper LANGUAGES C CXX)

set(IBKR_EXTRACT_ROOT "" CACHE PATH "Root directory of extracted IBKR TWS API content")
set(IBKR_VERBOSE_LAYOUT OFF CACHE BOOL "Print layout discovery info")
set(IBKR_PROTOBUF_INCLUDE_DIRS "" CACHE STRING "Extra include dirs for protobuf headers")
set(IBKR_PROTOBUF_INCLUDE_DIR "" CACHE PATH "Protobuf include dir (protobuf-src/src)")
set(IBKR_ABSEIL_INCLUDE_DIR "" CACHE PATH "Abseil include dir (abseil-cpp)")
set(IBKR_CLIENT_PROTOBUF_DIR "" CACHE STRING "IBKR client protobuf dir (protobuf or protobufUnix)")

if(NOT IBKR_EXTRACT_ROOT)
  message(FATAL_ERROR "IBKR_EXTRACT_ROOT is required")
endif()

file(GLOB_RECURSE _anchor CONFIGURE_DEPENDS
  "${IBKR_EXTRACT_ROOT}/*/TWS API/source/cppclient/client/EClientSocket.cpp"
  "${IBKR_EXTRACT_ROOT}/*/TWS API/source/CppClient/client/EClientSocket.cpp"
  "${IBKR_EXTRACT_ROOT}/*/cppclient/client/EClientSocket.cpp"
  "${IBKR_EXTRACT_ROOT}/*/CppClient/client/EClientSocket.cpp"
  "${IBKR_EXTRACT_ROOT}/*/source/cppclient/client/EClientSocket.cpp"
  "${IBKR_EXTRACT_ROOT}/*/source/CppClient/client/EClientSocket.cpp"
)

if(NOT _anchor)
  message(FATAL_ERROR
    "Could not locate IBAPI C++ client sources under IBKR_EXTRACT_ROOT='${IBKR_EXTRACT_ROOT}'.\n"
    "Expected to find something like: */source/cppclient/client/EClientSocket.cpp")
endif()

list(GET _anchor 0 _anchor0)

get_filename_component(_client_dir "${_anchor0}" DIRECTORY)
get_filename_component(_cppclient_dir "${_client_dir}" DIRECTORY)

set(IBKR_CPPCLIENT_ROOT "${_cppclient_dir}")

if(IBKR_VERBOSE_LAYOUT)
  message(STATUS "IBKR_EXTRACT_ROOT   = ${IBKR_EXTRACT_ROOT}")
  message(STATUS "IBKR_CPPCLIENT_ROOT = ${IBKR_CPPCLIENT_ROOT}")
endif()

file(GLOB_RECURSE IBKR_IBAPI_SOURCES CONFIGURE_DEPENDS
  "${IBKR_CPPCLIENT_ROOT}/*.cpp"
  "${IBKR_CPPCLIENT_ROOT}/*.cc"
)

if(NOT IBKR_IBAPI_SOURCES)
  message(FATAL_ERROR "No .cpp files found under ${IBKR_CPPCLIENT_ROOT}")
endif()

add_library(ibkr_ibapi STATIC ${IBKR_IBAPI_SOURCES})

set(_ibkr_client_pb_dir "${IBKR_CLIENT_PROTOBUF_DIR}")
if(_ibkr_client_pb_dir STREQUAL "")
  if(EXISTS "${IBKR_CPPCLIENT_ROOT}/client/protobufUnix")
    set(_ibkr_client_pb_dir "protobufUnix")
  else()
    set(_ibkr_client_pb_dir "protobuf")
  endif()
endif()
set(_ibkr_client_pb_path "${IBKR_CPPCLIENT_ROOT}/client/${_ibkr_client_pb_dir}")
if(NOT EXISTS "${_ibkr_client_pb_path}")
  message(FATAL_ERROR "IBKR client protobuf dir not found: ${_ibkr_client_pb_path}")
endif()

target_include_directories(ibkr_ibapi
  PUBLIC
    "${IBKR_CPPCLIENT_ROOT}"
    "${IBKR_CPPCLIENT_ROOT}/client"
    "${_ibkr_client_pb_path}"
    "${IBKR_CPPCLIENT_ROOT}/client/include"
    "${IBKR_CPPCLIENT_ROOT}/client/include/${_ibkr_client_pb_dir}"
    "${IBKR_CPPCLIENT_ROOT}/shared"
    "${IBKR_CPPCLIENT_ROOT}/samples"
)

if(IBKR_PROTOBUF_INCLUDE_DIRS)
  target_include_directories(ibkr_ibapi PRIVATE ${IBKR_PROTOBUF_INCLUDE_DIRS})
endif()
if(IBKR_PROTOBUF_INCLUDE_DIR)
  target_include_directories(ibkr_ibapi PRIVATE "${IBKR_PROTOBUF_INCLUDE_DIR}")
endif()
if(IBKR_ABSEIL_INCLUDE_DIR)
  target_include_directories(ibkr_ibapi PRIVATE "${IBKR_ABSEIL_INCLUDE_DIR}")
endif()

target_compile_features(ibkr_ibapi PUBLIC cxx_std_20)

if(WIN32)
  target_link_libraries(ibkr_ibapi PRIVATE ws2_32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(ibkr_ibapi PRIVATE Threads::Threads)
endif()

set_target_properties(ibkr_ibapi PROPERTIES OUTPUT_NAME "ibkr_ibapi")

include(GNUInstallDirs)

install(TARGETS ibkr_ibapi
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(DIRECTORY "${IBKR_CPPCLIENT_ROOT}/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
