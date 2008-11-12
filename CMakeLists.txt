project(kwalletd)

add_subdirectory(backend)

########### kwalletd ###############

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backend)

set(kwalletd_KDEINIT_SRCS
   main.cpp
   kbetterthankdialog.cpp
   kwalletd.cpp
   kwalletopenloop.cpp
   kwalletwizard.cpp
   ktimeout.cpp
   kwalletsynctimer.cpp
)


kde4_add_ui_files(kwalletd_KDEINIT_SRCS
   kbetterthankdialogbase.ui
   kwalletwizardpageexplanation.ui
   kwalletwizardpageintro.ui
   kwalletwizardpageoptions.ui
   kwalletwizardpagepassword.ui
)

set(kwallet_xml ${DBUS_INTERFACES_INSTALL_DIR}/org.kde.KWallet.xml)
qt4_add_dbus_adaptor( kwalletd_KDEINIT_SRCS ${kwallet_xml} kwalletd.h KWalletD )

kde4_add_kdeinit_executable( kwalletd ${kwalletd_KDEINIT_SRCS} )

# uses kio for KDirWatch
target_link_libraries(kdeinit_kwalletd  ${KDE4_KIO_LIBS} kwalletbackend )
install(TARGETS kdeinit_kwalletd  ${INSTALL_TARGETS_DEFAULT_ARGS})

target_link_libraries(kwalletd kdeinit_kwalletd)
install(TARGETS kwalletd  ${INSTALL_TARGETS_DEFAULT_ARGS} )

# Note that org.kde.KWallet.xml is installed by kdelibs/kdeui
# TODO: generate the adaptor from it

########### install files ###############

install( FILES kwalletd.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

add_subdirectory(tests)
