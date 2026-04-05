# Settings System

## Overview

MeshMC uses a hierarchical settings system built on Qt's `QObject` infrastructure. Settings support default values, overrides, signal-based change notification, and per-instance customization through a gate/override pattern.

## Architecture

### Key Classes

| Class | File | Purpose |
|---|---|---|
| `SettingsObject` | `settings/SettingsObject.{h,cpp}` | Abstract settings container |
| `INISettingsObject` | `settings/INISettingsObject.{h,cpp}` | File-backed settings |
| `INIFile` | `settings/INIFile.{h,cpp}` | QVariant map with file I/O |
| `Setting` | `settings/Setting.{h,cpp}` | Individual setting entry |
| `OverrideSetting` | `settings/OverrideSetting.{h,cpp}` | Per-instance override |
| `PassthroughSetting` | `settings/PassthroughSetting.{h,cpp}` | Delegate to another setting |

### Hierarchy

```
Global Settings (Application::settings())
    │
    ├── INISettingsObject backed by meshmc.cfg
    ├── Contains defaults for all application settings
    │
    ├──→ Instance Settings (BaseInstance::settings())
    │       │
    │       ├── INISettingsObject backed by instance.cfg
    │       ├── Uses OverrideSetting to selectively override globals
    │       └── Unoverridden settings transparently fall through to global
    │
    └──→ MinecraftInstance adds additional Minecraft-specific settings
```

## SettingsObject

The abstract base class for all settings containers:

```cpp
class SettingsObject : public QObject
{
    Q_OBJECT
public:
    virtual ~SettingsObject();

    // Registration
    std::shared_ptr<Setting> registerSetting(QStringList synonyms, QVariant defVal = QVariant());
    std::shared_ptr<Setting> registerOverride(std::shared_ptr<Setting> original, std::shared_ptr<Setting> gate);
    std::shared_ptr<Setting> registerPassthrough(std::shared_ptr<Setting> original, std::shared_ptr<Setting> gate);

    // Access
    std::shared_ptr<Setting> getSetting(const QString& id) const;
    QVariant get(const QString& id) const;
    bool set(const QString& id, QVariant value);
    void reset(const QString& id) const;
    bool contains(const QString& id);

    bool reload();

signals:
    void settingChanged(const Setting& setting, QVariant value);
    void settingReset(const Setting& setting);

protected:
    virtual void changeSetting(const Setting& setting, QVariant value) = 0;
    virtual void resetSetting(const Setting& setting) = 0;
    virtual QVariant retrieveValue(const Setting& setting) = 0;
    virtual bool contains(const QString& id) const = 0;

    QMap<QString, std::shared_ptr<Setting>> m_settings;
};
```

### Registration Methods

- **`registerSetting(synonyms, default)`** — registers a basic setting with optional synonyms for backward compatibility
- **`registerOverride(original, gate)`** — registers a setting that overrides `original` when `gate` is `true`; otherwise falls through to `original`
- **`registerPassthrough(original, gate)`** — registers a setting that always reads from the instance but writes only when `gate` is `true`

## Setting

Individual setting entries:

```cpp
class Setting : public QObject
{
    Q_OBJECT
public:
    Setting(QStringList synonyms, QVariant defVal);

    // Value access
    virtual QVariant get() const;
    virtual QVariant defValue() const;
    virtual void set(QVariant value);
    virtual void reset();

    // Identity
    QString id() const;
    QStringList synonyms() const;

signals:
    void SettingChanged(const Setting& setting, QVariant value);
    void SettingReset(const Setting& setting);
};
```

### Synonyms

Settings can have multiple names for backward compatibility:

```cpp
s->registerSetting({"MinecraftWinWidth", "MCWindowWidth"}, 854);
```

The first synonym is the canonical ID. Lookups work with any synonym. This allows renaming settings without breaking existing `meshmc.cfg` files.

## INISettingsObject

File-backed implementation using `INIFile`:

```cpp
class INISettingsObject : public SettingsObject
{
    Q_OBJECT
public:
    explicit INISettingsObject(const QString& path, QObject* parent = 0);
    explicit INISettingsObject(std::shared_ptr<INIFile> file, QObject* parent = 0);

    bool reload() override;

protected:
    void changeSetting(const Setting& setting, QVariant value) override;
    void resetSetting(const Setting& setting) override;
    QVariant retrieveValue(const Setting& setting) override;

    std::shared_ptr<INIFile> m_ini;
};
```

### INIFile

Simple key-value storage backed by a text file:

```cpp
class INIFile : public QMap<QString, QVariant>
{
public:
    explicit INIFile(const QString& filename);

    bool loadFile(const QString& fileName);
    bool saveFile(const QString& fileName);

    QVariant get(const QString& key, QVariant def) const;
    void set(const QString& key, QVariant val);
};
```

Format of `meshmc.cfg` / `instance.cfg`:
```ini
MinMemAlloc=512
MaxMemAlloc=4096
JavaPath=/usr/lib/jvm/java-21-openjdk/bin/java
Language=en_US
IconTheme=pe_colored
LaunchMaximized=false
```

## Override System

### OverrideSetting

Used by instance settings to selectively override global settings:

```cpp
class OverrideSetting : public Setting
{
    Q_OBJECT
public:
    OverrideSetting(std::shared_ptr<Setting> other, std::shared_ptr<Setting> gate);

    // Delegation logic:
    // get()   → if gate is true, return local value; else return other->get()
    // set()   → sets local value and sets gate to true
    // reset() → resets local value and sets gate to false

    bool isOverridden() const;  // Returns gate value

    virtual QVariant get() const override;
    virtual void set(QVariant value) override;
    virtual void reset() override;
    virtual QVariant defValue() const override;

private:
    std::shared_ptr<Setting> m_other;
    std::shared_ptr<Setting> m_gate;
};
```

The **gate** setting is a boolean that determines whether the instance-local value or the global value is used. When the gate is `false`, the setting falls through to the global setting.

### PassthroughSetting

Similar to `OverrideSetting` but always reads from instance storage:

```cpp
class PassthroughSetting : public Setting
{
    Q_OBJECT
public:
    PassthroughSetting(std::shared_ptr<Setting> other, std::shared_ptr<Setting> gate);

    // Always reads from instance storage
    // Writes only when gate is true
    // Falls through to other when gate is false for writes
};
```

## Global Settings Registration

In `Application.cpp`, global settings are registered:

```cpp
// Memory
m_settings->registerSetting("MinMemAlloc", 512);
m_settings->registerSetting("MaxMemAlloc", 4096);
m_settings->registerSetting("PermGen", 128);

// Java
m_settings->registerSetting("JavaPath", "");
m_settings->registerSetting("JvmArgs", "");
m_settings->registerSetting("IgnoreJavaCompatibility", false);
m_settings->registerSetting("IgnoreJavaWizard", false);

// Window
m_settings->registerSetting({"MinecraftWinWidth", "MCWindowWidth"}, 854);
m_settings->registerSetting({"MinecraftWinHeight", "MCWindowHeight"}, 480);
m_settings->registerSetting("LaunchMaximized", false);

// Network/proxy
m_settings->registerSetting("ProxyType", "None");
m_settings->registerSetting("ProxyAddr", "127.0.0.1");
m_settings->registerSetting("ProxyPort", 8080);
m_settings->registerSetting("ProxyUser", "");
m_settings->registerSetting("ProxyPass", "");

// Console
m_settings->registerSetting("ShowConsole", false);
m_settings->registerSetting("AutoCloseConsole", false);
m_settings->registerSetting("ShowConsoleOnError", true);
m_settings->registerSetting("LogPrePostOutput", true);

// Custom commands
m_settings->registerSetting("PreLaunchCommand", "");
m_settings->registerSetting("WrapperCommand", "");
m_settings->registerSetting("PostExitCommand", "");

// UI
m_settings->registerSetting("IconTheme", "pe_colored");
m_settings->registerSetting("ApplicationTheme", "system");
m_settings->registerSetting("Language", "");

// Updates
m_settings->registerSetting("AutoUpdate", true);
m_settings->registerSetting("UpdateChannel", "stable");

// Analytics
m_settings->registerSetting("Analytics", true);

// Miscellaneous
m_settings->registerSetting("InstSortMode", "Name");
m_settings->registerSetting("SelectedInstance", "");
m_settings->registerSetting("UpdateDialogGeometry", "");
m_settings->registerSetting("CatStyle", "kitteh");
```

## Instance Settings Override

`MinecraftInstance::settings()` creates override settings for per-instance customization:

