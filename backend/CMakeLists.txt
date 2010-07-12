########### Configure checks for kwalletbackend ###############

check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(sys/bitypes.h HAVE_SYS_BITYPES_H)

configure_file (config-kwalletbackend.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kwalletbackend.h )

########### kwalletbackend ###############

set(kwalletbackend_LIB_SRCS
   blockcipher.cc
   blowfish.cc
   cbc.cc
   sha1.cc
   kwalletentry.cc
   kwalletbackend.cc
)

kde4_add_library(kwalletbackend SHARED ${kwalletbackend_LIB_SRCS})

target_link_libraries(kwalletbackend ${KDE4_KDEUI_LIBS} )

# link with advapi32 on windows
if(WIN32 AND NOT WINCE)
   target_link_libraries(kwalletbackend advapi32)
endif(WIN32 AND NOT WINCE)

set_target_properties(kwalletbackend PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION} )
install(TARGETS kwalletbackend ${INSTALL_TARGETS_DEFAULT_ARGS})

add_subdirectory(tests)
