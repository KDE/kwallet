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

#include "kwalletopenloop.h"
#include "kwalletd.h"

int KWalletOpenLoop::waitForAsyncOpen(int tId)
{
	transaction = tId;
	connect(wallet, SIGNAL(walletAsyncOpened(int, int)),
                    SLOT(walletAsyncOpened(int, int)));
	exec();
	disconnect(this, SLOT(walletAsyncOpened(int, int)));
	return result;
}

void KWalletOpenLoop::walletAsyncOpened(int tId, int handle)
{
	// if our transaction finished, stop waiting
	if (tId == transaction) {
		result = handle;
		quit();
	}
}

#include "kwalletopenloop.moc"
