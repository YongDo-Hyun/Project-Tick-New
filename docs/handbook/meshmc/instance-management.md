# Instance Management

## Overview

Instance management is central to MeshMC's design. An "instance" is a self-contained Minecraft environment with its own game version, mods, settings, saves, resource packs, and configuration. MeshMC stores multiple instances in parallel, allowing users to maintain entirely separate Minecraft setups.

## Instance Storage Layout

### Instance Root Directory

All instances live under a single parent directory, configurable in settings (default: `instances/` within the MeshMC data directory). Each instance occupies its own subdirectory:

```
instances/
├── MyVanilla1.20/
│   ├── instance.cfg            # Instance-level settings (INI format)
│   ├── mmc-pack.json           # Component list (PackProfile)
│   ├── patches/                # Custom component overrides (JSON)
│   ├── .minecraft/             # Game directory
│   │   ├── mods/               # Loader mods
│   │   ├── resourcepacks/      # Resource packs
│   │   ├── shaderpacks/        # Shader packs
│   │   ├── saves/              # World saves
│   │   ├── config/             # Mod configuration
│   │   ├── options.txt         # Game options
│   │   ├── screenshots/        # Screenshots
│   │   └── logs/               # Game logs
│   └── libraries/              # Instance-local libraries
├── ForgeModded/
│   ├── instance.cfg
│   ├── mmc-pack.json
│   ├── patches/
│   └── .minecraft/
└── ...
```

### Instance Configuration File (`instance.cfg`)

Each instance has an `instance.cfg` file (INI format) managed by `INISettingsObject`. This stores per-instance metadata and setting overrides:

```ini
InstanceType=OneSix
name=My Modded Instance
iconKey=flame
notes=Testing Forge 1.20.4 with performance mods
lastLaunchTime=1712345678000
totalTimePlayed=3600
lastTimePlayed=1800
JoinServerOnLaunch=false
OverrideJavaPath=true
JavaPath=/usr/lib/jvm/java-21/bin/java
OverrideMemory=true
MinMemAlloc=2048
MaxMemAlloc=8192
```

## BaseInstance Class

`BaseInstance` is the abstract base class for all instance types, defined in `launcher/BaseInstance.h`:

```cpp
class BaseInstance : public QObject,
                     public std::enable_shared_from_this<BaseInstance>
{
    Q_OBJECT
protected:
    BaseInstance(SettingsObjectPtr globalSettings,
                 SettingsObjectPtr settings,
                 const QString& rootDir);
public:
    enum class Status { Present, Gone };

    virtual void saveNow() = 0;
    void invalidate();

    virtual QString id() const;
    void setRunning(bool running);
    bool isRunning() const;
    int64_t totalTimePlayed() const;
    int64_t lastTimePlayed() const;
    void resetTimePlayed();

    QString instanceType() const;
    QString instanceRoot() const;
    virtual QString gameRoot() const { return instanceRoot(); }
    virtual QString modsRoot() const = 0;

    QString name() const;
    void setName(QString val);
    QString windowTitle() const;
    QString iconKey() const;
    void setIconKey(QString val);
    QString notes() const;
    void setNotes(QString val);

    QString getPreLaunchCommand();
    QString getPostExitCommand();
    QString getWrapperCommand();

    virtual QSet<QString> traits() const = 0;
    qint64 lastLaunch() const;
    void setLastLaunch(qint64 val);

    virtual SettingsObjectPtr settings() const;
    virtual Task::Ptr createUpdateTask(Net::Mode mode) = 0;
    virtual shared_qobject_ptr<LaunchTask>
        createLaunchTask(AuthSessionPtr account,
                         MinecraftServerTargetPtr serverToJoin) = 0;
    shared_qobject_ptr<LaunchTask> getLaunchTask();
    virtual QProcessEnvironment createEnvironment() = 0;
    virtual IPathMatcher::Ptr getLogFileMatcher() = 0;
    virtual QString getLogFileRoot() = 0;
};

typedef std::shared_ptr<BaseInstance> InstancePtr;
```

