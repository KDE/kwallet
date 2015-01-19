/* This file is part of the KDE libraries
   Copyright (C) 2004 George Staikos <staikos@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "kwalletwizard.h"

#include "ui_kwalletwizardpageexplanation.h"
#include "ui_kwalletwizardpageintro.h"
#include "ui_kwalletwizardpageoptions.h"
#include "ui_kwalletwizardpagepassword.h"
#ifdef HAVE_QGPGME
#include "ui_kwalletwizardpagepasswordgpg.h"
#include "ui_kwalletwizardpagegpgkey.h"
#endif

#include <KLocalizedString>
#include <KIconLoader>
#include <QButtonGroup>
#include <QDebug>

#ifdef HAVE_QGPGME
#include <QComboBox>
#include <gpgme++/context.h>
#include <gpgme++/key.h>
#include <gpgme++/keylistresult.h>
#include <QDebug>
#include <kmessagebox.h>
#include <gpgme.h>
#endif

class PageIntro : public QWizardPage
{
public:
    PageIntro(QWidget *parent)
        : QWizardPage(parent)
    {
        ui.setupUi(this);

        ui.ktitlewidget->setText("<h1>" + i18n("KWallet") + "</h1>");

        int iconSize = 3 * fontMetrics().height();
        // round to multiple of 16
        iconSize = (iconSize + 8) & ~15;
        QPixmap pix = KIconLoader::global()->loadIcon("kwalletmanager", KIconLoader::Dialog, iconSize);
        ui.ktitlewidget->setPixmap(pix);

        bg = new QButtonGroup(this);
        bg->setExclusive(true);
        bg->addButton(ui._basic, 0);
        bg->addButton(ui._advanced, 1);

        // force the "basic" button to be selected
        ui._basic->setChecked(true);
    }

    QButtonGroup *bg;

private:
    Ui::KWalletWizardPageIntro ui;
};

class PagePassword : public QWizardPage
{
public:
    PagePassword(QWidget *parent)
        : QWizardPage(parent)
    {

        ui.setupUi(this);

        registerField("useWallet", ui._useWallet);
        registerField("pass1", ui._pass1);
        registerField("pass2", ui._pass2);
#ifdef HAVE_QGPGME
        registerField("useGPG", ui._radioGpg);
        registerField("useBlowfish", ui._radioBlowfish);
        connect(ui._radioBlowfish, SIGNAL(toggled(bool)), parent, SLOT(passwordPageUpdate()));
#endif

        connect(ui._useWallet, SIGNAL(clicked()), parent, SLOT(passwordPageUpdate()));
        connect(ui._pass1, SIGNAL(textChanged(QString)), parent, SLOT(passwordPageUpdate()));
        connect(ui._pass2, SIGNAL(textChanged(QString)), parent, SLOT(passwordPageUpdate()));
        ui._useWallet->setChecked(true);
    }

    virtual int nextId() const
    {
#ifdef HAVE_QGPGME
        int nextId = -1;
        if (field("useWallet").toBool()) {
            if (field("useBlowfish").toBool()) {
                nextId = static_cast<KWalletWizard *>(wizard())->wizardType() == KWalletWizard::Basic ? -1 : KWalletWizard::PageOptionsId; // same as non QGPGME case
            } else {
                nextId = KWalletWizard::PageGpgKeyId;
            }
        }

        return nextId;
#else
        return static_cast<KWalletWizard *>(wizard())->wizardType() == KWalletWizard::Basic ? -1 : KWalletWizard::PageOptionsId;
#endif
    }

    void setMatchLabelText(const QString &text)
    {
        ui._matchLabel->setText(text);
    }

private:
#ifdef HAVE_QGPGME
    Ui::KWalletWizardPagePasswordGpg ui;
#else
    Ui::KWalletWizardPagePassword ui;
#endif
};

#ifdef HAVE_QGPGME
typedef std::vector< GpgME::Key > KeysVector;
Q_DECLARE_METATYPE(GpgME::Key)

struct AddKeyToCombo {
    QComboBox *_list;
    AddKeyToCombo(QComboBox *list) : _list(list) {}
    void operator()(const GpgME::Key &k)
    {
        QString text = QString("%1 (%2)").arg(k.shortKeyID()).arg(k.userID(0).email());
        QVariant varKey;
        varKey.setValue(k);
        _list->addItem(text, varKey);
    }
};

class PageGpgKey : public QWizardPage
{
public:
    PageGpgKey(QWidget *parent)
        : QWizardPage(parent)
        , userHasGpgKeys(false)
    {
        ui.setupUi(this);

        registerField("gpgKey", ui._gpgKey);

        KeysVector keys;
        GpgME::initializeLibrary();
        GpgME::Error err = GpgME::checkEngine(GpgME::OpenPGP);
        if (err) {
            qDebug() << "OpenPGP not supported on your system!";
            KMessageBox::error(this, i18n("The QGpgME library failed to initialize for the OpenPGP protocol. Please check your system's configuration then try again."));
        } else {
            boost::shared_ptr< GpgME::Context > ctx(GpgME::Context::createForProtocol(GpgME::OpenPGP));
            if (0 == ctx) {
                KMessageBox::error(this, i18n("The QGpgME library failed to initialize for the OpenPGP protocol. Please check your system's configuration then try again."));
            } else {

                ctx->setKeyListMode(GPGME_KEYLIST_MODE_LOCAL);
                err = ctx->startKeyListing();
                while (!err) {
                    GpgME::Key k = ctx->nextKey(err);
                    if (err) {
                        break;
                    }
                    if (!k.isInvalid() && k.canEncrypt() && (k.ownerTrust() == GpgME::Key::Ultimate)) {
                        keys.push_back(k);
                    }
                }
                ctx->endKeyListing();
            }
        }
        std::for_each(keys.begin(), keys.end(), AddKeyToCombo(ui._gpgKey));

        userHasGpgKeys = keys.size() > 0;
        if (userHasGpgKeys) {
            ui.stackedWidget->setCurrentWidget(ui._pageWhenHasKeys);
        } else {
            ui.stackedWidget->setCurrentWidget(ui._pageNoKeys);
            setFinalPage(true);
        }
        emit completeChanged();
    }

    virtual int nextId() const
    {
        return static_cast<KWalletWizard *>(wizard())->wizardType() == KWalletWizard::Basic ? -1 : KWalletWizard::PageOptionsId;
    }

    virtual bool isComplete() const
    {
        return userHasGpgKeys;
    }

    bool hasGpgKeys() const
    {
        return userHasGpgKeys;
    }

    GpgME::Key gpgKey() const
    {
        QVariant varKey = ui._gpgKey->itemData(field("gpgKey").toInt());
        return varKey.value< GpgME::Key >();
    }
private:
    Ui::KWalletWizardPageGpgKey ui;
    bool userHasGpgKeys;
};
#endif

class PageOptions : public QWizardPage
{
public:
    PageOptions(QWidget *parent)
        : QWizardPage(parent)
    {
        ui.setupUi(this);

        registerField("closeWhenIdle", ui._closeIdle);
        registerField("networkWallet", ui._networkWallet);
    }

private:
    Ui::KWalletWizardPageOptions ui;
};

class PageExplanation : public QWizardPage
{
public:
    PageExplanation(QWidget *parent)
        : QWizardPage(parent)
    {
        ui.setupUi(this);
        setFinalPage(true);
    }

private:
    Ui::KWalletWizardPageExplanation ui;
};

KWalletWizard::KWalletWizard(QWidget *parent)
    : QWizard(parent)
{
    setOption(HaveFinishButtonOnEarlyPages);

    m_pageIntro = new PageIntro(this);
    setPage(PageIntroId, m_pageIntro);
    m_pagePasswd = new PagePassword(this);
    setPage(PagePasswordId, m_pagePasswd);
#ifdef HAVE_QGPGME
    m_pageGpgKey = new PageGpgKey(this);
    setPage(PageGpgKeyId, m_pageGpgKey);
#endif
    setPage(PageOptionsId, new PageOptions(this));
    setPage(PageExplanationId, new PageExplanation(this));

    resize(500, 420);
}

void KWalletWizard::passwordPageUpdate()
{
    bool complete = true;
    if (field("useWallet").toBool()) {
#ifdef HAVE_QGPGME
        if (field("useBlowfish").toBool()) {
            m_pagePasswd->setFinalPage(wizardType() == Basic);
            button(NextButton)->setVisible(wizardType() != Basic);
#endif
            if (field("pass1").toString() == field("pass2").toString()) {
                if (field("pass1").toString().isEmpty()) {
                    m_pagePasswd->setMatchLabelText(i18n("<qt>Password is empty.  <b>(WARNING: Insecure)</b></qt>"));
                } else {
                    m_pagePasswd->setMatchLabelText(i18n("Passwords match."));
                }
            } else {
                m_pagePasswd->setMatchLabelText(i18n("Passwords do not match."));
                complete = false;
            }
#ifdef HAVE_QGPGME
        } else {
            m_pagePasswd->setFinalPage(false);
            button(NextButton)->setEnabled(true);
            return;
        }
#endif
    } else {
        m_pagePasswd->setMatchLabelText(QString());
    }
    button(wizardType() == Basic ? FinishButton : NextButton)->setEnabled(complete);
}

KWalletWizard::WizardType KWalletWizard::wizardType() const
{
    return (KWalletWizard::WizardType)m_pageIntro->bg->checkedId();
}

void KWalletWizard::initializePage(int id)
{
    switch (id) {
    case PagePasswordId: {
        bool islast = m_pageIntro->bg->checkedId() == 0;
        m_pagePasswd->setFinalPage(islast);
        button(NextButton)->setVisible(!islast);
        break;
    }
    }
}

#ifdef HAVE_QGPGME
GpgME::Key KWalletWizard::gpgKey() const
{
    return m_pageGpgKey->gpgKey();
}
#endif
