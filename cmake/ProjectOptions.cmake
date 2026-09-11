function(ibapi4gu_project_options_setup)
  add_library(ibapi4gu_project_options INTERFACE)

  target_compile_features(ibapi4gu_project_options INTERFACE cxx_std_20)

  target_compile_definitions(ibapi4gu_project_options INTERFACE
    $<$<BOOL:${WIN32}>:NOMINMAX>
    $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN>
  )

  if(WIN32)
    if(DEFINED IBAPI4GU_WIN32_WINNT)
      target_compile_definitions(ibapi4gu_project_options INTERFACE
        _WIN32_WINNT=${IBAPI4GU_WIN32_WINNT}
      )
    endif()
    if(DEFINED IBAPI4GU_NTDDI_VERSION)
      target_compile_definitions(ibapi4gu_project_options INTERFACE
        _NTDDI_VERSION=${IBAPI4GU_NTDDI_VERSION}
      )
    endif()
  endif()

  if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
  endif()

  if(MSVC)
    target_compile_options(ibapi4gu_project_options INTERFACE
      /permissive-
      /Zc:__cplusplus
      /Zc:preprocessor
    )
  endif()
endfunction()
