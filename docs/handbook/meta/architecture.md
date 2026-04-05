# Meta — Architecture

## Module Structure

Meta is organized as a standard Python package with three sub-packages, each serving a distinct role in the pipeline:

```
meta/
├── __init__.py          # Package marker: """Meta package of meta"""
├── common/              # Shared utilities, constants, and static data
├── model/               # Pydantic data models for all upstream formats
└── run/                 # Executable scripts (update_*, generate_*, index)
```

### Dependency Flow

```
run/ ──depends-on──► model/ ──depends-on──► common/
run/ ──depends-on──► common/
```

The `run/` modules import from both `model/` and `common/`. The `model/` package imports from `common/` for shared constants and utilities. There are **no** circular dependencies.

---

## The `common/` Package

### `common/__init__.py` — Core Utilities

This module provides the foundational infrastructure used by every other module:

#### Path Resolution

Three functions resolve working directories from environment variables with filesystem fallbacks:

```python
def cache_path():
    if "META_CACHE_DIR" in os.environ:
        return os.environ["META_CACHE_DIR"]
    return "cache"

def launcher_path():
    if "META_LAUNCHER_DIR" in os.environ:
        return os.environ["META_LAUNCHER_DIR"]
    return "launcher"

def upstream_path():
    if "META_UPSTREAM_DIR" in os.environ:
        return os.environ["META_UPSTREAM_DIR"]
    return "upstream"
```

#### Directory Creation

```python
def ensure_upstream_dir(path):
    """Create a subdirectory under the upstream root."""
    path = os.path.join(upstream_path(), path)
    if not os.path.exists(path):
        os.makedirs(path)

def ensure_component_dir(component_id: str):
    """Create a component directory under the launcher root."""
    path = os.path.join(launcher_path(), component_id)
    if not os.path.exists(path):
        os.makedirs(path)
```

#### HTTP Session Factory

```python
def default_session():
    cache = FileCache(os.path.join(cache_path(), "http_cache"))
    sess = CacheControl(requests.Session(), cache)
    sess.headers.update({"User-Agent": "ProjectTickMeta/1.0"})
    return sess
```

All HTTP requests are made through this cached session. The `CacheControl` wrapper stores responses in a disk-backed `FileCache`, honoring standard HTTP caching headers.

#### Datetime Serialization

```python
def serialize_datetime(dt: datetime.datetime):
    if dt.tzinfo is None:
        return dt.replace(tzinfo=datetime.timezone.utc).isoformat()
    return dt.isoformat()
```

Used as Pydantic's custom JSON encoder for `datetime` fields — naive datetimes are assumed UTC.

#### File Hashing

```python
def file_hash(filename: str, hashtype: Callable[[], "hashlib._Hash"], blocksize: int = 65536) -> str:
    hashtype = hashtype()
    with open(filename, "rb") as f:
        for block in iter(lambda: f.read(blocksize), b""):
            hashtype.update(block)
    return hashtype.hexdigest()
```

Used throughout the pipeline for SHA-1 and SHA-256 integrity checksums on installer JARs and version files.

#### SHA-1 Caching

```python
def get_file_sha1_from_file(file_name: str, sha1_file: str) -> Optional[str]:
```

Reads a cached `.sha1` sidecar file if it exists; otherwise computes and writes the SHA-1 hash. Used by Forge/NeoForge update scripts to detect when an installer JAR needs re-downloading.

#### Other Utilities

| Function | Purpose |
|---|---|
| `transform_maven_key(key)` | Replaces `:` with `.` in Maven coordinates for filesystem paths |
| `replace_old_launchermeta_url(url)` | Rewrites `launchermeta.mojang.com` → `piston-meta.mojang.com` |
| `merge_dict(base, overlay)` | Deep-merges two dicts (base provides defaults, overlay wins) |
| `get_all_bases(cls)` | Returns the complete MRO (method resolution order) for a class |
| `remove_files(file_paths)` | Silently deletes a list of files |
| `eprint(*args)` | Prints to stderr |
| `LAUNCHER_MAVEN` | URL template: `"https://files.projecttick.org/maven/%s"` |

### `common/http.py` — Binary Downloads

