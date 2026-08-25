/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Valentin Rusu <kde@rusu.info>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "knewwalletdialog.h"
#include "ksecretd_debug.h"
#include <KLocalizedString>
#include <KMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <algorithm>
#include <gpgme++/context.h>
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>

Q_DECLARE_METATYPE(GpgME::Key)

namespace KWallet
{
KNewWalletDialog::KNewWalletDialog(const QString &walletName, QWidget *parent)
    : QWizard(parent)
{
    setOption(HaveFinishButtonOnEarlyPages);
    _intro = new KNewWalletDialogIntro(walletName, this);
    _introId = addPage(_intro);

    _gpg = new KNewWalletDialogGpg(this);
    _gpgId = addPage(_gpg);
}

bool KNewWalletDialog::isBlowfish() const
{
    return _intro->isBlowfish();
}

GpgME::Key KNewWalletDialog::gpgKey() const
{
    QVariant varKey = field(QStringLiteral("key"));
    return varKey.value<GpgME::Key>();
}

KNewWalletDialogIntro::KNewWalletDialogIntro(const QString &walletName, QWidget *parent)
    : QWizardPage(parent)
{
    _ui.setupUi(this);
    _ui.labelIntro->setText(
        i18n("<qt>KDE has requested to create a new wallet named '<b>%1</b>'. "
             "This is used to securely store sensitive data, such as account passwords. "
             "Choose the type of the new wallet below or click Cancel to deny the application's request.</qt>",
             walletName.toHtmlEscaped()));
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
    if (isBlowfish()) {
        return -1;
    } else {
        return qobject_cast<const KNewWalletDialog *>(wizard())->gpgId();
    }
}

KNewWalletDialogGpg::KNewWalletDialogGpg(QWidget *parent)
    : QWizardPage(parent)
{
    _ui.setupUi(this);
    registerField(QStringLiteral("key"), this);
}

struct AddKeyToList {
    QTableWidget *_list;
    int _row;
    AddKeyToList(QTableWidget *list)
        : _list(list)
        , _row(0)
    {
    }
    void operator()(const GpgME::Key &k)
    {
        GpgME::UserID uid = k.userID(0);
        QString name(uid.name());
        if (uid.comment()) {
            name = QStringLiteral("%1 (%2)").arg(name, uid.comment());
        }
        _list->setItem(_row, 0, new QTableWidgetItem(name));
        _list->setItem(_row, 1, new QTableWidgetItem(uid.email()));
        _list->setItem(_row, 2, new QTableWidgetItem(k.shortKeyID()));
        QVariant varKey;
        varKey.setValue(k);
        _list->item(_row, 0)->setData(Qt::UserRole, varKey);
        ++_row;
    }
};

void KNewWalletDialogGpg::initializePage()
{
    GpgME::initializeLibrary();
    GpgME::Error err = GpgME::checkEngine(GpgME::OpenPGP);
    if (err) {
        qCDebug(KSECRETD_LOG) << "OpenPGP not supported on your system!";
        KMessageBox::error(this,
                           i18n("The GpgME library failed to initialize for the OpenPGP protocol. Please check your system's configuration then try again."));
        Q_EMIT completeChanged();
        return;
    }
    _ctx = std::shared_ptr<GpgME::Context>(GpgME::Context::createForProtocol(GpgME::OpenPGP));
    if (!_ctx) {
        KMessageBox::error(this,
                           i18n("The GpgME library failed to initialize for the OpenPGP protocol. Please check your system's configuration then try again."));
        Q_EMIT completeChanged();
        return;
    }
    _ctx->setKeyListMode(GpgME::Local);

    std::vector<GpgME::Key> keys = getKeys();

    if (keys.size() == 0 && !_alreadyPrompted) {
        createGpgKeyDialog();
        _alreadyPrompted = true;
        Q_EMIT completeChanged();
    }
    connect(_ui.reloadCertificates, &QPushButton::released, this, &KNewWalletDialogGpg::loadKeys, Qt::UniqueConnection);
    loadKeys();
}

std::vector<GpgME::Key> KNewWalletDialogGpg::getKeys()
{
    std::vector<GpgME::Key> keys;
    GpgME::Error err = _ctx->startKeyListing();
    while (!err) {
        GpgME::Key k = _ctx->nextKey(err);
        if (err) {
            break;
        }
        if (!k.isInvalid() && k.canEncrypt() && (k.ownerTrust() == GpgME::Key::Ultimate)) {
            keys.push_back(k);
        }
    }
    _ctx->endKeyListing();
    return keys;
}

void KNewWalletDialogGpg::loadKeys()
{
    std::vector<GpgME::Key> keys = getKeys();

    _complete = false;
    setField(QStringLiteral("key"), QVariant());
    _ui.listCertificates->clearContents();
    _ui.listCertificates->setRowCount(keys.size());
    std::for_each(keys.begin(), keys.end(), AddKeyToList(_ui.listCertificates));
    _ui.listCertificates->resizeColumnsToContents();
    if (!keys.empty()) {
        _ui.listCertificates->setCurrentCell(0, 0);
    }
    onItemSelectionChanged();
}

void KNewWalletDialogGpg::createGpgKeyDialog()
{
    KMessageBox::error(this,
                       xi18nc("@info",
                              "No suitable GPG encryption keys were found. To create one, choose one of the following options:<list>"
                              "<item>Install Kleopatra and use it to create a new key. See these"
                              " <link url='https://docs.kde.org/stable_kf6/en/kleopatra/kleopatra/functions-newkey.html'>instructions</link>.</item>"
                              "<item>Open a terminal and enter <command>gpg --full-gen-key</command>.</item>"
                              "<item>Go back and use Blowfish encryption instead, which only requires a password.</item></list>"));
}

void KNewWalletDialogGpg::onItemSelectionChanged()
{
    const int currentRow = _ui.listCertificates->currentRow();
    const auto item = currentRow >= 0 ? _ui.listCertificates->item(currentRow, 0) : nullptr;
    _complete = item != nullptr;
    const QVariant varKey = item ? item->data(Qt::UserRole) : QVariant();
    setField(QStringLiteral("key"), varKey);
    Q_EMIT completeChanged();
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
