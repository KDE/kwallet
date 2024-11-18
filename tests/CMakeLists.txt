
add_subdirectory(KWallet)

if(BUILD_KSECRETD)
    add_subdirectory(kwalletd)
endif()
