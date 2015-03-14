
set(kwallet-dmenu_SRCS main.cpp)

add_executable(kwallet-dmenu ${kwallet-dmenu_SRCS})

TARGET_LINK_LIBRARIES(kwallet-dmenu
    KF5::Wallet
)

install( TARGETS kwallet-dmenu DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
