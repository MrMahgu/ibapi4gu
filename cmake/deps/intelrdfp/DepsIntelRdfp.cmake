include_guard(GLOBAL)
include(FetchContent)

FetchContent_Declare(
  intelrdfp
  URL "${IBAPI4GU_INTEL_RDFP_URL}"
  URL_HASH "SHA256=${IBAPI4GU_INTEL_RDFP_SHA256}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(intelrdfp)

set(_ibapi4gu_intelrdfp_source_directory "${intelrdfp_SOURCE_DIR}/LIBRARY/src")
file(GLOB _ibapi4gu_intelrdfp_sources LIST_DIRECTORIES FALSE
  "${_ibapi4gu_intelrdfp_source_directory}/*.c"
)

if(NOT _ibapi4gu_intelrdfp_sources)
  message(FATAL_ERROR
    "Intel RDFP sources were not found under "
    "${_ibapi4gu_intelrdfp_source_directory}."
  )
endif()

add_library(ibapi4gu_intelrdfp STATIC ${_ibapi4gu_intelrdfp_sources})
target_include_directories(ibapi4gu_intelrdfp PRIVATE
  "${_ibapi4gu_intelrdfp_source_directory}"
)
target_compile_definitions(ibapi4gu_intelrdfp PRIVATE DECIMAL_CALL_BY_REFERENCE=0)
set_target_properties(ibapi4gu_intelrdfp PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS NO
  FOLDER "dependencies/intel-rdfp"
)
