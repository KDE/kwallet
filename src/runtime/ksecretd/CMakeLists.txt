configure_file(config-ksecretd.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-ksecretd.h )

include(CheckSymbolExists)

check_symbol_exists(explicit_bzero "string.h" KSECRETD_HAVE_EXPLICIT_BZERO)
if (NOT KSECRETD_HAVE_EXPLICIT_BZERO)
# On FreeBSD it's in strings.h
check_symbol_exists(explicit_bzero "strings.h" KSECRETD_HAVE_EXPLICIT_BZERO)
endif(NOT KSECRETD_HAVE_EXPLICIT_BZERO)
check_symbol_exists(RtlSecureZeroMemory "windows.h" KSECRETD_HAVE_RTLSECUREZEROMEMORY)

find_package(Qt6 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Gui)

find_package(KF6ColorScheme ${KF_DEP_VERSION} REQUIRED)
find_package(KF6Config ${KF_DEP_VERSION} REQUIRED)
find_package(KF6CoreAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6DBusAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6Notifications ${KF_DEP_VERSION} REQUIRED)
find_package(KF6WidgetsAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6WindowSystem ${KF_DEP_VERSION} REQUIRED)
find_package(KF6Crash ${KF_DEP_VERSION} REQUIRED)

########### find needed packages ######

find_package(Qca-qt6 REQUIRED 2.3.1)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
########### build tests #########
add_subdirectory(autotests)

########### ksecretd ###############

add_executable(ksecretd)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../kwalletbackend)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../kwalletbackend)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../api/KWallet)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../api/KWallet)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)

ecm_setup_version(${KF_VERSION} VARIABLE_PREFIX KSECRETD
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/ksecretd_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF6WalletConfigVersion.cmake"
                        SOVERSION 6)

target_sources(ksecretd PRIVATE
   main.cpp
   kbetterthankdialog.cpp
   ksecretd.cpp
   kwalletwizard.cpp
   ktimeout.cpp
   kwalletsessionstore.cpp
   kwalletfreedesktopservice.cpp
   kwalletfreedesktopsession.cpp
   kwalletfreedesktopcollection.cpp
   kwalletfreedesktopitem.cpp
   kwalletfreedesktopprompt.cpp
   kwalletfreedesktopattributes.cpp

   kwalletportalsecrets.cpp
)
ecm_qt_declare_logging_category(ksecretd
    HEADER ksecretd_debug.h
    IDENTIFIER KSECRETD_LOG
    CATEGORY_NAME kf.wallet.ksecretd
    OLD_CATEGORY_NAMES kf5.kwallet.ksecretd
    DESCRIPTION "ksecretd"
    EXPORT KWALLET
)

ki18n_wrap_ui(ksecretd
   kbetterthankdialogbase.ui
   kwalletwizardpageexplanation.ui
   kwalletwizardpageintro.ui
   kwalletwizardpageoptions.ui
   kwalletwizardpagepassword.ui
)

if (Gpgmepp_FOUND)
    target_sources(ksecretd PRIVATE
        knewwalletdialog.cpp
    )
    ki18n_wrap_ui(ksecretd
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
set(fdo_portal_secret_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.impl.portal.Secret.xml)

set(ksecretd_dbus_SRCS)
qt_add_dbus_adaptor(ksecretd_dbus_SRCS ${kwallet_xml} ksecretd.h KSecretD kwalletadaptor KWalletAdaptor)
qt_add_dbus_adaptor(ksecretd_dbus_SRCS ${fdo_service_xml} kwalletfreedesktopservice.h KWalletFreedesktopService
    kwalletfreedesktopserviceadaptor KWalletFreedesktopServiceAdaptor)
qt_add_dbus_adaptor(ksecretd_dbus_SRCS ${fdo_session_xml} kwalletfreedesktopsession.h KWalletFreedesktopSession
    kwalletfreedesktopsessionadaptor KWalletFreedesktopSessionAdaptor)
qt_add_dbus_adaptor(ksecretd_dbus_SRCS ${fdo_collection_xml} kwalletfreedesktopcollection.h KWalletFreedesktopCollection
    kwalletfreedesktopcollectionadaptor KWalletFreedesktopCollectionAdaptor)
qt_add_dbus_adaptor(ksecretd_dbus_SRCS ${fdo_item_xml} kwalletfreedesktopitem.h KWalletFreedesktopItem
    kwalletfreedesktopitemadaptor KWalletFreedesktopItemAdaptor)
qt_add_dbus_adaptor(ksecretd_dbus_SRCS ${fdo_prompt_xml} kwalletfreedesktopprompt.h KWalletFreedesktopPrompt
    kwalletfreedesktoppromptadaptor KWalletFreedesktopPromptAdaptor)
qt_add_dbus_adaptor(ksecretd_dbus_SRCS ${fdo_portal_secret_xml} kwalletportalsecrets.h KWalletPortalSecrets
    kwalletportalsecretsadaptor KWalletPortalSecretsAdaptor)

target_sources(ksecretd PRIVATE
    ${ksecretd_dbus_SRCS}
)

if (HAIKU)
    target_link_libraries(ksecretd network)
endif ()

target_link_libraries(ksecretd
    KF6WalletBackend
    KF6Wallet
    Qt6::Widgets
    KF6::I18n
    KF6::ColorScheme
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::DBusAddons
    KF6::WidgetsAddons
    KF6::WindowSystem
    KF6::Notifications
    KF6::Crash
    ${Qca_LIBRARY})
if (Gpgmepp_FOUND)
    target_link_libraries(ksecretd Gpgmepp)
    kde_target_enable_exceptions(ksecretd PRIVATE)
endif(Gpgmepp_FOUND)

ecm_mark_nongui_executable(ksecretd)
install(TARGETS ksecretd  ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

if(WIN32)
    configure_file(org.kde.kwalletd5.service.win.in
                   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service)
    configure_file(org.kde.kwalletd6.service.win.in
                   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd6.service)
    install(
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service
            ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd6.service
        DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
    )
else()
    configure_file(org.kde.secretservicecompat.service.in
                   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.secretservicecompat.service)

    install(
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/org.kde.secretservicecompat.service
        DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
    )
endif()

########### install files ###############
install( FILES ksecretd.notifyrc DESTINATION  ${KDE_INSTALL_KNOTIFYRCDIR} )
install( FILES org.kde.ksecretd.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
install( FILES kwallet.portal DESTINATION ${KDE_INSTALL_DATADIR}/xdg-desktop-portal/portals)

