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
   kwalletsessionstore.cpp
)


kde4_add_ui_files(kwalletd_KDEINIT_SRCS
   kbetterthankdialogbase.ui
   kwalletwizardpageexplanation.ui
   kwalletwizardpageintro.ui
   kwalletwizardpageoptions.ui
   kwalletwizardpagepassword.ui
)

find_file(kwallet_xml org.kde.KWallet.xml HINTS ${KDE4_DBUS_INTERFACES_DIR} )

qt4_add_dbus_adaptor( kwalletd_KDEINIT_SRCS ${kwallet_xml} kwalletd.h KWalletD )

kde4_add_kdeinit_executable( kwalletd NOGUI ${kwalletd_KDEINIT_SRCS} )

target_link_libraries(kdeinit_kwalletd  ${KDE4_KDEUI_LIBS} kwalletbackend )
install(TARGETS kdeinit_kwalletd  ${INSTALL_TARGETS_DEFAULT_ARGS})

target_link_libraries(kwalletd kdeinit_kwalletd)
install(TARGETS kwalletd  ${INSTALL_TARGETS_DEFAULT_ARGS} )

########### install files ###############

install( FILES kwalletd.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES kwalletd.notifyrc DESTINATION  ${DATA_INSTALL_DIR}/kwalletd )

add_subdirectory(tests)
