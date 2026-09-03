/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002-2004 George Staikos <staikos@kde.org>
    SPDX-FileCopyrightText: 2008 Michael Leupold <lemma@confuego.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KSECRETD_H_
#define _KSECRETD_H_

#include "kwalletbackend.h"
#include <QDBusConnection>
#include <QDBusContext>
#include <QFuture>
#include <QHash>
#include <QPointer>
#include <QString>

#include "ktimeout.h"

#include <deque>

class KDirWatch;
class KTimeout;

// @Private
class KWalletTransaction;
class KWalletFreedesktopService;

class KWalletTransaction
{
public:
    enum Type {
        Open,
        ChangePassword,
        OpenFail,
    };

    explicit KWalletTransaction(Type type)
        : tType(type)
    {
    }

    Type tType;
    qlonglong wId;
    QString wallet;
    bool modal;
    QPromise<int> promise;
};

class KSecretD : public QObject, protected QDBusContext
{
    Q_OBJECT

public:
    KSecretD();
    ~KSecretD() override;

    // Is the wallet enabled?  If not, all open() calls fail.
    static bool isEnabled();

    QFuture<int> open(const QString &wallet, qlonglong wId);
    // Close and lock the wallet
    int close(int handle);

public Q_SLOTS:
    // Physically deletes the wallet from disk.
    int deleteWallet(const QString &wallet);

    // Returns true if the wallet is open
    bool isOpen(int handle);

    // Change the password of this wallet
    void changePassword(const QString &wallet, qlonglong wId, const QString &appId);

    // A list of all wallets
    QStringList wallets() const;

    // A list of all folders in this wallet
    QStringList folderList(int handle);

    // Create this folder
    bool createFolder(int handle, const QString &folder);

    // List of entries in this folder
    QStringList entryList(int handle, const QString &folder);

    // Read an entry.  If the entry does not exist, it just
    // returns an empty result.  It is your responsibility to check
    // hasEntry() first.
    QByteArray readEntry(int handle, const QString &folder, const QString &key);
    QByteArray readMap(int handle, const QString &folder, const QString &key);
    QString readPassword(int handle, const QString &folder, const QString &key);

    // Rename an entry.  rc=0 on success.
    int renameEntry(int handle, const QString &folder, const QString &oldName, const QString &newName);
    // Rename the wallet
    int renameWallet(const QString &oldName, const QString &newName);

    // Write an entry.  rc=0 on success.
    int writeEntry(int handle, const QString &folder, const QString &key, const QByteArray &value, int entryType);
    int writeEntry(int handle, const QString &folder, const QString &key, const QByteArray &value);
    int writePassword(int handle, const QString &folder, const QString &key, const QString &value);

    // Does the entry exist?
    bool hasEntry(int handle, const QString &folder, const QString &key);

    // What type is the entry?
    int entryType(int handle, const QString &folder, const QString &key);

    // Remove an entry.  rc=0 on success.
    int removeEntry(int handle, const QString &folder, const QString &key);

    void reconfigure();

    void closeAllWallets();

    // Open a wallet using a pre-hashed password. This is only useful in cooperation
    // with the kwallet PAM module
    int pamOpen(const QString &wallet, const QByteArray &passwordHash);

Q_SIGNALS:
    void walletCreated(const QString &wallet);
    void walletDeleted(const QString &wallet);
    void walletClosed(const QString &wallet); // clazy:exclude=overloaded-signal

    void entryUpdated(const QString &, const QString &, const QString &);
    void entryRenamed(const QString &, const QString &, const QString &, const QString &);
    void entryDeleted(const QString &, const QString &, const QString &);

private Q_SLOTS:
    void emitWalletListDirty();
    void timedOutClose(int handle);
    void timedOutSync(int handle);
    void notifyFailures();
    void processTransactions();
    void activatePasswordDialog();

private:
    // Internal - open a wallet
    int internalOpen(const QString &wallet, WId w, bool modal);
    // Internal - close this wallet.
    int internalClose(KWallet::Backend *const w, const int handle, const bool saveBeforeClose = true);

    QFuture<int> internalChangePassword(const QString &wallet, qlonglong wId);

    // This also validates the handle.  May return NULL.
    KWallet::Backend *getWallet(int handle);
    // Generate a new unique handle.
    int generateHandle();
    // Emit signals about closing wallets
    void emitEntryUpdated(const QString &, const QString &, const QString &);
    void emitEntryRenamed(const QString &, const QString &, const QString &, const QString &);
    void emitEntryDeleted(const QString &, const QString &, const QString &);

    void doTransactionChangePassword(const QString &wallet, qlonglong wId);
    int doTransactionOpen(const QString &wallet, qlonglong wId, bool modal);
    void initiateSync(int handle);

    void setupDialog(QWidget *dialog, WId wId, bool modal);
    void checkActiveDialog();

    QPair<int, KWallet::Backend *> findWallet(const QString &walletName) const;

    typedef QHash<int, KWallet::Backend *> Wallets;
    Wallets _wallets;
    KDirWatch *_dw;
    int _failed;

    // configuration values
    bool _closeIdle, _launchManager;
    bool _firstUse, _showingFailureNotify;
    int _idleTime;
    KTimeout _closeTimers;
    KTimeout _syncTimers;
    const int _syncTime;
    static bool _processing;

    std::deque<KWalletTransaction> _transactions;
    QPointer<QWidget> activeDialog;

    std::unique_ptr<KWalletFreedesktopService> _fdoService;

    bool _useGpg;
};

#endif
