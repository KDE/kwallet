#include <QtCore/QTextStream>
#include <QtWidgets/QApplication>
#include <QtCore/QTimer>

#include <kaboutdata.h>
#include <QDebug>
#include <kwallet.h>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusReply>
#include <KLocalizedString>

#include "kwallettest.h"

static QTextStream _out( stdout, QIODevice::WriteOnly );

void openWallet()
{
	_out << "About to ask for wallet async" << endl;

        // we have no wallet: ask for one.
	KWallet::Wallet *wallet = KWallet::Wallet::openWallet( KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Asynchronous );

	WalletReceiver r;
	r.connect( wallet, SIGNAL( walletOpened(bool) ), SLOT( walletOpened(bool) ) );

	_out << "About to start 30 second event loop" << endl;

	QTimer::singleShot( 30000, qApp, SLOT( quit() ) );
	int ret = qApp->exec();

	if ( ret == 0 )
		_out << "Timed out!" << endl;
	else
		_out << "Success!" << endl;
}

void WalletReceiver::walletOpened( bool got )
{
	_out << "Got async wallet: " << got << endl;
	qApp->exit( 1 );
}

int main( int argc, char *argv[] )
{
	QApplication app( argc, argv );
    app.setApplicationName("kwalletasync");
    app.setApplicationDisplayName(i18n("kwalletasync"));
    

	// force name with D-BUS
        QDBusReply<QDBusConnectionInterface::RegisterServiceReply> reply
            = QDBusConnection::sessionBus().interface()->registerService( "org.kde.kwalletasync",
                                                                QDBusConnectionInterface::ReplaceExistingService );

        if ( !reply.isValid() )
        {
                _out << "D-BUS name request returned " << reply.error().name() << endl;
        }

	openWallet();

	return 0;
}

// vim: set noet ts=4 sts=4 sw=4:

