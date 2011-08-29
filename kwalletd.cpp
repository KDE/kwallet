// -*- indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4; -*-
/*
   This file is part of the KDE libraries

   Copyright (c) 2002-2004 George Staikos <staikos@kde.org>
   Copyright (c) 2008 Michael Leupold <lemma@confuego.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.

*/

#include "kwalletd.h"

#include "kbetterthankdialog.h"
#include "kwalletwizard.h"

#include <kuniqueapplication.h>
#include <ktoolinvocation.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <kdirwatch.h>
#include <kglobal.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kpassworddialog.h>
#include <knewpassworddialog.h>
#include <kstandarddirs.h>
#include <kwalletentry.h>
#include <kwindowsystem.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <KNotification>

#include <QtCore/QDir>
#include <QtGui/QTextDocument> // Qt::escape
#include <QtCore/QRegExp>
#include <QtCore/QTimer>
#include <QtCore/QEventLoop>

#include <assert.h>

#include "kwalletadaptor.h"
#include "kwalletopenloop.h"

class KWalletTransaction {

    public:
        KWalletTransaction()
            : tType(Unknown), cancelled(false), tId(nextTransactionId)
        {
            nextTransactionId++;
            // make sure the id is never < 0 as that's used for the
            // error conditions.
            if (nextTransactionId < 0) {
                nextTransactionId = 0;
            }
        }

        ~KWalletTransaction() {
        }

        enum Type {
            Unknown,
            Open,
            ChangePassword,
            OpenFail,
            CloseCancelled
        };
        Type tType;
        QString appid;
        qlonglong wId;
        QString wallet;
        QString service;
        bool cancelled; // set true if the client dies before open
        bool modal;
        bool isPath;
        int tId; // transaction id

    private:
        static int nextTransactionId;
};

int KWalletTransaction::nextTransactionId = 0;

KWalletD::KWalletD()
 : QObject(0), _failed(0), _syncTime(5000), _curtrans(0) {

	srand(time(0));
	_showingFailureNotify = false;
	_closeIdle = false;
	_idleTime = 0;
	connect(&_closeTimers, SIGNAL(timedOut(int)), this, SLOT(timedOutClose(int)));
	connect(&_syncTimers, SIGNAL(timedOut(int)), this, SLOT(timedOutSync(int)));

	(void)new KWalletAdaptor(this);
	// register services
	QDBusConnection::sessionBus().registerService(QLatin1String("org.kde.kwalletd"));
	QDBusConnection::sessionBus().registerObject(QLatin1String("/modules/kwalletd"), this);
	
#ifdef Q_WS_X11
	screensaver = new QDBusInterface("org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver");
#endif

	reconfigure();
	KGlobal::dirs()->addResourceType("kwallet", 0, "share/apps/kwallet");
	_dw = new KDirWatch(this );
		_dw->setObjectName( QLatin1String( "KWallet Directory Watcher" ) );
	_dw->addDir(KGlobal::dirs()->saveLocation("kwallet"));
	_dw->startScan(true);
	connect(_dw, SIGNAL(dirty(const QString&)), this, SLOT(emitWalletListDirty()));

    _serviceWatcher.setWatchMode( QDBusServiceWatcher::WatchForOwnerChange );
    connect(&_serviceWatcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)), this, SLOT(slotServiceOwnerChanged(QString,QString,QString)));
}


KWalletD::~KWalletD() {
#ifdef Q_WS_X11
	delete screensaver;
	screensaver = 0;
#endif
	closeAllWallets();
	qDeleteAll(_transactions);
}


int KWalletD::generateHandle() {
	int rc;

	// ASSUMPTION: RAND_MAX is fairly large.
	do {
		rc = rand();
	} while (_wallets.contains(rc) || rc == 0);

	return rc;
}

QPair<int, KWallet::Backend*> KWalletD::findWallet(const QString& walletName) const
{
	Wallets::const_iterator it = _wallets.constBegin();
	const Wallets::const_iterator end = _wallets.constEnd();
	for (; it != end; ++it) {
		if (it.value()->walletName() == walletName) {
			return qMakePair(it.key(), it.value());
		}
	}
    return qMakePair(-1, static_cast<KWallet::Backend*>(0));
}

bool KWalletD::_processing = false;

void KWalletD::processTransactions() {
	if (_processing) {
		return;
	}

	_processing = true;

	// Process remaining transactions
	while (!_transactions.isEmpty()) {
		_curtrans = _transactions.takeFirst();
		int res;

		assert(_curtrans->tType != KWalletTransaction::Unknown);

		switch (_curtrans->tType) {
			case KWalletTransaction::Open:
				res = doTransactionOpen(_curtrans->appid, _curtrans->wallet, _curtrans->isPath,
				                        _curtrans->wId, _curtrans->modal, _curtrans->service);

				// multiple requests from the same client
				// should not produce multiple password
				// dialogs on a failure
				if (res < 0) {
					QList<KWalletTransaction *>::iterator it;
					for (it = _transactions.begin(); it != _transactions.end(); ++it) {
						KWalletTransaction *x = *it;
						if (_curtrans->appid == x->appid && x->tType == KWalletTransaction::Open
							&& x->wallet == _curtrans->wallet && x->wId == _curtrans->wId) {
							x->tType = KWalletTransaction::OpenFail;
						}
					}
				} else if (_curtrans->cancelled) {
					// the wallet opened successfully but the application
					// opening exited/crashed while the dialog was still shown.
					KWalletTransaction *_xact = new KWalletTransaction();
					_xact->tType = KWalletTransaction::CloseCancelled;
					_xact->appid = _curtrans->appid;
					_xact->wallet = _curtrans->wallet;
					_xact->service = _curtrans->service;
					_transactions.append(_xact);
				}
				
				// emit the AsyncOpened signal as a reply
				emit walletAsyncOpened(_curtrans->tId, res);
				break;
				
			case KWalletTransaction::OpenFail:
				// emit the AsyncOpened signal with an invalid handle
				emit walletAsyncOpened(_curtrans->tId, -1);
				break;
				
			case KWalletTransaction::ChangePassword:
				doTransactionChangePassword(_curtrans->appid, _curtrans->wallet, _curtrans->wId);
				break;

			case KWalletTransaction::CloseCancelled:
				doTransactionOpenCancelled(_curtrans->appid, _curtrans->wallet,
				                            _curtrans->service);
				break;
				
			case KWalletTransaction::Unknown:
				break;
			default:
				break;
		}
		
		delete _curtrans;
		_curtrans = 0;
	}

	_processing = false;
}



