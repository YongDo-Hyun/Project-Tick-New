# Mod System

## Overview

MeshMC provides comprehensive mod management through a combination of local folder models and mod platform integrations. The mod system handles installation, discovery, metadata extraction, enabling/disabling, and browsing of mods from CurseForge, Modrinth, ATLauncher, FTB, and Technic.

## Local Mod Management

### ModFolderModel (`minecraft/mod/ModFolderModel.h`)

`ModFolderModel` is a `QAbstractListModel` that represents the contents of a mod directory (e.g., `<instance>/.minecraft/mods/`):

```cpp
class ModFolderModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Columns {
        ActiveColumn = 0,   // Enabled/disabled toggle
        NameColumn,          // Mod name
        VersionColumn,       // Mod version
        DateColumn,          // File modification date
        NUM_COLUMNS
    };

    enum ModStatusAction { Disable, Enable, Toggle };

    ModFolderModel(const QString& dir);

    // Model interface
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value,
                 int role) override;
    Qt::DropActions supportedDropActions() const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QStringList mimeTypes() const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action,
                      int row, int column, const QModelIndex& parent) override;
    int rowCount(const QModelIndex&) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;
    int columnCount(const QModelIndex& parent) const override;

    size_t size() const;
    bool empty() const;
    Mod& operator[](size_t index);
};
```

Key features:
- **Drag and drop** — users can drag mod files into the view to install them
- **Enable/disable** — toggling a mod renames the file (`.jar` ↔ `.jar.disabled`)
- **Automatic refresh** — directory changes are detected via `QFileSystemWatcher`
- **Column sorting** — by name, version, or date

### Mod Class (`minecraft/mod/Mod.h`)

The `Mod` class represents a single mod file:

```cpp
class Mod {
public:
    enum ModType {
        MOD_UNKNOWN,    // Unknown format
        MOD_ZIPFILE,    // ZIP/JAR with metadata
        MOD_SINGLEFILE, // Single file (no metadata)
        MOD_FOLDER,     // Directory mod
        MOD_LITEMOD     // LiteLoader mod (.litemod)
    };

    Mod(const QFileInfo& file);

    QString name() const;
    QString version() const;
    QString homeurl() const;
    QString description() const;
    QStringList authors() const;
    QDateTime dateTimeChanged() const;

    bool enable(bool value);  // Enable or disable
    bool enabled() const;
    ModType type() const;
};
```

### ModDetails (`minecraft/mod/ModDetails.h`)

Extracted metadata from mod files:

```cpp
struct ModDetails {
    QString mod_id;
    QString name;
    QString version;
    QString description;
    QStringList authors;
    QString homeurl;
    QStringList loaders;     // Compatible mod loaders
};
```

### Mod Metadata Extraction (`minecraft/mod/LocalModParseTask.h`)

`LocalModParseTask` runs in a background thread to parse metadata from mod JAR/ZIP files:

```cpp
class LocalModParseTask : public QObject {
    Q_OBJECT
public:
    LocalModParseTask(int token, Mod::ModType type,
                      const QFileInfo& modFile);
signals:
    void metadataReady(int token, ModDetails details);
};
```

Supported metadata formats:
- **Forge** — `mcmod.info` (JSON, legacy) and `mods.toml` (TOML, modern)
- **Fabric** — `fabric.mod.json` (JSON)
- **Quilt** — `quilt.mod.json` (JSON)
- **LiteLoader** — `litemod.json` (JSON)

The parser uses `tomlc99` for TOML parsing and Qt's JSON facilities for JSON.

### ModFolderLoadTask (`minecraft/mod/ModFolderLoadTask.h`)

Background task that scans a mod directory and creates `Mod` objects:

```cpp
class ModFolderLoadTask : public QObject {
    Q_OBJECT
public:
    ModFolderLoadTask(const QString& dir);
    void run();
signals:
    void succeeded();
};
```

