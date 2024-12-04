/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 Slava Aseev <nullptrnine@basealt.ru>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "kwalletfreedesktopsession.h"

#include "ksecretd_debug.h"
#include "kwalletfreedesktopsessionadaptor.h"
#include <QDBusConnection>

#include <botan/block_cipher.h>
#include <botan/cipher_mode.h>
#include <botan/filters.h>
#include <botan/hex.h>
#include <botan/pipe.h>
#include <botan/system_rng.h>

KWalletFreedesktopSession::KWalletFreedesktopSession(KWalletFreedesktopService *service,
                                                     std::unique_ptr<KWalletFreedesktopSessionAlgorithm> algorithm,
                                                     QString sessionPath,
                                                     const QDBusConnection &connection,
                                                     const QDBusMessage &message)
    : m_service(service)
    , m_algorithm(std::move(algorithm))
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

QByteArray KWalletFreedesktopSession::negotiationOutput() const
{
    return m_algorithm->negotiationOutput();
}

bool KWalletFreedesktopSession::encrypt(const QDBusMessage &message, FreedesktopSecret &secret) const
{
    if (message.service() != m_serviceBusName) {
        return false;
    }

    return m_algorithm->encrypt(secret);
}

bool KWalletFreedesktopSession::decrypt(const QDBusMessage &message, FreedesktopSecret &secret) const
{
    if (message.service() != m_serviceBusName) {
        return false;
    }

    return m_algorithm->decrypt(secret);
}

KWalletFreedesktopService *KWalletFreedesktopSession::fdoService() const
{
    return m_service;
}

KSecretD *KWalletFreedesktopSession::backend() const
{
    return fdoService()->backend();
}

QDBusObjectPath KWalletFreedesktopSession::fdoObjectPath() const
{
    return QDBusObjectPath(m_sessionPath);
}

QByteArray KWalletFreedesktopSessionAlgorithmPlain::negotiationOutput() const
{
    return QByteArray();
}

bool KWalletFreedesktopSessionAlgorithmPlain::encrypt(FreedesktopSecret &secret) const
{
    secret.parameters = QByteArray();
    return true;
}

bool KWalletFreedesktopSessionAlgorithmPlain::decrypt(FreedesktopSecret &) const
{
    return true;
}

KWalletFreedesktopSessionAlgorithmDhAes::KWalletFreedesktopSessionAlgorithmDhAes(const QByteArray &publicKey, const QByteArray &symmetricKey)
    : m_publicKey(publicKey)
    , m_symmetricKey(symmetricKey)
{
}

QByteArray KWalletFreedesktopSessionAlgorithmDhAes::negotiationOutput() const
{
    return m_publicKey;
}

bool KWalletFreedesktopSessionAlgorithmDhAes::encrypt(FreedesktopSecret &secret) const
{
    const Botan::InitializationVector iv(Botan::system_rng(), FDO_SECRETS_CIPHER_KEY_SIZE);
    const Botan::SymmetricKey key(reinterpret_cast<const uint8_t *>(m_symmetricKey.constData()), m_symmetricKey.size());
    const auto cipher = Botan::get_cipher("AES-128/CBC/PKCS7", key, iv, Botan::Cipher_Dir::Encryption);

    if (!cipher) {
        qCWarning(KSECRETD_LOG) << "Failed to create cipher for encrypting";
        return false;
    }

    Botan::Pipe pipe(cipher);

    pipe.start_msg();
    pipe.write(secret.value);
    pipe.end_msg();

    secret.value = QByteArray(pipe.read_all());
    secret.parameters = QByteArray(iv.bits_of());

    return true;
}

bool KWalletFreedesktopSessionAlgorithmDhAes::decrypt(FreedesktopSecret &secret) const
{
    const Botan::SymmetricKey key(reinterpret_cast<const uint8_t *>(m_symmetricKey.constData()), m_symmetricKey.size());
    const Botan::InitializationVector iv(secret.parameters.toHex());
    const auto cipher = Botan::get_cipher("AES-128/CBC/PKCS7", key, iv, Botan::Cipher_Dir::Decryption);

    if (!cipher) {
        qCWarning(KSECRETD_LOG) << "Failed to create cipher for decrypting";
        return false;
    }

    Botan::Pipe pipe(cipher);

    pipe.process_msg(secret.value);

    secret.value = QByteArray(pipe.read_all());

    return true;
}

#include "moc_kwalletfreedesktopsession.cpp"
