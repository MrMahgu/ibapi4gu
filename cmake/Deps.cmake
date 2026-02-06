include_guard(GLOBAL)

add_library(ibapi_stack_deps INTERFACE)
add_library(ibapi_stack_deps_common INTERFACE)

target_link_libraries(ibapi_stack_deps_common INTERFACE
  project_options
  project_warnings
)

add_library(ibapi_stack_deps_boost      INTERFACE)
add_library(ibapi_stack_deps_protobuf   INTERFACE)
add_library(ibapi_stack_deps_intelrdfp  INTERFACE)
add_library(ibapi_stack_deps_ibkr       INTERFACE)

target_link_libraries(ibapi_stack_deps_boost      INTERFACE ibapi_stack_deps_common)
target_link_libraries(ibapi_stack_deps_protobuf   INTERFACE ibapi_stack_deps_common)
target_link_libraries(ibapi_stack_deps_ibkr       INTERFACE ibapi_stack_deps_common)

target_link_libraries(ibapi_stack_deps_intelrdfp  INTERFACE project_options)

add_library(ibapi_stack::deps           ALIAS ibapi_stack_deps)
add_library(ibapi_stack::deps_boost     ALIAS ibapi_stack_deps_boost)
add_library(ibapi_stack::deps_protobuf  ALIAS ibapi_stack_deps_protobuf)
add_library(ibapi_stack::deps_intelrdfp ALIAS ibapi_stack_deps_intelrdfp)
add_library(ibapi_stack::deps_ibkr      ALIAS ibapi_stack_deps_ibkr)

include(${CMAKE_CURRENT_LIST_DIR}/deps/boost/DepsBoost.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/deps/protobuf/DepsProtobuf.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/deps/intelrdfp/DepsIntelRdfp.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/deps/ibkr/DepsIBKR.cmake)

target_link_libraries(ibapi_stack_deps_intelrdfp INTERFACE intelrdfp::bid)
target_link_libraries(ibapi_stack_deps INTERFACE
  ibapi_stack_deps_boost
  ibapi_stack_deps_protobuf
  ibapi_stack_deps_intelrdfp
  ibapi_stack_deps_ibkr
)
