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

#include <QIODevice>
#include <QFile>
#include <QTextDocument>
#include <assert.h>
#include <ksavefile.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <klocalizedstring.h>
#ifdef HAVE_QGPGME
#include <gpgme.h>
#include <gpgme++/context.h>
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>
#include <gpgme++/data.h>
#include <gpgme++/encryptionresult.h>
#include <gpgme++/decryptionresult.h>
#endif
#include "backendpersisthandler.h"
#include "kwalletbackend.h"
#include "blowfish.h"
#include "sha1.h"
#include "cbc.h"

#ifdef Q_OS_WIN 
#include <windows.h>
#include <wincrypt.h>
#endif

#define KWALLET_CIPHER_BLOWFISH_CBC 0
#define KWALLET_CIPHER_3DES_CBC     1 // unsupported
#define KWALLET_CIPHER_GPG          2

#define KWALLET_HASH_SHA1       0
#define KWALLET_HASH_MD5        1 // unsupported

namespace KWallet {

static int getRandomBlock(QByteArray& randBlock) {

#ifdef Q_OS_WIN //krazy:exclude=cpp

    // Use windows crypto API to get randomness on win32
    // HACK: this should be done using qca
    HCRYPTPROV hProv;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
        CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) return -1; // couldn't get random data

    if (!CryptGenRandom(hProv, static_cast<DWORD>(randBlock.size()),
        (BYTE*)randBlock.data())) {
        return -3; // read error
    }

    // release the crypto context
    CryptReleaseContext(hProv, 0);

    return randBlock.size();

#else

    // First try /dev/urandom
    if (QFile::exists("/dev/urandom")) {
        QFile devrand("/dev/urandom");
        if (devrand.open(QIODevice::ReadOnly)) {
            int rc = devrand.read(randBlock.data(), randBlock.size());

            if (rc != randBlock.size()) {
                return -3;      // not enough data read
            }

            return 0;
        }
    }

    // If that failed, try /dev/random
    // FIXME: open in noblocking mode!
    if (QFile::exists("/dev/random")) {
        QFile devrand("/dev/random");
        if (devrand.open(QIODevice::ReadOnly)) {
            int rc = 0;
            int cnt = 0;

            do {
                int rc2 = devrand.read(randBlock.data() + rc, randBlock.size());

                if (rc2 < 0) {
                    return -3;  // read error
                }

                rc += rc2;
                cnt++;
                if (cnt > randBlock.size()) {
                    return -4;  // reading forever?!
                }
            } while(rc < randBlock.size());

            return 0;
        }
    }

    // EGD method
    QString randFilename = QString::fromLocal8Bit(qgetenv("RANDFILE"));
    if (!randFilename.isEmpty()) {
        if (QFile::exists(randFilename)) {
            QFile devrand(randFilename);
            if (devrand.open(QIODevice::ReadOnly)) {
                int rc = devrand.read(randBlock.data(), randBlock.size());
                if (rc != randBlock.size()) {
                    return -3;      // not enough data read
                }
                return 0;
            }
        }
    }

    // Couldn't get any random data!!
    return -1;

#endif
}



static BlowfishPersistHandler *blowfishHandler =0;
#ifdef HAVE_QGPGME
static GpgPersistHandler *gpgHandler =0;
#endif // HAVE_QGPGME

BackendPersistHandler *BackendPersistHandler::getPersistHandler(BackendCipherType cipherType)
{
    switch (cipherType){
        case BACKEND_CIPHER_BLOWFISH:
            if (0 == blowfishHandler)
                blowfishHandler = new BlowfishPersistHandler;
            return blowfishHandler;
#ifdef HAVE_QGPGME
        case BACKEND_CIPHER_GPG:
            if (0 == gpgHandler)
                gpgHandler = new GpgPersistHandler;
            return gpgHandler;
#endif // HAVE_QGPGME
        default:
            Q_ASSERT(0);
            return 0;
    }
}
    
BackendPersistHandler *BackendPersistHandler::getPersistHandler(char magicBuf[12])
{
    if (magicBuf[2] == KWALLET_CIPHER_BLOWFISH_CBC && 
        magicBuf[3] == KWALLET_HASH_SHA1) {
        if (0 == blowfishHandler)
            blowfishHandler = new BlowfishPersistHandler;
        return blowfishHandler;
    }
#ifdef HAVE_QGPGME
    if (magicBuf[2] == KWALLET_CIPHER_GPG &&
        magicBuf[3] == 0) {
        if (0 == gpgHandler)
            gpgHandler = new GpgPersistHandler;
        return gpgHandler;
    }
#endif // HAVE_QGPGME
    return 0;    // unknown cipher or hash
}
  
int BlowfishPersistHandler::write(Backend* wb, KSaveFile& sf, QByteArray& version, WId)
{
    assert(wb->_cipherType == BACKEND_CIPHER_BLOWFISH);

    version[2] = KWALLET_CIPHER_BLOWFISH_CBC;
    version[3] = KWALLET_HASH_SHA1;
    if (sf.write(version, 4) != 4) {
        sf.abort();
        return -4; // write error
    }

    // Holds the hashes we write out
    QByteArray hashes;
    QDataStream hashStream(&hashes, QIODevice::WriteOnly);
    KMD5 md5;
    hashStream << static_cast<quint32>(wb->_entries.count());

    // Holds decrypted data prior to encryption
    QByteArray decrypted;

    // FIXME: we should estimate the amount of data we will write in each
    // buffer and resize them approximately in order to avoid extra
    // resizes.

    // populate decrypted
    QDataStream dStream(&decrypted, QIODevice::WriteOnly);
    for (Backend::FolderMap::ConstIterator i = wb->_entries.constBegin(); i != wb->_entries.constEnd(); ++i) {
        dStream << i.key();
        dStream << static_cast<quint32>(i.value().count());

        md5.reset();
        md5.update(i.key().toUtf8());
        hashStream.writeRawData(reinterpret_cast<const char*>(&(md5.rawDigest()[0])), 16);
        hashStream << static_cast<quint32>(i.value().count());

        for (Backend::EntryMap::ConstIterator j = i.value().constBegin(); j != i.value().constEnd(); ++j) {
            dStream << j.key();
            dStream << static_cast<qint32>(j.value()->type());
            dStream << j.value()->value();

            md5.reset();
            md5.update(j.key().toUtf8());
            hashStream.writeRawData(reinterpret_cast<const char*>(&(md5.rawDigest()[0])), 16);
        }
    }

    if (sf.write(hashes, hashes.size()) != hashes.size()) {
        sf.abort();
        return -4; // write error
    }

    // calculate the hash of the file
    SHA1 sha;
    BlowFish _bf;
    CipherBlockChain bf(&_bf);

    sha.process(decrypted.data(), decrypted.size());

    // prepend and append the random data
    QByteArray wholeFile;
    long blksz = bf.blockSize();
    long newsize = decrypted.size() +
               blksz            +    // encrypted block
               4                +    // file size
               20;      // size of the SHA hash

    int delta = (blksz - (newsize % blksz));
    newsize += delta;
    wholeFile.resize(newsize);

    QByteArray randBlock;
    randBlock.resize(blksz+delta);
    if (getRandomBlock(randBlock) < 0) {
        sha.reset();
        decrypted.fill(0);
        sf.abort();
        return -3;      // Fatal error: can't get random
    }

    for (int i = 0; i < blksz; i++) {
        wholeFile[i] = randBlock[i];
    }

    for (int i = 0; i < 4; i++) {
        wholeFile[(int)(i+blksz)] = (decrypted.size() >> 8*(3-i))&0xff;
    }

    for (int i = 0; i < decrypted.size(); i++) {
        wholeFile[(int)(i+blksz+4)] = decrypted[i];
    }

    for (int i = 0; i < delta; i++) {
        wholeFile[(int)(i+blksz+4+decrypted.size())] = randBlock[(int)(i+blksz)];
    }

    const char *hash = (const char *)sha.hash();
    for (int i = 0; i < 20; i++) {
        wholeFile[(int)(newsize - 20 + i)] = hash[i];
    }

    sha.reset();
    decrypted.fill(0);

    // encrypt the data
    if (!bf.setKey(wb->_passhash.data(), wb->_passhash.size() * 8)) {
        wholeFile.fill(0);
        sf.abort();
        return -2; // encrypt error
    }

    int rc = bf.encrypt(wholeFile.data(), wholeFile.size());
    if (rc < 0) {
        wholeFile.fill(0);
        sf.abort();
        return -2;  // encrypt error
    }

    // write the file
    if (sf.write(wholeFile, wholeFile.size()) != wholeFile.size()) {
        wholeFile.fill(0);
        sf.abort();
        return -4; // write error
    }
    if (!sf.finalize()) {
        kDebug() << "WARNING: wallet sync to disk failed! KSaveFile status was " << sf.errorString();
        wholeFile.fill(0);
        return -4; // write error
    }

    wholeFile.fill(0);

    return 0;
}