Key characteristics:
- Uses `std::enable_shared_from_this` for safe self-reference in callbacks
- Instance ID is determined internally by MeshMC (typically the directory name)
- Tracks play time (total and last session) in milliseconds
- Supports custom pre-launch, post-exit, and wrapper commands
- `traits()` returns feature flags from the version profile (e.g., `"XR:Initial"`, `"FirstThreadOnMacOS"`)

## MinecraftInstance

`MinecraftInstance` is the concrete implementation of `BaseInstance` for modern Minecraft versions, defined in `launcher/minecraft/MinecraftInstance.h`:

```cpp
class MinecraftInstance : public BaseInstance
{
    Q_OBJECT
public:
    MinecraftInstance(SettingsObjectPtr globalSettings,
                      SettingsObjectPtr settings,
                      const QString& rootDir);

    // Directory accessors
    QString jarModsDir() const;
    QString resourcePacksDir() const;
    QString texturePacksDir() const;
    QString shaderPacksDir() const;
    QString modsRoot() const override;
    QString coreModsDir() const;
    QString modsCacheLocation() const;
    QString libDir() const;
    QString worldDir() const;
    QString resourcesDir() const;
    QDir jarmodsPath() const;
    QDir librariesPath() const;
    QDir versionsPath() const;
    QString instanceConfigFolder() const override;
    QString gameRoot() const override;
    QString binRoot() const;
    QString getNativePath() const;
    QString getLocalLibraryPath() const;

    // Component system
    std::shared_ptr<PackProfile> getPackProfile() const;

    // Mod folder models
    std::shared_ptr<ModFolderModel> loaderModList() const;
    std::shared_ptr<ModFolderModel> coreModList() const;
    std::shared_ptr<ModFolderModel> resourcePackList() const;
    std::shared_ptr<ModFolderModel> texturePackList() const;
    std::shared_ptr<ModFolderModel> shaderPackList() const;
    std::shared_ptr<WorldList> worldList() const;
    std::shared_ptr<GameOptions> gameOptionsModel() const;

    // Launch
    Task::Ptr createUpdateTask(Net::Mode mode) override;
    shared_qobject_ptr<LaunchTask>
        createLaunchTask(AuthSessionPtr account,
                         MinecraftServerTargetPtr serverToJoin) override;
    QStringList javaArguments() const;
    QStringList getClassPath() const;
    QStringList getNativeJars() const;
    QString getMainClass() const;
    QStringList processMinecraftArgs(AuthSessionPtr account,
                                     MinecraftServerTargetPtr serverToJoin) const;
    JavaVersion getJavaVersion() const;
};
```

`MinecraftInstance` provides:
- All directory path resolution for game assets
- Lazy-initialized folder models (`ModFolderModel`, `WorldList`, etc.)
- Launch task construction with the full step pipeline
- Java argument assembly including classpath, library paths, and game arguments
- Version-specific behaviors via traits

## InstanceList

`InstanceList` manages the collection of all instances, defined in `launcher/InstanceList.h`:

```cpp
class InstanceList : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit InstanceList(SettingsObjectPtr settings,
                          const QString& instDir, QObject* parent = 0);

    // Model interface
    QModelIndex index(int row, int column = 0,
                      const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value,
                 int role) override;

    enum AdditionalRoles {
        GroupRole = Qt::UserRole,
        InstancePointerRole = 0x34B1CB48,
        InstanceIDRole = 0x34B1CB49
    };

    InstancePtr at(int i) const { return m_instances.at(i); }
};
```

### Instance Discovery

On startup, `InstanceList` scans the instances directory:
1. Enumerates subdirectories in the instances folder
2. For each directory, looks for `instance.cfg`
3. Reads `InstanceType` from the config to determine the instance class
4. Creates the appropriate instance object (`MinecraftInstance`)
5. Adds it to the internal list and emits model change signals

### Custom Model Roles

