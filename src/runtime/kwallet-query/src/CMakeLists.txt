if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif ()
set(KF5Wallet_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../../api/KWallet)
FIND_PACKAGE(KF5 ${KF5_VERSION} REQUIRED
    CoreAddons
    I18n)

include_directories(${CMAKE_SOURCE_DIR}/src/api/KWallet)
include_directories(${CMAKE_BINARY_DIR}/src/api/KWallet)

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
