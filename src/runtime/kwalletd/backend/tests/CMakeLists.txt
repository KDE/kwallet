set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

include_directories(
   ${CMAKE_CURRENT_SOURCE_DIR}/..
   ${CMAKE_CURRENT_BINARY_DIR}/..
)


########### next target ###############

set(backendtest_SRCS backendtest.cpp )


kde4_add_executable(backendtest TEST ${backendtest_SRCS})

target_link_libraries(backendtest  ${KDE4_KDEUI_LIBS} kwalletbackend )


########### next target ###############

set(testbf_SRCS testbf.cpp )


kde4_add_executable(testbf TEST ${testbf_SRCS})

target_link_libraries(testbf  ${KDE4_KDECORE_LIBS} kwalletbackend )


########### next target ###############

set(testsha_SRCS testsha.cpp )


kde4_add_executable(testsha TEST ${testsha_SRCS})

target_link_libraries(testsha  ${KDE4_KDECORE_LIBS} kwalletbackend )

########### install files ###############

