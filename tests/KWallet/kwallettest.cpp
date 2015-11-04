/*
    This file is part of the KDE Libraries

    Copyright (C) 2007 Thomas McGuire (thomas.mcguire@gmx.net)

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
#include "kwallettest.h"

#include <QtTest>
#include <QWidget>
#include <QtCore/qglobal.h>

#include <kwallet.h>

using namespace KWallet;

void KWalletTest::init()
{
    if (!qEnvironmentVariableIsSet("DISPLAY")) {
        QSKIP("$DISPLAY is not set. These tests cannot be done without a graphical system.");
    }
}

void KWalletTest::testWallet()
{
    QString testWallet = QStringLiteral("kdewallet");
    QString testFolder = QStringLiteral("wallettestfolder");
    QString testKeys[] = { QStringLiteral("testKey"), QStringLiteral("account-302948"), QStringLiteral("\\"), QStringLiteral("/abc"),
                           QStringLiteral("a@b.c")
                         };
    QByteArray testValues[] = { "test", "@(!§\"%&", "", ".test", "\\" };
    int numTests = 5;

    // open
    Wallet *wallet = Wallet::openWallet(testWallet, 0, Wallet::Synchronous);
    if (wallet == 0) {
        qDebug() << "Couldn't open the wallet. Maybe the wallet daemon is not running?";
    }
    QVERIFY2(wallet != 0, "openWallet failed!");
    QVERIFY2(Wallet::isOpen(testWallet), "opwnWallet succeeded but the wallet !isOpen");

    // create folder
    wallet->createFolder(testFolder);
    QVERIFY2(wallet->hasFolder(testFolder), "Failed to create testFolder");
    wallet->setFolder(testFolder);
    QVERIFY2(wallet->currentFolder() == testFolder, "Failed to set current testFolder");
    QVERIFY2(wallet->folderList().contains(testFolder), "The wallet does not contain freshly created testFolder");

    // write & read many entries
    for (int i = 0; i < numTests; i++) {
        wallet->writeEntry(testKeys[i], testValues[i]);
        QVERIFY2(wallet->hasEntry(testKeys[i]), "hasEntry failed!");
        QByteArray readEntry;
        wallet->readEntry(testKeys[i], readEntry);
        QVERIFY2(readEntry == testValues[i], "readEntry failed!");
    }

    // close
    wallet->sync();
    Wallet::closeWallet(QStringLiteral("kdewallet"), true);
    QVERIFY2(!Wallet::isOpen("kdewallet"), "wallet is still opened after close call!");

    // test for key - closed wallet
    for (int i = 0; i < 5; i++) {
        QVERIFY2(!Wallet::keyDoesNotExist(testWallet, testFolder, testKeys[i]), "keyDoesNotExist(1) failed");
        QVERIFY2(Wallet::keyDoesNotExist(testWallet, testFolder, "madeUpKey"), "keyDoesNotExist(2) failed");
        QVERIFY2(Wallet::keyDoesNotExist(testWallet, "madeUpFolderName", "madeUpKey"), "keyDoesNotExist(3) failed");
        QVERIFY2(Wallet::keyDoesNotExist(testWallet, "madeUpFolderName", testKeys[i]), "keyDoesNotExist(4) failed");
    }

    // open
    wallet = Wallet::openWallet(testWallet, 0, Wallet::Synchronous);
    QVERIFY2(wallet != 0, "openWallet failed");
    QVERIFY2(Wallet::isOpen(testWallet), "openWallet succeeded but the wallet !isOpen (2)");

    // set folder
    QVERIFY2(wallet->hasFolder(testFolder), "The wallet do not have testFolder!");
    wallet->setFolder(testFolder);
    QVERIFY2(wallet->currentFolder() == testFolder, "Failed to set current folder");

    // test for key - opened wallet
    for (int i = 0; i < numTests; i++) {
        QVERIFY2(!Wallet::keyDoesNotExist(testWallet, testFolder, testKeys[i]), "keyDoesNotExist(1) failed");
        QVERIFY2(Wallet::keyDoesNotExist(testWallet, testFolder, "madeUpKey"), "keyDoesNotExist(2) failed");
        QVERIFY2(Wallet::keyDoesNotExist(testWallet, "madeUpFolderName", "madeUpKey"), "keyDoesNotExist(3) failed");
        QVERIFY2(Wallet::keyDoesNotExist(testWallet, "madeUpFolderName", testKeys[i]), "keyDoesNotExist(4) failed");
    }

    // read many keys
    for (int i = 0; i < numTests; i++) {
        QByteArray readEntry;
        wallet->readEntry(testKeys[i], readEntry);
        QVERIFY2(readEntry == testValues[i], "Test value after read many keys failed!");
    }

    // delete folder
    wallet->removeFolder(testFolder);
    QVERIFY2(!wallet->hasFolder(testFolder), "Failed to delete the testFolder");

    // close
    Wallet::closeWallet(QStringLiteral("kdewallet"), true);
    QVERIFY2(!Wallet::isOpen("kdewallet"), "Failed to close wallet");
}

QTEST_GUILESS_MAIN(KWalletTest)

