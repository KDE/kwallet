/**
  * This file is part of the KDE project
  * Copyright (C) 2013 Valentin Rusu <kde@rusu.info>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Library General Public
  * License version 2 as published by the Free Software Foundation.
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
#ifndef BACKENDPERSISTHANDLER_H
#define BACKENDPERSISTHANDLER_H

#define KWMAGIC_LEN 12

#include <qwindowdefs.h>

class QFile;
class KSaveFile;
namespace KWallet {

class Backend;

enum BackendCipherType {
    BACKEND_CIPHER_UNKNOWN,  /// this is used by freshly allocated wallets
    BACKEND_CIPHER_BLOWFISH, /// use the legacy blowfish cipher type
#ifdef HAVE_QGPGME
    BACKEND_CIPHER_GPG       /// use GPG backend to encrypt wallet contents
#endif // HAVE_QGPGME
};
        

class BackendPersistHandler {
protected:
    BackendPersistHandler() {}
public:
    virtual ~BackendPersistHandler() {}
    /**
     * This is a factory method used to get an instance of the backend suitable
     * for reading/writing using the given cipher type
     * 
     * @param cypherType indication of the backend that should be returned
     * @return a pointer to an instance of the requested handler type. No need to delete this pointer, it's lifetime is taken care of by this factory
     */
    static BackendPersistHandler *getPersistHandler(BackendCipherType cipherType);
    static BackendPersistHandler *getPersistHandler(char magicBuf[KWMAGIC_LEN]);
    
    virtual int write(Backend* wb, KSaveFile& sf, QByteArray& version, WId w) =0;
    virtual int read(Backend* wb, QFile& sf, WId w) =0;
};


class BlowfishPersistHandler : public BackendPersistHandler {
public:
    BlowfishPersistHandler() {}
    virtual ~BlowfishPersistHandler() {}
    
    virtual int write(Backend* wb, KSaveFile& sf, QByteArray& version, WId w);
    virtual int read(Backend* wb, QFile& sf, WId w);
};

#ifdef HAVE_QGPGME
class GpgPersistHandler : public BackendPersistHandler {
public:
    GpgPersistHandler() {}
    virtual ~GpgPersistHandler() {}
    
    virtual int write(Backend* wb, KSaveFile& sf, QByteArray& version, WId w);
    virtual int read(Backend* wb, QFile& sf, WId w);
};
#endif // HAVE_QGPGME

} // namespace

#endif // BACKENDPERSISTHANDLER_H
