
add_subdirectory(KWallet)

if(NOT KF5_KWALLET_NO_DAEMON)
add_subdirectory(kwalletd)
endif(NOT KF5_KWALLET_NO_DAEMON)