int KWalletD::openPath(const QString& path, qlonglong wId, const QString& appid) {
	int tId = openPathAsync(path, wId, appid, false);
	if (tId < 0) {
		return tId;
	}
	
	// wait for the open-transaction to be processed
	KWalletOpenLoop loop(this);
	return loop.waitForAsyncOpen(tId);
}

int KWalletD::open(const QString& wallet, qlonglong wId, const QString& appid) {
	int tId = openAsync(wallet, wId, appid, false);
	if (tId < 0) {
		return tId;
	}
	
	// wait for the open-transaction to be processed
	KWalletOpenLoop loop(this);
	return loop.waitForAsyncOpen(tId);
}

int KWalletD::openAsync(const QString& wallet, qlonglong wId, const QString& appid,
                        bool handleSession) {
	if (!_enabled) { // guard
		return -1;
	}
	
	if (!QRegExp("^[\\w\\^\\&\\'\\@\\{\\}\\[\\]\\,\\$\\=\\!\\-\\#\\(\\)\\%\\.\\+\\_\\s]+$").exactMatch(wallet)) {
		return -1;
	}

	KWalletTransaction *xact = new KWalletTransaction;
	_transactions.append(xact);

	xact->appid = appid;
	xact->wallet = wallet;
	xact->wId = wId;
	xact->modal = true; // mark dialogs as modal, the app has blocking wait
	xact->tType = KWalletTransaction::Open;
	xact->isPath = false;
	if (handleSession) {
        kDebug() << "openAsync for " << message().service();
        _serviceWatcher.setConnection(connection());
        _serviceWatcher.addWatchedService(message().service());
		xact->service = message().service();
	}
	QTimer::singleShot(0, this, SLOT(processTransactions()));
	checkActiveDialog();
	// opening is in progress. return the transaction number
	return xact->tId;
}

int KWalletD::openPathAsync(const QString& path, qlonglong wId, const QString& appid,
                            bool handleSession) {
	if (!_enabled) { // gaurd
		return -1;
	}
	
	KWalletTransaction *xact = new KWalletTransaction;
	_transactions.append(xact);
	
	xact->appid = appid;
	xact->wallet = path;
	xact->wId = wId;
	xact->modal = true;
	xact->tType = KWalletTransaction::Open;
	xact->isPath = true;
	if (handleSession) {
        kDebug() << "openPathAsync " << message().service();
        _serviceWatcher.setConnection(connection());
        _serviceWatcher.addWatchedService(message().service());
		xact->service = message().service();
	}
	QTimer::singleShot(0, this, SLOT(processTransactions()));
	checkActiveDialog();
	// opening is in progress. return the transaction number
	return xact->tId;
}

// Sets up a dialog that will be shown by kwallet.
void KWalletD::setupDialog( QWidget* dialog, WId wId, const QString& appid, bool modal ) {
	if( wId != 0 )
		KWindowSystem::setMainWindow( dialog, wId ); // correct, set dialog parent
	else {
		if( appid.isEmpty())
			kWarning() << "Using kwallet without parent window!";
		else
			kWarning() << "Application '" << appid << "' using kwallet without parent window!";
		// allow dialog activation even if it interrupts, better than trying hacks
		// with keeping the dialog on top or on all desktops
		kapp->updateUserTimestamp();
	}
	if( modal )
		KWindowSystem::setState( dialog->winId(), NET::Modal );
	else
		KWindowSystem::clearState( dialog->winId(), NET::Modal );
	activeDialog = dialog;
}

// If there's a dialog already open and another application tries some operation that'd lead to
// opening a dialog, that application will be blocked by this dialog. A proper solution would
// be to set the second application's window also as a parent for the active dialog, so that
// KWin properly handles focus changes and so on, but there's currently no support for multiple
// dialog parents. Hopefully to be done in KDE4, for now just use all kinds of bad hacks to make
//  sure the user doesn't overlook the active dialog.
void KWalletD::checkActiveDialog() {
	if( !activeDialog || activeDialog->isHidden())
		return;
	kapp->updateUserTimestamp();
	KWindowSystem::setState( activeDialog->winId(), NET::KeepAbove );
	KWindowSystem::setOnAllDesktops( activeDialog->winId(), true );
	KWindowSystem::forceActiveWindow( activeDialog->winId());
}


