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

#include <KConfig>
#include <KConfigGroup>
#include <QTimer>

#include "migrationagent.h"

#define SERVICE_KWALLETD4 "org.kde.kwalletd"
#define ENTRY_ALREADY_MIGRATED "alreadyMigrated"

MigrationAgent::MigrationAgent(KWalletD* kd) :
  _kf5_daemon(kd)
  , _kde4_daemon(0)
{
  QTimer::singleShot(100, this, SLOT(migrateWallets()));
}

void MigrationAgent::migrateWallets()
{
  // the migration is done in several steps :
  // is the setting "alreadyMigrated" present and true ?
  // if yes, then stop here
  // if not, then is kwalletd present (the daemon from KDE4) ?
  // if not, then stop here
  // if yes, then start the migration wizard
  // if the migration wizard returns without error
  // create "alreadyMigrated=true" setting
  qDebug() << "Migration agent starting...";
  if (!isAlreadyMigrated()) {
    if (connectOldDaemon()) {
      if (isMigrationWizardOk()) {
        setAlreadyMigrated();
      } else {
        qDebug() << "Migration wizard returned an error. The migration agent will resume upon next daemon start";
      }
    } else {
      qDebug() << "KDE4 kwalletd not present, stopping migration agent";
    }
  } else {
    qDebug() << "old wallets were already migrated";
  }
  qDebug() << "Migration agent stop.";
}

bool MigrationAgent::isAlreadyMigrated()
{
    KConfig kwalletrc("kwalletrc");
    KConfigGroup cfg(&kwalletrc, "Migration");
    return cfg.readEntry<bool>(ENTRY_ALREADY_MIGRATED, false);
}

void MigrationAgent::setAlreadyMigrated()
{
    KConfig kwalletrc("kwalletrc");
    KConfigGroup cfg(&kwalletrc, "Migration");
    cfg.writeEntry(ENTRY_ALREADY_MIGRATED, true);
}

bool MigrationAgent::connectOldDaemon()
{
    // the old daemon may not have been started, so attempt start it
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (!bus->isServiceRegistered(QString::fromLatin1(SERVICE_KWALLETD4))) {
        qDebug() << "kwalletd not started. Attempting start...";
        QDBusReply<void> reply = bus->startService(SERVICE_KWALLETD4);
        if (!reply.isValid()) {
            qDebug() << "Couldn't start kwalletd: " << reply.error();
            return false;
        }
        if (!bus->isServiceRegistered(QString::fromLatin1(SERVICE_KWALLETD4))) {
            qDebug() << "The kwalletd service is still not registered after start attemtp. Aborting migration";
            return false;
        } else {
            qDebug() << "The kwalletd service has been registered";
        }
    }
    
    _kde4_daemon = new org::kde::KWallet(QString::fromLatin1(SERVICE_KWALLETD4), "/modules/kwalletd", QDBusConnection::sessionBus());
    return _kde4_daemon->isValid();
}

bool MigrationAgent::isMigrationWizardOk()
{
    bool ok = false;
    
    return ok;
}

#include "migrationagent.moc"
