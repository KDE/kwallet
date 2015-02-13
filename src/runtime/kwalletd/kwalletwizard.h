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

#ifndef KWALLETWIZARD_H
#define KWALLETWIZARD_H

#include <QWizard>
#ifdef HAVE_QGPGME
#include <gpgme++/key.h>
#endif

class PageGpgKey;
class PagePassword;
class PageIntro;

class KWalletWizard : public QWizard
{
    Q_OBJECT
public:

    enum WizardType {
        Basic,
        Advanced
    };

    static const int PageIntroId = 0;
    static const int PagePasswordId = 1;
#ifdef HAVE_QGPGME
    static const int PageGpgKeyId = 2;
#endif
    static const int PageOptionsId = 3;
    static const int PageExplanationId = 4;

    KWalletWizard(QWidget *parent = 0);

    WizardType wizardType() const;

#ifdef HAVE_QGPGME
    GpgME::Key gpgKey() const;
#endif // HAVE_QGPGME

protected:
    void initializePage(int id) Q_DECL_OVERRIDE;

protected Q_SLOTS:
    void passwordPageUpdate();

private:
    PageIntro *m_pageIntro;
    PagePassword *m_pagePasswd;
#ifdef HAVE_QGPGME
    PageGpgKey *m_pageGpgKey;
#endif
};

#endif