int KWalletD::doTransactionOpen(const QString& appid, const QString& wallet, bool isPath,
                                qlonglong wId, bool modal, const QString& service) {
	if (_firstUse && !wallets().contains(KWallet::Wallet::LocalWallet()) && !isPath) {
		// First use wizard
		QPointer<KWalletWizard> wiz = new KWalletWizard(0);
		wiz->setWindowTitle(i18n("KDE Wallet Service"));
		setupDialog( wiz, (WId)wId, appid, modal );
		int rc = wiz->exec();
		if (rc == QDialog::Accepted && wiz) {
			bool useWallet = wiz->field("useWallet").toBool();
			KConfig kwalletrc("kwalletrc");
			KConfigGroup cfg(&kwalletrc, "Wallet");
			cfg.writeEntry("First Use", false);
			cfg.writeEntry("Enabled", useWallet);
			cfg.writeEntry("Close When Idle", wiz->field("closeWhenIdle").toBool());
			cfg.writeEntry("Use One Wallet", !wiz->field("networkWallet").toBool());
			cfg.sync();
			reconfigure();

			if (!useWallet) {
				delete wiz;
				return -1;
			}

			// Create the wallet
			KWallet::Backend *b = new KWallet::Backend(KWallet::Wallet::LocalWallet());
			QString pass = wiz->field("pass1").toString();
			QByteArray p(pass.toUtf8(), pass.length());
			b->open(p);
			p.fill(0);
			b->createFolder(KWallet::Wallet::PasswordFolder());
			b->createFolder(KWallet::Wallet::FormDataFolder());
			b->close(true);
			delete b;
			delete wiz;
		} else {
			delete wiz;
			return -1;
		}
	} else if (_firstUse && !isPath) {
		KConfig kwalletrc("kwalletrc");
		KConfigGroup cfg(&kwalletrc, "Wallet");
		_firstUse = false;
		cfg.writeEntry("First Use", false);
	}

	int rc = internalOpen(appid, wallet, isPath, WId(wId), modal, service);
	return rc;
}


int KWalletD::internalOpen(const QString& appid, const QString& wallet, bool isPath, WId w,
                           bool modal, const QString& service) {
	bool brandNew = false;
	
	QString thisApp;
	if (appid.isEmpty()) {
		thisApp = "KDE System";
	} else {
		thisApp = appid;
	}

	if (implicitDeny(wallet, thisApp)) {
		return -1;
	}

	QPair<int, KWallet::Backend*> walletInfo = findWallet(wallet);
	int rc = walletInfo.first;
	if (rc == -1) {
		if (_wallets.count() > 20) {
			kDebug() << "Too many wallets open.";
			return -1;
		}

		KWallet::Backend *b = new KWallet::Backend(wallet, isPath);
		QString password;
		bool emptyPass = false;
		if ((isPath && QFile::exists(wallet)) || (!isPath && KWallet::Backend::exists(wallet))) {
			int pwless = b->open(QByteArray());
			if (0 != pwless || !b->isOpen()) {
				if (pwless == 0) {
					// release, start anew
					delete b;
					b = new KWallet::Backend(wallet, isPath);
				}
				KPasswordDialog *kpd = new KPasswordDialog();
				if (appid.isEmpty()) {
					kpd->setPrompt(i18n("<qt>KDE has requested to open the wallet '<b>%1</b>'. Please enter the password for this wallet below.</qt>", Qt::escape(wallet)));
				} else {
					kpd->setPrompt(i18n("<qt>The application '<b>%1</b>' has requested to open the wallet '<b>%2</b>'. Please enter the password for this wallet below.</qt>", Qt::escape(appid), Qt::escape(wallet)));
				}
				brandNew = false;
				// don't use KStdGuiItem::open() here which has trailing ellipsis!
				kpd->setButtonGuiItem(KDialog::Ok,KGuiItem( i18n( "&Open" ), "wallet-open"));
				kpd->setCaption(i18n("KDE Wallet Service"));
				kpd->setPixmap(KIcon("kwalletmanager").pixmap(KIconLoader::SizeHuge));
				if (w != KWindowSystem::activeWindow() && w != 0L) {
					// If the dialog is modal to a minimized window it might not be visible
					// (but still blocking the calling application). Notify the user about
					// the request to open the wallet.
					KNotification *notification = new KNotification("needsPassword", kpd,
					                                                KNotification::Persistent |
					                                                KNotification::CloseWhenWidgetActivated);
					QStringList actions(i18nc("Text of a button to ignore the open-wallet notification", "Ignore"));
					if (appid.isEmpty()) {
						notification->setText(i18n("<b>KDE</b> has requested to open a wallet (%1).",
						                           Qt::escape(wallet)));
						actions.append(i18nc("Text of a button for switching to the (unnamed) application "
						                     "requesting a password", "Switch there"));
					} else {
						notification->setText(i18n("<b>%1</b> has requested to open a wallet (%2).",
						                           Qt::escape(appid), Qt::escape(wallet)));
						actions.append(i18nc("Text of a button for switching to the application requesting "
						                     "a password", "Switch to %1", Qt::escape(appid)));
					}
					notification->setActions(actions);
					connect(notification, SIGNAL(action1Activated()),
					        notification, SLOT(close()));
					connect(notification, SIGNAL(action2Activated()),
					        this, SLOT(activatePasswordDialog()));
					notification->sendEvent();
				}
				while (!b->isOpen()) {
					setupDialog( kpd, w, appid, modal );
					if (kpd->exec() == KDialog::Accepted) {
						password = kpd->password();
						int rc = b->open(password.toUtf8());
						if (!b->isOpen()) {
							kpd->setPrompt(i18n("<qt>Error opening the wallet '<b>%1</b>'. Please try again.<br />(Error code %2: %3)</qt>", Qt::escape(wallet), rc, KWallet::Backend::openRCToString(rc)));
						}
					} else {
						break;
					}
				}
				delete kpd;
			} else {
				emptyPass = true;
			}
		} else {
			KNewPasswordDialog *kpd = new KNewPasswordDialog();
			if (wallet == KWallet::Wallet::LocalWallet() ||
						 wallet == KWallet::Wallet::NetworkWallet())
			{
				// Auto create these wallets.
				if (appid.isEmpty()) {
					kpd->setPrompt(i18n("KDE has requested to open the wallet. This is used to store sensitive data in a secure fashion. Please enter a password to use with this wallet or click cancel to deny the application's request."));
				} else {
					kpd->setPrompt(i18n("<qt>The application '<b>%1</b>' has requested to open the KDE wallet. This is used to store sensitive data in a secure fashion. Please enter a password to use with this wallet or click cancel to deny the application's request.</qt>", Qt::escape(appid)));
				}
			} else {
				if (appid.length() == 0) {
					kpd->setPrompt(i18n("<qt>KDE has requested to create a new wallet named '<b>%1</b>'. Please choose a password for this wallet, or cancel to deny the application's request.</qt>", Qt::escape(wallet)));
				} else {
					kpd->setPrompt(i18n("<qt>The application '<b>%1</b>' has requested to create a new wallet named '<b>%2</b>'. Please choose a password for this wallet, or cancel to deny the application's request.</qt>", Qt::escape(appid), Qt::escape(wallet)));
				}
			}
			brandNew = true;
			kpd->setCaption(i18n("KDE Wallet Service"));
			kpd->setButtonGuiItem(KDialog::Ok,KGuiItem(i18n("C&reate"),"document-new"));
			kpd->setPixmap(KIcon("kwalletmanager").pixmap(96, 96));
			while (!b->isOpen()) {
				setupDialog( kpd, w, appid, modal );
				if (kpd->exec() == KDialog::Accepted) {
					password = kpd->password();
					int rc = b->open(password.toUtf8());
					if (!b->isOpen()) {
						kpd->setPrompt(i18n("<qt>Error opening the wallet '<b>%1</b>'. Please try again.<br />(Error code %2: %3)</qt>", Qt::escape(wallet), rc, KWallet::Backend::openRCToString(rc)));
					}
				} else {
					break;
				}
			}
			delete kpd;
		}



		if (!emptyPass && (password.isNull() || !b->isOpen())) {
			delete b;
			return -1;
		}

		if (emptyPass && _openPrompt && !isAuthorizedApp(appid, wallet, w)) {
			delete b;
			return -1;
		}

		_wallets.insert(rc = generateHandle(), b);
		_sessions.addSession(appid, service, rc);
		_syncTimers.addTimer(rc, _syncTime);

		if (brandNew) {
			createFolder(rc, KWallet::Wallet::PasswordFolder(), appid);
			createFolder(rc, KWallet::Wallet::FormDataFolder(), appid);
		}

		b->ref();
		if (_closeIdle) {
			_closeTimers.addTimer(rc, _idleTime);
		}
		if (brandNew)
			emit walletCreated(wallet);
		emit walletOpened(wallet);
		if (_wallets.count() == 1 && _launchManager) {
			KToolInvocation::startServiceByDesktopName("kwalletmanager-kwalletd");
		}
	} else {
		// prematurely add a reference so that the wallet does not close while the
		// authorization dialog is being shown.
		walletInfo.second->ref();
		bool isAuthorized = _sessions.hasSession(appid, rc) || isAuthorizedApp(appid, wallet, w);
		// as the wallet might have been forcefully closed, find it again to make sure it's
		// still available (isAuthorizedApp might show a dialog).
		walletInfo = findWallet(wallet);
		if (!isAuthorized) {
			if (walletInfo.first != -1) {
				walletInfo.second->deref();
				// check if the wallet should be closed now.
				internalClose(walletInfo.second, walletInfo.first, false);
			}
			return -1;
		} else {
			if (walletInfo.first != -1) {
				_sessions.addSession(appid, service, rc);
			} else {
				// wallet was forcefully closed.
				return -1;
			}
		}
	}

	return rc;
}


