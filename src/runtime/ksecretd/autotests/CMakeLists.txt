set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

find_package(Qt6Test REQUIRED)
find_package(KF6Config ${KF_DEP_VERSION} REQUIRED)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

include(ECMAddTests)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../api/KWallet)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../../api/KWallet)

add_definitions(-DFDO_ENABLE_DUMMY_MESSAGE_CONNECTION)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)

set(fdo_service_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.Secrets.Service.xml)
set(fdo_session_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.Secrets.Session.xml)
set(fdo_collection_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.Secrets.Collection.xml)
set(fdo_item_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.Secrets.Item.xml)
set(fdo_prompt_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.freedesktop.Secrets.Prompt.xml)

set(TEST_SRC
    fdo_secrets_test.cpp
    ../kwalletfreedesktopservice.cpp
    ../kwalletfreedesktopitem.cpp
    ../kwalletfreedesktopcollection.cpp
    ../kwalletfreedesktopsession.cpp
    ../kwalletfreedesktopprompt.cpp
    ../kwalletfreedesktopattributes.cpp
)

qt_add_dbus_adaptor( TEST_SRC ${fdo_service_xml} ../kwalletfreedesktopservice.h KWalletFreedesktopService
    kwalletfreedesktopserviceadaptor KWalletFreedesktopServiceAdaptor)
qt_add_dbus_adaptor( TEST_SRC ${fdo_session_xml} ../kwalletfreedesktopsession.h KWalletFreedesktopSession
    kwalletfreedesktopsessionadaptor KWalletFreedesktopSessionAdaptor)
qt_add_dbus_adaptor( TEST_SRC ${fdo_collection_xml} ../kwalletfreedesktopcollection.h KWalletFreedesktopCollection
    kwalletfreedesktopcollectionadaptor KWalletFreedesktopCollectionAdaptor)
qt_add_dbus_adaptor( TEST_SRC ${fdo_item_xml} ../kwalletfreedesktopitem.h KWalletFreedesktopItem
    kwalletfreedesktopitemadaptor KWalletFreedesktopItemAdaptor)
qt_add_dbus_adaptor( TEST_SRC ${fdo_prompt_xml} ../kwalletfreedesktopprompt.h KWalletFreedesktopPrompt
    kwalletfreedesktoppromptadaptor KWalletFreedesktopPromptAdaptor)

ecm_add_test(
    ${TEST_SRC}
    ../kwalletfreedesktopservice.h
    ../kwalletfreedesktopcollection.h
    ../kwalletfreedesktopitem.h
    ../kwalletfreedesktopsession.h
    ../kwalletfreedesktopprompt.h
    ../ksecretd.h
    ../ktimeout.h
    kwalletfreedesktopserviceadaptor.cpp
    kwalletfreedesktopcollectionadaptor.cpp
    kwalletfreedesktopitemadaptor.cpp
    kwalletfreedesktopsessionadaptor.cpp
    kwalletfreedesktoppromptadaptor.cpp
    TEST_NAME fdo_secrets_test
    LINK_LIBRARIES
        KF6Wallet
        KF6WalletBackend
        Qt6::Widgets
        Qt6::Test
        KF6::DBusAddons
        KF6::ConfigCore
        KF6::ConfigGui
        Botan::Botan
)

kconfig_target_kcfg_file(fdo_secrets_test FILE ${CMAKE_SOURCE_DIR}/src/kwalletsettings.kcfg CLASS_NAME KWalletSettings MUTATORS)

kde_target_enable_exceptions(fdo_secrets_test PRIVATE)
