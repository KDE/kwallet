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

#include "kwalletbackend.h"

#include <stdlib.h>

#include <kdebug.h>
#include <kglobal.h>
#include <klocale.h>
#include <kcodecs.h>
#include <ksavefile.h>
#include <kstandarddirs.h>
#include <gpgme++/key.h>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegExp>

#include "blowfish.h"
#include "sha1.h"
#include "cbc.h"

#include <assert.h>

// quick fix to get random numbers on win32
#ifdef Q_OS_WIN //krazy:exclude=cpp
  #include <windows.h>
  #include <wincrypt.h>
#endif

#define KWALLET_VERSION_MAJOR		0
#define KWALLET_VERSION_MINOR		0


using namespace KWallet;

#define KWMAGIC "KWALLET\n\r\0\r\n"

class Backend::BackendPrivate
{
};

static void initKWalletDir()
{
    KGlobal::dirs()->addResourceType("kwallet", 0, "share/apps/kwallet");
}

Backend::Backend(const QString& name, bool isPath) : d(0), _name(name), _ref(0), _cipherType(KWallet::BACKEND_CIPHER_UNKNOWN) {
	initKWalletDir();
	if (isPath) {
		_path = name;
	} else {
		_path = KGlobal::dirs()->saveLocation("kwallet") + _name + ".kwl";
	}

	_open = false;
}


Backend::~Backend() {
	if (_open) {
		close();
	}
	delete d;
}

void Backend::setCipherType(BackendCipherType ct)
{
    // changing cipher type on already initialed wallets is not permitted
    assert(_cipherType == KWallet::BACKEND_CIPHER_UNKNOWN);
    _cipherType = ct;
}

// this should be SHA-512 for release probably
static int password2hash(const QByteArray& password, QByteArray& hash) {
	SHA1 sha;
	int shasz = sha.size() / 8;

	assert(shasz >= 20);

	QByteArray block1(shasz, 0);

	sha.process(password.data(), qMin(password.size(), 16));

	// To make brute force take longer
	for (int i = 0; i < 2000; i++) {
		memcpy(block1.data(), sha.hash(), shasz);
		sha.reset();
		sha.process(block1.data(), shasz);
	}

	sha.reset();

	if (password.size() > 16) {
		sha.process(password.data() + 16, qMin(password.size() - 16, 16));
		QByteArray block2(shasz, 0);
		// To make brute force take longer
		for (int i = 0; i < 2000; i++) {
			memcpy(block2.data(), sha.hash(), shasz);
			sha.reset();
			sha.process(block2.data(), shasz);
		}

		sha.reset();

		if (password.size() > 32) {
			sha.process(password.data() + 32, qMin(password.size() - 32, 16));

			QByteArray block3(shasz, 0);
			// To make brute force take longer
			for (int i = 0; i < 2000; i++) {
				memcpy(block3.data(), sha.hash(), shasz);
				sha.reset();
				sha.process(block3.data(), shasz);
			}

			sha.reset();

			if (password.size() > 48) {
				sha.process(password.data() + 48, password.size() - 48);

				QByteArray block4(shasz, 0);
				// To make brute force take longer
				for (int i = 0; i < 2000; i++) {
					memcpy(block4.data(), sha.hash(), shasz);
					sha.reset();
					sha.process(block4.data(), shasz);
				}

				sha.reset();
				// split 14/14/14/14
				hash.resize(56);
				memcpy(hash.data(),      block1.data(), 14);
				memcpy(hash.data() + 14, block2.data(), 14);
				memcpy(hash.data() + 28, block3.data(), 14);
				memcpy(hash.data() + 42, block4.data(), 14);
				block4.fill(0);
			} else {
				// split 20/20/16
				hash.resize(56);
				memcpy(hash.data(),      block1.data(), 20);
				memcpy(hash.data() + 20, block2.data(), 20);
				memcpy(hash.data() + 40, block3.data(), 16);
			}
			block3.fill(0);
		} else {
			// split 20/20
			hash.resize(40);
			memcpy(hash.data(),      block1.data(), 20);
			memcpy(hash.data() + 20, block2.data(), 20);
		}
		block2.fill(0);
	} else {
		// entirely block1
		hash.resize(20);
		memcpy(hash.data(), block1.data(), 20);
	}

	block1.fill(0);

	return 0;
}


int Backend::deref() {
	if (--_ref < 0) {
		kDebug() << "refCount negative!";
		_ref = 0;
	}
	return _ref;
}

bool Backend::exists(const QString& wallet) {
	initKWalletDir();
	QString path = KGlobal::dirs()->saveLocation("kwallet") + '/' + wallet + ".kwl";
	// Note: 60 bytes is presently the minimum size of a wallet file.
	//       Anything smaller is junk.
	return QFile::exists(path) && QFileInfo(path).size() >= 60;
}


