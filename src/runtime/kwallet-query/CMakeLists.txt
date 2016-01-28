PROJECT(kwallet-dmenu)

set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

FIND_PACKAGE(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Core Widgets)

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(FeatureSummary)


ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(doc)

#if("${CMAKE_BINARY_DIR}" STREQUAL "{CMAKE_CURRENT_BINARY_DIR}")
    feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
#endif()

