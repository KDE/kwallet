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
#include <KAboutData>
#include <QtWidgets/QApplication>
#include <QTimer>
#include <kwallet.h>
#include <KLocalizedString>
#include <QtTest/QTest>

#include "kwalletautoclose.h"

using namespace KWallet;

static QTextStream _out(stdout, QIODevice::WriteOnly);

KWalletAutoClose::KWalletAutoClose() : QObject()
{
}

void KWalletAutoClose::openWallet()
{
	_out << "Opening wallet synchronously" << endl;
	Wallet *wallet = Wallet::openWallet(Wallet::NetworkWallet(), 0, Wallet::Synchronous);
    QVERIFY(wallet != 0);
	_out << "Exiting without closing. The wallet should autoclose." << endl;
	QApplication::exit(0);
}

int main(int argc, char *argv[]) 
{
	QApplication app(argc, argv);
    app.setApplicationName("kwalletmany");
    app.setApplicationDisplayName(i18n("kwalletmany"));
	KWalletAutoClose m;
	
	QTimer::singleShot(0, &m, SLOT(openWallet()));
	
	return app.exec();
}

#include "kwalletautoclose.moc"
