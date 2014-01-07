#include <QtCore/QTextStream>
#include <QtCore/QTimer>

#include <kaboutdata.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kwallet.h>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusReply>
#include <klocale.h>

static QTextStream _out(stdout, QIODevice::WriteOnly);

void openWallet()
{
	_out << "About to ask for wallet /tmp/test.kwl sync" << endl;

	KWallet::Wallet *wallet = KWallet::Wallet::openWallet("/tmp/test.kwl", 0, KWallet::Wallet::Path);

	_out << "Got path wallet: " << (wallet != 0) << endl;
	
	if (wallet) {
		_out << "Closing wallet" << endl;
		delete wallet;
	}
	
	kapp->exit(0);
}

int main(int argc, char *argv[])
{
	KAboutData aboutData("kwalletpath", 0, ki18n("kwalletpath"), "version");
	KCmdLineArgs::init(argc, argv, &aboutData);
	KApplication app;

	openWallet();

	exit(0);
}
