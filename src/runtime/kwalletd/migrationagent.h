/*
   This file is part of the KDE Frameworks

   Copyright (c) 2014 Valentin Rusu <kde@rusu.info>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.

*/
#ifndef _MIGRATIONAGENT_H_
#define _MIGRATIONAGENT_H_

#include <kwallet_interface.h>

#include <QWidget>

class KWalletD;


class MigrationAgent : public QObject {
    Q_OBJECT
public:
    MigrationAgent(KWalletD* kd);

    bool performMigration(WId wid);

private Q_SLOTS:
    void migrateWallets();
    bool isAlreadyMigrated();
    bool connectOldDaemon();
    bool isMigrationWizardOk();
    void setAlreadyMigrated();
public Q_SLOTS:
    void emitProgressMessage(QString);

Q_SIGNALS:
    void progressMessage(QString);

private:
    KWalletD		*_kf5_daemon;
    org::kde::KWallet 	*_kde4_daemon;
};

#endif // _MIGRATIONAGENT_H_