```cpp
auto globalSettings = APPLICATION->settings();
auto s = m_settings;  // instance SettingsObject

// Memory overrides
s->registerOverride(globalSettings->getSetting("MinMemAlloc"), gate);
s->registerOverride(globalSettings->getSetting("MaxMemAlloc"), gate);

// Java overrides
auto javaGate = s->registerSetting("OverrideJavaLocation", false);
s->registerOverride(globalSettings->getSetting("JavaPath"), javaGate);

auto jvmArgsGate = s->registerSetting("OverrideJvmArgs", false);
s->registerOverride(globalSettings->getSetting("JvmArgs"), jvmArgsGate);

// Window overrides
auto windowGate = s->registerSetting("OverrideWindow", false);
s->registerOverride(globalSettings->getSetting("MinecraftWinWidth"), windowGate);
s->registerOverride(globalSettings->getSetting("MinecraftWinHeight"), windowGate);
s->registerOverride(globalSettings->getSetting("LaunchMaximized"), windowGate);

// Console overrides
auto consoleGate = s->registerSetting("OverrideConsole", false);
s->registerOverride(globalSettings->getSetting("ShowConsole"), consoleGate);
s->registerOverride(globalSettings->getSetting("AutoCloseConsole"), consoleGate);
s->registerOverride(globalSettings->getSetting("ShowConsoleOnError"), consoleGate);
```

### Gate Pattern

Each category of overridable settings has its own gate setting:
- `OverrideJavaLocation` — gates JavaPath
- `OverrideJvmArgs` — gates JvmArgs
- `OverrideMemory` — gates MinMemAlloc, MaxMemAlloc, PermGen
- `OverrideWindow` — gates MinecraftWinWidth, MinecraftWinHeight, LaunchMaximized
- `OverrideConsole` — gates ShowConsole, AutoCloseConsole, ShowConsoleOnError
- `OverrideCommands` — gates PreLaunchCommand, WrapperCommand, PostExitCommand
- `OverrideNativeWorkarounds` — gates UseNativeOpenAL, UseNativeGLFW

In the UI, each category has a checkbox. Enabling the checkbox:
1. Sets the gate to `true`
2. Enables the corresponding UI fields
3. Makes the setting read from instance storage instead of global

## Settings UI

### Global Settings Pages

Settings UI is organized into pages, each a `BasePage` subclass:

| Page | File | Settings |
|---|---|---|
| `MeshMCPage` | `pages/global/MeshMCPage.h` | Update channel, auto-update, analytics |
| `MinecraftPage` | `pages/global/MinecraftPage.h` | Window size, maximize, console behavior |
| `JavaPage` | `pages/global/JavaPage.h` | Java path, memory, JVM args |
| `LanguagePage` | `pages/global/LanguagePage.h` | UI language selection |
| `ProxyPage` | `pages/global/ProxyPage.h` | Network proxy settings |
| `ExternalToolsPage` | `pages/global/ExternalToolsPage.h` | Profiler, editor paths |
| `PasteEEPage` | `pages/global/PasteEEPage.h` | Paste service configuration |
| `CustomCommandsPage` | `pages/global/CustomCommandsPage.h` | Pre/post/wrapper commands |
| `AppearancePage` | `pages/global/AppearancePage.h` | Theme, icon theme, cat style |

### Instance Settings Pages

Instance setting pages mirror global pages but include the override checkboxes:

| Instance Page | Override Gate |
|---|---|
| Instance Java settings | `OverrideJavaLocation`, `OverrideJvmArgs`, `OverrideMemory` |
| Instance window settings | `OverrideWindow` |
| Instance console settings | `OverrideConsole` |
| Instance custom commands | `OverrideCommands` |

## Settings File Locations

| File | Location | Content |
|---|---|---|
| `meshmc.cfg` | Data directory root | Global settings |
| `instance.cfg` | Instance directory | Per-instance settings + overrides |
| `accounts.json` | Data directory root | Account data (see Account Management) |
| `metacache/` | Data directory root | HTTP cache metadata |

## Change Notification

Settings use Qt signals for reactive updates:

```cpp
// Connect to a specific setting change
connect(APPLICATION->settings().get(), &SettingsObject::settingChanged,
    this, [](const Setting& setting, QVariant value) {
        if (setting.id() == "Language") {
            // Reload translations
        }
    });

// Settings emit when changed
app->settings()->set("Language", "de_DE");
// → emits settingChanged(Setting("Language"), "de_DE")
```

## Data Path Resolution

The settings data path is determined at startup in `Application::Application()`:

1. **Portable mode**: If `meshmc_portable.txt` exists next to the binary, data lives alongside the binary
2. **Standard paths**: Otherwise uses `QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/MeshMC"`
3. **CLI override**: `--dir <path>` overrides the data directory

The data directory contains:
```
<data_dir>/
├── meshmc.cfg          # Global settings
├── accounts.json       # Account storage
├── instances/          # Instance directories
├── icons/              # Custom icons
├── themes/             # Custom themes
├── translations/       # Translation files
├── metacache/          # HTTP cache
├── logs/               # Application logs
└── java/               # Managed Java installations
```
