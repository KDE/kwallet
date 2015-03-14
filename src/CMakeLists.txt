set(CMAKE_BUILD_TYPE Debug)
FIND_PACKAGE(KF5 ${KF5_VERSION} REQUIRED
    Wallet
    CoreAddons
    I18n)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)
set(kwallet-dmenu_SRCS
    main.cpp
    querydriver.cpp)

add_executable(kwallet-dmenu ${kwallet-dmenu_SRCS})

TARGET_LINK_LIBRARIES(kwallet-dmenu
    KF5::Wallet
    KF5::CoreAddons
    KF5::I18n
)

install( TARGETS kwallet-dmenu DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
