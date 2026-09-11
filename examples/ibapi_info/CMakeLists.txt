add_executable(ibapi_info main.cpp)
target_link_libraries(ibapi_info PRIVATE
  ibapi4gu::twsapi
  ibapi4gu_project_options
  ibapi4gu_project_warnings
)
set_property(TARGET ibapi_info PROPERTY FOLDER "examples")

if(CMAKE_CONFIGURATION_TYPES)
  foreach(_configuration IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${_configuration}" _configuration_upper)
    set_target_properties(ibapi_info PROPERTIES
      "RUNTIME_OUTPUT_DIRECTORY_${_configuration_upper}"
      "${CMAKE_BINARY_DIR}/bin/${_configuration}"
    )
  endforeach()
else()
  set_target_properties(ibapi_info PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
endif()
