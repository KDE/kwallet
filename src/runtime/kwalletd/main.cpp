/**
  * This file is part of the KDE project
  * Copyright (C) 2008 Michael Leupold <lemma@confuego.org>
  * Copyright (C) 2014 Alex Fiestas <afiestas@kde.org>
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "kwalletd.h"
#include "backend/kwalletbackend.h" //For the hash size

#include "kwalletd.h"
#include "kwalletd_version.h"
#include "migrationagent.h"

#define BSIZE 1000
static int pipefd = 0;
static int socketfd = 0;

static bool isWalletEnabled()
{
    KConfig cfg("kwalletrc");
    KConfigGroup walletGroup(&cfg, "Wallet");
    return walletGroup.readEntry("Enabled", true);
}

//Waits until the PAM_MODULE sends the hash
static char *waitForHash()
{
    printf("kwalletd: Waiting for hash on %d-\n", pipefd);
    int totalRead = 0;
    int readBytes = 0;
    int attempts = 0;
    char *buf = (char*)malloc(sizeof(char) * PBKDF2_SHA512_KEYSIZE);
    memset(buf, '\0', PBKDF2_SHA512_KEYSIZE);
    while(totalRead != PBKDF2_SHA512_KEYSIZE) {
        readBytes = read(pipefd, buf + totalRead, PBKDF2_SHA512_KEYSIZE - totalRead);
        if (readBytes == -1 || attempts > 5) {
            free(buf);
            return NULL;
        }
        totalRead += readBytes;
        ++attempts;
    }

    close(pipefd);
    return buf;
}

//Waits until startkde sends the environment variables
static int waitForEnvironment()
{
    printf("kwalletd: waitingForEnvironment on: %d\n", socketfd);

    int s2;
    socklen_t t;
    struct sockaddr_un remote;
    if ((s2 = accept(socketfd, (struct sockaddr *)&remote, &t)) == -1) {
        fprintf(stdout, "kwalletd: Couldn't accept incoming connection\n");
        return -1;
    }
    printf("kwalletd: client connected\n");

    char str[BSIZE];
    memset(str, '\0', sizeof(char) * BSIZE);

    int chop = 0;
    FILE *s3 = fdopen(s2, "r");
    while(!feof(s3)) {
        if (fgets(str, BSIZE, s3)) {
            chop = strlen(str) - 1;
            str[chop] = '\0';
            putenv(strdup(str));
        }
    }
    printf("kwalletd: client disconnected\n");
    close(socketfd);
    return 1;
}

char* checkPamModule(int argc, char **argv)
{
    printf("Checking for pam module\n");
    char *hash = NULL;
    int x = 1;
    for (; x < argc; ++x) {
        if (strcmp(argv[x], "--pam-login") != 0) {
            continue;
        }
        printf("Got pam-login\n");
        argv[x] = NULL;
        x++;
        //We need at least 2 extra arguments after --pam-login
        if (x + 1 > argc) {
            printf("Invalid arguments (less than needed)\n");
            return NULL;
        }

        //first socket for the hash, comes from a pipe
        pipefd = atoi(argv[x]);
        argv[x] = NULL;
        x++;
        //second socket for environment, comes from a localsocket
        socketfd = atoi(argv[x]);
        argv[x] = NULL;
        break;
    }

    if (!pipefd || !socketfd) {
        printf("Lacking a socket, pipe: %d, env:%d\n", pipefd, socketfd);
        return NULL;
    }

    hash = waitForHash();

    if (hash == NULL || waitForEnvironment() == -1) {
        printf("Hash or environment not received\n");
        free(hash);
        return NULL;
    }

    return hash;
}


#ifdef HAVE_KF5INIT
extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    char *hash = NULL;
    if (getenv("PAM_KWALLET_LOGIN")) {
        hash = checkPamModule(argc, argv);
    }

    QApplication app(argc, argv);
    // this kwalletd5 program should be able to start with KDE4's kwalletd
    // using kwalletd name would prevent KDBusService unique instance to initialize
    // so we setApplicationName("kwalletd5")
    app.setApplicationName("kwalletd5");
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
    MigrationAgent migrationAgent(&walletd);
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

    qDebug() << "kwalletd5 started";

    if (hash) {
        QByteArray passHash(hash, PBKDF2_SHA512_KEYSIZE);
        int wallet = walletd.pamOpen(KWallet::Wallet::LocalWallet(), passHash, 0);
        qDebug() << "Wallet opened by PAM";
        free(hash);
    }

    return app.exec();
}
