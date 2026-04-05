# ForgeWrapper — Building & Gradle Build System Reference

This document provides a comprehensive, line-by-line analysis of the ForgeWrapper
build system. Every statement references actual source code from the Gradle build
files. Variable names, values, and configuration blocks are taken directly from
the project.

---

## Table of Contents

1.  [Project Overview](#1-project-overview)
2.  [Multi-Project Structure](#2-multi-project-structure)
3.  [settings.gradle Analysis](#3-settingsgradle-analysis)
4.  [gradle.properties Analysis](#4-gradleproperties-analysis)
5.  [Gradle Wrapper Configuration](#5-gradle-wrapper-configuration)
6.  [Root build.gradle — Complete Line-by-Line Analysis](#6-root-buildgradle--complete-line-by-line-analysis)
    - 6.1 [Imports](#61-imports)
    - 6.2 [Plugins Block](#62-plugins-block)
    - 6.3 [Java Source/Target Compatibility](#63-java-sourcetarget-compatibility)
    - 6.4 [Version, Group, and Archives Base Name](#64-version-group-and-archives-base-name)
    - 6.5 [The multirelase Configuration](#65-the-multirelase-configuration)
    - 6.6 [Repositories](#66-repositories)
    - 6.7 [Dependencies](#67-dependencies)
    - 6.8 [Sources JAR](#68-sources-jar)
    - 6.9 [JAR Manifest Attributes](#69-jar-manifest-attributes)
    - 6.10 [Multi-Release JAR Packing](#610-multi-release-jar-packing)
    - 6.11 [Publishing Configuration](#611-publishing-configuration)
    - 6.12 [getVersionSuffix()](#612-getversionsuffix)
7.  [Jigsaw Subproject build.gradle — Complete Analysis](#7-jigsaw-subproject-buildgradle--complete-analysis)
    - 7.1 [Plugins](#71-plugins)
    - 7.2 [Java 9 Toolchain Auto-Detection](#72-java-9-toolchain-auto-detection)
    - 7.3 [JVM Version Attribute Override](#73-jvm-version-attribute-override)
8.  [Multi-Release JAR — Deep Dive](#8-multi-release-jar--deep-dive)
9.  [Build Pipeline Diagram](#9-build-pipeline-diagram)
10. [Build Targets and Tasks](#10-build-targets-and-tasks)
11. [Step-by-Step Build Guide](#11-step-by-step-build-guide)
12. [CI/CD Integration](#12-cicd-integration)
13. [Artifact Output Structure](#13-artifact-output-structure)
14. [Publishing to Local Maven Repository](#14-publishing-to-local-maven-repository)
15. [Java Version Requirements](#15-java-version-requirements)
16. [Source File Layout and the Dual-ModuleUtil Pattern](#16-source-file-layout-and-the-dual-moduleutil-pattern)
17. [Troubleshooting](#17-troubleshooting)
18. [Quick Reference Card](#18-quick-reference-card)

---

## 1. Project Overview

ForgeWrapper is a Java library that allows third-party Minecraft launchers
(originally MultiMC, now adopted more broadly) to launch Minecraft 1.13+ with
Forge and NeoForge. The build system produces a **Multi-Release JAR** (MRJAR)
that contains:

- **Java 8 bytecode** in the standard class path for maximum compatibility.
- **Java 9+ bytecode** under `META-INF/versions/9/` for environments running on
  the Java Platform Module System (JPMS / Project Jigsaw).

The Gradle build is a **multi-project build** consisting of:

| Project    | Directory         | Java Target | Purpose                         |
|------------|-------------------|-------------|---------------------------------|
| Root       | `forgewrapper/`   | Java 8      | Main ForgeWrapper library       |
| `jigsaw`   | `forgewrapper/jigsaw/` | Java 9 | JPMS-aware `ModuleUtil` overlay |

The root project's JAR task assembles both outputs into a single artifact with
`Multi-Release: true` in its manifest.

---

## 2. Multi-Project Structure

The ForgeWrapper build is organized as a Gradle multi-project build. The
directory tree relevant to the build system is:

```
forgewrapper/
├── build.gradle                          ← Root project build script
├── settings.gradle                       ← Declares root name + subprojects
├── gradle.properties                     ← Project-wide properties
├── gradlew                               ← Gradle wrapper (Linux/macOS)
├── gradlew.bat                           ← Gradle wrapper (Windows)
├── gradle/
│   └── wrapper/
│       └── gradle-wrapper.properties     ← Wrapper distribution URL + version
├── src/
│   └── main/
│       └── java/
│           └── io/github/zekerzhayard/forgewrapper/installer/
│               ├── Bootstrap.java
│               ├── Installer.java
│               ├── Main.java
│               ├── detector/
│               │   ├── DetectorLoader.java
│               │   ├── IFileDetector.java
│               │   └── MultiMCFileDetector.java
│               └── util/
│                   └── ModuleUtil.java   ← Java 8 stub (no-op methods)
├── jigsaw/
│   ├── build.gradle                      ← Subproject build script
│   └── src/
│       └── main/
│           └── java/
│               └── io/github/zekerzhayard/forgewrapper/installer/
│                   └── util/
│                       └── ModuleUtil.java   ← Java 9 full implementation
└── build/                                ← Generated outputs (after build)
    ├── libs/
    │   ├── ForgeWrapper-<version>.jar
    │   └── ForgeWrapper-<version>-sources.jar
    └── maven/                            ← Local maven publish target
```

The root project compiles all code under `src/` with Java 8. The `jigsaw`
subproject compiles its own `ModuleUtil.java` with Java 9. At JAR assembly time,
the jigsaw output is injected into `META-INF/versions/9/` inside the root JAR,
creating the Multi-Release JAR.

---

## 3. settings.gradle Analysis

**File: `forgewrapper/settings.gradle`** (2 lines of active content)

```groovy
rootProject.name = 'ForgeWrapper'

include 'jigsaw'
```

### Line-by-line:

**`rootProject.name = 'ForgeWrapper'`**

Sets the human-readable name of the root Gradle project to `ForgeWrapper`. This
name is used as:

- The default `archivesBaseName` (overridden in `build.gradle` to
  `rootProject.name`, which resolves to the same value `ForgeWrapper`).
- The `Specification-Title` and `Implementation-Title` in the JAR manifest.
- The base filename for produced artifacts: `ForgeWrapper-<version>.jar`.

**`include 'jigsaw'`**

Includes the subdirectory `jigsaw/` as a Gradle subproject. Gradle will look for
`jigsaw/build.gradle` and compile it as a separate project within the same build.
This subproject is referenced in the root `build.gradle` via
`project(":jigsaw")` in the `multirelase` dependency declaration.

The relationship between settings.gradle and the two build.gradle files:

```
settings.gradle
    │
    ├── rootProject.name = 'ForgeWrapper'
    │       ↓
    │   build.gradle  (root)
    │       archivesBaseName = rootProject.name  →  "ForgeWrapper"
    │       dependencies { multirelase project(":jigsaw") }
    │                                      │
    └── include 'jigsaw'                   │
            ↓                              ↓
        jigsaw/build.gradle  ←─────── resolved here
```

---

## 4. gradle.properties Analysis

**File: `forgewrapper/gradle.properties`** (2 active properties)

```properties
org.gradle.daemon = false

fw_version = projt
```

### `org.gradle.daemon = false`

Disables the Gradle daemon for this project. Normally Gradle keeps a long-lived
JVM process (the daemon) running between builds to speed up subsequent
invocations. Setting this to `false` means every `./gradlew` invocation starts a
fresh JVM. This is typical for:

- **CI/CD environments** where daemon state can cause flaky builds.
- **Projects with infrequent builds** where the daemon would just consume memory.
- **Reproducibility** — no cached daemon state between runs.

### `fw_version = projt`

Defines the base version string for ForgeWrapper as the literal value `projt`.
This property is referenced in `build.gradle` via string interpolation:

```groovy
version = "${fw_version}${-> getVersionSuffix()}"
```

The `${fw_version}` is replaced with `projt` from `gradle.properties`, and then
the lazy GString closure `${-> getVersionSuffix()}` appends a suffix. The final
version string will be one of:

| Environment                          | Resulting Version              |
|--------------------------------------|-------------------------------|
| Local development                    | `projt-LOCAL`                 |
| GitHub Actions CI                    | `projt-2026-04-05`           |
| IS_PUBLICATION env var set           | `projt-2026-04-05`           |

The `fw_version` property is available to all projects in the multi-project
build because `gradle.properties` in the root directory is automatically loaded
for all subprojects.

---

## 5. Gradle Wrapper Configuration

**File: `forgewrapper/gradle/wrapper/gradle-wrapper.properties`**

```properties
distributionBase=GRADLE_USER_HOME
distributionPath=wrapper/dists
distributionUrl=https\://services.gradle.org/distributions/gradle-7.3.3-all.zip
zipStoreBase=GRADLE_USER_HOME
zipStorePath=wrapper/dists
```

### Property-by-property:

**`distributionBase=GRADLE_USER_HOME`**

The base directory where the downloaded Gradle distribution will be stored.
`GRADLE_USER_HOME` defaults to `~/.gradle` on Linux/macOS and
`%USERPROFILE%\.gradle` on Windows.

**`distributionPath=wrapper/dists`**

Relative path under `distributionBase` where distributions are unpacked. The
full absolute path becomes `~/.gradle/wrapper/dists/`.

**`distributionUrl=https\://services.gradle.org/distributions/gradle-7.3.3-all.zip`**

The URL from which Gradle **7.3.3** is downloaded. Key observations:

- **Version 7.3.3** — Released December 2021. This is a Gradle 7.x release that
  supports Java toolchains (used in the jigsaw subproject) and is compatible
  with both Java 8 and Java 17+.
- **`-all` variant** — Includes Gradle source code and documentation, not just
  the binaries (`-bin`). This allows IDEs to show Gradle DSL documentation and
  enables source-level debugging of Gradle itself.
- The `\:` escape is standard `.properties` file syntax for a literal colon.

**`zipStoreBase=GRADLE_USER_HOME`** and **`zipStorePath=wrapper/dists`**

Where the downloaded ZIP file itself is cached before extraction. Same base
directory as the unpacked distribution.

### Gradle Version Compatibility Matrix

| Feature Used in Build          | Minimum Gradle Version  | Actual |
|--------------------------------|------------------------|--------|
| `java` plugin                  | 1.0                    | 7.3.3  |
| `maven-publish` plugin         | 1.3                    | 7.3.3  |
| `javaToolchains.compilerFor`   | 6.7                    | 7.3.3  |
| `layout.buildDirectory.dir()`  | 7.1                    | 7.3.3  |
| `TargetJvmVersion` attribute   | 6.0                    | 7.3.3  |

All features used in the build are supported by Gradle 7.3.3.

---

## 6. Root build.gradle — Complete Line-by-Line Analysis

**File: `forgewrapper/build.gradle`** (90 lines)

This is the primary build script. We will examine every section.

### 6.1 Imports

```groovy
import java.text.SimpleDateFormat
```

Line 1 imports `SimpleDateFormat`, used in the `getVersionSuffix()` method at
line 89 to format the current date as `-yyyy-MM-dd` (e.g., `-2026-04-05`).
This import is at the top of the script, outside any block, making it available
to all closures in the file.

### 6.2 Plugins Block

```groovy
plugins {
    id "java"
    id "eclipse"
    id "maven-publish"
}
```

Lines 3–7: Three plugins are applied.

**`java`** — The core Java compilation plugin. It provides:
- `compileJava` task — compiles `src/main/java/**/*.java`.
- `jar` task — packages compiled classes into a JAR.
- `sourceSets` — the `main` and `test` source sets.
- `sourceCompatibility` / `targetCompatibility` — Java version settings.
- `configurations` — `implementation`, `compileOnly`, `runtimeOnly`, etc.
- The `components.java` software component used by `maven-publish`.

**`eclipse`** — Generates Eclipse IDE project files (`.project`, `.classpath`,
`.settings/`). Allows developers to run `./gradlew eclipse` to import into
Eclipse. This does not affect the build output.

**`maven-publish`** — Enables publishing artifacts to Maven repositories. It
provides:
- The `publishing { }` DSL block.
- The `publish` task (and per-repository `publishToMavenLocal`, etc.).
- `MavenPublication` type for defining what gets published.

### 6.3 Java Source/Target Compatibility

```groovy
sourceCompatibility = targetCompatibility = 1.8
compileJava {
    sourceCompatibility = targetCompatibility = 1.8
}
```

Lines 9–12: Java 8 compatibility is set **twice** — at the project level and
inside the `compileJava` task configuration. This double declaration ensures the
setting is applied regardless of evaluation order.

- **`sourceCompatibility = 1.8`** — The Java source code must be valid Java 8
  syntax. The compiler will reject Java 9+ language features (e.g., `var`,
  private interface methods).
- **`targetCompatibility = 1.8`** — The emitted bytecode targets Java 8 (class
  file version 52.0). JVMs older than Java 8 will refuse to load these classes.

This is critical because ForgeWrapper must run on Minecraft launchers that may
still use Java 8. The `ModuleUtil.java` in the root source tree contains no-op
stubs precisely because Java 8 has no `java.lang.module` API:

```java
// forgewrapper/src/main/java/.../util/ModuleUtil.java
public class ModuleUtil {
    public static void addModules(String modulePath) {
        // nothing to do with Java 8
    }
    public static void addExports(List<String> exports) {
        // nothing to do with Java 8
    }
    public static void addOpens(List<String> opens) {
        // nothing to do with Java 8
    }
    // ...
}
```

### 6.4 Version, Group, and Archives Base Name

```groovy
version = "${fw_version}${-> getVersionSuffix()}"
group = "io.github.zekerzhayard"
archivesBaseName = rootProject.name
```

Lines 14–16: Maven coordinates and artifact naming.

**`version`** — A Groovy GString with a lazy evaluation closure. The `fw_version`
from `gradle.properties` is resolved immediately to `projt`. The closure
`${-> getVersionSuffix()}` is evaluated lazily — only when the GString is
converted to a String (at task execution time, not configuration time). This
matters because `getVersionSuffix()` calls `System.getenv()` and `new Date()`,
which should reflect the actual build time, not the configuration phase time.

**`group = "io.github.zekerzhayard"`** — The Maven `groupId`. Follows the
reverse-domain convention for the project's GitHub namespace
(`github.com/ZekerZhayard`).

**`archivesBaseName = rootProject.name`** — Set to `ForgeWrapper` (from
`settings.gradle`). This is the filename prefix for all produced JARs:
`ForgeWrapper-projt-LOCAL.jar`.

### 6.5 The multirelase Configuration

```groovy
configurations {
    multirelase {
        implementation.extendsFrom multirelase
    }
}
```

Lines 18–22: This block creates a **custom Gradle configuration** named
`multirelase` (note: this is intentionally or accidentally spelled without the
second "e" — it is not `multirelease`).

The statement `implementation.extendsFrom multirelase` establishes a dependency
inheritance chain: everything in the `multirelase` configuration is also visible
to the `implementation` configuration. This means:

1. When `multirelase project(":jigsaw")` is declared (line 38), the jigsaw
   subproject's classes are available on the root project's compile classpath.
2. The `multirelase` configuration is also used separately in the `jar` block
   (line 60) to extract the jigsaw classes and pack them into
   `META-INF/versions/9/`.

The configuration flow:

```
multirelase config
    │
    ├──→ implementation config (via extendsFrom)
    │       → compileJava can see jigsaw classes
    │
    └──→ jar task (via configurations.multirelase.files)
            → copies jigsaw classes into META-INF/versions/9/
```

This dual use is the heart of the Multi-Release JAR mechanism. The root source
code can reference `ModuleUtil` (which it does — in `Bootstrap.java` and
`Main.java`), and at compile time Gradle resolves the Java 8 stubs from the root
`src/`. At runtime, the JVM selects the appropriate `ModuleUtil` class: the Java
8 stub from the main class path on Java 8 JVMs, or the Java 9 implementation
from `META-INF/versions/9/` on Java 9+ JVMs.

### 6.6 Repositories

```groovy
repositories {
    mavenCentral()
    maven {
        name = "forge"
        url = "https://maven.minecraftforge.net/"
    }
}
```

Lines 24–29: Two Maven repositories are declared.

**`mavenCentral()`** — Maven Central Repository (`https://repo.maven.apache.org/maven2/`).
Used to resolve:
- `com.google.code.gson:gson:2.8.7`
- `net.sf.jopt-simple:jopt-simple:5.0.4`

**Forge Maven** (`https://maven.minecraftforge.net/`) — MinecraftForge's official
Maven repository. Named `"forge"` for logging clarity. Used to resolve:
- `cpw.mods:modlauncher:8.0.9`
- `net.minecraftforge:installer:2.2.7`

Note: The repository named `"forge"` is a Gradle naming convention for
readability; the name does not affect dependency resolution behavior.

### 6.7 Dependencies

```groovy
dependencies {
    compileOnly "com.google.code.gson:gson:2.8.7"
    compileOnly "cpw.mods:modlauncher:8.0.9"
    compileOnly "net.minecraftforge:installer:2.2.7"
    compileOnly "net.sf.jopt-simple:jopt-simple:5.0.4"

    multirelase project(":jigsaw")
}
```

Lines 31–38: All external dependencies use the `compileOnly` scope.

#### Why `compileOnly`?

`compileOnly` means the dependency is available at compile time but is **not**
included in the runtime classpath or the published POM. ForgeWrapper is loaded
into an environment (the Minecraft launcher) that already provides these
libraries. Bundling them would cause version conflicts and inflate the JAR.

#### Dependency Analysis

**`com.google.code.gson:gson:2.8.7`**

- **What**: Google's JSON serialization/deserialization library.
- **Why compileOnly**: The Minecraft launcher and Forge installer both bundle
  Gson. Including it again would create version conflicts.
- **Classes used**: `com.google.gson.Gson`, `com.google.gson.JsonObject`, etc.
  Used in `Installer.java` to parse the Forge installer's JSON metadata.

**`cpw.mods:modlauncher:8.0.9`**

- **What**: The Forge ModLauncher framework by cpw (ChickenPatches Warrior).
  Manages mod loading, class transformation, and game launch orchestration.
- **Why compileOnly**: ModLauncher is part of the Forge runtime. ForgeWrapper
  interacts with its API to set up the launch environment but doesn't ship it.
- **Classes used**: Launch target interfaces and transformation services.
- **Version 8.0.9**: Targets Forge for Minecraft 1.16.x–1.17.x era.

**`net.minecraftforge:installer:2.2.7`**

- **What**: The Forge installer library. Contains code for downloading,
  extracting, and setting up Forge libraries.
- **Why compileOnly**: ForgeWrapper loads the installer JAR at runtime via a
  `URLClassLoader` (see `Main.java` lines 48–50). It does not bundle it.
- **Classes used**: Invoked reflectively — `installer.getMethod("getData", ...)`
  and `installer.getMethod("install", ...)` in `Main.java`.

**`net.sf.jopt-simple:jopt-simple:5.0.4`**

- **What**: A command-line option parser for Java.
- **Why compileOnly**: Provided by the launcher environment (Forge uses it for
  argument parsing). ForgeWrapper reads parsed arguments but doesn't need its
  own copy.
- **Classes used**: `OptionParser`, `OptionSet` — for parsing launch arguments
  like `--fml.mcVersion`, `--fml.forgeVersion`, `--fml.neoForgeVersion`.

**`multirelase project(":jigsaw")`**

- **What**: A project dependency on the `jigsaw` subproject.
- **Scope**: The custom `multirelase` configuration (not `compileOnly` or
  `implementation` directly). Since `implementation.extendsFrom multirelase`,
  the jigsaw classes are on the compile classpath.
- **Purpose**: The jigsaw project produces a JAR with Java 9 bytecode. This JAR
  is consumed by the root project's `jar` task and unpacked into
  `META-INF/versions/9/`.

### 6.8 Sources JAR

```groovy
java {
    withSourcesJar()
}
```

Lines 40–42: Registers a `sourcesJar` task that produces
`ForgeWrapper-<version>-sources.jar` containing all `.java` files from
`src/main/java`. This artifact is included in Maven publications alongside the
main JAR, enabling downstream users and IDEs to access source code for debugging.

### 6.9 JAR Manifest Attributes

```groovy
jar {
    manifest.attributes([
        "Specification-Title": "${project.name}",
        "Specification-Vendor": "ZekerZhayard",
        "Specification-Version": "${project.version}".split("-")[0],
        "Implementation-Title": "${project.name}",
        "Implementation-Version": "${project.version}",
        "Implementation-Vendor" :"ZekerZhayard",
        "Implementation-Timestamp": new Date().format("yyyy-MM-dd'T'HH:mm:ssZ"),
        "Automatic-Module-Name": "${project.group}.${project.archivesBaseName}".toString().toLowerCase(),
        "Multi-Release": "true",
        "GitCommit": String.valueOf(System.getenv("GITHUB_SHA"))
    ])
```

Lines 44–56: The `jar` task configures `META-INF/MANIFEST.MF` with these
attributes. Each one explained:

**`Specification-Title: ForgeWrapper`**

Identifies the specification this JAR implements. Set to `project.name` which
resolves to `ForgeWrapper`. Part of the JAR Specification versioning convention
defined in `java.lang.Package`.

**`Specification-Vendor: ZekerZhayard`**

The organization or individual that maintains the specification. Hardcoded to
the project author's GitHub handle.

**`Specification-Version: projt`**

The spec version. Note the expression `"${project.version}".split("-")[0]` —
this takes the full version string (e.g., `projt-LOCAL` or `projt-2026-04-05`)
and splits on `-`, taking only the first element: `projt`. This gives a stable
specification version irrespective of the build suffix.

**`Implementation-Title: ForgeWrapper`**

Same as `Specification-Title`. Identifies this particular implementation.

**`Implementation-Version: projt-LOCAL`** (or `projt-2026-04-05`)

The full version string including the suffix. Unlike `Specification-Version`,
this captures the exact build variant.

**`Implementation-Vendor: ZekerZhayard`**

The vendor of this implementation. Same as `Specification-Vendor`.

**`Implementation-Timestamp: 2026-04-05T14:30:00+0000`** (example)

The build timestamp in ISO 8601 format. Generated by `new Date().format(...)`.
The format pattern `yyyy-MM-dd'T'HH:mm:ssZ` produces:
- `yyyy` — 4-digit year
- `MM` — 2-digit month
- `dd` — 2-digit day
- `'T'` — literal T separator
- `HH:mm:ss` — 24-hour time
- `Z` — timezone offset (e.g., `+0000`)

**`Automatic-Module-Name: io.github.zekerzhayard.forgewrapper`**

The JPMS module name for this JAR when it is placed on the module path. The
expression `"${project.group}.${project.archivesBaseName}".toString().toLowerCase()`
concatenates `io.github.zekerzhayard` + `.` + `ForgeWrapper`, converts to
lowercase: `io.github.zekerzhayard.forgewrapper`. This allows other JPMS modules
to `requires io.github.zekerzhayard.forgewrapper;`.

**`Multi-Release: true`**

The critical attribute. Tells the JVM (Java 9+) that this JAR contains
version-specific class files under `META-INF/versions/<N>/`. Without this
attribute, the JVM ignores the `META-INF/versions/` directory entirely. Defined
in JEP 238 (Multi-Release JAR Files).

**`GitCommit: <sha>`** (or `null`)

Captures the Git commit SHA from the `GITHUB_SHA` environment variable.
`String.valueOf(System.getenv("GITHUB_SHA"))` returns:
- The 40-character SHA hex string when built in GitHub Actions.
- The string `"null"` when built locally (since `getenv()` returns Java `null`,
  and `String.valueOf(null)` returns the string `"null"`).

This attribute enables tracing a built artifact back to its exact source commit.

### 6.10 Multi-Release JAR Packing

```groovy
    into "META-INF/versions/9", {
        from configurations.multirelase.files.collect {
            zipTree(it)
        }
        exclude "META-INF/**"
    }
}
```

Lines 58–63: This block within the `jar { }` closure is the mechanism that
creates the Multi-Release JAR. Detailed breakdown:

**`into "META-INF/versions/9"`** — All files produced by this `from` block are
placed inside the `META-INF/versions/9/` directory within the JAR. This is the
Java 9 version overlay directory per JEP 238.

**`configurations.multirelase.files`** — Resolves the `multirelase` configuration,
which contains `project(":jigsaw")`. This evaluates to the jigsaw subproject's
JAR file (e.g., `jigsaw/build/libs/jigsaw.jar`).

**`.collect { zipTree(it) }`** — For each file in the resolved configuration
(there's exactly one: the jigsaw JAR), `zipTree()` treats it as a ZIP archive
and returns a file tree of its contents. This effectively "explodes" the jigsaw
JAR.

**`exclude "META-INF/**"`** — Excludes the jigsaw JAR's own `META-INF/`
directory (which contains its own `MANIFEST.MF`). Only actual class files are
copied. This prevents nested/conflicting manifests.

The net result: the jigsaw subproject's compiled class files (specifically
`io/github/zekerzhayard/forgewrapper/installer/util/ModuleUtil.class`) are
placed at:

```
META-INF/versions/9/io/github/zekerzhayard/forgewrapper/installer/util/ModuleUtil.class
```

When the JVM is Java 9+, it will load this class **instead of** the Java 8 stub
at:

```
io/github/zekerzhayard/forgewrapper/installer/util/ModuleUtil.class
```

### 6.11 Publishing Configuration

```groovy
publishing {
    publications {
        maven(MavenPublication) {
            groupId "${project.group}"
            artifactId "${project.archivesBaseName}"
            version "${project.version}"

            from components.java
        }
    }
    repositories {
        maven {
            url = layout.buildDirectory.dir("maven")
        }
    }
}
tasks.publish.dependsOn build
```

Lines 65–82: Maven publishing setup.

**Publication: `maven(MavenPublication)`**

Defines a Maven publication with coordinates:
- `groupId` = `io.github.zekerzhayard`
- `artifactId` = `ForgeWrapper`
- `version` = `projt-LOCAL` (or dated variant)

**`from components.java`** — Publishes the Java component, which includes:
- The main JAR (`ForgeWrapper-<version>.jar`)
- The sources JAR (`ForgeWrapper-<version>-sources.jar`)
- A generated POM file with dependency metadata

**Repository target:**

```groovy
url = layout.buildDirectory.dir("maven")
```

Publishes to a local directory inside the build output:
`forgewrapper/build/maven/`. This is **not** Maven Central or any remote
repository. The published artifacts end up at:

```
build/maven/io/github/zekerzhayard/ForgeWrapper/<version>/
├── ForgeWrapper-<version>.jar
├── ForgeWrapper-<version>-sources.jar
├── ForgeWrapper-<version>.pom
├── ForgeWrapper-<version>.module
└── (checksums: .md5, .sha1, .sha256, .sha512)
```

**`tasks.publish.dependsOn build`** — Ensures the project is fully built before
publishing. Without this, Gradle could attempt to publish before the JAR is
assembled.

### 6.12 getVersionSuffix()

```groovy
static String getVersionSuffix() {
    if (System.getenv("IS_PUBLICATION") != null || System.getenv("GITHUB_ACTIONS") == "true")
        return new SimpleDateFormat("-yyyy-MM-dd").format(new Date())

    return "-LOCAL"
}
```

Lines 84–89: A static method that determines the version suffix.

**Logic:**

1. If the environment variable `IS_PUBLICATION` is set (to any value, including
   empty string — `!= null` is the check), append a date suffix.
2. **OR** if the environment variable `GITHUB_ACTIONS` equals the string
   `"true"` (which GitHub Actions always sets), append a date suffix.
3. Otherwise, append `-LOCAL`.

**Important Groovy/Java note:** The `==` comparison with `System.getenv()` uses
Java `String.equals()` semantics in Groovy when comparing strings. However,
there is a subtle behavior: `System.getenv("GITHUB_ACTIONS")` returns `null`
when not in GitHub Actions, and `null == "true"` evaluates to `false` in Groovy
(no NPE), which is the desired behavior.

**Date format:** `SimpleDateFormat("-yyyy-MM-dd")` produces strings like
`-2026-04-05`. The leading `-` is part of the pattern, so the result
includes the hyphen separator.

**Version assembly flow:**

```
gradle.properties          getVersionSuffix()          Final version
─────────────────          ──────────────────          ─────────────
fw_version = projt    +    "-LOCAL"              →    "projt-LOCAL"
fw_version = projt    +    "-2026-04-05"         →    "projt-2026-04-05"
```

---

## 7. Jigsaw Subproject build.gradle — Complete Analysis

**File: `forgewrapper/jigsaw/build.gradle`** (27 lines)

This subproject compiles the Java 9+ version of `ModuleUtil.java`.

### 7.1 Plugins

```groovy
plugins {
    id "java"
    id "eclipse"
}
```

Lines 1–4: Only `java` and `eclipse`. No `maven-publish` — the jigsaw subproject
is never published independently. Its output is consumed exclusively by the root
project's `jar` task via the `multirelase` configuration.

### 7.2 Java 9 Toolchain Auto-Detection

```groovy
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

Lines 6–14: The compilation block with intelligent Java version detection.

**The `if` check:** `JavaVersion.current()` returns the JVM version running
Gradle itself. If Gradle is running on Java 8 (which is `< VERSION_1_9`), then
the `javaCompiler` is overridden using Gradle's **Java Toolchain** feature.

**`javaToolchains.compilerFor { languageVersion = JavaLanguageVersion.of(9) }`**

This tells Gradle: "Find a Java 9 compiler on this system and use it for this
compilation task." Gradle will search:
1. JDK installations registered in the toolchain registry.
2. Standard JDK installation directories (`/usr/lib/jvm/`, etc.).
3. JDKs managed by tools like SDKMAN!, jabba, or Gradle's auto-provisioning.

If no Java 9+ JDK is found, the build fails with an error.

**If Gradle is already running on Java 9+**, the `if` block is skipped entirely.
The current JVM's compiler is used, with `sourceCompatibility = 9` and
`targetCompatibility = 9` ensuring Java 9 bytecode is produced.

**`sourceCompatibility = 9`** — Accept Java 9 language features (modules,
private interface methods, etc.). The jigsaw `ModuleUtil.java` uses:
- `java.lang.module.Configuration`
- `java.lang.module.ModuleFinder`
- `java.lang.module.ModuleReference`
- `java.lang.module.ResolvedModule`
- `ModuleLayer.boot()`
- `List.of()`
- `ClassLoader.getPlatformClassLoader()`

These APIs do not exist in Java 8, which is why this code cannot be in the root
project.

**`targetCompatibility = 9`** — Emit class file version 53.0 (Java 9). The JVM
only loads classes from `META-INF/versions/9/` if they are version 53.0 or
higher.

### 7.3 JVM Version Attribute Override

```groovy
configurations {
    apiElements {
        attributes {
            attribute TargetJvmVersion.TARGET_JVM_VERSION_ATTRIBUTE, 8
        }
    }
    runtimeElements {
        attributes {
            attribute TargetJvmVersion.TARGET_JVM_VERSION_ATTRIBUTE, 8
        }
    }
}
```

Lines 16–27: This is a subtle but critical configuration.

**Problem:** The jigsaw subproject compiles with `targetCompatibility = 9`, so
Gradle automatically sets the `TargetJvmVersion` attribute on its
`apiElements` and `runtimeElements` configurations to `9`. When the root project
(which targets Java 8) depends on `project(":jigsaw")`, Gradle's dependency
resolution would detect a JVM version mismatch and fail:

```
> Could not resolve project :jigsaw.
  > Incompatible because this component declares a component compatible
    with Java 9 and the consumer needed a component compatible with Java 8
```

**Solution:** Override the `TARGET_JVM_VERSION_ATTRIBUTE` to `8` on both
outgoing configurations (`apiElements` and `runtimeElements`). This tells
Gradle: "Even though this project compiles with Java 9, treat its output as
Java 8-compatible for dependency resolution purposes."

This is safe because:
1. The jigsaw JAR is **never loaded directly** on Java 8. It is packed into
   `META-INF/versions/9/` and only loaded by Java 9+ JVMs.
2. The attribute override only affects Gradle's dependency resolution metadata,
   not the actual bytecode version.

**`apiElements`** — The configuration used when another project declares a
`compileOnly` or `api` dependency on this project.

**`runtimeElements`** — The configuration used when another project declares an
`implementation` or `runtimeOnly` dependency on this project.

Both must be overridden because the root project's `multirelase` configuration
(which extends `implementation`) resolves through `runtimeElements`.

---

## 8. Multi-Release JAR — Deep Dive

The Multi-Release JAR (MRJAR) is the defining feature of ForgeWrapper's build
system. Here is the complete picture.

### What is a Multi-Release JAR?

Defined by [JEP 238](https://openjdk.org/jeps/238) and standardized in Java 9,
a Multi-Release JAR allows a single JAR file to contain multiple versions of
the same class, each compiled for a different Java version. The JVM
automatically selects the appropriate version at runtime based on the JVM
version.

### JAR Internal Structure

```
ForgeWrapper-projt-LOCAL.jar
│
├── META-INF/
│   ├── MANIFEST.MF
│   │   ├── Multi-Release: true         ← Activates MRJAR behavior
│   │   ├── Specification-Title: ForgeWrapper
│   │   ├── Specification-Vendor: ZekerZhayard
│   │   ├── Specification-Version: projt
│   │   ├── Implementation-Title: ForgeWrapper
│   │   ├── Implementation-Version: projt-LOCAL
│   │   ├── Implementation-Vendor: ZekerZhayard
│   │   ├── Implementation-Timestamp: 2026-04-05T...
│   │   ├── Automatic-Module-Name: io.github.zekerzhayard.forgewrapper
│   │   └── GitCommit: null
│   │
│   └── versions/
│       └── 9/
│           └── io/github/zekerzhayard/forgewrapper/installer/util/
│               └── ModuleUtil.class     ← Java 9 bytecode (version 53.0)
│
├── io/github/zekerzhayard/forgewrapper/installer/
│   ├── Bootstrap.class                  ← Java 8 bytecode (version 52.0)
│   ├── Installer.class
│   ├── Main.class
│   ├── detector/
│   │   ├── DetectorLoader.class
│   │   ├── IFileDetector.class
│   │   └── MultiMCFileDetector.class
│   └── util/
│       └── ModuleUtil.class             ← Java 8 bytecode (no-op stubs)
```

### Runtime Class Selection

```
JVM Version      Class Loaded for ModuleUtil
───────────      ──────────────────────────────────────────────────
Java 8           io/github/.../util/ModuleUtil.class (root, no-ops)
Java 9           META-INF/versions/9/io/github/.../util/ModuleUtil.class
Java 10          META-INF/versions/9/io/github/.../util/ModuleUtil.class
Java 11          META-INF/versions/9/io/github/.../util/ModuleUtil.class
  ...                         (same — highest ≤ JVM version wins)
Java 21          META-INF/versions/9/io/github/.../util/ModuleUtil.class
```

The JVM always selects the highest versioned class that is ≤ the running JVM
version. Since ForgeWrapper only has a version 9 overlay, all Java 9+ JVMs
use the same Java 9 `ModuleUtil`.

### The Dual ModuleUtil Classes

**Root ModuleUtil** (`src/main/java/.../util/ModuleUtil.java`) — 42 lines:

```java
public class ModuleUtil {
    public static void addModules(String modulePath) {
        // nothing to do with Java 8
    }
    public static void addExports(List<String> exports) {
        // nothing to do with Java 8
    }
    public static void addOpens(List<String> opens) {
        // nothing to do with Java 8
    }
    public static void setupClassPath(Path libraryDir, List<String> paths)
            throws Throwable {
        // Uses URLClassLoader.addURL() via reflection
    }
    public static Class<?> setupBootstrapLauncher(Class<?> mainClass) {
        // nothing to do with Java 8
        return mainClass;
    }
    public static ClassLoader getPlatformClassLoader() {
        // PlatformClassLoader does not exist in Java 8
        return null;
    }
}
```

**Jigsaw ModuleUtil** (`jigsaw/src/main/java/.../util/ModuleUtil.java`) — 150+ lines:

The Java 9 version provides actual JPMS integration:
- `addModules()` — Dynamically adds module paths to the boot module layer at
  runtime using `sun.misc.Unsafe` to access `MethodHandles.Lookup.IMPL_LOOKUP`
  and reflectively manipulate the module graph.
- `addExports()` / `addOpens()` — Adds `--add-exports` / `--add-opens` at
  runtime via `Module.implAddExports()` and `Module.implAddOpens()`.
- `setupClassPath()` — Uses the module system's `ModuleLayer` to properly add
  libraries.
- `setupBootstrapLauncher()` — Sets up the BootstrapLauncher class introduced
  in newer Forge versions.
- `getPlatformClassLoader()` — Returns `ClassLoader.getPlatformClassLoader()`
  (Java 9+ API, does not exist in Java 8).

---

## 9. Build Pipeline Diagram

### Complete Build Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                        ./gradlew build                          │
└──────────────────────────────┬──────────────────────────────────┘
                               │
              ┌────────────────┴────────────────┐
              ▼                                 ▼
┌──────────────────────┐           ┌──────────────────────┐
│   :compileJava       │           │   :jigsaw:compileJava │
│   (Java 8, v52.0)    │           │   (Java 9, v53.0)    │
│                      │           │                      │
│ Source:              │           │ Source:              │
│  src/main/java/      │           │  jigsaw/src/main/    │
│   └─ ...ModuleUtil   │           │   └─ ...ModuleUtil   │
│      (no-op stubs)   │           │      (full JPMS impl)│
└──────────┬───────────┘           └──────────┬───────────┘
           │                                  │
           ▼                                  ▼
┌──────────────────────┐           ┌──────────────────────┐
│   :processResources  │           │  :jigsaw:jar         │
└──────────┬───────────┘           │  → jigsaw.jar        │
           │                       └──────────┬───────────┘
           │                                  │
           │      ┌───────────────────────────┘
           │      │ configurations.multirelase
           │      │ resolves to jigsaw.jar
           ▼      ▼
┌──────────────────────────────────────────────┐
│                    :jar                       │
│                                              │
│  1. Pack root classes → /                    │
│  2. zipTree(jigsaw.jar)                      │
│     exclude META-INF/**                      │
│     → META-INF/versions/9/                   │
│  3. Generate MANIFEST.MF with attributes     │
│     including Multi-Release: true            │
│                                              │
│  Output: build/libs/ForgeWrapper-<ver>.jar   │
└──────────────────────┬───────────────────────┘
                       │
          ┌────────────┼────────────┐
          ▼            ▼            ▼
┌──────────────┐ ┌──────────┐ ┌──────────────┐
│ :sourcesJar  │ │ :check   │ │ :assemble    │
│ (sources)    │ │ (tests)  │ │ (lifecycle)  │
└──────┬───────┘ └────┬─────┘ └──────┬───────┘
       │              │              │
       └──────────────┼──────────────┘
                      ▼
            ┌──────────────────┐
            │     :build       │
            └──────────────────┘
```

### Publishing Flow

```
┌──────────────┐
│    :build     │
└──────┬───────┘
       │  tasks.publish.dependsOn build
       ▼
┌──────────────────────────────────────────────────────┐
│                     :publish                          │
│                                                      │
│  Publication: maven(MavenPublication)                │
│    groupId:    io.github.zekerzhayard                │
│    artifactId: ForgeWrapper                          │
│    version:    projt-LOCAL (or projt-YYYY-MM-DD)     │
│                                                      │
│  from components.java:                               │
│    ├── ForgeWrapper-<ver>.jar                        │
│    └── ForgeWrapper-<ver>-sources.jar                │
│                                                      │
│  Target: build/maven/                                │
│    └── io/github/zekerzhayard/ForgeWrapper/<ver>/    │
│        ├── ForgeWrapper-<ver>.jar                    │
│        ├── ForgeWrapper-<ver>-sources.jar            │
│        ├── ForgeWrapper-<ver>.pom                    │
│        └── ForgeWrapper-<ver>.module                 │
└──────────────────────────────────────────────────────┘
```

### Version Resolution Flow

```
gradle.properties                build.gradle
┌──────────────────┐      ┌──────────────────────────────────┐
│ fw_version=projt │─────▶│ version="${fw_version}${->...}"  │
└──────────────────┘      │              │                   │
                          │              ▼                   │
                          │   getVersionSuffix()             │
                          │     │                            │
                          │     ├─ IS_PUBLICATION!=null?      │
                          │     │   └─ YES → "-YYYY-MM-DD"   │
                          │     │                            │
                          │     ├─ GITHUB_ACTIONS=="true"?   │
                          │     │   └─ YES → "-YYYY-MM-DD"   │
                          │     │                            │
                          │     └─ else → "-LOCAL"           │
                          │                                  │
                          │  Result: "projt-LOCAL"           │
                          │      or: "projt-2026-04-05"      │
                          └──────────────────────────────────┘
```

---

## 10. Build Targets and Tasks

### Key Tasks

| Task                        | Type              | Description                                    |
|-----------------------------|-------------------|------------------------------------------------|
| `:compileJava`              | JavaCompile       | Compiles root `src/main/java` with Java 8      |
| `:processResources`         | Copy              | Copies `src/main/resources` to build dir       |
| `:classes`                  | Lifecycle         | Depends on compileJava + processResources      |
| `:jar`                      | Jar               | Assembles the Multi-Release JAR                |
| `:sourcesJar`               | Jar               | Assembles sources JAR                          |
| `:assemble`                 | Lifecycle         | Depends on jar + sourcesJar                    |
| `:check`                    | Lifecycle         | Runs tests (none configured)                   |
| `:build`                    | Lifecycle         | Depends on assemble + check                    |
| `:publish`                  | Lifecycle         | Publishes to build/maven/ (depends on build)   |
| `:jigsaw:compileJava`       | JavaCompile       | Compiles jigsaw `src/main/java` with Java 9    |
| `:jigsaw:jar`               | Jar               | Packages jigsaw classes into jigsaw.jar        |
| `:eclipse`                  | Lifecycle         | Generates Eclipse project files                |
| `:jigsaw:eclipse`           | Lifecycle         | Generates Eclipse files for jigsaw subproject  |
| `:clean`                    | Delete            | Removes `build/` directory                     |
| `:jigsaw:clean`             | Delete            | Removes `jigsaw/build/` directory              |

### Task Dependency Chain

Running `./gradlew build` triggers this chain:

```
:build
├── :assemble
│   ├── :jar
│   │   ├── :classes
│   │   │   ├── :compileJava
│   │   │   └── :processResources
│   │   └── [multirelase config resolution]
│   │       └── :jigsaw:jar
│   │           └── :jigsaw:classes
│   │               ├── :jigsaw:compileJava
│   │               └── :jigsaw:processResources
│   └── :sourcesJar
└── :check
    └── :test (no tests configured → no-op)
```

### What Each Task Produces

| Task                    | Output                                              |
|-------------------------|-----------------------------------------------------|
| `:compileJava`          | `build/classes/java/main/**/*.class`                |
| `:jigsaw:compileJava`   | `jigsaw/build/classes/java/main/**/*.class`         |
| `:jigsaw:jar`           | `jigsaw/build/libs/jigsaw.jar`                      |
| `:jar`                  | `build/libs/ForgeWrapper-<ver>.jar`                 |
| `:sourcesJar`           | `build/libs/ForgeWrapper-<ver>-sources.jar`         |
| `:publish`              | `build/maven/io/github/zekerzhayard/ForgeWrapper/`  |

---

## 11. Step-by-Step Build Guide

### Prerequisites

1. **Java 8 or higher** — To run Gradle itself. Gradle 7.3.3 supports Java
   8 through Java 17.
2. **Java 9 or higher JDK** — Required to compile the jigsaw subproject. If
   Gradle runs on Java 8, the toolchain must find a Java 9+ JDK. If Gradle
   runs on Java 9+, no additional JDK is needed.
3. **Internet access** — To download the Gradle wrapper distribution and
   Maven dependencies (first build only; cached after that).

### Clone and Build

```bash
# Navigate to the forgewrapper directory
cd forgewrapper/

# Make the wrapper executable (Linux/macOS)
chmod +x gradlew

# Build the project
./gradlew build
```

On Windows:
```cmd
cd forgewrapper\
gradlew.bat build
```

### Build Output

After a successful build:

```
build/libs/
├── ForgeWrapper-projt-LOCAL.jar             ← Main MRJAR artifact
└── ForgeWrapper-projt-LOCAL-sources.jar     ← Source code archive
```

### Build + Publish

```bash
./gradlew publish
```

This runs `build` first (due to `tasks.publish.dependsOn build`), then publishes:

```
build/maven/
└── io/
    └── github/
        └── zekerzhayard/
            └── ForgeWrapper/
                └── projt-LOCAL/
                    ├── ForgeWrapper-projt-LOCAL.jar
                    ├── ForgeWrapper-projt-LOCAL-sources.jar
                    ├── ForgeWrapper-projt-LOCAL.pom
                    └── ForgeWrapper-projt-LOCAL.module
```

### Clean Build

```bash
./gradlew clean build
```

Removes all generated files in `build/` and `jigsaw/build/` before rebuilding.

### Individual Tasks

```bash
# Compile only the root project
./gradlew compileJava

# Compile only the jigsaw subproject
./gradlew :jigsaw:compileJava

# Build only the JAR (no tests, no publish)
./gradlew jar

# Generate Eclipse project files
./gradlew eclipse

# List all available tasks
./gradlew tasks --all

# Show the dependency tree
./gradlew dependencies
```

### Simulating CI Build Locally

```bash
# Set environment variables to simulate GitHub Actions
GITHUB_ACTIONS=true GITHUB_SHA=abc123def456 ./gradlew build

# Or force a publication-style version
IS_PUBLICATION=1 ./gradlew build
```

With either of these, the version suffix changes from `-LOCAL` to the current
date (e.g., `-2026-04-05`), and `GitCommit` in the manifest is set to the
provided SHA.

### Verbose/Debug Build

```bash
# Show task execution details
./gradlew build --info

# Full debug logging
./gradlew build --debug

# Show dependency resolution details
./gradlew build --scan
```

---

## 12. CI/CD Integration

The build system detects CI environments via environment variables.

### GitHub Actions Detection

Two environment variables are checked in `getVersionSuffix()` (line 86 of
`build.gradle`):

**`GITHUB_ACTIONS`** — Automatically set to `"true"` by GitHub Actions runners.
When detected, the version suffix switches from `-LOCAL` to a date stamp.

**`GITHUB_SHA`** — The full SHA of the commit that triggered the workflow. This
is embedded in the JAR manifest as the `GitCommit` attribute (line 55 of
`build.gradle`):

```groovy
"GitCommit": String.valueOf(System.getenv("GITHUB_SHA"))
```

### IS_PUBLICATION

**`IS_PUBLICATION`** — A custom environment variable. When set (to any non-null
value), it forces date-stamped versioning regardless of whether the build runs
in GitHub Actions. This allows publication builds from other CI systems or
local environments:

```bash
IS_PUBLICATION=yes ./gradlew publish
```

### CI Build vs Local Build Comparison

```
┌───────────────────────┬─────────────────────┬──────────────────────┐
│ Aspect                │ Local Build         │ CI Build             │
├───────────────────────┼─────────────────────┼──────────────────────┤
│ Version suffix        │ -LOCAL              │ -2026-04-05          │
│ GitCommit manifest    │ null                │ abc123def456...      │
│ Gradle daemon         │ disabled (property) │ disabled (property)  │
│ Full version example  │ projt-LOCAL         │ projt-2026-04-05     │
│ Spec-Version          │ projt               │ projt                │
│ Impl-Version          │ projt-LOCAL         │ projt-2026-04-05     │
└───────────────────────┴─────────────────────┴──────────────────────┘
```

### Environment Variable Summary

| Variable         | Set By           | Used In                   | Effect                         |
|------------------|------------------|---------------------------|--------------------------------|
| `GITHUB_ACTIONS` | GitHub Actions   | `getVersionSuffix()`      | Enables date-stamped version   |
| `GITHUB_SHA`     | GitHub Actions   | `jar.manifest.attributes` | Records commit SHA in manifest |
| `IS_PUBLICATION` | Manual / CI      | `getVersionSuffix()`      | Forces date-stamped version    |

---

## 13. Artifact Output Structure

### Primary Artifact: ForgeWrapper JAR

**File:** `build/libs/ForgeWrapper-<version>.jar`

Inspecting the JAR contents (example):

```bash
jar tf build/libs/ForgeWrapper-projt-LOCAL.jar
```

Expected output:

```
META-INF/
META-INF/MANIFEST.MF
io/
io/github/
io/github/zekerzhayard/
io/github/zekerzhayard/forgewrapper/
io/github/zekerzhayard/forgewrapper/installer/
io/github/zekerzhayard/forgewrapper/installer/Bootstrap.class
io/github/zekerzhayard/forgewrapper/installer/Installer.class
io/github/zekerzhayard/forgewrapper/installer/Main.class
io/github/zekerzhayard/forgewrapper/installer/detector/
io/github/zekerzhayard/forgewrapper/installer/detector/DetectorLoader.class
io/github/zekerzhayard/forgewrapper/installer/detector/IFileDetector.class
io/github/zekerzhayard/forgewrapper/installer/detector/MultiMCFileDetector.class
io/github/zekerzhayard/forgewrapper/installer/util/
io/github/zekerzhayard/forgewrapper/installer/util/ModuleUtil.class
META-INF/versions/
META-INF/versions/9/
META-INF/versions/9/io/
META-INF/versions/9/io/github/
META-INF/versions/9/io/github/zekerzhayard/
META-INF/versions/9/io/github/zekerzhayard/forgewrapper/
META-INF/versions/9/io/github/zekerzhayard/forgewrapper/installer/
META-INF/versions/9/io/github/zekerzhayard/forgewrapper/installer/util/
META-INF/versions/9/io/github/zekerzhayard/forgewrapper/installer/util/ModuleUtil.class
```

### Sources Artifact

**File:** `build/libs/ForgeWrapper-<version>-sources.jar`

Contains `.java` source files from the root project's `src/main/java/` tree.

### Published Maven Artifacts

**Directory:** `build/maven/io/github/zekerzhayard/ForgeWrapper/<version>/`

| File                                     | Description                        |
|------------------------------------------|------------------------------------|
| `ForgeWrapper-<ver>.jar`                 | The MRJAR binary                   |
| `ForgeWrapper-<ver>-sources.jar`         | Source code archive                |
| `ForgeWrapper-<ver>.pom`                 | Maven POM with dependency metadata |
| `ForgeWrapper-<ver>.module`              | Gradle Module Metadata (GMM)       |
| `*.md5`, `*.sha1`, `*.sha256`, `*.sha512`| Integrity checksums               |

### Manifest File Content

The `META-INF/MANIFEST.MF` in the produced JAR:

```
Manifest-Version: 1.0
Specification-Title: ForgeWrapper
Specification-Vendor: ZekerZhayard
Specification-Version: projt
Implementation-Title: ForgeWrapper
Implementation-Version: projt-LOCAL
Implementation-Vendor: ZekerZhayard
Implementation-Timestamp: 2026-04-05T12:00:00+0000
Automatic-Module-Name: io.github.zekerzhayard.forgewrapper
Multi-Release: true
GitCommit: null
```

---

## 14. Publishing to Local Maven Repository

### How It Works

The `publishing` block in `build.gradle` (lines 65–80) configures a Maven
publication with a **local filesystem repository**:

```groovy
repositories {
    maven {
        url = layout.buildDirectory.dir("maven")
    }
}
```

`layout.buildDirectory.dir("maven")` resolves to `build/maven/`. This is a
Gradle 7+ API (`layout.buildDirectory` replaces the deprecated `$buildDir`).

### Running the Publish

```bash
./gradlew publish
```

The `publish` task depends on `build` (line 81: `tasks.publish.dependsOn build`),
so running `publish` implicitly runs the full build first.

### Generated POM

The POM is auto-generated from `components.java`. Since all external dependencies
are `compileOnly`, they are **not** included in the POM. The POM contains only
the GAV (Group, Artifact, Version) coordinates:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xsi:schemaLocation="...">
  <modelVersion>4.0.0</modelVersion>
  <groupId>io.github.zekerzhayard</groupId>
  <artifactId>ForgeWrapper</artifactId>
  <version>projt-LOCAL</version>
</project>
```

No `<dependencies>` section is generated because `compileOnly` dependencies are
not published.

### Using the Local Maven Repo

Other projects can consume the published artifact by adding the local directory
as a Maven repository:

```groovy
repositories {
    maven {
        url = file("/path/to/forgewrapper/build/maven")
    }
}

dependencies {
    implementation "io.github.zekerzhayard:ForgeWrapper:projt-LOCAL"
}
```

---

## 15. Java Version Requirements

### Summary Table

| Component           | Minimum Java | Configured As                               |
|---------------------|--------------|---------------------------------------------|
| Gradle execution    | Java 8       | Gradle 7.3.3 supports Java 8–17             |
| Root compilation    | Java 8       | `sourceCompatibility = targetCompatibility = 1.8` |
| Jigsaw compilation  | Java 9       | `sourceCompatibility = targetCompatibility = 9`   |
| Runtime (Java 8)    | Java 8       | Uses root ModuleUtil (no-op stubs)          |
| Runtime (Java 9+)   | Java 9       | Uses jigsaw ModuleUtil (full JPMS)          |

### Toolchain Behavior

The jigsaw subproject uses Gradle's Java Toolchain feature with conditional
logic (lines 7–9 of `jigsaw/build.gradle`):

```
┌────────────────────────────────┐
│ Gradle running on Java 8?      │
│                                │
│   YES → javaToolchains finds   │
│         Java 9 JDK on system   │
│         and uses it to compile │
│                                │
│   NO  → Current JVM (≥9)      │
│         compiles with          │
│         sourceCompatibility=9  │
└────────────────────────────────┘
```

### Finding Java Toolchains

When Gradle needs a Java 9 toolchain, it searches these locations:

1. **Environment** — `JAVA_HOME`, `JDK_HOME`, and `PATH` entries.
2. **Standard paths** — `/usr/lib/jvm/` (Linux), `/Library/Java/JavaVirtualMachines/` (macOS),
   Windows Registry.
3. **Tool managers** — SDKMAN!, jabba, IntelliJ installations.
4. **Gradle auto-provisioning** — If enabled, Gradle can download a JDK
   automatically from AdoptOpenJDK/Adoptium.

### Bytecode Versions

| Class Origin        | Target | Class File Version | `-target` flag |
|---------------------|--------|--------------------|----------------|
| Root project        | 1.8    | 52.0               | `1.8`          |
| Jigsaw subproject   | 9      | 53.0               | `9`            |

You can verify bytecode versions with `javap`:

```bash
# Check root ModuleUtil (should be 52.0 = Java 8)
javap -v build/classes/java/main/io/github/zekerzhayard/forgewrapper/installer/util/ModuleUtil.class \
  | grep "major version"

# Check jigsaw ModuleUtil (should be 53.0 = Java 9)
javap -v jigsaw/build/classes/java/main/io/github/zekerzhayard/forgewrapper/installer/util/ModuleUtil.class \
  | grep "major version"
```

---

## 16. Source File Layout and the Dual-ModuleUtil Pattern

### Why Two ModuleUtil Classes?

ForgeWrapper must run on both Java 8 and Java 9+ JVMs. The Java 9+ `ModuleUtil`
uses APIs that don't exist in Java 8:

| API Used in Jigsaw ModuleUtil           | Introduced In |
|-----------------------------------------|---------------|
| `java.lang.module.Configuration`        | Java 9        |
| `java.lang.module.ModuleFinder`         | Java 9        |
| `java.lang.module.ModuleReference`      | Java 9        |
| `java.lang.module.ResolvedModule`       | Java 9        |
| `ModuleLayer.boot()`                    | Java 9        |
| `ClassLoader.getPlatformClassLoader()`  | Java 9        |
| `List.of()`                             | Java 9        |
| `Module.implAddExports()`               | Java 9 (internal) |
| `Module.implAddOpens()`                 | Java 9 (internal) |

If these APIs were in the main source tree compiled with Java 8, the build
would fail with compilation errors. The Multi-Release JAR approach solves this
by keeping the Java 9 code in a separate compilation unit.

### Method Signature Compatibility

Both `ModuleUtil` classes must have **identical method signatures** so that
callers in the root project (e.g., `Bootstrap.java`, `Main.java`) can reference
`ModuleUtil` without caring which version is loaded at runtime.

**Root ModuleUtil (Java 8 stubs):**
```java
public static void addModules(String modulePath)        // line 10: empty body
public static void addExports(List<String> exports)     // line 14: empty body
public static void addOpens(List<String> opens)         // line 18: empty body
public static void setupClassPath(Path, List<String>)   // line 22: URLClassLoader reflection
public static Class<?> setupBootstrapLauncher(Class<?>)// line 31: returns mainClass
public static ClassLoader getPlatformClassLoader()      // line 36: returns null
```

**Jigsaw ModuleUtil (Java 9 implementation):**
```java
public static void addModules(String modulePath)        // Full JPMS module loading
public static void addExports(List<String> exports)     // Module.implAddExports
public static void addOpens(List<String> opens)         // Module.implAddOpens
public static void setupClassPath(Path, List<String>)   // Module-aware loading
public static Class<?> setupBootstrapLauncher(Class<?>)// BootstrapLauncher setup
public static ClassLoader getPlatformClassLoader()      // ClassLoader.getPlatformClassLoader()
```

### Call Sites

`Bootstrap.java` calls these ModuleUtil methods (lines 70–76):

```java
if (modulePath != null) {
    ModuleUtil.addModules(modulePath);
}
ModuleUtil.addExports(addExports);
ModuleUtil.addOpens(addOpens);
```

`Main.java` calls (lines 49, 62, 63):

```java
// line 49 (in URLClassLoader creation):
ModuleUtil.getPlatformClassLoader()

// lines 62-63:
ModuleUtil.setupClassPath(detector.getLibraryDir(), ...);
Class<?> mainClass = ModuleUtil.setupBootstrapLauncher(Class.forName(...));
```

At runtime, the JVM transparently loads the correct `ModuleUtil` class based on
the Java version, with no conditional logic needed in the calling code.

---

## 17. Troubleshooting

### Build Fails: "No compatible toolchains found"

**Symptom:**
```
> No locally installed toolchains match and toolchain download repositories
  have not been configured.
```

**Cause:** Gradle is running on Java 8 and cannot find a Java 9+ JDK for the
jigsaw subproject.

**Fix:** Install a Java 9+ JDK. On Linux:
```bash
# Ubuntu/Debian
sudo apt install openjdk-11-jdk

# Fedora
sudo dnf install java-11-openjdk-devel
```

Or set `JAVA_HOME` to an existing Java 9+ installation:
```bash
export JAVA_HOME=/path/to/jdk-11
./gradlew build
```

### Build Fails: "Could not resolve project :jigsaw"

**Symptom:**
```
> Could not resolve project :jigsaw.
  > Incompatible because this component declares a component compatible
    with Java 9 and the consumer needed a component compatible with Java 8
```

**Cause:** The `TargetJvmVersion` attribute override in `jigsaw/build.gradle`
is missing or incorrect.

**Fix:** Ensure lines 16–27 of `jigsaw/build.gradle` are present:
```groovy
configurations {
    apiElements {
        attributes {
            attribute TargetJvmVersion.TARGET_JVM_VERSION_ATTRIBUTE, 8
        }
    }
    runtimeElements {
        attributes {
            attribute TargetJvmVersion.TARGET_JVM_VERSION_ATTRIBUTE, 8
        }
    }
}
```

### Dependencies Not Found

**Symptom:**
```
> Could not find cpw.mods:modlauncher:8.0.9.
```

**Cause:** The Forge Maven repository is unreachable or not declared.

**Fix:** Check network connectivity to `https://maven.minecraftforge.net/`.
Verify the `repositories` block in `build.gradle` includes the `forge` maven
repository (lines 24–29).

### Wrong Version in JAR Name

**Symptom:** JAR is named `ForgeWrapper-projt-LOCAL.jar` but you expected a
dated version.

**Cause:** `GITHUB_ACTIONS` and `IS_PUBLICATION` are not set.

**Fix:**
```bash
IS_PUBLICATION=1 ./gradlew build
```

### MultiRelease Attribute Missing

**Symptom:** The JAR does not exhibit Multi-Release behavior on Java 9+.
`ModuleUtil` methods are no-ops even on Java 11.

**Diagnosis:** Inspect the manifest:
```bash
unzip -p build/libs/ForgeWrapper-*.jar META-INF/MANIFEST.MF | grep Multi-Release
```

If `Multi-Release: true` is missing, check the `jar` block in `build.gradle`
(line 54).

Also verify the jigsaw classes exist in the JAR:
```bash
jar tf build/libs/ForgeWrapper-*.jar | grep "META-INF/versions/9"
```

Expected output should show the `ModuleUtil.class` under `META-INF/versions/9/`.

### Gradle Daemon Issues

**Symptom:** Stale build state, phantom errors that disappear after restarting.

**Note:** The daemon is disabled (`org.gradle.daemon = false` in
`gradle.properties`), so this should not normally occur. If a daemon is somehow
running from a previous configuration:

```bash
./gradlew --stop
./gradlew clean build
```

### Java Version Mismatch in IDE

**Symptom:** Eclipse or IntelliJ shows errors on jigsaw source files.

**Cause:** The IDE is using Java 8 to compile the jigsaw source set.

**Fix for Eclipse:**
```bash
./gradlew eclipse
```
Then re-import the project. The Eclipse plugin generates `.classpath` with
correct JRE containers.

**Fix for IntelliJ:** Import as Gradle project. IntelliJ reads the `compileJava`
block and sets the jigsaw module to use Java 9.

### Verifying the MRJAR

Full verification workflow:

```bash
# 1. Build
./gradlew clean build

# 2. Check JAR contents
jar tf build/libs/ForgeWrapper-projt-LOCAL.jar

# 3. Verify manifest
unzip -p build/libs/ForgeWrapper-projt-LOCAL.jar META-INF/MANIFEST.MF

# 4. Check bytecode versions
cd build/libs
mkdir -p _verify && cd _verify
jar xf ../ForgeWrapper-projt-LOCAL.jar

# Root ModuleUtil → should be 52 (Java 8)
javap -v io/github/zekerzhayard/forgewrapper/installer/util/ModuleUtil.class \
  | grep "major version"

# Jigsaw ModuleUtil → should be 53 (Java 9)
javap -v META-INF/versions/9/io/github/zekerzhayard/forgewrapper/installer/util/ModuleUtil.class \
  | grep "major version"

# 5. Cleanup
cd .. && rm -rf _verify
```

---

## 18. Quick Reference Card

```
┌──────────────────────────────────────────────────────────────────┐
│                    ForgeWrapper Build Cheat Sheet                 │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Build:          ./gradlew build                                 │
│  Clean+Build:    ./gradlew clean build                           │
│  Publish:        ./gradlew publish                               │
│  JAR only:       ./gradlew jar                                   │
│  Eclipse:        ./gradlew eclipse                               │
│  Tasks list:     ./gradlew tasks --all                           │
│  Dependencies:   ./gradlew dependencies                          │
│                                                                  │
│  CI version:     GITHUB_ACTIONS=true ./gradlew build             │
│  Pub version:    IS_PUBLICATION=1 ./gradlew build                │
│                                                                  │
│  Gradle:         7.3.3 (wrapper)                                 │
│  Root Java:      1.8 (source + target)                           │
│  Jigsaw Java:    9 (source + target, toolchain auto-detect)      │
│  Group:          io.github.zekerzhayard                          │
│  Artifact:       ForgeWrapper                                    │
│  Base Version:   projt (from gradle.properties: fw_version)      │
│                                                                  │
│  Output JAR:     build/libs/ForgeWrapper-<ver>.jar               │
│  Sources JAR:    build/libs/ForgeWrapper-<ver>-sources.jar       │
│  Maven output:   build/maven/                                    │
│                                                                  │
│  Repos:          Maven Central                                   │
│                  https://maven.minecraftforge.net/                │
│                                                                  │
│  Dependencies (all compileOnly):                                 │
│    gson 2.8.7, modlauncher 8.0.9,                                │
│    installer 2.2.7, jopt-simple 5.0.4                            │
│                                                                  │
│  MRJAR overlay:  META-INF/versions/9/ (from :jigsaw)             │
│  Manifest key:   Multi-Release: true                             │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

*This document was generated from direct analysis of the ForgeWrapper build
configuration files: `build.gradle`, `jigsaw/build.gradle`, `settings.gradle`,
`gradle.properties`, and `gradle/wrapper/gradle-wrapper.properties`.*
