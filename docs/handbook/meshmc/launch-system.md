# Launch System

## Overview

MeshMC's launch system orchestrates the process of starting a Minecraft game instance. It involves account authentication, component resolution, file preparation, JVM argument assembly, and process management. The launch system is built around a step-based pipeline that allows individual phases to be executed, paused, and resumed.

## Launch Architecture

### Key Classes

| Class | File | Purpose |
|---|---|---|
| `LaunchController` | `launcher/LaunchController.{h,cpp}` | High-level orchestrator, UI interaction |
| `LaunchTask` | `launcher/launch/LaunchTask.{h,cpp}` | Step pipeline executor |
| `LaunchStep` | `launcher/launch/LaunchStep.{h,cpp}` | Abstract step interface |
| `DirectJavaLaunch` | `minecraft/launch/DirectJavaLaunch.{h,cpp}` | JVM process spawner |
| `MeshMCPartLaunch` | `minecraft/launch/MeshMCPartLaunch.{h,cpp}` | Java-wrapped launcher |
| `LoggedProcess` | `launcher/LoggedProcess.{h,cpp}` | QProcess wrapper with logging |
| `LogModel` | `launcher/launch/LogModel.{h,cpp}` | Game log data model |

## LaunchController

`LaunchController` extends `Task` and handles the user-facing launch flow:

```cpp
class LaunchController : public Task
{
    Q_OBJECT
public:
    void executeTask() override;
    void setInstance(InstancePtr instance);
    void setOnline(bool online);
    void setProfiler(BaseProfilerFactory* profiler);
    void setParentWidget(QWidget* widget);
    void setServerToJoin(MinecraftServerTargetPtr serverToJoin);
    void setAccountToUse(MinecraftAccountPtr accountToUse);
    bool abort() override;

private:
    void login();
    void launchInstance();
    void decideAccount();

private slots:
    void readyForLaunch();
    void onSucceeded();
    void onFailed(QString reason);
    void onProgressRequested(Task* task);

private:
    BaseProfilerFactory* m_profiler = nullptr;
    bool m_online = true;
    InstancePtr m_instance;
    QWidget* m_parentWidget = nullptr;
    InstanceWindow* m_console = nullptr;
    MinecraftAccountPtr m_accountToUse = nullptr;
    MinecraftServerTargetPtr m_serverToJoin;
};
```

### Launch Flow

```
LaunchController::executeTask()
    │
    ├── decideAccount()
    │   ├── If m_accountToUse is set → use it
    │   ├── Else → get default account from AccountList
    │   └── If no account → prompt user with ProfileSelectDialog
    │
    ├── login()
    │   ├── If online → authenticate via MSASilent (token refresh)
    │   │   ├── On success → launchInstance()
    │   │   └── On failure → fall back to MSAInteractive (browser login)
    │   └── If offline → create offline AuthSession → launchInstance()
    │
    └── launchInstance()
        ├── instance->createUpdateTask(mode) → update game files if needed
        ├── instance->createLaunchTask(session, server) → build step pipeline
        ├── Connect LaunchTask signals to controller slots
        └── LaunchTask::start()
```

### Account Decision (`decideAccount()`)

The account selection priority:
1. Explicitly provided `m_accountToUse` (from command line or UI)
2. Default account from `AccountList::defaultAccount()`
3. User prompt via `ProfileSelectDialog` if no default is set

### Authentication (`login()`)

For online mode:
1. Attempt `MSASilent` (token refresh using existing tokens)
2. If refresh fails, prompt for `MSAInteractive` (browser-based OAuth2 login)
3. On success, an `AuthSession` is created containing:
   - Access token
   - Player UUID
   - Player name
   - User type

For offline mode:
- An `AuthSession` is created with a dummy token
- The player name comes from the account profile

## LaunchTask

`LaunchTask` is the central pipeline executor:

```cpp
class LaunchTask : public Task
{
    Q_OBJECT
public:
    enum State { NotStarted, Running, Waiting, Failed, Aborted, Finished };

    static shared_qobject_ptr<LaunchTask> create(InstancePtr inst);

    void appendStep(shared_qobject_ptr<LaunchStep> step);
    void prependStep(shared_qobject_ptr<LaunchStep> step);
    void setCensorFilter(QMap<QString, QString> filter);

    InstancePtr instance();
    void setPid(qint64 pid);
    qint64 pid();

    void executeTask() override;
    void proceed();
    bool abort() override;
    bool canAbort() const override;

    shared_qobject_ptr<LogModel> getLogModel();

signals:
    void log(QString text, MessageLevel::Enum level);
    void readyForLaunch();
    void requestProgress(Task* task);
    void requestLogging();
};
```

