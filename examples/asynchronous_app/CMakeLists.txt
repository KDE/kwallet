set(SRCS main.cpp dialog.cpp)

add_executable(kwallet-example-asynchronous ${SRCS})
target_link_libraries(kwallet-example-asynchronous KF5::Wallet Qt5::Widgets)



