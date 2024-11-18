/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2022 Slava Aseev <nullptrnine@basealt.ru>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "static_mock.hpp"

#include "../ksecretd.h"
#include "../kwalletfreedesktopcollection.h"
#include "../kwalletfreedesktopitem.h"
#include "../kwalletfreedesktopprompt.h"
#include "../kwalletfreedesktopservice.h"
#include "../kwalletfreedesktopsession.h"

KSecretD::KSecretD()
    : _syncTime(0)
{
}
KSecretD::~KSecretD()
{
}

KWalletSessionStore::KWalletSessionStore()
{
}
KWalletSessionStore::~KWalletSessionStore()
{
}

KTimeout::KTimeout(QObject *)
{
}
KTimeout::~KTimeout()
{
}

MOCK_FUNCTION(KSecretD, encodeWalletName, 1, );
MOCK_FUNCTION(KSecretD, decodeWalletName, 1, );

MOCK_FUNCTION(KTimeout, clear, 0, );
MOCK_FUNCTION(KTimeout, resetTimer, 2, );
MOCK_FUNCTION(KTimeout, removeTimer, 1, );
MOCK_FUNCTION(KTimeout, addTimer, 2, );

void KTimeout::timerEvent(QTimerEvent *)
{
}

MOCK_FUNCTION(KSecretD, readEntry, 4, );
MOCK_FUNCTION(KSecretD, readMap, 4, );
MOCK_FUNCTION(KSecretD, readPassword, 4, );

MOCK_FUNCTION_RES(KSecretD, removeEntry, 4, 0, );
MOCK_FUNCTION_RES(KSecretD, writeMap, 5, 0, );
MOCK_FUNCTION_RES(KSecretD, writePassword, 5, 0, );

using OVWriteEntry_6 = int (KSecretD::*)(int, const QString &, const QString &, const QByteArray &, int, const QString &);
MOCK_FUNCTION_OVERLOADED_RES(KSecretD, writeEntry, 6, 0, OVWriteEntry_6);

using OVWriteEntry_5 = int (KSecretD::*)(int, const QString &, const QString &, const QByteArray &, const QString &);
MOCK_FUNCTION_OVERLOADED_RES(KSecretD, writeEntry, 5, 0, OVWriteEntry_5);

MOCK_FUNCTION(KSecretD, entryType, 4, );
MOCK_FUNCTION_RES(KSecretD, renameEntry, 5, 0, );

MOCK_FUNCTION(KSecretD, isEnabled, 0, const);
MOCK_FUNCTION(KSecretD, open, 3, );
MOCK_FUNCTION(KSecretD, openPath, 3, );
MOCK_FUNCTION(KSecretD, openPathAsync, 4, );

using OVOpenAsync4 = int (KSecretD::*)(const QString &, qlonglong, const QString &, bool);
MOCK_FUNCTION_OVERLOADED(KSecretD, openAsync, 4, OVOpenAsync4);

using OVOpenAsync6 = int (KSecretD::*)(const QString &, qlonglong, const QString &, bool, const QDBusConnection &, const QDBusMessage &);
MOCK_FUNCTION_OVERLOADED(KSecretD, openAsync, 6, OVOpenAsync6);

using OVClose4 = int (KSecretD::*)(int, bool, const QString &, const QDBusMessage &);
MOCK_FUNCTION_OVERLOADED(KSecretD, close, 4, OVClose4);

using OVClose2 = int (KSecretD::*)(const QString &, bool);
MOCK_FUNCTION_OVERLOADED(KSecretD, close, 2, OVClose2);

using OVClose3 = int (KSecretD::*)(int, bool, const QString &);
MOCK_FUNCTION_OVERLOADED(KSecretD, close, 3, OVClose3);

MOCK_FUNCTION(KSecretD, deleteWallet, 1, );

MOCK_FUNCTION_OVERLOADED(KSecretD, isOpen, 1, bool (KSecretD::*)(const QString &));
MOCK_FUNCTION_OVERLOADED(KSecretD, isOpen, 1, bool (KSecretD::*)(int));

MOCK_FUNCTION(KSecretD, users, 1, const);
MOCK_FUNCTION(KSecretD, wallets, 0, const);
MOCK_FUNCTION(KSecretD, folderList, 2, );
MOCK_FUNCTION(KSecretD, hasFolder, 3, );
MOCK_FUNCTION(KSecretD, createFolder, 3, );
MOCK_FUNCTION(KSecretD, removeFolder, 3, );
MOCK_FUNCTION(KSecretD, entryList, 3, );

#if KWALLET_BUILD_DEPRECATED_SINCE(5, 72)
MOCK_FUNCTION(KSecretD, readEntryList, 4, );
MOCK_FUNCTION(KSecretD, readMapList, 4, );
MOCK_FUNCTION(KSecretD, readPasswordList, 4, );
#endif

MOCK_FUNCTION(KSecretD, entriesList, 3, );
MOCK_FUNCTION(KSecretD, mapList, 3, );
MOCK_FUNCTION(KSecretD, passwordList, 3, );
MOCK_FUNCTION(KSecretD, renameWallet, 2, );
MOCK_FUNCTION(KSecretD, hasEntry, 4, );
MOCK_FUNCTION(KSecretD, disconnectApplication, 2, );
MOCK_FUNCTION(KSecretD, folderDoesNotExist, 2, );
MOCK_FUNCTION(KSecretD, keyDoesNotExist, 3, );
MOCK_FUNCTION(KSecretD, networkWallet, 0, );
MOCK_FUNCTION(KSecretD, localWallet, 0, );
MOCK_FUNCTION(KSecretD, pamOpen, 3, );
MOCK_FUNCTION(KSecretD, sync, 2, );
MOCK_FUNCTION(KSecretD, changePassword, 3, );
MOCK_FUNCTION(KSecretD, reconfigure, 0, );
MOCK_FUNCTION(KSecretD, closeAllWallets, 0, );
MOCK_FUNCTION(KSecretD, screenSaverChanged, 1, );

void KSecretD::slotServiceOwnerChanged(const QString &, const QString &, const QString &)
{
}
void KSecretD::emitWalletListDirty()
{
}
void KSecretD::timedOutClose(int)
{
}
void KSecretD::timedOutSync(int)
{
}
void KSecretD::notifyFailures()
{
}
void KSecretD::processTransactions()
{
}
void KSecretD::activatePasswordDialog()
{
}

#include <QLoggingCategory>
const QLoggingCategory &KSECRETD_LOG()
{
    static const QLoggingCategory category("kf.wallet.ksecretd", QtFatalMsg);
    return category;
}
