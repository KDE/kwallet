// -*- indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4; -*-
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
#include <KApplication>
#include <KCmdLineArgs>
#include <KWallet/Wallet>
#include <klocale.h>

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

void KWalletMany::walletOpened(bool open)
{
	_out << "Got async wallet: " << (open) << endl;
	--_pending;
	if (_pending == 0) {
		quit();
	}
}

void KWalletMany::quit()
{
	if (_pending == 0) {
		_out << "Success!" << endl;
	} else {
		_out << "Failed: " << _pending << " requests were not handled!" << endl;
	}
	_loop.quit();
}

void KWalletMany::openWallet()
{
	QEventLoop waitLoop;
	
	// open plenty of wallets in synchronous and asynchronous mode
	for (int i = 0; i < NUMWALLETS; ++i) {
		// request asynchronous wallet
		_out << "About to ask for wallet async" << endl;
		Wallet *wallet;
		wallet = Wallet::openWallet(Wallet::NetworkWallet(), 0, Wallet::Asynchronous);
		connect(wallet, SIGNAL(walletOpened(bool)), SLOT(walletOpened(bool)));
		_wallets.append(wallet);
		
		QTimer::singleShot(500, &waitLoop, SLOT(quit()));
		waitLoop.exec();
	}
	
	_loop.exec();
	while (!_wallets.isEmpty()) {
		delete _wallets.takeFirst();
	}
}

int main(int argc, char *argv[])
{
	KAboutData aboutData("kwalletmany", 0, ki18n("kwalletmany"), "version");
	KCmdLineArgs::init(argc, argv, &aboutData);
	KApplication app;
	KWalletMany m;
	
	QTimer::singleShot(0, &m, SLOT(openWallet()));
	QTimer::singleShot(30000, &m, SLOT(quit()));
	
	return app.exec();
}

#include "kwalletmany.moc"
