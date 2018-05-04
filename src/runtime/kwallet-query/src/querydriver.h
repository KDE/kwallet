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

#pragma once

#include <QApplication>

#include <KWallet>

using namespace KWallet;

class QueryDriver : public QApplication {
    Q_OBJECT
public:
    enum Mode {
        List,
        Read,
        Write
    };
    QueryDriver(int &argc, char* argv[]);
    ~QueryDriver() override;

    void setWalletName(const QString& walletName);
    void setMode(Mode mode);
    void setVerbose() { verbose = true; }
    void setEntryName(const QString& entryName) { this->entryName = entryName; }
    void setEntryFolder(const QString& entryFolder) { this->entryFolder = entryFolder; }

private:
    void timerEvent(QTimerEvent* event) override;
    void readEntries();
    void readValue();
    void readMapValue();
    void readPasswordValue();
    void writeValue();

private Q_SLOTS:
    void walletOpened(bool);

public:
    QString walletName;
    Wallet* theWallet;
    Mode mode;
    bool verbose;
    QString entryName;
    QString entryFolder;
};

