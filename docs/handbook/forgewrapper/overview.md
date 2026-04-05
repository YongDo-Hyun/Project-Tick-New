# ForgeWrapper — Overview

## What Is ForgeWrapper?

ForgeWrapper is a specialized Java wrapper and bootstrap layer authored by ZekerZhayard that enables third-party Minecraft launchers — originally MultiMC, and now any launcher implementing the `IFileDetector` interface — to launch Minecraft 1.13+ with Forge or NeoForge mod loaders. It intercepts the standard Forge/NeoForge installation process, running the installer's post-processors in a headless (non-GUI) mode, and then configures the Java module system, classpath, and system properties required to launch the modded game.

**Root Package:** `io.github.zekerzhayard.forgewrapper.installer`  
**Language:** Java  
**Build System:** Gradle (multi-project: root + `jigsaw` subproject)  
**License:** LGPL-3.0-only  
**Source Compatibility:** Java 8 (base), Java 9+ (jigsaw multi-release overlay)  
**Version Property:** `fw_version = projt` (defined in `gradle.properties`)  

---

## The Problem ForgeWrapper Solves

The official Forge and NeoForge installers are designed to run as standalone GUI applications. They download libraries, run post-processors (such as binary patching and JAR merging), and produce a launchable game directory. This works for the vanilla Minecraft launcher, but third-party launchers like MultiMC, PrismLauncher, and custom launchers manage libraries and game directories differently.

ForgeWrapper bridges this gap by:

1. **Detecting required files** — locating the Forge/NeoForge installer JAR, the vanilla Minecraft client JAR, and the shared libraries directory via a pluggable `IFileDetector` system.
2. **Running the installer headlessly** — invoking `PostProcessors.process()` from the Forge installer API reflectively, inside an isolated `URLClassLoader`, so that binary patches and data generation happen automatically.
3. **Configuring the Java Platform Module System (JPMS)** — on Java 9+, dynamically adding modules, exports, and opens to the boot module layer at runtime using `sun.misc.Unsafe` and `MethodHandles.Lookup` tricks.
4. **Setting up the classpath** — adding extra libraries that lack direct-download URLs (and thus are missing from launcher metadata) to the system class loader at runtime.
5. **Launching the game** — loading and invoking the real main class (typically `cpw.mods.bootstraplauncher.BootstrapLauncher` for modern Forge/NeoForge) with the original command-line arguments.

---

## High-Level Architecture

ForgeWrapper is structured as a Gradle multi-project build with two subprojects:

```
forgewrapper/
├── build.gradle              # Root project: Java 8 target, multi-release JAR assembly
├── settings.gradle           # Includes the :jigsaw subproject
├── gradle.properties         # fw_version = projt
├── jigsaw/
│   ├── build.gradle          # Java 9 target, produces ModuleUtil override
│   └── src/main/java/...     # Java 9+ ModuleUtil implementation
├── src/
│   └── main/
│       ├── java/
│       │   └── io/github/zekerzhayard/forgewrapper/installer/
│       │       ├── Main.java                         # Entry point
│       │       ├── Bootstrap.java                    # JVM argument processing
│       │       ├── Installer.java                    # Forge installer integration
│       │       ├── detector/
│       │       │   ├── IFileDetector.java            # File detection interface
│       │       │   ├── DetectorLoader.java           # ServiceLoader-based loader
│       │       │   └── MultiMCFileDetector.java      # Default MultiMC detector
│       │       └── util/
│       │           └── ModuleUtil.java               # Java 8 stubs (no-ops)
│       └── resources/
│           └── META-INF/
│               └── services/
│                   └── io.github...IFileDetector      # ServiceLoader registration
```

The final JAR is a **Multi-Release JAR** (MR-JAR). The root classes are compiled for Java 8. The `jigsaw` subproject compiles a replacement `ModuleUtil` class for Java 9+, which gets placed under `META-INF/versions/9/` in the JAR. At runtime:

- On Java 8: the JVM loads the base `ModuleUtil` with no-op module methods.
- On Java 9+: the JVM loads `META-INF/versions/9/.../ModuleUtil` with full JPMS manipulation.

---

## Complete Execution Flow

The entire lifecycle of a ForgeWrapper invocation proceeds through these phases:

### Phase 1: Entry and Argument Parsing (`Main.main()`)

The launcher starts ForgeWrapper by calling `Main.main(String[] args)`. The arguments are the standard Forge/NeoForge FML launch arguments:

```
--fml.neoForgeVersion 20.2.20-beta
--fml.fmlVersion 1.0.2
--fml.mcVersion 1.20.2
--fml.neoFormVersion 20231019.002635
--launchTarget forgeclient
```

`Main` converts the args array to a mutable `List<String>` via `Stream.of(args).collect(Collectors.toList())` and parses the following:

