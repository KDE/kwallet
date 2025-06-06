/*
    SPDX-FileCopyrightText: 2024 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "secretserviceclient.h"
#include "kwalletd_debug.h"

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QEventLoop>
#include <QTimer>

// Copied from
// https://github.com/frankosterfeld/qtkeychain/blob/main/qtkeychain/libsecret.cpp
// This is intended to be a data format compatible with QtKeychain
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

using GListPtr = std::unique_ptr<GList, GListDeleter>;
using GHashTablePtr = std::unique_ptr<GHashTable, GHashTableDeleter>;
using SecretValuePtr = std::unique_ptr<SecretValue, SecretValueDeleter>;

static bool wasErrorFree(GError **error)
{
    if (!*error) {
        return true;
    }
    qCWarning(KWALLETD_LOG) << QString::fromUtf8((*error)->message);
    g_error_free(*error);
    *error = nullptr;
    return false;
}

static QString typeToString(SecretServiceClient::Type type)
{
    // Similar to QtKeychain implementation: adds the "map" datatype
    switch (type) {
    case SecretServiceClient::Base64:
        return QStringLiteral("base64");
    case SecretServiceClient::Binary:
        return QStringLiteral("binary");
    case SecretServiceClient::Map:
        return QStringLiteral("map");
    default:
        return QStringLiteral("plaintext");
    }
}

static SecretServiceClient::Type stringToType(const QString &typeName)
{
    if (typeName == QStringLiteral("binary")) {
        return SecretServiceClient::Binary;
    } else if (typeName == QStringLiteral("base64")) {
        return SecretServiceClient::Base64;
    } else if (typeName == QStringLiteral("map")) {
        return SecretServiceClient::Map;
    } else {
        return SecretServiceClient::PlainText;
    }
}

SecretServiceClient::SecretServiceClient(QObject *parent)
    : QObject(parent)
{
    KConfig cfg(QStringLiteral("kwalletrc"));
    KConfigGroup ksecretGroup(&cfg, QStringLiteral("KSecretD"));
    m_useKSecretBackend = ksecretGroup.readEntry("Enabled", true);

    if (m_useKSecretBackend) {
        // Tell libsecret where the secretservice api is
        qputenv("SECRET_SERVICE_BUS_NAME", "org.kde.secretservicecompat");
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

    // Unconditionally try to connect to the service without checking it exists:
    // it will try to dbus-activate it if not running
    if (attemptConnection()) {
        bool ok = false;
        for (const QString &collection : listCollections(&ok)) {
            watchCollection(collection, &ok);
        }
    }

    QDBusConnection bus = QDBusConnection::sessionBus();

    // React to collections being created, either by us or somebody else
    bus.connect(m_serviceBusName,
                QStringLiteral("/org/freedesktop/secrets"),
                QStringLiteral("org.freedesktop.Secret.Service"),
                QStringLiteral("CollectionCreated"),
                this,
                SLOT(onCollectionCreated(QDBusObjectPath)));
    bus.connect(m_serviceBusName,
                QStringLiteral("/org/freedesktop/secrets"),
                QStringLiteral("org.freedesktop.Secret.Service"),
                QStringLiteral("CollectionDeleted"),
                this,
                SLOT(onCollectionDeleted(QDBusObjectPath)));
}

QString SecretServiceClient::collectionLabelForPath(const QDBusObjectPath &path)
{
    if (!attemptConnection()) {
        return {};
    }
    QDBusInterface collectionInterface(m_serviceBusName, path.path(), QStringLiteral("org.freedesktop.Secret.Collection"), QDBusConnection::sessionBus());

    if (!collectionInterface.isValid()) {
        qCWarning(KWALLETD_LOG) << "Failed to connect to the DBus collection object:" << path.path();
        return {};
    }

    QVariant reply = collectionInterface.property("Label");

    if (!reply.isValid()) {
        qCWarning(KWALLETD_LOG) << "Error reading label:" << collectionInterface.lastError();
        return {};
    }

    return reply.toString();
}

SecretCollection *SecretServiceClient::retrieveCollection(const QString &name)
{
    if (!attemptConnection()) {
        return nullptr;
    }

    auto it = m_openCollections.find(name);
    if (it != m_openCollections.end()) {
        return it->second.get();
    }

    GListPtr collections = GListPtr(secret_service_get_collections(m_service.get()));

    for (GList *l = collections.get(); l != nullptr; l = l->next) {
        SecretCollectionPtr colPtr = SecretCollectionPtr(SECRET_COLLECTION(l->data));
        const gchar *label = secret_collection_get_label(colPtr.get());
        if (QString::fromUtf8(label) == name) {
            m_openCollections.insert(std::make_pair(name, std::move(colPtr)));
            SecretCollection *collection = colPtr.get();
            return collection;
        }
    }

    return nullptr;
}

SecretItemPtr
SecretServiceClient::retrieveItem(const QString &key, const SecretServiceClient::Type type, const QString &folder, const QString &collectionName, bool *ok)
{
    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    GHashTablePtr attributes = GHashTablePtr(g_hash_table_new(g_str_hash, g_str_equal));
    g_hash_table_insert(attributes.get(), g_strdup("server"), g_strdup(folder.toUtf8().constData()));
    g_hash_table_insert(attributes.get(), g_strdup("user"), g_strdup(key.toUtf8().constData()));
    if (type != Unknown) {
        g_hash_table_insert(attributes.get(), g_strdup("type"), g_strdup(typeToString(type).toUtf8().constData()));
    }

    GListPtr glist = GListPtr(secret_collection_search_sync(collection,
                                                            qtKeychainSchema(),
                                                            attributes.get(),
                                                            static_cast<SecretSearchFlags>(SECRET_SEARCH_ALL | SECRET_SEARCH_LOAD_SECRETS),
                                                            nullptr,
                                                            &error));

    *ok = wasErrorFree(&error);
    if (!*ok) {
        return nullptr;
    }

    SecretItem *item = nullptr;
    if (glist) {
        GList *iter = glist.get();
        if (iter != nullptr) {
            item = static_cast<SecretItem *>(iter->data);
        }

    } else {
        qCWarning(KWALLETD_LOG) << i18n("Item not found");
        *ok = false;
    }

    return SecretItemPtr(item);
}

bool SecretServiceClient::attemptConnection()
{
    if (m_service) {
        return true;
    }

    GError *error = nullptr;
    m_service = SecretServicePtr(
        secret_service_get_sync(static_cast<SecretServiceFlags>(SECRET_SERVICE_OPEN_SESSION | SECRET_SERVICE_LOAD_COLLECTIONS), nullptr, &error));

    bool ok = wasErrorFree(&error);

    if (!ok || !m_service) {
        qCWarning(KWALLETD_LOG) << i18n("Could not connect to Secret Service");
        return false;
    }

    return true;
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
                                          SLOT(onSecretItemChanged(QDBusObjectPath)));
    QDBusConnection::sessionBus().connect(m_serviceBusName,
                                          path,
                                          QStringLiteral("org.freedesktop.Secret.Collection"),
                                          QStringLiteral("ItemCreated"),
                                          this,
                                          SLOT(onSecretItemChanged(QDBusObjectPath)));
    QDBusConnection::sessionBus().connect(m_serviceBusName,
                                          path,
                                          QStringLiteral("org.freedesktop.Secret.Collection"),
                                          QStringLiteral("ItemDeleted"),
                                          this,
                                          SLOT(onSecretItemChanged(QDBusObjectPath)));

    m_watchedCollections.insert(collectionName);
    *ok = true;
}

void SecretServiceClient::onServiceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(serviceName);
    Q_UNUSED(oldOwner);

    bool available = !newOwner.isEmpty();

    m_openCollections.clear();

    if (available && !m_service) {
        GError *error = nullptr;
        m_service = SecretServicePtr(
            secret_service_get_sync(static_cast<SecretServiceFlags>(SECRET_SERVICE_OPEN_SESSION | SECRET_SERVICE_LOAD_COLLECTIONS), nullptr, &error));
        available = wasErrorFree(&error);
    }

    if (!available) {
        m_service.reset();
    }

    qDebug() << "Secret Service availability changed:" << (available ? "Available" : "Unavailable");
    Q_EMIT serviceChanged();
}

void SecretServiceClient::onCollectionCreated(const QDBusObjectPath &path)
{
    const QString label = collectionLabelForPath(path);
    if (label.isEmpty()) {
        return;
    }

    Q_EMIT collectionCreated(label);
    Q_EMIT collectionListDirty();
}

void SecretServiceClient::onCollectionDeleted(const QDBusObjectPath &path)
{
    Q_UNUSED(path);
    if (!attemptConnection()) {
        return;
    }

    // Only emitting collectionListDirty here as we can't know the actual label
    // of the collection as is already deleted
    Q_EMIT collectionListDirty();
}

void SecretServiceClient::onSecretItemChanged(const QDBusObjectPath &path)
{
    if (!attemptConnection()) {
        return;
    }

    // Don't trigger a reload if we did changes ourselves
    if (m_updateInProgress) {
        m_updateInProgress = false;
        return;
    }

    QStringList pieces = path.path().split(QStringLiteral("/"), Qt::SkipEmptyParts);

    // 6 items: /org/freedesktop/secrets/collection/collectionName/itemName
    if (pieces.length() != 6) {
        return;
    }
    pieces.pop_back();
    const QString collectionPath = QStringLiteral("/") % pieces.join(QStringLiteral("/"));

    const QString label = collectionLabelForPath(QDBusObjectPath(collectionPath));
    if (label.isEmpty()) {
        return;
    }

    m_dirtyCollections.insert(label);
    m_collectionDirtyTimer->start();
}

void SecretServiceClient::handlePrompt(bool dismissed)
{
    Q_EMIT promptClosed(!dismissed);
}

bool SecretServiceClient::useKSecretBackend() const
{
    return m_useKSecretBackend;
}

bool SecretServiceClient::isAvailable() const
{
    return m_service != nullptr;
}

bool SecretServiceClient::unlockCollection(const QString &collectionName, bool *ok)
{
    if (!attemptConnection()) {
        *ok = false;
        return false;
    }

    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    if (!collection) {
        *ok = false;
        return false;
    }

    watchCollection(collectionName, ok);

    gboolean locked = secret_collection_get_locked(collection);

    if (locked) {
        gboolean success = secret_service_unlock_sync(m_service.get(), g_list_append(nullptr, collection), nullptr, nullptr, &error);
        *ok = wasErrorFree(&error);
        if (!success) {
            qCWarning(KWALLETD_LOG) << i18n("Unable to unlock collectionName %1", collectionName);
        }
        return success;
    }

    return true;
}

QString SecretServiceClient::defaultCollection(bool *ok)
{
    if (!attemptConnection()) {
        *ok = false;
        return QString();
    }

    QString label = QStringLiteral("kdewallet");

    QDBusInterface serviceInterface(m_serviceBusName,
                                    QStringLiteral("/org/freedesktop/secrets"),
                                    QStringLiteral("org.freedesktop.Secret.Service"),
                                    QDBusConnection::sessionBus());

    if (!serviceInterface.isValid()) {
        qCWarning(KWALLETD_LOG) << "Failed to connect to the DBus SecretService object";
        *ok = false;
        return label;
    }

    QDBusReply<QDBusObjectPath> reply = serviceInterface.call(QStringLiteral("ReadAlias"), QStringLiteral("default"));

    if (!reply.isValid()) {
        qCWarning(KWALLETD_LOG) << "Error reading label:" << reply.error().message();
        *ok = false;
        return label;
    }

    label = collectionLabelForPath(reply.value());

    if (label.isEmpty()) {
        *ok = false;
        return QStringLiteral("kdewallet");
    }

    return label;
}

void SecretServiceClient::setDefaultCollection(const QString &collectionName, bool *ok)
{
    if (!attemptConnection()) {
        *ok = false;
        return;
    }

    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    *ok = secret_service_set_alias_sync(m_service.get(), "default", collection, nullptr, &error);

    *ok = *ok && wasErrorFree(&error);
}

QStringList SecretServiceClient::listCollections(bool *ok)
{
    if (!attemptConnection()) {
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
        qCDebug(KWALLETD_LOG) << i18n("No collections");
    }

    *ok = true;
    return collections;
}

QStringList SecretServiceClient::listFolders(const QString &collectionName, bool *ok)
{
    if (!attemptConnection()) {
        *ok = false;
        return {};
    }

    QSet<QString> folders;

    SecretCollection *collection = retrieveCollection(collectionName);

    if (!collection) {
        *ok = false;
        return {};
    }
    GListPtr glist = GListPtr(secret_collection_get_items(collection));

    if (glist) {
        for (GList *iter = glist.get(); iter != nullptr; iter = iter->next) {
            SecretItem *item = static_cast<SecretItem *>(iter->data);

            GHashTable *attributes = secret_item_get_attributes(item);
            if (attributes) {
                const gchar *value = (const char *)g_hash_table_lookup(attributes, "server");
                if (value) {
                    folders.insert(QString::fromUtf8(value));
                }
            }
        }
    } else {
        qCDebug(KWALLETD_LOG) << i18n("No entries");
    }
    *ok = true;
    return folders.values();
}

QStringList SecretServiceClient::listEntries(const QString &folder, const QString &collectionName, bool *ok)
{
    if (!attemptConnection()) {
        *ok = false;
        return {};
    }

    // TODO: deduplicate
    QSet<QString> items;
    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    if (!collection) {
        *ok = false;
        return {};
    }

    GHashTablePtr attributes = GHashTablePtr(g_hash_table_new(g_str_hash, g_str_equal));
    g_hash_table_insert(attributes.get(), g_strdup("server"), g_strdup(folder.toUtf8().constData()));

    GListPtr glist = GListPtr(secret_collection_search_sync(collection, qtKeychainSchema(), attributes.get(), SECRET_SEARCH_ALL, nullptr, &error));

    *ok = wasErrorFree(&error);
    if (!*ok) {
        return QStringList();
    }

    if (glist) {
        for (GList *iter = glist.get(); iter != nullptr; iter = iter->next) {
            SecretItemPtr item = SecretItemPtr(static_cast<SecretItem *>(iter->data));
            GHashTablePtr attributes = GHashTablePtr(secret_item_get_attributes(item.get()));

            if (attributes) {
                const gchar *value = (const char *)g_hash_table_lookup(attributes.get(), "user");
                if (value) {
                    items.insert(QString::fromUtf8(value));
                }
            }
        }
    } else {
        qCDebug(KWALLETD_LOG) << i18n("No entries");
    }

    return items.values();
}

QHash<QString, QString> SecretServiceClient::readMetadata(const QString &key, const QString &folder, const QString &collectionName, bool *ok)
{
    if (!attemptConnection()) {
        *ok = false;
        return {};
    }

    QHash<QString, QString> hash;

    SecretItemPtr item = retrieveItem(key, Unknown, folder, collectionName, ok);

    if (!item) {
        qCWarning(KWALLETD_LOG) << i18n("Entry not found, key: %1, folder: %2", key, folder);
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
    if (!attemptConnection()) {
        *ok = false;
        return;
    }

    GError *error = nullptr;

    secret_collection_create_sync(m_service.get(), collectionName.toUtf8().data(), nullptr, SECRET_COLLECTION_CREATE_NONE, nullptr, &error);

    *ok = wasErrorFree(&error);
}

void SecretServiceClient::deleteCollection(const QString &collectionName, bool *ok)
{
    if (!attemptConnection()) {
        *ok = false;
        return;
    }

    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    *ok = secret_collection_delete_sync(collection, nullptr, &error);
    m_openCollections.erase(collectionName);
    m_watchedCollections.remove(collectionName);

    *ok = *ok && wasErrorFree(&error);
    if (ok) {
        Q_EMIT collectionDeleted(collectionName);
    }
}

void SecretServiceClient::deleteFolder(const QString &folder, const QString &collectionName, bool *ok)
{
    if (!attemptConnection()) {
        *ok = false;
        return;
    }

    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    GHashTablePtr attributes = GHashTablePtr(g_hash_table_new(g_str_hash, g_str_equal));
    g_hash_table_insert(attributes.get(), g_strdup("server"), g_strdup(folder.toUtf8().constData()));

    GListPtr glist = GListPtr(secret_collection_search_sync(collection, qtKeychainSchema(), attributes.get(), SECRET_SEARCH_ALL, nullptr, &error));

    *ok = wasErrorFree(&error);
    if (!*ok) {
        return;
    }

    if (glist) {
        for (GList *iter = glist.get(); iter != nullptr; iter = iter->next) {
            SecretItem *item = static_cast<SecretItem *>(iter->data);
            m_updateInProgress = true;
            secret_item_delete_sync(item, nullptr, &error);
            *ok = wasErrorFree(&error);
            g_object_unref(item);
        }
    } else {
        qCWarning(KWALLETD_LOG) << i18n("No entries");
    }
}

QByteArray
SecretServiceClient::readEntry(const QString &key, const SecretServiceClient::Type type, const QString &folder, const QString &collectionName, bool *ok)
{
    if (!attemptConnection()) {
        *ok = false;
        return {};
    }

    GError *error = nullptr;
    QByteArray data;

    SecretItemPtr item = retrieveItem(key, type, folder, collectionName, ok);

    if (item) {
        // Some providers like KeepassXC lock each item individually, and need to be
        // unlocked by the user prior being able to access
        if (secret_item_get_locked(item.get())) {
            secret_service_unlock_sync(m_service.get(), g_list_append(nullptr, item.get()), nullptr, nullptr, &error);
            *ok = wasErrorFree(&error);
            if (!ok) {
                qCWarning(KWALLETD_LOG) << i18n("Unable to unlock item");
                return data;
            }

            secret_item_load_secret_sync(item.get(), nullptr, &error);
            *ok = wasErrorFree(&error);
        }

        SecretValuePtr secretValue = SecretValuePtr(secret_item_get_secret(item.get()));

        if (secretValue) {
            if (type == SecretServiceClient::Binary) {
                gsize length = 0;
                const gchar *password = secret_value_get(secretValue.get(), &length);
                return QByteArray(password, length);
            }

            const gchar *password = secret_value_get_text(secretValue.get());
            if (type == SecretServiceClient::Base64) {
                data = QByteArray::fromBase64(QByteArray(password));
            } else {
                data = QByteArray(password);
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
    SecretItemPtr item = retrieveItem(oldKey, Unknown, folder, collectionName, ok);

    if (!*ok) {
        return;
    }
    if (!item) {
        *ok = false;
        qCWarning(KWALLETD_LOG) << i18n("Entry to rename not found");
        return;
    }

    SecretItemPtr existingItem = retrieveItem(newKey, Unknown, folder, collectionName, ok);
    if (existingItem) {
        *ok = false;
        qCWarning(KWALLETD_LOG) << i18n("Entry named %1 in folder %2 and wallet %3 already exists.", newKey, folder, collectionName);
        return;
    }

    QByteArray data;

    Type type = PlainText;
    GHashTablePtr attributes = GHashTablePtr(secret_item_get_attributes(item.get()));
    if (attributes) {
        const gchar *value = (const char *)g_hash_table_lookup(attributes.get(), "type");
        if (value) {
            type = stringToType(QString::fromUtf8(value));
        }
    } else {
        *ok = false;
        qCWarning(KWALLETD_LOG) << i18n("Entry to rename incomplete");
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
        qCWarning(KWALLETD_LOG) << i18n("Entry to rename incomplete");
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
    if (!attemptConnection()) {
        *ok = false;
        return;
    }

    GError *error = nullptr;

    SecretCollection *collection = retrieveCollection(collectionName);

    QByteArray data;
    if (type == SecretServiceClient::Base64) {
        data = value.toBase64();
    } else {
        data = value;
    }

    QString mimeType;
    if (type == SecretServiceClient::Binary) {
        mimeType = QStringLiteral("application/octet-stream");
    } else {
        mimeType = QStringLiteral("text/plain");
    }

    SecretValuePtr secretValue = SecretValuePtr(secret_value_new(data.constData(), -1, mimeType.toLatin1().constData()));
    if (!secretValue) {
        *ok = false;
        qCWarning(KWALLETD_LOG) << i18n("Failed to create SecretValue");
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

    *ok = wasErrorFree(&error);
}

void SecretServiceClient::deleteEntry(const QString &key, const QString &folder, const QString &collectionName, bool *ok)
{
    if (!attemptConnection()) {
        *ok = false;
        return;
    }

    GError *error = nullptr;
    SecretItemPtr item = retrieveItem(key, Unknown, folder, collectionName, ok);
    if (!*ok) {
        return;
    }
    if (!item) {
        *ok = false;
        qCWarning(KWALLETD_LOG) << i18n("Entry to rename not found");
        return;
    }
    m_updateInProgress = true;
    secret_item_delete_sync(item.get(), nullptr, &error);
    *ok = wasErrorFree(&error);
}

#include <moc_secretserviceclient.cpp>
