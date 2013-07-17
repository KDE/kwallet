
#include <QIODevice>
#include <QFile>
#include <assert.h>
#include <ksavefile.h>
#include "backendpersisthandler.h"
#include "kwalletbackend.h"
#include "blowfish.h"
#include "sha1.h"
#include "cbc.h"

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
        return gpgHandler;
    }
#endif // HAVE_QGPGME
    return 0;    // unknown cipher or hash
}
  
int BlowfishPersistHandler::write(Backend* wb, KSaveFile& sf, QByteArray& version)
{
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
        wholeFile.fill(0);
        return -4; // write error
    }

    wholeFile.fill(0);

    return 0;
}


int BlowfishPersistHandler::read(Backend* wb, QFile& db)
{
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
int GpgPersistHandler::write(Backend* wb, KSaveFile& sf, QByteArray& version)
{
    return 0;
}

int GpgPersistHandler::read(Backend* wb, QFile& sf)
{
    return 0;
}
#endif // HAVE_QGPGME

} // namespace