| Role | Value | Returns |
|---|---|---|
| `GroupRole` | `Qt::UserRole` | Group name (QString) |
| `InstancePointerRole` | `0x34B1CB48` | `InstancePtr` (shared pointer) |
| `InstanceIDRole` | `0x34B1CB49` | Instance ID (QString) |

## Instance Groups

Instances can be organized into named groups. Group assignments are stored in `instgroups.json` in the data directory:

```json
{
    "formatVersion": 1,
    "groups": {
        "Modded": {
            "hidden": false,
            "instances": [
                "ForgeModded",
                "FabricServer"
            ]
        },
        "Vanilla": {
            "hidden": false,
            "instances": [
                "MyVanilla1.20"
            ]
        }
    }
}
```

Group state tracking uses a tri-state enum:

```cpp
enum class GroupsState { NotLoaded, Steady, Dirty };
```

Groups are loaded lazily and saved when dirty. The `GroupRole` in the model provides group information for UI display.

## Instance Creation

### New Instance Creation

New instances are created via `NewInstanceDialog`, which collects:
- Instance name and icon
- Minecraft version selection
- Optional mod loader (Forge, Fabric, Quilt, NeoForge)
- Optional modpack import

The actual creation is handled by `InstanceCreationTask`:
1. Creates the instance directory
2. Writes initial `instance.cfg`
3. Creates a `PackProfile` with the selected components
4. Saves the component list to `mmc-pack.json`
5. Runs `ComponentUpdateTask` to resolve dependencies and download metadata

### Instance Import

`InstanceImportTask` handles importing instances from external sources:

```cpp
class InstanceImportTask : public InstanceTask
{
    Q_OBJECT
public:
    explicit InstanceImportTask(const QUrl& url);
protected:
    virtual void executeTask() override;
};
```

Supported import formats:
- **ZIP archives** — exported MeshMC/MultiMC instance packages
- **URLs** — downloads and extracts (supports CurseForge, Modrinth, and Technic pack URLs)
- **Modpack manifests** — platform-specific manifests trigger specialized import logic

### Instance Copying

`InstanceCopyTask` clones an existing instance:

```cpp
class InstanceCopyTask : public InstanceTask
{
    Q_OBJECT
public:
    explicit InstanceCopyTask(InstancePtr origInstance,
                              bool copySaves, bool keepPlaytime);
protected:
    virtual void executeTask() override;
private:
    InstancePtr m_origInstance;
    QFuture<bool> m_copyFuture;
    QFutureWatcher<bool> m_copyFutureWatcher;
    std::unique_ptr<IPathMatcher> m_matcher;
    bool m_keepPlaytime;
};
```

Key options:
- `copySaves` — whether to include world save data
- `keepPlaytime` — whether to copy playtime statistics
- Uses `QtConcurrent` for background file copying with progress tracking
- An `IPathMatcher` can exclude specific files/directories from the copy

The copy dialog is `CopyInstanceDialog`:

```cpp
class CopyInstanceDialog : public QDialog
// UI file: CopyInstanceDialog.ui
```

## Instance Lifecycle States

An instance progresses through several states during its lifetime:

```
Created → Present → [Running] → Present → [Gone]
```

The `BaseInstance::Status` enum tracks the primary state:

```cpp
enum class Status {
    Present,  // Instance exists and is tracked
    Gone      // Instance was removed or invalidated
};
```

Running state is tracked separately:

```cpp
void BaseInstance::setRunning(bool running);
bool BaseInstance::isRunning() const;
```

When running:
- Play time counters are updated
- The instance icon shows a running indicator in the UI
- Certain operations are disabled (delete, move, etc.)

## Instance Invalidation

```cpp
void BaseInstance::invalidate();
```

An instance is invalidated when:
- Its directory is externally deleted or moved
- A `RecursiveFileSystemWatcher` detects the directory change
- The `InstanceList` removes it from its model

## Instance Settings Override System

Each instance has its own `SettingsObject` that can override global settings:

```cpp
virtual SettingsObjectPtr settings() const;
```

