#include <QtCore/QTextStream>
#include <QtWidgets/QApplication>
#include <QtCore/QTimer>
#include <QtTest/QTest>

#include <kaboutdata.h>
#include <kwallet.h>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusReply>
#include <KLocalizedString>

#include "kwalletsync.h"

static QTextStream _out( stdout, QIODevice::WriteOnly );

void KWalletSyncTest::init()
{
    if (!qEnvironmentVariableIsSet("DISPLAY")) {
        QSKIP("$DISPLAY is not set. These tests cannot be done without a graphical system.");
    }
}

void KWalletSyncTest::openWallet()
{
	_out << "About to ask for wallet sync" << endl;

	KWallet::Wallet *w = KWallet::Wallet::openWallet( KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Synchronous );
    QVERIFY(w != 0);

	_out << "Got sync wallet: " << (w != 0) << endl;
}

QTEST_GUILESS_MAIN(KWalletSyncTest)

// vim: set noet ts=4 sts=4 sw=4:

