# Java Detection

## Overview

MeshMC requires a compatible Java installation to launch Minecraft. The Java detection system automatically discovers Java installations across all supported platforms, validates their architecture and version, and manages bundled Java downloads.

## Architecture

### Key Classes

| Class | File | Purpose |
|---|---|---|
| `JavaUtils` | `java/JavaUtils.{h,cpp}` | Platform-specific Java discovery |
| `JavaChecker` | `java/JavaChecker.{h,cpp}` | Java installation validator |
| `JavaCheckerJob` | `java/JavaCheckerJob.{h,cpp}` | Batch validation task |
| `JavaInstall` | `java/JavaInstall.{h,cpp}` | Java installation descriptor |
| `JavaInstallList` | `java/JavaInstallList.{h,cpp}` | Discovered Java list model |
| `JavaVersion` | `java/JavaVersion.{h,cpp}` | Version string parser |

## JavaUtils

Platform-specific Java discovery:

```cpp
class JavaUtils : public QObject
{
    Q_OBJECT
public:
    JavaUtils();

    // Discovery
    QList<QString> FindJavaPaths();
    static QString GetDefaultJava();
    QList<JavaInstallPtr> FindJavaFromRegistryKey(
        DWORD keyType, QString keyName, QString keyJavaDir, QString subkeySuffix = ""
    );  // Windows only

private:
    // Platform-specific search paths
    QStringList platformSearchPaths();
};
```

### Platform Search Paths

#### Linux

```cpp
QStringList JavaUtils::platformSearchPaths()
{
    return {
        "/usr/lib/jvm",                          // Distro packages
        "/usr/lib64/jvm",                        // 64-bit distros
        "/usr/lib32/jvm",                        // 32-bit compat
        "/opt/java",                             // Manual installs
        "/opt/jdk",                              // Manual JDK installs
        QDir::homePath() + "/.sdkman/candidates/java",  // SDKMAN
        QDir::homePath() + "/.jdks",             // IntelliJ downloads
        "/snap/openjdk",                         // Snap packages
        "/usr/lib/jvm/java-*/jre",               // JRE subdirs
    };
}
```

Scans each directory recursively for `bin/java` executables.

#### macOS

```cpp
QStringList JavaUtils::platformSearchPaths()
{
    return {
        "/Library/Java/JavaVirtualMachines",     // System JDKs
        "/System/Library/Java/JavaVirtualMachines", // Apple JDKs
        "/usr/local/opt/openjdk",                // Homebrew
        "/opt/homebrew/opt/openjdk",             // Homebrew (Apple Silicon)
        QDir::homePath() + "/Library/Java/JavaVirtualMachines",  // User JDKs
        QDir::homePath() + "/.sdkman/candidates/java",
        QDir::homePath() + "/.jdks",
    };
}
```

Also runs `/usr/libexec/java_home -V` to discover system-registered JDKs.

#### Windows

```cpp
QStringList JavaUtils::platformSearchPaths()
{
    QStringList paths;
    // Registry-based discovery
    FindJavaFromRegistryKey(KEY_WOW64_64KEY,
        "SOFTWARE\\JavaSoft\\Java Runtime Environment", "JavaHome");
    FindJavaFromRegistryKey(KEY_WOW64_64KEY,
        "SOFTWARE\\JavaSoft\\Java Development Kit", "JavaHome");
    FindJavaFromRegistryKey(KEY_WOW64_64KEY,
        "SOFTWARE\\JavaSoft\\JDK", "JavaHome");
    FindJavaFromRegistryKey(KEY_WOW64_64KEY,
        "SOFTWARE\\Eclipse Adoptium\\JDK", "Path", "\\hotspot\\MSI");
    FindJavaFromRegistryKey(KEY_WOW64_64KEY,
        "SOFTWARE\\Microsoft\\JDK", "Path", "\\hotspot\\MSI");

    // Filesystem paths
    paths << "C:/Program Files/Java"
          << "C:/Program Files (x86)/Java"
          << "C:/Program Files/Eclipse Adoptium"
          << "C:/Program Files/Microsoft";

    return paths;
}
```

