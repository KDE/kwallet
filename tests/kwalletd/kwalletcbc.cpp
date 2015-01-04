/*
    This file is part of the KDE Libraries

    Copyright (C) 2015 Valentin Rusu (kde@rusu.info)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB. If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kwalletcbc.h"

#include <backend/cbc.h>
#include <backend/blowfish.h>

void KWalletCBCTest::encryptDecryptOneBlock() {
    BlockCipher *bf;
    char data[] = "OneBlock";
    char key[] = "testkey";

    bf = new BlowFish();

    bf->setKey((void *)key, 7 * 8);

    QVERIFY(bf->readyToGo());
    QVERIFY(8 == bf->encrypt((void *)data, 8));

    delete bf;
    bf = new BlowFish();
    bf->setKey((void *)key, 7 * 8);

    QVERIFY(8 == bf->decrypt((void *)data, 8));
    QVERIFY(strcmp(data, "OneBlock") == 0);
    delete bf;
}

void KWalletCBCTest::encryptDecryptMultiblock() {
    BlockCipher *bf;
    char data[] = "This is a test.";
    char key[] = "testkey";

    bf = new BlowFish();

    bf->setKey((void *)key, 7 * 8);

    QVERIFY(bf->readyToGo());
    QVERIFY(16 == bf->encrypt((void *)data, 16));

    delete bf;
    bf = new BlowFish();
    bf->setKey((void *)key, 7 * 8);

    QVERIFY(16 == bf->decrypt((void *)data, 16));
    QVERIFY(strcmp(data, "This is a test.") == 0);
    delete bf;
}

QTEST_GUILESS_MAIN(KWalletCBCTest)

