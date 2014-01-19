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

static QTextStream _out(stdout, QIODevice::WriteOnly);

void openWallet()
{
	_out << "About to ask for wallet /tmp/test.kwl sync" << endl;

	KWallet::Wallet *wallet = KWallet::Wallet::openWallet("/tmp/test.kwl", 0, KWallet::Wallet::Path);
    QVERIFY(wallet != 0);

	_out << "Got path wallet: " << (wallet != 0) << endl;
	
	if (wallet) {
		_out << "Closing wallet" << endl;
		delete wallet;
	}
	
	QApplication::exit(0);
}

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
    app.setApplicationName("kwalletpath");
    app.setApplicationDisplayName(i18n("kwalletpath"));

	openWallet();

	exit(0);
}