bool KWalletD::isAuthorizedApp(const QString& appid, const QString& wallet, WId w) {
	int response = 0;

	QString thisApp;
	if (appid.isEmpty()) {
		thisApp = "KDE System";
	} else {
		thisApp = appid;
	}

	if (!implicitAllow(wallet, thisApp)) {
		KConfigGroup cfg = KSharedConfig::openConfig("kwalletrc")->group("Auto Allow");
		if (!cfg.isEntryImmutable(wallet)) {
			KBetterThanKDialog *dialog = new KBetterThanKDialog;
			dialog->setWindowTitle(i18n("KDE Wallet Service"));
			if (appid.isEmpty()) {
				dialog->setLabel(i18n("<qt>KDE has requested access to the open wallet '<b>%1</b>'.</qt>", Qt::escape(wallet)));
			} else {
				dialog->setLabel(i18n("<qt>The application '<b>%1</b>' has requested access to the open wallet '<b>%2</b>'.</qt>", Qt::escape(QString(appid)), Qt::escape(wallet)));
			}
			setupDialog( dialog, w, appid, false );
			response = dialog->exec();
			delete dialog;
		}
	}

	if (response == 0 || response == 1) {
		if (response == 1) {
			KConfigGroup cfg = KSharedConfig::openConfig("kwalletrc")->group("Auto Allow");
			QStringList apps = cfg.readEntry(wallet, QStringList());
			if (!apps.contains(thisApp)) {
				if (cfg.isEntryImmutable(wallet)) {
					return false;
				}
				apps += thisApp;
				_implicitAllowMap[wallet] += thisApp;
				cfg.writeEntry(wallet, apps);
				cfg.sync();
			}
		}
	} else if (response == 3) {
		KConfigGroup cfg = KSharedConfig::openConfig("kwalletrc")->group("Auto Deny");
		QStringList apps = cfg.readEntry(wallet, QStringList());
		if (!apps.contains(thisApp)) {
			apps += thisApp;
			_implicitDenyMap[wallet] += thisApp;
			cfg.writeEntry(wallet, apps);
			cfg.sync();
		}
		return false;
	} else {
		return false;
	}
	return true;
}


int KWalletD::deleteWallet(const QString& wallet) {
	QString path = KGlobal::dirs()->saveLocation("kwallet") + QDir::separator() + wallet + ".kwl";

	if (QFile::exists(path)) {
		const QPair<int, KWallet::Backend*> walletInfo = findWallet(wallet);
		internalClose(walletInfo.second, walletInfo.first, true);
		QFile::remove(path);
		emit walletDeleted(wallet);
		return 0;
	}

	return -1;
}