### Step Execution Model

Steps are executed sequentially:

```
Step 0: executeTask() → completes → advance
Step 1: executeTask() → completes → advance
Step 2: executeTask() → emits readyForLaunch() → WAIT
    User clicks "Launch" in console → proceed()
Step 3: executeTask() → starts JVM process → RUNNING
    Process exits → completes → advance
Step N: finalize() called in reverse order
```

Each step can:
- **Complete immediately** — call `emitSucceeded()` to advance
- **Pause** — emit `readyForLaunch()` to wait for user interaction
- **Fail** — call `emitFailed()` to abort the pipeline
- **Run persistently** — stay active until an external event (process exit)

### Censor Filter

The `setCensorFilter()` method installs a string replacement map that redacts sensitive information from logs:

```cpp
void LaunchTask::setCensorFilter(QMap<QString, QString> filter);
```

`MinecraftInstance` populates this with:
- Access token → `<ACCESS TOKEN>`
- Client token → `<CLIENT TOKEN>`
- Player UUID → `<PROFILE ID>`

## LaunchStep

Abstract base class for individual launch steps:

```cpp
class LaunchStep : public Task
{
    Q_OBJECT
public:
    explicit LaunchStep(LaunchTask* parent)
        : Task(nullptr), m_parent(parent) { bind(parent); }

signals:
    void logLines(QStringList lines, MessageLevel::Enum level);
    void logLine(QString line, MessageLevel::Enum level);
    void readyForLaunch();
    void progressReportingRequest();

public slots:
    virtual void proceed() {}
    virtual void finalize() {}

protected:
    LaunchTask* m_parent;
};
```

## Minecraft Launch Steps

### Step Pipeline Construction

`MinecraftInstance::createLaunchTask()` builds the step pipeline for a Minecraft launch:

```cpp
shared_qobject_ptr<LaunchTask>
MinecraftInstance::createLaunchTask(AuthSessionPtr session,
                                    MinecraftServerTargetPtr serverToJoin)
{
    auto task = LaunchTask::create(std::dynamic_pointer_cast<MinecraftInstance>(
        shared_from_this()));

    // Step order:
    task->appendStep(make_shared<VerifyJavaInstall>(task.get()));
    task->appendStep(make_shared<CreateGameFolders>(task.get()));
    task->appendStep(make_shared<ScanModFolders>(task.get()));
    task->appendStep(make_shared<ExtractNatives>(task.get()));
    task->appendStep(make_shared<ModMinecraftJar>(task.get()));
    task->appendStep(make_shared<ReconstructAssets>(task.get()));
    task->appendStep(make_shared<ClaimAccount>(task.get()));
    task->appendStep(make_shared<PrintInstanceInfo>(task.get()));

    // Choose launch method
    auto method = launchMethod();
    if (method == "LauncherPart") {
        auto step = make_shared<MeshMCPartLaunch>(task.get());
        step->setAuthSession(session);
        step->setServerToJoin(serverToJoin);
        task->appendStep(step);
    } else {
        auto step = make_shared<DirectJavaLaunch>(task.get());
        step->setAuthSession(session);
        step->setServerToJoin(serverToJoin);
        task->appendStep(step);
    }

    // Set up censor filter
    task->setCensorFilter(createCensorFilterFromSession(session));

    return task;
}
```

### VerifyJavaInstall (`minecraft/launch/VerifyJavaInstall.h`)

Validates that the configured Java installation exists and is compatible:
- Checks that the Java binary exists at the configured path
- Verifies Java version meets minimum requirements
- Fails with descriptive error if Java is missing or incompatible

### CreateGameFolders (`minecraft/launch/CreateGameFolders.h`)

Ensures required directory structure exists:
- `.minecraft/` game directory
- `mods/`, `resourcepacks/`, `saves/` subdirectories
- `libraries/` for instance-local libraries
- `natives/` for platform-specific native libraries

### ScanModFolders (`minecraft/launch/ScanModFolders.h`)

Scans mod directories and updates the mod list:
- Enumerates `.minecraft/mods/` for loader mods
- Enumerates `.minecraft/coremods/` for core mods (Forge legacy)
- Updates the instance's mod models

### ExtractNatives (`minecraft/launch/ExtractNatives.h`)

