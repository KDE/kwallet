include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../src/api/KWallet ) # for kwallet.h
include_directories( ${CMAKE_CURRENT_BINARY_DIR}/../../src/api/KWallet )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../src/runtime/kwalletd ) # kwalletwizard.h
include_directories( ${CMAKE_CURRENT_BINARY_DIR}/../../src/runtime/kwalletd/backend ) # kwalletwizard.h

include(ECMMarkAsTest)

find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Test)
find_package(KF5CoreAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5I18n ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5IconThemes ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5WidgetsAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(Gpgmepp) # provided by GpgME
set(GPGMEPP_LIBS Gpgmepp)

if (NOT Gpgmepp_FOUND)
    find_package(KF5Gpgmepp) # provided by KF5Gpgmepp
    set(GPGMEPP_LIBS KF5::Gpgmepp)
endif(NOT Gpgmepp_FOUND)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

set(kwalletwizardtest_SRCS
   kwalletwizardtest.cpp
   ../../src/runtime/kwalletd/kwalletwizard.cpp
)
qt5_wrap_ui(kwalletwizardtest_SRCS
   ../../src/runtime/kwalletd/kbetterthankdialogbase.ui
   ../../src/runtime/kwalletd/kwalletwizardpageexplanation.ui
   ../../src/runtime/kwalletd/kwalletwizardpageintro.ui
   ../../src/runtime/kwalletd/kwalletwizardpageoptions.ui
   ../../src/runtime/kwalletd/kwalletwizardpagepassword.ui
   ../../src/runtime/kwalletd/kwalletwizardpagegpgkey.ui
)

if (Gpgmepp_FOUND OR KF5Gpgmepp_FOUND)
    qt5_wrap_ui(kwalletwizardtest_SRCS
        ../../src/runtime/kwalletd/kwalletwizardpagepasswordgpg.ui)
endif(Gpgmepp_FOUND OR KF5Gpgmepp_FOUND)

add_executable(kwalletwizardtest ${kwalletwizardtest_SRCS})
ecm_mark_as_test(kwalletwizardtest)
target_link_libraries(kwalletwizardtest
    KF5Wallet
    Qt5::Test
    KF5::CoreAddons
    KF5::I18n
    KF5::IconThemes
    KF5::WidgetsAddons)
if (Gpgmepp_FOUND OR KF5Gpgmepp_FOUND)
    target_link_libraries(kwalletwizardtest ${GPGMEPP_LIBS})
endif(Gpgmepp_FOUND OR KF5Gpgmepp_FOUND)

set(kwallettestlib_SRCS kwallettest.cpp)
add_library(kwallettestlib STATIC ${kwallettestlib_SRCS})
target_link_libraries(kwallettestlib KF5Wallet Qt5::Widgets)

macro(kwalletd_tests)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    ecm_mark_as_test(${_testname})
    target_link_libraries(${_testname}
        kwallettestlib
        kwalletbackend5
        KF5Wallet
        Qt5::DBus
        Qt5::Test
        KF5::CoreAddons
        KF5::I18n)
  endforeach(_testname)
endmacro()

kwalletd_tests(
    kwalletasync
    kwalletsync
    kwalletpath
    kwalletboth
    kwalletmany
    kwalletcbc
)


