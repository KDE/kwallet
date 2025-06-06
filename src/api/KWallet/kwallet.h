/*
    This file is part of the KDE project

    SPDX-FileCopyrightText: 2002-2004 George Staikos <staikos@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KWALLET_H
#define _KWALLET_H

#include <QObject>
#include <QStringList>
#include <qwindowdefs.h> // krazy:exclude=includes (for WId)

#include <kwallet_export.h>

/**
 * NOTE: KSecretsService folder semantics
 * The KWallet API uses folders for organising items. KSecretsService does not
 * have this notion. But it uses attributes that can be applied arbitrarily on
 * all the items. The KWallet code that maps to KSecretsService applies an special
 * attribute KSS_ATTR_ENTRYFOLDER to all items with the currentFolder() value.
 * The KWallet folder API's calls will always succeed and they'll only change the
 * current folder value. The folderList() call will scan all the collection
 * items and collect the KSS_ATTR_ENTRYFOLDER attributes into a list.
 */

/**
 * NOTE: KWalet API distinguish KSecretsService collection items by attaching
 * them some specific attributes, defined below
 */
#define KSS_ATTR_ENTRYFOLDER "kwallet.folderName"
#define KSS_ATTR_WALLETTYPE "kwallet.type"

/*!
 * \namespace KWallet
 * \inmodule KWallet
 */
namespace KWallet
{
/*!
 * \class KWallet::Wallet
 * \inheaderfile KWallet
 * \inmodule KWallet
 *
 * \brief KDE Wallet Class.
 *
 * This class implements a generic system-wide Wallet for KDE.  This is the
 * ONLY public interface.
 */
class KWALLET_EXPORT Wallet : public QObject
{
    Q_OBJECT
protected:
    /*!
     * \internal
     *  Construct a KWallet object.
     *
     *  \a handle The handle for the wallet.
     *
     *  \a name The name of the wallet.
     */
    Wallet(int handle, const QString &name);
    /*!
     *  \brief Copy a KWallet object.
     *  \internal
     */
    Wallet(const Wallet &);

public:
    /*!
     * \value Unknown An invalid entry
     * \value Password The entry is a simple text password
     * \value Stream The entry is binary data which the client will need to know how to decode
     * \value Map The entry is a key/value map and can be decoded as a QMap<QString, QString>
     * \omitvalue Unused
     */
    enum EntryType {
        Unknown = 0,
        Password,
        Stream,
        Map,
        Unused = 0xffff,
    };

    /*!
     * Destroy a KWallet object.
     *
     * Closes the wallet.
     */
    ~Wallet() override;

    /*!
     * List all the wallets available.
     *
     * Returns a list of the names of all wallets that are
     *          open.
     */
    static QStringList walletList();

    /*!
     * Determine if the KDE wallet is enabled.  Normally you do
     * not need to use this because openWallet() will just fail.
     *
     * Returns true if the wallet enabled, else false.
     */
    static bool isEnabled();

    /*!
     *  Determine if the wallet \a name is open by any application.
     *
     *  \a name The name of the wallet to check.
     *
     *  Returns \c true if the wallet is open, else false.
     */
    static bool isOpen(const QString &name);

    /*!
     * Close the wallet \a name.  The wallet will only be closed
     * if it is open but not in use (rare), or if it is forced
     * closed.
     *
     * \a name The name of the wallet to close.
     *
     * \a force Set true to force the wallet closed even if it
     *               is in use by others.
     *
     * Returns 0 on success, non-zero on error.
     */
    static int closeWallet(const QString &name, bool force);

    /*!
     *  Delete the wallet \a name.  The wallet will be forced closed
     *  first.
     *
     *  \a name The name of the wallet to delete.
     *
     *  Returns 0 on success, non-zero on error.
     */
    static int deleteWallet(const QString &name);

    /*!
     * Disconnect the application \a app from \a wallet.
     *
     * \a wallet The name of the wallet to disconnect.
     *
     * \a app The name of the application to disconnect.
     *
     * Returns \c true on success, false on error.
     */
    static bool disconnectApplication(const QString &wallet, const QString &app);

    /*!
     * \value Synchronous
     * \value Asynchronous
     * \value Path
     * \omitvalue OpenTypeUnused
     *
     */
    enum OpenType {
        Synchronous = 0,
        Asynchronous,
        Path,
        OpenTypeUnused = 0xff,
    };

