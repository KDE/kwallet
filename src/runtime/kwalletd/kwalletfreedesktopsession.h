/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 Slava Aseev <nullptrnine@basealt.ru>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef _KWALLETFREEDESKTOPSESSION_H_
#define _KWALLETFREEDESKTOPSESSION_H_

#include "kwalletfreedesktopservice.h"
#include <QDBusContext>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QtCrypto>

#define FDO_SECRETS_SESSION_PATH FDO_SECRETS_SERVICE_OBJECT "/session/"

class KWalletD;

struct CipherResult {
    bool ok;
    QCA::SecureArray bytes;
};

class KWalletFreedesktopSession : public QObject, protected QDBusContext
{
    Q_OBJECT

public:
    KWalletFreedesktopSession(class KWalletFreedesktopService *parent,
                              const QCA::PublicKey &publicKey,
                              QCA::SymmetricKey symmetricKey,
                              QString sessionPath,
                              const QDBusConnection &connection,
                              const QDBusMessage &message);

    KWalletFreedesktopSession(const KWalletFreedesktopSession &) = delete;
    KWalletFreedesktopSession &operator=(const KWalletFreedesktopSession &) = delete;

    KWalletFreedesktopSession(KWalletFreedesktopSession &&) = delete;
    KWalletFreedesktopSession &operator=(KWalletFreedesktopSession &&) = delete;

    KWalletFreedesktopService *fdoService() const;
    KWalletD *backend() const;
    QDBusObjectPath fdoObjectPath() const;

    const QCA::PublicKey &publicKey() const
    {
        return m_publicKey;
    }
    CipherResult encrypt(const QDBusMessage &message, const QCA::SecureArray &bytes, const QCA::SecureArray &initVector) const;
    CipherResult decrypt(const QDBusMessage &message, const QCA::SecureArray &bytes, const QCA::SecureArray &initVector) const;

private Q_SLOTS:
    void slotServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    class KWalletFreedesktopService *m_service;
    QCA::PublicKey m_publicKey;
    QCA::SymmetricKey m_symmetricKey;
    QString m_sessionPath;
    QString m_serviceBusName;
    QDBusServiceWatcher m_serviceWatcher;

    /* Freedesktop API */

    /* org.freedesktop.Secret.Session methods */
public Q_SLOTS:
    void Close();
};

#endif
