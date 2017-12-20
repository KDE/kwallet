project(kwalletd5)

find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Gui)

find_package(KF5Config ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5ConfigWidgets ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5CoreAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5DBusAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5IconThemes ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Notifications ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5Service ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5WidgetsAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5WindowSystem ${KF5_DEP_VERSION} REQUIRED)

########### find needed packages ######
find_package(Gpgmepp 1.7.0) # provided by GpgME

if (Gpgmepp_FOUND)
    message("GPG support enabled")
    add_definitions(-DHAVE_GPGMEPP)
    add_definitions(-DBOOST_NO_EXCEPTIONS)
    include_directories(${GPGME_INCLUDES})
endif(Gpgmepp_FOUND)


include_directories(${CMAKE_CURRENT_BINARY_DIR})
########### build backends #########
add_subdirectory(backend)

########### kwalletd ###############

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backend)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../api/KWallet)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../api/KWallet)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)

# migration agent need exceptions enabled
kde_enable_exceptions()

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
   migrationagent.cpp
   migrationwizard.cpp
)
ecm_qt_declare_logging_category(kwalletd_SRCS HEADER kwalletd_debug.h IDENTIFIER KWALLETD_LOG CATEGORY_NAME kf5.kwallet.kwalletd)

ki18n_wrap_ui(kwalletd_SRCS
   kbetterthankdialogbase.ui
   kwalletwizardpageexplanation.ui
   kwalletwizardpageintro.ui
   kwalletwizardpageoptions.ui
   kwalletwizardpagepassword.ui
   migrationwizard1.ui
   migrationwizard2.ui
)

if (Gpgmepp_FOUND)
    set(kwalletd_SRCS
        ${kwalletd_SRCS}
        knewwalletdialog.cpp
    )
    ki18n_wrap_ui(kwalletd_SRCS
        kwalletwizardpagepasswordgpg.ui
        kwalletwizardpagegpgkey.ui
        knewwalletdialogintro.ui
        knewwalletdialoggpg.ui
    )
endif(Gpgmepp_FOUND)

set(kwallet_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.kde.KWallet.xml)

qt5_add_dbus_adaptor( kwalletd_SRCS ${kwallet_xml} kwalletd.h KWalletD )

# this is needed for the migration agent
qt5_add_dbus_interface(kwalletd_SRCS
    ../../api/KWallet/org.kde.KWallet.xml kwallet4_interface
)

if(WIN32)
    configure_file(org.kde.kwalletd5.service.win.in
                   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service)
else()
    configure_file(org.kde.kwalletd5.service.in
                   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service)
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service
        DESTINATION ${DBUS_SERVICES_INSTALL_DIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.kwalletd.service
        DESTINATION ${DBUS_SERVICES_INSTALL_DIR})

add_executable( kwalletd5 ${kwalletd_SRCS} )

target_link_libraries(kwalletd5
    kwalletbackend5
    KF5Wallet
    Qt5::Widgets
    KF5::I18n
    KF5::CoreAddons
    KF5::ConfigCore
    KF5::ConfigWidgets
    KF5::IconThemes
    KF5::Service
    KF5::DBusAddons
    KF5::WidgetsAddons
    KF5::WindowSystem
    KF5::Notifications)
if (Gpgmepp_FOUND)
    target_link_libraries(kwalletd5 Gpgmepp)
    kde_target_enable_exceptions(kwalletd5 PRIVATE)
endif(Gpgmepp_FOUND)

install(TARGETS kwalletd5  ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############

install( FILES kwalletd5.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES kwalletd.notifyrc DESTINATION  ${KNOTIFYRC_INSTALL_DIR} )

