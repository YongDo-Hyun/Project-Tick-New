# Theme System

## Overview

MeshMC supports application-wide theming through a `ThemeManager` that manages both visual themes (widget styling, colors) and icon themes. Themes can be built-in or user-provided via the `themes/` directory.

## Architecture

### Key Classes

| Class | File | Purpose |
|---|---|---|
| `ThemeManager` | `ui/themes/ThemeManager.{h,cpp}` | Theme registry and lifecycle |
| `ITheme` | `ui/themes/ITheme.{h,cpp}` | Abstract theme interface |
| `BrightTheme` | `ui/themes/BrightTheme.{h,cpp}` | Built-in light theme |
| `DarkTheme` | `ui/themes/DarkTheme.{h,cpp}` | Built-in dark theme |
| `FusionTheme` | `ui/themes/FusionTheme.{h,cpp}` | Qt Fusion-based theme |
| `SystemTheme` | `ui/themes/SystemTheme.{h,cpp}` | OS-native theme |
| `CustomTheme` | `ui/themes/CustomTheme.{h,cpp}` | User-defined theme |
| `CatPack` | `ui/themes/CatPack.{h,cpp}` | Cat background customization |

## ITheme Interface

All themes implement the `ITheme` interface:

```cpp
class ITheme
{
public:
    virtual ~ITheme() {}

    // Identity
    virtual QString id() = 0;
    virtual QString name() = 0;

    // Application
    virtual void apply(bool initial);

    // Qt integration
    virtual bool hasStyleSheet() = 0;
    virtual QString appStyleSheet() = 0;
    virtual QString qtTheme() = 0;

    // Colors
    virtual Qt::ColorScheme colorScheme() = 0;
    virtual QPalette colorScheme(QPalette basePalette);
    virtual double fadeAmount() = 0;
    virtual QColor fadeColor() = 0;

    // Badges
    virtual QString postprocessSVG(QString svg);

    // Tooltip colors
    virtual QColor tooltipBackground();
    virtual QColor tooltipForeground();
};
```

### apply()

The `apply()` method is called when activating a theme:
1. Sets `QApplication::setStyle()` based on `qtTheme()`
2. Sets the palette via `QApplication::setPalette()`
3. Applies stylesheet from `appStyleSheet()` if `hasStyleSheet()` is true
4. Emits color scheme change for Qt6 integration

### Color Scheme

`colorScheme()` returns `Qt::ColorScheme::Light` or `Qt::ColorScheme::Dark`, used by Qt6 to adjust native widget rendering.

### Fade

`fadeAmount()` and `fadeColor()` control the disabled-state appearance of instances in the grid view:
- `fadeColor()` — base color for fade overlay (typically background color)
- `fadeAmount()` — opacity of the fade (0.0 = no fade, 1.0 = fully faded)

## Built-in Themes

### SystemTheme

Uses the OS-provided widget style and colors:

```cpp
class SystemTheme : public ITheme {
public:
    QString id() override { return "system"; }
    QString name() override { return QObject::tr("System"); }
    bool hasStyleSheet() override { return false; }
    QString qtTheme() override { return QStyleFactory::keys().first(); }
    Qt::ColorScheme colorScheme() override { return Qt::ColorScheme::Unknown; }
};
```

### BrightTheme

A clean light theme with custom palette:

```cpp
class BrightTheme : public FusionTheme {
public:
    QString id() override { return "bright"; }
    QString name() override { return QObject::tr("Bright"); }
    Qt::ColorScheme colorScheme() override { return Qt::ColorScheme::Light; }
    bool hasStyleSheet() override { return true; }
    // Custom color palette with light backgrounds and dark text
};
```

### DarkTheme

A dark theme for low-light environments:

```cpp
class DarkTheme : public FusionTheme {
public:
    QString id() override { return "dark"; }
    QString name() override { return QObject::tr("Dark"); }
    Qt::ColorScheme colorScheme() override { return Qt::ColorScheme::Dark; }
    bool hasStyleSheet() override { return true; }
    // Custom color palette with dark backgrounds and light text
};
```

### FusionTheme

Base class for Bright and Dark themes, using Qt's Fusion style:

```cpp
class FusionTheme : public ITheme {
public:
    QString qtTheme() override { return "Fusion"; }
    // Shared Fusion-based styling logic
};
```

## CustomTheme

User-provided themes loaded from the `themes/` directory:

```cpp
class CustomTheme : public ITheme {
public:
    CustomTheme(ITheme* baseTheme, const QString& folder);

    QString id() override;
    QString name() override;
    bool hasStyleSheet() override;
    QString appStyleSheet() override;
    Qt::ColorScheme colorScheme() override;

private:
    ITheme* m_baseTheme;      // Fallback theme
    QString m_id;
    QString m_name;
    QString m_styleSheet;
    QString m_folder;
    // Custom palette overrides
};
```

### Custom Theme Format

A custom theme is a directory in `themes/` containing:

```
themes/my-theme/
├── theme.json         # Theme metadata and palette
└── themeStyle.css     # Qt stylesheet (optional)
```

#### theme.json

```json
{
    "name": "My Custom Theme",
    "baseTheme": "dark",
    "colors": {
        "Window": "#1a1b26",
        "WindowText": "#c0caf5",
        "Base": "#16161e",
        "AlternateBase": "#1a1b26",
        "ToolTipBase": "#1a1b26",
        "ToolTipText": "#c0caf5",
        "Text": "#c0caf5",
        "Button": "#24283b",
        "ButtonText": "#c0caf5",
        "BrightText": "#ff0000",
        "Link": "#7aa2f7",
        "Highlight": "#3d59a1",
        "HighlightedText": "#c0caf5",
        "fadeColor": "#1a1b26",
        "fadeAmount": 0.5
    }
}
```