int BlowfishPersistHandler::read(Backend* wb, QFile& db, WId)
{
    wb->_cipherType = BACKEND_CIPHER_BLOWFISH;
    wb->_hashes.clear();
    // Read in the hashes
    QDataStream hds(&db);
    quint32 n;
    hds >> n;
    if (n > 0xffff) { // sanity check
        return -43;
    }

    for (size_t i = 0; i < n; ++i) {
        KMD5::Digest d, d2; // judgment day
        MD5Digest ba;
        QMap<MD5Digest,QList<MD5Digest> >::iterator it;
        quint32 fsz;
        if (hds.atEnd()) return -43;
        hds.readRawData(reinterpret_cast<char *>(d), 16);
        hds >> fsz;
        ba = MD5Digest(reinterpret_cast<char *>(d));
        it = wb->_hashes.insert(ba, QList<MD5Digest>());
        for (size_t j = 0; j < fsz; ++j) {
            hds.readRawData(reinterpret_cast<char *>(d2), 16);
            ba = MD5Digest(reinterpret_cast<char *>(d2));
            (*it).append(ba);
        }
    }

    // Read in the rest of the file.
    QByteArray encrypted = db.readAll();
    assert(encrypted.size() < db.size());

    BlowFish _bf;
    CipherBlockChain bf(&_bf);
    int blksz = bf.blockSize();
    if ((encrypted.size() % blksz) != 0) {
        return -5;     // invalid file structure
    }

    bf.setKey((void *)wb->_passhash.data(), wb->_passhash.size()*8);

    if (!encrypted.data()) {
        wb->_passhash.fill(0);
        encrypted.fill(0);
        return -7; // file structure error
    }

    int rc = bf.decrypt(encrypted.data(), encrypted.size());
    if (rc < 0) {
        wb->_passhash.fill(0);
        encrypted.fill(0);
        return -6;  // decrypt error
    }

    const char *t = encrypted.data();

    // strip the leading data
    t += blksz;    // one block of random data

    // strip the file size off
    long fsize = 0;

    fsize |= (long(*t) << 24) & 0xff000000;
    t++;
    fsize |= (long(*t) << 16) & 0x00ff0000;
    t++;
    fsize |= (long(*t) <<  8) & 0x0000ff00;
    t++;
    fsize |= long(*t) & 0x000000ff;
    t++;

    if (fsize < 0 || fsize > long(encrypted.size()) - blksz - 4) {
        //kDebug() << "fsize: " << fsize << " encrypted.size(): " << encrypted.size() << " blksz: " << blksz;
        encrypted.fill(0);
        return -9;         // file structure error.
    }

    // compute the hash ourself
    SHA1 sha;
    sha.process(t, fsize);
    const char *testhash = (const char *)sha.hash();

    // compare hashes
    int sz = encrypted.size();
    for (int i = 0; i < 20; i++) {
        if (testhash[i] != encrypted[sz - 20 + i]) {
            encrypted.fill(0);
            sha.reset();
            return -8;         // hash error.
        }
    }

    sha.reset();

    // chop off the leading blksz+4 bytes
    QByteArray tmpenc(encrypted.data()+blksz+4, fsize);
    encrypted = tmpenc;
    tmpenc.fill(0);

    // Load the data structures up
    QDataStream eStream(encrypted);

    while (!eStream.atEnd()) {
        QString folder;
        quint32 n;

        eStream >> folder;
        eStream >> n;

        // Force initialisation
        wb->_entries[folder].clear();

        for (size_t i = 0; i < n; ++i) {
            QString key;
            KWallet::Wallet::EntryType et = KWallet::Wallet::Unknown;
            Entry *e = new Entry;
            eStream >> key;
            qint32 x = 0; // necessary to read properly
            eStream >> x;
            et = static_cast<KWallet::Wallet::EntryType>(x);

            switch (et) {
            case KWallet::Wallet::Password:
            case KWallet::Wallet::Stream:
            case KWallet::Wallet::Map:
            break;
            default: // Unknown entry
                delete e;
                continue;
            }

            QByteArray a;
            eStream >> a;
            e->setValue(a);
            e->setType(et);
            e->setKey(key);
            wb->_entries[folder][key] = e;
        }
    }

    wb->_open = true;
    encrypted.fill(0);
    return 0;
}