void KWalletD::changePassword(const QString& wallet, qlonglong wId, const QString& appid) {
	KWalletTransaction *xact = new KWalletTransaction;

	//msg.setDelayedReply(true);
	xact->appid = appid;
	xact->wallet = wallet;
	xact->wId = wId;
	xact->modal = false;
	xact->tType = KWalletTransaction::ChangePassword;

	_transactions.append(xact);

	QTimer::singleShot(0, this, SLOT(processTransactions()));
	checkActiveDialog();
	checkActiveDialog();
}

void KWalletD::initiateSync(int handle) {
	// add a timer and reset it right away
	_syncTimers.addTimer(handle, _syncTime);
	_syncTimers.resetTimer(handle, _syncTime);
}

void KWalletD::doTransactionChangePassword(const QString& appid, const QString& wallet, qlonglong wId) {

	const QPair<int, KWallet::Backend*> walletInfo = findWallet(wallet);
	int handle = walletInfo.first;
	KWallet::Backend* w = walletInfo.second;

	bool reclose = false;
	if (!w) {
		handle = doTransactionOpen(appid, wallet, false, wId, false, "");
		if (-1 == handle) {
			KMessageBox::sorryWId((WId)wId, i18n("Unable to open wallet. The wallet must be opened in order to change the password."), i18n("KDE Wallet Service"));
			return;
		}

		w = _wallets.value(handle);
		reclose = true;
	}

	assert(w);

	QPointer<KNewPasswordDialog> kpd = new KNewPasswordDialog();
	kpd->setPrompt(i18n("<qt>Please choose a new password for the wallet '<b>%1</b>'.</qt>", Qt::escape(wallet)));
	kpd->setCaption(i18n("KDE Wallet Service"));
	kpd->setAllowEmptyPasswords(true);
	setupDialog( kpd, (WId)wId, appid, false );
	if (kpd->exec() == KDialog::Accepted && kpd) {
		QString p = kpd->password();
		if (!p.isNull()) {
			w->setPassword(p.toUtf8());
			int rc = w->close(true);
			if (rc < 0) {
				KMessageBox::sorryWId((WId)wId, i18n("Error re-encrypting the wallet. Password was not changed."), i18n("KDE Wallet Service"));
				reclose = true;
			} else {
				rc = w->open(p.toUtf8());
				if (rc < 0) {
					KMessageBox::sorryWId((WId)wId, i18n("Error reopening the wallet. Data may be lost."), i18n("KDE Wallet Service"));
					reclose = true;
				}
			}
		}
	}

	delete kpd;

	if (reclose) {
		internalClose(w, handle, true);
	}
}


int KWalletD::close(const QString& wallet, bool force) {
	const QPair<int, KWallet::Backend*> walletInfo = findWallet(wallet);
	int handle = walletInfo.first;
	KWallet::Backend* w = walletInfo.second;

	return internalClose(w, handle, force);
}


int KWalletD::internalClose(KWallet::Backend *w, int handle, bool force) {
	if (w) {
		const QString& wallet = w->walletName();
		if ((w->refCount() == 0 && !_leaveOpen) || force) {
			// this is only a safety measure. sessions should be gone already.
			_sessions.removeAllSessions(handle);
			if (_closeIdle) {
				_closeTimers.removeTimer(handle);
			}
			_syncTimers.removeTimer(handle);
			_wallets.remove(handle);
			w->close(true);
			doCloseSignals(handle, wallet);
			delete w;
			return 0;
		}
		return 1;
	}

	return -1;
}


int KWalletD::close(int handle, bool force, const QString& appid) {
	KWallet::Backend *w = _wallets.value(handle);

	if (w) {
		if (_sessions.hasSession(appid, handle)) {
			// remove one handle for the application
			bool removed = _sessions.removeSession(appid, message().service(), handle);
			// alternatively try sessionless
			if (removed || _sessions.removeSession(appid, "", handle)) {
				w->deref();
			}
			return internalClose(w, handle, force);
		}
		return 1; // not closed, handle unknown
	}
	return -1; // not open to begin with, or other error
}


bool KWalletD::isOpen(const QString& wallet) {
	const QPair<int, KWallet::Backend*> walletInfo = findWallet(wallet);
	return walletInfo.second != 0;
}


bool KWalletD::isOpen(int handle) {
	if (handle == 0) {
		return false;
	}

	KWallet::Backend *rc = _wallets.value(handle);

	if (rc == 0 && ++_failed > 5) {
		_failed = 0;
		QTimer::singleShot(0, this, SLOT(notifyFailures()));
	} else if (rc != 0) {
		_failed = 0;
	}

	return rc != 0;
}


QStringList KWalletD::wallets() const {
	QString path = KGlobal::dirs()->saveLocation("kwallet");
	QDir dir(path, "*.kwl");
	QStringList rc;

	dir.setFilter(QDir::Files | QDir::Hidden);

	foreach (const QFileInfo &fi, dir.entryInfoList()) {
		QString fn = fi.fileName();
		if (fn.endsWith(QLatin1String(".kwl"))) {
			fn.truncate(fn.length()-4);
		}
		rc += fn;
	}
	return rc;
}


void KWalletD::sync(int handle, const QString& appid) {
	KWallet::Backend *b;
	
	// get the wallet and check if we have a password for it (safety measure)
	if ((b = getWallet(appid, handle))) {
		QString wallet = b->walletName();
		b->sync();
	}
}

void KWalletD::timedOutSync(int handle) {
	_syncTimers.removeTimer(handle);
	if (_wallets.contains(handle) && _wallets[handle]) {
		_wallets[handle]->sync();
	}
}

