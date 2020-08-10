/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2008 Michael Leupold <lemma@confuego.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kwalletmany.h"

#include <QTextStream>
#include <QThread>
#include <QApplication>
#include <QTest>
#include <kwallet.h>

#define NUMWALLETS 10

using namespace KWallet;

static QTextStream _out(stdout, QIODevice::WriteOnly);

KWalletMany::KWalletMany() : QObject(), _pending(10)
{
}

KWalletMany::~KWalletMany()
{
}

void KWalletMany::init()
{
    if (!qEnvironmentVariableIsSet("DISPLAY")) {
        QSKIP("$DISPLAY is not set. These tests cannot be done without a graphical system.");
    }
}

void KWalletMany::walletOpened(bool open)
{
    _out << "Got async wallet: " << (open) << '\n';
    _out.flush();
    --_pending;
}

void KWalletMany::openWallet()
{
    // open plenty of wallets in synchronous and asynchronous mode
    for (int i = 0; i < NUMWALLETS; ++i) {
        // request asynchronous wallet
        _out << "About to ask for wallet async" << '\n';
        Wallet *wallet;
        wallet = Wallet::openWallet(Wallet::NetworkWallet(), 0, Wallet::Asynchronous);
        QVERIFY(wallet != nullptr);
        connect(wallet, SIGNAL(walletOpened(bool)), SLOT(walletOpened(bool)));
        _wallets.append(wallet);
    }
    _out.flush();

    // wait for 30s to receive the wallet opened replies from kwalletd
    QTRY_VERIFY_WITH_TIMEOUT(_pending == 0, 30000);

    while (!_wallets.isEmpty()) {
        delete _wallets.takeFirst();
    }
    QApplication::quit();
}

QTEST_GUILESS_MAIN(KWalletMany)
