# Component System

## Overview

MeshMC's component system is the mechanism by which Minecraft versions, mod loaders, and library overlays are decomposed into modular, reorderable, and independently versionable units. Rather than storing a single monolithic version profile, each instance maintains a `PackProfile` — an ordered list of `Component` objects that are resolved and merged into a `LaunchProfile` at launch time.

## Core Classes

### Component (`minecraft/Component.h`)

A `Component` represents a single versioned layer in the instance's version profile:

```cpp
class Component : public QObject, public ProblemProvider
{
    Q_OBJECT
public:
    Component(PackProfile* parent, const QString& uid);
    Component(PackProfile* parent, std::shared_ptr<Meta::Version> version);
    Component(PackProfile* parent, const QString& uid,
              std::shared_ptr<VersionFile> file);

    void applyTo(LaunchProfile* profile);

    // State queries
    bool isEnabled();
    bool setEnabled(bool state);
    bool canBeDisabled();
    bool isMoveable();
    bool isCustomizable();
    bool isRevertible();
    bool isRemovable();
    bool isCustom();
    bool isVersionChangeable();

    // Identity
    QString getID();
    QString getName();
    QString getVersion();
    std::shared_ptr<Meta::Version> getMeta();
    QDateTime getReleaseDateTime();

    // Customization
    bool customize();
    bool revert();
    void setVersion(const QString& version);
    void setImportant(bool state);

    // Problem reporting
    const QList<PatchProblem> getProblems() const override;
    ProblemSeverity getProblemSeverity() const override;

    void updateCachedData();

signals:
    void dataChanged();
};
```

### Component Data Members

Each component stores both persistent and cached data:

```cpp
// Persistent properties (saved to mmc-pack.json)
QString m_uid;                    // Component identifier (e.g., "net.minecraft", "net.minecraftforge")
QString m_version;                // Selected version string
bool m_dependencyOnly = false;    // Auto-added to satisfy dependencies
bool m_important = false;         // Cannot be removed (e.g., base Minecraft)
bool m_disabled = false;          // Temporarily disabled

// Cached properties (from version file)
QString m_cachedName;             // Display name
QString m_cachedVersion;          // Resolved version (may differ from m_version)
Meta::RequireSet m_cachedRequires;  // Dependencies
Meta::RequireSet m_cachedConflicts; // Conflicts
bool m_cachedVolatile = false;    // Auto-removable when not needed

// Load state
std::shared_ptr<Meta::Version> m_metaVersion;   // Remote metadata
std::shared_ptr<VersionFile> m_file;             // Parsed version file
bool m_loaded = false;
```

### Component UIDs

Components are identified by UIDs that follow a reverse-domain convention:

| UID | Component |
|---|---|
| `net.minecraft` | Minecraft base game |
| `net.minecraftforge` | Minecraft Forge |
| `net.fabricmc.fabric-loader` | Fabric Loader |
| `org.quiltmc.quilt-loader` | Quilt Loader |
| `net.neoforged.neoforge` | NeoForge |
| `com.mumfrey.liteloader` | LiteLoader |
| `net.fabricmc.intermediary` | Fabric Intermediary mappings |
| `org.lwjgl` | LWJGL (auto-dependency) |
| `org.lwjgl3` | LWJGL 3 (auto-dependency) |

## PackProfile (`minecraft/PackProfile.h`)

`PackProfile` is a `QAbstractListModel` that manages the ordered list of components for a `MinecraftInstance`:

```cpp
class PackProfile : public QAbstractListModel
{
    Q_OBJECT
    friend ComponentUpdateTask;

public:
    enum Columns { NameColumn = 0, VersionColumn, NUM_COLUMNS };

    explicit PackProfile(MinecraftInstance* instance);

    // Model interface
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value,
                 int role) override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Component operations
    void buildingFromScratch();
    void installJarMods(QStringList selectedFiles);
    void installCustomJar(QString selectedFile);

    enum MoveDirection { MoveUp, MoveDown };
    void move(const int index, const MoveDirection direction);
    bool remove(const int index);
    bool remove(const QString id);
    bool customize(int index);
    bool revertToBase(int index);

    // Resolution
    void reload(Net::Mode netmode);
    void resolve(Net::Mode netmode);
    Task::Ptr getCurrentTask();

    // Profile access
    std::shared_ptr<LaunchProfile> getProfile() const;

    // Component queries
    QString getComponentVersion(const QString& uid) const;
    bool setComponentVersion(const QString& uid, const QString& version,
                             bool important = false);
    bool installEmpty(const QString& uid, const QString& name);
    Component* getComponent(const QString& id);
    Component* getComponent(int index);
    void appendComponent(ComponentPtr component);

    void saveNow();

signals:
    void minecraftChanged();
};
```