A single function:

```python
def download_binary_file(sess, path, url):
    with open(path, "wb") as f:
        r = sess.get(url)
        r.raise_for_status()
        for chunk in r.iter_content(chunk_size=128):
            f.write(chunk)
```

Used to download Forge/NeoForge installer JARs and Fabric/Quilt JAR files.

### `common/mojang.py` — Mojang Constants

```python
BASE_DIR = "mojang"
VERSION_MANIFEST_FILE = join(BASE_DIR, "version_manifest_v2.json")
VERSIONS_DIR = join(BASE_DIR, "versions")
ASSETS_DIR = join(BASE_DIR, "assets")

STATIC_EXPERIMENTS_FILE = join(dirname(__file__), "mojang-minecraft-experiments.json")
STATIC_OLD_SNAPSHOTS_FILE = join(dirname(__file__), "mojang-minecraft-old-snapshots.json")
STATIC_OVERRIDES_FILE = join(dirname(__file__), "mojang-minecraft-legacy-override.json")
STATIC_LEGACY_SERVICES_FILE = join(dirname(__file__), "mojang-minecraft-legacy-services.json")
LIBRARY_PATCHES_FILE = join(dirname(__file__), "mojang-library-patches.json")

MINECRAFT_COMPONENT = "net.minecraft"
LWJGL_COMPONENT = "org.lwjgl"
LWJGL3_COMPONENT = "org.lwjgl3"
JAVA_MANIFEST_FILE = join(BASE_DIR, "java_all.json")
```

### `common/forge.py` — Forge Constants

```python
BASE_DIR = "forge"
JARS_DIR = join(BASE_DIR, "jars")
INSTALLER_INFO_DIR = join(BASE_DIR, "installer_info")
INSTALLER_MANIFEST_DIR = join(BASE_DIR, "installer_manifests")
VERSION_MANIFEST_DIR = join(BASE_DIR, "version_manifests")
FILE_MANIFEST_DIR = join(BASE_DIR, "files_manifests")
DERIVED_INDEX_FILE = join(BASE_DIR, "derived_index.json")
LEGACYINFO_FILE = join(BASE_DIR, "legacyinfo.json")

FORGE_COMPONENT = "net.minecraftforge"

FORGEWRAPPER_LIBRARY = make_launcher_library(
    GradleSpecifier("io.github.zekerzhayard", "ForgeWrapper", "projt-2026-04-04"),
    "4c4653d80409e7e968d3e3209196ffae778b7b4e",
    29731,
)

BAD_VERSIONS = ["1.12.2-14.23.5.2851"]
```

The `FORGEWRAPPER_LIBRARY` is a pre-built `Library` object pointing to a custom ForgeWrapper build hosted on the ProjT Maven. ForgeWrapper acts as a shim layer to run modern Forge installers at launch time.

### `common/neoforge.py` — NeoForge Constants

```python
BASE_DIR = "neoforge"
NEOFORGE_COMPONENT = "net.neoforged"
```

Similar directory layout to Forge, but with its own `DERIVED_INDEX_FILE`.

### `common/fabric.py` — Fabric Constants

```python
BASE_DIR = "fabric"
JARS_DIR = join(BASE_DIR, "jars")
INSTALLER_INFO_DIR = join(BASE_DIR, "loader-installer-json")
META_DIR = join(BASE_DIR, "meta-v2")

LOADER_COMPONENT = "net.fabricmc.fabric-loader"
INTERMEDIARY_COMPONENT = "net.fabricmc.intermediary"

DATETIME_FORMAT_HTTP = "%a, %d %b %Y %H:%M:%S %Z"
```

### `common/quilt.py` — Quilt Constants

```python
USE_QUILT_MAPPINGS = False  # Quilt recommends using Fabric's intermediary

BASE_DIR = "quilt"
LOADER_COMPONENT = "org.quiltmc.quilt-loader"
INTERMEDIARY_COMPONENT = "org.quiltmc.hashed"

# If USE_QUILT_MAPPINGS is False, uses Fabric's intermediary instead
if not USE_QUILT_MAPPINGS:
    INTERMEDIARY_COMPONENT = FABRIC_INTERMEDIARY_COMPONENT

DISABLE_BEACON_ARG = "-Dloader.disable_beacon=true"
DISABLE_BEACON_VERSIONS = {
    "0.19.2-beta.3", "0.19.2-beta.4", ..., "0.20.0-beta.14",
}
```

