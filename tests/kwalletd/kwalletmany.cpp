/*
   This file is part of the KDE libraries

   Copyright (c) 2008 Michael Leupold <lemma@confuego.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.

*/

#include <QTextStream>
#include <QTimer>
#include <QThread>
#include <KAboutData>
#include <QtWidgets/QApplication>
#include <QtTest/QTest>
#include <KLocalizedString>
#include <kwallet.h>

#include "kwalletmany.h"

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
    _out << "Got async wallet: " << (open) << endl;
    --_pending;
}

void KWalletMany::openWallet()
{
    // open plenty of wallets in synchronous and asynchronous mode
    for (int i = 0; i < NUMWALLETS; ++i) {
        // request asynchronous wallet
        _out << "About to ask for wallet async" << endl;
        Wallet *wallet;
        wallet = Wallet::openWallet(Wallet::NetworkWallet(), 0, Wallet::Asynchronous);
        QVERIFY(wallet != 0);
        connect(wallet, SIGNAL(walletOpened(bool)), SLOT(walletOpened(bool)));
        _wallets.append(wallet);
    }

    // wait for 30s to receive the wallet opened replies from kwalletd
    QTRY_VERIFY_WITH_TIMEOUT(_pending == 0, 30000);

    while (!_wallets.isEmpty()) {
        delete _wallets.takeFirst();
    }
    QApplication::quit();
}

QTEST_GUILESS_MAIN(KWalletMany)
