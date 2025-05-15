/*
    SPDX-FileCopyrightText: 2024 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kwalletd.h"

#include <KAboutData>
#include <KConfig>
#include <KConfigGroup>
#include <KDBusService>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>

static bool isWalletEnabled()
{
    KConfig cfg(QStringLiteral("kwalletrc"));
    KConfigGroup walletGroup(&cfg, QStringLiteral("Wallet"));
    return walletGroup.readEntry(QStringLiteral("Enabled"), true);
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);

    KLocalizedString::setApplicationDomain("kwalletd");

    KAboutData aboutData(QStringLiteral("kwalletd"),
                         i18n("kwalletd"),
                         QStringLiteral("0.1"),
                         i18n("A KWallet compatibility service, wrapping upon Secret Service"),
                         KAboutLicense::LGPL,
                         i18n("(C) 2025, The KDE Developers"));

    aboutData.addAuthor(i18n("Marco Martin"), i18n("Author"), QStringLiteral("notmart@gmail.com"));
    aboutData.setOrganizationDomain("kde.org");
    aboutData.setDesktopFileName(QStringLiteral("org.kde.kwalletd"));

    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);

    parser.process(application);
    aboutData.processCommandLine(&parser);

    // check if kwallet is disabled
    if (!isWalletEnabled()) {
        qCDebug(KWALLETD_LOG) << "kwallet is disabled!";

        return (-1);
    }

    KDBusService dbusUniqueInstance(KDBusService::Unique);

    KWalletD wallet;

    return application.exec();
}
