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

#include "migrationagent.h"
#include "migrationwizard.h"
#include "kwalletd.h"

#include <KConfig>
#include <KConfigGroup>
#include <QTimer>
#include <QApplication>

#include <KLocalizedString>
#include <KWallet>

#define SERVICE_KWALLETD4 "org.kde.kwalletd"
#define ENTRY_ALREADY_MIGRATED "alreadyMigrated"

MigrationAgent::MigrationAgent(KWalletD* kd) :
  _kf5_daemon(kd)
  , _kde4_daemon(0)
{
  if (isAlreadyMigrated()) {
    qDebug() << "old wallets were already migrated";
  } else {
    QTimer::singleShot(100, this, SLOT(migrateWallets()));
  }
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
  {
    if (connectOldDaemon()) {
      if (!isEmptyOldWallet()) {
        if (isMigrationWizardOk()) {
          setAlreadyMigrated();
        } else {
          qDebug() << "Migration wizard returned an error or has been canceled. The migration agent will resume upon next daemon start";
        }
      } else {
        qDebug() << "Old wallet is empty. No need to migrate.";
        setAlreadyMigrated();
      }
    } else {
      qDebug() << "KDE4 kwalletd not present, stopping migration agent";
    }
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
    // NOTE we provide a "fake" org.kde.kwalletd.service file - see the project structure
    // however, this thing is a HACK as we cannot tell here, in KF5, user's KDE4 install prefix
    // the provided .service file assumes /usr/bin
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

    MigrationWizard *wizard = new MigrationWizard(this);
    int result = wizard->exec();
    if (QDialog::Accepted == result) {
        // the user either migrated the wallets, or choose not to be prompted again
        ok = true;
    }

    return ok;
}

void MigrationAgent::emitProgressMessage(QString msg)
{
    emit progressMessage(msg);
}

class MigrationException {
    public: MigrationException(QString msg): _msg(msg) {}
    QString _msg;
};

MigrationAgent *migrationAgent = NULL;

template <typename R, typename F> R invokeAndCheck(F f, QString errorMsg) {
    QDBusPendingReply<R> reply = f();
    reply.waitForFinished();
    QApplication::processEvents(); // keep UI responsive to show progress messages
    if (!reply.isValid()) {
        migrationAgent->emitProgressMessage(errorMsg);
        throw MigrationException(errorMsg);
    }
    return reply.value();
}

bool MigrationAgent::isEmptyOldWallet() const {
    QStringList wallets;
    try {
      wallets = invokeAndCheck<QStringList>(
              [this] { return _kde4_daemon->wallets(); },
              i18n("Cannot read old wallet list. Aborting."));
    } catch (MigrationException ex) {
        return true;
    }

    return wallets.length() == 0;
}

bool MigrationAgent::performMigration(WId wid)
{
    auto appId = i18n("KDE Wallet Migration Agent");
    try {
        migrationAgent = this;
        QStringList wallets = invokeAndCheck<QStringList>(
                [this] { return _kde4_daemon->wallets(); },
                i18n("Cannot read old wallet list. Aborting."));

        foreach (const QString &wallet, wallets) {
            emit progressMessage(i18n("Migrating wallet: %1", wallet));
            emit progressMessage(i18n("* Creating KF5 wallet: %1", wallet));

            int handle5 = _kf5_daemon->internalOpen(appId, wallet, false, 0, true, QString());
            if (handle5 <0) {
                emit progressMessage(i18n("ERROR when attempting new wallet creation. Aborting."));
                return false;
            }

            int handle4 = invokeAndCheck<int>(
                [this, wallet, wid, appId] { return _kde4_daemon->open(wallet, wid, appId); },
                i18n("Cannot open KDE4 wallet named: %1", wallet));
            emit progressMessage(i18n("* Opened KDE4 wallet: %1", wallet));

            const QStringList folders = invokeAndCheck<QStringList>(
                [this, handle4, appId] { return _kde4_daemon->folderList(handle4, appId); },
                i18n("Cannot retrieve folder list. Aborting."));

            foreach (const QString &folder, folders) {
                emit progressMessage(i18n("* Migrating folder %1", folder));

                QStringList entries = invokeAndCheck<QStringList>(
                    [this, handle4, folder, appId] { return _kde4_daemon->entryList(handle4, folder, appId); },
                    i18n("Cannot retrieve folder %1 entries. Aborting.", folder));

                foreach (const QString &key, entries) {

                    int entryType = invokeAndCheck<int>(
                        [this, handle4, folder, key, appId] { return _kde4_daemon->entryType(handle4, folder, key, appId); },
                        i18n("Cannot retrieve key %1 info. Aborting.", key));

                    // handle the case where the entries are already there
                    if (_kf5_daemon->hasEntry(handle5, folder, key, appId)) {
                        emit progressMessage(i18n("* SKIPPING entry %1 in folder %2 as it seems already migrated", key, folder));
                    } else {
                        QByteArray entry = invokeAndCheck<QByteArray>(
                            [this, handle4, folder, key, appId] { return _kde4_daemon->readEntry(handle4, folder, key, appId); },
                                                                      i18n("Cannot retrieve key %1 data. Aborting.", key));
                        if ( _kf5_daemon->writeEntry(handle5, folder, key, entry, entryType, appId) != 0 ) {
                            auto msg = i18n("Cannot write entry %1 in the new wallet. Aborting.", key);
                            emit progressMessage(msg);
                            throw MigrationException(msg);
                        }
                    }
                }
            }

            //_kde4_daemon->close(handle4, false, appId);
            //_kf5_daemon->close(handle5, true, appId);
            _kf5_daemon->sync(handle5, appId);
            emit progressMessage(i18n("DONE migrating wallet\n"));
        }
    } catch (MigrationException ex) {
        return false;
    }
    return true;
}

