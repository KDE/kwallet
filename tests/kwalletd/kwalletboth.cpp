#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QMap>
#include <QtWidgets/QApplication>
#include <QtTest/QTest>

#include <kaboutdata.h>
#include <kwallet.h>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusReply>
#include <KLocalizedString>

#include "kwalletboth.h"
#include "kwallettest.h"

static QTextStream _out( stdout, QIODevice::WriteOnly );

void KWalletBothTest::init()
{
    if (!qEnvironmentVariableIsSet("DISPLAY")) {
        QSKIP("$DISPLAY is not set. These tests cannot be done without a graphical system.");
    }
}

void KWalletBothTest::openWallet()
{
	_out << "About to ask for wallet async" << endl;

        // we have no wallet: ask for one.
	KWallet::Wallet *wallet = KWallet::Wallet::openWallet( KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Asynchronous );
    QVERIFY(wallet != 0);

	WalletReceiver r;
	r.connect( wallet, SIGNAL( walletOpened(bool) ), SLOT( walletOpened(bool) ) );

	_out << "About to ask for wallet sync" << endl;

	wallet = KWallet::Wallet::openWallet( KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Synchronous );
    QVERIFY(wallet != 0);

	_out << "Got sync wallet: " << (wallet != 0) << endl;
	_out << "About to start 30 second event loop" << endl;

	QTimer::singleShot( 30000, qApp, SLOT( quit() ) );
	int ret = qApp->exec();


	if ( ret == 0 )
		_out << "Timed out!" << endl;
	else
		_out << "Success!" << endl;

	QMap<QString,QString> p;
	ret = wallet->readPasswordList("*", p);
	_out << "readPasswordList returned: " << ret << endl;
	_out << "readPasswordList returned " << p.keys().count() << " entries" << endl;
	QMap<QString, QMap<QString, QString> > q;
	ret = wallet->readMapList("*", q);
	_out << "readMapList returned: " << ret << endl;
	_out << "readMapList returned " << q.keys().count() << " entries" << endl;

	QMap<QString, QByteArray> s;
	ret = wallet->readEntryList("*", s);
	_out << "readEntryList returned: " << ret << endl;
	_out << "readEntryList returned " << s.keys().count() << " entries" << endl;

	delete wallet;
}

void WalletReceiver::walletOpened( bool got )
{
	_out << "Got async wallet: " << got << endl;
	qApp->exit( 1 );
}

QTEST_GUILESS_MAIN(KWalletBothTest)
// vim: set noet ts=4 sts=4 sw=4:

