project(kwalletd)

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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE_ENABLE_EXCEPTIONS}")
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
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5WalletConfigVersion.cmake")

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

include(ECMDBusAddActivationService)
ecm_dbus_add_activation_service(org.kde.kwalletd.service.in)

add_executable( kwalletd ${kwalletd_SRCS} )

target_link_libraries(kwalletd
    kwalletbackend
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
endif(GPGME_FOUND AND QGPGME_FOUND)

install(TARGETS kwalletd  ${INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############

install( FILES kwalletd.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES kwalletd.notifyrc DESTINATION  ${DATA_INSTALL_DIR}/kwalletd )

