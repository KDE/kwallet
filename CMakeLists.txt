cmake_minimum_required(VERSION 3.16)

set(KF_VERSION "5.113.0") # handled by release scripts
set(KF_DEP_VERSION "5.112.0") # handled by release scripts
project(KWallet VERSION ${KF_VERSION})
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include(FeatureSummary)
find_package(ECM 5.112.0  NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://invent.kde.org/frameworks/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)

include(KDEGitCommitHooks)
include(ECMDeprecationSettings)

set(REQUIRED_QT_VERSION 5.15.2)
find_package(Qt${QT_MAJOR_VERSION} ${REQUIRED_QT_VERSION} CONFIG REQUIRED Widgets DBus)

include(ECMAddQch)
include(ECMGenerateExportHeader)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(ECMMarkNonGuiExecutable)

option(BUILD_KWALLETD "Build the kwallet daemon" ON)
option(BUILD_KWALLET_QUERY "Build kwallet-query tool" ON)
option(BUILD_QCH "Build API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)" OFF)
add_feature_info(QCH ${BUILD_QCH} "API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)")
set(EXCLUDE_DEPRECATED_BEFORE_AND_AT 0 CACHE STRING "Control the range of deprecated API excluded from the build [default=0].")

find_package(KF5CoreAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF5Config ${KF_DEP_VERSION} REQUIRED)
find_package(KF5WindowSystem ${KF_DEP_VERSION} REQUIRED)
find_package(KF5I18n ${KF_DEP_VERSION} REQUIRED)
find_package(KF5DocTools ${KF_DEP_VERSION})

ecm_set_disabled_deprecation_versions(
    QT 5.15.2
    KF 5.95
    KSERVICE 5.81 # KToolInvocation::startServiceByDesktopName
)

add_definitions(-DTRANSLATION_DOMAIN=\"kwalletd5\")
ki18n_install(po)
add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(autotests)
    add_subdirectory(tests)
    add_subdirectory(examples)
endif()
if (KF5DocTools_FOUND)
    add_subdirectory(docs)
endif()

feature_summary(WHAT ALL   FATAL_ON_MISSING_REQUIRED_PACKAGES)

kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