Extracts platform-specific native libraries from JAR files:
- Iterates through native libraries in the `LaunchProfile`
- Extracts `.so` (Linux), `.dll` (Windows), or `.dylib` (macOS) files
- Places them in the `natives/` directory within the instance

### ModMinecraftJar (`minecraft/launch/ModMinecraftJar.h`)

Applies jar mods to the Minecraft game JAR:
- If jar mods are present in the component list, creates a modified JAR
- Overlays jar mod contents onto the vanilla JAR
- Stores the modified JAR for use by the launcher

### ReconstructAssets (`minecraft/launch/ReconstructAssets.h`)

Handles legacy asset management:
- For older Minecraft versions that use the "legacy" asset system
- Copies assets from the shared cache to the instance's `resources/` directory
- Modern versions use the asset index system and skip this step

### ClaimAccount (`minecraft/launch/ClaimAccount.h`)

Marks the account as in-use for this launch session:
- Prevents the same account from being used in concurrent launches
- Releases the claim when the game exits

### PrintInstanceInfo (`minecraft/launch/PrintInstanceInfo.h`)

Logs debug information to the console:
- Instance name and ID
- Minecraft version
- Java path and version
- JVM arguments
- Classpath
- Native library path
- Working directory

### DirectJavaLaunch (`minecraft/launch/DirectJavaLaunch.h`)

The primary launch step that spawns the JVM process:

```cpp
class DirectJavaLaunch : public LaunchStep
{
    Q_OBJECT
public:
    explicit DirectJavaLaunch(LaunchTask* parent);

    virtual void executeTask();
    virtual bool abort();
    virtual void proceed();
    virtual bool canAbort() const { return true; }

    void setWorkingDirectory(const QString& wd);
    void setAuthSession(AuthSessionPtr session);
    void setServerToJoin(MinecraftServerTargetPtr serverToJoin);

private slots:
    void on_state(LoggedProcess::State state);

private:
    LoggedProcess m_process;
    QString m_command;
    AuthSessionPtr m_session;
    MinecraftServerTargetPtr m_serverToJoin;
};
```

This step:
1. Assembles the full Java command line
2. Sets the working directory to the game root
3. Configures environment variables
4. Spawns the process via `LoggedProcess`
5. Connects to `on_state()` for process lifecycle events
6. Emits `readyForLaunch()` — the pipeline pauses until the user confirms
7. On `proceed()`, the process starts
8. Monitors the process until exit

### MeshMCPartLaunch (`minecraft/launch/MeshMCPartLaunch.h`)

Alternative launch method using MeshMC's Java-side launcher component:
- Writes launch parameters to a temporary file
- Starts the Java-side launcher (`libraries/launcher/`) which reads the parameters
- The Java launcher handles classpath assembly and game startup
- Provides additional launch customization capabilities

## JVM Argument Assembly

`MinecraftInstance` assembles JVM arguments through several methods:

### `javaArguments()`

Returns the complete list of JVM arguments:

```cpp
QStringList MinecraftInstance::javaArguments() const;
```

Components:
1. **Memory settings** — `-Xms<min>m -Xmx<max>m`
2. **Permission size** — `-XX:PermSize=<size>` (for older JVMs)
3. **Custom JVM args** — user-specified in instance/global settings
4. **System properties** — `-D` flags for various launchwrapper parameters

### `getClassPath()`

Builds the Java classpath:

```cpp
QStringList MinecraftInstance::getClassPath() const;
```

Sources:
1. Libraries from the resolved `LaunchProfile`
2. Maven files
3. Main game JAR (possibly modified by jar mods)
4. Instance-local libraries

### `getMainClass()`

Returns the Java main class:

```cpp
QString MinecraftInstance::getMainClass() const;
```

This comes from the resolved `LaunchProfile`, which may be:
- `net.minecraft.client.main.Main` — vanilla
- `net.minecraftforge.fml.launching.FMLClientLaunchProvider` — Forge
- `net.fabricmc.loader.impl.launch.knot.KnotClient` — Fabric
- `org.quiltmc.loader.impl.launch.knot.KnotClient` — Quilt

### `processMinecraftArgs()`

Template-expands game arguments:

```cpp
QStringList MinecraftInstance::processMinecraftArgs(
    AuthSessionPtr account,
    MinecraftServerTargetPtr serverToJoin) const;
```

