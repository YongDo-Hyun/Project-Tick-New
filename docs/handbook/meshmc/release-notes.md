# Release Notes

## Versioning Scheme

MeshMC follows a three-component version scheme:

```
MAJOR.MINOR.HOTFIX
```

Defined in the top-level `CMakeLists.txt`:

```cmake
set(MeshMC_VERSION_MAJOR    7)
set(MeshMC_VERSION_MINOR    0)
set(MeshMC_VERSION_HOTFIX   0)
set(MeshMC_VERSION_NAME "${MeshMC_VERSION_MAJOR}.${MeshMC_VERSION_MINOR}.${MeshMC_VERSION_HOTFIX}")
```

| Component | When Incremented |
|---|---|
| **MAJOR** | Breaking changes, major feature overhauls |
| **MINOR** | New features, significant improvements |
| **HOTFIX** | Bug fixes, security patches |

### Build Metadata

Additional version metadata is generated at build time:

```cmake
set(MeshMC_BUILD_PLATFORM "${CMAKE_SYSTEM_NAME}")   # Linux, Darwin, Windows
set(MeshMC_GIT_COMMIT ...)                            # Git commit hash
set(MeshMC_GIT_TAG ...)                               # Git tag if on a tag
```

The full version string displayed in the About dialog includes platform and git info.

## Update System

MeshMC uses a dual-source update checking system.

### UpdateChecker

```cpp
class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    void checkForUpdate(QString updateUrl, bool notifyNoUpdate);

    bool hasNewUpdate() const;
    QString getLatestVersion() const;
    QString getDownloadUrl() const;
    QString getChangelog() const;

signals:
    void updateAvailable(QString version, QString url, QString changelog);
    void noUpdateAvailable();
    void updateCheckFailed();

private:
    void parseRSSFeed(const QByteArray& data);
    void parseGitHubRelease(const QJsonObject& release);

    QString m_currentVersion;
    QString m_latestVersion;
    QString m_downloadUrl;
    QString m_changelog;
    bool m_hasUpdate = false;
};
```

### Update Sources

#### RSS Feed

Primary update channel — an RSS/Atom feed listing available versions:

```
https://projecttick.org/feed/meshmc.xml
```

The feed contains:
- Version number
- Download URLs per platform
- Changelog summary

#### GitHub Releases

Fallback/alternative update source using the GitHub Releases API:

```
https://api.github.com/repos/Project-Tick/MeshMC/releases/latest
```

Returns:
- Tag name (version)
- Release body (changelog in Markdown)
- Asset download URLs

### Update Check Flow

```
Application startup (if AutoUpdate enabled)
    │
    └── UpdateChecker::checkForUpdate()
            │
            ├── Fetch RSS feed
            │   ├── Parse version from <item> entries
            │   ├── Compare with current version
            │   └── Extract download URL + changelog
            │
            ├── OR Fetch GitHub release
            │   ├── Parse tag_name for version
            │   ├── Compare with current version
            │   └── Extract asset URL + body
            │
            ├── If new version available:
            │   └── emit updateAvailable(version, url, changelog)
            │       └── MainWindow shows update notification
            │
            └── If no update:
                └── emit noUpdateAvailable()
```

### Update Settings

```cpp
m_settings->registerSetting("AutoUpdate", true);
m_settings->registerSetting("UpdateChannel", "stable");
```

| Setting | Values | Description |
|---|---|---|
| `AutoUpdate` | `true`/`false` | Check for updates on startup |
| `UpdateChannel` | `stable`/`beta` | Which release channel to follow |

### Update Dialog

When an update is available, `UpdateDialog` is shown:

- Displays current version vs. available version
- Shows changelog (rendered from Markdown via cmark)
- "Update Now" button → opens download URL in browser
- "Skip This Version" → suppresses notification for this version
- "Remind Me Later" → dismisses until next startup

### Sparkle (macOS)

On macOS, updates are handled by the Sparkle framework instead of the built-in `UpdateChecker`:

- Native macOS update UI
- Differential updates (only download changed parts)
- Code signature verification
- Appcast feed (XML similar to RSS)
- Background update installation

```cpp
#ifdef Q_OS_MACOS
    // Use Sparkle for updates instead of built-in checker
    SUUpdater* updater = [SUUpdater sharedUpdater];
    [updater setAutomaticallyChecksForUpdates:autoUpdate];
    [updater checkForUpdatesInBackground];
#endif
```

## Distribution

### Build Artifacts

| Platform | Artifact | Format |
|---|---|---|
| Linux | AppImage | `.AppImage` |
| Linux | Tarball | `.tar.gz` |
| Linux | Nix | Flake output |
| macOS | Disk Image | `.dmg` (app bundle) |
| Windows | Installer | `.exe` (NSIS) |
| Windows | Portable | `.zip` |

### Release Branches

| Branch | Purpose |
|---|---|
| `develop` | Active development |
| `release-7.x` | Current stable release line |
| Tagged releases | `v7.0.0`, `v7.0.1`, etc. |

Backports from `develop` to `release-*` branches are automated via GitHub Actions (see [Contributing](contributing.md#backporting)).

## Changelog Format

Release changelogs follow this structure:

```markdown
## MeshMC 7.0.0

### New Features
- Feature description

### Bug Fixes
- Fix description

### Internal Changes
- Change description

### Dependencies
- Updated Qt to 6.7.x
```

## Current Version

**MeshMC 7.0.0** — Initial release as MeshMC (fork of PrismLauncher/MultiMC).

Key features:
- C++23 codebase with Qt6
- Microsoft Account authentication via OAuth2
- Multi-platform support (Linux, macOS, Windows)
- Instance management with component system
- Mod management with CurseForge, Modrinth, ATLauncher, FTB, Technic integration
- Customizable themes and icon packs
- Managed Java downloads
- Automatic update checking
