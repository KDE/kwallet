#include <stdlib.h>
#include <stdio.h>

#include <QtWidgets/QApplication>
#include <QtCore/QString>

#include "kwalletbackend.h"

int main(int argc, char **argv)
{
    QApplication a(argc, argv);
    a.setApplicationName("backendtest");

    KWallet::Backend be("ktestwallet");
    printf("KWalletBackend constructed\n");

    QByteArray apass("apassword", 9);
    QByteArray bpass("bpassword", 9);
    QByteArray cpass("cpassword", 9);

    printf("Passwords initialised.\n");
    be.setPassword(apass);
    int rc = be.close(true);

    printf("be.close(true) returned %d  (should be -255)\n", rc);

    rc = be.open(bpass);

    printf("be.open(bpass) returned %d  (should be 0 or 1)\n", rc);

    rc = be.close(true);

    printf("be.close(true) returned %d  (should be 0)\n", rc);

    rc = be.open(apass);

    printf("be.open(apass) returned %d  (should be negative)\n", rc);

    rc = be.open(bpass);

    printf("be.open(bpass) returned %d  (should be 0)\n", rc);

    return 0;
}

