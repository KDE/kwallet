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
find_package(KF5 CONFIG REQUIRED COMPONENTS WidgetsAddons CoreAddons Notifications I18n)

include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KDE4_INCLUDES} ${KF5_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../api/KWallet)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../../api/KWallet)

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

add_library(kwalletbackend SHARED ${kwalletbackend_LIB_SRCS})
generate_export_header(kwalletbackend)

target_link_libraries(kwalletbackend Qt5::Widgets KF5::WidgetsAddons KF5::CoreAddons KF5::Notifications KF5::I18n)
if(QGPGME_FOUND)
target_link_libraries(kwalletbackend ${QGPGME_LIBRARIES} )
endif(QGPGME_FOUND)

# link with advapi32 on windows
if(WIN32 AND NOT WINCE)
   target_link_libraries(kwalletbackend advapi32)
endif(WIN32 AND NOT WINCE)

set_target_properties(kwalletbackend PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION} )
install(TARGETS kwalletbackend ${INSTALL_TARGETS_DEFAULT_ARGS})

add_subdirectory(tests)
