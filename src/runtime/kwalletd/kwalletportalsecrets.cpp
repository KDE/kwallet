/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kwalletportalsecrets.h"

#include "kwalletportalsecretsadaptor.h"

#include <QRandomGenerator>

KWalletPortalSecrets::KWalletPortalSecrets(KWalletD *parent)
    : QObject(parent)
    , m_kwalletd(parent)
{
    (void)new KWalletPortalSecretsAdaptor(this);

    QDBusConnection::sessionBus().registerService(QStringLiteral("org.freedesktop.impl.portal.desktop.kwallet"));
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), this, QDBusConnection::ExportAdaptors);

    connect(m_kwalletd, &KWalletD::walletAsyncOpened, this, &KWalletPortalSecrets::walletOpened);
}

uint KWalletPortalSecrets::RetrieveSecret(const QDBusObjectPath &handle,
                                          const QString &app_id,
                                          const QDBusUnixFileDescriptor &fd,
                                          const QVariantMap &options,
                                          QVariantMap &results)
{
    Q_UNUSED(handle);
    Q_UNUSED(options);
    Q_UNUSED(results);

    setDelayedReply(true);

    const QString wallet = m_kwalletd->networkWallet();
    const bool walletIsOpen = m_kwalletd->isOpen(m_kwalletd->networkWallet());

    Request request{message(), fd.fileDescriptor(), app_id};

    if (!walletIsOpen) {
        m_pendingRequests.insert(app_id, request);
        m_kwalletd->openAsync(wallet, 0, app_id, false, connection(), message());
    } else {
        Q_ASSERT(m_handle != -1);
        handleRequest(request);
    }

    return 0;
}

void KWalletPortalSecrets::walletOpened(int id, int handle)
{
    Q_UNUSED(id);
    Q_ASSERT(handle != -1);

    m_handle = handle;

    for (const Request &request : std::as_const(m_pendingRequests)) {
        handleRequest(request);
    }

    m_pendingRequests.clear();
}

void KWalletPortalSecrets::handleRequest(const Request &request)
{
    bool exists = m_kwalletd->hasEntry(m_handle, "xdg-desktop-portal", request.appId, request.appId);

    QByteArray secret;

    if (exists) {
        secret = m_kwalletd->readEntry(m_handle, "xdg-desktop-portal", request.appId, request.appId);
    } else {
        secret = generateSecret();
        m_kwalletd->writeEntry(m_handle, "xdg-desktop-portal", request.appId, secret, request.appId);
        m_kwalletd->sync(m_handle, request.appId);
    }

    QFile outFile;
    outFile.open(request.fd, QIODevice::ReadWrite);

    outFile.write(secret);

    const auto replyList = QVariantList{{(uint)0}, {{QVariantMap{}}}};
    auto reply = request.message.createReply(replyList);
    QDBusConnection::sessionBus().send(reply);
}

QByteArray KWalletPortalSecrets::generateSecret() const
{
    const int secretSize = 64;
    QByteArray secret;
    secret.resize(secretSize);
    QRandomGenerator::securelySeeded().generate(secret.begin(), secret.end());

    return secret;
}
