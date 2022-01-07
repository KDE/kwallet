include(ECMAddTests)

find_package(Qt${QT_MAJOR_VERSION}Test ${REQUIRED_QT_VERSION} CONFIG QUIET)

if(NOT TARGET Qt${QT_MAJOR_VERSION}::Test)
    message(STATUS "Qt${QT_MAJOR_VERSION}Test not found, autotests will not be built.")
    return()
endif()

ecm_add_tests(
    blowfishtest.cpp
    LINK_LIBRARIES Qt${QT_MAJOR_VERSION}::Test kwalletbackend5
    )

target_include_directories(blowfishtest PRIVATE ${CMAKE_SOURCE_DIR}/src/runtime/kwalletd
    ${CMAKE_BINARY_DIR}/src/runtime/kwalletd/backend)

add_subdirectory(KWallet)
