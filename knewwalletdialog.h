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
#ifndef KNEWWALLETDIALOG_H
#define KNEWWALLETDIALOG_H

#include <QDialog>

#include "ui_knewwalletdialog.h"

namespace KWallet {

class KNewWalletDialog : public QDialog {
    Q_OBJECT
public:
    explicit KNewWalletDialog(const QString &appName, const QString &walletName, QWidget* parent = 0);

    bool isBlowfish() const;
private:    
    Ui_KNewWalletDialog ui;
};

} // namespace

#endif // KNEWWALLETDIALOG_H
