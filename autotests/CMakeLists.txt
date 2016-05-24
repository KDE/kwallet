include(ECMAddTests)

find_package(Qt5Test ${REQUIRED_QT_VERSION} CONFIG QUIET)

if(NOT Qt5Test_FOUND)
    message(STATUS "Qt5Test not found, autotests will not be built.")
    return()
endif()

ecm_add_tests(
    blowfishtest.cpp
    LINK_LIBRARIES Qt5::Test kwalletbackend5
    )

target_include_directories(blowfishtest PRIVATE ${CMAKE_SOURCE_DIR}/src/runtime/kwalletd
    ${CMAKE_BINARY_DIR}/src/runtime/kwalletd/backend)

add_subdirectory(KWallet)
