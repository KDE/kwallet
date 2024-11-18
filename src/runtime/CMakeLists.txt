if(BUILD_KSECRETD OR BUILD_KWALLETD)
    find_package(Gpgmepp 1.7.0) # provided by GpgME

    if (Gpgmepp_FOUND)
        message("GPG support enabled")
        add_definitions(-DHAVE_GPGMEPP)
        add_definitions(-DBOOST_NO_EXCEPTIONS)
        include_directories(${GPGME_INCLUDES})
    endif(Gpgmepp_FOUND)
    add_subdirectory(kwalletbackend)
endif()

if(BUILD_KSECRETD)
    add_subdirectory(ksecretd)
endif()

if(BUILD_KWALLETD)
    add_subdirectory(kwalletd)
endif()

if(BUILD_KWALLET_QUERY)
  add_subdirectory(kwallet-query)
endif()
