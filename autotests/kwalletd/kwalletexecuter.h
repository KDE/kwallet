/*************************************************************************************
 *  Copyright (C) 2014 by Alejandro Fiestas Olivares <afiestas@kde.org>              *
 *  Copyright (C) 2015 by Martin Klapetek <mklapetek@kde.org>                        *
 *                                                                                   *
 *  This program is free software; you can redistribute it and/or                    *
 *  modify it under the terms of the GNU General Public License                      *
 *  as published by the Free Software Foundation; either version 2                   *
 *  of the License, or (at your option) any later version.                           *
 *                                                                                   *
 *  This program is distributed in the hope that it will be useful,                  *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
 *  GNU General Public License for more details.                                     *
 *                                                                                   *
 *  You should have received a copy of the GNU General Public License                *
 *  along with this program; if not, write to the Free Software                      *
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA   *
 *************************************************************************************/

#ifndef KWALLET_EXECUTER
#define KWALLET_EXECUTER

#include <QObject>
#include <QDBusMessage>
#include <QDBusConnection>
/**
 * Executes kwalletd exactly in the same way it is executed in the pam module.
 * This makes the integration test closer to the real thing.
 */
class KWalletExecuter : public QObject
{
    Q_OBJECT

protected:
    KWalletExecuter(QObject *parent=0);
    void execute();
    void pamOpen();
    void pamWrite(const QString &value) const;
    void pamRead(const QString &value) const;

protected Q_SLOTS:
    void cleanupTestCase();

private:
    void execute_kwallet(int toWalletPipe[2], int envSocket);

    int m_handler;

// public:
//     void qFatal(const char* arg1);
};
#endif //KWALLET_EXECUTER