| Variable          | Source Argument                       | Example Value          |
|-------------------|---------------------------------------|------------------------|
| `isNeoForge`      | presence of `--fml.neoForgeVersion`   | `true`                 |
| `mcVersion`       | `--fml.mcVersion`                     | `1.20.2`               |
| `forgeGroup`      | `--fml.forgeGroup` or default `net.neoforged` | `net.neoforged` |
| `forgeArtifact`   | `neoforge` if NeoForge, else `forge`  | `neoforge`             |
| `forgeVersion`    | `--fml.neoForgeVersion` or `--fml.forgeVersion` | `20.2.20-beta` |
| `forgeFullVersion`| NeoForge: version alone; Forge: `mcVersion-forgeVersion` | `20.2.20-beta` |

**Key distinction:** NeoForge versions after 20.2.x use `--fml.neoForgeVersion`; early NeoForge for 1.20.1 is not handled by this codepath.

### Phase 2: File Detection (`DetectorLoader` + `IFileDetector`)

`DetectorLoader.loadDetector()` uses `java.util.ServiceLoader` to discover all implementations of `IFileDetector`. It builds a `HashMap<String, IFileDetector>` map of `name -> detector`, then iterates through each entry. For each detector, it creates a copy of the map with that detector removed (the `others` map) and calls `detector.enabled(others)`. Exactly one detector must return `true` from `enabled()`; zero enabled detectors causes `"No file detector is enabled!"`, and two or more causes `"There are two or more file detectors are enabled!"`.

The default `MultiMCFileDetector` returns `true` from `enabled()` only when `others.size() == 0` — that is, when it is the sole registered detector. This means any launcher that registers its own `IFileDetector` will automatically disable `MultiMCFileDetector`.

After detection, `Main` validates that both the installer JAR and Minecraft JAR exist as regular files via `Files.isRegularFile()`.

### Phase 3: Isolated ClassLoader and Data Extraction (`Installer.getData()`)

`Main` creates a child `URLClassLoader` containing:
1. ForgeWrapper's own JAR (from `Main.class.getProtectionDomain().getCodeSource().getLocation()`)
2. The Forge/NeoForge installer JAR

This classloader's parent is `ModuleUtil.getPlatformClassLoader()` — on Java 8 this returns `null` (bootstrap loader), on Java 9+ it returns the platform class loader. This isolation prevents the installer's classes from conflicting with the launcher's classpath.

Through this classloader, `Main` reflectively loads `Installer.class` and calls `getData(libraryDir)`, which:

1. Calls `Util.loadInstallProfile()` to parse the installer's embedded `install_profile.json`.
2. Wraps it in `InstallV1Wrapper` (a subclass of `InstallV1`), storing a reference to `librariesDir`.
3. Loads the embedded version JSON (e.g., `version.json`) via `Version0.loadVersion()` using the JSON path from the install profile.
4. Extracts the main class name, JVM arguments, and extra library paths into a `HashMap`.
5. Returns a `Map<String, Object>` with keys: `"mainClass"`, `"jvmArgs"`, `"extraLibraries"`.

### Phase 4: JVM Bootstrap (`Bootstrap.bootstrap()`)

`Bootstrap.bootstrap()` receives the JVM args array, the Minecraft JAR filename, and the library directory path. It performs placeholder replacement, classpath sanitization, JVM argument extraction, and module system configuration. See the [Bootstrap System](bootstrap-system.md) document for full details.

### Phase 5: Installation (`Installer.install()`)

`Installer.install()` runs the Forge/NeoForge post-processors via reflective invocation of `PostProcessors.process()`. The `InstallV1Wrapper` adds caching of processor outputs and an optional hash-check bypass. See the [Installer System](installer-system.md) document for full details.

### Phase 6: Classpath Setup and Game Launch

After installation succeeds:

1. `ModuleUtil.setupClassPath()` adds extra libraries (those with empty download URLs in the version JSON) to the system class loader at runtime.
2. `ModuleUtil.setupBootstrapLauncher()` ensures the main class's package is open to ForgeWrapper's module (Java 9+ only).
3. The main class's `main(String[])` method is invoked with the original `args`, launching the game.

---

## Key Design Decisions

### Multi-Release JAR Strategy

ForgeWrapper must run on both Java 8 and Java 17+. Rather than using two separate JARs or runtime Java version checks, it uses the Multi-Release JAR specification (JEP 238). The base `ModuleUtil` provides no-op stubs for Java 8, while the jigsaw version provides full JPMS manipulation for Java 9+.

### ServiceLoader-Based Detector Plugin System

The `IFileDetector` interface allows any launcher to provide its own file detection logic without modifying ForgeWrapper. Launchers add their IFileDetector implementation JAR to the classpath along with a `META-INF/services` file. The `DetectorLoader` ensures exactly one detector is active — this prevents conflicting detection logic from multiple launchers.

### Isolated URLClassLoader for Installer

The Forge installer JAR contains classes (from `net.minecraftforge.installer`) that may conflict with the game's runtime classes. By loading them in a child `URLClassLoader` with the platform class loader as parent (rather than the application class loader), ForgeWrapper keeps the installer isolated. The `URLClassLoader` is wrapped in a try-with-resources block and closed after use.

### Reflective Access via sun.misc.Unsafe