The `DISABLE_BEACON_VERSIONS` set enumerates Quilt Loader versions that had a telemetry beacon, which is disabled via a JVM argument.

### `common/java.py` — Java Runtime Constants

```python
BASE_DIR = "java_runtime"
ADOPTIUM_DIR = join(BASE_DIR, "adoptium")
OPENJ9_DIR = join(BASE_DIR, "ibm")
AZUL_DIR = join(BASE_DIR, "azul")

JAVA_MINECRAFT_COMPONENT = "net.minecraft.java"
JAVA_ADOPTIUM_COMPONENT = "net.adoptium.java"
JAVA_OPENJ9_COMPONENT = "com.ibm.java"
JAVA_AZUL_COMPONENT = "com.azul.java"
```

---

## The `model/` Package

All data models inherit from `MetaBase`, which is a customized `pydantic.BaseModel`.

### Inheritance Hierarchy

```
pydantic.BaseModel
└── MetaBase
    ├── Versioned (adds formatVersion field)
    │   ├── MetaVersion          # Primary output format
    │   ├── MetaPackage           # Package metadata
    │   ├── MetaVersionIndex      # Version list per package
    │   └── MetaPackageIndex      # Master package list
    │
    ├── MojangArtifactBase        # sha1, sha256, size, url
    │   ├── MojangAssets          # Asset index metadata
    │   ├── MojangArtifact        # Library artifact with path
    │   └── MojangLoggingArtifact # Logging config artifact
    │
    ├── Library                   # Minecraft library reference
    │   ├── JavaAgent             # Library with Java agent argument
    │   ├── ForgeLibrary          # Forge-specific library fields
    │   └── NeoForgeLibrary       # NeoForge-specific library fields
    │
    ├── GradleSpecifier           # Maven coordinate (not a MetaBase)
    ├── Dependency                # Component dependency (uid + version)
    │
    ├── MojangVersion             # Raw Mojang version JSON
    │   ├── ForgeVersionFile      # Forge version JSON (extends Mojang)
    │   └── NeoForgeVersionFile   # NeoForge version JSON
    │
    ├── OSRule / MojangRule / MojangRules  # Platform rules
    │
    ├── ForgeEntry / NeoForgeEntry         # Version index entries
    ├── DerivedForgeIndex / DerivedNeoForgeIndex  # Reconstructed indexes
    ├── ForgeInstallerProfile / ForgeInstallerProfileV2  # Installer data
    │
    ├── FabricInstallerDataV1     # Fabric loader installer info
    ├── FabricJarInfo             # JAR release timestamp
    │
    ├── LiteloaderIndex           # Full LiteLoader metadata
    │
    ├── JavaRuntimeMeta           # Normalized Java runtime info
    ├── JavaRuntimeVersion        # MetaVersion with runtimes list
    ├── JavaVersionMeta           # Semver-style Java version
    │
    └── APIQuery                  # Base for API URL query builders
        ├── AdoptxAPIFeatureReleasesQuery
        └── AzulApiPackagesQuery
```

### `MetaBase` — The Foundation

```python
class MetaBase(pydantic.BaseModel):
    def dict(self, **kwargs):
        return super().dict(by_alias=True, **kwargs)

    def json(self, **kwargs):
        return super().json(
            exclude_none=True, sort_keys=True, by_alias=True, indent=4, **kwargs
        )

    def write(self, file_path: str):
        Path(file_path).parent.mkdir(parents=True, exist_ok=True)
        with open(file_path, "w") as f:
            f.write(self.json())

    def merge(self, other: "MetaBase"):
        """Merge other into self: concatenate lists, union sets, deep-merge dicts,
        recurse on MetaBase fields, overwrite primitives."""

    class Config:
        allow_population_by_field_name = True
        json_encoders = {datetime: serialize_datetime, GradleSpecifier: str}
```

