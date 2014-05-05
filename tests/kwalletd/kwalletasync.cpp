#include <QtCore/QTextStream>
#include <QtWidgets/QApplication>
#include <QtCore/QTimer>
#include <QtTest/QTest>

#include <kaboutdata.h>
#include <QDebug>
#include <kwallet.h>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusReply>
#include <KLocalizedString>

#include "kwalletasync.h"
#include "kwallettest.h"

static QTextStream _out(stdout, QIODevice::WriteOnly);

void KWalletAsyncTest::init()
{
    if (!qEnvironmentVariableIsSet("DISPLAY")) {
        QSKIP("$DISPLAY is not set. These tests cannot be done without a graphical system.");
    }
}

void KWalletAsyncTest::openWallet()
{
    _out << "About to ask for wallet async" << endl;

    // we have no wallet: ask for one.
    KWallet::Wallet *wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Asynchronous);
    QVERIFY(wallet != 0);

    WalletReceiver r;
    QVERIFY(r.connect(wallet, SIGNAL(walletOpened(bool)), SLOT(walletOpened(bool))));

    _out << "About to start 30 second event loop" << endl;

    QTimer::singleShot(30000, qApp, SLOT(quit()));
    int ret = qApp->exec();

    if (ret == 0) {
        _out << "Timed out!" << endl;
    } else {
        _out << "Success!" << endl;
    }
    QVERIFY2(ret == 1, "Timeout when waiting for wallet open");
}

void WalletReceiver::walletOpened(bool got)
{
    _out << "Got async wallet: " << got << endl;
    qApp->exit(1);
}

QTEST_GUILESS_MAIN(KWalletAsyncTest);