#ifdef HAVE_QGPGME
GpgME::Error initGpgME()
{
    GpgME::Error err;
    static bool alreadyInitialized = false;
    if (!alreadyInitialized) {
        GpgME::initializeLibrary();
        err = GpgME::checkEngine(GpgME::OpenPGP);
        if (err){
            kDebug() << "OpenPGP not supported!";
        }
        alreadyInitialized = true;
    }
    return err;
}

int GpgPersistHandler::write(Backend* wb, KSaveFile& sf, QByteArray& version, WId w)
{
    version[2] = KWALLET_CIPHER_GPG;
    version[3] = 0;
    if (sf.write(version, 4) != 4) {
        sf.abort();
        return -4; // write error
    }

    GpgME::Error err = initGpgME();
    if (err) {
        kDebug() << "initGpgME returned " << err.code();
        KMessageBox::errorWId( w, i18n("<qt>Error when attempting to initialize OpenPGP while attempting to save the wallet <b>%1</b>. Error code is <b>%2</b>. Please fix your system configuration, then try again!</qt>", Qt::escape(wb->_name), err.code()));
        sf.abort();
        return -1;
    }
    
    boost::shared_ptr< GpgME::Context > ctx( GpgME::Context::createForProtocol(GpgME::OpenPGP) );
    if (0 == ctx) {
        kDebug() << "Cannot setup OpenPGP context!";
        KMessageBox::errorWId(w, i18n("<qt>Error when attempting to initialize OpenPGP while attempting to save the wallet <b>%1</b>. Please fix your system configuration, then try again!</qt>"), Qt::escape(wb->_name));
        return -1;
    }

    assert(wb->_cipherType == BACKEND_CIPHER_GPG);
    
    QByteArray hashes;
    QDataStream hashStream(&hashes, QIODevice::WriteOnly);
    KMD5 md5;
    hashStream << static_cast<quint32>(wb->_entries.count());

    QByteArray values;
    QDataStream valueStream(&values, QIODevice::WriteOnly);
    Backend::FolderMap::ConstIterator i = wb->_entries.constBegin();
    Backend::FolderMap::ConstIterator ie = wb->_entries.constEnd();
    for ( ; i != ie; ++i) {
        valueStream << i.key();
        valueStream << static_cast<quint32>(i.value().count());

        md5.reset();
        md5.update(i.key().toUtf8());
        hashStream.writeRawData(reinterpret_cast<const char*>(&(md5.rawDigest()[0])), 16);
        hashStream << static_cast<quint32>(i.value().count());

        Backend::EntryMap::ConstIterator j = i.value().constBegin();
        Backend::EntryMap::ConstIterator je = i.value().constEnd();
        for (; j != je; ++j) {
            valueStream << j.key();
            valueStream << static_cast<qint32>(j.value()->type());
            valueStream << j.value()->value();

            md5.reset();
            md5.update(j.key().toUtf8());
            hashStream.writeRawData(reinterpret_cast<const char*>(&(md5.rawDigest()[0])), 16);
        }
    }

    QByteArray dataBuffer;
    QDataStream dataStream(&dataBuffer, QIODevice::WriteOnly);
    QString keyID(wb->_gpgKey.keyID());
    dataStream << keyID;
    dataStream << hashes;
    dataStream << values;
    
    GpgME::Data decryptedData(dataBuffer.data(), dataBuffer.size(), false);
    GpgME::Data encryptedData;
    std::vector< GpgME::Key > keys;
    keys.push_back(wb->_gpgKey);
    GpgME::EncryptionResult res = ctx->encrypt(keys, decryptedData, encryptedData, GpgME::Context::None);
    if (res.error()){
        int gpgerr = res.error().code();
        KMessageBox::errorWId( w, i18n("<qt>Encryption error while attempting to save the wallet <b>%1</b>. Error code is <b>%2 (%3)</b>. Please fix your system configuration, then try again!</qt>",
                                       Qt::escape(wb->_name), gpgerr, gpgme_strerror(gpgerr)));
        kDebug() << "GpgME encryption error: " << res.error().code();
        sf.abort();
        return -1;
    }

    char buffer[4096];
    ssize_t bytes =0;
    encryptedData.seek(0, SEEK_SET);
    while (bytes = encryptedData.read(buffer, sizeof(buffer)/sizeof(buffer[0]))){
        if (sf.write(buffer, bytes) != bytes){
            KMessageBox::errorWId( w, i18n("<qt>File handling error while attempting to save the wallet <b>%1</b>. Error was <b>%2</b>. Please fix your system configuration, then try again!</qt>", Qt::escape(wb->_name), sf.errorString()));
            sf.abort();
            return -4; // write error
        }
    }
    
    return 0;
}

