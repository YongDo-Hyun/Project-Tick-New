# ForgeWrapper Architecture

## Table of Contents
1. [Class Hierarchy and Structure](#class-hierarchy-and-structure)
2. [Package Organization](#package-organization)
3. [Design Patterns Used](#design-patterns-used)
4. [Data Flow and Execution Model](#data-flow-and-execution-model)
5. [Thread Model and Concurrency](#thread-model-and-concurrency)
6. [Error Handling Strategy](#error-handling-strategy)
7. [Dual-Module System](#dual-module-system)
8. [ModuleUtil Implementation Details](#moduleutil-implementation-details)
9. [Class Diagram](#class-diagram)
10. [Reflection-Based Architecture](#reflection-based-architecture)

## Class Hierarchy and Structure

### Core Class Relationships

```
io.github.zekerzhayard.forgewrapper.installer package:
│
├─ Main (public class)
│  ├─ main(String[]) : void - ENTRY POINT
│  └─ isFile(Path) : boolean
│
├─ Installer (public class)
│  ├─ getData(File) : Map<String, Object>
│  ├─ install(File, File, File) : boolean
│  ├─ getWrapper(File) : InstallV1Wrapper
│  │
│  ├─ InstallV1Wrapper (public static inner class extends InstallV1)
│  │  ├─ processors : Map<String, List<Processor>>
│  │  ├─ librariesDir : File
│  │  ├─ getProcessors(String) : List<Processor>
│  │  ├─ checkProcessorFiles(List<Processor>, Map<String, String>, File) : void
│  │  └─ setOutputs(Processor, Map<String, String>) : void
│  │
│  └─ Version0 (public static class extends Version)
│     ├─ mainClass : String
│     ├─ arguments : Arguments
│     ├─ getMainClass() : String
│     ├─ getArguments() : Arguments
│     └─ Arguments (public static inner class)
│        ├─ jvm : String[]
│        └─ getJvm() : String[]
│
├─ Bootstrap (public class)
│  └─ bootstrap(String[], String, String) : void throws Throwable
│
└─ detector package:
   │
   ├─ IFileDetector (public interface)
   │  ├─ name() : String (abstract)
   │  ├─ enabled(HashMap<String, IFileDetector>) : boolean (abstract)
   │  ├─ getLibraryDir() : Path (default, can override)
   │  ├─ getInstallerJar(String, String, String) : Path (default)
   │  └─ getMinecraftJar(String) : Path (default)
   │
   ├─ DetectorLoader (public class)
   │  └─ loadDetector() : IFileDetector (static)
   │
   └─ MultiMCFileDetector (public class implements IFileDetector)
      ├─ libraryDir : Path
      ├─ installerJar : Path
      ├─ minecraftJar : Path
      ├─ name() : String
      ├─ enabled(HashMap<String, IFileDetector>) : boolean
      ├─ getLibraryDir() : Path
      ├─ getInstallerJar(String, String, String) : Path
      └─ getMinecraftJar(String) : Path

util package:
│
└─ ModuleUtil (public class)
   ├─ addModules(String) : void throws Throwable
   ├─ addExports(List<String>) : void
   ├─ addOpens(List<String>) : void
   ├─ setupClassPath(Path, List<String>) : void throws Throwable
   ├─ setupBootstrapLauncher(Class<?>) : Class<?>
   ├─ getPlatformClassLoader() : ClassLoader
   └─ (version-specific implementation)
```

### Field Definitions and Types

#### Main.java Fields
None (purely algorithmic, no state)

#### Installer.java Fields
```java
// Static field
private static InstallV1Wrapper wrapper;  // Cached wrapper instance

// Inner class InstallV1Wrapper fields
protected Map<String, List<Processor>> processors;  // Cache of side-specific processors
protected File librariesDir;                        // Base libraries directory
protected String serverJarPath;                     // Server JAR path (inherited)

// Inner class Version0 fields
protected String mainClass;                         // Forge bootstrap main class
protected Version0.Arguments arguments;             // JVM arguments
protected String[] jvm;                             // JVM argument array (in Arguments)
```

#### Bootstrap.java Fields
None (purely algorithmic, no state)

#### DetectorLoader.java Fields
None (stateless utility)

#### MultiMCFileDetector.java Fields
```java
protected Path libraryDir;      // Cached library directory (lazy-initialized)
protected Path installerJar;    // Cached installer JAR path
protected Path minecraftJar;    // Cached Minecraft JAR path
```

#### ModuleUtil.java Fields

**Main version (Java 8)**:
```java
// No fields - all methods are no-op for Java 8
```

**Jigsaw version (Java 9+)**:
```java
private final static MethodHandles.Lookup IMPL_LOOKUP;  // Internal method handle lookup

// TypeToAdd enum fields
EXPORTS {
    private final MethodHandle implAddMH;               // Invoke implAddExports()
    private final MethodHandle implAddToAllUnnamedMH;   // Invoke implAddExportsToAllUnnamed()
}
OPENS {
    private final MethodHandle implAddMH;               // Invoke implAddOpens()
    private final MethodHandle implAddToAllUnnamedMH;   // Invoke implAddOpensToAllUnnamed()
}

// ParserData fields
class ParserData {
    final String module;    // Source module name
    final String packages;  // Package names to export/open
    final String target;    // Target module or ALL-UNNAMED
}
```

## Package Organization

### Main Package Structure

```
io.github.zekerzhayard.forgewrapper.installer/
├─ Main.java (entry point, 50 lines)
├─ Installer.java (installer integration, 200+ lines)
├─ Bootstrap.java (runtime configuration, 70 lines)
│
├─ detector/ (file detection subsystem)
│  ├─ IFileDetector.java (interface contract, 90 lines)
│  ├─ DetectorLoader.java (SPI loader, 30 lines)
│  └─ MultiMCFileDetector.java (concrete impl, 40 lines)
│
└─ util/ (utility functions)
   └─ ModuleUtil.java (Java 8 version, 40 lines)

jigsaw/src/main/java/io.../util/
└─ ModuleUtil.java (Java 9+ version, 200+ lines)
```

### Resource Structure

```
src/main/resources/
└─ META-INF/services/
   └─ io.github.zekerzhayard.forgewrapper.installer.detector.IFileDetector
      (contains: io.github.zekerzhayard.forgewrapper.installer.detector.MultiMCFileDetector)

jigsaw/src/main/resources/
(empty - no SPI definitions)
```

### Build Output Structure

The Gradle multi-release JAR structure (from `build.gradle` lines 47-54):

```
ForgeWrapper.jar/
├─ io/github/zekerzhayard/forgewrapper/installer/
│  ├─ Main.class (Java 8 version)
│  ├─ Installer.class including inner classes (Java 8 version)
│  ├─ Bootstrap.class (Java 8 version)
│  ├─ detector/
│  │  ├─ IFileDetector.class (Java 8 version)
│  │  ├─ DetectorLoader.class (Java 8 version)
│  │  └─ MultiMCFileDetector.class (Java 8 version)
│  └─ util/
│     └─ ModuleUtil.class (Java 8 stub version)
│
├─ META-INF/versions/9/
│  └─ io/github/zekerzhayard/forgewrapper/installer/
│     └─ util/
│        └─ ModuleUtil.class (Java 9+ Advanced version)
│
└─ META-INF/services/
   └─ io.github.zekerzhayard.forgewrapper.installer.detector.IFileDetector
```

When running on Java 9+, the JVM automatically prefers classes in `META-INF/versions/9/` over base classes.

## Design Patterns Used

### 1. Service Provider Interface (SPI) Pattern

**Location**: `DetectorLoader.java` and `IFileDetector` interface

**Purpose**: Decouple file detection from launcher specifics. Allows multiple implementations without modifying core code.

**Implementation**:
- `IFileDetector` defines the contract
- `MultiMCFileDetector` implements it
- `META-INF/services/` file registers the implementation
- `ServiceLoader.load()` discovers implementations at runtime

**Benefits**:
- Custom launchers can provide their own detector by adding a JAR with SPI metadata
- ForgeWrapper doesn't need recompilation
- Multiple detectors can coexist; the `enabled()` logic selects the active one

**Code Example** (from `DetectorLoader.java` lines 4-8):
```java
ServiceLoader<IFileDetector> sl = ServiceLoader.load(IFileDetector.class);
HashMap<String, IFileDetector> detectors = new HashMap<>();
for (IFileDetector detector : sl) {
    detectors.put(detector.name(), detector);
}
```

### 2. Strategy Pattern

**Location**: `IFileDetector` implementations (current: `MultiMCFileDetector`)

**Purpose**: Encapsulate different file location strategies for different launchers.

**Implementation**:
- `IFileDetector` is the strategy interface defining `getLibraryDir()`, `getInstallerJar()`, `getMinecraftJar()`
- `MultiMCFileDetector` is the concrete strategy for MultiMC/MeshMC
- Future: `MeshMCFileDetector`, `ATLauncherFileDetector` could be added

**Algorithm Variation**: File path construction differs:
- MultiMC: `libraries/net/neoforged/neoforge/VERSION/neoforge-VERSION-installer.jar`
- Another launcher: might use different structure

**Code Example** (from `MultiMCFileDetector.java` lines 25-34):
```java
@Override
public Path getInstallerJar(String forgeGroup, String forgeArtifact, String forgeFullVersion) {
    Path path = IFileDetector.super.getInstallerJar(...);
    if (path == null) {
        Path installerBase = this.getLibraryDir();
        for (String dir : forgeGroup.split("\\."))
            installerBase = installerBase.resolve(dir);
        return installerBase.resolve(forgeArtifact).resolve(forgeFullVersion)...;
    }
    return path;
}
```

### 3. Template Method Pattern

**Location**: `IFileDetector.getLibraryDir()` default implementation

**Purpose**: Provide default library discovery algorithm while allowing overrides.

**Implementation**:
- `IFileDetector` provides `getLibraryDir()` implementation (lines 34-63)
- `MultiMCFileDetector` calls `IFileDetector.super.getLibraryDir()` then caches (lines 19-23)
- Template steps:
  1. Check system property
  2. Find launcher JAR location
  3. Walk up directory tree to find `libraries` folder
  4. Return absolute path

**Template Hook**: Subclasses can call `super.getLibraryDir()` to use default or override entirely

**Code Example** (from `IFileDetector.java` lines 34-63):
```java
default Path getLibraryDir() {
    String libraryDir = System.getProperty("forgewrapper.librariesDir");
    if (libraryDir != null) {
        return Paths.get(libraryDir).toAbsolutePath();
    }
    // ... walk classloader resources and find libraries/
}
```

### 4. Wrapper/Adapter Pattern

**Location**: `Installer.InstallV1Wrapper` class

**Purpose**: Adapt Forge's `InstallV1` class to handle missing processor outputs.

**Implementation**:
- Extends `InstallV1` (the wrapped class)
- Overrides `getProcessors()` to intercept processor handling
- Calls `super.getProcessors()` to get original implementation
- Wraps result with custom `checkProcessorFiles()` logic

**Problem Solved**: Forge's installer expects all processor outputs to have SHA1 hashes, but some libraries may not. ForgeWrapper computes hashes for files that exist.

**Code Example** (from `Installer.java` lines 42-52):
```java
@Override
public List<Processor> getProcessors(String side) {
    List<Processor> processor = this.processors.get(side);
    if (processor == null) {
        checkProcessorFiles(
            processor = super.getProcessors(side),  // get original
            super.getData("client".equals(side)),
            this.librariesDir
        );
        this.processors.put(side, processor);
    }
    return processor;
}
```

### 5. Facade Pattern

**Location**: `Main.java`

**Purpose**: Provide simplified interface to complex subsystems (Detector, Installer, Bootstrap, ModuleUtil).

**Implementation**:
- `Main.main()` coordinates multiple subsystems
- Hides detector loading complexity
- Hides custom classloader creation
- Hides reflection-based Installer invocation
- Hides version-specific ModuleUtil calls

**Client Perspective**: Launcher only needs to invoke:
```bash
java -cp ForgeWrapper.jar io.github.zekerzhayard.forgewrapper.installer.Main args...
```

**Code Example** (from `Main.java` lines 17-46):
```java
public static void main(String[] args) throws Throwable {
    // Simplified entry point - complex logic hidden
    IFileDetector detector = DetectorLoader.loadDetector();
    // ... use detector ...
    // ... create classloader ...
    // ... invoke Installer via reflection ...
}
```

### 6. Lazy Initialization Pattern

**Location**: `MultiMCFileDetector` field initialization and `Installer` wrapper caching

**Purpose**: Defer expensive operations until actually needed.

**Implementation** (from `MultiMCFileDetector.java`):
```java
@Override
public Path getLibraryDir() {
    if (this.libraryDir == null) {
        this.libraryDir = IFileDetector.super.getLibraryDir();  // computed on first access
    }
    return this.libraryDir;
}
```

**Benefits**:
- If detector method isn't called, path resolution doesn't happen
- Caching prevents repeated expensive classloader searches
- Memory efficient for unused paths

### 7. Builder Pattern (Implicit)

**Location**: `Installer.Version0.Arguments` and `Installer.Version0`

**Purpose**: Construct complex configuration objects from JSON.

**Implementation**:
- `Version0.loadVersion()` (lines 209-214) deserializes from JSON using GSON
- JSON structure becomes nested object graph
- Fields are populated via reflection by GSON

**Code Example** (from `Installer.java` lines 203-214):
```java
public static Version0 loadVersion(Install profile) {
    try (InputStream stream = Util.class.getResourceAsStream(profile.getJson())) {
        return Util.GSON.fromJson(new InputStreamReader(stream, StandardCharsets.UTF_8), Version0.class);
    } catch (Throwable t) {
        throw new RuntimeException(t);
    }
}
```

## Data Flow and Execution Model

### Execution Pipeline

```
Phase 1: Invocation
  Launcher executes: java -cp ForgeWrapper.jar ... Main args...
    │
    ├─ JVM starts ForgeWrapper JAR as classpath
    ├─ Java locates Main.main(String[]) entry point
    └─ Execution begins

Phase 2: Argument Parsing (Main.java lines 17-27)
  args[] contains: --fml.mcVersion 1.20.2 --fml.neoForgeVersion 20.2.20-beta ...
    │
    ├─ Convert String[] to List for easier manipulation
    ├─ Check for --fml.neoForgeVersion to detect NeoForge vs Forge
    ├─ Search list for mcVersion index, forge version index
    ├─ Extract and store values
    └─ Result: 4 string variables containing version info

Phase 3: Detector Selection (Main.java line 28)
  Calls: IFileDetector detector = DetectorLoader.loadDetector()
    │
    ├─ ServiceLoader scans classpath for IFileDetector implementations
    ├─ Instantiates each implementation (no-arg constructor)
    ├─ Builds HashMap of name() → detector instance
    ├─ Iterates to find exactly one with enabled(others) == true
    ├─ Validates no conflicts
    └─ Returns single enabled detector (MultiMCFileDetector)

Phase 4: File Location (Main.java lines 30-36)
  Calls detector methods to locate files:
    │
    ├─ detector.getInstallerJar(forgeGroup, forgeArtifact, forgeFullVersion)
    │  └─ Returns Path or null
    ├─ Validates with isFile(Path)
    ├─ If not found, throws "Unable to detect the forge installer!"
    │
    ├─ detector.getMinecraftJar(mcVersion)
    │  └─ Returns Path or null
    ├─ Validates with isFile(Path)
    └─ If not found, throws "Unable to detect the Minecraft jar!"

Phase 5: Custom Classloader Creation (Main.java lines 38-41)
  Creates URLClassLoader with:
    │
    ├─ URLClassLoader.newInstance(new URL[] {
    │    Main.class.getProtectionDomain().getCodeSource().getLocation(),  // ForgeWrapper.jar
    │    installerJar.toUri().toURL()                                    // forge-installer.jar
    │  }, ModuleUtil.getPlatformClassLoader())
    │
    └─ Result: custom classloader with both JARs

Phase 6: Installer Invocation via Reflection (Main.java lines 42-43)
  Loads Installer from custom classloader:
    │
    ├─ Class<?> installer = ucl.loadClass("io.github.zekerzhayard.forgewrapper.installer.Installer")
    ├─ installer.getMethod("getData", File.class) gets method reference
    │
    ├─ Invokes: installer.getMethod("getData", File.class)
    │          .invoke(null, detector.getLibraryDir().toFile())
    │
    └─ Returns: Map<String, Object> with mainClass, jvmArgs, extraLibraries

Phase 7: Bootstrap Configuration (Main.java line 44)
  Calls: Bootstrap.bootstrap((String[]) data.get("jvmArgs"), ...)
    │
    ├─ Replaces placeholders in JVM arguments
    ├─ Removes NewLaunch.jar from classpath
    ├─ Extracts module paths from arguments
    ├─ Calls ModuleUtil.addModules(), addExports(), addOpens()
    ├─ Sets up system properties
    └─ Returns normally (or logs error if Java 8)

Phase 8: Installation Execution (Main.java line 47)
  Invokes: installer.getMethod("install", File.class, File.class, File.class)
          .invoke(null, libraryDir, minecraftJar, installerJar)
    │
    ├─ Installer.install() creates InstallV1Wrapper
    ├─ Loads installation profile from installer JAR
    ├─ Gets processors via getProcessors(side)
    ├─ InstallV1Wrapper.checkProcessorFiles() computes missing hashes
    ├─ PostProcessors.process() executes installation
    ├─ Libraries are downloaded or linked
    ├─ Mod loaders are configured
    └─ Returns boolean success

Phase 9: Classloader Setup (Main.java line 50)
  Calls: ModuleUtil.setupClassPath(detector.getLibraryDir(), extraLibraries)
    │
    ├─ Reflects into sun.misc.Unsafe to access internal classloader
    ├─ Gets URLClassPath for system classloader
    ├─ Adds URLs for all extra libraries
    └─ System classloader now has all library JARs in path

Phase 10: Module System Finalization (Main.java line 51)
  Calls: Class<?> mainClass = ModuleUtil.setupBootstrapLauncher(...)
    │
    ├─ Java 8: returns mainClass unchanged (no-op)
    ├─ Java 9+: performs final module system configuration
    └─ Result: ClassLoader-ready class

Phase 11: Forge Invocation (Main.java line 52)
  Calls: mainClass.getMethod("main", String[].class)
         .invoke(null, new Object[] {args})
    │
    ├─ Forge bootstrap launcher main() method is invoked
    ├─ Original launcher arguments are passed through
    ├─ Forge initializes mod framework
    ├─ Minecraft main is invoked
    └─ Game runs under mod loader
```

### Data Transformation Chain

```
Original Arguments from Launcher
  ↓
Parsed into: { mcVersion, forgeVersion, forgeGroup, isNeoForge, ... }
  ↓
Detector#get*() methods
  ↓
File Path objects
  ↓
URLClassLoader creation
  ↓
Installer class loading and invocation
  ↓
Installation Profile (JSON deserialization)
  ↓
Version0 object with mainClass, Arguments
  ↓
JVM Arguments string array with placeholders
  ↓
Bootstrap placeholder replacement
  ↓
Modified JVM Arguments suitable for Forge
  ↓
Module paths extracted from arguments
  ↓
ModuleUtil#addModules() invocations
  ↓
Modified Java module layer
  ↓
Final Forge main class invocation
  ↓
Running Minecraft with mods
```

## Thread Model and Concurrency

### Single-Threaded Execution Model

ForgeWrapper operates entirely in a single thread (the launcher thread):

1. **Main thread**: Entry point and execution coordinator
2. **No worker threads**: ForgeWrapper itself doesn't spawn threads
3. **Blocking operations**: All I/O is synchronous and blocking
4. **Sequential execution**: Phases execute in strict order

**Code Flow**:
```java
Main.main(args) // launcher thread
  ├─ DetectorLoader.loadDetector() // synchronous
  └─ detector.getInstallerJar() // synchronous
  └─ detector.getMinecraftJar() // synchronous
  └─ URLClassLoader creation // synchronous
  └─ Installer.getData() // synchronous
  └─ Bootstrap.bootstrap() // synchronous
  └─ ModuleUtil methods // synchronous
  └─ Installer.install() // synchronous (Forge may download, happens here)
  └─ mainClass.main(args) // delegates to Forge (Forge may be multi-threaded)
```

### Concurrency Considerations

**Not Thread-Safe**:
- `MultiMCFileDetector` field caching (lines 7-9 in `MultiMCFileDetector.java`) is not synchronized
- If multiple threads called detector methods, race conditions could occur
- However, this is not a concern because ForgeWrapper is designed as single-shot execution

**Why Single-Threaded Design**:
1. Launchers invoke ForgeWrapper once, get result, and move on
2. No need for concurrent execution
3. Simplifies error handling (no thread coordination needed)
4. Reduces memory overhead

**Installer Thread Safety**:
- `Installer.wrapper` static field (line 23 in `Installer.java`) is cached but not synchronized
- Safe because it's only accessed during single-shot initialization
- Even if multiple threads accessed, worst case is re-creation (not data corruption)

### Reflection and ClassLoader Thread Safety

The custom URLClassLoader (Main.java line 38-41) operates in a thread-safe manner:
- ClassLoader has internal synchronization for class loading
- Multiple threads can load classes from same classloader
- However, ForgeWrapper doesn't use this capability

**ModuleUtil Reflection Safety** (jigsaw version):
- Uses `sun.misc.Unsafe` for low-level access
- `Unsafe.putObject()` operations are atomic at hardware level
- Module layer is designed to be modified during startup only
- No concurrent access to module layer during bootstrap

## Error Handling Strategy

### Error Scenarios and Recovery

#### Scenario 1: Detector Not Found

**Code** (from `DetectorLoader.java` line 21):
```java
if (temp == null) {
    throw new RuntimeException("No file detector is enabled!");
}
```

**When**: No implementations of `IFileDetector` found in `META-INF/services/`

**Recovery**: None - launcher must ensure ForgeWrapper JAR is complete and classpath includes implementations

**User Impact**: Launcher cannot start; must fix installation

#### Scenario 2: Multiple Detectors Enabled

**Code** (from `DetectorLoader.java` lines 18-19):
```java
} else if (detector.getValue().enabled(others)) {
    throw new RuntimeException("There are two or more file detectors are enabled! ...");
}
```

**When**: Two implementations both return `true` from `enabled()`

**Recovery**: None - one detector must disable itself

**User Impact**: Configuration error; launcher installation needs repair

#### Scenario 3: Installer JAR Not Found

**Code** (from `Main.java` line 32):
```java
if (!isFile(installerJar)) {
    throw new RuntimeException("Unable to detect the forge installer!");
}
```

**When**: 
- `detector.getInstallerJar()` returns null
- Or returns path to non-existent file
- Or returns directory instead of file

**Recovery Hints**:
- Check ForgeWrapper properties: `-Dforgewrapper.installer=/path/to/jar`
- Verify installer JAR exists at expected MultiMC paths
- Ensure forge version is correctly specified

**User Impact**: Modded instance cannot launch

#### Scenario 4: Minecraft JAR Not Found

**Code** (from `Main.java` line 36):
```java
if (!isFile(minecraftJar)) {
    throw new RuntimeException("Unable to detect the Minecraft jar!");
}
```

**When**:
- Vanilla Minecraft not installed in expected location
- Path resolution failed

**Recovery Hints**:
- Manually specify: `-Dforgewrapper.minecraft=/path/to/1.20.2.jar`
- Ensure vanilla Minecraft is installed first

**User Impact**: Modded instance cannot launch

#### Scenario 5: Library Directory Not Resolvable

**Code** (from `IFileDetector.java` line 60):
```java
throw new UnsupportedOperationException(
    "Could not detect the libraries folder - it can be manually specified with `-Dforgewrapper.librariesDir=`"
);
```

**When**:
- Classloader resources don't contain expected Forge classes
- Libraries folder walking fails
- ClassLoader inspection fails

**Recovery**:
- Set property: `-Dforgewrapper.librariesDir=/home/user/.minecraft/libraries`
- Standard recovery path is well-documented

**User Impact**: ForgeWrapper fails at startup; user must configure manually

#### Scenario 6: Installation Failed

**Code** (from `Main.java` line 47):
```java
if (!((boolean) installer.getMethod("install", ...)
      .invoke(null, ...))) {
    return;  // Silent failure
}
```

**When**: `PostProcessors.process()` returns false

**Recovery**: None built-in

**User Impact**: 
- Mod loaders not installed
- Minecraft may fail to start
- User must reinstall or redownload

#### Scenario 7: Bootstrap Configuration Error

**Code** (from `Main.java` lines 44-46):
```java
try {
    Bootstrap.bootstrap(...);
} catch (Throwable t) {
    t.printStackTrace();  // Error logged but execution continues
}
```

**When**: JVM argument parsing fails or module configuration fails

**Recovery**: Execution continues regardless

**Rationale**: Bootstrap errors should not prevent Forge initialization attempt

**User Impact**: Depends on specific error; may cause module system misconfiguration

#### Scenario 8: Reflection Error (ModuleUtil)

**Code** (from jigsaw `ModuleUtil.java` line 35):
```java
} catch (Throwable t) {
    throw new RuntimeException(t);
}
```

**When**: Module system manipulation via reflection fails

**Recovery**: None - throws RuntimeException

**User Impact**: Java 9+ users cannot run modded Minecraft; critical error

## Dual-Module System

ForgeWrapper uses a sophisticated dual-module architecture to support both Java 8 and Java 9+:

### File Organization

**Main project** (`src/main/`):
- Targets Java 8 (minimum compatibility)
- Contains all public APIs
- `ModuleUtil.java` is stub implementation with all methods as no-ops
- This is the "default" version used on Java 8

**Jigsaw subproject** (`jigsaw/src/main/`):
- Targets Java 9+
- Compiles with `-targetCompatibility 9`
- Contains advanced `ModuleUtil.java` with full module system support
- Only compiled if JDK 9+ is available for compilation
- Classes packaged into `META-INF/versions/9/` in final JAR

### Build Configuration

**From `build.gradle` lines 23-27**:
```gradle
configurations {
    multirelase {
        implementation.extendsFrom multirelase  // typo in original, should be multirelease
    }
}

dependencies {
    multirelase project(":jigsaw")  // jigsaw output goes into multirelease config
}
```

**From `build.gradle` lines 47-54**:
```gradle
jar {
    into "META-INF/versions/9", {
        from configurations.multirelase.files.collect {
            zipTree(it)
        }
    }
}
```

**From `jigsaw/build.gradle` lines 14-23**:
```gradle
compileJava {
    if (JavaVersion.current() < JavaVersion.VERSION_1_9) {
        javaCompiler = javaToolchains.compilerFor {
            languageVersion = JavaLanguageVersion.of(9)
        }
    }
    sourceCompatibility = 9
    targetCompatibility = 9
}
```

### Runtime Behavior

**On Java 8**:
```
JVM loads ForgeWrapper.jar
  ├─ Main class path: io/github/zekerzhayard/forgewrapper/installer/Main.class (Java 8 version)
  ├─ ModuleUtil path: io/.../util/ModuleUtil.class (Java 8 stub)
  │   ├─ addModules() → no-op
  │   ├─ addExports() → no-op
  │   ├─ addOpens() → no-op
  │   └─ getPlatformClassLoader() → returns null
  └─ Execution proceeds with no module system features
```

**On Java 9+**:
```
JVM loads ForgeWrapper.jar
  ├─ Main class path: io/github/zekerzhayard/forgewrapper/installer/Main.class (Java 8 version)
  │   (same binary works on both versions)
  ├─ ModuleUtil path: Preferentially loads from META-INF/versions/9/
  │   io/.../util/ModuleUtil.class (Java 9+ advanced version)
  │   ├─ addModules() → complex module layer manipulation
  │   ├─ addExports() → module export configuration
  │   ├─ addOpens() → module open configuration
  │   └─ getPlatformClassLoader() → returns actual PlatformClassLoader
  └─ Execution proceeds with full module system support
```

### Manifest Configuration

**From `build.gradle` lines 50-51**:
```gradle
"Multi-Release": "true",
```

This manifest attribute signals to the JVM that the JAR contains multi-release content. Without this, the JVM ignores `META-INF/versions/9/` directory.

### Why Dual Module System Is Necessary

**Java 8** has no module system:
- No `java.lang.module` package
- No `sun.misc.Unsafe` reflective access patterns for modules
- Runtime module manipulation not possible or necessary
- Stub implementation sufficient

**Java 9+** requires module configuration:
- Minecraft uses JPMS
- Forge uses module system features
- Need to add modules at runtime
- Need to configure exports and opens
- Requires low-level JDK manipulation

**Forward Compatibility**:
- Java 8 code can run unchanged on Java 9+
- Java 9+ code cannot run on Java 8 (compilation fails)
- Multi-release JAR solves this elegantly

## ModuleUtil Implementation Details

### Java 8 Version (Simple)

Located in `src/main/java/...ModuleUtil.java` (40 lines total):

**Purpose**: Provide stubs that do nothing, allowing the same API to be called regardless of Java version.

**Methods**:

```java
public static void addModules(String modulePath) {
    // nothing to do with Java 8
}
```
No-op: Java 8 has no module system; modulePath argument is ignored.

```java
public static void setupClassPath(Path libraryDir, List<String> paths) throws Throwable {
    Method addURLMethod = URLClassLoader.class.getDeclaredMethod("addURL", URL.class);
    addURLMethod.setAccessible(true);
    for (String path : paths) {
        addURLMethod.invoke(ClassLoader.getSystemClassLoader(), 
            libraryDir.resolve(path).toUri().toURL());
    }
}
```
Functional: Uses reflection to access Java 8's URLClassLoader.addURL() to dynamically add JARs to the system classloader.

**Pattern**: All other methods follow same no-op structure for Java 8 compatibility.

### Java 9+ Version (Complex)

Located in `jigsaw/src/main/java/...ModuleUtil.java` (200+ lines):

**Prerequisite Knowledge**:
- Java module system basics: modules named, exports controlled
- JDK internals: MethodHandles, Unsafe, reflection APIs
- Module layer: boot layer is the root layer

**Key Insight** (line 32):
```java
private final static MethodHandles.Lookup IMPL_LOOKUP = getImplLookup();
```

The `IMPL_LOOKUP` is a special method handle lookup obtained via `sun.misc.Unsafe`. This provides access to internal JDK classes not normally accessible through reflection.

**The addModules() Method (lines 52-137)**: This is the most complex method. Step-by-step:

1. **Module Discovery** (lines 56-60):
```java
ModuleFinder finder = ModuleFinder.of(
    Stream.of(modulePath.split(File.pathSeparator))
        .map(Paths::get)
        .filter(p -> ...)  // exclude existing modules
        .toArray(Path[]::new)
);
```
Creates a ModuleFinder for the provided module path, excluding modules that already exist.

2. **Module Loading** (lines 61-63):
```java
MethodHandle loadModuleMH = IMPL_LOOKUP.findVirtual(
    Class.forName("jdk.internal.loader.BuiltinClassLoader"),
    "loadModule",
    MethodType.methodType(void.class, ModuleReference.class)
);
```
Obtains a method handle for `BuiltinClassLoader.loadModule()`, which is an internal JDK method.

3. **Resolution and Configuration** (lines 66-79):
```java
Configuration config = Configuration.resolveAndBind(
    finder, List.of(ModuleLayer.boot().configuration()), finder, roots
);
```
Creates a module configuration graph for the new modules, bound to the boot module layer.

4. **Graph Manipulation** (lines 81-118):
```java
HashMap<ResolvedModule, Set<ResolvedModule>> graphMap = ...
for (Map.Entry<ResolvedModule, Set<ResolvedModule>> entry : graphMap.entrySet()) {
    cfSetter.invokeWithArguments(entry.getKey(), ModuleLayer.boot().configuration());
    ...
}
```
Modifies the module dependency graph to integrate new modules into the boot layer's configuration.

5. **Module Definition** (lines 120-123):
Invokes internal `Module.defineModules()` to actually create the module objects in the boot layer.

6. **Read Edge Setup** (lines 131-137):
```java
implAddReadsMH.invokeWithArguments(module, bootModule);
```
Establishes "reads" relationships between new modules and existing boot modules, allowing them to see each other.

**The addExports() Method (lines 139-140)**:
```java
public static void addExports(List<String> exports) {
    TypeToAdd.EXPORTS.implAdd(exports);
}
```
Delegates to enum that uses method handles to call module export methods.

**The addOpens() Method (lines 143-144)**:
```java
public static void addOpens(List<String> opens) {
    TypeToAdd.OPENS.implAdd(opens);
}
```
Similar to addExports but for "opens" (deep reflection access).

**The parseModuleExtra() Method (lines 173-186)**:
Parses format: `<module>/<package>=<target>`

Example: `java.base/java.lang=ALL-UNNAMED`

This means: Module `java.base` should export/open package `java.lang` to all unnamed modules.

## Class Diagram

ASCII UML representation of full architecture:

```
┌─────────────────────────────────────────────────────────────────┐
│  <<entry-point>>                                                │
│  Main                                                            │
├─────────────────────────────────────────────────────────────────┤
│ - (no fields, stateless)                                        │
├─────────────────────────────────────────────────────────────────┤
│ + main(arguments: String[]): void (static)                      │
│ + isFile(path: Path): boolean (private static)                  │
└────────────────┬──────────────────────────────────────────────┬─┘
                 │ invokes                         creates       │
                 │                                              │
                 ▼                                              ▼
    ┌────────────────────────┐                    ┌──────────────────────────┐
    │ DetectorLoader         │                    │ URLClassLoader           │
    ├────────────────────────┤                    ├──────────────────────────┤
    │ - (no fields)          │                    │ - custom classloader     │
    ├────────────────────────┤                    ├──────────────────────────┤
    │ + loadDetector()       │────┐               │ loadClass(name)         │
    │   : IFileDetector      │    │               └──────────────────────────┘
    └────────────────────────┘    │
                 ▲                │  
    uses SPI     │         ┌──────▼─────────────────┐
                 │         │ <<interface>>          │
    ┌────────────┴───────────IFileDetector         │
    │            │         ├────────────────────────┤
    │            │         │ + name(): String       │
    │     returns │         │ + enabled(...):bool    │
    │            │         │ + getLibraryDir():Path │
    │            └────────→ │ + getInstallerJar(...):Path
    │                       │ + getMinecraftJar(...):Path
    │                       └────────────────────────┘
    │                              ▲
    │                              │ implements
    │                              │
    │                       ┌──────┴──────────────────────┐
    │                       │ MultiMCFileDetector         │
    │                       ├─────────────────────────────┤
    │                       │ - libraryDir: Path (cached) │
    │                       │ - installerJar: Path        │
    │                       │ - minecraftJar: Path        │
    │                       ├─────────────────────────────┤
    │                       │ + name()                    │
    │                       │ + enabled(...)              │
    │                       │ + getLibraryDir()           │
    │                       │ + getInstallerJar(...): Path
    │                       │ + getMinecraftJar(...)      │
    │                       └─────────────────────────────┘
    │
    └────────────────────────────────────────────────────────┘

         ┌─────────────────────────────────────────────────────┐
         │ Main calls Bootstrap.bootstrap(...)                 │
         └─────────────────────┬───────────────────────────────┘
                               │
                               ▼
                     ┌────────────────────────────┐
                     │ Bootstrap                  │
                     ├────────────────────────────┤
                     │ - (no fields)              │
                     ├────────────────────────────┤
                     │ + bootstrap(jvmArgs,...):  │
                     │   void (static throws)     │
                     └────┬───────────────────────┘
                          │ invokes
                          ▼
            ┌─────────────────────────────────┐
            │ ModuleUtil                      │
            ├─────────────────────────────────┤
            │ - (version-specific)            │
            ├─────────────────────────────────┤
            │ + addModules(path): void        │
            │ + addExports(list): void        │
            │ + addOpens(list): void          │
            │ + setupClassPath(...): void     │
            │ + setupBootstrapLauncher(...):  │
            │   Class<?>                      │
            │ + getPlatformClassLoader():     │
            │   ClassLoader                   │
            └─────────────────────────────────┘
         (Java 8: all no-op)
         (Java 9+: advanced module manipulation)

         ┌──────────────────────────────────────────┐
         │ URLClassLoader loads Installer via       │
         │ reflection                               │
         └──────────────────────────┬───────────────┘
                                    │
                                    ▼
                        ┌────────────────────────────────────┐
                        │ Installer                          │
                        ├────────────────────────────────────┤
                        │- wrapper: InstallV1Wrapper (static)│
                        ├────────────────────────────────────┤
                        │+ getData(File): Map                │
                        │+ install(File,File,File): boolean  │
                        │- getWrapper(File):InstallV1Wrapper │
                        └────────────┬─────────────────────┬─┘
                                     │                     │
                    ┌────────────────┘                     └────────────┐
                    │                                                   │
                    ▼                                                   ▼
         ┌────────────────────────────────┐       ┌──────────────────────────────┐
         │ InstallV1Wrapper               │       │ Version0                     │
         │ extends InstallV1              │       │ extends Version              │
         ├────────────────────────────────┤       ├──────────────────────────────┤
         │- processors: Map<...>          │       │- mainClass: String           │
         │- librariesDir: File            │       │- arguments: Arguments        │
         ├────────────────────────────────┤       ├──────────────────────────────┤
         │+ getProcessors(side): List     │       │+ getMainClass(): String      │
         │- checkProcessorFiles(...)      │       │+ getArguments(): Arguments   │
         │- setOutputs(...)               │       │+ loadVersion(...):Version0 │
         └────────────────────────────────┘       └──────────────────────────────┘
                                                           │
                                                           │ contains
                                                           ▼
                                                  ┌──────────────────────┐
                                                  │ Arguments            │
                                                  ├──────────────────────┤
                                                  │- jvm: String[]       │
                                                  ├──────────────────────┤
                                                  │+ getJvm(): String[]  │
                                                  └──────────────────────┘
```

## Reflection-Based Architecture

ForgeWrapper uses reflection extensively to provide a flexible, decoupled architecture. Here are key reflection patterns:

### Pattern 1: Dynamic Class Loading (Main.java lines 42-43)

```java
Class<?> installer = ucl.loadClass("io.github.zekerzhayard.forgewrapper.installer.Installer");
```

**Why**: Avoids compile-time dependency on installer JAR. The Installer class is only loaded from the custom classloader that includes the installer JAR.

**Benefits**:
- ForgeWrapper source doesn't need Installer on buildpath
- Different Installer versions can be used without rebuilding ForgeWrapper
- Decouples ForgeWrapper versioning from Forge versioning

### Pattern 2: Method Invocation via Reflection (Main.java lines 43-44)

```java
Map<String, Object> data = (Map<String, Object>) installer
    .getMethod("getData", File.class)
    .invoke(null, detector.getLibraryDir().toFile());
```

**Why**: Main.java doesn't have the Installer class available at compile time.

**Mechanics**:
- `getMethod("getData", File.class)` finds method that takes File parameter
- `.invoke(null, ...)` invokes it (null = static method, no instance needed)
- Result cast to Map<String, Object>

### Pattern 3: Unsafe Field Access (jigsaw ModuleUtil.java lines 33-48)

```java
Field unsafeField = Unsafe.class.getDeclaredField("theUnsafe");
unsafeField.setAccessible(true);
Unsafe unsafe = (Unsafe) unsafeField.get(null);

Field implLookupField = MethodHandles.Lookup.class.getDeclaredField("IMPL_LOOKUP");
return (MethodHandles.Lookup) unsafe.getObject(
    unsafe.staticFieldBase(implLookupField),
    unsafe.staticFieldOffset(implLookupField)
);
```

**Why**: Need access to internal `IMPL_LOOKUP` field that's normally inaccessible.

**What It Does**: Uses Unsafe to reflectively read a private static field from MethodHandles.Lookup, obtaining the internal "implementation lookup" that has special JDK permissions.

**Risk**: Relies on internal JDK implementation; may break in future Java versions.

### Pattern 4: Method Handle Creation (TypeToAdd enum constructors)

```java
this.implAddMH = IMPL_LOOKUP.findVirtual(Module.class, "implAddExports", 
    MethodType.methodType(void.class, String.class, Module.class));
```

**Why**: Method handles provide high-performance reflection; used for repeated invocations.

**Benefit**: Once created, method handles are cached. Repeated calls are very fast.

### Pattern 5: Field Reflection for Modification (Installer.java lines 185-192)

```java
private static Field outputsField;
private static void setOutputs(Processor processor, Map<String, String> outputs) {
    try {
        if (outputsField == null) {
            outputsField = Processor.class.getDeclaredField("outputs");
            outputsField.setAccessible(true);
        }
        outputsField.set(processor, outputs);
    } catch (Throwable t) {
        throw new RuntimeException(t);
    }
}
```

**Why**: Need to modify private field `outputs` on Processor objects.

**Pattern**: Lazy-initialize field reference, cache it, reuse for all modifications.

This comprehensive architectural documentation provides deep understanding of ForgeWrapper's design, pattern usage, and implementation strategy.
```