    /*!
     * Open the wallet \a name.  The user will be prompted to
     * allow your application to open the wallet, and may be
     * prompted for a password.  You are responsible for deleting
     * this object when you are done with it.
     *
     * \a name The name of the wallet to open.
     *
     * \a ot    If Asynchronous, the call will return
     *          immediately with a non-null pointer to an
     *          invalid wallet.  You must immediately connect
     *          the walletOpened() signal to a slot so that
     *          you will know when it is opened, or when it
     *          fails.
     *
     * \a w The window id to associate any dialogs with. You can pass
     *          0 if you don't have a window the password dialog should
     *          associate with.
     *
     * Returns a pointer to the wallet if successful,
     * or a null pointer on error or if rejected.
     * A null pointer can also be returned if user choose to
     * deactivate the wallet system.
     */
    static Wallet *openWallet(const QString &name, WId w, OpenType ot = Synchronous);

    /*!
     * List the applications that are using the wallet \a wallet.
     *
     * \a wallet The wallet to query.
     *
     * Returns a list of all D-Bus application IDs using
     *          the wallet.
     */
    static QStringList users(const QString &wallet);

    /*!
     * Returns the name of the wallet used to store local passwords.
     */
    static const QString LocalWallet();

    /*!
     * Returns the name of the wallet used to store network passwords.
     */
    static const QString NetworkWallet();

    /*!
     * \brief Returns the standardized name of the password folder.
     *
     * It is automatically created when a wallet is created, but
     * the user may still delete it so you should check for its
     * existence and recreate it if necessary and desired.
     */
    static const QString PasswordFolder();

    /*!
     *  \brief Returns the standardized name of the form data folder.
     *
     * It is automatically created when a wallet is created, but
     * the user may still delete it so you should check for its
     * existence and recreate it if necessary and desired.
     */
    static const QString FormDataFolder();

    /*!
     *  Request to the wallet service to change the password of
     *  the wallet \a name.
     *
     *  \a name The wallet to change the password of.
     *
     *  \a w The window id to associate any dialogs with. You can pass
     *           0 if you don't have a window the password dialog should
     *           associate with.
     */
    static void changePassword(const QString &name, WId w);

    /*!
     * This syncs the wallet file on disk with what is in memory.
     * You don't normally need to use this.  It happens
     * automatically on close.
     *
     * Returns 0 on success, non-zero on error.
     */
    virtual int sync();

    /*!
     * This closes and locks the current wallet.  It will
     * disconnect all applications using the wallet.
     *
     * Returns 0 on success, non-zero on error.
     */
    virtual int lockWallet();

    /*!
     * Returns the name of the current wallet.
     */
    virtual const QString &walletName() const;

    /*!
     * Determine if the current wallet is open, and is a valid
     * wallet handle.
     *
     * Returns true if the wallet handle is valid and open.
     */
    virtual bool isOpen() const;

    /*!
     * Request to the wallet service to change the password of
     * the current wallet.
     *
     * \a w The window id to associate any dialogs with. You can pass
     *           0 if you don't have a window the password dialog should
     *           associate with.
     */
    virtual void requestChangePassword(WId w);

    /*!
     * Obtain the list of all folders contained in the wallet.
     *
     * Returns an empty list if the wallet is not open.
     */
    virtual QStringList folderList();

    /*!
     * Determine if the folder \a f exists in the wallet.
     *
     * \a f the name of the folder to check for
     *
     * Returns true if the folder exists in the wallet.
     */
    virtual bool hasFolder(const QString &f);

    /*!
     * Set the current working folder to \a f.  The folder must
     * exist, or this call will fail.  Create a folder with
     * createFolder().
     *
     * \a f the name of the folder to make the working folder
     *
     * Returns true if the folder was successfully set.
     */
    virtual bool setFolder(const QString &f);

    /*!
     *  Remove the folder \a f and all its entries from the wallet.
     *
     *  \a f the name of the folder to remove
     *
     *  Returns true if the folder was successfully removed.
     */
    virtual bool removeFolder(const QString &f);

    /*!
     *  Created the folder \a f.
     *
     * \a f the name of the folder to create
     *
     * Returns true if the folder was successfully created.
     */
    virtual bool createFolder(const QString &f);

    /*!
     * Determine the current working folder in the wallet.
     * If the folder name is empty, it is working in the global
     * folder, which is valid but discouraged.
     *
     * Returns the current working folder.
     */
    virtual const QString &currentFolder() const;

