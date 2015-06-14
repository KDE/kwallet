/*
    This file is part of the KDE

    Copyright (C) 2015 Valentin Rusu (kde@rusu.info)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
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

#include <QByteArray>
#include <QDebug>
#include <QDesktopWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <KLocalizedString>


QueryDriver::QueryDriver(int &argc, char* argv[]) :
    QApplication(argc, argv)
    , theWallet(0)
    , verbose(false)
    , entryFolder("Passwords")
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
        std::cout << i18n("Wallet %1 not found", walletName).toUtf8().constData() << std::endl;
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
        std::cout << i18n("Failed to open wallet %1. Aborting", walletName).toUtf8().constData() << std::endl;
        exit(2);
    } else {
        switch (mode) {
            case List:
                readEntries();
                break;
            case Read:
                readValue();
                break;
            case Write:
                writeValue();
                break;
            default:
                Q_ASSERT(0);
        }
    }
}

void QueryDriver::readEntries() {
    theWallet = Wallet::openWallet(walletName, 0);
    if (entryFolder.isEmpty()) {
      auto fl = theWallet->folderList();
      for (auto f: fl) {
          std::cout << f.toUtf8().constData() << std::endl;
          Q_ASSERT(theWallet->setFolder(f));
          auto el = theWallet->entryList();
          for (auto e: el) {
              std::cout << "\t" << e.toUtf8().constData() << std::endl;
          }
      }
    } else {
      if (!theWallet->setFolder(entryFolder)) {
        std::cout << i18n("The folder %1 doeos not exist!", entryFolder).toUtf8().constData() << std::endl;
        exit(4);
      }
      auto el = theWallet->entryList();
      for (auto e: el) {
          std::cout << e.toUtf8().constData() << std::endl;
      }
    }
    quit();
}

void QueryDriver::readValue() {
    if (verbose) qDebug() << "reading" << entryName << "from" << entryFolder << "from" << walletName;
    if (!theWallet->setFolder(entryFolder)) {
      std::cout << i18n("The folder %1 does not exist!", entryFolder).toUtf8().constData() << std::endl;
      exit(4);
    }
    Wallet::EntryType kind = theWallet->entryType(entryName);
    if (kind == Wallet::Password) {
        readPasswordValue();
    } else if (kind == Wallet::Map) {
        readMapValue();
    } else {
        std::cout << i18n("Failed to read entry %1 value from the %2 wallet.", entryName, walletName).toUtf8().constData() << std::endl;
        exit(4);
    }
    quit();
}

void QueryDriver::readMapValue() {
    QMap<QString, QString> map;
    int rc = theWallet->readMap(entryName, map);
    if (rc != 0) {
        std::cout << i18n("Failed to read entry %1 value from the %2 wallet", entryName, walletName).toUtf8().constData() << std::endl;
        exit(4);
    }
    QJsonObject json;
    for (auto e : map.keys()) {
        json.insert(e, QJsonValue::fromVariant(QVariant(map.value(e))));
    }
    std::cout << QJsonDocument(json).toJson().constData() << std::endl;
}

void QueryDriver::readPasswordValue() {
    QString entryValue;
    int rc = theWallet->readPassword(entryName, entryValue);
    if (rc != 0) {
        std::cout << i18n("Failed to read entry %1 value from the %2 wallet", entryName, walletName).toUtf8().constData() << std::endl;
        exit(4);
    }
    QStringList el = entryValue.split("\n", QString::SkipEmptyParts);
    for (auto e : el) {
        std::cout << e.toUtf8().constData() << std::endl;
    }
}

void QueryDriver::writeValue() {
    if (verbose) qDebug() << "writing" << entryName << "to" << entryFolder << "to" << walletName;
    theWallet->setFolder(entryFolder);

    QString passwordContents;
    for (std::string line; std::getline(std::cin, line); ) {
        if (!passwordContents.isEmpty()) passwordContents += '\n';
        passwordContents += QString::fromStdString(line);
        if (!std::cin) break;
    }
    Wallet::EntryType kind = theWallet->entryType(entryName);
    if (kind == Wallet::Password) {
        if (verbose) qDebug() << "about to write" << passwordContents;
        int rc = theWallet->writePassword(entryName, passwordContents);
        if (rc != 0) {
            std::cout << i18n("Failed to write entry %1 value to %2 wallet", entryName, walletName).toUtf8().constData() << std::endl;
            exit(4);
        }
    } else if (kind == Wallet::Map) {
        const QJsonDocument json = QJsonDocument::fromJson(passwordContents.toLatin1());
        if (!json.isNull()) {
            QJsonObject values = json.object();
            QMap<QString, QString> map;
            for (auto e : values.keys()) {
                map.insert(e, values.value(e).toString());
            }
            if (verbose) qDebug() << "about to write" << map;
            int rc = theWallet->writeMap(entryName, map);
            if (rc != 0) {
                std::cout << i18n("Failed to write entry %1 value to %2 wallet", entryName, walletName).toUtf8().constData() << std::endl;
                exit(4);
            }
        }
    }
    quit();
}