On Java 9+, the JPMS restricts access to internal APIs. ForgeWrapper's jigsaw `ModuleUtil` uses `sun.misc.Unsafe` to obtain `MethodHandles.Lookup.IMPL_LOOKUP`, which has unrestricted access. This is necessary to:
- Add modules to the boot layer at runtime
- Modify `Configuration` internal fields (`graph`, `modules`, `nameToModule`)
- Add exports and opens between modules
- Access `jdk.internal.loader.BuiltinClassLoader.loadModule()`

---

## NeoForge vs. Forge Detection

ForgeWrapper distinguishes between NeoForge and legacy Forge by checking for `--fml.neoForgeVersion` in the argument list:

| Property            | Forge                           | NeoForge                        |
|---------------------|---------------------------------|---------------------------------|
| Detection key       | `--fml.forgeVersion`            | `--fml.neoForgeVersion`         |
| Default group       | `net.minecraftforge`            | `net.neoforged`                 |
| Artifact name       | `forge`                         | `neoforge`                      |
| Full version format | `{mcVersion}-{forgeVersion}`    | `{forgeVersion}` (standalone)   |

The `--fml.forgeGroup` argument can override the default group for either variant.

---

## JVM System Properties Reference

ForgeWrapper recognizes and uses the following system properties:

| Property                        | Purpose                                             | Default                |
|---------------------------------|-----------------------------------------------------|------------------------|
| `forgewrapper.librariesDir`     | Override library directory path                     | Auto-detected          |
| `forgewrapper.installer`        | Override installer JAR path                         | Auto-detected          |
| `forgewrapper.minecraft`        | Override Minecraft client JAR path                  | Auto-detected          |
| `forgewrapper.skipHashCheck`    | Skip processor output hash verification             | `false`                |
| `libraryDirectory`              | Library directory for Forge internal use             | Set by Bootstrap       |
| `ignoreList`                    | Comma-separated list of JARs to ignore in ModLauncher | Extended by Bootstrap |
| `java.net.preferIPv4Stack`      | Force IPv4 for Forge network operations             | `true` if not set      |

---

## Version Scheme

The version string is assembled in `build.gradle`:

```groovy
version = "${fw_version}${-> getVersionSuffix()}"
```

Where:
- `fw_version` is `projt` (from `gradle.properties`)
- The suffix is `-yyyy-MM-dd` in CI (`IS_PUBLICATION` env var or `GITHUB_ACTIONS == "true"`), or `-LOCAL` for local builds

Examples: `projt-2024-03-15`, `projt-LOCAL`.

---

## Dependencies

ForgeWrapper declares all major dependencies as `compileOnly` — they are expected to be on the classpath at runtime (provided by the launcher or the installer JAR):

| Dependency                              | Version | Purpose                                    |
|-----------------------------------------|---------|--------------------------------------------|
| `com.google.code.gson:gson`             | 2.8.7   | JSON parsing for install profiles          |
| `cpw.mods:modlauncher`                  | 8.0.9   | Forge's mod loading framework              |
| `net.minecraftforge:installer`          | 2.2.7   | Forge installer API (`PostProcessors`, `InstallV1`, etc.) |
| `net.sf.jopt-simple:jopt-simple`        | 5.0.4   | Command-line argument parsing              |

The `jigsaw` subproject has no additional dependencies — it only uses JDK internal APIs.

---

## Repository Configuration

The build resolves dependencies from:

```groovy
repositories {
    mavenCentral()
    maven {
        name = "forge"
        url = "https://maven.minecraftforge.net/"
    }
}
```

The Forge Maven repository provides the `net.minecraftforge:installer` and `cpw.mods:modlauncher` artifacts, which are not available on Maven Central.

---

## Summary of Source Files

| File                        | Role                                                        |
|-----------------------------|-------------------------------------------------------------|
| `Main.java`                 | Entry point: argument parsing, detector loading, orchestration |
| `Bootstrap.java`            | JVM argument processing, module system setup delegation      |
| `Installer.java`            | Forge installer integration, post-processor execution        |
| `IFileDetector.java`        | Interface for file detection with default implementations    |
| `DetectorLoader.java`       | ServiceLoader-based detector discovery and validation        |
| `MultiMCFileDetector.java`  | Default file detector using Maven-style library paths        |
| `ModuleUtil.java` (base)    | Java 8 no-op stubs for module operations                    |
| `ModuleUtil.java` (jigsaw)  | Java 9+ full JPMS manipulation via Unsafe/MethodHandles     |

---

## Further Reading

- [Architecture](architecture.md) — Detailed class relationships and data flow diagrams
- [Bootstrap System](bootstrap-system.md) — JVM argument processing and placeholder replacement
- [Installer System](installer-system.md) — Forge/NeoForge installer integration mechanics
- [Module System](module-system.md) — JPMS manipulation on Java 9+
- [File Detection](file-detection.md) — The IFileDetector plugin system
- [NeoForge Support](neoforge-support.md) — NeoForge-specific handling
- [Building](building.md) — Build instructions and Gradle configuration
- [Java Compatibility](java-compatibility.md) — Multi-release JAR and Java version support
- [Gradle Configuration](gradle-configuration.md) — Detailed build.gradle analysis
- [Code Style](code-style.md) — Coding conventions
- [Contributing](contributing.md) — Contribution guidelines