void KWalletD::doTransactionOpenCancelled(const QString& appid, const QString& wallet,
                                          const QString& service) {

	// there will only be one session left to remove - all others
	// have already been removed in slotServiceOwnerChanged and all
	// transactions for opening new sessions have been deleted.
	if (!_sessions.hasSession(appid)) {
		return;
	}

	const QPair<int, KWallet::Backend*> walletInfo = findWallet(wallet);
	int handle = walletInfo.first;
	KWallet::Backend *b = walletInfo.second;
	if (handle != -1 && b) {
		b->deref();
		internalClose(b, handle, false);
	}

	// close the session in case the wallet hasn't been closed yet
	_sessions.removeSession(appid, service, handle);
}

QStringList KWalletD::folderList(int handle, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		return b->folderList();
	}

	return QStringList();
}


bool KWalletD::hasFolder(int handle, const QString& f, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		return b->hasFolder(f);
	}

	return false;
}


bool KWalletD::removeFolder(int handle, const QString& f, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		bool rc = b->removeFolder(f);
		initiateSync(handle);
		emit folderListUpdated(b->walletName());
		return rc;
	}

	return false;
}


bool KWalletD::createFolder(int handle, const QString& f, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		bool rc = b->createFolder(f);
		initiateSync(handle);
		emit folderListUpdated(b->walletName());
		return rc;
	}

	return false;
}


QByteArray KWalletD::readMap(int handle, const QString& folder, const QString& key, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		KWallet::Entry *e = b->readEntry(key);
		if (e && e->type() == KWallet::Wallet::Map) {
			return e->map();
		}
	}

	return QByteArray();
}


QVariantMap KWalletD::readMapList(int handle, const QString& folder, const QString& key, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		QVariantMap rc;
		foreach (KWallet::Entry *entry, b->readEntryList(key)) {
			if (entry->type() == KWallet::Wallet::Map) {
				rc.insert(entry->key(), entry->map());
			}
		}
		return rc;
	}

	return QVariantMap();
}


QByteArray KWalletD::readEntry(int handle, const QString& folder, const QString& key, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		KWallet::Entry *e = b->readEntry(key);
		if (e) {
			return e->value();
		}
	}

	return QByteArray();
}


QVariantMap KWalletD::readEntryList(int handle, const QString& folder, const QString& key, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		QVariantMap rc;
		foreach (KWallet::Entry *entry, b->readEntryList(key)) {
			rc.insert(entry->key(), entry->value());
		}
		return rc;
	}

	return QVariantMap();
}


QStringList KWalletD::entryList(int handle, const QString& folder, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		return b->entryList();
	}

	return QStringList();
}


QString KWalletD::readPassword(int handle, const QString& folder, const QString& key, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		KWallet::Entry *e = b->readEntry(key);
		if (e && e->type() == KWallet::Wallet::Password) {
			return e->password();
		}
	}

	return QString();
}


QVariantMap KWalletD::readPasswordList(int handle, const QString& folder, const QString& key, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		QVariantMap rc;
		foreach (KWallet::Entry *entry, b->readEntryList(key)) {
			if (entry->type() == KWallet::Wallet::Password) {
				rc.insert(entry->key(), entry->password());
			}
		}
		return rc;
	}

	return QVariantMap();
}


int KWalletD::writeMap(int handle, const QString& folder, const QString& key, const QByteArray& value, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		KWallet::Entry e;
		e.setKey(key);
		e.setValue(value);
		e.setType(KWallet::Wallet::Map);
		b->writeEntry(&e);
		initiateSync(handle);
		emitFolderUpdated(b->walletName(), folder);
		return 0;
	}

	return -1;
}


int KWalletD::writeEntry(int handle, const QString& folder, const QString& key, const QByteArray& value, int entryType, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		KWallet::Entry e;
		e.setKey(key);
		e.setValue(value);
		e.setType(KWallet::Wallet::EntryType(entryType));
		b->writeEntry(&e);
		initiateSync(handle);
		emitFolderUpdated(b->walletName(), folder);
		return 0;
	}

	return -1;
}


int KWalletD::writeEntry(int handle, const QString& folder, const QString& key, const QByteArray& value, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		KWallet::Entry e;
		e.setKey(key);
		e.setValue(value);
		e.setType(KWallet::Wallet::Stream);
		b->writeEntry(&e);
		initiateSync(handle);
		emitFolderUpdated(b->walletName(), folder);
		return 0;
	}

	return -1;
}


int KWalletD::writePassword(int handle, const QString& folder, const QString& key, const QString& value, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		KWallet::Entry e;
		e.setKey(key);
		e.setValue(value);
		e.setType(KWallet::Wallet::Password);
		b->writeEntry(&e);
		initiateSync(handle);
		emitFolderUpdated(b->walletName(), folder);
		return 0;
	}

	return -1;
}


int KWalletD::entryType(int handle, const QString& folder, const QString& key, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		if (!b->hasFolder(folder)) {
			return KWallet::Wallet::Unknown;
		}
		b->setFolder(folder);
		if (b->hasEntry(key)) {
			return b->readEntry(key)->type();
		}
	}

	return KWallet::Wallet::Unknown;
}


bool KWalletD::hasEntry(int handle, const QString& folder, const QString& key, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		if (!b->hasFolder(folder)) {
			return false;
		}
		b->setFolder(folder);
		return b->hasEntry(key);
	}

	return false;
}


int KWalletD::removeEntry(int handle, const QString& folder, const QString& key, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		if (!b->hasFolder(folder)) {
			return 0;
		}
		b->setFolder(folder);
		bool rc = b->removeEntry(key);
		initiateSync(handle);
		emitFolderUpdated(b->walletName(), folder);
		return rc ? 0 : -3;
	}

	return -1;
}


