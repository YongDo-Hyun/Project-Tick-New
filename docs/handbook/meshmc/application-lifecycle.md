# Application Lifecycle

## Overview

MeshMC's application lifecycle is managed by the `Application` class, which extends `QApplication`. The lifecycle progresses through distinct phases: initialization, main event loop, and shutdown. The `Application` singleton owns all major subsystems and coordinates their creation and destruction.

## Entry Point (`main.cpp`)

The application entry point is in `launcher/main.cpp`:

```cpp
int main(int argc, char* argv[])
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 8, 0))
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    // initialize Qt
    Application app(argc, argv);

    switch (app.status()) {
        case Application::StartingUp:
        case Application::Initialized: {
            Q_INIT_RESOURCE(multimc);
            Q_INIT_RESOURCE(backgrounds);
            Q_INIT_RESOURCE(documents);
            Q_INIT_RESOURCE(meshmc);

            Q_INIT_RESOURCE(pe_dark);
            Q_INIT_RESOURCE(pe_light);
            Q_INIT_RESOURCE(pe_blue);
            Q_INIT_RESOURCE(pe_colored);
            Q_INIT_RESOURCE(breeze_dark);
            Q_INIT_RESOURCE(breeze_light);
            Q_INIT_RESOURCE(OSX);
            Q_INIT_RESOURCE(iOS);
            Q_INIT_RESOURCE(flat);
            Q_INIT_RESOURCE(flat_white);
            return app.exec();
        }
        case Application::Failed:
            return 1;
        case Application::Succeeded:
            return 0;
    }
}
```

Key observations:
- High DPI scaling is enabled for Qt ≥ 6.8
- The `Application` constructor performs all initialization
- Qt resources (QRC files) for themes and branding are loaded only if initialization succeeds
- The return value depends on `Application::status()` — `Failed` returns 1, `Succeeded` returns 0, otherwise enters the event loop via `app.exec()`

## Application Status Enum

The `Application` class tracks its lifecycle state:

```cpp
class Application : public QApplication {
public:
    enum Status {
        StartingUp,   // Constructor is running, subsystems initializing
        Failed,       // Fatal error during initialization
        Succeeded,    // Initialization complete, immediate exit (e.g., --version)
        Initialized   // Ready for main event loop
    };
};
```

## Initialization Sequence

The `Application` constructor (`Application::Application(int&, char**)`) executes a carefully ordered initialization sequence. Each step is implemented as a private helper method:

### Phase 1: Platform Initialization

```cpp
void Application::initPlatform();
```

Platform-specific setup:
- **Windows** — attaches console for stdout/stderr if launched from terminal
- **macOS** — sets `QDir::homePath()` workarounds if needed
- **Linux** — no special platform init required

### Phase 2: Command-Line Parsing

```cpp
QHash<QString, QVariant> Application::parseCommandLine(int& argc, char** argv);
```

Uses the `Commandline` module to parse arguments:
- `--dir <path>` — override data directory
- `--launch <instance_id>` — launch an instance immediately
- `--server <address[:port]>` — join a server on launch
- `--profile <name>` — use a specific account profile
- `--alive` — check if another instance is running (live check)
- `--import <url_or_path>` — import a ZIP or URL as an instance

Parsed values are stored in member variables:
```cpp
QString m_instanceIdToLaunch;
QString m_serverToJoin;
QString m_profileToUse;
bool m_liveCheck = false;
QUrl m_zipToImport;
```

### Phase 3: Data Path Resolution

```cpp
bool Application::resolveDataPath(
    const QHash<QString, QVariant>& args,
    QString& dataPath,
    QString& adjustedBy,
    QString& origcwdPath);
```

Determines the data directory with the following priority:
1. Command-line `--dir` argument
2. Portable mode (if `portable.txt` exists next to executable)
3. Platform-specific standard location:
   - **macOS** — `~/Library/Application Support/MeshMC`
   - **Linux** — `$XDG_DATA_HOME/MeshMC` or `~/.local/share/MeshMC`
   - **Windows** — `%APPDATA%/MeshMC`

### Phase 4: Single-Instance Check

```cpp
bool Application::initPeerInstance();
```