QString Backend::openRCToString(int rc) {
	switch (rc) {
		case -255:
			return i18n("Already open.");
		case -2:
			return i18n("Error opening file.");
		case -3:
			return i18n("Not a wallet file.");
		case -4:
			return i18n("Unsupported file format revision.");
		case -42:
			return i18n("Unknown encryption scheme.");
		case -43:
			return i18n("Corrupt file?");
		case -8:
			return i18n("Error validating wallet integrity. Possibly corrupted.");
		case -5:
		case -7:
		case -9:
			return i18n("Read error - possibly incorrect password.");
		case -6:
			return i18n("Decryption error.");
		default:
			return QString();
	}
}


int Backend::open(const QByteArray& password, WId w) {
	if (_open) {
		return -255;  // already open
	}
	
	setPassword(password);
   return openInternal(w);
}

#ifdef HAVE_QGPGME
int Backend::open(const GpgME::Key& key)
{
    if (_open) {
        return -255;  // already open
    }
    _gpgKey = key;
    setPassword(QByteArray());
    return openInternal();
}
#endif // HAVE_QGPGME

int Backend::openPreHashed(const QByteArray &passwordHash)
{
   if (_open) {
      return -255;  // already open
   }
   
   // check the password hash for correct size (currently fixed)
   if (passwordHash.size() != 20 && passwordHash.size() != 40 &&
	   passwordHash.size() != 56) {
      return -42; // unsupported encryption scheme
   }
   
   _passhash = passwordHash;
   return openInternal();
}
 
int Backend::openInternal(WId w)
{
	// No wallet existed.  Let's create it.
	// Note: 60 bytes is presently the minimum size of a wallet file.
	//       Anything smaller is junk and should be deleted.
	if (!QFile::exists(_path) || QFileInfo(_path).size() < 60) {
		QFile newfile(_path);
		if (!newfile.open(QIODevice::ReadWrite)) {
			return -2;   // error opening file
		}
		newfile.close();
		_open = true;
		sync(w);
		return 1;          // new file opened, but OK
	}

	QFile db(_path);

	if (!db.open(QIODevice::ReadOnly)) {
		return -2;         // error opening file
	}

	char magicBuf[KWMAGIC_LEN];
	db.read(magicBuf, KWMAGIC_LEN);
	if (memcmp(magicBuf, KWMAGIC, KWMAGIC_LEN) != 0) {
		return -3;         // bad magic
	}

	db.read(magicBuf, 4);

	// First byte is major version, second byte is minor version
	if (magicBuf[0] != KWALLET_VERSION_MAJOR) {
		return -4;         // unknown version
	}

	if (magicBuf[1] != KWALLET_VERSION_MINOR) {
		return -4;	   // unknown version
	}

	BackendPersistHandler *phandler = BackendPersistHandler::getPersistHandler(magicBuf);
    if (0 == phandler){
        return 42; // unknown cipher or hash
    }
    return phandler->read(this, db, w);
}


int Backend::sync(WId w) {
	if (!_open) {
		return -255;  // not open yet
	}

	KSaveFile sf(_path);

	if (!sf.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
		return -1;		// error opening file
	}
	sf.setPermissions(QFile::ReadUser|QFile::WriteUser);

	if (sf.write(KWMAGIC, KWMAGIC_LEN) != KWMAGIC_LEN) {
		sf.abort();
		return -4; // write error
	}

	// Write the version number
	QByteArray version(4, 0);
	version[0] = KWALLET_VERSION_MAJOR;
	version[1] = KWALLET_VERSION_MINOR;

    BackendPersistHandler *phandler = BackendPersistHandler::getPersistHandler(_cipherType);
    if (0 == phandler) {
        return -4; // write error
    }
    return phandler->write(this, sf, version, w);
}


int Backend::close(bool save) {
	// save if requested
	if (save) {
		int rc = sync(0);
		if (rc != 0) {
			return rc;
		}
	}
	
	// do the actual close
	for (FolderMap::ConstIterator i = _entries.constBegin(); i != _entries.constEnd(); ++i) {
		for (EntryMap::ConstIterator j = i.value().constBegin(); j != i.value().constEnd(); ++j) {
			delete j.value();
		}
	}
	_entries.clear();
	
	// empty the password hash
	_passhash.fill(0);

	_open = false;
	
	return 0;
}

const QString& Backend::walletName() const {
	return _name;
}


bool Backend::isOpen() const {
	return _open;
}


QStringList Backend::folderList() const {
	return _entries.keys();
}


QStringList Backend::entryList() const {
	return _entries[_folder].keys();
}


Entry *Backend::readEntry(const QString& key) {
	Entry *rc = 0L;

	if (_open && hasEntry(key)) {
		rc = _entries[_folder][key];
	}

	return rc;
}