This task:
1. Enumerates all files in the mod directory
2. Creates `Mod` objects for each `.jar`, `.zip`, `.litemod`, `.disabled`, and directory entry
3. Emits `succeeded()` when scanning is complete
4. The `ModFolderModel` then triggers `LocalModParseTask` for each mod to extract metadata

### Resource Pack and Texture Pack Models

Similar models exist for resource packs and texture packs:

```cpp
class ResourcePackFolderModel : public ModFolderModel {
    // Manages <instance>/.minecraft/resourcepacks/
};

class TexturePackFolderModel : public ModFolderModel {
    // Manages <instance>/.minecraft/texturepacks/
};
```

These inherit from `ModFolderModel` but specialize for their respective content types.

## Mod Platform Integrations

### Directory Structure

```
launcher/modplatform/
├── atlauncher/     # ATLauncher API client
├── flame/          # CurseForge (Flame) API client
├── legacy_ftb/     # Legacy FTB modpack support
├── modpacksch/     # FTB/modpacksch API (modern)
├── modrinth/       # Modrinth API client
└── technic/        # Technic Platform API client
```

### CurseForge Integration (`modplatform/flame/`)

CurseForge (internally called "Flame") integration provides:

- **Mod search** — query CurseForge's API for mods compatible with the instance's game version and loader
- **Modpack installation** — download and install complete CurseForge modpacks
- **Mod installation** — download individual mods and place them in the mods folder
- **Version resolution** — select the correct mod version for the instance's configuration

API authentication uses the `MeshMC_CURSEFORGE_API_KEY` set at build time:

```cmake
set(MeshMC_CURSEFORGE_API_KEY "$2a$10$..." CACHE STRING
    "API key for the CurseForge API")
```

### Modrinth Integration (`modplatform/modrinth/`)

Modrinth integration provides:

- **Mod search** — query Modrinth's API for mods
- **Modpack installation** — download and install Modrinth modpacks (`.mrpack` format)
- **Mod installation** — download and install individual mods
- **Version filtering** — filter by game version, loader, and project type

Modrinth uses a public API and does not require an API key for basic operations.

### ATLauncher Integration (`modplatform/atlauncher/`)

ATLauncher support enables importing ATLauncher modpack definitions:

- Parse ATLauncher pack JSON manifests
- Download required mods and configurations
- Create a new instance with the correct components

### FTB/modpacksch Integration (`modplatform/modpacksch/`)

Modern FTB modpack support via the modpacksch API:

- Browse available FTB modpacks
- Download and install modpacks
- Handle FTB-specific pack format

### Legacy FTB Integration (`modplatform/legacy_ftb/`)

Support for the older FTB modpack format:

- Parse legacy FTB pack definitions
- Import packs from FTB launcher directories

### Technic Integration (`modplatform/technic/`)

Technic Platform support:

- Browse Technic modpacks
- Download and install Technic packs
- Handle Technic's ZIP-based pack format

## Mod Installation Flow

### From Platform Browse Page

1. User opens a mod platform page (CurseForge/Modrinth) from the instance settings
2. Searches or browses for a mod
3. Selects a version compatible with their instance
4. MeshMC creates a `Download` via `NetJob` to fetch the mod file
5. The mod file is placed in `<instance>/.minecraft/mods/`
6. `ModFolderModel` detects the new file and updates the listing
7. `LocalModParseTask` extracts metadata for display

### From Drag and Drop

1. User drags a `.jar` file onto the mod list in `ModFolderPage`
2. Qt's drag-and-drop system fires `ModFolderModel::dropMimeData()`
3. The file is copied to the mods directory
4. The model updates automatically

### From File Dialog

1. User clicks "Add" in `ModFolderPage`
2. A `QFileDialog` opens for file selection
3. Selected files are copied to the mods directory
4. The model refreshes

## Modpack Import

### Overview

Modpack import is handled through `InstanceImportTask` and platform-specific import logic:

```
User selects modpack file/URL
    │
    ▼
InstanceImportTask::executeTask()
    │
    ├── Detect format (CurseForge manifest, Modrinth mrpack, etc.)
    │
    ├── CurseForge: parse manifest.json → download mods → set up instance
    ├── Modrinth: parse modrinth.index.json → download mods → set up instance
    ├── ATLauncher: parse ATL config → download mods → set up instance
    ├── Technic: extract ZIP → set up instance
    └── Generic: extract ZIP → copy files → set up instance
```

### CurseForge Modpack Format

CurseForge modpacks contain a `manifest.json`:
```json
{
    "minecraft": {
        "version": "1.20.4",
        "modLoaders": [
            { "id": "forge-49.0.19", "primary": true }
        ]
    },
    "files": [
        { "projectID": 123456, "fileID": 789012, "required": true }
    ]
}
```

MeshMC parses this manifest, creates components for the game version and mod loader, then downloads each mod file by its CurseForge project/file IDs.

### Modrinth Modpack Format (`.mrpack`)

Modrinth packs use the `.mrpack` format (ZIP with `modrinth.index.json`):
```json
{
    "formatVersion": 1,
    "game": "minecraft",
    "versionId": "1.0.0",
    "dependencies": {
        "minecraft": "1.20.4",
        "fabric-loader": "0.15.6"
    },
    "files": [
        {
            "path": "mods/sodium-0.5.5.jar",
            "hashes": { "sha1": "...", "sha512": "..." },
            "downloads": ["https://cdn.modrinth.com/..."]
        }
    ]
}
```

### Blocked Mods Handling

Some mods on CurseForge restrict third-party downloads. `BlockedModsDialog` handles this case:

```cpp
class BlockedModsDialog : public QDialog {
    // Shows a list of mods that couldn't be auto-downloaded
    // Provides manual download links for the user
};
```

## Mod Enable/Disable Mechanism

MeshMC enables and disables mods by renaming files:

```cpp
bool Mod::enable(bool value);
```

- **Disable**: Rename `modname.jar` → `modname.jar.disabled`
- **Enable**: Rename `modname.jar.disabled` → `modname.jar`

This approach ensures disabled mods are not loaded by the game's mod loader while remaining in the directory for easy re-enabling.

## Mod Folder Page (`ui/pages/instance/ModFolderPage.h`)

`ModFolderPage` provides the UI for managing mods within an instance:

```cpp
class ModFolderPage : public QMainWindow, public BasePage {
    Q_OBJECT
};
```

Features:
- List view with columns: Active (checkbox), Name, Version, Date
- Add button — file dialog for selecting mod files
- Remove button — delete selected mods
- Enable/Disable button — toggle selection
- View folder button — open the mods directory in the file manager
- Mod details panel showing name, version, authors, description, homepage URL

## Shader Packs and Resource Packs

MeshMC provides similar management for other content types:

| Content Type | Directory | Page Class | Model |
|---|---|---|---|
| Mods | `.minecraft/mods/` | `ModFolderPage` | `ModFolderModel` |
| Resource Packs | `.minecraft/resourcepacks/` | `ResourcePackPage` | `ResourcePackFolderModel` |
| Texture Packs | `.minecraft/texturepacks/` | `TexturePackPage` | `TexturePackFolderModel` |
| Shader Packs | `.minecraft/shaderpacks/` | `ShaderPackPage` | `ModFolderModel` |

`ResourcePackPage`, `TexturePackPage`, and `ShaderPackPage` are thin wrappers around `ModFolderPage`:

```cpp
// ShaderPackPage.h
class ShaderPackPage : public ModFolderPage {
    // Specializes base path and file filters for shader packs
};
```

## World Management

While not strictly part of the mod system, world management follows a similar pattern:

### WorldList (`minecraft/WorldList.h`)

```cpp
class WorldList : public QAbstractListModel {
    Q_OBJECT
public:
    WorldList(const QString& dir);
    // Provides list of worlds with name, last played, game mode
};
```

### World (`minecraft/World.h`)

```cpp
class World {
public:
    World(const QFileInfo& file);
    QString name() const;
    // Reads level.dat for world metadata
};
```

The `WorldListPage` provides UI for browsing, adding, copying, and deleting worlds.
