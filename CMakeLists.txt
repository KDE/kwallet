cmake_minimum_required(VERSION 3.16)

set(KF_VERSION "6.10.0") # handled by release scripts
set(KF_DEP_VERSION "6.10.0") # handled by release scripts
project(KWallet VERSION ${KF_VERSION})
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include(FeatureSummary)
find_package(ECM 6.10.0  NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://invent.kde.org/frameworks/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)

include(KDEGitCommitHooks)
include(ECMDeprecationSettings)

set(REQUIRED_QT_VERSION 6.6.0)
find_package(Qt6 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Widgets DBus)

include(ECMAddQch)
include(ECMGenerateExportHeader)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(ECMMarkNonGuiExecutable)

option(BUILD_KWALLETD "Build the kwallet daemon" ON)
option(BUILD_KWALLET_QUERY "Build kwallet-query tool" ON)
option(BUILD_QCH "Build API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)" OFF)
add_feature_info(QCH ${BUILD_QCH} "API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)")

if(NOT WIN32 AND NOT APPLE AND NOT HAIKU)
    option(WITH_X11 "Build with support for KWindowSystem::isPlatformX11()" ON)
endif()

# kwalletd contains functions that are marked as deprecated.
# These are part of the public DBus API and are used by older applications
# Therefore we must not exclude those by default
set(EXCLUDE_DEPRECATED_BEFORE_AND_AT 0 CACHE STRING "Control the range of deprecated API excluded from the build [default=0].")

find_package(KF6CoreAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6Config ${KF_DEP_VERSION} REQUIRED)
find_package(KF6WindowSystem ${KF_DEP_VERSION} REQUIRED)
find_package(KF6I18n ${KF_DEP_VERSION} REQUIRED)
find_package(KF6DocTools ${KF_DEP_VERSION})

ecm_set_disabled_deprecation_versions(
    QT 6.8.0
    KF 6.8.0
)

add_definitions(-DTRANSLATION_DOMAIN=\"kwalletd6\")
ki18n_install(po)
add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(autotests)
    add_subdirectory(tests)
    add_subdirectory(examples)
endif()
if (KF6DocTools_FOUND)
    add_subdirectory(docs)
endif()

include(ECMFeatureSummary)
ecm_feature_summary(WHAT ALL   FATAL_ON_MISSING_REQUIRED_PACKAGES)

kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
