#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtTest/QTest>

#include <kaboutdata.h>
#include <QtWidgets/QApplication>
#include <kwallet.h>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusReply>
#include <KLocalizedString>

#include "kwalletpath.h"

static QTextStream _out(stdout, QIODevice::WriteOnly);

void KWalletPathTest::init()
{
    if (!qEnvironmentVariableIsSet("DISPLAY")) {
        QSKIP("$DISPLAY is not set. These tests cannot be done without a graphical system.");
    }
}

void KWalletPathTest::openWallet()
{
    _out << "About to ask for wallet /tmp/test.kwl sync" << endl;

    KWallet::Wallet *wallet = KWallet::Wallet::openWallet(QStringLiteral("/tmp/test.kwl"), 0, KWallet::Wallet::Path);
    QVERIFY(wallet != 0);

    _out << "Got path wallet: " << (wallet != 0) << endl;

    if (wallet) {
        _out << "Closing wallet" << endl;
        delete wallet;
    }
}

QTEST_GUILESS_MAIN(KWalletPathTest)
