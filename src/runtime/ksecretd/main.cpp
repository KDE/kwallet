/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2008 Michael Leupold <lemma@confuego.org>
    SPDX-FileCopyrightText: 2014 Alex Fiestas <afiestas@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "ksecretd_debug.h"
#include <KAboutData>
#include <KConfig>
#include <KConfigGroup>
#include <KCrash>
#include <KDBusService>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QString>

#include <stdio.h>

#include "../kwalletbackend/kwalletbackend.h" //For the hash size
#include "ksecretd.h"
#include "ksecretd_version.h"
#include "kwalletfreedesktopservice.h"

#ifndef Q_OS_WIN
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define BSIZE 1000
static int pipefd = 0;
static int socketfd = 0;
#endif

#ifndef Q_OS_WIN
// Waits until the PAM_MODULE sends the hash
static char *waitForHash()
{
    qCDebug(KSECRETD_LOG) << "ksecretd5: Waiting for hash on" << pipefd;
    int totalRead = 0;
    int readBytes = 0;
    int attempts = 0;
    char *buf = (char *)malloc(sizeof(char) * PBKDF2_SHA512_KEYSIZE);
    memset(buf, '\0', PBKDF2_SHA512_KEYSIZE);
    while (totalRead != PBKDF2_SHA512_KEYSIZE) {
        readBytes = read(pipefd, buf + totalRead, PBKDF2_SHA512_KEYSIZE - totalRead);
        if (readBytes == -1 || attempts > 5) {
            free(buf);
            return nullptr;
        }
        totalRead += readBytes;
        ++attempts;
    }

    close(pipefd);
    return buf;
}

// Waits until startkde sends the environment variables
static int waitForEnvironment()
{
    qCDebug(KSECRETD_LOG) << "ksecretd5: waitingForEnvironment on:" << socketfd;

    int s2;
    struct sockaddr_un remote;
    socklen_t t = sizeof(remote);
    if ((s2 = accept(socketfd, (struct sockaddr *)&remote, &t)) == -1) {
        qCWarning(KSECRETD_LOG) << "ksecretd5: Couldn't accept incoming connection";
        return -1;
    }
    qCDebug(KSECRETD_LOG) << "ksecretd5: client connected";

    char str[BSIZE] = {'\0'};

    int chop = 0;
    FILE *s3 = fdopen(dup(s2), "r");
    while (!feof(s3)) {
        if (fgets(str, BSIZE, s3)) {
            chop = strlen(str) - 1;
            if (str[chop] == '\n') {
                str[chop] = '\0';
            }
            putenv(strdup(str));
        }
    }
    fclose(s3);

    qCDebug(KSECRETD_LOG) << "ksecretd5: client disconnected";
    close(socketfd);
    return 1;
}

char *checkPamModule(int argc, char **argv)
{
    qCDebug(KSECRETD_LOG) << "ksecretd5: Checking for pam module";
    char *hash = nullptr;
    int x = 1;
    for (; x < argc; ++x) {
        if (strcmp(argv[x], "--pam-login") != 0) {
            continue;
        }
        qCDebug(KSECRETD_LOG) << "ksecretd5: Got pam-login param";
        argv[x] = nullptr;
        x++;
        // We need at least 2 extra arguments after --pam-login
        if (x + 1 > argc) {
            qCWarning(KSECRETD_LOG) << "ksecretd5: Invalid arguments (less than needed)";
            return nullptr;
        }

        // first socket for the hash, comes from a pipe
        pipefd = atoi(argv[x]);
        argv[x] = nullptr;
        x++;
        // second socket for environment, comes from a localsocket
        socketfd = atoi(argv[x]);
        argv[x] = nullptr;
        break;
    }

    if (!pipefd || !socketfd) {
        qCWarning(KSECRETD_LOG) << "Lacking a socket, pipe:" << pipefd << "env:" << socketfd;
        return nullptr;
    }

    hash = waitForHash();

    if (hash == nullptr || waitForEnvironment() == -1) {
        qCWarning(KSECRETD_LOG) << "ksecretd5: Hash or environment not received";
        free(hash);
        return nullptr;
    }

    return hash;
}
#endif

int main(int argc, char **argv)
{
    char *hash = nullptr;
#ifndef Q_OS_WIN
    if (getenv("PAM_KWALLET5_LOGIN")) {
        hash = checkPamModule(argc, argv);
    }
#endif

    QCoreApplication::setAttribute(Qt::AA_DisableSessionManager);
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("kwalletmanager")));

    // this ksecretd5 program should be able to start with KDE4's ksecretd
    // using ksecretd name would prevent KDBusService unique instance to initialize
    // so we setApplicationName("ksecretd6")
    KAboutData aboutdata("ksecretd",
                         i18n("KDE Wallet Service"),
                         KSECRETD_VERSION_STRING,
                         i18n("KDE Wallet Service"),
                         KAboutLicense::LGPL,
                         i18n("(C) 2002-2025, The KDE Developers"));
    aboutdata.addAuthor(i18n("Valentin Rusu"), i18n("Former Maintainer, GPG backend support"), QStringLiteral("kde@rusu.info"));
    aboutdata.addAuthor(i18n("Michael Leupold"), i18n("Former Maintainer"), QStringLiteral("lemma@confuego.org"));
    aboutdata.addAuthor(i18n("George Staikos"), i18n("Former maintainer"), QStringLiteral("staikos@kde.org"));
    aboutdata.addAuthor(i18n("Thiago Maceira"), i18n("D-Bus Interface"), QStringLiteral("thiago@kde.org"));

    KAboutData::setApplicationData(aboutdata);

    KCrash::initialize();

    // BUG 509680: When launched by PAM (hash != nullptr), use Replace so that
    // if a premature D-Bus-activated instance grabbed the bus name, the PAM
    // instance takes over (it has the password hash for auto-unlock).
    KDBusService dbusUniqueInstance(hash
        ? (KDBusService::Unique | KDBusService::Replace)
        : KDBusService::Unique);

    // NOTE: the command should be parsed only after KDBusService instantiation
    QCommandLineParser cmdParser;
    aboutdata.setupCommandLine(&cmdParser);
    cmdParser.process(app);

    app.setQuitOnLastWindowClosed(false);

    // check if kwallet is disabled
    if (!KSecretD::isEnabled()) {
        qCDebug(KSECRETD_LOG) << "ksecretd is disabled!";

        /* Do not keep dbus-daemon waiting for the org.freedesktop.secrets if kwallet is disabled */
        KWalletFreedesktopService(nullptr);

        return (-1);
    }

    KSecretD secretd;
    qCDebug(KSECRETD_LOG) << "ksecretd6 started";

    if (hash) {
        QByteArray passHash(hash, PBKDF2_SHA512_KEYSIZE);
        int wallet = secretd.pamOpen(KWallet::Wallet::LocalWallet(), passHash, 0);
        if (wallet < 0) {
            qCWarning(KSECRETD_LOG) << "Wallet failed to get opened by PAM, error code is" << wallet;
        } else {
            qCDebug(KSECRETD_LOG) << "Wallet opened by PAM";
        }
        free(hash);
    }

    return app.exec();
}
