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
#include <QTimer>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <gpgme.h>
#include <gpgme++/context.h>
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>
#include <kmessagebox.h>
#include <kdebug.h>

Q_DECLARE_METATYPE(GpgME::Key)

namespace KWallet {

KNewWalletDialog::KNewWalletDialog(const QString &appName, const QString &walletName, QWidget* parent): 
    QWizard(parent), _intro(0), _introId(0), _gpg(0), _gpgId(0)
{
    setOption(HaveFinishButtonOnEarlyPages);
    _intro = new KNewWalletDialogIntro(appName, walletName, this);
    _introId = addPage(_intro);

    _gpg = new KNewWalletDialogGpg(appName, walletName, this);
    _gpgId = addPage(_gpg);
}

bool KNewWalletDialog::isBlowfish() const
{
    return _intro->isBlowfish();
}

GpgME::Key KNewWalletDialog::gpgKey() const
{
    QVariant varKey = field("key");
    return varKey.value< GpgME::Key >();
}

KNewWalletDialogIntro::KNewWalletDialogIntro(const QString& appName, const QString& walletName, QWidget* parent): QWizardPage(parent)
{
    _ui.setupUi(this);
    if (appName.isEmpty()){
        _ui.labelIntro->setText(i18n("<qt>KDE has requested to create a new wallet named '<b>%1</b>'. This is used to store sensitive data in a secure fashion. Please choose the new wallet's type below or click cancel to deny the application's request.</qt>", Qt::escape(walletName)));
    } else {
        _ui.labelIntro->setText(i18n("<qt>The application '<b>%1</b>' has requested to create a new wallet named '<b>%2</b>'. This is used to store sensitive data in a secure fashion. Please choose the new wallet's type below or click cancel to deny the application's request.</qt>", Qt::escape(appName), Qt::escape(walletName)));
    }
}

void KNewWalletDialogIntro::onBlowfishToggled(bool blowfish)
{
    setFinalPage(blowfish);
}


bool KNewWalletDialogIntro::isBlowfish() const
{
    return _ui.radioBlowfish->isChecked();
}

int KNewWalletDialogIntro::nextId() const
{
    if (isBlowfish()){
        return -1;
    } else {
        return qobject_cast< const KNewWalletDialog* >(wizard())->gpgId();
    }
}

KNewWalletDialogGpg::KNewWalletDialogGpg(const QString& appName, const QString& walletName, QWidget* parent): 
    QWizardPage(parent), _alreadyInitialized(false), _complete(false)
{
    _ui.setupUi(this);
}

struct AddKeyToList {
    QTableWidget *_list;
    int _row;
    AddKeyToList(QTableWidget *list) : _list(list), _row(0) {}
    void operator()( const GpgME::Key &k) {
        GpgME::UserID uid = k.userID(0);
        QString name(uid.name());
        if (uid.comment()){
            name = QString("%1 (%2)").arg(name).arg(uid.comment());
        }
        _list->setItem(_row, 0, new QTableWidgetItem(name));
        _list->setItem(_row, 1, new QTableWidgetItem(uid.email()));
        _list->setItem(_row, 2, new QTableWidgetItem(k.shortKeyID()));
        QVariant varKey;
        varKey.setValue(k);
        _list->item(0, 0)->setData(Qt::UserRole, varKey);
        ++_row;
    }
};

void KNewWalletDialogGpg::initializePage()
{
    if (_alreadyInitialized)
        return;
    
    registerField("key", this);
    
    GpgME::initializeLibrary();
    GpgME::Error err = GpgME::checkEngine(GpgME::OpenPGP);
    if (err){
        kDebug() << "OpenPGP not supported on your system!";
        KMessageBox::error(this, i18n("The QGpgME library failed to initialize for the OpenPGP protocol. Please check your system's configuration then try again."));
        emit completeChanged();
        return;
    }
    _ctx = GpgME::Context::createForProtocol(GpgME::OpenPGP);
    if (0 == _ctx) {
        KMessageBox::error(this, i18n("The QGpgME library failed to initialize for the OpenPGP protocol. Please check your system's configuration then try again."));
        emit completeChanged();
        return;
    }
    _ctx->setKeyListMode(GPGME_KEYLIST_MODE_LOCAL);

    std::vector< GpgME::Key > keys;
    int row =0;
    err = _ctx->startKeyListing();
    while (!err) {
        GpgME::Key k = _ctx->nextKey(err);
        if (err)
            break;
        if (!k.isInvalid() && k.canEncrypt()) {
            keys.push_back(k);
        }
    }
    _ctx->endKeyListing();
    
    if (keys.size() == 0) {
        KMessageBox::error(this, i18n("Seems that your system has no keys suitable for encryption. Please set-up at least an encryption key, then try again."));
        emit completeChanged();
        return;
    }
    
    _ui.listCertificates->setRowCount(keys.size());
    std::for_each(keys.begin(), keys.end(), AddKeyToList(_ui.listCertificates));
    _ui.listCertificates->resizeColumnsToContents();
    _ui.listCertificates->setCurrentCell(0, 0);
    
    _alreadyInitialized = true;
}

void KNewWalletDialogGpg::onItemSelectionChanged()
{
    _complete = _ui.listCertificates->currentRow() >= 0;
    QVariant varKey = _ui.listCertificates->item(_ui.listCertificates->currentRow(), 0)->data(Qt::UserRole);
    setField("key", varKey);
    emit completeChanged();
}

bool KNewWalletDialogGpg::isComplete() const
{
    return _complete;
}

bool KNewWalletDialogGpg::validateCurrentPage()
{
    return false;
}


} // namespace
#include "moc_knewwalletdialog.cpp"