int GpgPersistHandler::read(Backend* wb, QFile& sf, WId w)
{
    GpgME::Error err = initGpgME();
    if (err){
        KMessageBox::errorWId( w, i18n("<qt>Error when attempting to initialize OpenPGP while attempting to open the wallet <b>%1</b>. Error code is <b>%2</b>. Please fix your system configuration, then try again!</qt>", Qt::escape(wb->_name), err.code()));
        return -1;
    }

    wb->_cipherType = BACKEND_CIPHER_GPG;
    wb->_hashes.clear();

    // the remainder of the file is GPG encrypted. Let's decrypt it
    GpgME::Data encryptedData;
    char buffer[4096];
    ssize_t bytes = 0;
    while (bytes = sf.read(buffer, sizeof(buffer)/sizeof(buffer[0]))){
        encryptedData.write(buffer, bytes);
    }
    
  retry_label:
    boost::shared_ptr< GpgME::Context > ctx( GpgME::Context::createForProtocol(GpgME::OpenPGP) );
    if (0 == ctx) {
        KMessageBox::errorWId(w, i18n("<qt>Error when attempting to initialize OpenPGP while attempting to open the wallet <b>%1</b>. Please fix your system configuration, then try again!</qt>", Qt::escape(wb->_name)));
        kDebug() << "Cannot setup OpenPGP context!";
        return -1;
    }

    GpgME::Data decryptedData;
    encryptedData.seek(0, SEEK_SET);
    GpgME::DecryptionResult res = ctx->decrypt(encryptedData, decryptedData);
    if (res.error()){
        kDebug() << "Error decrypting message: " << res.error().asString() << ", code " << res.error().code() << ", source " << res.error().source();
        KGuiItem btnRetry(i18n("Retry"));
        // FIXME the logic here should be a little more elaborate; a dialog box should be used with "retry", "cancel", but also "troubleshoot" with options to show card status and to kill scdaemon
        int userChoice = KMessageBox::warningYesNoWId(w, i18n("<qt>Error when attempting to decrypt the wallet <b>%1</b> using GPG. If you're using a SmartCard, please ensure it's inserted then try again.<br><br>GPG error was <b>%2</b></qt>", Qt::escape(wb->_name), res.error().asString()),
            i18n("kwalletd GPG backend"), btnRetry, KStandardGuiItem::cancel());
        if (userChoice == KMessageBox::Yes) {
            decryptedData.seek(0, SEEK_SET);
            goto retry_label;
        }
        return -1;
    }
    
    decryptedData.seek(0, SEEK_SET);
    QByteArray dataBuffer;
    while (bytes = decryptedData.read(buffer, sizeof(buffer)/sizeof(buffer[0]))){
        dataBuffer.append(buffer, bytes);
    }
    
    // load the wallet from the decrypted data
    QDataStream dataStream(dataBuffer);
    QString keyID;
    QByteArray hashes;
    QByteArray values;
    dataStream >> keyID;
    dataStream >> hashes;
    dataStream >> values;

    // locate the GPG key having the ID found inside the file. This will be needed later, when writing changes to disk.
    QDataStream fileStream(&sf);
    fileStream.unsetDevice();
    kDebug() << "This wallet was encrypted using GPG key with ID " << keyID;

    ctx->setKeyListMode(GPGME_KEYLIST_MODE_LOCAL);
    std::vector< GpgME::Key > keys;
    int row =0;
    err = ctx->startKeyListing();
    while (!err) {
        GpgME::Key k = ctx->nextKey(err);
        if (err)
            break;
        if (keyID == k.keyID()){
            kDebug() << "The key was found.";
            wb->_gpgKey = k;
            break;
        }
    }
    ctx->endKeyListing();
    if (wb->_gpgKey.isNull()){
        KMessageBox::errorWId(w, i18n("<qt>Error when attempting to open the wallet <b>%1</b>. The wallet was encrypted using the GPG Key ID <b>%2</b> but this key was not found on your system.</qt>", Qt::escape(wb->_name), keyID));
        return -1;
    }

    
    QDataStream hashStream(hashes);
    QDataStream valueStream(values);
    
    quint32 hashCount;
    hashStream >> hashCount;
    if (hashCount > 0xFFFF) {
        return -43;
    }

    quint32 folderCount = hashCount;
    while (hashCount--){
        KMD5::Digest d;
        hashStream.readRawData(reinterpret_cast<char *>(d), 16);
        
        quint32 folderSize;
        hashStream >> folderSize;
        
        MD5Digest ba = MD5Digest(reinterpret_cast<char *>(d));
        QMap<MD5Digest, QList<MD5Digest> >::iterator it = wb->_hashes.insert(ba, QList<MD5Digest>());
        while (folderSize--){
            KMD5::Digest d2;
            hashStream.readRawData(reinterpret_cast<char *>(d2), 16);
            ba = MD5Digest(reinterpret_cast<char *>(d2));
            (*it).append(ba);
        }
    }
    
    while (folderCount--){
        QString folder;
        valueStream >> folder;
        
        quint32 entryCount;
        valueStream >> entryCount;
        
        wb->_entries[folder].clear();
        
        while (entryCount--){
            KWallet::Wallet::EntryType et = KWallet::Wallet::Unknown;
            Entry *e = new Entry;
            
            QString key;
            valueStream >> key;
            
            qint32 x =0; // necessary to read properly
            valueStream >> x;
            et = static_cast<KWallet::Wallet::EntryType>(x);
            
            switch (et) {
            case KWallet::Wallet::Password:
            case KWallet::Wallet::Stream:
            case KWallet::Wallet::Map:
            break;
            default: // Unknown entry
                delete e;
                continue;
            }
            
            QByteArray a;
            valueStream >> a;
            e->setValue(a);
            e->setType(et);
            e->setKey(key);
            wb->_entries[folder][key] = e;
        }
    }
    
    wb->_open = true;

    return 0;
}
#endif // HAVE_QGPGME

} // namespace
