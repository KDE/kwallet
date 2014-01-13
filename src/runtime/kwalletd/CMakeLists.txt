project(kwalletd)

find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Widgets DBus)
find_package(KF5Config CONFIG REQUIRED)
find_package(KF5Wallet CONFIG REQUIRED)
find_package(KF5I18n CONFIG REQUIRED)
find_package(KF5Config CONFIG REQUIRED)
find_package(KF5CoreAddons CONFIG REQUIRED)
find_package(KF5IconThemes CONFIG REQUIRED)
find_package(KF5Service CONFIG REQUIRED)
find_package(KF5DBusAddons CONFIG REQUIRED)
find_package(KF5WidgetsAddons CONFIG REQUIRED)
find_package(KF5WindowSystem CONFIG REQUIRED)
find_package(KF5Notifications CONFIG REQUIRED)
include_directories($<TARGET_PROPERTY:KF5::Wallet,INTERFACE_INCLUDE_DIRECTORIES>)

########### find needed packages ######
find_package(Gpgme)  # Called by FindQGpgme, but since we call some gpgme
                     # functions ourselves we need to link against it directly.
find_package(QGpgme) # provided by kdepimlibs

if (GPGME_FOUND AND QGPGME_FOUND)
    add_definitions(-DHAVE_QGPGME)
    include_directories(${GPGME_INCLUDES} ${QGPGME_INCLUDE_DIR})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE_ENABLE_EXCEPTIONS}")
endif(GPGME_FOUND AND QGPGME_FOUND)


########### build backends #########
add_subdirectory(backend)

########### kwalletd ###############

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backend)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../api/KWallet)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=285)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

ecm_setup_version(${KF5_VERSION} VARIABLE_PREFIX KWALLETD
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kwalletd_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5WalletConfigVersion.cmake")

set(kwalletd_KDEINIT_SRCS
   main.cpp
   kbetterthankdialog.cpp
   kwalletd.cpp
   kwalletwizard.cpp
   ktimeout.cpp
   kwalletsessionstore.cpp
)

qt5_wrap_ui(kwalletd_KDEINIT_SRCS
   kbetterthankdialogbase.ui
   kwalletwizardpageexplanation.ui
   kwalletwizardpageintro.ui
   kwalletwizardpageoptions.ui
   kwalletwizardpagepassword.ui
)

if (GPGME_FOUND AND QGPGME_FOUND)
    set(kwalletd_KDEINIT_SRCS
        ${kwalletd_KDEINIT_SRCS}
        knewwalletdialog.cpp
    )
    qt5_wrap_ui(kwalletd_KDEINIT_SRCS
        kwalletwizardpagepasswordgpg.ui
        kwalletwizardpagegpgkey.ui
        knewwalletdialogintro.ui
        knewwalletdialoggpg.ui
    )
endif(GPGME_FOUND AND QGPGME_FOUND)

#message("DBUS_INTERFACES_INSTALL_DIR = " ${CMAKE_INSTALL_PREFIX}/${DBUS_INTERFACES_INSTALL_DIR}) 
find_file(kwallet_xml org.kde.KWallet.xml HINTS ${CMAKE_INSTALL_PREFIX}/${DBUS_INTERFACES_INSTALL_DIR})

qt5_add_dbus_adaptor( kwalletd_KDEINIT_SRCS ${kwallet_xml} kwalletd.h KWalletD )

kf5_add_kdeinit_executable( kwalletd ${kwalletd_KDEINIT_SRCS} )

target_link_libraries(kdeinit_kwalletd  kwalletbackend
    Qt5::Widgets
    KF5::Wallet
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
    target_link_libraries(kdeinit_kwalletd ${QGPGME_LIBRARIES} )
endif(GPGME_FOUND AND QGPGME_FOUND)

install(TARGETS kdeinit_kwalletd  ${INSTALL_TARGETS_DEFAULT_ARGS})

target_link_libraries(kwalletd kdeinit_kwalletd)
install(TARGETS kwalletd  ${INSTALL_TARGETS_DEFAULT_ARGS} )

########### install files ###############

install( FILES kwalletd.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES kwalletd.notifyrc DESTINATION  ${DATA_INSTALL_DIR}/kwalletd )


