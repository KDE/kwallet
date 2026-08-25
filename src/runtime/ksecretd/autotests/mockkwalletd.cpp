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

KTimeout::KTimeout(QObject *)
{
}
KTimeout::~KTimeout()
{
}

MOCK_FUNCTION(KTimeout, clear, 0, );
MOCK_FUNCTION(KTimeout, resetTimer, 2, );
MOCK_FUNCTION(KTimeout, removeTimer, 1, );
MOCK_FUNCTION(KTimeout, addTimer, 2, );

void KTimeout::timerEvent(QTimerEvent *)
{
}

MOCK_FUNCTION(KSecretD, readEntry, 3, );
MOCK_FUNCTION(KSecretD, readMap, 3, );
MOCK_FUNCTION(KSecretD, readPassword, 3, );

MOCK_FUNCTION_RES(KSecretD, removeEntry, 3, 0, );
MOCK_FUNCTION_RES(KSecretD, writePassword, 4, 0, );

using OVWriteEntry_5 = int (KSecretD::*)(int, const QString &, const QString &, const QByteArray &, int);
MOCK_FUNCTION_OVERLOADED_RES(KSecretD, writeEntry, 5, 0, OVWriteEntry_5);

using OVWriteEntry_4 = int (KSecretD::*)(int, const QString &, const QString &, const QByteArray &);
MOCK_FUNCTION_OVERLOADED_RES(KSecretD, writeEntry, 4, 0, OVWriteEntry_4);

MOCK_FUNCTION(KSecretD, entryType, 3, );
MOCK_FUNCTION_RES(KSecretD, renameEntry, 4, 0, );

MOCK_FUNCTION_RES(KSecretD, isEnabled, 0, 1);

using OVOpenAsync4 = int (KSecretD::*)(const QString &, qlonglong, const QDBusConnection &, const QDBusMessage &);
MOCK_FUNCTION_OVERLOADED(KSecretD, openAsync, 4, OVOpenAsync4);

using OVClose2 = int (KSecretD::*)(int, const QDBusMessage &);
MOCK_FUNCTION_OVERLOADED(KSecretD, close, 2, OVClose2);

MOCK_FUNCTION(KSecretD, deleteWallet, 1, );

MOCK_FUNCTION_OVERLOADED(KSecretD, isOpen, 1, bool (KSecretD::*)(int));

MOCK_FUNCTION(KSecretD, wallets, 0, const);
MOCK_FUNCTION(KSecretD, folderList, 1, );
MOCK_FUNCTION(KSecretD, createFolder, 2, );
MOCK_FUNCTION(KSecretD, entryList, 2, );

MOCK_FUNCTION(KSecretD, renameWallet, 2, );
MOCK_FUNCTION(KSecretD, hasEntry, 3, );
MOCK_FUNCTION(KSecretD, pamOpen, 3, );
MOCK_FUNCTION(KSecretD, changePassword, 3, );
MOCK_FUNCTION(KSecretD, reconfigure, 0, );
MOCK_FUNCTION(KSecretD, closeAllWallets, 0, );

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
