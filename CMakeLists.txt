cmake_minimum_required(VERSION 4.2...4.4)

project(ibapi4gu VERSION 0.1.0 LANGUAGES C CXX)

# Keep the language mode consistent across dependencies and consumers.
# Abseil's auto-selected compatibility types are ABI-sensitive, so one build
# must use one language mode throughout. Respect an explicitly supplied
# CMAKE_CXX_STANDARD when initializing a new build, otherwise default to C++26.
if(DEFINED CMAKE_CXX_STANDARD)
  set(_ibapi4gu_default_cxx_standard "${CMAKE_CXX_STANDARD}")
else()
  set(_ibapi4gu_default_cxx_standard "26")
endif()
set(IBAPI4GU_CXX_STANDARD "${_ibapi4gu_default_cxx_standard}" CACHE STRING
  "C++ standard for ibapi4gu, its dependencies, and consumers"
)
set_property(CACHE IBAPI4GU_CXX_STANDARD PROPERTY STRINGS 11 14 17 20 23 26)
if(NOT IBAPI4GU_CXX_STANDARD MATCHES "^(11|14|17|20|23|26)$")
  message(FATAL_ERROR
    "IBAPI4GU_CXX_STANDARD must be 11, 14, 17, 20, 23, or 26; got "
    "'${IBAPI4GU_CXX_STANDARD}'."
  )
endif()

# Some dependencies cache language-feature probes whose results affect ABI.
# Refuse in-place standard changes instead of allowing a mixed build tree.
if(DEFINED IBAPI4GU_CONFIGURED_CXX_STANDARD
   AND NOT IBAPI4GU_CONFIGURED_CXX_STANDARD STREQUAL IBAPI4GU_CXX_STANDARD)
  message(FATAL_ERROR
    "This build directory was configured for C++"
    "${IBAPI4GU_CONFIGURED_CXX_STANDARD} and cannot be changed to C++"
    "${IBAPI4GU_CXX_STANDARD}. Configure a separate build directory."
  )
endif()
set(IBAPI4GU_CONFIGURED_CXX_STANDARD "${IBAPI4GU_CXX_STANDARD}" CACHE INTERNAL
  "C++ standard fixed for this build directory"
)

# CMake 4.2 does not model MSVC C++26 yet. Its cxx_std_23 feature selects
# /std:c++latest, which is the compiler's C++26 preview mode.
if(MSVC AND IBAPI4GU_CXX_STANDARD EQUAL 26)
  set(CMAKE_CXX_STANDARD 23)
  set(IBAPI4GU_CXX_COMPILE_FEATURE cxx_std_23)
else()
  set(CMAKE_CXX_STANDARD "${IBAPI4GU_CXX_STANDARD}")
  set(IBAPI4GU_CXX_COMPILE_FEATURE
    "cxx_std_${IBAPI4GU_CXX_STANDARD}"
  )
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(WIN32)
  set(_ibapi4gu_default_distribution "WINDOWS")
else()
  set(_ibapi4gu_default_distribution "UNIX")
endif()

set(IBAPI4GU_IBKR_DISTRIBUTION "${_ibapi4gu_default_distribution}" CACHE STRING
  "IBKR generated source set to build (UNIX or WINDOWS)"
)
set_property(CACHE IBAPI4GU_IBKR_DISTRIBUTION PROPERTY STRINGS UNIX WINDOWS)
string(TOUPPER "${IBAPI4GU_IBKR_DISTRIBUTION}" IBAPI4GU_IBKR_DISTRIBUTION)

if(NOT IBAPI4GU_IBKR_DISTRIBUTION MATCHES "^(UNIX|WINDOWS)$")
  message(FATAL_ERROR
    "IBAPI4GU_IBKR_DISTRIBUTION must be UNIX or WINDOWS, got "
    "'${IBAPI4GU_IBKR_DISTRIBUTION}'."
  )
endif()

set(IBAPI4GU_IBKR_GENERATION "STABLE" CACHE STRING
  "IBKR generated Protobuf revision to build (STABLE or FUTURE)"
)
set_property(CACHE IBAPI4GU_IBKR_GENERATION PROPERTY STRINGS STABLE FUTURE)
string(TOUPPER "${IBAPI4GU_IBKR_GENERATION}" IBAPI4GU_IBKR_GENERATION)

if(NOT IBAPI4GU_IBKR_GENERATION MATCHES "^(STABLE|FUTURE)$")
  message(FATAL_ERROR
    "IBAPI4GU_IBKR_GENERATION must be STABLE or FUTURE, got "
    "'${IBAPI4GU_IBKR_GENERATION}'."
  )
endif()

option(IBAPI4GU_BUILD_EXAMPLES "Build ibapi4gu examples" ${PROJECT_IS_TOP_LEVEL})

include(CTest)

include(ProjectOptions)
include(ProjectWarnings)

ibapi4gu_project_options_setup()
ibapi4gu_project_warnings_setup()

include(Deps)

if(IBAPI4GU_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
