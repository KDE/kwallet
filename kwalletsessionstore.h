// -*- indent-tabs-mode: t; tab-width: 4; c-basic-offset: 4; -*-
/*
   This file is part of the KDE Wallet Daemon

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

#ifndef _KWALLETSESSIONSTORE_H_
#define _KWALLETSESSIONSTORE_H_

#include <QPair>
#include <QString>
#include <QList>
#include <QHash>
#include <QStringList>

typedef QPair<QString,int> KWalletAppHandlePair;

class KWalletSessionStore
{
public:
	KWalletSessionStore();
	~KWalletSessionStore();
	
	// add a new session
	void addSession(const QString &appid, const QString &service, int handle);
	// check if the application has a session using that handle
	bool hasSession(const QString &appid, int handle = -1) const;
	// find all sessions a service has
	QList<KWalletAppHandlePair> findSessions(const QString &service) const;
	// remove a specific session
	bool removeSession(const QString &appid, const QString &service, int handle);
	// remove all sessions an application has to a specific handle
	int removeAllSessions(const QString &appid, int handle);
	// remove all sessions related to a handle
	int removeAllSessions(int handle);
	// get all applications using a handle
	QStringList getApplications(int handle) const;
	
private:
	class Session;
	QHash< QString,QList<Session*> > m_sessions; // appid => session
};

#endif // _KWALLETSESSIONSTORE_H_