Template variables replaced:
| Variable | Replacement |
|---|---|
| `${auth_player_name}` | Player name |
| `${auth_session}` | Session token |
| `${auth_uuid}` | Player UUID |
| `${auth_access_token}` | Access token |
| `${version_name}` | Minecraft version |
| `${game_directory}` | Game root path |
| `${assets_root}` | Assets directory path |
| `${assets_index_name}` | Asset index ID |
| `${user_type}` | Account type |
| `${version_type}` | Version type (release/snapshot) |

Additional server join arguments:
- `--server <address>` and `--port <port>` (if `serverToJoin` is set)

## LoggedProcess

`LoggedProcess` wraps `QProcess` with structured logging:

```cpp
class LoggedProcess : public QProcess
{
    Q_OBJECT
public:
    enum State {
        NotRunning,
        Starting,
        FailedToStart,
        Running,
        Finished,
        Crashed,
        Aborted
    };

    explicit LoggedProcess(QObject* parent = 0);

    State state() const;
    int exitCode() const;
    qint64 processId() const;
    void setDetachable(bool detachable);

signals:
    void log(QStringList lines, MessageLevel::Enum level);
    void stateChanged(LoggedProcess::State state);

public slots:
    void kill();

private slots:
    void on_stdErr();
    void on_stdOut();
    void on_exit(int exit_code, QProcess::ExitStatus status);
    void on_error(QProcess::ProcessError error);
    void on_stateChange(QProcess::ProcessState);
};
```

Features:
- Captures `stdout` and `stderr` separately
- Splits output into lines
- Emits structured log events with message levels
- Handles line buffering for partial reads (`m_err_leftover`, `m_out_leftover`)
- Tracks process state transitions
- Supports detachable processes

## LogModel

`LogModel` stores game log output for display:

```cpp
class LogModel : public QAbstractListModel
{
    Q_OBJECT
};
```

Used by `LogPage` and `InstanceWindow` to display real-time game output with color coding based on `MessageLevel::Enum`:

| Level | Color | Examples |
|---|---|---|
| `Unknown` | Default | Unclassified output |
| `StdOut` | Default | Normal game output |
| `StdErr` | Red | Error output |
| `Info` | Default | Informational messages |
| `Warning` | Yellow | Warning messages |
| `Error` | Red | Error messages |
| `Fatal` | Dark Red | Fatal/crash messages |
| `MeshMC` | Blue | Launcher messages |

## Server Join

`MinecraftServerTarget` carries server join information:

```cpp
struct MinecraftServerTarget {
    QString address;
    quint16 port;

    static MinecraftServerTargetPtr parse(const QString& fullAddress);
};
```

When `--server` is provided on the command line or configured in instance settings:
- The server address/port is parsed
- Passed to `LaunchController::setServerToJoin()`
- Forwarded to `DirectJavaLaunch::setServerToJoin()`
- Appended as `--server` and `--port` to Minecraft's arguments

## Profiler Integration

Profiler tools hook into the launch pipeline:

```cpp
class BaseProfilerFactory {
public:
    virtual BaseProfiler* createProfiler(InstancePtr instance, QObject* parent) = 0;
    virtual bool check(QString* error) = 0;
    virtual QString name() const = 0;
};
```

When a profiler is selected:
1. `LaunchController` creates the profiler via the factory
2. The profiler adds its own steps or arguments to the launch
3. JProfiler: opens profiling session via JProfiler's agent
4. JVisualVM: launches alongside the game process

## Process Environment

`MinecraftInstance::createEnvironment()` constructs the `QProcessEnvironment` for the game:

```cpp
QProcessEnvironment MinecraftInstance::createEnvironment() override;
```

This includes:
- System environment (inherited)
- `INST_NAME` — instance name
- `INST_ID` — instance ID
- `INST_DIR` — instance root directory
- `INST_MC_DIR` — game directory
- `INST_JAVA` — Java binary path
- `INST_JAVA_ARGS` — JVM arguments
- Native library path (`LD_LIBRARY_PATH` / `PATH` / `DYLD_LIBRARY_PATH`)

## Custom Commands

Instances support custom commands that execute at specific lifecycle points:

| Command | When | Purpose |
|---|---|---|
| `PreLaunchCommand` | Before JVM starts | Custom setup scripts |
| `PostExitCommand` | After JVM exits | Cleanup scripts |
| `WrapperCommand` | Wraps the JVM command | e.g., `gamemoderun` or `mangohud` |

These are configured per-instance (with override gate) or globally.
