if(BUILD_KSECRETD)
    add_subdirectory(ksecretd)
endif()

if(BUILD_KWALLET_QUERY)
  add_subdirectory(kwallet-query)
endif()
