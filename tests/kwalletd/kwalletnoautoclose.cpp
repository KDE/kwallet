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
#include <KApplication>
#include <KCmdLineArgs>
#include <QEventLoop>
#include <QtDBus>
#include <klocale.h>

static QTextStream _out(stdout, QIODevice::WriteOnly);
static QString _kdewallet;

int openAndClose()
{
	QDBusInterface service("org.kde.kwalletd", "/modules/kwalletd");
	if (!service.isValid()) {
		_out << "Constructed service is invalid!" << endl;
		return 1;
	}
	
	QDBusReply<int> h = service.call(QDBus::Block, "open", _kdewallet, (qlonglong)0, "kwalletnoautoclose");
	if (!h.isValid() || h.value() < 0) {
		_out << "Opening the wallet failed!" << endl;
		_out << "Error: " << h.error().message() << endl;
		return 1;
	} else {
		_out << "Wallet opened." << endl;
	}

	_out << "closing the wallet" << endl;
	QDBusReply<int> r = service.call(QDBus::Block, "close", h.value(), false, "kwalletnoautoclose");
	
	return r;
}

int main(int argc, char *argv[])
{
	KAboutData aboutData("kwalletnoautoclose", 0, ki18n("kwalletnoautoclose"), "version");
	KCmdLineArgs::init(argc, argv, &aboutData);
	KApplication app;

	QDBusInterface service("org.kde.kwalletd", "/modules/kwalletd");
	if (!service.isValid()) {
		_out << "Constructed service is invalid!" << endl;
		return 1;
	}
	
	QDBusReply<bool> r = service.call(QDBus::Block, "isEnabled");
	if (!r.isValid() || !r) {
		_out << "kwalletd is disabled or not running!" << endl;
		return 1;
	}
	
	QDBusReply<QString> kdewallet = service.call(QDBus::Block, "localWallet");
	_kdewallet = kdewallet;
	_out << "local wallet is " << _kdewallet << endl;
	
	QDBusReply<bool> open = service.call(QDBus::Block, "isOpen", _kdewallet);
	if (open) {
		_out << "wallet is already open. Please close to run this test." << endl;
		return 1;
	}
	
	int rc;
	
	_out << "Opening and closing the wallet properly." << endl;
	rc = openAndClose();
	if (rc != 0) {
		_out << "FAILED!" << endl;
		return rc;
	}
	
	_out << "Opening and exiting." << endl;
	QDBusReply<int> h = service.call(QDBus::Block, "open", _kdewallet, (qlonglong)0, "kwalletnoautoclose");
	if (h < 0) {
		_out << "Opening the wallet failed!" << endl;
		return 1;
	} else {
		_out << "Wallet opened." << endl;
	}
	
	_out << "Exiting. Wallet should stay open." << endl;
	
	return 0;
}
