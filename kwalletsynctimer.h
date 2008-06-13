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

#ifndef _KWALLETSYNCTIMER_H_
#define _KWALLETSYNCTIMER_H_

#include <QtCore/QString>
#include <QtCore/QTimer>

class KWalletSyncTimer : public QTimer {
	Q_OBJECT

	public:
		KWalletSyncTimer(QObject* parent, const QString& _wallet);
		~KWalletSyncTimer() {
		}
		
	private:
		QString wallet;
		
	private Q_SLOTS:
		void emitSync();
		
	Q_SIGNALS:
		void timeoutSync(const QString& wallet);
};

#endif
