/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kwalletportalsecrets.h"

#include "kwalletportalsecretsadaptor.h"

#include <QRandomGenerator>

#include <KWindowSystem>

using namespace Qt::StringLiterals;

KWalletPortalSecrets::KWalletPortalSecrets(KSecretD *parent)
    : QObject(parent)
    , m_kwalletd(parent)
{
    (void)new KWalletPortalSecretsAdaptor(this);

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), this, QDBusConnection::ExportAdaptors);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.freedesktop.impl.portal.desktop.kwallet"));
}

uint KWalletPortalSecrets::RetrieveSecret(const QDBusObjectPath &handle,
                                          const QString &app_id,
                                          const QDBusUnixFileDescriptor &fd,
                                          const QVariantMap &options,
                                          QVariantMap &results)
{
    Q_UNUSED(handle);
    Q_UNUSED(results);

    setDelayedReply(true);

    Request request{message(), fd.fileDescriptor(), app_id};

    const auto parentWindow = options.value(u"parent_window"_s).toString();
    const auto activationToken = options.value(u"activation_token"_s).toString();

    std::variant<QString, qulonglong> windowHandle = 0u;
    if (parentWindow.startsWith("x11:"_L1)) {
        windowHandle = QStringView(parentWindow).mid(strlen("x11:")).toULongLong(nullptr, 16);
    } else if (parentWindow.startsWith((QLatin1String("wayland:")))) {
        windowHandle = parentWindow.mid(strlen("wayland:"));
    }

    auto future = std::visit(
        [this](const auto &windowHandle) {
            return m_kwalletd->open(KWallet::Backend::networkWallet(), windowHandle);
        },
        windowHandle);
    future.then(this, [this, request](int walletHandle) {
        walletOpened(request, walletHandle);
    });

    return 0;
}

void KWalletPortalSecrets::walletOpened(const Request &request, int walletHandle)
{
    if (walletHandle == -1) {
        const auto replyList = QVariantList{{(uint)2}, {{QVariantMap{}}}};
        auto reply = request.message.createReply(replyList);
        QDBusConnection::sessionBus().send(reply);
        return;
    }

    bool exists = m_kwalletd->hasEntry(walletHandle, "xdg-desktop-portal", request.appId);

    QByteArray secret;

    if (exists) {
        secret = m_kwalletd->readEntry(walletHandle, "xdg-desktop-portal", request.appId);
    } else {
        secret = generateSecret();
        m_kwalletd->writeEntry(walletHandle, "xdg-desktop-portal", request.appId, secret, KWallet::Wallet::Stream);
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

#include "moc_kwalletportalsecrets.cpp"
