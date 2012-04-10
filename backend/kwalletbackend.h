/* This file is part of the KDE project
 *
 * Copyright (C) 2001-2004 George Staikos <staikos@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifndef _KWALLETBACKEND_H
#define _KWALLETBACKEND_H

#include <kcodecs.h>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include "kwalletentry.h"


namespace KWallet {

/**
 * @internal
 */
class MD5Digest : public QByteArray {
	public:
		MD5Digest() : QByteArray(16, 0) {}
		MD5Digest(const char *data) : QByteArray(data, 16) {}
		MD5Digest(const KMD5::Digest d) : QByteArray(reinterpret_cast<const char *>(d), 16) {}
		virtual ~MD5Digest() {}

		int operator<(const MD5Digest& r) const {
				int i = 0;
				char x, y;
				for (; i < 16; ++i) {
					x = at(i);
					y = r.at(i);
					if (x != y) {
						break;
					}
				}
				if (i < 16 && x < y) {
					return 1;
				}
				return 0;
			}
};


/* @internal
 */
class KDE_EXPORT Backend {
	public:
		explicit Backend(const QString& name = QLatin1String("kdewallet"), bool isPath = false);
		~Backend();

		// Open and unlock the wallet.
		// If opening succeeds, the password's hash will be remembered.
		// If opening fails, the password's hash will be cleared.
		int open(const QByteArray& password);
      
      // Open and unlock the wallet using a pre-hashed password.
      // If opening succeeds, the password's hash will be remembered.
      // If opening fails, the password's hash will be cleared.
      int openPreHashed(const QByteArray &passwordHash);

		// Close the wallet, losing any changes.
		// if save is true, the wallet is saved prior to closing it.
		int close(bool save = false);

		// Write the wallet to disk
		int sync();

		// Returns true if the current wallet is open.
		bool isOpen() const;

		// Returns the current wallet name.
		const QString& walletName() const;

		// The list of folders.
		QStringList folderList() const;

		// Force creation of a folder.
		bool createFolder(const QString& f);

		// Change the folder.
		void setFolder(const QString& f) { _folder = f; }

		// Current folder.  If empty, it's the global folder.
		const QString& folder() const { return _folder; }

		// Does it have this folder?
		bool hasFolder(const QString& f) const { return _entries.contains(f); }

		// Look up an entry.  Returns null if it doesn't exist.
		Entry *readEntry(const QString& key);
		
		// Look up a list of entries.  Supports wildcards.
		// You delete the list
		QList<Entry*> readEntryList(const QString& key);

		// Store an entry.
		void writeEntry(Entry *e);

		// Does this folder contain this entry?
		bool hasEntry(const QString& key) const;

		// Returns true if the entry was removed
		bool removeEntry(const QString& key);

		// Returns true if the folder was removed
		bool removeFolder(const QString& f);

		// The list of entries in this folder.
		QStringList entryList() const;

		// Rename an entry in this folder.
		int renameEntry(const QString& oldName, const QString& newName);
		
		// Set the password used for opening/closing the wallet.
		// This does not sync the wallet to disk!
		void setPassword(const QByteArray &password);
      
		int ref() { return ++_ref; }

		int deref();

		int refCount() const { return _ref; }

		static bool exists(const QString& wallet);

		bool folderDoesNotExist(const QString& folder) const;

		bool entryDoesNotExist(const QString& folder, const QString& entry) const;

		static QString openRCToString(int rc);

	private:
		Q_DISABLE_COPY( Backend )
		class BackendPrivate;
		BackendPrivate *const d;
		QString _name;
		QString _path;
		bool _open;
		QString _folder;
		int _ref;
		// Map Folder->Entries
		typedef QMap< QString, Entry* > EntryMap;
		typedef QMap< QString, EntryMap > FolderMap;
		FolderMap _entries;
		typedef QMap<MD5Digest, QList<MD5Digest> > HashMap;
		HashMap _hashes;
		QByteArray _passhash; // password hash used for saving the wallet
      
      // open the wallet with the password already set. This is
      // called internally by both open and openPreHashed.
      int openInternal();
};

}

#endif

