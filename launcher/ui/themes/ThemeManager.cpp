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

#include "ThemeManager.h"
#include "ITheme.h"
#include "SystemTheme.h"
#include "DarkTheme.h"
#include "BrightTheme.h"
#include "CustomTheme.h"

#include <QApplication>
#include <QDebug>
#include <QSet>
#include <xdgicon.h>

ThemeManager::ThemeManager()
{
    auto insertTheme = [this](ITheme* theme)
    {
        addTheme(std::unique_ptr<ITheme>(theme));
    };
    auto darkTheme = new DarkTheme();
    insertTheme(new SystemTheme());
    insertTheme(darkTheme);
    insertTheme(new BrightTheme());
    insertTheme(new CustomTheme(darkTheme, "custom"));

    initIconThemes();
}

void ThemeManager::addTheme(std::unique_ptr<ITheme> theme)
{
    QString id = theme->id();
    m_themes.insert(std::make_pair(id, std::move(theme)));
}

ITheme* ThemeManager::getTheme(const QString& id)
{
    auto it = m_themes.find(id);
    if (it != m_themes.end())
    {
        return it->second.get();
    }
    return nullptr;
}

void ThemeManager::setApplicationTheme(const QString& id, bool initial)
{
    auto theme = getTheme(id);
    if (theme)
    {
        theme->apply(initial);
    }
    else
    {
        qWarning() << "Tried to set invalid theme:" << id;
    }
}

void ThemeManager::setIconTheme(const QString& name)
{
    XdgIcon::setThemeName(name);
}

std::vector<ITheme*> ThemeManager::allThemes()
{
    std::vector<ITheme*> ret;
    for (auto& pair : m_themes)
    {
        ret.push_back(pair.second.get());
    }
    return ret;
}

QStringList ThemeManager::families()
{
    QStringList ret;
    QSet<QString> seen;
    for (auto& pair : m_themes)
    {
        QString fam = pair.second->family();
        if (!seen.contains(fam))
        {
            seen.insert(fam);
            ret.append(fam);
        }
    }
    return ret;
}

std::vector<ITheme*> ThemeManager::themesInFamily(const QString& family)
{
    std::vector<ITheme*> ret;
    for (auto& pair : m_themes)
    {
        if (pair.second->family() == family)
        {
            ret.push_back(pair.second.get());
        }
    }
    return ret;
}

QList<IconThemeEntry> ThemeManager::iconThemes() const
{
    return m_iconThemes;
}

QStringList ThemeManager::iconThemeFamilies() const
{
    QStringList ret;
    QSet<QString> seen;
    for (const auto& entry : m_iconThemes)
    {
        const QString& fam = entry.family;
        if (!seen.contains(fam))
        {
            seen.insert(fam);
            ret.append(fam);
        }
    }
    return ret;
}

QList<IconThemeEntry> ThemeManager::iconThemesInFamily(const QString& family) const
{
    QList<IconThemeEntry> ret;
    for (const auto& entry : m_iconThemes)
    {
        if (entry.family == family)
        {
            ret.append(entry);
        }
    }
    return ret;
}

QString ThemeManager::resolveIconTheme(const QString& family) const
{
    auto entries = iconThemesInFamily(family);
    if (entries.size() <= 1)
    {
        return entries.isEmpty() ? QString() : entries[0].id;
    }

    // Check if family has variants
    bool hasVariants = false;
    for (const auto& entry : entries)
    {
        if (!entry.variant.isEmpty())
        {
            hasVariants = true;
            break;
        }
    }

    if (!hasVariants)
    {
        return entries[0].id;
    }

    // Auto-detect based on current palette brightness
    auto windowColor = QApplication::palette().color(QPalette::Window);
    bool isDark = windowColor.lightnessF() < 0.5;

    for (const auto& entry : entries)
    {
        QString v = entry.variant.toLower();
        if (isDark && v == "dark")
            return entry.id;
        if (!isDark && v == "light")
            return entry.id;
    }

    return entries[0].id;
}

void ThemeManager::initIconThemes()
{
    m_iconThemes = {
        { "pe_colored", QObject::tr("Default"),               QObject::tr("Default"),               QString() },
        { "multimc",    QStringLiteral("MultiMC"),            QStringLiteral("MultiMC"),            QString() },
        { "pe_dark",    QObject::tr("Simple (Dark Icons)"),   QObject::tr("Simple (Dark Icons)"),   QString() },
        { "pe_light",   QObject::tr("Simple (Light Icons)"),  QObject::tr("Simple (Light Icons)"),  QString() },
        { "pe_blue",    QObject::tr("Simple (Blue Icons)"),   QObject::tr("Simple (Blue Icons)"),   QString() },
        { "pe_colored", QObject::tr("Simple (Colored Icons)"),QObject::tr("Simple (Colored Icons)"),QString() },
        { "OSX",        QStringLiteral("OSX"),                QStringLiteral("OSX"),                QString() },
        { "iOS",        QStringLiteral("iOS"),                QStringLiteral("iOS"),                QString() },
        { "flat",         QStringLiteral("Flat Light"),       QStringLiteral("Flat"),               QObject::tr("Light") },
        { "flat_white",   QStringLiteral("Flat Dark"),        QStringLiteral("Flat"),               QObject::tr("Dark") },
        { "breeze_dark",  QStringLiteral("Breeze Dark"),      QStringLiteral("Breeze"),             QObject::tr("Dark") },
        { "breeze_light", QStringLiteral("Breeze Light"),     QStringLiteral("Breeze"),             QObject::tr("Light") },
        { "custom",     QObject::tr("Custom"),                QObject::tr("Custom"),                QString() },
    };
}
