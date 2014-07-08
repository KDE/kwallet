/*
 *   This file is part of the KDE Frameworks
 * 
 *   Copyright (c) 2014 Valentin Rusu <kde@rusu.info>
 * 
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 * 
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 * 
 *   You should have received a copy of the GNU Library General Public License
 *   along with this library; see the file COPYING.LIB.  If not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 * 
 */

#include "migrationwizard.h"
#include "ui_migrationwizard1.h"
#include "ui_migrationwizard2.h"

class MigrationPage1 : public QWizardPage {
public:
    MigrationPage1(QWidget *wizard) 
        : QWizardPage(wizard) 
        {
            _ui.setupUi(this);
            connect(_ui._optionNo, SIGNAL(toggled(bool)), wizard, SLOT(page1Updated()));
            connect(_ui._optionNotInterested, SIGNAL(toggled(bool)), wizard, SLOT(page1Updated()));
            connect(_ui._optionYes, SIGNAL(toggled(bool)), wizard, SLOT(page1Updated()));
        }

    Ui::MigrationPage1 _ui;
};

class MigrationPage2 : public QWizardPage {
public:
    MigrationPage2(QWidget* wizard)
        : QWizardPage(wizard) 
        {
            _ui.setupUi(this);
        }
    Ui::MigrationPage2 _ui;
};

MigrationPage1 *page1 = NULL;
MigrationPage2 *page2 = NULL;

MigrationWizard::MigrationWizard()
{
    setOption(HaveFinishButtonOnEarlyPages);
    page1 = new MigrationPage1(this);
    addPage(page1);
    
    page2 = new MigrationPage2(this);
    addPage(page2);
}

MigrationWizard::~MigrationWizard()
{
    delete page1;
    delete page2;
}

void MigrationWizard::page1Updated()
{
    if (page1->_ui._optionYes->isChecked()) {
        page1->setFinalPage(false);
        button(NextButton)->setEnabled(true);
    }
    if (page1->_ui._optionNo->isChecked()) {
        page1->setFinalPage(true);
    }
    if (page1->_ui._optionNotInterested->isChecked()) {
        page1->setFinalPage(true);
        button(NextButton)->setEnabled(true);
    }
}


#include "migrationwizard.moc"