Key design decisions:
- **`by_alias=True`** everywhere: Field aliases like `mainClass`, `releaseTime`, `assetIndex` match the JSON format the launcher expects.
- **`exclude_none=True`**: Optional fields that aren't set are omitted from output.
- **`sort_keys=True`**: Deterministic output for diff-friendly Git commits.
- **`write()`**: Every model can serialize itself to disk, creating parent directories as needed.

### `MetaVersion` — The Core Output Model

This is the primary data structure that Meta produces. Each component version (Minecraft 1.21.5, Forge 49.0.31, Fabric Loader 0.16.9, etc.) is represented as a `MetaVersion`:

```python
class MetaVersion(Versioned):
    name: str                    # Human-readable name ("Minecraft", "Forge", etc.)
    version: str                 # Version string
    uid: str                     # Component UID ("net.minecraft", "net.minecraftforge")
    type: Optional[str]          # "release", "snapshot", "experiment", "old_snapshot"
    order: Optional[int]         # Load order (-2=MC, -1=LWJGL, 5=Forge, 10=Fabric)
    volatile: Optional[bool]     # If true, may change between runs (e.g., LWJGL, mappings)
    requires: Optional[List[Dependency]]    # Required components (with version constraints)
    conflicts: Optional[List[Dependency]]   # Conflicting components
    libraries: Optional[List[Library]]      # Runtime classpath libraries
    asset_index: Optional[MojangAssets]     # Asset index reference
    maven_files: Optional[List[Library]]    # Install-time Maven downloads
    main_jar: Optional[Library]             # Main game JAR
    jar_mods: Optional[List[Library]]       # Legacy JAR mod injection
    main_class: Optional[str]               # Java main class
    applet_class: Optional[str]             # Legacy applet class
    minecraft_arguments: Optional[str]      # Game launch arguments
    release_time: Optional[datetime]        # When this version was released
    compatible_java_majors: Optional[List[int]]  # Compatible Java major versions
    compatible_java_name: Optional[str]     # Mojang Java component name
    java_agents: Optional[List[JavaAgent]]  # Java agent libraries
    additional_traits: Optional[List[str]]  # Launcher behavior hints
    additional_tweakers: Optional[List[str]]# Forge/LiteLoader tweaker classes
    additional_jvm_args: Optional[List[str]]# Extra JVM arguments
    logging: Optional[MojangLogging]        # Log4j logging configuration
```

### `GradleSpecifier` — Maven Coordinates

Not a Pydantic model but a core class used as a Pydantic-compatible type:

```python
class GradleSpecifier:
    """Maven coordinate like 'org.lwjgl.lwjgl:lwjgl:2.9.0' or
    'com.mojang:minecraft:1.21.5:client'"""

    def __init__(self, group, artifact, version, classifier=None, extension=None):
        # extension defaults to "jar"

    def filename(self):   # e.g., "lwjgl-2.9.0.jar"
    def base(self):       # e.g., "org/lwjgl/lwjgl/lwjgl/2.9.0/"
    def path(self):       # e.g., "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar"
    def is_lwjgl(self):   # True for org.lwjgl, org.lwjgl.lwjgl, java.jinput, java.jutils
    def is_log4j(self):   # True for org.apache.logging.log4j

    @classmethod
    def from_string(cls, v: str):
        # Parses "group:artifact:version[:classifier][@extension]"
```

This class supports Pydantic validators via `__get_validators__`, comparison operators for sorting, and hashing for use in sets.

---

## The `run/` Package

### Module Naming Convention

Every module follows a strict naming pattern:

- `update_<loader>.py` — Phase 1: fetch upstream data
- `generate_<loader>.py` — Phase 2: produce launcher metadata
- `index.py` — Final step: build the master package index

### Common Patterns Across Run Modules

1. **Module-level initialization**: Upstream directories are created at import time:
   ```python
   UPSTREAM_DIR = upstream_path()
   ensure_upstream_dir(JARS_DIR)
   sess = default_session()
   ```

2. **`main()` entry point**: Every module exposes a `main()` function called by `update.sh` or the Poetry entrypoints.

