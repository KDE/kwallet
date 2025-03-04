/*
    SPDX-FileCopyrightText: 2024 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QDBusObjectPath>
#include <QObject>

#include <libsecret/secret.h>

class QDBusServiceWatcher;
class QTimer;

// To allow gobject derived things with std::unique_ptr
struct GObjectDeleter {
    template<typename T>
    void operator()(T *obj) const
    {
        if (obj) {
            g_object_unref(obj);
        }
    }
};

template<typename T>
using GObjectPtr = std::unique_ptr<T, GObjectDeleter>;
using SecretServicePtr = GObjectPtr<SecretService>;
using SecretCollectionPtr = GObjectPtr<SecretCollection>;
using SecretItemPtr = GObjectPtr<SecretItem>;

class SecretServiceClient : public QObject
{
    Q_OBJECT

public:
    enum Type {
        PlainText = 0,
        Base64,
        Binary,
        Map,
        Unknown
    };
    Q_ENUM(Type);

    SecretServiceClient(bool useKWalletBackend, QObject *parent = nullptr);

    bool isAvailable() const;

    bool unlockCollection(const QString &collectionName, bool *ok);

    QString defaultCollection(bool *ok);
    void setDefaultCollection(const QString &collectionName, bool *ok);
    QStringList listCollections(bool *ok);
    QStringList listFolders(const QString &collectionName, bool *ok);

    QStringList listEntries(const QString &folder, const QString &collectionName, bool *ok);

    QHash<QString, QString> readMetadata(const QString &key, const QString &folder, const QString &collectionName, bool *ok);

    void createCollection(const QString &collectionName, bool *ok);

    void deleteCollection(const QString &collectionName, bool *ok);

    void deleteFolder(const QString &folder, const QString &collectionName, bool *ok);

    QByteArray readEntry(const QString &key, const Type type, const QString &folder, const QString &collectionName, bool *ok);

    void renameEntry(const QString &display_name, const QString &oldKey, const QString &newKey, const QString &folder, const QString &collectionName, bool *ok);

    void writeEntry(const QString &itemName,
                    const QString &key,
                    const QByteArray &value,
                    const SecretServiceClient::Type type,
                    const QString &folder,
                    const QString &collectionName,
                    bool *ok);

    void deleteEntry(const QString &key, const QString &folder, const QString &collectionName, bool *ok);

Q_SIGNALS:
    // Emitted when the service availability changed, or the service owner of secretservice has changed to a new one
    void serviceChanged();
    void promptClosed(bool accepted);
    void collectionListDirty();
    void collectionDirty(const QString &collection);
    void collectionCreated(const QString &collection);
    void collectionDeleted(const QString &collection);

protected:
    bool attemptConnection();
    void watchCollection(const QString &collectionName, bool *ok);
    void onServiceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner);

    QString collectionLabelForPath(const QDBusObjectPath &path);
    SecretCollection *retrieveCollection(const QString &name);
    SecretItemPtr retrieveItem(const QString &key, const SecretServiceClient::Type type, const QString &folder, const QString &collectionName, bool *ok);

protected Q_SLOTS:
    void handlePrompt(bool dismissed);
    void onCollectionCreated(const QDBusObjectPath &path);
    void onCollectionDeleted(const QDBusObjectPath &path);
    void onSecretItemChanged(const QDBusObjectPath &path);

private:
    SecretServicePtr m_service;
    std::map<QString, SecretCollectionPtr> m_openCollections;
    QDBusServiceWatcher *m_serviceWatcher;
    QSet<QString> m_watchedCollections;
    QSet<QString> m_dirtyCollections;
    QTimer *m_collectionDirtyTimer;
    QString m_serviceBusName;
    bool m_updateInProgress = false;
};
