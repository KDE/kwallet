/**
  * This file is part of the KDE project
  * Copyright (C) 2008 Michael Leupold <lemma@confuego.org>
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

#include <QApplication>
#include <QDebug>
#include <QtCore/QString>
#include <KLocalizedString>
#include <KAboutData>
#include <KConfig>
#include <KConfigGroup>
#include <KDBusService>

#include "kwalletd.h"
#include "kwalletd_version.h"

static bool isWalletEnabled()
{
    KConfig cfg("kwalletrc");
    KConfigGroup walletGroup(&cfg, "Wallet");
    return walletGroup.readEntry("Enabled", true);
}

#ifdef HAVE_KF5INIT
extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    QApplication app(argc, argv);
    app.setApplicationName("kwalletd");
    app.setApplicationDisplayName(i18n("KDE Wallet Service"));
    app.setOrganizationDomain("kde.org");
    app.setApplicationVersion(KWALLETD_VERSION_STRING);

    KAboutData aboutdata(I18N_NOOP("kwalletd"),
                         i18n("KDE Wallet Service"),
                         KWALLETD_VERSION_STRING,
                         i18n("KDE Wallet Service"),
                         KAboutLicense::LGPL,
                         i18n("(C) 2002-2013, The KDE Developers"));
    aboutdata.addAuthor(i18n("Valentin Rusu"), i18n("Maintainer, GPG backend support"), "kde@rusu.info");
    aboutdata.addAuthor(i18n("Michael Leupold"), i18n("Former Maintainer"), "lemma@confuego.org");
    aboutdata.addAuthor(i18n("George Staikos"), i18n("Former maintainer"), "staikos@kde.org");
    aboutdata.addAuthor(i18n("Thiago Maceira"), i18n("D-Bus Interface"), "thiago@kde.org");

    KWalletD walletd;
    KDBusService dbusUniqueInstance(KDBusService::Unique | KDBusService::NoExitOnFailure);

    // NOTE: the command should be parsed only after KDBusService instantiation
    QCommandLineParser cmdParser;
    aboutdata.setupCommandLine(&cmdParser);
    cmdParser.process(app);

    aboutdata.setProgramIconName("kwalletmanager");

    app.setQuitOnLastWindowClosed(false);

    // check if kwallet is disabled
    if (!isWalletEnabled()) {
        qDebug() << "kwalletd is disabled!";
        return (0);
    }

    if (!dbusUniqueInstance.isRegistered()) {
        qDebug() << "kwalletd is already running!";
        return 1;
    }

    qDebug() << "kwalletd started";
    return app.exec();
}