The override mechanism uses `OverrideSetting`:
- A **gate setting** (boolean) controls whether the override is active
- When the gate is ON, the instance's local value is used
- When the gate is OFF, the global value is passed through

Common overridable settings:

| Setting | Gate Setting | Purpose |
|---|---|---|
| `JavaPath` | `OverrideJavaPath` | Java binary path |
| `MinMemAlloc` | `OverrideMemory` | Minimum memory (MB) |
| `MaxMemAlloc` | `OverrideMemory` | Maximum memory (MB) |
| `JvmArgs` | `OverrideJavaArgs` | Additional JVM arguments |
| `MCLaunchMethod` | `OverrideMCLaunchMethod` | Launch method |
| `PreLaunchCommand` | `OverrideCommands` | Pre-launch command |
| `PostExitCommand` | `OverrideCommands` | Post-exit command |
| `WrapperCommand` | `OverrideCommands` | Wrapper command |
| `WindowWidth` | `OverrideWindow` | Window width |
| `WindowHeight` | `OverrideWindow` | Window height |
| `MaximizeWindow` | `OverrideWindow` | Start maximized |

The `InstanceSettingsPage` UI provides checkboxes for each gate setting, enabling or disabling the corresponding override section.

## Instance UI Integration

### Instance View

The main window displays instances in a custom view (`InstanceView` in `ui/instanceview/`):
- Grid or list layout
- Group headers with collapse/expand
- Drag and drop between groups
- Context menu for instance operations
- Icon display with status overlay (running indicator)

### Instance Pages

When editing an instance, a `PageDialog` opens with these pages:

| Page | File | Purpose |
|---|---|---|
| `VersionPage` | `ui/pages/instance/VersionPage.{h,cpp}` | Component management |
| `ModFolderPage` | `ui/pages/instance/ModFolderPage.{h,cpp}` | Mod list |
| `ResourcePackPage` | `ui/pages/instance/ResourcePackPage.h` | Resource packs |
| `TexturePackPage` | `ui/pages/instance/TexturePackPage.h` | Texture packs |
| `ShaderPackPage` | `ui/pages/instance/ShaderPackPage.h` | Shader packs |
| `NotesPage` | `ui/pages/instance/NotesPage.{h,cpp}` | Instance notes |
| `LogPage` | `ui/pages/instance/LogPage.{h,cpp}` | Game log viewer |
| `ScreenshotsPage` | `ui/pages/instance/ScreenshotsPage.{h,cpp}` | Screenshots |
| `WorldListPage` | `ui/pages/instance/WorldListPage.{h,cpp}` | World management |
| `GameOptionsPage` | `ui/pages/instance/GameOptionsPage.{h,cpp}` | Game options editor |
| `ServersPage` | `ui/pages/instance/ServersPage.{h,cpp}` | Server list editor |
| `InstanceSettingsPage` | `ui/pages/instance/InstanceSettingsPage.{h,cpp}` | Settings overrides |
| `OtherLogsPage` | `ui/pages/instance/OtherLogsPage.{h,cpp}` | Additional log files |

### Instance Window

`InstanceWindow` provides a dedicated window for a running instance:

```cpp
class InstanceWindow : public QMainWindow
{
    Q_OBJECT
};
```

It displays:
- Real-time game log output via `LogModel`
- Launch/kill controls
- Instance page navigation

## Instance Export

`ExportInstanceDialog` allows exporting an instance to a ZIP archive:
- Select which files/directories to include
- Exclude sensitive data, caches, and temporary files
- The exported archive can be imported by MeshMC on another machine

## Play Time Tracking

Each instance tracks cumulative play time:

```cpp
int64_t BaseInstance::totalTimePlayed() const;
int64_t BaseInstance::lastTimePlayed() const;
void BaseInstance::resetTimePlayed();
```

- `totalTimePlayed` — cumulative milliseconds across all sessions
- `lastTimePlayed` — duration of the most recent session
- Time is recorded when `setRunning(false)` is called after a session
- Stored in `instance.cfg` as `totalTimePlayed` and `lastTimePlayed`
