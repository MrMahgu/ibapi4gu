cmake_minimum_required(VERSION 3.28.3)

project(ibapi_stack VERSION 0.0.1 LANGUAGES C CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectOptions)
include(ProjectWarnings)

project_options_setup()
project_warnings_setup()

include(Deps)

add_subdirectory(apps)
