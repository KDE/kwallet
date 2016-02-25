remove_definitions(-DQT_NO_CAST_FROM_ASCII)
set(kwallet-query_SRCS
    main.cpp
    querydriver.cpp)

add_executable(kwallet-query ${kwallet-query_SRCS})

TARGET_LINK_LIBRARIES(kwallet-query
    KF5Wallet
    KF5::CoreAddons
    KF5::I18n
    Qt5::Widgets
)

install( TARGETS kwallet-query DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
