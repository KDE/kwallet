/**
  * This file is part of the KDE project
  * Copyright (C) 2013 Valentin Rusu <kde@rusu.info>
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

#include "knewwalletdialog.h"
#include <klocalizedstring.h>
#include <QLabel>
#include <QTextDocument>

namespace KWallet {

KNewWalletDialog::KNewWalletDialog(const QString &appName, const QString &walletName, QWidget* parent): QDialog(parent)
{
    ui.setupUi(this);

    if (appName.isEmpty()){
        ui.labelIntro->setText(i18n("<qt>KDE has requested to create a new wallet named '<b>%1</b>'. This is used to store sensitive data in a secure fashion. Please choose the new wallet's type below or click cancel to deny the application's request.</qt>", Qt::escape(walletName)));
    } else {
        ui.labelIntro->setText(i18n("<qt>The application '<b>%1</b>' has requested to create a new wallet named '<b>%2</b>'. This is used to store sensitive data in a secure fashion. Please choose the new wallet's type below or click cancel to deny the application's request.</qt>", Qt::escape(appName), Qt::escape(walletName)));
    }
}

bool KNewWalletDialog::isBlowfish() const
{
    return ui.radioBlowfish->isChecked();
}

} // namespace
