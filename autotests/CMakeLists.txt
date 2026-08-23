include(ECMAddTests)

find_package(Qt6Test ${REQUIRED_QT_VERSION} CONFIG REQUIRED)

if(BUILD_KSECRETD)
    ecm_add_tests(
        blowfishtest.cpp
        LINK_LIBRARIES Qt6::Test KF6WalletBackend
        )

    target_include_directories(blowfishtest PRIVATE ${CMAKE_SOURCE_DIR}/src/runtime/ksecretd
        ${CMAKE_BINARY_DIR}/src/runtime/kwalletbackend)
endif()
