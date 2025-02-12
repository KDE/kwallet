/*
    SPDX-FileCopyrightText: 2024 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kwalletd.h"

#include <QDBusConnection>
#include <QDebug>

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPasswordDialog>

#include "kwalletadaptor.h"
#include "secretserviceclient.h"

#include "../kwalletbackend/kwalletbackend.h"
#include "../kwalletbackend/kwalletentry.h"

unsigned int KWalletD::s_lastTransaction = 0;

static void startManagerForKwalletd()
{
    if (!QStandardPaths::findExecutable(QStringLiteral("kstart")).isEmpty()) {
        QProcess::startDetached(QStringLiteral("kstart"), {QStringLiteral("kwalletmanager5"), QStringLiteral("--"), QStringLiteral("--kwalletd")});
    } else if (!QStandardPaths::findExecutable(QStringLiteral("kstart5")).isEmpty()) {
        QProcess::startDetached(QStringLiteral("kstart"), {QStringLiteral("kwalletmanager5"), QStringLiteral("--"), QStringLiteral("--kwalletd")});
    } else {
        QProcess::startDetached(QStringLiteral("kwalletmanager5"), QStringList{QStringLiteral("--kwalletd")});
    }
}

KWalletD::KWalletD(bool useKWalletBackend, QObject *parent)
    : QObject(parent)
    , m_backend(new SecretServiceClient(useKWalletBackend, this))
    , m_useKWalletBackend(useKWalletBackend)
{
    new KWalletAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/modules/kwalletd5"), this);
    dbus.registerObject(QStringLiteral("/modules/kwalletd6"), this);
    dbus.registerService(QStringLiteral("org.kde.kwalletd5"));
    dbus.registerService(QStringLiteral("org.kde.kwalletd6"));

    auto readStructure = [this]() {
        bool ok;
        for (const QString &wallet : m_backend->listCollections(&ok)) {
            for (const QString &folder : m_backend->listFolders(wallet, &ok)) {
                m_structure.insert(wallet, folder);
            }
        }
        qWarning() << "Structure:" << m_structure;
        qWarning() << "Default wallet:" << m_backend->defaultCollection(&ok);

        if (!m_useKWalletBackend) {
            migrateData();
        }
    };
    readStructure();

    connect(m_backend, &SecretServiceClient::serviceAvailableChanged, this, [this, readStructure](bool available) {
        if (available) {
            readStructure();
            Q_EMIT walletListDirty();
        } else {
            closeAllWallets();
        }
    });

    connect(m_backend, &SecretServiceClient::collectionDirty, this, [this](const QString &wallet) {
        bool ok;
        QStringList folders = m_structure.values(wallet);
        QSet<QString> oldFolders(folders.constBegin(), folders.constEnd());
        folders = m_backend->listFolders(wallet, &ok);
        QSet<QString> newFolders(folders.constBegin(), folders.constEnd());

        // Reload m_structure
        m_structure.clear();
        for (const QString &wallet : m_backend->listCollections(&ok)) {
            for (const QString &folder : m_backend->listFolders(wallet, &ok)) {
                m_structure.insert(wallet, folder);
            }
        }

        Q_EMIT folderListUpdated(wallet);
        // We are not sure which folder or item has been updated, reload all folders of all wallets
        for (const QString &folder : m_structure.values(wallet)) {
            Q_EMIT folderUpdated(folder, wallet);
        }
    });

    reconfigure();

    m_configWatcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kwalletrc")));
    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, [this]() {
        bool ok = false;
        KConfig cfg(QStringLiteral("kwalletrc"));
        KConfigGroup walletGroup(&cfg, QStringLiteral("Wallet"));
        const QString defaultCollection = m_backend->defaultCollection(&ok);
        const QString newDefaultWallet = walletGroup.readEntry(QStringLiteral("Default Wallet"), defaultCollection);

        if (newDefaultWallet != defaultCollection) {
            if (m_backend->unlockCollection(newDefaultWallet, &ok)) {
                m_backend->setDefaultCollection(newDefaultWallet, &ok);
            }
        }
    });
}

KWalletD::~KWalletD()
{
}

bool KWalletD::migrateWallet(const QString &sourceWallet, const QString &destWallet)
{
    qWarning() << "Migrating" << sourceWallet;
    KWallet::Backend backend(sourceWallet, sourceWallet.endsWith(QStringLiteral(".kwl")));
    backend.open(QByteArray());
    while (!backend.isOpen()) {
        KPasswordDialog dlg;
        dlg.setPrompt(i18n("Migrating the wallet \"%1\". Please provide the password to unlock.", sourceWallet));
        dlg.setRevealPasswordMode(KPassword::RevealMode::OnlyNew);

        if (dlg.exec() == QDialog::Accepted) {
            backend.open(dlg.password().toUtf8());
            if (backend.isOpen()) {
                break;
            }
        } else {
            break;
        }
    }
    if (!backend.isOpen()) {
        return false;
    }

    bool ok = false;

    const QStringList allDestWallets = m_backend->listCollections(&ok);
    if (!ok) {
        return false;
    }
    if (!allDestWallets.contains(destWallet)) {
        qWarning() << "Creating" << destWallet;
        m_backend->createCollection(destWallet, &ok);
        if (!ok) {
            return false;
        }
    }
    qWarning() << "Unlocking" << destWallet;
    bool unlocked = m_backend->unlockCollection(destWallet, &ok);
    if (!ok || !unlocked) {
        return false;
    }

    for (const QString &folder : backend.folderList()) {
        backend.setFolder(folder);
        for (KWallet::Entry *entry : backend.entriesList()) {
            switch (entry->type()) {
            case KWallet::Wallet::Password:
                writeString(entry->key(), entry->password(), folder, destWallet, &ok);
                break;
            case KWallet::Wallet::Stream:
                writeBinary(entry->key(), entry->value(), folder, destWallet, &ok);
                break;
            case KWallet::Wallet::Map: {
                QByteArray mapData = entry->map();
                if (!mapData.isEmpty()) {
                    QDataStream ds(&mapData, QIODevice::ReadOnly);
                    QMap<QString, QString> v;
                    ds >> v;
                    QJsonObject obj;
                    for (auto it = v.constBegin(); it != v.constEnd(); it++) {
                        obj[it.key()] = it.value();
                    }

                    writeRawJson(entry->key(), QJsonDocument(obj).toJson(QJsonDocument::Compact), folder, destWallet, &ok);
                }
                break;
            }
            default:
                break;
            }
        }
    }
    return true;
}

void KWalletD::migrateData()
{
    if (!m_backend->isAvailable() || m_useKWalletBackend) {
        return;
    }
    KConfig cfg(QStringLiteral("kwalletrc"));
    KConfigGroup walletGroup(&cfg, QStringLiteral("Wallet"));
    KConfigGroup migrationGroup(&cfg, QStringLiteral("Migration"));
    QStringList walletsMigrated = migrationGroup.readEntry(QStringLiteral("WalletsMigratedToSecretService"), QStringList());

    // Get the list of all known kwallets
    const QString path = KWallet::Backend::getSaveLocation();
    QDir dir(path, QStringLiteral("*.kwl"));
    QStringList wallets;

    dir.setFilter(QDir::Files | QDir::Hidden);

    const auto list = dir.entryInfoList();
    // This code to list wallets comed from original kwalletd, to be use the
    // list is the same
    for (const QFileInfo &fi : list) {
        QString fn = fi.fileName();
        if (fn.endsWith(QLatin1String(".kwl"))) {
            fn.truncate(fn.length() - 4);
        }
        wallets += KWallet::Backend::decodeWalletName(fn);
    }

    bool ok = false;
    const QString oldDefaultWallet = walletGroup.readEntry(QStringLiteral("Default Wallet"), QStringLiteral("kdewallet"));
    for (const QString &sourceWallet : std::as_const(wallets)) {
        QString destWallet = sourceWallet;
        // Use the SecretService default collection as the new default wallet
        if (sourceWallet == oldDefaultWallet) {
            destWallet = m_backend->defaultCollection(&ok);
        }
        if (walletsMigrated.contains(sourceWallet)) {
            continue;
        }
        if (migrateWallet(sourceWallet, destWallet)) {
            walletsMigrated << sourceWallet;
        }
    }

    // The default wallet will always be the one Secret Service says
    walletGroup.writeEntry(QStringLiteral("Default Wallet"), m_backend->defaultCollection(&ok));

    migrationGroup.writeEntry(QStringLiteral("WalletsMigratedToSecretService"), walletsMigrated);
    cfg.sync();
}

QString KWalletD::walletForHandle(int handle, const QString &appId)
{
    // This is called by all accessing functions, so reset the idle timer here
    if (m_closeIdle) {
        const QPair<int, QString> key(handle, appId);
        killTimer(m_idleTimers[key]);
        m_idleTimers[key] = startTimer(m_idleTime);
    }

    const QString walletName = m_openWallets.value(QPair<int, QString>(handle, appId));
    if (!walletName.isEmpty()) {
        bool ok;
        bool unlocked = m_backend->unlockCollection(walletName, &ok);
        if (!ok || !unlocked) {
            return QString();
        }
    }

    return walletName;
}

QString KWalletD::folderPath(const QString &folder, const QString &key) const
{
    return folder % QStringLiteral("/") % key;
}

KWalletD::EntryType KWalletD::keyType(const QString &wallet, const QString &folder, const QString &key)
{
    bool ok;
    const QHash<QString, QString> hash = m_backend->readMetadata(key, folder, wallet, &ok);

    if (!ok) {
        return Unknown;
    }

    const QString typeStr = hash.value(QStringLiteral("type"));

    if (typeStr == QStringLiteral("map")) {
        return Map;
    } else if (typeStr == QStringLiteral("base64")) {
        return Stream;
    } else {
        return Password;
    }
}

QString KWalletD::readString(const QString &key, const QString &folder, const QString &wallet, bool *ok)
{
    return QString::fromUtf8(m_backend->readEntry(key, SecretServiceClient::PlainText, folder, wallet, ok));
}

QByteArray KWalletD::readRawJson(const QString &key, const QString &folder, const QString &wallet, bool *ok)
{
    return m_backend->readEntry(key, SecretServiceClient::Map, folder, wallet, ok);
}

QByteArray KWalletD::readBinary(const QString &key, const QString &folder, const QString &wallet, bool *ok)
{
    return m_backend->readEntry(key, SecretServiceClient::Binary, folder, wallet, ok);
}

void KWalletD::writeString(const QString &key, const QString &value, const QString &folder, const QString &wallet, bool *ok)
{
    m_backend->writeEntry(folderPath(folder, key), key, value.toUtf8(), SecretServiceClient::PlainText, folder, wallet, ok);
}

void KWalletD::writeBinary(const QString &key, const QByteArray &value, const QString &folder, const QString &wallet, bool *ok)
{
    m_backend->writeEntry(folderPath(folder, key), key, value, SecretServiceClient::Binary, folder, wallet, ok);
}

void KWalletD::writeRawJson(const QString &key, const QByteArray &value, const QString &folder, const QString &wallet, bool *ok)
{
    m_backend->writeEntry(folderPath(folder, key), key, value, SecretServiceClient::Map, folder, wallet, ok);
}

void KWalletD::removeItem(const QString &key, const QString &folder, const QString &wallet, bool *ok)
{
    m_backend->deleteEntry(key, folder, wallet, ok);
}

void KWalletD::timerEvent(QTimerEvent *ev)
{
    const int timer = ev->timerId();

    int handle;
    QString appId;

    for (auto it = m_idleTimers.constBegin(); it != m_idleTimers.end(); it++) {
        if (it.value() == timer) {
            handle = it.key().first;
            appId = it.key().second;
            break;
        }
    }
    killTimer(timer);

    close(handle, true, appId);
}

// KWallet API

bool KWalletD::isEnabled() const
{
    return m_enabled && m_backend->isAvailable();
}

int KWalletD::open(const QString &wallet, qlonglong wId, const QString &appId)
{
    if (wId <= 0) {
        return -1;
    }

    bool ok;
    const QStringList wallets = m_backend->listCollections(&ok);
    if (!ok) {
        return -1;
    }
    if (!wallets.contains(wallet)) {
        m_backend->createCollection(wallet, &ok);
        if (!ok) {
            return -1;
        }
    }

    bool unlocked = m_backend->unlockCollection(wallet, &ok);
    if (!ok || !unlocked) {
        return -1;
    }

    for (const QString &folder : m_backend->listFolders(wallet, &ok)) {
        m_structure.insert(wallet, folder);
    }

    QRandomGenerator rand = QRandomGenerator::securelySeeded();
    const int rnd = std::abs(int(rand.generate()));

    m_openWallets[QPair<int, QString>(rnd, appId)] = wallet;

    if (m_closeIdle) {
        m_idleTimers[QPair<int, QString>(rnd, appId)] = startTimer(m_idleTime);
    }

    Q_EMIT walletOpened(wallet);
    return rnd;
}

int KWalletD::openPath(const QString &path, qlonglong wId, const QString &appId)
{
    // This is intended to just be a stub, so it will always fail
    Q_UNUSED(path);
    Q_UNUSED(wId);
    Q_UNUSED(appId);
    return -1;
}

int KWalletD::openAsync(const QString &wallet, qlonglong wId, const QString &appId, bool handleSession)
{
    Q_UNUSED(handleSession);

    if (wId <= 0) {
        return -1;
    }

    QRandomGenerator rand = QRandomGenerator::securelySeeded();
    const int rnd = std::abs(int(rand.generate()));
    const int tid = ++s_lastTransaction;

    if (m_closeIdle) {
        m_idleTimers[QPair<int, QString>(rnd, appId)] = startTimer(m_idleTime);
    }

    QTimer::singleShot(0, [this, wallet, appId, tid, rnd]() {
        bool ok;
        const QStringList wallets = m_backend->listCollections(&ok);
        if (!ok) {
            return;
        }
        if (!wallets.contains(wallet)) {
            m_backend->createCollection(wallet, &ok);
            if (!ok) {
                return;
            }
        }
        bool unlocked = m_backend->unlockCollection(wallet, &ok);
        if (!ok || !unlocked) {
            return;
        }

        for (const QString &folder : m_backend->listFolders(wallet, &ok)) {
            m_structure.insert(wallet, folder);
        }
        m_openWallets[QPair<int, QString>(rnd, appId)] = wallet;
        Q_EMIT walletAsyncOpened(tid, rnd);
    });

    return tid;
}

int KWalletD::openPathAsync(const QString &path, qlonglong wId, const QString &appId, bool handleSession)
{
    // This is intended to just be a stub, so it will always fail
    Q_UNUSED(path);
    Q_UNUSED(wId);
    Q_UNUSED(appId);
    Q_UNUSED(handleSession);
    return -1;
}

int KWalletD::close(const QString &wallet, bool force)
{
    int ret = 0;
    QSet<QString> apps;
    auto it = m_openWallets.begin();

    while (it != m_openWallets.end()) {
        if (it.value() == wallet) {
            if (force) {
                apps << it.key().second;
                it = m_openWallets.erase(it);
            } else {
                ret = 1;
                ++it;
            }
        } else {
            ++it;
        }
    }

    if (ret == 0) {
        Q_EMIT walletClosed(wallet);
        if (m_openWallets.isEmpty()) {
            Q_EMIT allWalletsClosed();
        }
        QSet<QString> newApps;
        for (auto it = m_openWallets.constBegin(); it != m_openWallets.constEnd(); it++) {
            newApps << it.key().second;
        }
        QSet<QString> disconnected = apps.subtract(newApps);
        for (const QString &appId : std::as_const(disconnected)) {
            Q_EMIT applicationDisconnected(wallet, appId);
        }
    }

    return ret;
}

int KWalletD::close(int handle, bool force, const QString &appId)
{
    Q_UNUSED(force);
    const QString wallet = walletForHandle(handle, appId);
    int ret = 0;
    auto it = m_openWallets.begin();

    while (it != m_openWallets.end()) {
        if (it.key().first == handle && it.key().second == appId) {
            it = m_openWallets.erase(it);
        } else {
            ++it;
        }
    }

    if (ret == 0) {
        Q_EMIT walletClosed(handle);
        Q_EMIT walletClosedId(handle);
        if (m_openWallets.isEmpty()) {
            Q_EMIT allWalletsClosed();
        }
        QSet<QString> newApps;
        for (auto it = m_openWallets.constBegin(); it != m_openWallets.constEnd(); it++) {
            newApps << it.key().second;
        }
        if (!newApps.contains(appId)) {
            Q_EMIT applicationDisconnected(wallet, appId);
        }
    }

    return ret;
}

void KWalletD::sync(int handle, const QString &appId)
{
    // STUB
    Q_UNUSED(handle);
    Q_UNUSED(appId);
}

int KWalletD::deleteWallet(const QString &wallet)
{
    bool ok;
    m_backend->deleteCollection(wallet, &ok);

    if (ok) {
        m_structure.remove(wallet);
        return 0;
    }
    return -1;
}

bool KWalletD::isOpen(const QString &wallet)
{
    for (const QString &w : std::as_const(m_openWallets)) {
        if (w == wallet) {
            return true;
        }
    }
    return false;
}

bool KWalletD::isOpen(int handle)
{
    for (auto it = m_openWallets.constBegin(); it != m_openWallets.constEnd(); it++) {
        if (it.key().first == handle) {
            return true;
        }
    }
    return false;
}

QStringList KWalletD::users(const QString &wallet) const
{
    QSet<QString> allApps;

    for (auto it = m_openWallets.constBegin(); it != m_openWallets.constEnd(); it++) {
        if (it.value() == wallet) {
            allApps.insert(it.key().second);
        }
    }

    return allApps.values();
}

void KWalletD::changePassword(const QString &wallet, qlonglong wId, const QString &appId)
{
    if (!m_useKWalletBackend) {
        return;
    }

    QDBusInterface legacyKWalletInterface(QStringLiteral("org.kde.ksecretd"),
                                          QStringLiteral("/ksecretd"),
                                          QStringLiteral("org.kde.KWallet"),
                                          QDBusConnection::sessionBus());

    if (!legacyKWalletInterface.isValid()) {
        return;
    }

    legacyKWalletInterface.asyncCall(QStringLiteral("changePassword"), wallet, wId, appId);
}

QStringList KWalletD::wallets() const
{
    bool ok;
    return m_backend->listCollections(&ok);
}

QStringList KWalletD::folderList(int handle, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return QStringList();
    }

    return m_structure.values(wallet);
}

bool KWalletD::hasFolder(int handle, const QString &folder, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return false;
    }
    for (auto it = m_structure.constBegin(); it != m_structure.constEnd(); it++) {
        if (it.value().contains(folder)) {
            return true;
        }
    }
    return m_structure.contains(folder);
}

bool KWalletD::createFolder(int handle, const QString &folder, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return false;
    }

    // We can't save it on disk: if no key will be written,
    // after daemon restart this folder will be gone
    m_structure.insert(wallet, folder);
    return true;
}

bool KWalletD::removeFolder(int handle, const QString &folder, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return false;
    }

    bool ok;
    m_backend->deleteFolder(folder, wallet, &ok);
    if (ok) {
        m_structure.remove(wallet, folder);
    }
    return ok;
}

QStringList KWalletD::entryList(int handle, const QString &folder, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return QStringList();
    }

    bool ok;
    return m_backend->listEntries(folder, wallet, &ok);
}

QByteArray KWalletD::readEntry(int handle, const QString &folder, const QString &key, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return QByteArray();
    }

    bool ok;
    EntryType type = keyType(wallet, folder, key);

    switch (type) {
    case Stream:
        return readBinary(key, folder, wallet, &ok);
    case Map:
        return readMap(handle, folder, key, appId);
    default:
        return readString(key, folder, wallet, &ok).toUtf8();
    }
}

QByteArray KWalletD::readMap(int handle, const QString &folder, const QString &key, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return QByteArray();
    }

    bool ok;

    QJsonObject obj = QJsonDocument::fromJson(readRawJson(key, folder, wallet, &ok)).object();
    if (!ok) {
        return QByteArray();
    }

    QMap<QString, QString> map;
    for (auto it = obj.constBegin(); it != obj.constEnd(); it++) {
        map[it.key()] = it.value().toString();
    }

    QByteArray data;
    QDataStream ds(&data, QIODevice::WriteOnly);
    ds << map;

    return data;
}

QString KWalletD::readPassword(int handle, const QString &folder, const QString &key, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return QString();
    }

    bool ok;
    return readString(key, folder, wallet, &ok);
}

QVariantMap KWalletD::readEntryList(int handle, const QString &folder, const QString &key, const QString &appId)
{
    QVariantMap map;
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return map;
    }

    bool ok;

    EntryType type = keyType(wallet, folder, key);
    switch (type) {
    case Stream:
        map[key] = readBinary(key, folder, wallet, &ok);
        break;
    case Map:
        map[key] = readMapList(handle, folder, key, appId);
        break;
    default:
        map[key] = readString(key, folder, wallet, &ok).toUtf8();
    }

    return map;
}

QVariantMap KWalletD::entriesList(int handle, const QString &folder, const QString &appId)
{
    QVariantMap map;
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return map;
    }

    bool ok;

    const QStringList keys = m_backend->listEntries(folder, wallet, &ok);
    if (!ok) {
        return map;
    }

    for (const QString &key : std::as_const(keys)) {
        EntryType type = keyType(wallet, folder, key);
        switch (type) {
        case Stream:
            map[key] = readBinary(key, folder, wallet, &ok);
            break;
        case Map:
            map[key] = readMap(handle, folder, key, appId);
            break;
        default:
            map[key] = readString(key, folder, wallet, &ok).toUtf8();
        }
        if (!ok) {
            return QVariantMap();
        }
    }

    return map;
}

QVariantMap KWalletD::readMapList(int handle, const QString &folder, const QString &key, const QString &appId)
{
    QVariantMap map;
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return map;
    }

    bool ok;

    EntryType type = keyType(wallet, folder, key);
    if (type != Map) {
        return map;
    }

    QJsonObject obj = QJsonDocument::fromJson(readRawJson(key, folder, wallet, &ok)).object();
    if (!ok) {
        return map;
    }

    for (auto it = obj.constBegin(); it != obj.constEnd(); it++) {
        map[it.key()] = it.value().toString();
    }

    return map;
}

QVariantMap KWalletD::mapList(int handle, const QString &folder, const QString &appId)
{
    QVariantMap map;
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return map;
    }

    bool ok;

    const QStringList keys = m_backend->listEntries(folder, wallet, &ok);
    if (!ok) {
        return map;
    }

    for (const QString &key : std::as_const(keys)) {
        EntryType type = keyType(wallet, folder, key);
        if (type == Map) {
            map[key] = readMap(handle, folder, key, appId);
        }
    }

    return map;
}

QVariantMap KWalletD::readPasswordList(int handle, const QString &folder, const QString &key, const QString &appId)
{
    QVariantMap map;
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return map;
    }

    bool ok;

    EntryType type = keyType(wallet, folder, key);
    if (type == Password) {
        map[key] = readString(key, folder, wallet, &ok);
        if (!ok) {
            return QVariantMap();
        }
    }

    return map;
}

QVariantMap KWalletD::passwordList(int handle, const QString &folder, const QString &appId)
{
    QVariantMap map;
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return map;
    }

    bool ok;

    const QStringList keys = m_backend->listEntries(folder, wallet, &ok);
    if (!ok) {
        return map;
    }

    for (const QString &key : std::as_const(keys)) {
        EntryType type = keyType(wallet, folder, key);
        if (type == Password) {
            map[key] = readString(key, folder, wallet, &ok);
            if (!ok) {
                return QVariantMap();
            }
        }
    }

    return map;
}

int KWalletD::renameEntry(int handle, const QString &folder, const QString &oldName, const QString &newName, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return -1;
    }

    bool ok;
    m_backend->renameEntry(folderPath(folder, newName), oldName, newName, folder, wallet, &ok);

    if (!ok) {
        return -1;
    }

    return 0;
}

int KWalletD::writeEntry(int handle, const QString &folder, const QString &key, const QByteArray &value, int entryType, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);

    if (wallet.isEmpty()) {
        return -1;
    }

    bool ok;
    QStringList oldWallets = m_backend->listCollections(&ok);
    QStringList oldFolders = m_backend->listFolders(wallet, &ok);
    QStringList oldEntries = m_backend->listEntries(wallet, folder, &ok);

    switch (entryType) {
    case Password:
        writeString(key, QString::fromUtf8(value), folder, wallet, &ok);
        break;
    case Stream:
        writeBinary(key, value, folder, wallet, &ok);
        break;
    case Map: {
        QMap<QString, QString> map;
        QByteArray data;
        QDataStream ds(value);
        ds >> map;

        QJsonObject obj;
        for (auto it = map.constBegin(); it != map.constEnd(); it++) {
            obj.insert(it.key(), it.value());
        }

        writeRawJson(key, QJsonDocument(obj).toJson(QJsonDocument::Compact), folder, wallet, &ok);
        break;
    }
    default:
        return -1;
    }

    if (ok) {
        if (!oldWallets.contains(wallet)) {
            Q_EMIT walletCreated(wallet);
            Q_EMIT walletListDirty();
        }
        if (!oldFolders.contains(folder)) {
            m_structure.insert(wallet, folder);
            Q_EMIT folderListUpdated(wallet);
        }
        if (!oldEntries.contains(key)) {
            Q_EMIT folderUpdated(wallet, folder);
        } else {
            Q_EMIT entryUpdated(wallet, folder, key);
        }
        return 0;
    }

    return -1;

    switch (entryType) {
    case Password:
        return writePassword(handle, folder, key, QString::fromUtf8(value), appId);
    case Stream:
        return writeEntry(handle, folder, key, value, appId);
    case Map:
        return writeMap(handle, folder, key, value, appId);
    default:
        return -1;
    }
}

int KWalletD::writeEntry(int handle, const QString &folder, const QString &key, const QByteArray &value, const QString &appId)
{
    return writeEntry(handle, folder, key, value, Stream, appId);
}

int KWalletD::writeMap(int handle, const QString &folder, const QString &key, const QByteArray &value, const QString &appId)
{
    return writeEntry(handle, folder, key, value, Map, appId);
}

int KWalletD::writePassword(int handle, const QString &folder, const QString &key, const QString &value, const QString &appId)
{
    return writeEntry(handle, folder, key, value.toUtf8(), Password, appId);
}

bool KWalletD::hasEntry(int handle, const QString &folder, const QString &key, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return false;
    }

    bool ok;
    const QStringList entries = m_backend->listEntries(folder, wallet, &ok);

    if (!ok) {
        return false;
    }

    return entries.contains(key);
}

int KWalletD::entryType(int handle, const QString &folder, const QString &key, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return Unknown;
    }

    return keyType(wallet, folder, key);
}

int KWalletD::removeEntry(int handle, const QString &folder, const QString &key, const QString &appId)
{
    const QString wallet = walletForHandle(handle, appId);
    if (wallet.isEmpty()) {
        return -1;
    }

    bool ok;
    removeItem(key, folder, wallet, &ok);
    if (ok) {
        QStringList folders = m_backend->listFolders(wallet, &ok);
        if (ok && !folders.contains(folder)) {
            m_structure.remove(wallet, folder);
            folderListUpdated(wallet);
        }

        Q_EMIT entryDeleted(wallet, folder, key);
        return 0;
    }

    return -1;
}

bool KWalletD::disconnectApplication(const QString &wallet, const QString &application)
{
    bool found = false;
    auto it = m_openWallets.begin();

    while (it != m_openWallets.end()) {
        if (it.key().second == application && it.value() == wallet) {
            found = true;
            it = m_openWallets.erase(it);
        } else {
            ++it;
        }
    }

    if (found) {
        Q_EMIT walletClosed(wallet);
        if (m_openWallets.isEmpty()) {
            Q_EMIT allWalletsClosed();
        }

        Q_EMIT applicationDisconnected(wallet, application);
    }

    return false;
}

void KWalletD::reconfigure()
{
    KConfig cfg(QStringLiteral("kwalletrc"));
    KConfigGroup walletGroup(&cfg, QStringLiteral("Wallet"));
    const bool wasEnabled = m_enabled;
    m_enabled = walletGroup.readEntry("Enabled", true);
    m_launchManager = walletGroup.readEntry("Launch Manager", false);
    const bool closeIdle = m_closeIdle;
    m_closeIdle = walletGroup.readEntry("Close When Idle", false);
    const int idleTime = m_idleTime;
    // in minutes!
    m_idleTime = walletGroup.readEntry("Idle Timeout", 10) * 60 * 1000;

    if (wasEnabled != m_enabled) {
        if (isEnabled()) {
            Q_EMIT walletListDirty();
        } else {
            closeAllWallets();
        }
    }

    if (closeIdle != m_closeIdle) {
        if (m_closeIdle) {
            for (auto it = m_openWallets.constBegin(); it != m_openWallets.constEnd(); it++) {
                m_idleTimers[it.key()] = startTimer(m_idleTime);
            }
        } else {
            for (auto it = m_idleTimers.constBegin(); it != m_idleTimers.constEnd(); it++) {
                killTimer(it.value());
            }
            m_idleTimers.clear();
        }
    } else if (idleTime != m_idleTime) {
        for (auto it = m_idleTimers.begin(); it != m_idleTimers.constEnd(); it++) {
            killTimer(it.value());
            *it = (startTimer(m_idleTime));
        }
    }

    if (m_launchManager) {
        startManagerForKwalletd();
    }
}

bool KWalletD::folderDoesNotExist(const QString &wallet, const QString &folder)
{
    bool ok;
    const QStringList entries = m_backend->listFolders(wallet, &ok);

    if (!ok) {
        return true;
    }

    return !entries.contains(folder);
}

bool KWalletD::keyDoesNotExist(const QString &wallet, const QString &folder, const QString &key)
{
    bool ok;
    const QStringList entries = m_backend->listEntries(folder, wallet, &ok);

    if (!ok) {
        return true;
    }

    return !entries.contains(key);
}

void KWalletD::closeAllWallets()
{
    QSet<QString> openWallets;
    QSet<QPair<QString, QString>> apps;

    for (auto it = m_openWallets.constBegin(); it != m_openWallets.constEnd(); it++) {
        openWallets.insert(it.value());
        apps.insert({it.value(), it.key().second});
    }

    m_openWallets.clear();
    for (const QPair<QString, QString> &connection : apps.values()) {
        Q_EMIT applicationDisconnected(connection.first, connection.second);
    }
    for (const QString &wallet : openWallets.values()) {
        Q_EMIT walletClosed(wallet);
    }
    Q_EMIT allWalletsClosed();
}

QString KWalletD::networkWallet()
{
    bool ok;
    const QString defaultWallet = m_backend->defaultCollection(&ok);

    KConfig cfg(QStringLiteral("kwalletrc"));
    KConfigGroup walletGroup(&cfg, QStringLiteral("Wallet"));
    return walletGroup.readEntry(QStringLiteral("Default Wallet"), defaultWallet);
}

QString KWalletD::localWallet()
{
    KConfig cfg(QStringLiteral("kwalletrc"));
    KConfigGroup walletGroup(&cfg, QStringLiteral("Wallet"));
    return walletGroup.readEntry(QStringLiteral("Local Wallet"), networkWallet());
}

int KWalletD::pamOpen(const QString &wallet, const QByteArray &passwordHash, int sessionTimeout)
{
    // STUB
    Q_UNUSED(wallet);
    Q_UNUSED(passwordHash);
    Q_UNUSED(sessionTimeout);
    return -1;
}

#include "moc_kwalletd.cpp"
