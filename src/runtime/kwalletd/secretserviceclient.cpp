/*
    SPDX-FileCopyrightText: 2024 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "secretserviceclient.h"

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QEventLoop>
#include <QTimer>

const SecretSchema *qtKeychainSchema(void)
{
    static const SecretSchema schema = {
        "org.qt.keychain",
        SECRET_SCHEMA_DONT_MATCH_NAME,
        {{"user", SECRET_SCHEMA_ATTRIBUTE_STRING}, {"server", SECRET_SCHEMA_ATTRIBUTE_STRING}, {"type", SECRET_SCHEMA_ATTRIBUTE_STRING}}};

    return &schema;
}

// Custom deleters for non GObject things
struct GListDeleter {
    void operator()(GList *list) const
    {
        if (list) {
            g_list_free(list);
        }
    }
};

struct GHashTableDeleter {
    void operator()(GHashTable *table) const
    {
        if (table) {
            g_hash_table_destroy(table);
        }
    }
};

struct SecretValueDeleter {
    void operator()(SecretValue *value) const
    {
        if (value) {
            secret_value_unref(value);
        }
    }
};

using SecretItemPtr = GObjectPtr<SecretItem>;

using GListPtr = std::unique_ptr<GList, GListDeleter>;
using GHashTablePtr = std::unique_ptr<GHashTable, GHashTableDeleter>;
using SecretValuePtr = std::unique_ptr<SecretValue, SecretValueDeleter>;

static bool wasErrorFree(GError **error, SecretServiceClient *wrapper)
{
    if (!*error) {
        return true;
    }
    Q_EMIT wrapper->error(QString::fromUtf8((*error)->message));
    g_error_free(*error);
    *error = nullptr;
    return false;
}

static QString typeToString(SecretServiceClient::Type type)
{
    switch (type) {
    case SecretServiceClient::Binary:
        return QStringLiteral("base64");
    case SecretServiceClient::Map:
        return QStringLiteral("map");
    default:
        return QStringLiteral("plaintext");
    }
}

static SecretServiceClient::Type stringToType(const QString &typeName)
{
    if (typeName == QStringLiteral("base64")) {
        return SecretServiceClient::Binary;
    } else if (typeName == QStringLiteral("map")) {
        return SecretServiceClient::Map;
    } else {
        return SecretServiceClient::PlainText;
    }
}

SecretServiceClient::SecretServiceClient(bool useKWalletBackend, QObject *parent)
    : QObject(parent)
{
    if (useKWalletBackend) {
        m_serviceBusName = QStringLiteral("org.kde.secretservicecompat");
    } else {
        m_serviceBusName = QStringLiteral("org.freedesktop.secrets");
    }

    m_collectionDirtyTimer = new QTimer(this);
    m_collectionDirtyTimer->setSingleShot(true);
    m_collectionDirtyTimer->setInterval(100);
    connect(m_collectionDirtyTimer, &QTimer::timeout, this, [this]() {
        for (const QString &collection : std::as_const(m_dirtyCollections)) {
            Q_EMIT collectionDirty(collection);
        }
        m_dirtyCollections.clear();
    });

    m_serviceWatcher = new QDBusServiceWatcher(m_serviceBusName, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange, this);

    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &SecretServiceClient::onServiceOwnerChanged);

    QDBusInterface iface(QStringLiteral("org.freedesktop.DBus"),
                         QStringLiteral("/org/freedesktop/DBus"),
                         QStringLiteral("org.freedesktop.DBus"),
                         QDBusConnection::sessionBus());

    QDBusReply<QString> reply = iface.call(QStringLiteral("GetNameOwner"), m_serviceBusName);

    GError *error = nullptr;
    m_service = SecretServicePtr(
        secret_service_get_sync(static_cast<SecretServiceFlags>(SECRET_SERVICE_OPEN_SESSION | SECRET_SERVICE_LOAD_COLLECTIONS), nullptr, &error));

    if (reply.isValid() && !reply.value().isEmpty()) {
        onServiceOwnerChanged(QString(), QString(), reply.value());
    }

    bool ok = wasErrorFree(&error, this);
    if (ok) {
        for (const QString &collection : listCollections(&ok)) {
            watchCollection(collection, &ok);
        }
    }
}

SecretCollection *SecretServiceClient::retrieveCollection(const QString &name)
{
    if (!m_service) {
        Q_EMIT SecretServiceClient::error(i18n("Not connected to Secret Service"));
        return nullptr;
    }

    auto it = m_openCollections.find(name);
    if (it != m_openCollections.end()) {
        return it->second.get();
    }

    SecretCollection *collection = nullptr;
    GListPtr collections = GListPtr(secret_service_get_collections(m_service.get()));

    for (GList *l = collections.get(); l != nullptr; l = l->next) {
        SecretCollection *coll = SECRET_COLLECTION(l->data);
        const gchar *label = secret_collection_get_label(coll);
        if (QString::fromUtf8(label) == name) {
            collection = coll;
            break;
        } else {
            g_object_unref(coll);
        }
    }

    SecretCollectionPtr colPtr = SecretCollectionPtr(collection);
    m_openCollections.insert(std::make_pair(name, std::move(colPtr)));

    return collection;
}

SecretItem *SecretServiceClient::retrieveItem(const QString &key, const QString &folder, const QString &collectionName, bool *ok)
{
    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    GHashTablePtr attributes = GHashTablePtr(g_hash_table_new(g_str_hash, g_str_equal));
    g_hash_table_insert(attributes.get(), g_strdup("server"), g_strdup(folder.toUtf8().constData()));
    g_hash_table_insert(attributes.get(), g_strdup("user"), g_strdup(key.toUtf8().constData()));

    GListPtr glist = GListPtr(secret_collection_search_sync(collection,
                                                            qtKeychainSchema(),
                                                            attributes.get(),
                                                            static_cast<SecretSearchFlags>(SECRET_SEARCH_ALL | SECRET_SEARCH_LOAD_SECRETS),
                                                            nullptr,
                                                            &error));

    *ok = wasErrorFree(&error, this);
    if (!*ok) {
        return nullptr;
    }

    SecretItem *item = nullptr;
    if (glist) {
        for (GList *iter = glist.get(); iter != nullptr; iter = iter->next) {
            item = static_cast<SecretItem *>(iter->data);
            break;
        }

    } else {
        Q_EMIT SecretServiceClient::error(i18n("Not found"));
    }

    return item;
}

void SecretServiceClient::watchCollection(const QString &collectionName, bool *ok)
{
    if (m_watchedCollections.contains(collectionName)) {
        *ok = true;
        return;
    }

    SecretCollection *collection = retrieveCollection(collectionName);

    GObjectPtr<GDBusProxy> proxy = GObjectPtr<GDBusProxy>(G_DBUS_PROXY(collection));
    const QString path = QString::fromUtf8(g_strdup(g_dbus_proxy_get_object_path(proxy.get())));
    if (path.isEmpty()) {
        *ok = false;
        return;
    }

    QDBusConnection::sessionBus().connect(m_serviceBusName,
                                          path,
                                          QStringLiteral("org.freedesktop.Secret.Collection"),
                                          QStringLiteral("ItemChanged"),
                                          this,
                                          SLOT(onDbusSecretItemChanged(QDBusObjectPath)));
    QDBusConnection::sessionBus().connect(m_serviceBusName,
                                          path,
                                          QStringLiteral("org.freedesktop.Secret.Collection"),
                                          QStringLiteral("ItemCreated"),
                                          this,
                                          SLOT(onDbusSecretItemChanged(QDBusObjectPath)));
    QDBusConnection::sessionBus().connect(m_serviceBusName,
                                          path,
                                          QStringLiteral("org.freedesktop.Secret.Collection"),
                                          QStringLiteral("ItemDeleted"),
                                          this,
                                          SLOT(onDbusSecretItemChanged(QDBusObjectPath)));

    m_watchedCollections.insert(collectionName);
}

void SecretServiceClient::onServiceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(serviceName);
    Q_UNUSED(oldOwner);

    bool available = !newOwner.isEmpty();

    if (available == (m_service != nullptr)) {
        return;
    }

    m_openCollections.clear();

    if (available && !m_service) {
        GError *error = nullptr;
        m_service = SecretServicePtr(
            secret_service_get_sync(static_cast<SecretServiceFlags>(SECRET_SERVICE_OPEN_SESSION | SECRET_SERVICE_LOAD_COLLECTIONS), nullptr, &error));
        available = wasErrorFree(&error, this);
    }

    if (!available) {
        m_service.reset();
    }

    qDebug() << "Secret Service availability changed:" << (available ? "Available" : "Unavailable");
    Q_EMIT serviceAvailableChanged(m_service != nullptr);
}

void SecretServiceClient::onCollectionCreated(const QDBusObjectPath &path)
{
    if (!m_service) {
        Q_EMIT SecretServiceClient::error(i18n("Not connected to Secret Service"));
        return;
    }

    GError *error = nullptr;

    SecretCollection *collection =
        secret_collection_new_for_dbus_path_sync(m_service.get(), path.path().toUtf8().constData(), SECRET_COLLECTION_NONE, nullptr, &error);

    bool ok = wasErrorFree(&error, this);
    if (!ok) {
        return;
    }

    const QString label = QString::fromUtf8(secret_collection_get_label(collection));

    Q_EMIT collectionCreated(label);
}

void SecretServiceClient::onDbusSecretItemChanged(const QDBusObjectPath &path)
{
    if (!m_service) {
        Q_EMIT SecretServiceClient::error(i18n("Not connected to Secret Service"));
        return;
    }

    // Don't trigger a reload if we did changes ourselves
    if (m_updateInProgress) {
        m_updateInProgress = false;
        return;
    }

    GError *error = nullptr;

    QStringList pieces = path.path().split(QStringLiteral("/"), Qt::SkipEmptyParts);

    // 6 items: /org/freedesktop/secrets/collection/collectionName/itemName
    if (pieces.length() != 6) {
        return;
    }
    pieces.pop_back();
    const QString collectionPath = QStringLiteral("/") % pieces.join(QStringLiteral("/"));

    SecretCollection *collection =
        secret_collection_new_for_dbus_path_sync(m_service.get(), collectionPath.toUtf8().data(), SECRET_COLLECTION_NONE, nullptr, &error);

    bool ok = wasErrorFree(&error, this);
    if (!ok) {
        return;
    }
    if (!collection) {
        return;
    }

    const QString label = QString::fromUtf8(secret_collection_get_label(collection));

    m_dirtyCollections.insert(label);
    m_collectionDirtyTimer->start();
}

void SecretServiceClient::handlePrompt(bool dismissed)
{
    Q_EMIT promptClosed(!dismissed);
}

bool SecretServiceClient::isAvailable() const
{
    return m_service != nullptr;
}

bool SecretServiceClient::unlockCollection(const QString &collectionName, bool *ok)
{
    if (!m_service) {
        Q_EMIT SecretServiceClient::error(i18n("Not connected to Secret Service"));
        *ok = false;
        return false;
    }

    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    if (!collection) {
        *ok = false;
        return false;
    }

    gboolean locked = secret_collection_get_locked(collection);

    if (locked) {
        gboolean success = secret_service_unlock_sync(m_service.get(), g_list_append(nullptr, collection), nullptr, nullptr, &error);
        *ok = wasErrorFree(&error, this);
        if (!success) {
            Q_EMIT SecretServiceClient::error(i18n("Unable to unlock collectionName %1", collectionName));
        } else {
            watchCollection(collectionName, ok);
        }
        return success;
    }

    watchCollection(collectionName, ok);
    return true;
}

QString SecretServiceClient::defaultCollection(bool *ok)
{
    if (!m_service) {
        Q_EMIT SecretServiceClient::error(i18n("Not connected to Secret Service"));
        *ok = false;
        return QString();
    }

    QString label = QStringLiteral("kdecollectionName");
    GError *error = nullptr;

    gchar *path = secret_service_read_alias_dbus_path_sync(m_service.get(), SECRET_COLLECTION_DEFAULT, nullptr, &error);

    *ok = wasErrorFree(&error, this);
    if (!*ok) {
        return label;
    }

    // TODO: port from path to label
    QDBusInterface collectionInterface(m_serviceBusName,
                                       QString::fromUtf8(path),
                                       QStringLiteral("org.freedesktop.Secret.Collection"),
                                       QDBusConnection::sessionBus());

    if (!collectionInterface.isValid()) {
        Q_EMIT SecretServiceClient::error(i18n("Cannot retrieve default collection"));
        *ok = false;
        return label;
    }

    label = collectionInterface.property("Label").toString();
    if (!label.isEmpty()) {
        *ok = true;
        return label;
    }

    *ok = false;
    Q_EMIT SecretServiceClient::error(i18n("Cannot retrieve default collection"));
    return label;
}

void SecretServiceClient::setDefaultCollection(const QString &collectionName, bool *ok)
{
    if (!m_service) {
        Q_EMIT SecretServiceClient::error(i18n("Not connected to Secret Service"));
        *ok = false;
        return;
    }

    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    *ok = secret_service_set_alias_sync(m_service.get(), collectionName.toUtf8().data(), collection, nullptr, &error);

    *ok = *ok && wasErrorFree(&error, this);
}

QStringList SecretServiceClient::listCollections(bool *ok)
{
    if (!m_service) {
        Q_EMIT SecretServiceClient::error(i18n("Not connected to Secret Service"));
        *ok = false;
        return QStringList();
    }

    QStringList collections;

    GListPtr glist = GListPtr(secret_service_get_collections(m_service.get()));

    if (glist) {
        for (GList *iter = glist.get(); iter != nullptr; iter = iter->next) {
            SecretCollection *collection = SECRET_COLLECTION(iter->data);
            const gchar *rawLabel = secret_collection_get_label(collection);
            const QString label = QString::fromUtf8(rawLabel);
            if (!label.isEmpty()) {
                collections.append(label);
            }
        }
    } else {
        Q_EMIT SecretServiceClient::error(i18n("No collectionNames"));
    }

    *ok = true;
    return collections;
}

QStringList SecretServiceClient::listFolders(const QString &collectionName, bool *ok)
{
    QSet<QString> folders;

    SecretCollection *collection = retrieveCollection(collectionName);

    GListPtr glist = GListPtr(secret_collection_get_items(collection));

    if (glist) {
        for (GList *iter = glist.get(); iter != nullptr; iter = iter->next) {
            SecretItem *item = static_cast<SecretItem *>(iter->data);

            GHashTable *attributes = secret_item_get_attributes(item);
            if (attributes) {
                GHashTableIter attrIter;
                gpointer key, value;
                g_hash_table_iter_init(&attrIter, attributes);
                while (g_hash_table_iter_next(&attrIter, &key, &value)) {
                    QString keyString = QString::fromUtf8(static_cast<gchar *>(key));
                    if (keyString == QStringLiteral("server")) {
                        folders.insert(QString::fromUtf8(static_cast<gchar *>(value)));
                        break;
                    }
                }
            }
        }
    } else {
        *ok = false;
        Q_EMIT SecretServiceClient::error(i18n("No entries"));
    }
    *ok = true;
    return folders.values();
}

QStringList SecretServiceClient::listEntries(const QString &folder, const QString &collectionName, bool *ok)
{
    // TODO: deduplicate
    QSet<QString> folders;
    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    GHashTablePtr attributes = GHashTablePtr(g_hash_table_new(g_str_hash, g_str_equal));
    g_hash_table_insert(attributes.get(), g_strdup("server"), g_strdup(folder.toUtf8().constData()));

    GListPtr glist = GListPtr(secret_collection_search_sync(collection, qtKeychainSchema(), attributes.get(), SECRET_SEARCH_ALL, nullptr, &error));

    *ok = wasErrorFree(&error, this);
    if (!*ok) {
        return QStringList();
    }

    if (glist) {
        for (GList *iter = glist.get(); iter != nullptr; iter = iter->next) {
            SecretItemPtr item = SecretItemPtr(static_cast<SecretItem *>(iter->data));
            GHashTablePtr attributes = GHashTablePtr(secret_item_get_attributes(item.get()));

            if (attributes) {
                GHashTableIter attrIter;
                gpointer key, value;
                g_hash_table_iter_init(&attrIter, attributes.get());
                while (g_hash_table_iter_next(&attrIter, &key, &value)) {
                    QString keyString = QString::fromUtf8(static_cast<gchar *>(key));
                    if (keyString == QStringLiteral("user")) {
                        folders.insert(QString::fromUtf8(static_cast<gchar *>(value)));
                        break;
                    }
                }
            }
        }
    } else {
        Q_EMIT SecretServiceClient::error(i18n("No entries"));
    }

    return folders.values();
}

QHash<QString, QString> SecretServiceClient::readMetadata(const QString &key, const QString &folder, const QString &collectionName, bool *ok)
{
    QHash<QString, QString> hash;

    SecretItemPtr item = SecretItemPtr(retrieveItem(key, folder, collectionName, ok));

    if (!item) {
        Q_EMIT SecretServiceClient::error(i18n("Entry not found, key: %1, folder: %2", key, folder));
        return hash;
    }

    GHashTablePtr attributes = GHashTablePtr(secret_item_get_attributes(item.get()));

    if (attributes) {
        GHashTableIter attrIter;
        gpointer key, value;
        g_hash_table_iter_init(&attrIter, attributes.get());
        while (g_hash_table_iter_next(&attrIter, &key, &value)) {
            QString keyString = QString::fromUtf8(static_cast<gchar *>(key));
            QString valueString = QString::fromUtf8(static_cast<gchar *>(value));
            hash.insert(keyString, valueString);
        }
    }

    return hash;
}

void SecretServiceClient::createCollection(const QString &collectionName, bool *ok)
{
    // Using DBus directly here as libsecret doesn't have stable (and complete)
    // api yet to create collections
    QDBusConnection bus = QDBusConnection::sessionBus();

    QDBusMessage message = QDBusMessage::createMethodCall(m_serviceBusName,
                                                          QStringLiteral("/org/freedesktop/secrets"),
                                                          QStringLiteral("org.freedesktop.Secret.Service"),
                                                          QStringLiteral("CreateCollection"));

    QVariantMap props;
    props[QStringLiteral("org.freedesktop.Secret.Collection.Label")] = collectionName;
    message << props << QString();
    QDBusMessage reply = bus.call(message);

    bus.connect(m_serviceBusName,
                reply.arguments().last().value<QDBusObjectPath>().path(),
                QStringLiteral("org.freedesktop.Secret.Prompt"),
                QStringLiteral("Completed"),
                this,
                SLOT(handlePrompt(bool)));

    bus.connect(m_serviceBusName,
                QStringLiteral("/org/freedesktop/secrets"),
                QStringLiteral("org.freedesktop.Secret.Service"),
                QStringLiteral("CollectionCreated"),
                this,
                SLOT(onCollectionCreated(QDBusObjectPath)));

    QEventLoop loop;
    bool created = false;
    connect(this, &SecretServiceClient::collectionCreated, &loop, [this, &loop, collectionName, &created, ok](const QString &name) {
        *ok = !name.isEmpty();
        if (listCollections(ok).contains(collectionName)) {
            created = true;
        }
        loop.quit();
    });

    message = QDBusMessage::createMethodCall(m_serviceBusName,
                                             reply.arguments().last().value<QDBusObjectPath>().path(),
                                             QStringLiteral("org.freedesktop.Secret.Prompt"),
                                             QStringLiteral("Prompt"));
    message << QString();
    bus.call(message);

    connect(this, &SecretServiceClient::promptClosed, &loop, [this, &loop, collectionName, &created, ok](bool accepted) {
        *ok = accepted;
        if (listCollections(ok).contains(collectionName)) {
            created = true;
        }
        loop.quit();
    });
    loop.exec();

    // If the CollectionCreated signal didn't arrive yet, wait for it
    if (!created) {
        QTimer timer;
        timer.setSingleShot(true);
        // Wait for maximum 1 second before giving up
        timer.setInterval(1000);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start();
        loop.exec();
    }
}

void SecretServiceClient::deleteCollection(const QString &collectionName, bool *ok)
{
    if (!m_service) {
        Q_EMIT SecretServiceClient::error(i18n("Not connected to Secret Service"));
        *ok = false;
        return;
    }

    GError *error = nullptr;

    SecretCollection *collection = nullptr;
    GListPtr collections = GListPtr(secret_service_get_collections(m_service.get()));

    for (GList *l = collections.get(); l != nullptr; l = l->next) {
        SecretCollection *coll = SECRET_COLLECTION(l->data);
        const gchar *label = secret_collection_get_label(coll);

        if (QString::fromUtf8(label) == collectionName) {
            collection = coll;
            break;
        } else {
            g_object_unref(coll);
        }
    }

    *ok = secret_collection_delete_sync(collection, nullptr, &error);
    g_object_unref(collection);

    *ok = *ok && wasErrorFree(&error, this);
}

void SecretServiceClient::deleteFolder(const QString &folder, const QString &collectionName, bool *ok)
{
    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    GHashTablePtr attributes = GHashTablePtr(g_hash_table_new(g_str_hash, g_str_equal));
    g_hash_table_insert(attributes.get(), g_strdup("server"), g_strdup(folder.toUtf8().constData()));

    GListPtr glist = GListPtr(secret_collection_search_sync(collection, qtKeychainSchema(), attributes.get(), SECRET_SEARCH_ALL, nullptr, &error));

    *ok = wasErrorFree(&error, this);
    if (!*ok) {
        return;
    }

    if (glist) {
        for (GList *iter = glist.get(); iter != nullptr; iter = iter->next) {
            SecretItem *item = static_cast<SecretItem *>(iter->data);
            m_updateInProgress = true;
            secret_item_delete_sync(item, nullptr, &error);
            *ok = wasErrorFree(&error, this);
            g_object_unref(item);
        }
    } else {
        Q_EMIT SecretServiceClient::error(i18n("No entries"));
    }
}

QByteArray
SecretServiceClient::readEntry(const QString &key, const SecretServiceClient::Type type, const QString &folder, const QString &collectionName, bool *ok)
{
    GError *error = nullptr;
    QByteArray data;

    SecretItemPtr item = SecretItemPtr(retrieveItem(key, folder, collectionName, ok));

    if (item) {
        // Some providers like KeepassXC lock each item individually, and need to be
        // unlocked by the user prior being able to access
        if (secret_item_get_locked(item.get())) {
            secret_service_unlock_sync(m_service.get(), g_list_append(nullptr, item.get()), nullptr, nullptr, &error);
            *ok = wasErrorFree(&error, this);
            if (!ok) {
                Q_EMIT SecretServiceClient::error(i18n("Unable to unlock item"));
                return data;
            }

            secret_item_load_secret_sync(item.get(), nullptr, &error);
            *ok = wasErrorFree(&error, this);
            SecretValuePtr secretValue = SecretValuePtr(secret_item_get_secret(item.get()));
            if (secretValue) {
                const gchar *password = secret_value_get_text(secretValue.get());
                if (type == SecretServiceClient::Binary) {
                    data = QByteArray::fromBase64(QByteArray(password));
                } else {
                    data = QByteArray(password);
                }
            }
        } else {
            SecretValuePtr secretValue = SecretValuePtr(secret_item_get_secret(item.get()));
            if (secretValue) {
                const gchar *password = secret_value_get_text(secretValue.get());
                if (type == SecretServiceClient::Binary) {
                    data = QByteArray::fromBase64(QByteArray(password));
                } else {
                    data = QByteArray(password);
                }
            }
        }
    }

    return data;
}

void SecretServiceClient::renameEntry(const QString &display_name,
                                      const QString &oldKey,
                                      const QString &newKey,
                                      const QString &folder,
                                      const QString &collectionName,
                                      bool *ok)
{
    SecretItemPtr item = SecretItemPtr(retrieveItem(oldKey, folder, collectionName, ok));
    if (!*ok) {
        return;
    }
    if (!item) {
        *ok = false;
        Q_EMIT SecretServiceClient::error(i18n("Entry to rename not found"));
        return;
    }

    QByteArray data;

    Type type = PlainText;
    GHashTablePtr attributes = GHashTablePtr(secret_item_get_attributes(item.get()));
    if (attributes) {
        GHashTableIter attrIter;
        gpointer key, value;
        g_hash_table_iter_init(&attrIter, attributes.get());
        while (g_hash_table_iter_next(&attrIter, &key, &value)) {
            QString keyString = QString::fromUtf8(static_cast<gchar *>(key));
            if (keyString == QStringLiteral("type")) {
                const QString typeString = QString::fromUtf8(static_cast<gchar *>(value));
                type = stringToType(typeString);
            }
        }
    } else {
        *ok = false;
        Q_EMIT SecretServiceClient::error(i18n("Entry to rename incomplete"));
        return;
    }

    SecretValuePtr secretValue = SecretValuePtr(secret_item_get_secret(item.get()));
    if (secretValue) {
        const gchar *password = secret_value_get_text(secretValue.get());

        if (type == Binary) {
            data = QByteArray::fromBase64(QByteArray(password));
        } else {
            data = QByteArray(password);
        }
    } else {
        *ok = false;
        Q_EMIT SecretServiceClient::error(i18n("Entry to rename incomplete"));
        return;
    }

    deleteEntry(oldKey, folder, collectionName, ok);
    if (!*ok) {
        return;
    }
    writeEntry(display_name, newKey, data, type, folder, collectionName, ok);
}

void SecretServiceClient::writeEntry(const QString &display_name,
                                     const QString &key,
                                     const QByteArray &value,
                                     const SecretServiceClient::Type type,
                                     const QString &folder,
                                     const QString &collectionName,
                                     bool *ok)
{
    if (!m_service) {
        Q_EMIT SecretServiceClient::error(i18n("Not connected to Secret Service"));
        *ok = false;
        return;
    }

    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    QByteArray data;
    if (type == SecretServiceClient::Binary) {
        data = value.toBase64();
    } else {
        data = value;
    }

    SecretValuePtr secretValue = SecretValuePtr(secret_value_new(data.constData(), -1, "text/plain"));
    if (!secretValue) {
        *ok = false;
        Q_EMIT SecretServiceClient::error(i18n("Failed to create SecretValue"));
        return;
    }

    GHashTablePtr attributes = GHashTablePtr(g_hash_table_new(g_str_hash, g_str_equal));
    g_hash_table_insert(attributes.get(), g_strdup("user"), g_strdup(key.toUtf8().constData()));
    g_hash_table_insert(attributes.get(), g_strdup("type"), g_strdup(typeToString(type).toUtf8().constData()));
    g_hash_table_insert(attributes.get(), g_strdup("server"), g_strdup(folder.toUtf8().constData()));

    m_updateInProgress = true;
    SecretItemPtr item = SecretItemPtr(secret_item_create_sync(collection,
                                                               qtKeychainSchema(),
                                                               attributes.get(),
                                                               display_name.toUtf8().constData(),
                                                               secretValue.get(),
                                                               SECRET_ITEM_CREATE_REPLACE,
                                                               nullptr,
                                                               &error));

    *ok = wasErrorFree(&error, this);
}

void SecretServiceClient::deleteEntry(const QString &key, const QString &folder, const QString &collectionName, bool *ok)
{
    GError *error = nullptr;
    SecretItemPtr item = SecretItemPtr(retrieveItem(key, folder, collectionName, ok));
    if (!*ok) {
        return;
    }
    if (!item) {
        *ok = false;
        Q_EMIT SecretServiceClient::error(i18n("Entry to rename not found"));
        return;
    }
    m_updateInProgress = true;
    secret_item_delete_sync(item.get(), nullptr, &error);
    *ok = wasErrorFree(&error, this);
}

#include <moc_secretserviceclient.cpp>