## JavaChecker

Validates a Java installation by spawning a subprocess:

```cpp
class JavaChecker : public QObject
{
    Q_OBJECT
public:
    void performCheck();

    // Input
    QString m_path;          // Path to java binary
    int m_minMem = 0;       // Minimum memory to test
    int m_maxMem = 0;       // Maximum memory to test
    int m_permGen = 0;      // PermGen size to test

    // Results
    struct Result {
        QString path;
        QString javaVersion;
        QString realArch;       // "amd64", "aarch64", etc.
        bool valid = false;
        bool is_64bit = false;
        int id = 0;
        QString errorLog;
        QString outLog;
    };

signals:
    void checkFinished(JavaChecker::Result result);

private:
    QProcess* m_process = nullptr;
};
```

### Check Process

`JavaChecker` spawns the Java binary with a small Java program (`javacheck.jar`):

```
java -jar javacheck.jar
```

The `javacheck` program (in `libraries/javacheck/`) prints system properties:

```java
// javacheck/src/main/java/org/projecttick/meshmc/JavaCheck.java
public class JavaCheck {
    public static void main(String[] args) {
        System.out.println("os.arch=" + System.getProperty("os.arch"));
        System.out.println("java.version=" + System.getProperty("java.version"));
        System.out.println("java.vendor=" + System.getProperty("java.vendor"));
        System.out.println("sun.arch.data.model=" + System.getProperty("sun.arch.data.model"));
        System.out.println("java.runtime.name=" + System.getProperty("java.runtime.name"));
    }
}
```

Output is parsed to populate the `Result` struct.

### Timeout

The check process has a timeout (typically 30 seconds). If Java hangs or takes too long, the check is marked as failed.

## JavaCheckerJob

Batch job for checking multiple Java installations:

```cpp
class JavaCheckerJob : public Task
{
    Q_OBJECT
public:
    explicit JavaCheckerJob(QString job_name);

    void addJavaCheckerAction(JavaCheckerPtr base);

signals:
    void checkFinished(JavaChecker::Result result);

protected:
    void executeTask() override;

private slots:
    void partFinished(JavaChecker::Result result);

private:
    QList<JavaCheckerPtr> m_checks;
    int m_done = 0;
};
```

Used by `JavaInstallList` to validate all discovered Java paths in parallel.

## JavaInstall

Descriptor for a single Java installation:

```cpp
class JavaInstall
{
public:
    using Ptr = std::shared_ptr<JavaInstall>;

    QString id;            // Unique identifier
    QString path;          // Path to java binary
    JavaVersion version;   // Parsed version
    QString arch;          // Architecture (amd64, aarch64)
    bool is_64bit;         // 64-bit flag
    bool recommended;      // Whether this is the recommended choice
};
```

## JavaInstallList

Model for the discovered Java installations:

```cpp
class JavaInstallList : public BaseVersionList
{
    Q_OBJECT
public:
    void load();           // Triggers discovery + validation
    void updateListData(QList<BaseVersion::Ptr> versions) override;

    // Filtering
    BaseVersion::Ptr getRecommended();

protected:
    void loadList();
    void sortVersionList();

    QList<BaseVersion::Ptr> m_vlist;
    bool loaded = false;
};
```

### Discovery Flow

```
JavaInstallList::load()
    │
    ├── JavaUtils::FindJavaPaths()
    │   └── Returns list of java binary paths
    │
    ├── Create JavaCheckerJob
    │   └── Add JavaChecker for each path
    │
    ├── Run JavaCheckerJob
    │   ├── Spawn each java with javacheck.jar (parallel)
    │   └── Parse output → JavaChecker::Result
    │
    ├── Filter valid results
    │   └── Discard paths where valid == false
    │
    └── Create JavaInstall entries
        └── Store in m_vlist, emit signal
```

