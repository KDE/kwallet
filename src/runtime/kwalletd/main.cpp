/*
    SPDX-FileCopyrightText: 2024 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kwalletd.h"

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char **argv)
{
    QApplication application(argc, argv);

    KConfig cfg(QStringLiteral("kwalletrc"));
    KConfigGroup migrationGroup(&cfg, QStringLiteral("Migration"));
    const bool useKWalletBackend = migrationGroup.readEntry("UseKWalletBackend", true);
    if (useKWalletBackend) {
        qputenv("SECRET_SERVICE_BUS_NAME", "org.kde.secretservicecompat");
    }

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

    KDBusService dbusUniqueInstance(KDBusService::Unique);

    KWalletD wallet(useKWalletBackend);

    return application.exec();
}
