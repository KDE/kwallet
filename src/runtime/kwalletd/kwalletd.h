/*
    SPDX-FileCopyrightText: 2024 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QDBusContext>
#include <QObject>

#include <KConfigWatcher>

#include "kwalletd_debug.h"

struct Folder {
    QString name;
    QStringList passwords;
    QStringList maps;
};

struct Wallet {
    QString name;
    QList<Folder> folders;
};

class SecretServiceClient;

class KWalletD : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    // Enum names and values from KWallet
    enum EntryType {
        Unknown = 0,
        Password,
        Stream,
        Map,
        Unused = 0xffff,
    };
    Q_ENUM(EntryType)
    KWalletD(bool useKWalletBackend, QObject *parent = nullptr);

    ~KWalletD() override;

protected:
    // Migrate a single wallet, returns true on success
    // sourceWallet is the wallet name on kwallet backend
    // destWallet is the collection name on secretservice
    bool migrateWallet(const QString &sourceWallet, const QString &destWallet);
    // Migrate all wallets
    void migrateData();
    // from an handle int representing a session to the open wallet name of that session
    QString walletForHandle(int handle, const QString &appId);
    // A folder as user readable name in the keychain in the format wallet/folder
    QString folderPath(const QString &folder, const QString &key) const;
    // Type of a key in a wallet and folder
    EntryType keyType(const QString &wallet, const QString &folder, const QString &key);

    QString readString(const QString &key, const QString &folder, const QString &wallet, bool *ok);
    QByteArray readRawJson(const QString &key, const QString &folder, const QString &wallet, bool *ok);
    QByteArray readBinary(const QString &key, const QString &folder, const QString &wallet, bool *ok);
    void writeString(const QString &key, const QString &value, const QString &folder, const QString &wallet, bool *ok);
    void writeBinary(const QString &key, const QByteArray &value, const QString &folder, const QString &wallet, bool *ok);
    void writeRawJson(const QString &key, const QByteArray &value, const QString &folder, const QString &wallet, bool *ok);
    void removeItem(const QString &key, const QString &folder, const QString &wallet, bool *ok);

    void timerEvent(QTimerEvent *) override;

public Q_SLOTS:
    // Is the wallet enabled?  If not, all open() calls fail.
    bool isEnabled() const;
    // Open and unlock the wallet
    int open(const QString &wallet, qlonglong wId, const QString &appId);
    // STUB: Open and unlock the wallet with this path
    int openPath(const QString &path, qlonglong wId, const QString &appId);
    // Open the wallet asynchronously
    int openAsync(const QString &wallet, qlonglong wId, const QString &appId, bool handleSession);
    // STUB: Open and unlock the wallet with this path asynchronously
    int openPathAsync(const QString &path, qlonglong wId, const QString &appId, bool handleSession);

    // Close and lock the wallet
    // If force = true, will close it for all users.  Behave.  This
    // can break applications, and is generally intended for use by
    // the wallet manager app only.
    int close(const QString &wallet, bool force);
    int close(int handle, bool force, const QString &appId);

    // STUB: Save to disk but leave open
    Q_NOREPLY void sync(int handle, const QString &appId);

    // Physically deletes the wallet from disk.
    int deleteWallet(const QString &wallet);

    // Returns true if the wallet is open
    bool isOpen(const QString &wallet);
    bool isOpen(int handle);

    // List the users of this wallet
    QStringList users(const QString &wallet) const;

    // STUB: Change the password of this wallet
    void changePassword(const QString &wallet, qlonglong wId, const QString &appId);

    // A list of all wallets
    QStringList wallets() const;

    // A list of all folders in this wallet
    QStringList folderList(int handle, const QString &appId);
    // Does this wallet have this folder?
    bool hasFolder(int handle, const QString &folder, const QString &appId);

    // Create this folder
    bool createFolder(int handle, const QString &folder, const QString &appId);

    // Remove this folder
    bool removeFolder(int handle, const QString &folder, const QString &appId);

    // List of entries in this folder
    QStringList entryList(int handle, const QString &folder, const QString &appId);

    // Read an entry.  If the entry does not exist, it just
    // returns an empty result.  It is your responsibility to check
    // hasEntry() first.
    QByteArray readEntry(int handle, const QString &folder, const QString &key, const QString &appId);
    QByteArray readMap(int handle, const QString &folder, const QString &key, const QString &appId);
    QString readPassword(int handle, const QString &folder, const QString &key, const QString &appId);

    // Deprecated, use entriesList()
    QVariantMap readEntryList(int handle, const QString &folder, const QString &key, const QString &appId);
    // returns in a variantmap every key/value of a folder
    QVariantMap entriesList(int handle, const QString &folder, const QString &appId);
    // Deprecated, use mapList()
    QVariantMap readMapList(int handle, const QString &folder, const QString &key, const QString &appId);
    // Keys of the QVariantMap are the keys in the folder, values are the binary serialized maps of a key
    QVariantMap mapList(int handle, const QString &folder, const QString &appId);
    // use passwordList()
    QVariantMap readPasswordList(int handle, const QString &folder, const QString &key, const QString &appId);
    // Key = entry name in the folder Value = password cleartext
    QVariantMap passwordList(int handle, const QString &folder, const QString &appId);

    // Rename an entry.  rc=0 on success.
    int renameEntry(int handle, const QString &folder, const QString &oldName, const QString &newName, const QString &appId);

    // Write an entry.  rc=0 on success.
    int writeEntry(int handle, const QString &folder, const QString &key, const QByteArray &value, int entryType, const QString &appId);
    int writeEntry(int handle, const QString &folder, const QString &key, const QByteArray &value, const QString &appId);
    int writeMap(int handle, const QString &folder, const QString &key, const QByteArray &value, const QString &appId);
    int writePassword(int handle, const QString &folder, const QString &key, const QString &value, const QString &appId);

    // Does the entry exist?
    bool hasEntry(int handle, const QString &folder, const QString &key, const QString &appId);
    // What type is the entry?
    int entryType(int handle, const QString &folder, const QString &key, const QString &appId);
    // Remove an entry.  rc=0 on success.
    int removeEntry(int handle, const QString &folder, const QString &key, const QString &appId);

    // Disconnect an app from a wallet
    bool disconnectApplication(const QString &wallet, const QString &application);

    void reconfigure();

    // Determine
    bool folderDoesNotExist(const QString &wallet, const QString &folder);
    bool keyDoesNotExist(const QString &wallet, const QString &folder, const QString &key);

    // Everybody will need to call open() again
    void closeAllWallets();

    QString networkWallet();
    QString localWallet();

    // STUB: Open a wallet using a pre-hashed password. This is only useful in cooperation
    // with the kwallet PAM module. It's also less secure than manually entering the
    // password as the password hash is transmitted using D-Bus.
    int pamOpen(const QString &wallet, const QByteArray &passwordHash, int sessionTimeout);

Q_SIGNALS:
    void error(const QString &message);

    void walletAsyncOpened(int id, int handle); // used to notify KWallet::Wallet
    void walletListDirty();
    void walletCreated(const QString &wallet);
    void walletOpened(const QString &wallet);
    void walletDeleted(const QString &wallet);
    void walletClosed(const QString &wallet); // clazy:exclude=overloaded-signal

    void walletClosed(int handle); // clazy:exclude=overloaded-signal
    // since 5.81
    void walletClosedId(int handle);

    void allWalletsClosed();
    void folderListUpdated(const QString &wallet);
    void folderUpdated(const QString &wallet, const QString &folder);
    void entryUpdated(const QString &wallet, const QString &folder, const QString &key);
    void entryRenamed(const QString &wallet, const QString &folder, const QString &oldName, const QString &newName);
    void entryDeleted(const QString &wallet, const QString &folder, const QString &key);
    void applicationDisconnected(const QString &wallet, const QString &application);

private:
    SecretServiceClient *m_backend;
    // We need to store a structure here as well, because the api has createFolder that would make a folder without any keys
    QMultiHash<QString, QString> m_structure;
    QHash<QPair<int, QString>, QString> m_openWallets;
    QHash<QPair<int, QString>, int> m_idleTimers;

    bool m_enabled = true;
    bool m_launchManager = false;
    bool m_closeIdle = false;
    bool m_useKWalletBackend = true;
    // in minutes
    int m_idleTime = 10 * 60 * 1000;
    KConfigWatcher::Ptr m_configWatcher;

    static unsigned int s_lastTransaction;
};