### PackProfile Serialization (`mmc-pack.json`)

The component list is persisted in `mmc-pack.json` within the instance directory:

```json
{
    "components": [
        {
            "cachedName": "Minecraft",
            "cachedVersion": "1.20.4",
            "cachedRequires": [],
            "important": true,
            "uid": "net.minecraft",
            "version": "1.20.4"
        },
        {
            "cachedName": "Fabric Loader",
            "cachedVersion": "0.15.6",
            "cachedRequires": [
                {
                    "suggests": "1.20.4",
                    "uid": "net.minecraft"
                },
                {
                    "uid": "net.fabricmc.intermediary"
                }
            ],
            "uid": "net.fabricmc.fabric-loader",
            "version": "0.15.6"
        },
        {
            "cachedName": "Intermediary Mappings",
            "cachedVersion": "1.20.4",
            "dependencyOnly": true,
            "uid": "net.fabricmc.intermediary",
            "version": "1.20.4"
        }
    ],
    "formatVersion": 1
}
```

### Component Ordering

Components are ordered in the list and applied in sequence. Order matters because later components can override values from earlier ones:

1. **Minecraft base** — always first, provides core libraries, assets, main class
2. **Intermediary/deobfuscation** — mappings layer if present
3. **Mod loader** — Forge/Fabric/Quilt/NeoForge, adds loader libraries and tweakers
4. **Additional libraries** — LWJGL overrides, etc.

Users can reorder components using `PackProfile::move()`:

```cpp
enum MoveDirection { MoveUp, MoveDown };
void PackProfile::move(const int index, const MoveDirection direction);
```

Not all components are movable — `Component::isMoveable()` returns false for important components.

## ComponentUpdateTask (`minecraft/ComponentUpdateTask.h`)

`ComponentUpdateTask` is the task responsible for resolving component dependencies and updating version metadata:

```cpp
class ComponentUpdateTask : public Task
{
    Q_OBJECT
public:
    enum class Mode {
        Launch,      // Full resolution for launching
        Resolution   // Lightweight resolution for UI display
    };

    explicit ComponentUpdateTask(Mode mode, Net::Mode netmode,
                                 PackProfile* list, QObject* parent = 0);

protected:
    void executeTask();

private:
    void loadComponents();
    void resolveDependencies(bool checkOnly);
    void remoteLoadSucceeded(size_t index);
    void remoteLoadFailed(size_t index, const QString& msg);
    void checkIfAllFinished();
};
```

### Resolution Process

The component update task follows this algorithm:

1. **Load local data** — for each component, load the local version file (`patches/<uid>.json`) if it exists
2. **Fetch metadata** — for components without local data, fetch from the metadata server (`Meta::Version`)
3. **Parse version files** — each `Meta::Version` resolves to a `VersionFile` containing libraries, arguments, and rules
4. **Resolve dependencies** — scan all components' `cachedRequires` sets:
   - If a required UID is not in the component list, add it as a dependency-only component
   - If a required UID specifies a version suggestion, set the dependency component's version
   - Check for conflicts in `cachedConflicts`
5. **Repeat** — re-resolve until no new dependencies are added (fixed-point iteration)
6. **Validate** — check for unresolved dependencies, conflicts, and problems

### Network Modes

Resolution can operate in different network modes:

```cpp
namespace Net {
    enum class Mode {
        Offline,   // Use only cached/local data
        Online     // Fetch from network
    };
}
```

In `Offline` mode, the task uses only cached metadata. In `Online` mode, it fetches fresh metadata from `meta.projecttick.org`.

## LaunchProfile (`minecraft/LaunchProfile.h`)

`LaunchProfile` is the resolved, merged result of applying all components in sequence. It contains the concrete values needed to launch Minecraft:

