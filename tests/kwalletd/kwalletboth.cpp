#include "kwalletboth.h"

#include <QTextStream>
#include <QTimer>
#include <QMap>
#include <QApplication>
#include <QTest>

#include <kaboutdata.h>
#include <kwallet.h>
#include <QDBusConnectionInterface>
#include <QDBusConnection>

#include "kwallettest.h"

static QTextStream _out(stdout, QIODevice::WriteOnly);

void KWalletBothTest::init()
{
    if (!qEnvironmentVariableIsSet("DISPLAY")) {
        QSKIP("$DISPLAY is not set. These tests cannot be done without a graphical system.");
    }
}

void KWalletBothTest::openWallet()
{
    _out << "About to ask for wallet async\n";

    // we have no wallet: ask for one.
    KWallet::Wallet *wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Asynchronous);
    QVERIFY(wallet != nullptr);

    WalletReceiver r;
    r.connect(wallet, SIGNAL(walletOpened(bool)), SLOT(walletOpened(bool)));

    _out << "About to ask for wallet sync\n";

    wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Synchronous);
    QVERIFY(wallet != nullptr);

    _out << "Got sync wallet: " << (wallet != nullptr) << '\n';
    _out << "About to start 30 second event loop\n";

    QTimer::singleShot(30000, qApp, SLOT(quit()));
    int ret = qApp->exec();

    if (ret == 0) {
        _out << "Timed out!\n";
    } else {
        _out << "Success!\n";
    }

    bool success = false;
    QMap<QString, QString> p;
    p = wallet->passwordList(&success);
    _out << "passwordList returned: " << success << '\n';
    _out << "passwordList returned " << p.keys().count() << " entries\n";
#if KWALLET_BUILD_DEPRECATED_SINCE(5, 72)
    p = {};
    ret = wallet->readPasswordList(QStringLiteral("*"), p);
    _out << "readPasswordList returned: " << ret << '\n';
    _out << "readPasswordList returned " << p.keys().count() << " entries\n";
#endif

    QMap<QString, QMap<QString, QString> > q;
    q = wallet->mapList(&success);
    _out << "mapList returned: " << success << '\n';
    _out << "mapList returned " << q.keys().count() << " entries\n";
#if KWALLET_BUILD_DEPRECATED_SINCE(5, 72)
    q = {};
    ret = wallet->readMapList(QStringLiteral("*"), q);
    _out << "readMapList returned: " << ret << '\n';
    _out << "readMapList returned " << q.keys().count() << " entries\n";

#endif

    QMap<QString, QByteArray> s;
    s = wallet->entriesList(&success);
    _out << "entryList returned: " << success << '\n';
    _out << "entryList returned " << s.keys().count() << " entries\n";
#if KWALLET_BUILD_DEPRECATED_SINCE(5, 72)
    s = {};
    ret = wallet->readEntryList(QStringLiteral("*"), s);
    _out << "readEntryList returned: " << ret << '\n';
    _out << "readEntryList returned " << s.keys().count() << " entries\n";
#endif

    _out.flush();

    delete wallet;
}

void WalletReceiver::walletOpened(bool got)
{
    _out << "Got async wallet: " << got << '\n';
    _out.flush();
    qApp->exit(1);
}

QTEST_GUILESS_MAIN(KWalletBothTest)

