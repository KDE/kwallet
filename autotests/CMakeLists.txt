include(ECMAddTests)

find_package(Qt6Test ${REQUIRED_QT_VERSION} CONFIG QUIET)

if(NOT TARGET Qt6::Test)
    message(STATUS "Qt6Test not found, autotests will not be built.")
    return()
endif()

if(BUILD_KSECRETD)
    ecm_add_tests(
        blowfishtest.cpp
        LINK_LIBRARIES Qt6::Test KF6WalletBackend
        )

    target_include_directories(blowfishtest PRIVATE ${CMAKE_SOURCE_DIR}/src/runtime/ksecretd
        ${CMAKE_BINARY_DIR}/src/runtime/kwalletbackend)
endif()

add_subdirectory(KWallet)
