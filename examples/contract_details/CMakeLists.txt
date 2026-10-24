add_executable(ibapi4gu_contract_details main.cpp)
target_link_libraries(ibapi4gu_contract_details PRIVATE
  ibapi4gu::api
  ibapi4gu_project_options
  ibapi4gu_project_warnings
)
set_target_properties(ibapi4gu_contract_details PROPERTIES
  FOLDER "examples"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)