3. **Concurrent processing**: Most modules use `concurrent.futures.ThreadPoolExecutor`. The Fabric updater uniquely uses `multiprocessing.Pool`.

4. **Error handling**: Errors during version processing skip individual versions rather than failing the entire pipeline (with `eprint()` logging).

---

## Pipeline Architecture

### Data Flow Diagram

```
┌──────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│  Upstream APIs   │     │  upstream/ repo   │     │  launcher/ repo  │
│  (Mojang, Forge, │────►│  (raw JSON, JARs, │────►│  (MetaVersion    │
│   NeoForge, etc.)│     │   manifests)      │     │   JSON files)    │
└──────────────────┘     └──────────────────┘     └──────────────────┘
     Phase 1: UPDATE          Intermediate           Phase 2: GENERATE
     (fetch + cache)          Storage                 (transform + write)
```

### Phase 1: Update Pipeline

Each `update_*` module follows this pattern:

1. **Fetch index**: GET the version manifest or release list from the upstream API.
2. **Diff against local**: Compare remote version list with what's already in `upstream/`.
3. **Download new/changed**: Fetch only what's new or modified.
4. **Extract install data**: For Forge/NeoForge, extract `install_profile.json` and `version.json` from installer JARs.
5. **Write to upstream**: Serialize all data to `upstream/<loader>/`.

### Phase 2: Generate Pipeline

Each `generate_*` module follows this pattern:

1. **Load upstream data**: Parse the raw data from `upstream/`.
2. **Transform**: Convert upstream-specific models into `MetaVersion` objects.
3. **Apply patches**: Merge library patches, override legacy data, fix known issues.
4. **Write to launcher**: Serialize `MetaVersion` and `MetaPackage` JSON into `launcher/<component_uid>/`.

### Index Building

The `index.py` module (Phase 2, final step):

1. Walks every directory in `launcher/`.
2. Reads each `package.json` to get package metadata.
3. For each version file, computes SHA-256 and creates a `MetaVersionIndexEntry`.
4. Sorts versions by `release_time` (descending).
5. Writes per-package `index.json` files.
6. Produces the master `index.json` with SHA-256 hashes of each package index.

```python
# From index.py — the core indexing logic:
for package in sorted(os.listdir(LAUNCHER_DIR)):
    sharedData = MetaPackage.parse_file(package_json_path)
    versionList = MetaVersionIndex(uid=package, name=sharedData.name)

    for filename in os.listdir(package_path):
        filehash = file_hash(filepath, hashlib.sha256)
        versionFile = MetaVersion.parse_file(filepath)
        is_recommended = versionFile.version in recommendedVersions
        versionEntry = MetaVersionIndexEntry.from_meta_version(
            versionFile, is_recommended, filehash
        )
        versionList.versions.append(versionEntry)

    versionList.versions = sorted(
        versionList.versions, key=attrgetter("release_time"), reverse=True
    )
    versionList.write(outFilePath)
```

---

## Component Dependency Graph

Components declare dependencies via the `requires` and `conflicts` fields:

```
net.minecraft
├── requires: org.lwjgl (or org.lwjgl3)
│
net.minecraftforge
├── requires: net.minecraft (equals=<mc_version>)
│
net.neoforged
├── requires: net.minecraft (equals=<mc_version>)
│
net.fabricmc.fabric-loader
├── requires: net.fabricmc.intermediary
│
net.fabricmc.intermediary
├── requires: net.minecraft (equals=<mc_version>)
│
org.quiltmc.quilt-loader
├── requires: net.fabricmc.intermediary (or org.quiltmc.hashed)
│
org.lwjgl ◄──conflicts──► org.lwjgl3
```

---

## Version Ordering

The `order` field controls in what sequence the launcher processes components:

| Order | Component |
|---|---|
| -2 | `net.minecraft` |
| -1 | `org.lwjgl` / `org.lwjgl3` |
| 5 | `net.minecraftforge` / `net.neoforged` |
| 10 | `net.fabricmc.fabric-loader` / `org.quiltmc.quilt-loader` |
| 11 | `net.fabricmc.intermediary` |

Lower order = loaded first. Minecraft is always base, LWJGL provides native libraries, then mod loaders layer on top.