void KWalletD::slotServiceOwnerChanged(const QString& name, const QString &oldOwner,
                                       const QString& newOwner)
{
	Q_UNUSED(name);
	kDebug() << "slotServiceOwnerChanged " << name << ", " << oldOwner << ", " << newOwner;

	if (!newOwner.isEmpty()) {
		return; // no application exit, don't care.
	}
	
	// as we don't have the application id we have to cycle
	// all sessions. As an application can basically open wallets
	// with several appids, we can't stop if we found one.
	QString service(oldOwner);
	QList<KWalletAppHandlePair> sessremove(_sessions.findSessions(service));
	KWallet::Backend *b = 0;
	
	// check all sessions for wallets to close
	Q_FOREACH(const KWalletAppHandlePair &s, sessremove) {
		b = getWallet(s.first, s.second);
		if (b) {
			b->deref();
			internalClose(b, s.second, false);
		}
	}

	// remove all the sessions in case they aren't gone yet
	Q_FOREACH(const KWalletAppHandlePair &s, sessremove) {
		_sessions.removeSession(s.first, service, s.second);
	}

	// cancel all open-transactions still running for the service
	QList<KWalletTransaction*>::iterator tit;
	for (tit = _transactions.begin(); tit != _transactions.end(); ++tit) {
		if ((*tit)->tType == KWalletTransaction::Open && (*tit)->service == oldOwner) {
			delete (*tit);
			*tit = 0;
		}
	}
	_transactions.removeAll(0);
		
	// if there's currently an open-transaction being handled,
	// mark it as cancelled.
	if (_curtrans && _curtrans->tType == KWalletTransaction::Open &&
		_curtrans->service == oldOwner) {
        kDebug() << "Cancelling current transaction!";
		_curtrans->cancelled = true;
	}
	_serviceWatcher.removeWatchedService(oldOwner);
}

KWallet::Backend *KWalletD::getWallet(const QString& appid, int handle) {
	if (handle == 0) {
		return 0L;
	}

	KWallet::Backend *w = _wallets.value(handle);

	if (w) { // the handle is valid
		if (_sessions.hasSession(appid, handle)) {
			// the app owns this handle
			_failed = 0;
			if (_closeIdle) {
				_closeTimers.resetTimer(handle, _idleTime);
			}
			return w;
		}
	}

	if (++_failed > 5) {
		_failed = 0;
		QTimer::singleShot(0, this, SLOT(notifyFailures()));
	}

	return 0L;
}


void KWalletD::notifyFailures() {
	if (!_showingFailureNotify) {
		_showingFailureNotify = true;
		KMessageBox::information(0, i18n("There have been repeated failed attempts to gain access to a wallet. An application may be misbehaving."), i18n("KDE Wallet Service"));
		_showingFailureNotify = false;
	}
}


void KWalletD::doCloseSignals(int handle, const QString& wallet) {
	emit walletClosed(handle);
	emit walletClosed(wallet);
	if (_wallets.isEmpty()) {
		emit allWalletsClosed();
	}
}


int KWalletD::renameEntry(int handle, const QString& folder, const QString& oldName, const QString& newName, const QString& appid) {
	KWallet::Backend *b;

	if ((b = getWallet(appid, handle))) {
		b->setFolder(folder);
		int rc = b->renameEntry(oldName, newName);
		initiateSync(handle);
		emitFolderUpdated(b->walletName(), folder);
		return rc;
	}

	return -1;
}


QStringList KWalletD::users(const QString& wallet) const {
	const QPair<int,KWallet::Backend*> walletInfo = findWallet(wallet);
	return _sessions.getApplications(walletInfo.first);
}


bool KWalletD::disconnectApplication(const QString& wallet, const QString& application) {
	const QPair<int, KWallet::Backend*> walletInfo = findWallet(wallet);
	int handle = walletInfo.first;
	KWallet::Backend* backend = walletInfo.second;

	if (handle != -1 && _sessions.hasSession(application, handle)) {
		int removed = _sessions.removeAllSessions(application, handle);

		for (int i = 0; i < removed; ++i) {
			backend->deref();
		}
		internalClose(backend, handle, false);

		emit applicationDisconnected(wallet, application);
		return true;
	}

	return false;
}


void KWalletD::emitFolderUpdated(const QString& wallet, const QString& folder) {
	emit folderUpdated(wallet, folder);
}


void KWalletD::emitWalletListDirty() {
	emit walletListDirty();
}