    /*!
     * Return the list of keys of all entries in this folder.
     *
     * Returns an empty list if the wallet is not open, or
     * if the folder is empty.
     */
    virtual QStringList entryList();

    // TODO KDE5: a entryList(folder) so that kwalletmanager can list a folder without
    // having to call setFolder before and after (which calls contains() in each)

    /*!
     * Rename the entry \a oldName to \a newName.
     *
     * \a oldName The original key of the entry.
     *
     * \a newName The new key of the entry.
     *
     * Returns 0 on success, non-zero on error.
     */
    virtual int renameEntry(const QString &oldName, const QString &newName);

    /*!
     *  Read the entry \a key from the current folder.
     *  The entry format is unknown except that it is either a
     *  QByteArray or a QDataStream, which effectively means that
     *  it is anything.
     *
     *  \a key The key of the entry to read.
     *
     *  \a value A buffer to fill with the value.
     *
     *  Returns 0 on success, non-zero on error.
     */
    virtual int readEntry(const QString &key, QByteArray &value);

    /*!
     *  Read the map entry \a key from the current folder.
     *
     *  \a key The key of the entry to read.
     *
     *  \a value A map buffer to fill with the value.
     *
     * Returns 0 on success, non-zero on error.  Will
     * return an error if the key was not originally
     * written as a map.
     */
    virtual int readMap(const QString &key, QMap<QString, QString> &value);

    /*!
     * Read the password entry \a key from the current folder.
     *
     * \a key The key of the entry to read.
     *
     * \a value A password buffer to fill with the value.
     *
     * Returns 0 on success, non-zero on error.  Will
     * return an error if the key was not originally
     * written as a password.
     */
    virtual int readPassword(const QString &key, QString &value);

    /*!
     *  Get a list of all the entries in the current folder. The entry
     *  format is unknown except that it is either a QByteArray or a
     *  QDataStream, which effectively means that it could be anything.
     *
     *  \a ok if not nullptr, the object this parameter points to will be set
     *        to true to indicate success or false otherwise
     *
     *  Returns a map of key/value pairs where the key in the map is the entry key
     *
     *  \since 5.72
     */
    QMap<QString, QByteArray> entriesList(bool *ok) const;

    /*!
     * Get a list of all the maps in the current folder.
     *
     * \a ok if not nullptr, the object this parameter points to will be set
     *       to true to indicate success or false otherwise. Note that if any
     *       of the keys was not originally written as a map, the object will
     *       be set to false
     *
     * Returns a map of key/value pairs where the key in the map is the entry key
     *
     * \since 5.72
     */
    QMap<QString, QMap<QString, QString>> mapList(bool *ok) const;

    /*!
     * Get a list of all the passwords in the current folder, which can
     * be used to populate a list view in a password manager.
     *
     * \a ok if not nullptr, the object this parameter points to will be
     *       set to true to indicate success or false otherwise. Note that
     *       the object will be set to false if any of the keys was not
     *       originally written as a password
     *
     * Returns a map of key/value pairs, where the key in the map is the entry key
     *
     * \since 5.72
     */
    QMap<QString, QString> passwordList(bool *ok) const;

    /*!
     * Write \a key = \a value as a binary entry to the current
     * folder.  Be careful with this, it could cause inconsistency
     * in the future since you can put an arbitrary entry type in
     * place.
     *
     * \a key The key of the new entry.
     *
     * \a value The value of the entry.
     *
     * \a entryType The type of the entry.
     *
     * Returns 0 on success, non-zero on error.
     */
    virtual int writeEntry(const QString &key, const QByteArray &value, EntryType entryType);

    /*!
     * Write \a key = \a value as a binary entry to the current
     * folder.
     *
     * \a key The key of the new entry.
     *
     * \a value The value of the entry.
     *
     * Returns 0 on success, non-zero on error.
     */
    virtual int writeEntry(const QString &key, const QByteArray &value);

    /*!
     * Write \a key = \a value as a map to the current folder.
     *
     * \a key The key of the new entry.
     *
     * \a value The value of the map.
     *
     * Returns 0 on success, non-zero on error.
     */
    virtual int writeMap(const QString &key, const QMap<QString, QString> &value);

