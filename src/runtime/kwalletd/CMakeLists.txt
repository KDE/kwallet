project(kwalletd5)

include(CheckSymbolExists)

check_symbol_exists(explicit_bzero "string.h" KWALLETD_HAVE_EXPLICIT_BZERO)
check_symbol_exists(RtlSecureZeroMemory "windows.h" KWALLETD_HAVE_RTLSECUREZEROMEMORY)

find_package(Qt${QT_MAJOR_VERSION} ${REQUIRED_QT_VERSION} CONFIG REQUIRED Gui)

find_package(KF6Config ${KF_DEP_VERSION} REQUIRED)
find_package(KF6ConfigWidgets ${KF_DEP_VERSION} REQUIRED)
find_package(KF6CoreAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6DBusAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6Notifications ${KF_DEP_VERSION} REQUIRED)
find_package(KF6Service ${KF_DEP_VERSION} REQUIRED)
find_package(KF6WidgetsAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6WindowSystem ${KF_DEP_VERSION} REQUIRED)

########### find needed packages ######
find_package(Gpgmepp 1.7.0) # provided by GpgME

if (Gpgmepp_FOUND)
    message("GPG support enabled")
    add_definitions(-DHAVE_GPGMEPP)
    add_definitions(-DBOOST_NO_EXCEPTIONS)
    include_directories(${GPGME_INCLUDES})
endif(Gpgmepp_FOUND)

find_package(Qca-qt${QT_MAJOR_VERSION} REQUIRED 2.3.1)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
########### build backends #########
add_subdirectory(backend)
add_subdirectory(autotests)

########### kwalletd ###############

add_executable(kwalletd5)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backend)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../api/KWallet)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../api/KWallet)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)

ecm_setup_version(${KF_VERSION} VARIABLE_PREFIX KWALLETD
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kwalletd_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF6WalletConfigVersion.cmake"
                        SOVERSION 5)

target_sources(kwalletd5 PRIVATE
   main.cpp
   kbetterthankdialog.cpp
   kwalletd.cpp
   kwalletwizard.cpp
   ktimeout.cpp
   kwalletsessionstore.cpp
   kwalletfreedesktopservice.cpp
   kwalletfreedesktopsession.cpp
   kwalletfreedesktopcollection.cpp
   kwalletfreedesktopitem.cpp
   kwalletfreedesktopprompt.cpp
   kwalletfreedesktopattributes.cpp
)
ecm_qt_declare_logging_category(kwalletd5
    HEADER kwalletd_debug.h
    IDENTIFIER KWALLETD_LOG
    CATEGORY_NAME kf.wallet.kwalletd
    OLD_CATEGORY_NAMES kf5.kwallet.kwalletd
    DESCRIPTION "kwalletd"
    EXPORT KWALLET
)

ki18n_wrap_ui(kwalletd5
   kbetterthankdialogbase.ui
   kwalletwizardpageexplanation.ui
   kwalletwizardpageintro.ui
   kwalletwizardpageoptions.ui
   kwalletwizardpagepassword.ui
)

if (Gpgmepp_FOUND)
    target_sources(kwalletd5 PRIVATE
        knewwalletdialog.cpp
    )
    ki18n_wrap_ui(kwalletd5
        kwalletwizardpagepasswordgpg.ui
        kwalletwizardpagegpgkey.ui
        knewwalletdialogintro.ui
        knewwalletdialoggpg.ui
    )
endif(Gpgmepp_FOUND)

if (NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND
    EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.72.0)
    set(kwallet_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.kde.KWallet.xml)
else()
    # copy of org.kde.KWallet.xml, but with all deprecated API removed
    set(kwallet_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.kde.KWallet.nodeprecated.xml)
endif()
set(fdo_service_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.Secrets.Service.xml)
set(fdo_session_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.Secrets.Session.xml)
set(fdo_collection_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.Secrets.Collection.xml)
set(fdo_item_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.Secrets.Item.xml)
set(fdo_prompt_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.Secrets.Prompt.xml)

set(kwalletd_dbus_SRCS)
qt_add_dbus_adaptor(kwalletd_dbus_SRCS ${kwallet_xml} kwalletd.h KWalletD kwalletadaptor KWalletAdaptor)
qt_add_dbus_adaptor(kwalletd_dbus_SRCS ${fdo_service_xml} kwalletfreedesktopservice.h KWalletFreedesktopService
    kwalletfreedesktopserviceadaptor KWalletFreedesktopServiceAdaptor)
qt_add_dbus_adaptor(kwalletd_dbus_SRCS ${fdo_session_xml} kwalletfreedesktopsession.h KWalletFreedesktopSession
    kwalletfreedesktopsessionadaptor KWalletFreedesktopSessionAdaptor)
qt_add_dbus_adaptor(kwalletd_dbus_SRCS ${fdo_collection_xml} kwalletfreedesktopcollection.h KWalletFreedesktopCollection
    kwalletfreedesktopcollectionadaptor KWalletFreedesktopCollectionAdaptor)
qt_add_dbus_adaptor(kwalletd_dbus_SRCS ${fdo_item_xml} kwalletfreedesktopitem.h KWalletFreedesktopItem
    kwalletfreedesktopitemadaptor KWalletFreedesktopItemAdaptor)
qt_add_dbus_adaptor(kwalletd_dbus_SRCS ${fdo_prompt_xml} kwalletfreedesktopprompt.h KWalletFreedesktopPrompt
    kwalletfreedesktoppromptadaptor KWalletFreedesktopPromptAdaptor)
target_sources(kwalletd5 PRIVATE
    ${kwalletd_dbus_SRCS}
)

if(WIN32)
    configure_file(org.kde.kwalletd5.service.win.in
                   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service)
else()
    configure_file(org.kde.kwalletd5.service.in
                   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service)
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service
        DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})

target_link_libraries(kwalletd5
    kwalletbackend5
    KF6Wallet
    Qt${QT_MAJOR_VERSION}::Widgets
    KF6::I18n
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::ConfigWidgets
    KF6::Service
    KF6::DBusAddons
    KF6::WidgetsAddons
    KF6::WindowSystem
    KF6::Notifications
    ${Qca_LIBRARY})
if (Gpgmepp_FOUND)
    target_link_libraries(kwalletd5 Gpgmepp)
    kde_target_enable_exceptions(kwalletd5 PRIVATE)
endif(Gpgmepp_FOUND)

ecm_mark_nongui_executable(kwalletd5)
install(TARGETS kwalletd5  ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############
install( FILES kwalletd5.notifyrc DESTINATION  ${KDE_INSTALL_KNOTIFYRCDIR} )
install( FILES org.kde.kwalletd5.desktop DESTINATION ${KDE_INSTALL_APPDIR} )

