project(kwalletd5)

find_package(KF5Config ${KF5_VERSION} REQUIRED)
find_package(KF5CoreAddons ${KF5_VERSION} REQUIRED)
find_package(KF5DBusAddons ${KF5_VERSION} REQUIRED)
find_package(KF5I18n ${KF5_VERSION} REQUIRED)
find_package(KF5IconThemes ${KF5_VERSION} REQUIRED)
find_package(KF5Notifications ${KF5_VERSION} REQUIRED)
find_package(KF5Service ${KF5_VERSION} REQUIRED)
find_package(KF5WidgetsAddons ${KF5_VERSION} REQUIRED)
find_package(KF5WindowSystem ${KF5_VERSION} REQUIRED)

########### find needed packages ######
find_package(Gpgme)  # Called by FindQGpgme, but since we call some gpgme
                     # functions ourselves we need to link against it directly.
find_package(QGpgme) # provided by kdepimlibs

if (GPGME_FOUND AND QGPGME_FOUND)
    add_definitions(-DHAVE_QGPGME)
    include_directories(${GPGME_INCLUDES} ${QGPGME_INCLUDE_DIR})
endif(GPGME_FOUND AND QGPGME_FOUND)


########### build backends #########
add_subdirectory(backend)

########### kwalletd ###############

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backend)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../api/KWallet)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../api/KWallet)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=285)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

ecm_setup_version(${KF5_VERSION} VARIABLE_PREFIX KWALLETD
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kwalletd_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5WalletConfigVersion.cmake"
                        SOVERSION 5)

set(kwalletd_SRCS
   main.cpp
   kbetterthankdialog.cpp
   kwalletd.cpp
   kwalletwizard.cpp
   ktimeout.cpp
   kwalletsessionstore.cpp
)

qt5_wrap_ui(kwalletd_SRCS
   kbetterthankdialogbase.ui
   kwalletwizardpageexplanation.ui
   kwalletwizardpageintro.ui
   kwalletwizardpageoptions.ui
   kwalletwizardpagepassword.ui
)

if (GPGME_FOUND AND QGPGME_FOUND)
    set(kwalletd_SRCS
        ${kwalletd_SRCS}
        knewwalletdialog.cpp
    )
    qt5_wrap_ui(kwalletd_SRCS
        kwalletwizardpagepasswordgpg.ui
        kwalletwizardpagegpgkey.ui
        knewwalletdialogintro.ui
        knewwalletdialoggpg.ui
    )
endif(GPGME_FOUND AND QGPGME_FOUND)

find_file(kwallet_xml org.kde.KWallet.xml HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../../api/KWallet)

qt5_add_dbus_adaptor( kwalletd_SRCS ${kwallet_xml} kwalletd.h KWalletD )

configure_file(org.kde.kwalletd5.service.in
               ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service
        DESTINATION ${DBUS_SERVICES_INSTALL_DIR})

add_executable( kwalletd5 ${kwalletd_SRCS} )

target_link_libraries(kwalletd5
    kwalletbackend5
    KF5Wallet
    Qt5::Widgets
    KF5::I18n
    KF5::CoreAddons
    KF5::ConfigCore
    KF5::IconThemes
    KF5::Service
    KF5::DBusAddons
    KF5::WidgetsAddons
    KF5::WindowSystem
    KF5::Notifications)
if (GPGME_FOUND AND QGPGME_FOUND)
    target_link_libraries(kwalletd ${QGPGME_LIBRARIES} )
    kde_target_enable_exceptions(kwalletd5 PRIVATE)
endif(GPGME_FOUND AND QGPGME_FOUND)

install(TARGETS kwalletd5  ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############

install( FILES kwalletd5.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
# This is DATA_INSTALL_DIR, rather than KF5_DATA_INSTALL_DIR, because of
# how KNotifyConfig searches for this file.
install( FILES kwalletd.notifyrc DESTINATION  ${KNOTIFYRC_INSTALL_DIR} )

