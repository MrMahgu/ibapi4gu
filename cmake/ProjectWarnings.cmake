function(project_warnings_setup)
  add_library(project_warnings INTERFACE)

  if(MSVC)
    target_compile_options(project_warnings INTERFACE
      /W4
      /w14242 /w14254 /w14263 /w14265 /w14287
      /we4289
      /w14296
      /w14311 /w14545 /w14546 /w14547 /w14549
      /w14555 /w14619 /w14640 /w14826 /w14905 /w14906 /w14928
    )
  else()
    target_compile_options(project_warnings INTERFACE
      -Wall -Wextra -Wpedantic
    )
  endif()  
endfunction()
