set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

include_directories(
   ${CMAKE_CURRENT_SOURCE_DIR}/..
   ${CMAKE_CURRENT_BINARY_DIR}/..
)

include(ECMMarkAsTest)

find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Test)

find_package(KF5 CONFIG REQUIRED COMPONENTS WidgetsAddons CoreAddons Notifications)

# includes to let KDE4Support below find it's dependencies
find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Network PrintSupport)
find_package(KF5KDE4Support CONFIG REQUIRED NO_MODULE)

include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KDE4_INCLUDES} ${KF5_INCLUDE_DIRS})

macro(kwallet_executable_tests)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    ecm_mark_as_test(${_testname})
    target_link_libraries(${_testname} Qt5::Test KF5::Wallet kwalletbackend)
  endforeach(_testname)
endmacro()

kwallet_executable_tests(
  backendtest
  testbf
  testsha
)