void KWalletD::reconfigure() {
	KConfig cfg("kwalletrc");
	KConfigGroup walletGroup(&cfg, "Wallet");
	_firstUse = walletGroup.readEntry("First Use", true);
	_enabled = walletGroup.readEntry("Enabled", true);
	_launchManager = walletGroup.readEntry("Launch Manager", true);
	_leaveOpen = walletGroup.readEntry("Leave Open", false);
	bool idleSave = _closeIdle;
	_closeIdle = walletGroup.readEntry("Close When Idle", false);
	_openPrompt = walletGroup.readEntry("Prompt on Open", true);
	int timeSave = _idleTime;
	// in minutes!
	_idleTime = walletGroup.readEntry("Idle Timeout", 10) * 60 * 1000;
#ifdef Q_WS_X11
	if ( screensaver->isValid() ) {
		if (walletGroup.readEntry("Close on Screensaver", false)) {
			connect(screensaver, SIGNAL(ActiveChanged(bool)), SLOT(screenSaverChanged(bool)));
		} else {
			screensaver->disconnect(SIGNAL(ActiveChanged(bool)), this, SLOT(screenSaverChanged(bool)));
		}
	}
#endif
	// Handle idle changes
	if (_closeIdle) {
		if (_idleTime != timeSave) { // Timer length changed
			Wallets::const_iterator it = _wallets.constBegin();
			const Wallets::const_iterator end = _wallets.constEnd();
			for (; it != end; ++it) {
				_closeTimers.resetTimer(it.key(), _idleTime);
			}
		}

		if (!idleSave) { // add timers for all the wallets
			Wallets::const_iterator it = _wallets.constBegin();
			const Wallets::const_iterator end = _wallets.constEnd();
			for (; it != end; ++it) {
				_closeTimers.addTimer(it.key(), _idleTime);
			}
		}
	} else {
		_closeTimers.clear();
	}

	// Update the implicit allow stuff
	_implicitAllowMap.clear();
	const KConfigGroup autoAllowGroup(&cfg, "Auto Allow");
	QStringList entries = autoAllowGroup.entryMap().keys();
	for (QStringList::const_iterator i = entries.constBegin(); i != entries.constEnd(); ++i) {
		_implicitAllowMap[*i] = autoAllowGroup.readEntry(*i, QStringList());
	}

	// Update the implicit allow stuff
	_implicitDenyMap.clear();
	const KConfigGroup autoDenyGroup(&cfg, "Auto Deny");
	entries = autoDenyGroup.entryMap().keys();
	for (QStringList::const_iterator i = entries.constBegin(); i != entries.constEnd(); ++i) {
		_implicitDenyMap[*i] = autoDenyGroup.readEntry(*i, QStringList());
	}

	// Update if wallet was enabled/disabled
	if (!_enabled) { // close all wallets
		while (!_wallets.isEmpty()) {
			Wallets::const_iterator it = _wallets.constBegin();
			internalClose(it.value(), it.key(), true);
		}
		KUniqueApplication::exit(0);
	}
}


bool KWalletD::isEnabled() const {
	return _enabled;
}


bool KWalletD::folderDoesNotExist(const QString& wallet, const QString& folder) {
	if (!wallets().contains(wallet)) {
		return true;
	}

	const QPair<int, KWallet::Backend*> walletInfo = findWallet(wallet);
	if (walletInfo.second) {
		return walletInfo.second->folderDoesNotExist(folder);
	}

	KWallet::Backend *b = new KWallet::Backend(wallet);
	b->open(QByteArray());
	bool rc = b->folderDoesNotExist(folder);
	delete b;
	return rc;
}


bool KWalletD::keyDoesNotExist(const QString& wallet, const QString& folder, const QString& key) {
	if (!wallets().contains(wallet)) {
		return true;
	}

	const QPair<int, KWallet::Backend*> walletInfo = findWallet(wallet);
	if (walletInfo.second) {
		return walletInfo.second->entryDoesNotExist(folder, key);
	}

	KWallet::Backend *b = new KWallet::Backend(wallet);
	b->open(QByteArray());
	bool rc = b->entryDoesNotExist(folder, key);
	delete b;
	return rc;
}


bool KWalletD::implicitAllow(const QString& wallet, const QString& app) {
	return _implicitAllowMap[wallet].contains(app);
}


bool KWalletD::implicitDeny(const QString& wallet, const QString& app) {
	return _implicitDenyMap[wallet].contains(app);
}


void KWalletD::timedOutClose(int id) {
	KWallet::Backend *w = _wallets.value(id);
	if (w) {
		internalClose(w, id, true);
	}
}


void KWalletD::closeAllWallets() {
	Wallets walletsCopy = _wallets;

	Wallets::const_iterator it = walletsCopy.constBegin();
	const Wallets::const_iterator end = walletsCopy.constEnd();
	for (; it != end; ++it) {
		internalClose(it.value(), it.key(), true);
	}

	walletsCopy.clear();

	// All of this should be basically noop.  Let's just be safe.
	_wallets.clear();
}


QString KWalletD::networkWallet() {
	return KWallet::Wallet::NetworkWallet();
}


QString KWalletD::localWallet() {
	return KWallet::Wallet::LocalWallet();
}

void KWalletD::screenSaverChanged(bool s)
{
	if (s)
		closeAllWallets();
}

void KWalletD::activatePasswordDialog()
{
	if (activeDialog) {
		KWindowSystem::forceActiveWindow(activeDialog->winId());
	}
}

int KWalletD::pamOpen(const QString &wallet, const QByteArray &passwordHash, int sessionTimeout)
{
   // don't do anything if transactions are already being processed!
   if (_processing) {
      return -1;
   }
   
   // check if the wallet is already open
   QPair<int, KWallet::Backend*> walletInfo = findWallet(wallet);
   int rc = walletInfo.first;
   if (rc == -1) {
      if (_wallets.count() > 20) {
         kDebug() << "Too many wallets open.";
         return -1;
      }
      
      if (!QRegExp("^[\\w\\^\\&\\'\\@\\{\\}\\[\\]\\,\\$\\=\\!\\-\\#\\(\\)\\%\\.\\+\\_\\s]+$").exactMatch(wallet) ||
          !KWallet::Backend::exists(wallet)) {
         return -1;
      }
      
      KWallet::Backend *b = new KWallet::Backend(wallet);
      int openrc = b->openPreHashed(passwordHash);
      if (openrc == 0 && b->isOpen()) {
         // opening the wallet was successful
         int handle = generateHandle();
         _wallets.insert(handle, b);
         _syncTimers.addTimer(handle, _syncTime);
         
         // don't reference the wallet or add a session so it
         // can be reclosed easily.
         
         if (sessionTimeout > 0) {
            _closeTimers.addTimer(handle, sessionTimeout);
         } else if (_closeIdle) {
            _closeTimers.addTimer(handle, _idleTime);
         }
         emit walletOpened(wallet);
         if (_wallets.count() == 1 && _launchManager) {
            KToolInvocation::startServiceByDesktopName("kwalletmanager-kwalletd");
         }
         return handle;
      }
   }
   
   return -1;
}

#include "kwalletd.moc"
