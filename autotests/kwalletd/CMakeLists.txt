add_definitions(-DTEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/")

include(ECMAddTests)
find_package(Qt5Test ${REQUIRED_QT_VERSION} CONFIG QUIET)
find_package(Qt5Network ${REQUIRED_QT_VERSION} CONFIG QUIET)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/
    ${CMAKE_BINARY_DIR}
)

if(NOT Qt5Test_FOUND)
    message(STATUS "Qt5Test not found, autotests will not be built.")
    return()
endif()

macro(KWALLETD_TESTS)
    foreach(_testname ${ARGN})
        set(_srcs ${_testname}.cpp kwalletexecuter.cpp)
        ecm_add_test(${_srcs} TEST_NAME kwalletd-${_testname} LINK_LIBRARIES Qt5::Test Qt5::Core Qt5::DBus Qt5::Network KF5::Wallet KF5::ConfigCore)
    endforeach(_testname)
endmacro()

KWALLETD_TESTS(
  testpamopen
  testpamopennofile
)

# set(testname testpamopen)
# include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})
# ecm_add_unit_test(${testname} kwalletexecuter.cpp ${testname}.cpp)
# target_link_libraries(${testname} ${QT_QTTEST_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${KDE4_KDECORE_LIBS})
#
# set(testname testpamopennofile)
# include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})
# kde4_add_unit_test(${testname} kwalletexecuter.cpp ${testname}.cpp)
# target_link_libraries(${testname} ${QT_QTTEST_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${KDE4_KDECORE_LIBS})