Uses the `LocalPeer` library to ensure only one instance of MeshMC runs at a time:
- Creates a local socket with the application ID
- If another instance is already running, sends the command-line message to it and returns `false` (sets status to `Succeeded`)
- The running instance receives the message via `Application::messageReceived()` and processes it (e.g., showing the main window, launching an instance)

### Phase 5: Logging Setup

```cpp
bool Application::initLogging(const QString& dataPath);
```

Configures Qt's logging framework:
- Creates a log file in the data directory
- Installs a custom message handler that writes to both the log file and stderr
- Loads `qtlogging.ini` for logging category configuration

### Phase 6: Path Configuration

```cpp
void Application::setupPaths(
    const QString& binPath,
    const QString& origcwdPath,
    const QString& adjustedBy);
```

Sets up critical directory paths:
- `m_rootPath` — the application installation root
- JAR path for Java launcher components
- Ensures required directories exist (instances, icons, themes, translations, etc.)

### Phase 7: Settings Initialization

```cpp
void Application::initSettings();
```

Creates the global `SettingsObject` backed by `INISettingsObject` reading from `meshmc.cfg`. Registers all global settings with their default values:

- **General** — language, update channel, instance directory, icon theme
- **Java** — Java path, min/max memory, JVM arguments, permission mode
- **Minecraft** — window size, fullscreen, console behavior
- **Proxy** — proxy type, host, port, username, password
- **External tools** — paths to JProfiler, JVisualVM, MCEdit
- **Paste** — paste.ee API key configuration, paste type
- **Custom commands** — pre-launch, post-exit, wrapper commands
- **Analytics** — analytics opt-in/opt-out
- **Appearance** — application theme, icon theme, cat pack

### Phase 8: Subsystem Initialization

```cpp
void Application::initSubsystems();
```

Creates and initializes all major subsystems in dependency order:

1. **Network** — `QNetworkAccessManager` with proxy configuration
2. **HTTP Meta Cache** — `HttpMetaCache` for download caching
3. **Translations** — `TranslationsModel` with language loading
4. **Theme Manager** — `ThemeManager` with theme detection and application
5. **Icon List** — `IconList` scanning icon directories
6. **Metadata Index** — `Meta::Index` pointing to the metadata server
7. **Instance List** — `InstanceList` scanning the instances directory
8. **Account List** — `AccountList` loading from `accounts.json`
9. **Java List** — `JavaInstallList` for detected Java installations
10. **Profilers** — `JProfiler` and `JVisualVM` tool factories
11. **MCEdit** — `MCEditTool` integration
12. **Update Checker** — `UpdateChecker` with network manager
13. **News Checker** — RSS feed reader

### Phase 9: Analytics Initialization

```cpp
void Application::initAnalytics();
```

Initializes Google Analytics (`GAnalytics`) if the user has opted in. The analytics ID comes from `BuildConfig.ANALYTICS_ID`. Analytics can be toggled via the settings, with changes handled by `Application::analyticsSettingChanged()`.

### Phase 10: First-Run Wizard or Main Window

```cpp
bool Application::createSetupWizard();
void Application::performMainStartupAction();
```

If this is the first run or required settings are missing, a `SetupWizard` is displayed with pages:
- `LanguageWizardPage` — language selection
- `JavaWizardPage` — Java path configuration
- `AnalyticsWizardPage` — analytics opt-in

On wizard completion (`Application::setupWizardFinished()`), or if no wizard is needed, `performMainStartupAction()` runs:
- If `m_instanceIdToLaunch` is set → launch that instance directly
- If `m_zipToImport` is set → show import dialog
- Otherwise → show `MainWindow`

## Main Event Loop

After initialization, `main()` calls `app.exec()`, which enters the Qt event loop. The event loop processes:

- **UI events** — mouse clicks, keyboard input, paint events
- **Timer events** — periodic checks, delayed operations
- **Network events** — HTTP responses from `QNetworkAccessManager`
- **IPC events** — messages from other MeshMC instances via `LocalPeer`
- **File system events** — directory changes detected by `RecursiveFileSystemWatcher`

## Window Management

The `Application` class manages window lifecycle:

```cpp
MainWindow* Application::showMainWindow(bool minimized = false);
InstanceWindow* Application::showInstanceWindow(InstancePtr instance, QString page = QString());
```

### Main Window

- Created on first call to `showMainWindow()`
- Tracked via `m_mainWindow` pointer
- Connected to `Application::on_windowClose()` via the `isClosing` signal

### Instance Windows

- One per running instance, tracked in `m_instanceExtras`:
  ```cpp
  struct InstanceXtras {
      InstanceWindow* window = nullptr;
      shared_qobject_ptr<LaunchController> controller;
  };
  std::map<QString, InstanceXtras> m_instanceExtras;
  ```
- Created when launching or editing an instance
- Display game logs and instance-specific controls

### Window Close Tracking

The application tracks open windows and running instances:

```cpp
size_t m_openWindows = 0;
size_t m_runningInstances = 0;
```

The `on_windowClose()` slot decrements the window counter. When both counters reach zero and no updates are running, the application quits.

## Instance Launching

The `Application::launch()` slot orchestrates instance launches:

```cpp
bool Application::launch(
    InstancePtr instance,
    bool online = true,
    BaseProfilerFactory* profiler = nullptr,
    MinecraftServerTargetPtr serverToJoin = nullptr,
    MinecraftAccountPtr accountToUse = nullptr);
```

1. Creates a `LaunchController`
2. Configures it with the instance, online mode, profiler, server target, and account
3. Connects `controllerSucceeded()` and `controllerFailed()` slots
4. Opens an `InstanceWindow` for log display
5. Starts the controller task
6. Increments `m_runningInstances` via `addRunningInstance()`

## Instance Kill

```cpp
bool Application::kill(InstancePtr instance);
```

Terminates a running instance's launch controller, which aborts the `LaunchTask` and kills the JVM process.

## Application Shutdown

Shutdown is triggered when all windows close and no instances are running:

```cpp
bool Application::shouldExitNow() const;
```

This checks:
- `m_openWindows == 0`
- `m_runningInstances == 0`
- `m_updateRunning == false`

The destructor `Application::~Application()` cleans up:
- Saves pending settings
- Saves instance group data
- Saves account list
- Closes log file
- Destroys all owned subsystems (in reverse initialization order due to smart pointer destruction)

## IPC Message Handling

When a second MeshMC process starts, it sends a message to the running instance via `LocalPeer`:

```cpp
void Application::messageReceived(const QByteArray& message);
```

The message is parsed by `ApplicationMessage` and can trigger:
- Showing the main window (if no specific action)
- Launching an instance (`launch <instance_id>`)
- Importing a file (`import <path_or_url>`)

## Signal/Slot Connections

Key application-level signal/slot connections:

| Signal | Slot | Purpose |
|---|---|---|
| `Setting::SettingChanged` | `analyticsSettingChanged()` | Toggle analytics |
| `LaunchController::succeeded` | `controllerSucceeded()` | Handle launch success |
| `LaunchController::failed` | `controllerFailed()` | Handle launch failure |
| `LocalPeer::messageReceived` | `messageReceived()` | IPC from other instances |
| `SetupWizard::finished` | `setupWizardFinished()` | Wizard completion |
| `updateAllowedChanged` | (MainWindow) | Enable/disable update button |
| `globalSettingsAboutToOpen` | (listeners) | Notify settings dialog opening |
| `globalSettingsClosed` | (listeners) | Notify settings dialog closed |

## Update Flow During Lifecycle

Updates interact with the lifecycle:

```cpp
void Application::updateIsRunning(bool running);
bool Application::updatesAreAllowed();
```

- `updateIsRunning()` sets `m_updateRunning` and emits `updateAllowedChanged()`
- `updatesAreAllowed()` returns false if any instances are running (prevents updating while games are active)
- The `UpdateChecker` runs its check asynchronously; when an update is found, `MainWindow` handles the `updateAvailable()` signal

## Global Settings Dialog

```cpp
void Application::ShowGlobalSettings(QWidget* parent, QString open_page = QString());
```

Opens the settings `PageDialog` with the global page provider. Emits `globalSettingsAboutToOpen()` before and `globalSettingsClosed()` after.
