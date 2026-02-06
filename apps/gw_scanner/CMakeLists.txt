add_executable(gw_scanner
  main.cpp
)

target_link_libraries(gw_scanner
  PRIVATE
    ibapi_stack::deps
)

set_property(TARGET gw_scanner PROPERTY FOLDER "apps")

set_target_properties(gw_scanner PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/rundir/bin"
)
