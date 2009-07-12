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

#include "kwalletsessionstore.h"

class KWalletSessionStore::Session
{
public:
	QString m_service; // client dbus service (or empty)
	int m_handle; // backend handle
};

KWalletSessionStore::KWalletSessionStore()
{
}

KWalletSessionStore::~KWalletSessionStore()
{
	Q_FOREACH(const QList<Session*> &l, m_sessions) {
		qDeleteAll(l);
	}
}

void KWalletSessionStore::addSession(const QString &appid, const QString &service, int handle)
{
	Session *sess = new Session();
	sess->m_service = service;
	sess->m_handle = handle;
	m_sessions[appid].append(sess);
}

bool KWalletSessionStore::hasSession(const QString &appid, int handle) const
{
	if (!m_sessions.contains(appid)) {
		return false;
	} else if (handle == -1) {
		return true;
	}
	
	QList<Session*>::const_iterator it;
	QList<Session*>::const_iterator end = m_sessions[appid].constEnd();
	for (it = m_sessions[appid].constBegin(); it != end; ++it) {
		Q_ASSERT(*it);
		if ((*it)->m_handle == handle) {
			return true;
		}
	}
	
	return false;
}

QList<KWalletAppHandlePair> KWalletSessionStore::findSessions(const QString &service) const
{
	QList<KWalletAppHandlePair> rc;
	QList<QString> sessionKeys(m_sessions.keys());
	Q_FOREACH(const QString &appid, sessionKeys) {
		Q_FOREACH(const Session *sess, m_sessions[appid]) {
			Q_ASSERT(sess);
			if (sess->m_service == service) {
				rc.append(qMakePair(appid, sess->m_handle));
			}
		}
	}
	return rc;
}

bool KWalletSessionStore::removeSession(const QString &appid, const QString &service, int handle)
{
	if (!m_sessions.contains(appid)) {
		return false;
	}
	
	QList<Session*>::const_iterator it;
	QList<Session*>::const_iterator end = m_sessions[appid].constEnd();
	for (it = m_sessions[appid].constBegin(); it != end; ++it) {
		Q_ASSERT(*it);
		if ((*it)->m_service == service && (*it)->m_handle == handle) {
			Session *sess = *it;
			m_sessions[appid].removeAll(sess);
			delete sess;
			if (m_sessions[appid].isEmpty()) {
				m_sessions.remove(appid);
			}
			return true;
		}
	}
	
	return false;
}


int KWalletSessionStore::removeAllSessions(const QString &appid, int handle)
{
	if (!m_sessions.contains(appid)) {
		return false;
	}
	
	QList<Session*>::iterator it;
	QList<Session*>::iterator end = m_sessions[appid].end();
	for (it = m_sessions[appid].begin(); it != end; ++it) {
		Q_ASSERT(*it);
		if ((*it)->m_handle == handle) {
			delete *it;
			*it = 0;
		}
	}
	
	int removed = m_sessions[appid].removeAll(0);
	if (m_sessions[appid].isEmpty()) {
		m_sessions.remove(appid);
	}
	
	return removed;
}

int KWalletSessionStore::removeAllSessions(int handle) {
	QList<QString> appremove;
	int numrem = 0;
	QList<QString> sessionKeys(m_sessions.keys());
	Q_FOREACH(const QString &appid, sessionKeys) {
		QList<Session*>::iterator it;
		QList<Session*>::iterator end = m_sessions[appid].end();
		for (it = m_sessions[appid].begin(); it != end; ++it) {
			Q_ASSERT(*it);
			if ((*it)->m_handle == handle) {
				delete *it;
				*it = 0;
				numrem++;
			}
		}
		// remove all zeroed sessions
		m_sessions[appid].removeAll(0);
		if (m_sessions[appid].isEmpty()) {
			appremove.append(appid);
		}
	}
	
	// now remove all applications without sessions
	Q_FOREACH(const QString &appid, appremove) {
		m_sessions.remove(appid);
	}
	
	return numrem;
}

QStringList KWalletSessionStore::getApplications(int handle) const {
	QStringList rc;
	Q_FOREACH(const QString &appid, m_sessions.uniqueKeys()) {
		if (hasSession(appid, handle)) {
			rc.append(appid);
		}
	}
	return rc;
}