---

## Library Patching System

The `LibraryPatches` system (`model/mojang.py`) allows surgically modifying or extending libraries in generated Minecraft versions:

```python
class LibraryPatch(MetaBase):
    match: List[GradleSpecifier]       # Which libraries to match
    override: Optional[Library]         # Fields to merge into matched lib
    additionalLibraries: Optional[List[Library]]  # Extra libs to add
    patchAdditionalLibraries: bool = False          # Recurse on additions?
```

The `patch_library()` function in `generate_mojang.py` applies patches:

```python
def patch_library(lib: Library, patches: LibraryPatches) -> List[Library]:
    to_patch = [lib]
    new_libraries = []
    while to_patch:
        target = to_patch.pop(0)
        for patch in patches:
            if patch.applies(target):
                if patch.override:
                    target.merge(patch.override)
                if patch.additionalLibraries:
                    additional_copy = copy.deepcopy(patch.additionalLibraries)
                    new_libraries += list(dict.fromkeys(additional_copy))
                    if patch.patchAdditionalLibraries:
                        to_patch += additional_copy
    return new_libraries
```

This system is used to inject missing ARM64 natives, add supplementary libraries, and fix broken upstream metadata.

---

## LWJGL Version Selection

One of the most complex parts of `generate_mojang.py` is LWJGL version deduplication. Mojang's version manifests include LWJGL libraries inline with every Minecraft version, but the launcher manages LWJGL as a separate component. Meta must:

1. **Extract** LWJGL libraries from each Minecraft version.
2. **Group** them into "variants" by hashing the library set (excluding release time).
3. **Select** the correct variant using curated lists (`PASS_VARIANTS` and `BAD_VARIANTS`).
4. **Write** each unique LWJGL version as its own component file.

```python
PASS_VARIANTS = [
    "1fd0e4d1f0f7c97e8765a69d38225e1f27ee14ef",  # 3.4.1
    "2b00f31688148fc95dbc8c8ef37308942cf0dce0",  # 3.3.6
    ...
]

BAD_VARIANTS = [
    "6442fc475f501fbd0fc4244fd1c38c02d9ebaf7e",  # 3.3.3 (broken freetype)
    ...
]
```

Each LWJGL variant is identified by a SHA-1 hash of its serialized library list. Only variants in `PASS_VARIANTS` are accepted; those in `BAD_VARIANTS` are rejected; unknown variants raise an exception.

---

## Split Natives Workaround

Modern Minecraft versions (1.19+) use "split natives" — native libraries are separate Maven artifacts with classifiers like `natives-linux`, `natives-windows`, etc. The launcher has a bug handling these, so Meta applies a workaround:

```python
APPLY_SPLIT_NATIVES_WORKAROUND = True

if APPLY_SPLIT_NATIVES_WORKAROUND and lib_is_split_native(lib):
    specifier.artifact += f"-{specifier.classifier}"
    specifier.classifier = None
```

This merges the classifier into the artifact name, effectively renaming `lwjgl:3.3.3:natives-linux` to `lwjgl-natives-linux:3.3.3`.

---

## ForgeWrapper Integration

Modern Forge (post-1.12.2) uses an installer-based system that runs processors at install time. The launcher cannot run these processors directly, so Meta injects ForgeWrapper as the main class:

```python
v.main_class = "io.github.zekerzhayard.forgewrapper.installer.Main"
```

ForgeWrapper runs the Forge installer's processors transparently when the game is first launched. The installer JAR itself is included under `mavenFiles` so the launcher downloads it alongside regular libraries.

---

## Error Recovery and Resilience

The pipeline is designed to be resumable and fault-tolerant:

1. **Incremental updates**: Only new or changed versions are downloaded.
2. **SHA-1 verification**: Installer JARs are re-downloaded if their SHA-1 changes.
3. **Cached intermediates**: Installer profiles and manifests are cached to disk.
4. **Git reset on failure**: `update.sh` runs `git reset --hard HEAD` on the upstream/launcher repos before starting, and again on failure via `fail_in()`/`fail_out()`.
5. **Per-version error handling**: A failing version logs an error but doesn't abort the entire pipeline.
