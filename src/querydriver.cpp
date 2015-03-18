/*
    This file is part of the KDE

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

#include "querydriver.h"

#include <iostream>
#include <QDebug>
#include <QTimer>
#include <QDesktopWidget>
#include <QByteArray>

#include <KLocalizedString>


QueryDriver::QueryDriver(int &argc, char* argv[]) :
    QApplication(argc, argv)
    , theWallet(0)
    , verbose(false)
{
    QTimerEvent *timerEvent = new QTimerEvent(100);
    postEvent(this, timerEvent);
}

QueryDriver::~QueryDriver() {
    // NOTE: no need to close the wallet, let's leave it open for next time
    // we'll query it preventing too many annoying password prompts
    // if (theWallet != NULL && theWallet->isOpen()) {
    //     Wallet::closeWallet(walletName, false);
    // }
    delete theWallet;
}
void QueryDriver::timerEvent(QTimerEvent *event) {
    QApplication::timerEvent(event);
    if (verbose) qDebug() << "timer event";

    auto wl = Wallet::walletList();
    if (wl.indexOf(walletName) == -1) {
        std::cout << i18n("Wallet %1 not found", walletName).toStdString() << std::endl;
        exit(1);
    } else {
        if (verbose) qDebug() << "standby opening wallet " << walletName;

        theWallet = Wallet::openWallet(walletName, desktop()->screen()->winId(), Wallet::Asynchronous);
        // theWallet = Wallet::openWallet(walletName, desktop()->screen()->winId());
        connect(theWallet, SIGNAL(walletOpened(bool)), this, SLOT(walletOpened(bool)));
    }
}

void QueryDriver::setWalletName(const QString& name) {
    walletName = name;
}

void QueryDriver::setMode(Mode mode) {
    this->mode = mode;
}
void QueryDriver::walletOpened(bool success) {
    if (!success) {
        std::cout << i18n("Failed to open wallet %1. Aborting", walletName).toStdString() << std::endl;
        exit(2);
    } else {
        switch (mode) {
            case List:
                readPasswordEntries();
                break;
            case Read:
                readPasswordValue();
                break;
            case Write:
                writePasswordValue();
                break;
            default:
                Q_ASSERT(0);
        }
    }
}

void QueryDriver::readPasswordEntries() {
    theWallet = Wallet::openWallet(walletName, 0);
    auto fl = theWallet->folderList();
    if (fl.indexOf("Passwords") == -1) {
        std::cout << i18n("'Passwords' folder not found").toStdString() << std::endl;
        exit(2);
    }
    theWallet->setFolder("Passwords");
    auto el = theWallet->entryList();
    for (auto e: el) {
        if (theWallet->entryType(e) == Wallet::Password)
            std::cout << e.toStdString() << std::endl;
    }
    quit();
}

void QueryDriver::readPasswordValue() {
    if (verbose) qDebug() << "reading " << entryName << " from " << walletName;
    QString entryValue;
    theWallet->setFolder("Passwords");
    int rc = theWallet->readPassword(entryName, entryValue);
    if (rc != 0) {
        std::cout << i18n("Failed to read entry %1 value from the %2 wallet", entryName, walletName).toStdString() << std::endl;
        exit(2);
    }
    QStringList el = entryValue.split("\n", QString::SkipEmptyParts);
    for (auto e : el) {
        std::cout << e.toStdString() << std::endl;
    }
    quit();
}

void QueryDriver::writePasswordValue() {
    if (verbose) qDebug() << "writing " << entryName << " to " << walletName;
    theWallet->setFolder("Passwords");
    QString passwordContents;
    for (std::string line; std::getline(std::cin, line); ) {
        if (!passwordContents.isEmpty()) passwordContents += '\n';
        passwordContents += QString::fromStdString(line);
        if (!std::cin) break;
    }
    if (verbose) qDebug() << "  about to write " << passwordContents;
    int rc = theWallet->writePassword(entryName, passwordContents);
    if (rc != 0) {
        std::cout << i18n("Failed to write entry %1 value to %2 wallet", entryName, walletName).toStdString() << std::endl;
        exit(2);
    }
    quit();
}
