cmake_minimum_required(VERSION 3.16)

set(KF_VERSION "6.23.0") # handled by release scripts
set(KF_DEP_VERSION "6.22.0") # handled by release scripts
project(KWallet VERSION ${KF_VERSION})
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include(FeatureSummary)
find_package(ECM 6.22.0  NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://invent.kde.org/frameworks/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)

include(KDEGitCommitHooks)
include(ECMDeprecationSettings)

set(REQUIRED_QT_VERSION 6.8.0)
find_package(Qt6 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Core DBus Gui Widgets)

include(ECMGenerateExportHeader)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(ECMMarkNonGuiExecutable)
include(ECMGenerateQDoc)

option(BUILD_KSECRETD "Build the ksecretd daemon" ON)
if(NOT WIN32 AND NOT APPLE)
    option(BUILD_KWALLETD "Build the kwallet daemon" ON)
else()
    set(BUILD_KWALLETD OFF)
endif()
option(BUILD_KWALLET_QUERY "Build kwallet-query tool" ON)

if(NOT WIN32 AND NOT APPLE AND NOT HAIKU)
    option(WITH_X11 "Build with support for KWindowSystem::isPlatformX11()" ON)
endif()

# ksecretd contains functions that are marked as deprecated.
# These are part of the public DBus API and are used by older applications
# Therefore we must not exclude those by default
set(EXCLUDE_DEPRECATED_BEFORE_AND_AT 0 CACHE STRING "Control the range of deprecated API excluded from the build [default=0].")

find_package(KF6Config ${KF_DEP_VERSION} REQUIRED)

if(BUILD_KSECRETD OR BUILD_KWALLETD)
    find_package(KF6 ${KF_DEP_VERSION} REQUIRED COMPONENTS CoreAddons I18n WindowSystem)
endif()

ecm_set_disabled_deprecation_versions(
    QT 6.11.0
    KF 6.21.0
)

if (WIN32)
    set(EXPORT_LEGACY_API 1)
else()
    set(EXPORT_LEGACY_API 0)
endif()

add_definitions(-DTRANSLATION_DOMAIN=\"ksecretd6\")
if(BUILD_KSECRETD OR BUILD_KWALLETD OR BUILD_KWALLET_QUERY)
    ki18n_install(po)
endif()
add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(autotests)
    add_subdirectory(tests)
    add_subdirectory(examples)
endif()
if(BUILD_KWALLET_QUERY)
    find_package(KF6DocTools ${KF_DEP_VERSION})
    if(KF6DocTools_FOUND)
        add_subdirectory(docs)
    endif()
endif()

include(ECMFeatureSummary)
ecm_feature_summary(WHAT ALL   FATAL_ON_MISSING_REQUIRED_PACKAGES)

kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
