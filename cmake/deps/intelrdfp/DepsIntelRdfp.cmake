include_guard(GLOBAL)
include(FetchContent)

set(IBAPI_RDFP_VERSION "2.3" CACHE STRING "IntelRDFPMathLib version tag (vX.Y)")

FetchContent_Declare(
  intelrdfp
  GIT_REPOSITORY https://github.com/xmake-mirror/IntelRDFPMathLib.git
  GIT_TAG        v${IBAPI_RDFP_VERSION}
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
  USES_TERMINAL_DOWNLOAD TRUE
)

FetchContent_GetProperties(intelrdfp)
if(NOT intelrdfp_POPULATED)
  message(STATUS "Git: Fetching IntelRDFPMathLib v${IBAPI_RDFP_VERSION}")
endif()

FetchContent_MakeAvailable(intelrdfp)

set(INTELRDFP_ROOT "${intelrdfp_SOURCE_DIR}")
set(INTELRDFP_LIB_DIR "${INTELRDFP_ROOT}/LIBRARY")

file(GLOB_RECURSE INTELRDFP_SOURCES CONFIGURE_DEPENDS
  "${INTELRDFP_LIB_DIR}/src/*.c"
)

if(NOT INTELRDFP_SOURCES)
  message(FATAL_ERROR "Could not find IntelRDFPMathLib sources under ${INTELRDFP_LIB_DIR}/src")
endif()

# Project-owned target
add_library(intelrdfp_bid STATIC)
target_sources(intelrdfp_bid PRIVATE ${INTELRDFP_SOURCES})

add_library(intelrdfp::bid ALIAS intelrdfp_bid)

target_include_directories(intelrdfp_bid
  PUBLIC
    "${INTELRDFP_LIB_DIR}/src"
)

set_target_properties(intelrdfp_bid PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS NO
  FOLDER "deps/intelrdfp"
)

target_compile_definitions(intelrdfp_bid PRIVATE
  DECIMAL_CALL_BY_REFERENCE=0
)
