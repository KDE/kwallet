/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 Slava Aseev <nullptrnine@basealt.ru>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "kwalletfreedesktopsession.h"

#include "kwalletfreedesktopsessionadaptor.h"
#include <QDBusConnection>

KWalletFreedesktopSession::KWalletFreedesktopSession(KWalletFreedesktopService *service,
                                                     const QCA::PublicKey &publicKey,
                                                     QCA::SymmetricKey symmetricKey,
                                                     QString sessionPath,
                                                     const QDBusConnection &connection,
                                                     const QDBusMessage &message)
    : m_service(service)
    , m_publicKey(publicKey)
    , m_symmetricKey(std::move(symmetricKey))
    , m_sessionPath(std::move(sessionPath))
    , m_serviceBusName(message.service())
{
    (void)new KWalletFreedesktopSessionAdaptor(this);
    QDBusConnection::sessionBus().registerObject(m_sessionPath, this);

    m_serviceWatcher.setConnection(connection);
    m_serviceWatcher.addWatchedService(m_serviceBusName);
    m_serviceWatcher.setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    connect(&m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &KWalletFreedesktopSession::slotServiceOwnerChanged);
}

void KWalletFreedesktopSession::slotServiceOwnerChanged(const QString &, const QString &, const QString &)
{
    fdoService()->deleteSession(m_sessionPath);
}

void KWalletFreedesktopSession::Close()
{
    if (message().service() != m_serviceBusName) {
        sendErrorReply(QDBusError::ErrorType::UnknownObject, QStringLiteral("Can't find session ") + m_sessionPath);
    } else {
        fdoService()->deleteSession(m_sessionPath);
    }
}

CipherResult KWalletFreedesktopSession::encrypt(const QDBusMessage &message, const QCA::SecureArray &bytes, const QCA::SecureArray &initVector) const
{
    if (message.service() != m_serviceBusName) {
        return {false, QByteArray()};
    }

    auto cipher =
        QCA::Cipher(QStringLiteral("aes128"), QCA::Cipher::CBC, QCA::Cipher::PKCS7, QCA::Encode, m_symmetricKey, QCA::InitializationVector(initVector));
    QCA::SecureArray result;
    result.append(cipher.update(QCA::MemoryRegion(bytes)));
    if (cipher.ok()) {
        result.append(cipher.final());
    }

    return {cipher.ok(), std::move(result)};
}

CipherResult KWalletFreedesktopSession::decrypt(const QDBusMessage &message, const QCA::SecureArray &bytes, const QCA::SecureArray &initVector) const
{
    if (message.service() != m_serviceBusName) {
        return {false, QByteArray()};
    }

    auto cipher =
        QCA::Cipher(QStringLiteral("aes128"), QCA::Cipher::CBC, QCA::Cipher::PKCS7, QCA::Decode, m_symmetricKey, QCA::InitializationVector(initVector));
    QCA::SecureArray result;
    result.append(cipher.update(QCA::MemoryRegion(bytes)));
    if (cipher.ok()) {
        result.append(cipher.final());
    }

    return {cipher.ok(), std::move(result)};
}

KWalletFreedesktopService *KWalletFreedesktopSession::fdoService() const
{
    return m_service;
}

KWalletD *KWalletFreedesktopSession::backend() const
{
    return fdoService()->backend();
}

QDBusObjectPath KWalletFreedesktopSession::fdoObjectPath() const
{
    return QDBusObjectPath(m_sessionPath);
}