```cpp
class LaunchProfile : public ProblemProvider
{
public:
    // Apply methods (called by Component::applyTo)
    void applyMinecraftVersion(const QString& id);
    void applyMainClass(const QString& mainClass);
    void applyAppletClass(const QString& appletClass);
    void applyMinecraftArguments(const QString& minecraftArguments);
    void applyMinecraftVersionType(const QString& type);
    void applyMinecraftAssets(MojangAssetIndexInfo::Ptr assets);
    void applyTraits(const QSet<QString>& traits);
    void applyTweakers(const QStringList& tweakers);
    void applyJarMods(const QList<LibraryPtr>& jarMods);
    void applyMods(const QList<LibraryPtr>& mods);
    void applyLibrary(LibraryPtr library);
    void applyMavenFile(LibraryPtr library);
    void applyMainJar(LibraryPtr jar);
    void applyProblemSeverity(ProblemSeverity severity);
    void clear();

    // Getters
    QString getMinecraftVersion() const;
    QString getMainClass() const;
    QString getAppletClass() const;
    QString getMinecraftVersionType() const;
    MojangAssetIndexInfo::Ptr getMinecraftAssets() const;
    QString getMinecraftArguments() const;
    const QSet<QString>& getTraits() const;
    const QStringList& getTweakers() const;
    const QList<LibraryPtr>& getJarMods() const;
    const QList<LibraryPtr>& getLibraries() const;
    const QList<LibraryPtr>& getNativeLibraries() const;
    const QList<LibraryPtr>& getMavenFiles() const;
    const LibraryPtr getMainJar() const;
    void getLibraryFiles(const QString& architecture,
                         QStringList& jars, QStringList& nativeJars,
                         const QString& overridePath,
                         const QString& tempPath) const;
    bool hasTrait(const QString& trait) const;
};
```

### Profile Merging

Components are applied in order via `Component::applyTo(LaunchProfile*)`:

```
Component[0] (net.minecraft) → applyTo(profile)
   sets: mainClass, libraries, assets, arguments, mainJar
Component[1] (net.fabricmc.intermediary) → applyTo(profile)
   adds: intermediary libraries
Component[2] (net.fabricmc.fabric-loader) → applyTo(profile)
   overrides: mainClass (to Fabric's knot launcher)
   adds: Fabric libraries, tweakers
```

The `applyLibrary()` method handles library deduplication — if a library with the same name but different version exists, the later component's version wins.

### Profile Fields

```cpp
private:
    QString m_minecraftVersion;            // "1.20.4"
    QString m_minecraftVersionType;        // "release" or "snapshot"
    MojangAssetIndexInfo::Ptr m_minecraftAssets;  // Asset index info
    QString m_minecraftArguments;          // Template arguments string
    QStringList m_tweakers;               // Tweaker classes
    QString m_mainClass;                   // e.g., "net.fabricmc.loader.impl.launch.knot.KnotClient"
    QString m_appletClass;                 // Legacy applet class
    QList<LibraryPtr> m_libraries;        // Classpath libraries
    QList<LibraryPtr> m_nativeLibraries;  // Native libraries
    QList<LibraryPtr> m_mavenFiles;       // Maven artifacts
    QList<LibraryPtr> m_jarMods;          // Jar modifications
    LibraryPtr m_mainJar;                  // Minecraft main JAR
    QSet<QString> m_traits;               // Feature traits
    ProblemSeverity m_problemSeverity;     // Worst problem severity
    QList<PatchProblem> m_problems;        // Accumulated problems
```

## VersionFile (`minecraft/VersionFile.h`)

A `VersionFile` represents the parsed content of a version JSON file. It is the intermediate format between raw JSON and the `LaunchProfile`:

Key fields include:
- `mainClass` — Java main class
- `mainJar` — the primary game JAR
- `libraries` — list of `Library` objects for the classpath
- `mavenFiles` — additional Maven artifacts
- `jarMods` — jar modifications
- `minecraftArguments` — game argument template
- `tweakers` — tweaker classes for legacy Forge
- `requires` — dependency requirements (`Meta::RequireSet`)
- `conflicts` — conflict declarations
- `traits` — feature traits (set of strings)
- `rules` — platform/feature conditional rules

## Library (`minecraft/Library.h`)

A `Library` represents a Java library dependency identified by Maven coordinates:

```cpp
class Library {
public:
    GradleSpecifier m_name;          // e.g., "net.minecraft:launchwrapper:1.12"
    QString m_absoluteURL;           // Direct download URL (if any)
    QString m_repositoryURL;         // Maven repository base URL
    QList<Rule> m_rules;             // Platform conditional rules
    QStringList m_natives;           // Native classifier map
    // ...
};
```