    /*!
     *  Write \a key = \a value as a password to the current folder.
     *
     * \a key The key of the new entry.
     *
     * \a value The value of the password.
     *
     *  Returns 0 on success, non-zero on error.
     */
    virtual int writePassword(const QString &key, const QString &value);

    /*!
     * Determine if the current folder has they entry \a key.
     *
     * \a key The key to search for.
     *
     * Returns true if the folder contains \a key.
     */
    virtual bool hasEntry(const QString &key);

    /*!
     * Remove the entry \a key from the current folder.
     *
     * \a key The key to remove.
     *
     * Returns 0 on success, non-zero on error.
     */
    virtual int removeEntry(const QString &key);

    /*!
     * Determine the type of the entry \a key in this folder.
     *
     * \a key The key to look up.
     *
     * Returns an enumerated type representing the type
     * of the entry.
     */
    virtual EntryType entryType(const QString &key);

    /*!
     * Determine if a folder does not exist in a wallet.  This
     * does not require decryption of the wallet.
     * This is a handy optimization to avoid prompting the user
     * if your data is certainly not in the wallet.
     *
     * \a wallet The wallet to look in.
     *
     * \a folder The folder to look up.
     *
     * Returns true if the folder does NOT exist in the
     * wallet, or the wallet does not exist.
     */
    static bool folderDoesNotExist(const QString &wallet, const QString &folder);

    /*!
     * Determine if an entry in a folder does not exist in a
     * wallet.  This does not require decryption of the wallet.
     * This is a handy optimization to avoid prompting the user
     * if your data is certainly not in the wallet.
     *
     * \a wallet The wallet to look in.
     *
     * \a folder The folder to look in.
     *
     * \a key The key to look up.
     *
     * Returns true if the key does NOT exist in the
     *  wallet, or the folder or wallet does not exist.
     */
    static bool keyDoesNotExist(const QString &wallet, const QString &folder, const QString &key);

Q_SIGNALS:
    /*!
     * Emitted when this wallet is closed.
     */
    void walletClosed();

    /*!
     * Emitted when a folder in this wallet is updated.
     *
     * \a folder The folder that was updated.
     */
    void folderUpdated(const QString &folder);

    /*!
     * Emitted when the folder list is changed in this wallet.
     */
    void folderListUpdated();

    /*!
     * Emitted when a folder in this wallet is removed.
     *
     * \a folder The folder that was removed.
     */
    void folderRemoved(const QString &folder);

    /*!
     * Emitted when a wallet is opened in asynchronous mode.
     *
     * \a success True if the wallet was opened successfully.
     */
    void walletOpened(bool success);

private Q_SLOTS:
    /*!
     * \internal
     * D-Bus slot for signals emitted by the wallet service.
     */
    KWALLET_NO_EXPORT void slotWalletClosed(int handle);

    /*!
     * \internal
     * D-Bus slot for signals emitted by the wallet service.
     */
    KWALLET_NO_EXPORT void slotFolderUpdated(const QString &wallet, const QString &folder);

    /*!
     * \internal
     * D-Bus slot for signals emitted by the wallet service.
     */
    KWALLET_NO_EXPORT void slotFolderListUpdated(const QString &wallet);

    /*!
     * \internal
     * D-Bus slot for signals emitted by the wallet service.
     */
    KWALLET_NO_EXPORT void slotApplicationDisconnected(const QString &wallet, const QString &application);

    /*!
     * \internal
     * Callback for kwalletd
     *
     * \a tId identifier for the open transaction
     *
     * \a handle the wallet's handle
     */
    KWALLET_NO_EXPORT void walletAsyncOpened(int tId, int handle);

    /*!
     * \internal
     * D-Bus error slot.
     */
    KWALLET_NO_EXPORT void emitWalletAsyncOpenError();

    /*!
     * \internal
     * Emits wallet opening success.
     */
    KWALLET_NO_EXPORT void emitWalletOpened();

    /*!
     * \internal
     * Receives status changed notifications from KSecretsService infrastructure
     */
    KWALLET_NO_EXPORT void slotCollectionStatusChanged(int);
    /*!
     * \internal
     * Received delete notification from KSecretsService infrastructure
     */
    KWALLET_NO_EXPORT void slotCollectionDeleted();

private:
    class WalletPrivate;
    WalletPrivate *const d;
    Q_PRIVATE_SLOT(d, void walletServiceUnregistered())

protected:
    virtual void virtual_hook(int id, void *data);
};

}

#endif //_KWALLET_H
