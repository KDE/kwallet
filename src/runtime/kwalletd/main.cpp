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

#include <kuniqueapplication.h>
#include <k4aboutdata.h>
#include <kcmdlineargs.h>
#include <kdebug.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <klocale.h>

#include "kwalletd.h"

static bool isWalletEnabled()
{
	KConfig cfg("kwalletrc");
	KConfigGroup walletGroup(&cfg, "Wallet");
	return walletGroup.readEntry("Enabled", true);
}

extern "C" KDE_EXPORT int kdemain(int argc, char **argv)
{
    K4AboutData aboutdata("kwalletd", 0, ki18n("KDE Wallet Service"),
                         "0.2", ki18n("KDE Wallet Service"),
                         K4AboutData::License_LGPL, ki18n("(C) 2002-2013, The KDE Developers"));
    aboutdata.addAuthor(ki18n("Michael Leupold"),ki18n("Maintainer"),"lemma@confuego.org");
    aboutdata.addAuthor(ki18n("George Staikos"),ki18n("Former maintainer"),"staikos@kde.org");
    aboutdata.addAuthor(ki18n("Thiago Maceira"),ki18n("D-Bus Interface"),"thiago@kde.org");
    aboutdata.addAuthor(ki18n("Valentin Rusu"),ki18n("GPG backend support"),"kde@rusu.info");

    aboutdata.setProgramIconName("kwalletmanager");

    KCmdLineArgs::init( argc, argv, &aboutdata );
    KUniqueApplication::addCmdLineOptions();
    KUniqueApplication app;

    // This app is started automatically, no need for session management
    app.disableSessionManagement();
    app.setQuitOnLastWindowClosed( false );

    // check if kwallet is disabled
    if (!isWalletEnabled()) {
      kDebug() << "kwalletd is disabled!";
      return (0);
    }

    if (!KUniqueApplication::start())
    {
      kDebug() << "kwalletd is already running!";
      return (0);
    }

    kDebug() << "kwalletd started";
    KWalletD walletd;
    return app.exec();
}