## JavaVersion

Version string parsing and comparison:

```cpp
class JavaVersion
{
public:
    JavaVersion() {}
    JavaVersion(const QString& rhs);

    bool operator<(const JavaVersion& rhs) const;
    bool operator>(const JavaVersion& rhs) const;
    bool operator==(const JavaVersion& rhs) const;
    bool requiresPermGen() const;

    int major() const { return m_major; }
    int minor() const { return m_minor; }
    int security() const { return m_security; }
    QString toString() const;

private:
    int m_major = 0;
    int m_minor = 0;
    int m_security = 0;
    QString m_prerelease;
    bool m_parseable = false;
};
```

### Version String Formats

Handles both old and new Java version schemes:

| Format | Example | Major |
|---|---|---|
| Old (1.x.y) | `1.8.0_312` | 8 |
| New (x.y.z) | `17.0.2` | 17 |
| New (x.y.z+b) | `21.0.1+12` | 21 |
| EA builds | `22-ea` | 22 |

### PermGen Detection

```cpp
bool JavaVersion::requiresPermGen() const
{
    return m_major < 8;  // PermGen removed in Java 8
}
```

Used to conditionally add `-XX:PermSize` and `-XX:MaxPermSize` JVM arguments for Java 7 and below.

## Java Compatibility

### Version Requirements

| Minecraft Version | Minimum Java | Recommended |
|---|---|---|
| 1.16.5 and below | Java 8 | Java 8 |
| 1.17 - 1.17.1 | Java 16 | Java 16 |
| 1.18 - 1.20.4 | Java 17 | Java 17 |
| 1.20.5+ | Java 21 | Java 21 |

`ComponentUpdateTask` determines the required Java version from the Minecraft version metadata and validates the configured Java installation.

### Compatibility Warnings

When launching with an incompatible Java version:

```cpp
// LaunchController checks Java compatibility
if (settings->get("IgnoreJavaCompatibility").toBool()) {
    // Skip check, launch anyway
} else {
    // Show warning dialog
    // "Minecraft X.Y requires Java Z, but Java W is configured"
    // Options: Continue anyway / Change Java / Cancel
}
```

## Managed Java Downloads

The `java/download/` subdirectory handles automatic Java downloads:

### ArchiveDownloadTask

Downloads and extracts Java archives:
- Platform-appropriate archives (tar.gz for Linux/macOS, zip for Windows)
- Progress tracking during download and extraction
- Installs to `<data_dir>/java/<version>/`

### ManifestDownloadTask

Fetches Java availability manifest:
- Queries Mojang's runtime manifest for available Java versions
- Selects appropriate version for the operating system and architecture

### Java Auto-Setup

In the setup wizard, if no compatible Java is found:

```
JavaPage (wizard)
    │
    ├── Display "No compatible Java found"
    ├── Offer to download recommended version
    │
    └── On accept:
        ├── Fetch runtime manifest
        ├── Select appropriate Java version
        ├── Download archive
        ├── Extract to data_dir/java/
        └── Set JavaPath setting to extracted binary
```

## JavaPage Settings UI

The Java settings page (`ui/pages/global/JavaPage.h`) provides:

| Control | Description |
|---|---|
| Java Path | Text field + Browse button for java binary |
| Auto-detect | Scans system and lists all found Java installations in a dialog |
| Test | Validates the current Java path using `JavaChecker` |
| Min Memory | Spinbox for minimum heap allocation (MB) |
| Max Memory | Spinbox for maximum heap allocation (MB) |
| PermGen | Spinbox for PermGen (only shown for Java < 8) |
| JVM Arguments | Text field for additional JVM flags |
| Ignore Compatibility | Checkbox to skip version compatibility checks |

### Auto-Detect Dialog

When "Auto-detect" is clicked:
1. `JavaInstallList::load()` runs full discovery
2. Results shown in a table: Path, Version, Architecture, 64-bit
3. User selects one → fills Java Path field
4. Recommended installation is highlighted
