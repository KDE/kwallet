#include <QTextStream>
#include <QTimer>
#include <QTest>

#include <kaboutdata.h>
#include <QApplication>
#include <kwallet.h>
#include <QDBusConnectionInterface>
#include <QDBusConnection>
#include <QDBusReply>
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
    QVERIFY(wallet != nullptr);

    _out << "Got path wallet: " << (wallet != nullptr) << endl;

    if (wallet) {
        _out << "Closing wallet" << endl;
        delete wallet;
    }
}

QTEST_GUILESS_MAIN(KWalletPathTest)
