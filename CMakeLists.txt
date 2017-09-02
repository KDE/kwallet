cmake_minimum_required(VERSION 3.0)

set(KF5_VERSION "5.38.0") # handled by release scripts
set(KF5_DEP_VERSION "5.37.0") # handled by release scripts
project(KWallet VERSION ${KF5_VERSION})
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include(FeatureSummary)
find_package(ECM 5.38.0  NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://projects.kde.org/projects/kdesupport/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(REQUIRED_QT_VERSION 5.7.0)
find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Widgets DBus)

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)

include(ECMAddQch)
include(ECMSetupVersion)

option(BUILD_KWALLETD "Build the kwallet daemon" ON)
option(BUILD_KWALLET_QUERY "Build kwallet-query tool" ON)
option(BUILD_QCH "Build API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)" OFF)
add_feature_info(QCH ${BUILD_QCH} "API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)")

find_package(KF5CoreAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Config ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5WindowSystem ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5I18n ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5DocTools ${KF5_DEP_VERSION})

add_definitions(-DTRANSLATION_DOMAIN=\"kwalletd5\")
if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/po")
    ki18n_install(po)
endif()
add_subdirectory(src)
add_subdirectory(autotests)
add_subdirectory(tests)
if (KF5DocTools_FOUND)
    add_subdirectory(docs)
endif()

feature_summary(WHAT ALL   FATAL_ON_MISSING_REQUIRED_PACKAGES)