QList<Entry*> Backend::readEntryList(const QString& key) {
	QList<Entry*> rc;

	if (!_open) {
		return rc;
	}

	QRegExp re(key, Qt::CaseSensitive, QRegExp::Wildcard);

	const EntryMap& map = _entries[_folder];
	for (EntryMap::ConstIterator i = map.begin(); i != map.end(); ++i) {
		if (re.exactMatch(i.key())) {
			rc.append(i.value());
		}
	}
	return rc;
}


bool Backend::createFolder(const QString& f) {
	if (_entries.contains(f)) {
		return false;
	}

	_entries.insert(f, EntryMap());

	KMD5 folderMd5;
	folderMd5.update(f.toUtf8());
	_hashes.insert(MD5Digest(folderMd5.rawDigest()), QList<MD5Digest>());

	return true;
}


int Backend::renameEntry(const QString& oldName, const QString& newName) {
	EntryMap& emap = _entries[_folder];
	EntryMap::Iterator oi = emap.find(oldName);
	EntryMap::Iterator ni = emap.find(newName);

	if (oi != emap.end() && ni == emap.end()) {
		Entry *e = oi.value();
		emap.erase(oi);
		emap[newName] = e;

		KMD5 folderMd5;
		folderMd5.update(_folder.toUtf8());

		HashMap::iterator i = _hashes.find(MD5Digest(folderMd5.rawDigest()));
		if (i != _hashes.end()) {
			KMD5 oldMd5, newMd5;
			oldMd5.update(oldName.toUtf8());
			newMd5.update(newName.toUtf8());
			i.value().removeAll(MD5Digest(oldMd5.rawDigest()));
			i.value().append(MD5Digest(newMd5.rawDigest()));
		}
		return 0;
	}

	return -1;
}


void Backend::writeEntry(Entry *e) {
	if (!_open)
		return;

	if (!hasEntry(e->key())) {
		_entries[_folder][e->key()] = new Entry;
	}
	_entries[_folder][e->key()]->copy(e);

	KMD5 folderMd5;
	folderMd5.update(_folder.toUtf8());

	HashMap::iterator i = _hashes.find(MD5Digest(folderMd5.rawDigest()));
	if (i != _hashes.end()) {
		KMD5 md5;
		md5.update(e->key().toUtf8());
		i.value().append(MD5Digest(md5.rawDigest()));
	}
}


bool Backend::hasEntry(const QString& key) const {
	return _entries.contains(_folder) && _entries[_folder].contains(key);
}


bool Backend::removeEntry(const QString& key) {
	if (!_open) {
		return false;
	}

	FolderMap::Iterator fi = _entries.find(_folder);
	EntryMap::Iterator ei = fi.value().find(key);

	if (fi != _entries.end() && ei != fi.value().end()) {
		delete ei.value();
		fi.value().erase(ei);
		KMD5 folderMd5;
		folderMd5.update(_folder.toUtf8());

		HashMap::iterator i = _hashes.find(MD5Digest(folderMd5.rawDigest()));
		if (i != _hashes.end()) {
			KMD5 md5;
			md5.update(key.toUtf8());
			i.value().removeAll(MD5Digest(md5.rawDigest()));
		}
		return true;
	}

	return false;
}


bool Backend::removeFolder(const QString& f) {
	if (!_open) {
		return false;
	}

	FolderMap::Iterator fi = _entries.find(f);

	if (fi != _entries.end()) {
		if (_folder == f) {
			_folder.clear();
		}

		for (EntryMap::Iterator ei = fi.value().begin(); ei != fi.value().end(); ++ei) {
			delete ei.value();
		}

		_entries.erase(fi);

		KMD5 folderMd5;
		folderMd5.update(f.toUtf8());
		_hashes.remove(MD5Digest(folderMd5.rawDigest()));
		return true;
	}

	return false;
}


bool Backend::folderDoesNotExist(const QString& folder) const {
	KMD5 md5;
	md5.update(folder.toUtf8());
	return !_hashes.contains(MD5Digest(md5.rawDigest()));
}


bool Backend::entryDoesNotExist(const QString& folder, const QString& entry) const {
	KMD5 md5;
	md5.update(folder.toUtf8());
	HashMap::const_iterator i = _hashes.find(MD5Digest(md5.rawDigest()));
	if (i != _hashes.end()) {
		md5.reset();
		md5.update(entry.toUtf8());
		return !i.value().contains(MD5Digest(md5.rawDigest()));
	}
	return true;
}

void Backend::setPassword(const QByteArray &password) {
	_passhash.fill(0); // empty just in case
	BlowFish _bf;
	CipherBlockChain bf(&_bf);
	_passhash.resize(bf.keyLen()/8);
	password2hash(password, _passhash);
}

#ifdef HAVE_QGPGME
const GpgME::Key &Backend::gpgKey() const {
    return _gpgKey;
}
#endif
