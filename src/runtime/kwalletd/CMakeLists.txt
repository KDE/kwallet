
find_package(KF6WidgetsAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6DBusAddons ${KF_DEP_VERSION} REQUIRED)
find_package(KF6Crash ${KF_DEP_VERSION} REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
include_directories(${LIBSECRET_INCLUDE_DIRS})
link_directories(${LIBSECRET_LIBRARY_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../kwalletbackend)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../kwalletbackend)


if(NOT WIN32)
    configure_file(org.kde.kwalletd5.service.in
                   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service)
    configure_file(org.kde.kwalletd6.service.in
                   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd6.service)
endif()

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd5.service
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwalletd6.service
    DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
)

set(kwalletd6_SRCS
    main.cpp
    kwalletd.cpp
    secretserviceclient.cpp
)

add_executable(kwalletd6)

if (NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND
    EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.72.0)
    set(kwallet_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.kde.KWallet.xml)
else()
    # copy of org.kde.KWallet.xml, but with all deprecated API removed
    set(kwallet_xml ${CMAKE_SOURCE_DIR}/src/api/KWallet/org.kde.nodeprecated.KWallet.xml)
endif()

qt_add_dbus_adaptor(kwalletd6_SRCS
    ${kwallet_xml}
    kwalletd.h KWalletD
)

target_sources(kwalletd6 PRIVATE
    ${kwalletd6_SRCS}
)

ecm_qt_declare_logging_category(kwalletd6
    HEADER kwalletd_debug.h
    IDENTIFIER KWALLETD_LOG
    CATEGORY_NAME kf.wallet.kwalletd
    DESCRIPTION "kwalletd"
    EXPORT KWALLET
)

target_link_libraries(kwalletd6
    KF6WalletBackend
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::WidgetsAddons
    KF6::Crash
    KF6::I18n
    KF6::DBusAddons
    KF6::Wallet
    Qt6::Widgets
    Qt::DBus
    ${LIBSECRET_LIBRARIES}
)

install( TARGETS kwalletd6 ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )
