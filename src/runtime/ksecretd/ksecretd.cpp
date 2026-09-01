/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002-2004 George Staikos <staikos@kde.org>
    SPDX-FileCopyrightText: 2008 Michael Leupold <lemma@confuego.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "ksecretd.h"
#include "ksecretd_debug.h"

#include "kwalletfreedesktopcollection.h"
#include "kwalletfreedesktopitem.h"
#include "kwalletfreedesktopprompt.h"
#include "kwalletfreedesktopservice.h"
#include "kwalletfreedesktopsession.h"
#include "kwalletportalsecrets.h"
#include "kwalletsettings.h"
#include "kwalletwizard.h"

#ifdef HAVE_GPGMEPP
#include "knewwalletdialog.h"
#endif

#include <KColorScheme>
#include <KConfig>
#include <KConfigGroup>
#include <KDirWatch>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNewPasswordDialog>
#include <KNotification>
#include <KPasswordDialog>
#include <KPluginFactory>
#include <KSharedConfig>
#include <kwalletentry.h>
#include <kwindowsystem.h>

#include <config-ksecretd.h>
#if WITH_X11
#include <KX11Extras>
#endif

#ifdef HAVE_GPGMEPP
#include <gpgme++/key.h>
#endif

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QTimer>

#include <assert.h>

#include "kwalletcompatadaptor.h"

static void startManagerForKSecretD()
{
    if (!QStandardPaths::findExecutable(QStringLiteral("kstart")).isEmpty()) {
        QProcess::startDetached(QStringLiteral("kstart"), {QStringLiteral("--application"), QStringLiteral("kwalletmanager5-kwalletd")});
    } else {
        QProcess::startDetached(QStringLiteral("kwalletmanager5"), QStringList{QStringLiteral("--kwalletd")});
    }
}

class KWalletTransaction
{
public:
    explicit KWalletTransaction(QDBusConnection conn)
        : tId(nextTransactionId)
        , res(-1)
        , connection(conn)
    {
        nextTransactionId++;
        // make sure the id is never < 0 as that's used for the
        // error conditions.
        if (nextTransactionId < 0) {
            nextTransactionId = 0;
        }
    }

    static int getTransactionId()
    {
        return nextTransactionId;
    }

    ~KWalletTransaction()
    {
    }

    enum Type {
        Unknown,
        Open,
        ChangePassword,
        OpenFail,
    };
    Type tType = Unknown;
    qlonglong wId;
    QString wallet;
    bool modal;
    int tId; // transaction id
    int res;
    QDBusMessage message;
    QDBusConnection connection;

private:
    static int nextTransactionId;
};

int KWalletTransaction::nextTransactionId = 0;

KSecretD::KSecretD()
    : QObject(nullptr)
    , _failed(0)
    , _syncTime(5000)
    , _curtrans(nullptr)
    , _useGpg(false)
{
#ifdef HAVE_GPGMEPP
    _useGpg = true;
#endif

    srand(time(nullptr));
    _showingFailureNotify = false;
    _closeIdle = false;
    _idleTime = 0;
    connect(&_closeTimers, &KTimeout::timedOut, this, &KSecretD::timedOutClose);
    connect(&_syncTimers, &KTimeout::timedOut, this, &KSecretD::timedOutSync);

    KConfig kwalletrc(QStringLiteral("kwalletrc"));
    KConfigGroup cfgWallet(&kwalletrc, "Wallet");

    if (cfgWallet.readEntry<bool>("apiEnabled", true)) {
        (void)new KWalletCompatAdaptor(this);
        // register legacy services
        QDBusConnection::sessionBus().registerObject(QStringLiteral("/ksecretd"), this);
        QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.ksecretd"));
        QDBusConnection::sessionBus().interface()->registerService(QStringLiteral("org.kde.ksecretd"), QDBusConnectionInterface::QueueService);

        new KWalletPortalSecrets(this);
    }

    reconfigure();
    //  KGlobal::dirs()->addResourceType("kwallet", 0, "share/apps/kwallet");
    _dw = new KDirWatch(this);
    _dw->setObjectName(QStringLiteral("KWallet Directory Watcher"));
    //  _dw->addDir(KGlobal::dirs()->saveLocation("kwallet"));
    _dw->addDir(KWallet::Backend::getSaveLocation());

    _dw->startScan(true);
    connect(_dw, &KDirWatch::dirty, this, &KSecretD::emitWalletListDirty);
    connect(_dw, &KDirWatch::deleted, this, &KSecretD::emitWalletListDirty);

    _fdoService.reset(new KWalletFreedesktopService(this));
}

KSecretD::~KSecretD()
{
    closeAllWallets();
    qDeleteAll(_transactions);
}

int KSecretD::generateHandle()
{
    int rc;

    // ASSUMPTION: RAND_MAX is fairly large.
    do {
        rc = rand();
    } while (_wallets.contains(rc) || rc == 0);

    return rc;
}

QPair<int, KWallet::Backend *> KSecretD::findWallet(const QString &walletName) const
{
    Wallets::const_iterator it = _wallets.constBegin();
    const Wallets::const_iterator end = _wallets.constEnd();
    for (; it != end; ++it) {
        if (it.value()->walletName() == walletName) {
            return qMakePair(it.key(), it.value());
        }
    }
    return qMakePair(-1, static_cast<KWallet::Backend *>(nullptr));
}

bool KSecretD::_processing = false;

void KSecretD::processTransactions()
{
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
            res = doTransactionOpen(_curtrans->wallet, _curtrans->wId, _curtrans->modal);

            // multiple requests from the same client
            // should not produce multiple password
            // dialogs on a failure
            if (res < 0) {
                QList<KWalletTransaction *>::iterator it;
                for (it = _transactions.begin(); it != _transactions.end(); ++it) {
                    KWalletTransaction *x = *it;
                    if (x->tType == KWalletTransaction::Open && x->wallet == _curtrans->wallet && x->wId == _curtrans->wId) {
                        x->tType = KWalletTransaction::OpenFail;
                    }
                }
            }

            // emit the AsyncOpened signal as a reply
            _curtrans->res = res;
            Q_EMIT walletAsyncOpened(_curtrans->tId, res);
            break;

        case KWalletTransaction::OpenFail:
            // emit the AsyncOpened signal with an invalid handle
            _curtrans->res = -1;
            Q_EMIT walletAsyncOpened(_curtrans->tId, -1);
            break;

        case KWalletTransaction::ChangePassword:
            doTransactionChangePassword(_curtrans->wallet, _curtrans->wId);
            break;

        case KWalletTransaction::Unknown:
            break;
        default:
            break;
        }

        // send delayed dbus message reply to the caller
        if (_curtrans->message.type() != QDBusMessage::InvalidMessage) {
            if (_curtrans->connection.isConnected()) {
                QDBusMessage reply = _curtrans->message.createReply();
                reply << _curtrans->res;
                _curtrans->connection.send(reply);
            }
        }

        delete _curtrans;
        _curtrans = nullptr;
    }

    _processing = false;
}

int KSecretD::nextTransactionId() const
{
    return KWalletTransaction::getTransactionId();
}

int KSecretD::openAsync(const QString &wallet, qlonglong wId, const QDBusConnection &connection)
{
    if (!isEnabled()) { // guard
        return -1;
    }

    KWalletTransaction *xact = new KWalletTransaction(connection);
    _transactions.append(xact);

    xact->wallet = wallet;
    xact->wId = wId;
    xact->modal = true; // mark dialogs as modal, the app has blocking wait
    xact->tType = KWalletTransaction::Open;

    QTimer::singleShot(0, this, SLOT(processTransactions()));
    checkActiveDialog();
    // opening is in progress. return the transaction number
    return xact->tId;
}

// Sets up a dialog that will be shown by kwallet.
void KSecretD::setupDialog(QWidget *dialog, WId wId, bool modal)
{
    if (wId != 0) {
        // correct, set dialog parent
        dialog->setAttribute(Qt::WA_NativeWindow, true);
        KWindowSystem::setMainWindow(dialog->windowHandle(), wId);
    } else {
        qWarning() << "Using kwallet without parent window!";
        // allow dialog activation even if it interrupts, better than trying
        // hacks
        // with keeping the dialog on top or on all desktops
        // KF5 FIXME what should we use now instead of this:
        //         kapp->updateUserTimestamp();
    }

#if WITH_X11
    if (KWindowSystem::isPlatformX11()) {
        if (modal) {
            KX11Extras::setState(dialog->winId(), NET::Modal);
        } else {
            KX11Extras::clearState(dialog->winId(), NET::Modal);
        }
    }
#endif
    activeDialog = dialog;
}

// If there's a dialog already open and another application tries some
// operation that'd lead to
// opening a dialog, that application will be blocked by this dialog. A proper
// solution would
// be to set the second application's window also as a parent for the active
// dialog, so that
// KWin properly handles focus changes and so on, but there's currently no
// support for multiple
// dialog parents. In the absence of this support, we use all kinds of bad
// hacks to make
// sure the user doesn't overlook the active dialog.
void KSecretD::checkActiveDialog()
{
    if (!activeDialog) {
        return;
    }

    // KF5 FIXME what should we use now instead of this:
    //  kapp->updateUserTimestamp();

    activeDialog->show();

#if WITH_X11
    if (KWindowSystem::isPlatformX11()) {
        WId window = activeDialog->winId();
        KX11Extras::setState(window, NET::KeepAbove);
        KX11Extras::setOnAllDesktops(window, true);
        KX11Extras::forceActiveWindow(window);
    }
#endif
}

int KSecretD::doTransactionOpen(const QString &wallet, qlonglong wId, bool modal)
{
    if (_firstUse) {
        // if the user specifies a wallet name, the use it as the default
        // wallet name
        if (wallet != KWallet::Backend::localWallet()) {
            KWalletSettings settings;
            settings.setDefaultWallet(wallet);
            settings.save();
        }
        if (wallets().contains(KWallet::Backend::localWallet())) {
            KWalletSettings settings;
            _firstUse = false;
            settings.setFirstUse(false);
            settings.save();
        }
    }

    int rc = internalOpen(wallet, WId(wId), modal);
    return rc;
}

int KSecretD::internalOpen(const QString &wallet, WId w, bool modal)
{
    bool brandNew = false;

    QString thisApp = QStringLiteral("KDE System");

    QPair<int, KWallet::Backend *> walletInfo = findWallet(wallet);
    int rc = walletInfo.first;
    if (rc == -1) {
        if (_wallets.count() > 20) {
            qCDebug(KSECRETD_LOG) << "Too many wallets open.";
            return -1;
        }

        KWallet::Backend *b = new KWallet::Backend(wallet);
        QString password;
        bool emptyPass = false;
        if (KWallet::Backend::exists(wallet)) {
            // this open attempt will set wallet type from the file header,
            // even if password is needed
            int pwless = b->open(QByteArray(), w);
#ifdef HAVE_GPGMEPP
            assert(b->cipherType() != KWallet::BACKEND_CIPHER_UNKNOWN);
            if (b->cipherType() == KWallet::BACKEND_CIPHER_GPG) {
                // GPG based wallets do not prompt for password here. Instead,
                // GPG should already have popped pinentry utility for wallet
                // decryption
                if (!b->isOpen()) {
                    // for some reason, GPG operation failed
                    delete b;
                    return -1;
                }
                emptyPass = true;
            } else {
#endif
                if (0 != pwless || !b->isOpen()) {
                    if (pwless == 0) {
                        // release, start anew
                        delete b;
                        b = new KWallet::Backend(wallet);
                    }
                    KPasswordDialog *kpd = new KPasswordDialog();
                    kpd->setPrompt(i18n("<qt>KDE has requested to open the wallet '<b>%1</b>'. Please enter the password for this wallet below.</qt>",
                                        wallet.toHtmlEscaped()));
                    brandNew = false;
                    // don't use KStdGuiItem::open() here which has trailing
                    // ellipsis!
                    // KF5 FIXME what should we use now instead of this:
                    //              kpd->setButtonGuiItem(KDialog::Ok,KGuiItem(
                    //              i18n( "&Open" ), "wallet-open"));
                    kpd->setWindowTitle(i18n("KDE Wallet Service"));
                    kpd->setIcon(QIcon::fromTheme(QStringLiteral("kwalletmanager")));

#if WITH_X11
                    if (KWindowSystem::isPlatformX11() && w != KX11Extras::activeWindow() && w != 0L) {
                        // If the dialog is modal to a minimized window it
                        // might not be visible
                        // (but still blocking the calling application).
                        // Notify the user about
                        // the request to open the wallet.
                        KNotification *notification =
                            new KNotification(QStringLiteral("needsPassword"), KNotification::Persistent | KNotification::CloseWhenWindowActivated);
                        notification->setWindow(kpd->windowHandle());
                        QString actionText;
                        notification->setText(i18n("An application has requested to open a wallet (%1).", wallet.toHtmlEscaped()));
                        actionText = i18nc("Text of a button for switching to the (unnamed) application requesting a password", "Switch there");

                        KNotificationAction *action = notification->addAction(actionText);
                        connect(action, &KNotificationAction::activated, this, &KSecretD::activatePasswordDialog);
                        notification->sendEvent();
                    }
#endif

                    while (!b->isOpen()) {
                        setupDialog(kpd, w, modal);
                        if (kpd->exec() == QDialog::Accepted) {
                            password = kpd->password();
                            int rc = b->open(password.toUtf8());
                            if (!b->isOpen()) {
                                const auto errorStr = KWallet::Backend::openRCToString(rc);
                                qCWarning(KSECRETD_LOG) << "Failed to open wallet" << wallet << errorStr;
                                kpd->setPrompt(i18n("<qt>Error opening the wallet '<b>%1</b>'. Please try again.<br />(Error code %2: %3)</qt>",
                                                    wallet.toHtmlEscaped(),
                                                    rc,
                                                    errorStr));
                                kpd->setPassword(QLatin1String(""));
                            }
                        } else {
                            break;
                        }
                    }
                    delete kpd;
                } else {
                    emptyPass = true;
                }
#ifdef HAVE_GPGMEPP
            }
#endif
        } else {
            brandNew = true;
#ifdef HAVE_GPGMEPP
            // prompt the user for the new wallet format here
            KWallet::BackendCipherType newWalletType = KWallet::BACKEND_CIPHER_UNKNOWN;

            std::shared_ptr<KWallet::KNewWalletDialog> newWalletDlg(new KWallet::KNewWalletDialog(wallet, QWidget::find(w)));
            GpgME::Key gpgKey;
            setupDialog(newWalletDlg.get(), (WId)w, true);
            if (newWalletDlg->exec() == QDialog::Accepted) {
                newWalletType = newWalletDlg->isBlowfish() ? KWallet::BACKEND_CIPHER_BLOWFISH : KWallet::BACKEND_CIPHER_GPG;
                gpgKey = newWalletDlg->gpgKey();
            } else {
                // user cancelled the dialog box
                delete b;
                return -1;
            }

            if (newWalletType == KWallet::BACKEND_CIPHER_GPG) {
                b->setCipherType(newWalletType);
                b->open(gpgKey);
            } else if (newWalletType == KWallet::BACKEND_CIPHER_BLOWFISH) {
#endif // HAVE_GPGMEPP
                b->setCipherType(KWallet::BACKEND_CIPHER_BLOWFISH);
                KNewPasswordDialog *kpd = new KNewPasswordDialog();
                KColorScheme colorScheme(QPalette::Active, KColorScheme::View);
                kpd->setBackgroundWarningColor(colorScheme.background(KColorScheme::NegativeBackground).color());
                if (wallet == KWallet::Backend::localWallet() || wallet == KWallet::Backend::networkWallet()) {
                    // Auto create these wallets.
                    kpd->setPrompt(
                        i18n("KDE has requested to open the wallet. This is used to store sensitive data in a "
                             "secure fashion. Please enter a password to use with this wallet or click cancel to "
                             "deny the application's request."));
                } else {
                    kpd->setPrompt(
                        i18n("<qt>KDE has requested to create a new wallet named '<b>%1</b>'. Please choose a "
                             "password for this wallet, or cancel to deny the application's request.</qt>",
                             wallet.toHtmlEscaped()));
                }
                kpd->setWindowTitle(i18n("KDE Wallet Service"));
                // KF5 FIXME what should we use now instead of this:
                //              kpd->setButtonGuiItem(KDialog::Ok,KGuiItem(i18n("C&reate"),"document-new"));
                kpd->setIcon(QIcon::fromTheme(QStringLiteral("kwalletmanager")));
                while (!b->isOpen()) {
                    setupDialog(kpd, w, modal);
                    if (kpd->exec() == QDialog::Accepted) {
                        password = kpd->password();
                        int rc = b->open(password.toUtf8());
                        if (!b->isOpen()) {
                            kpd->setPrompt(i18n("<qt>Error opening the wallet '<b>%1</b>'. Please try again.<br />(Error code %2: %3)</qt>",
                                                wallet.toHtmlEscaped(),
                                                rc,
                                                KWallet::Backend::openRCToString(rc)));
                        }
                    } else {
                        break;
                    }
                }
                delete kpd;
#ifdef HAVE_GPGMEPP
            }
#endif
        }

        if ((b->cipherType() == KWallet::BACKEND_CIPHER_BLOWFISH) && !emptyPass && (password.isNull() || !b->isOpen())) {
            delete b;
            return -1;
        }

        _wallets.insert(rc = generateHandle(), b);
        _syncTimers.addTimer(rc, _syncTime);

        if (brandNew) {
            createFolder(rc, KWallet::Wallet::PasswordFolder());
            createFolder(rc, KWallet::Wallet::FormDataFolder());
        }

        b->ref();
        if (_closeIdle) {
            _closeTimers.addTimer(rc, _idleTime);
        }
        if (brandNew) {
            Q_EMIT walletCreated(wallet);
        }
        if (_wallets.count() == 1 && _launchManager) {
            startManagerForKSecretD();
        }
    } else {
        // prematurely add a reference so that the wallet does not close while
        // the
        // authorization dialog is being shown.
        walletInfo.second->ref();
        // as the wallet might have been forcefully closed, find it again to
        // make sure it's
        // still available (isAuthorizedApp might show a dialog).
        walletInfo = findWallet(wallet);
        if (walletInfo.first == -1) {
            // wallet was forcefully closed.
            return -1;
        }
    }

    return rc;
}

int KSecretD::deleteWallet(const QString &wallet)
{
    int result = -1;
    QString path = KWallet::Backend::getSaveLocation() + "/" + KWallet::Backend::encodeWalletName(wallet) + ".kwl";
    QString pathSalt = KWallet::Backend::getSaveLocation() + "/" + KWallet::Backend::encodeWalletName(wallet) + ".salt";

    if (QFile::exists(path)) {
        const QPair<int, KWallet::Backend *> walletInfo = findWallet(wallet);
        internalClose(walletInfo.second, walletInfo.first);
        QFile::remove(path);
        Q_EMIT walletDeleted(wallet);

        if (QFile::exists(pathSalt)) {
            QFile::remove(pathSalt);
        }

        result = 0;
    }

    return result;
}

void KSecretD::changePassword(const QString &wallet, qlonglong wId, const QString &appId)
{
    Q_UNUSED(appId);

    KWalletTransaction *xact = new KWalletTransaction(connection());

    message().setDelayedReply(true);
    xact->message = message();
    // TODO GPG this shouldn't be allowed on a GPG managed wallet; a warning
    // should be displayed about this

    xact->wallet = wallet;
    xact->wId = wId;
    xact->modal = false;
    xact->tType = KWalletTransaction::ChangePassword;

    _transactions.append(xact);

    QTimer::singleShot(0, this, SLOT(processTransactions()));
    checkActiveDialog();
    checkActiveDialog();
}

void KSecretD::initiateSync(int handle)
{
    // add a timer and reset it right away
    _syncTimers.addTimer(handle, _syncTime);
    _syncTimers.resetTimer(handle, _syncTime);
}

void KSecretD::doTransactionChangePassword(const QString &wallet, qlonglong wId)
{
    const QPair<int, KWallet::Backend *> walletInfo = findWallet(wallet);
    int handle = walletInfo.first;
    KWallet::Backend *w = walletInfo.second;

    bool reclose = false;
    if (!w) {
        handle = doTransactionOpen(wallet, wId, false);
        if (-1 == handle) {
            KMessageBox::errorWId((WId)wId,
                                  i18n("Unable to open wallet. The wallet must be opened in order to change the password."),
                                  i18n("KDE Wallet Service"));
            return;
        }

        w = _wallets.value(handle);
        reclose = true;
    }

    assert(w);

#ifdef HAVE_GPGMEPP
    if (w->cipherType() == KWallet::BACKEND_CIPHER_GPG) {
        QString keyID = w->gpgKey().shortKeyID();
        assert(!keyID.isNull());
        KMessageBox::errorWId((WId)wId,
                              i18n("<qt>The <b>%1</b> wallet is encrypted using GPG key <b>%2</b>. Please use <b>GPG</b> tools (such "
                                   "as <b>kleopatra</b>) to change the passphrase associated to that key.</qt>",
                                   wallet.toHtmlEscaped(),
                                   keyID));
    } else {
#endif
        QPointer<KNewPasswordDialog> kpd = new KNewPasswordDialog();
        kpd->setPrompt(i18n("<qt>Please choose a new password for the wallet '<b>%1</b>'.</qt>", wallet.toHtmlEscaped()));
        kpd->setWindowTitle(i18n("KDE Wallet Service"));
        kpd->setAllowEmptyPasswords(true);
        KColorScheme colorScheme(QPalette::Active, KColorScheme::View);
        kpd->setBackgroundWarningColor(colorScheme.background(KColorScheme::NegativeBackground).color());
        setupDialog(kpd, (WId)wId, false);
        if (kpd->exec() == QDialog::Accepted && kpd) {
            QString p = kpd->password();
            if (!p.isNull()) {
                w->setPassword(p.toUtf8());
                int rc = w->close(true);
                if (rc < 0) {
                    KMessageBox::errorWId((WId)wId, i18n("Error re-encrypting the wallet. Password was not changed."), i18n("KDE Wallet Service"));
                    reclose = true;
                } else {
                    rc = w->open(p.toUtf8());
                    if (rc < 0) {
                        KMessageBox::errorWId((WId)wId, i18n("Error reopening the wallet. Data may be lost."), i18n("KDE Wallet Service"));
                        reclose = true;
                    }
                }
            }
        }

        delete kpd;
#ifdef HAVE_GPGMEPP
    }
#endif

    if (reclose) {
        internalClose(w, handle);
    }
}

int KSecretD::internalClose(KWallet::Backend *const w, const int handle, const bool saveBeforeClose)
{
    if (w) {
        const QString &wallet = w->walletName();
        if (_closeIdle) {
            _closeTimers.removeTimer(handle);
        }
        _syncTimers.removeTimer(handle);
        _wallets.remove(handle);
        w->close(saveBeforeClose);
        _fdoService->lockCollection(wallet);
        delete w;
        return 0;
    }

    return -1;
}

int KSecretD::close(int handle)
{
    KWallet::Backend *w = _wallets.value(handle);

    if (w) {
        w->deref();
        return internalClose(w, handle);
    }
    return -1; // not open to begin with, or other error
}

bool KSecretD::isOpen(int handle)
{
    if (handle == 0) {
        return false;
    }

    KWallet::Backend *rc = _wallets.value(handle);

    if (rc == nullptr && ++_failed > 5) {
        _failed = 0;
        QTimer::singleShot(0, this, SLOT(notifyFailures()));
    } else if (rc != nullptr) {
        _failed = 0;
    }

    return rc != nullptr;
}

QStringList KSecretD::wallets() const
{
    QString path = KWallet::Backend::getSaveLocation();
    QDir dir(path, QStringLiteral("*.kwl"));
    QStringList rc;

    dir.setFilter(QDir::Files | QDir::Hidden);

    const auto list = dir.entryInfoList();
    for (const QFileInfo &fi : list) {
        QString fn = fi.fileName();
        if (fn.endsWith(QLatin1String(".kwl"))) {
            fn.truncate(fn.length() - 4);
        }
        rc += KWallet::Backend::decodeWalletName(fn);
    }
    return rc;
}

void KSecretD::timedOutSync(int handle)
{
    _syncTimers.removeTimer(handle);
    if (_wallets.contains(handle) && _wallets[handle]) {
        _wallets[handle]->sync();
    } else {
        qDebug("wallet not found for sync!");
    }
}

QStringList KSecretD::folderList(int handle)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        return b->folderList();
    }

    return QStringList();
}

bool KSecretD::createFolder(int handle, const QString &f)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        bool rc = b->createFolder(f);
        initiateSync(handle);
        return rc;
    }

    return false;
}

QByteArray KSecretD::readMap(int handle, const QString &folder, const QString &key)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        b->setFolder(folder);
        KWallet::Entry *e = b->readEntry(key);
        if (e && e->type() == KWallet::Wallet::Map) {
            return e->map();
        }
    }

    return QByteArray();
}

QByteArray KSecretD::readEntry(int handle, const QString &folder, const QString &key)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        b->setFolder(folder);
        KWallet::Entry *e = b->readEntry(key);
        if (e) {
            return e->value();
        }
    }

    return QByteArray();
}

QStringList KSecretD::entryList(int handle, const QString &folder)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        b->setFolder(folder);
        return b->entryList();
    }

    return QStringList();
}

QString KSecretD::readPassword(int handle, const QString &folder, const QString &key)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        b->setFolder(folder);
        KWallet::Entry *e = b->readEntry(key);
        if (e && e->type() == KWallet::Wallet::Password) {
            return e->password();
        }
    }

    return QString();
}

int KSecretD::writeEntry(int handle, const QString &folder, const QString &key, const QByteArray &value)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        b->setFolder(folder);
        KWallet::Entry e;
        e.setKey(key);
        e.setValue(value);
        e.setType(KWallet::Wallet::Stream);
        b->writeEntry(&e);
        initiateSync(handle);
        emitEntryUpdated(b->walletName(), folder, key);
        return 0;
    }

    return -1;
}

int KSecretD::writeEntry(int handle, const QString &folder, const QString &key, const QByteArray &value, int entryType)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        b->setFolder(folder);
        KWallet::Entry e;
        e.setKey(key);
        e.setValue(value);
        e.setType(KWallet::Wallet::EntryType(entryType));
        b->writeEntry(&e);
        initiateSync(handle);
        return 0;
    }

    return -1;
}

int KSecretD::writePassword(int handle, const QString &folder, const QString &key, const QString &value)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        b->setFolder(folder);
        KWallet::Entry e;
        e.setKey(key);
        e.setValue(value);
        e.setType(KWallet::Wallet::Password);
        b->writeEntry(&e);
        initiateSync(handle);
        emitEntryUpdated(b->walletName(), folder, key);
        return 0;
    }

    return -1;
}

int KSecretD::entryType(int handle, const QString &folder, const QString &key)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
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

bool KSecretD::hasEntry(int handle, const QString &folder, const QString &key)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        if (!b->hasFolder(folder)) {
            return false;
        }
        b->setFolder(folder);
        return b->hasEntry(key);
    }

    return false;
}

int KSecretD::removeEntry(int handle, const QString &folder, const QString &key)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        if (!b->hasFolder(folder)) {
            return 0;
        }
        b->setFolder(folder);
        bool rc = b->removeEntry(key);
        initiateSync(handle);
        emitEntryDeleted(b->walletName(), folder, key);
        return rc ? 0 : -3;
    }

    return -1;
}

KWallet::Backend *KSecretD::getWallet(int handle)
{
    if (handle == 0) {
        return nullptr;
    }

    KWallet::Backend *w = _wallets.value(handle);

    if (w) { // the handle is valid
        _failed = 0;
        if (_closeIdle) {
            _closeTimers.resetTimer(handle, _idleTime);
        }
        return w;
    }

    if (++_failed > 5) {
        _failed = 0;
        QTimer::singleShot(0, this, SLOT(notifyFailures()));
    }

    return nullptr;
}

void KSecretD::notifyFailures()
{
    if (!_showingFailureNotify) {
        _showingFailureNotify = true;
        KMessageBox::information(nullptr,
                                 i18n("There have been repeated failed attempts to gain access to a wallet. An application may be misbehaving."),
                                 i18n("KDE Wallet Service"));
        _showingFailureNotify = false;
    }
}

int KSecretD::renameEntry(int handle, const QString &folder, const QString &oldName, const QString &newName)
{
    KWallet::Backend *b;

    if ((b = getWallet(handle))) {
        b->setFolder(folder);
        int rc = b->renameEntry(oldName, newName);
        initiateSync(handle);
        emitEntryRenamed(b->walletName(), folder, oldName, newName);
        return rc;
    }

    return -1;
}

int KSecretD::renameWallet(const QString &oldName, const QString &newName)
{
    const QPair<int, KWallet::Backend *> walletInfo = findWallet(oldName);
    return walletInfo.second->renameWallet(newName);
}

void KSecretD::emitEntryUpdated(const QString &wallet, const QString &folder, const QString &key)
{
    Q_EMIT entryUpdated(wallet, folder, key);
}

void KSecretD::emitEntryRenamed(const QString &wallet, const QString &folder, const QString &oldName, const QString &newName)
{
    Q_EMIT entryRenamed(wallet, folder, oldName, newName);
}

void KSecretD::emitEntryDeleted(const QString &wallet, const QString &folder, const QString &key)
{
    Q_EMIT entryDeleted(wallet, folder, key);
}

void KSecretD::emitWalletListDirty()
{
    const QStringList walletsInDisk = wallets();
    const auto lst = _wallets.values();
    for (auto i : lst) {
        if (!walletsInDisk.contains(i->walletName())) {
            internalClose(i, _wallets.key(i), false);
        }
    }
}

void KSecretD::reconfigure()
{
    KWalletSettings settings;
    _firstUse = settings.firstUse();
    _launchManager = settings.launchManager();
    bool idleSave = _closeIdle;
    _closeIdle = settings.closeWhenIdle();
    int timeSave = _idleTime;
    // in minutes!
    _idleTime = settings.idleTimeout() * 60 * 1000;
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

    // Update if wallet was enabled/disabled
    if (!isEnabled()) { // close all wallets
        while (!_wallets.isEmpty()) {
            Wallets::const_iterator it = _wallets.constBegin();
            internalClose(it.value(), it.key());
        }
        QApplication::exit(0);
    }
}

bool KSecretD::isEnabled()
{
    KWalletSettings settings;

    // For KSecredD to be enabled, it needs both global kwallet enabled and ksecretd enabled
    // values, as it does not make sense without kwallet, but is possible kwallet without
    // ksecretd
    return settings.kSecretDEnabled() && settings.kWalletDEnabled();
}

void KSecretD::timedOutClose(int id)
{
    KWallet::Backend *w = _wallets.value(id);
    if (w) {
        internalClose(w, id);
    }
}

void KSecretD::closeAllWallets()
{
    Wallets walletsCopy = _wallets;

    Wallets::const_iterator it = walletsCopy.constBegin();
    const Wallets::const_iterator end = walletsCopy.constEnd();
    for (; it != end; ++it) {
        internalClose(it.value(), it.key());
    }

    walletsCopy.clear();

    // All of this should be basically noop.  Let's just be safe.
    _wallets.clear();
}

void KSecretD::activatePasswordDialog()
{
    checkActiveDialog();
}

int KSecretD::pamOpen(const QString &wallet, const QByteArray &passwordHash)
{
    if (_processing) {
        return -1;
    }

    bool brandNew = false;

    // check if the wallet is already open
    QPair<int, KWallet::Backend *> walletInfo = findWallet(wallet);
    int rc = walletInfo.first;
    if (rc != -1) {
        return rc; // Wallet already opened, return handle
    }
    if (_wallets.count() > 20) {
        return -1;
    }

    KWallet::Backend *b = nullptr;
    // If the wallet we want to open does not exists. create it and set pam
    // hash
    if (!wallets().contains(wallet)) {
        b = new KWallet::Backend(wallet);
        b->setCipherType(KWallet::BACKEND_CIPHER_BLOWFISH);
        brandNew = true;
    } else {
        b = new KWallet::Backend(wallet);
    }

    int openrc = b->openPreHashed(passwordHash);
    if (openrc != 0 || !b->isOpen()) {
        delete b;
        return openrc;
    }

    // opening the wallet was successful
    int handle = generateHandle();
    _wallets.insert(handle, b);
    _syncTimers.addTimer(handle, _syncTime);

    // don't reference the wallet or add a session so it
    // can be reclosed easily.

    if (_closeIdle) {
        _closeTimers.addTimer(handle, _idleTime);
    }
    if (brandNew) {
        Q_EMIT walletCreated(wallet);
    }

    auto collection = _fdoService->getCollectionByWalletName(wallet);
    collection->onWalletChangeState(handle);

    if (_wallets.count() == 1 && _launchManager) {
        startManagerForKSecretD();
    }

    return handle;
}

// vim: tw=220:ts=4

#include "moc_ksecretd.cpp"
