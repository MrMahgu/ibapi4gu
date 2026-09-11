cmake_minimum_required(VERSION 3.28.3...4.4)

project(ibapi4gu VERSION 0.1.0 LANGUAGES C CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(WIN32)
  set(_ibapi4gu_default_distribution "WINDOWS")
else()
  set(_ibapi4gu_default_distribution "UNIX")
endif()

set(IBAPI4GU_IBKR_DISTRIBUTION "${_ibapi4gu_default_distribution}" CACHE STRING
  "IBKR source distribution to build (UNIX or WINDOWS)"
)
set_property(CACHE IBAPI4GU_IBKR_DISTRIBUTION PROPERTY STRINGS UNIX WINDOWS)
string(TOUPPER "${IBAPI4GU_IBKR_DISTRIBUTION}" IBAPI4GU_IBKR_DISTRIBUTION)

if(NOT IBAPI4GU_IBKR_DISTRIBUTION MATCHES "^(UNIX|WINDOWS)$")
  message(FATAL_ERROR
    "IBAPI4GU_IBKR_DISTRIBUTION must be UNIX or WINDOWS, got "
    "'${IBAPI4GU_IBKR_DISTRIBUTION}'."
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
