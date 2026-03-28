/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   MeshMC - A Custom Launcher for Minecraft
 *   Copyright (C) 2026 Project Tick
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "AppearancePage.h"
#include "ui_AppearancePage.h"

#include "Application.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/ThemeManager.h"

AppearancePage::AppearancePage(QWidget *parent) : QWidget(parent), ui(new Ui::AppearancePage)
{
    ui->setupUi(this);

    connect(ui->themeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AppearancePage::onThemeFamilyChanged);

    loadSettings();
}

AppearancePage::~AppearancePage()
{
    delete ui;
}

bool AppearancePage::apply()
{
    applySettings();
    return true;
}

void AppearancePage::applySettings()
{
    auto s = APPLICATION->settings();

    // Apply color theme
    QString newThemeId;
    if (ui->variantComboBox->isVisible() && ui->variantComboBox->count() > 0)
    {
        newThemeId = ui->variantComboBox->currentData().toString();
    }
    else
    {
        newThemeId = ui->themeComboBox->currentData().toString();
    }

    auto originalAppTheme = s->get("ApplicationTheme").toString();
    if (!newThemeId.isEmpty() && originalAppTheme != newThemeId)
    {
        s->set("ApplicationTheme", newThemeId);
        APPLICATION->setApplicationTheme(newThemeId, false);
    }

    // Apply icon theme
    auto originalIconTheme = s->get("IconTheme").toString();
    auto tm = APPLICATION->themeManager();
    // Resolve the correct icon variant based on currently active color theme
    QString iconFamily = ui->iconThemeComboBox->currentText();
    QString newIconTheme = tm->resolveIconTheme(iconFamily);
    if (!newIconTheme.isEmpty() && originalIconTheme != newIconTheme)
    {
        s->set("IconTheme", newIconTheme);
        APPLICATION->setIconTheme(newIconTheme);
    }
}

void AppearancePage::loadSettings()
{
    auto s = APPLICATION->settings();
    auto tm = APPLICATION->themeManager();

    // Load color theme families
    ui->themeComboBox->blockSignals(true);
    ui->themeComboBox->clear();

    auto currentThemeId = s->get("ApplicationTheme").toString();
    auto currentTheme = tm->getTheme(currentThemeId);

    auto familyList = tm->families();
    int familyIdx = 0;
    QString currentFamily;
    if (currentTheme)
    {
        currentFamily = currentTheme->family();
    }

    for (int i = 0; i < familyList.size(); i++)
    {
        const auto& family = familyList[i];
        auto themes = tm->themesInFamily(family);
        // Store the first theme's id as data for single-theme families
        QString dataId = themes.empty() ? QString() : themes[0]->id();
        ui->themeComboBox->addItem(family, dataId);
        if (family == currentFamily)
        {
            familyIdx = i;
        }
    }

    ui->themeComboBox->setCurrentIndex(familyIdx);
    ui->themeComboBox->blockSignals(false);

    // Populate variants for the selected family
    onThemeFamilyChanged(familyIdx);

    // Select the current variant
    if (currentTheme && !currentTheme->variant().isEmpty())
    {
        for (int i = 0; i < ui->variantComboBox->count(); i++)
        {
            if (ui->variantComboBox->itemData(i).toString() == currentThemeId)
            {
                ui->variantComboBox->setCurrentIndex(i);
                break;
            }
        }
    }

    // Load icon theme families
    ui->iconThemeComboBox->clear();
    auto currentIconTheme = s->get("IconTheme").toString();
    auto iconFamilies = tm->iconThemeFamilies();
    int iconFamilyIdx = 0;

    // Find the current icon theme's family
    QString currentIconFamily;
    for (const auto& entry : tm->iconThemes())
    {
        if (entry.id == currentIconTheme)
        {
            currentIconFamily = entry.family;
            break;
        }
    }

    for (int i = 0; i < iconFamilies.size(); i++)
    {
        auto entries = tm->iconThemesInFamily(iconFamilies[i]);
        QString dataId = entries.isEmpty() ? QString() : entries[0].id;
        ui->iconThemeComboBox->addItem(iconFamilies[i], dataId);
        if (iconFamilies[i] == currentIconFamily)
        {
            iconFamilyIdx = i;
        }
    }

    ui->iconThemeComboBox->setCurrentIndex(iconFamilyIdx);
}

void AppearancePage::onThemeFamilyChanged(int index)
{
    auto tm = APPLICATION->themeManager();
    auto familyList = tm->families();

    if (index < 0 || index >= familyList.size())
    {
        ui->variantComboBox->setVisible(false);
        ui->labelVariant->setVisible(false);
        return;
    }

    auto themes = tm->themesInFamily(familyList[index]);

    bool hasVariants = false;
    for (auto* theme : themes)
    {
        if (!theme->variant().isEmpty())
        {
            hasVariants = true;
            break;
        }
    }

    ui->variantComboBox->clear();
    if (hasVariants && themes.size() > 1)
    {
        ui->variantComboBox->setVisible(true);
        ui->labelVariant->setVisible(true);
        for (auto* theme : themes)
        {
            QString label = theme->variant().isEmpty() ? theme->name() : theme->variant();
            ui->variantComboBox->addItem(label, theme->id());
        }
    }
    else
    {
        ui->variantComboBox->setVisible(false);
        ui->labelVariant->setVisible(false);
    }
}
