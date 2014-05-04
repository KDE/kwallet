include(CheckIncludeFiles)
include(GenerateExportHeader)

########### Configure checks for kwalletbackend ###############

check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(sys/bitypes.h HAVE_SYS_BITYPES_H)
if (QGPGME_FOUND)
    add_definitions(-DHAVE_QGPGME)
endif(QGPGME_FOUND)

configure_file (config-kwalletbackend.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kwalletbackend.h )

########### kwalletbackend ###############
find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Widgets DBus)
find_package(KF5CoreAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5I18n ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Notifications ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5WidgetsAddons ${KF5_DEP_VERSION} REQUIRED)

find_package(LibGcrypt 1.5.0 REQUIRED)
set_package_properties(LibGcrypt PROPERTIES
                       TYPE REQUIRED
                       PURPOSE "kwalletd needs libgcrypt to perform PBKDF2-SHA512 hashing"
                      )

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../api/KWallet)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../../api/KWallet)
include_directories(${LIBGCRYPT_INCLUDE_DIRS})

#remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_KEYWORDS)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)

set(kwalletbackend_LIB_SRCS
   blockcipher.cc
   blowfish.cc
   cbc.cc
   sha1.cc
   kwalletentry.cc
   kwalletbackend.cc
   backendpersisthandler.cpp
)

add_library(kwalletbackend5 SHARED ${kwalletbackend_LIB_SRCS})
generate_export_header(kwalletbackend5)

ecm_setup_version(${KF5_VERSION} VARIABLE_PREFIX KWALLETBACKEND SOVERSION 5)

target_link_libraries(kwalletbackend5 Qt5::Widgets KF5::WidgetsAddons KF5::CoreAddons KF5::Notifications KF5::I18n ${LIBGCRYPT_LIBRARIES})
if(QGPGME_FOUND)
target_link_libraries(kwalletbackend5 ${QGPGME_LIBRARIES} )
endif(QGPGME_FOUND)

# link with advapi32 on windows
if(WIN32 AND NOT WINCE)
   target_link_libraries(kwalletbackend5 advapi32)
endif(WIN32 AND NOT WINCE)

set_target_properties(kwalletbackend5 PROPERTIES VERSION ${KWALLETBACKEND_VERSION_STRING} SOVERSION ${KWALLETBACKEND_SOVERSION} )
install(TARGETS kwalletbackend5 ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

add_subdirectory(tests)
