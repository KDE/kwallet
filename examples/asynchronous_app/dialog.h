/* This file is part of the KDE project
 *
 * Copyright (C) 2010-2012 Martin Sandsmark <martin.sandsmark@kde.org>
 * Copyright (C) 2018 Olivier Churlaud <olivier@churlaud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
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

#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>

namespace KWallet {
    class Wallet;
}

class Dialog : public QDialog
{
Q_OBJECT
public:
    Dialog(QWidget *parent = nullptr);

private Q_SLOTS:
    void doSave();
    void walletOpened(bool ok);

private:
    KWallet::Wallet *m_wallet;
    QLineEdit *m_keyInput;
    QLineEdit *m_valueInput;
    QLabel *m_statusLabel;
    QPushButton *m_launchButton;
};

#endif // DIALOG_H