- `baseTheme` — fallback theme ID (`bright`, `dark`, `system`)
- `colors` — QPalette color role overrides (any role not specified falls back to base theme)
- `fadeColor` / `fadeAmount` — instance fade styling

#### themeStyle.css

Optional Qt stylesheet for fine-grained control:

```css
QMainWindow {
    background-color: #1a1b26;
}

QToolBar {
    background-color: #24283b;
    border: none;
}

QPushButton {
    background-color: #3d59a1;
    color: #c0caf5;
    border: 1px solid #565f89;
    border-radius: 4px;
    padding: 4px 12px;
}

QPushButton:hover {
    background-color: #7aa2f7;
}
```

## ThemeManager

The central theme registry:

```cpp
class ThemeManager : public QObject
{
    Q_OBJECT
public:
    ThemeManager();

    QList<ITheme*> getValidApplicationThemes();
    ITheme* getTheme(const QString& themeId);
    void applyCurrentlySelectedTheme(bool initial = false);
    void setApplicationTheme(const QString& name, bool initial = false);

    // Icon themes
    void applyCurrentlySelectedIconTheme();
    void setIconTheme(const QString& name);
    QList<IconThemeEntry> getValidIconThemes();

    // Cat packs
    void setCatPack(const QString& name);
    QList<CatPack*> getValidCatPacks();
    CatPack* getCatPack(const QString& name);

private:
    void initializeThemes();
    void initializeIcons();
    void initializeCatPacks();

    QMap<QString, ITheme*> m_themes;
    QMap<QString, IconThemeEntry> m_iconThemes;
    QMap<QString, CatPack*> m_catPacks;
    ITheme* m_currentTheme = nullptr;
};
```

### Initialization

On startup, `ThemeManager` discovers:
1. **Built-in themes**: System, Bright, Dark (always available)
2. **Custom themes**: Scans `themes/` directory for `theme.json` files, creates `CustomTheme` instances
3. **Icon themes**: Scans for icon theme directories
4. **Cat packs**: Scans for cat background packs

### Theme Application

```cpp
void ThemeManager::setApplicationTheme(const QString& name, bool initial)
{
    auto theme = m_themes.value(name);
    if (!theme)
        theme = m_themes.value("system");  // Fallback

    m_currentTheme = theme;
    theme->apply(initial);
}
```

## Icon Theme System

### IconThemeEntry

```cpp
struct IconThemeEntry {
    QString id;
    QString name;
    QString path;
};
```

### Built-in Icon Themes

| ID | Name | Description |
|---|---|---|
| `pe_colored` | PE Colored | Colorful flat icons (default) |
| `pe_dark` | PE Dark | Dark variant |
| `pe_light` | PE Light | Light variant |
| `pe_blue` | PE Blue | Blue-tinted variant |
| `OSX` | OSX | macOS-style icons |
| `iOS` | iOS | iOS-style icons |
| `flat` | Flat | Minimal flat icons |
| `flat_white` | Flat White | White flat icons |
| `multimc` | MultiMC | Classic MultiMC icons |
| `custom` | Custom | User-provided icons |

### Icon Resolution

Icons are resolved through Qt's resource system and theme hierarchy:

```cpp
// Standard icon lookup
QIcon::fromTheme("instances/creeper")

// Falls back through:
// 1. Current icon theme directory
// 2. Default (pe_colored) theme
// 3. Built-in Qt resources
```

### Custom Instance Icons

Users can set custom icons per-instance:
- Icons stored in `icons/` directory in the data path
- PNG, SVG, ICO formats supported
- `IconPickerDialog` provides selection UI
- Custom icons override theme icons

## CatPack System

CatPack provides the cat/background image shown in the main window:

```cpp
class CatPack {
public:
    virtual ~CatPack() {}

    virtual QString id() = 0;
    virtual QString name() = 0;

    virtual QDate startDate();
    virtual QDate endDate();

    virtual QString path();
};
```

### Built-in Cat Packs

- **kitteh** — Standard cat background
- **rory** — Rory the cat

Cat packs can have date ranges for seasonal variants.

### Custom Cat Packs

User-provided cat packs in `catpacks/`:

```
catpacks/my-cats/
├── catpack.json
└── images/
    ├── default.png
    ├── christmas.png     # Dec 24 - Dec 26
    └── halloween.png     # Oct 31 - Nov 1
```

```json
{
    "name": "My Cats",
    "default": "images/default.png",
    "variants": [
        {
            "startDate": "12-24",
            "endDate": "12-26",
            "path": "images/christmas.png"
        }
    ]
}
```

## SVG Post-Processing

Themes can customize SVG icons at runtime:

```cpp
QString ITheme::postprocessSVG(QString svg)
{
    // Replace placeholder colors in SVG with theme colors
    svg.replace("%%BADGE_COLOR%%", badgeColor().name());
    svg.replace("%%BADGE_TEXT%%", badgeTextColor().name());
    return svg;
}
```

This allows icon badges (e.g., update counts, status indicators) to adapt to the current theme's color scheme.

## Theme Settings Integration

Theme selection is stored in global settings:

```cpp
// Settings registration (Application.cpp)
m_settings->registerSetting("ApplicationTheme", "system");
m_settings->registerSetting("IconTheme", "pe_colored");
m_settings->registerSetting("CatStyle", "kitteh");
```

The `AppearancePage` in global settings provides the UI for theme selection with live preview.