Libraries are resolved to file paths using `GradleSpecifier`:

```cpp
class GradleSpecifier {
    QString m_group;      // "net.minecraft"
    QString m_artifact;   // "launchwrapper"
    QString m_version;    // "1.12"
    QString m_classifier; // "" or "natives-linux"
    QString m_extension;  // "jar"

    // Produces paths like: net/minecraft/launchwrapper/1.12/launchwrapper-1.12.jar
    QString toPath() const;
};
```

## Version Format Parsers

### MojangVersionFormat (`minecraft/MojangVersionFormat.h`)

Parses Mojang's official version JSON format:

```cpp
class MojangVersionFormat {
public:
    static VersionFilePtr versionFileFromJson(const QJsonDocument& doc,
                                               const QString& filename);
    static QJsonDocument versionFileToJson(const VersionFilePtr& patch);
    static LibraryPtr libraryFromJson(const QJsonObject& libObj,
                                       const QString& filename);
    static QJsonObject libraryToJson(Library* library);
};
```

### OneSixVersionFormat (`minecraft/OneSixVersionFormat.h`)

Parses MeshMC's extended version JSON format (superset of Mojang's):

```cpp
class OneSixVersionFormat {
public:
    static VersionFilePtr versionFileFromJson(const QJsonDocument& doc,
                                               const QString& filename,
                                               bool requireOrder);
    static QJsonDocument versionFileToJson(const VersionFilePtr& patch);
    static LibraryPtr libraryFromJson(ProblemContainer& problems,
                                       const QJsonObject& libObj,
                                       const QString& filename);
    static QJsonObject libraryToJson(Library* library);
};
```

The "OneSix" format adds fields for component metadata, requirements, conflicts, and MeshMC-specific extensions.

## Component Customization

### Customizing a Component

```cpp
bool Component::customize();
```

Customizing a component creates a local override file in `patches/<uid>.json` within the instance directory. This allows users to manually edit the version JSON:
- Modified libraries
- Changed main class
- Custom arguments
- Additional tweakers

### Reverting to Base

```cpp
bool Component::revert();
```

Reverting removes the local override file and restores the component to its remote metadata version.

### Version Changes

```cpp
void Component::setVersion(const QString& version);
```

Changing a component's version triggers a re-resolution:
1. Update the version string
2. Clear cached data
3. `PackProfile` schedules a save
4. Next `resolve()` call fetches the new version's metadata

## Metadata System Integration

Components rely on the metadata system (`meta/`) for version information:

```
Meta::Index (root, loaded from meta.projecttick.org)
  └── Meta::VersionList (per UID, e.g., "net.minecraft")
       └── Meta::Version (e.g., "1.20.4")
            └── VersionFile (parsed JSON data)
```

When a component needs metadata:
1. `Component` requests its `Meta::Version` from the metadata index
2. `Meta::Version` checks local cache
3. If cache is stale, fetches fresh data from `meta.projecttick.org/v1/<uid>/<version>.json`
4. Parses the response into a `VersionFile`
5. `Component` applies the `VersionFile` to the `LaunchProfile`

## Jar Mods

Jar mods are modifications applied directly to the Minecraft JAR file:

```cpp
void PackProfile::installJarMods(QStringList selectedFiles);
void PackProfile::installCustomJar(QString selectedFile);
```

- `installJarMods()` adds JAR files to the jar mods list; they are merged into the game JAR at launch by `ModMinecraftJar`
- `installCustomJar()` replaces the main Minecraft JAR entirely

Jar mod files are stored in `<instance>/jarmods/` and referenced in the component list.

## Problem Detection

Both `Component` and `LaunchProfile` implement the `ProblemProvider` interface:

```cpp
class ProblemProvider {
public:
    virtual const QList<PatchProblem> getProblems() const = 0;
    virtual ProblemSeverity getProblemSeverity() const = 0;
};
```

Problems are detected during component resolution:
- Missing metadata for a required component
- Unresolvable dependency
- Conflicting component versions
- Incompatible library combinations
- Missing or invalid local override files

Problem severity levels:
- **None** — no issues
- **Warning** — may cause problems but launch is allowed
- **Error** — launch should be prevented

The `VersionPage` UI displays problems as icons/tooltips on affected components.
