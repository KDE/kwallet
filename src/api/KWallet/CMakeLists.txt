
include(GenerateExportHeader)
include(ECMGenerateHeaders)

ecm_setup_version(${KF_VERSION} VARIABLE_PREFIX KWALLET
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kwallet_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5WalletConfigVersion.cmake"
                        SOVERSION 5)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kwallet_version.h
        DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KWallet COMPONENT Devel
)

set(HAVE_KSECRETSSERVICE 0)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config-kwallet.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config-kwallet.h
)

if(APPLE)
    option(MAC_USE_OSXKEYCHAIN "On OS X, use the keychain as backend for kwallet, instead of kwalletd.")
else()
    set(MAC_USE_OSXKEYCHAIN FALSE)
endif()

add_library(KF5Wallet)
add_library(KF5::Wallet ALIAS KF5Wallet)

if (MAC_USE_OSXKEYCHAIN)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(SECURITY_LIBRARY Security)
    target_sources(KF5Wallet PRIVATE
        kwallet_mac.cpp
    )
else()
    set(kwallet_dbus_SRCS)
    if (NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND
        EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.72.0)
        set(kwallet_xml org.kde.KWallet.xml)
    else()
        # copy of org.kde.KWallet.xml, but with all deprecated API removed
        set(kwallet_xml org.kde.KWallet.nodeprecated.xml)
    endif()
    qt_add_dbus_interface(kwallet_dbus_SRCS ${kwallet_xml} kwallet_interface)
    target_sources(KF5Wallet PRIVATE
        kwallet.cpp
        ${kwallet_dbus_SRCS}
    )
endif()

ecm_qt_declare_logging_category(KF5Wallet
    HEADER kwallet_api_debug.h
    IDENTIFIER KWALLET_API_LOG
    CATEGORY_NAME kf.wallet.api
    OLD_CATEGORY_NAMES kf5.kwallet.api
    DESCRIPTION "kwallet api"
    EXPORT KWALLET
)

ecm_generate_export_header(KF5Wallet
    BASE_NAME KWallet
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 5.72
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)

target_include_directories(KF5Wallet INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KWallet>")

target_link_libraries(KF5Wallet
    PUBLIC
        Qt${QT_MAJOR_VERSION}::Gui
    PRIVATE
        Qt${QT_MAJOR_VERSION}::DBus
        Qt${QT_MAJOR_VERSION}::Widgets
        KF5::WindowSystem # KWindowSystem::allowExternalProcessWindowActivation
        KF5::ConfigCore   # used to store the wallet to be used
)

if(MAC_USE_OSXKEYCHAIN)
   target_link_libraries(KF5Wallet PRIVATE ${CARBON_LIBRARY}
                                           ${SECURITY_LIBRARY}
                                           ${COREFOUNDATION_LIBRARY})
elseif(APPLE)
    target_link_libraries(KF5Wallet PRIVATE ${CARBON_LIBRARY})
else()
    target_link_libraries(KF5Wallet PRIVATE Qt${QT_MAJOR_VERSION}::DBus)
    install(FILES ${kwallet_xml} DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR} RENAME kf5_org.kde.KWallet.xml)
endif()

set_target_properties(KF5Wallet PROPERTIES VERSION   ${KWALLET_VERSION}
                                         SOVERSION ${KWALLET_SOVERSION}
                                         EXPORT_NAME Wallet
)

ecm_generate_headers(KWallet_HEADERS
  HEADER_NAMES
  KWallet

  REQUIRED_HEADERS KWallet_HEADERS
)

install(TARGETS KF5Wallet EXPORT KF5WalletTargets ${KF_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/kwallet_export.h
  ${KWallet_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KWallet COMPONENT Devel
)

if(BUILD_QCH)
    ecm_add_qch(
        KF5Wallet_QCH
        NAME KWallet
        BASE_NAME KF5Wallet
        VERSION ${KF_VERSION}
        ORG_DOMAIN org.kde
        SOURCES # using only public headers, to cover only public API
            ${KWallet_HEADERS}
        LINK_QCHS
            Qt5Gui_QCH
        INCLUDE_DIRS
            ${CMAKE_CURRENT_BINARY_DIR}
        BLANK_MACROS
            KWALLET_EXPORT
            KWALLET_DEPRECATED
            KWALLET_DEPRECATED_EXPORT
            "KWALLET_DEPRECATED_VERSION(x, y, t)"
        TAGFILE_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        QCH_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        COMPONENT Devel
    )
endif()


# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KF5Wallet")

if (BUILD_QCH)
    ecm_install_qch_export(
        TARGETS KF5Wallet_QCH
        FILE KF5WalletQchTargets.cmake
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel
    )
    set(PACKAGE_INCLUDE_QCHTARGETS "include(\"\${CMAKE_CURRENT_LIST_DIR}/KF5WalletQchTargets.cmake\")")
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KF5WalletConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5WalletConfig.cmake"
  PATH_VARS KDE_INSTALL_DBUSINTERFACEDIR
  PATH_VARS KDE_INSTALL_BINDIR
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/KF5WalletConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5WalletConfigVersion.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)

install(EXPORT KF5WalletTargets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KF5WalletTargets.cmake NAMESPACE KF5:: )


include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KWallet LIB_NAME KF5Wallet DEPS "widgets" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF}/KWallet)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
