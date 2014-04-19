cmake_minimum_required(VERSION 2.8.12)

find_package(ECM 0.0.12 REQUIRED NO_MODULE)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(REQUIRED_QT_VERSION 5.2.0)
find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Widgets DBus)

set(KF5_VERSION "4.98.0")
include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings)
include(KDECMakeSettings)
include(FeatureSummary)

option(BUILD_KWALLETD "Build the kwallet daemon" ON)

find_package(LibGcrypt 1.5.0 REQUIRED QUIET)
set_package_properties(LibGcrypt PROPERTIES
                       TYPE REQUIRED
                       PURPOSE "kwalletd needs libgcrypt to perform PBKDF2-SHA512 hashing"
                      )

add_subdirectory(src)
add_subdirectory(autotests)
add_subdirectory(tests)

feature_summary(WHAT ALL   FATAL_ON_MISSING_REQUIRED_PACKAGES)
