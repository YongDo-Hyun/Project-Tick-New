# Meta — Data Models

## Overview

Meta uses **Pydantic v1** (`pydantic==1.10.13`) for all data models. Every model inherits from `MetaBase`, which standardizes JSON serialization and provides merge semantics. The models serve two purposes:

1. **Upstream models** — parse vendor API responses (Mojang, Forge, Fabric, Adoptium, Azul, etc.)
2. **Output models** — produce launcher-compatible JSON (`MetaVersion`, `MetaPackage`, `JavaRuntimeVersion`)

---

## Core Base Classes

### `MetaBase`

The root class for all Meta models:

```python
class MetaBase(pydantic.BaseModel):
    def dict(self, **kwargs):
        return super().dict(by_alias=True, **kwargs)

    def json(self, **kwargs):
        return super().json(
            exclude_none=True, sort_keys=True,
            by_alias=True, indent=4, **kwargs,
        )

    def write(self, file_path: str):
        Path(file_path).parent.mkdir(parents=True, exist_ok=True)
        with open(file_path, "w") as f:
            f.write(self.json())

    class Config:
        allow_population_by_field_name = True
        json_encoders = {
            datetime: serialize_datetime,
            GradleSpecifier: str,
        }
```

Key design decisions:
- **`by_alias=True`** always — JSON keys use aliases (camelCase) rather than Python field names (snake_case).
- **`exclude_none=True`** — `None` fields are omitted from JSON output, keeping files compact.
- **`sort_keys=True`** — deterministic key ordering enables meaningful git diffs.
- **`indent=4`** — human-readable output.
- **`allow_population_by_field_name=True`** — allows constructing models using either the Python name or the alias.
- **`write()`** — creates parent directories automatically.

### `MetaBase.merge()`

Deep merge of two model instances:

```python
def merge(self, other: "MetaBase"):
    assert type(other) is type(self)
    for key, field in self.__fields__.items():
        ours = getattr(self, key)
        theirs = getattr(other, key)
        if theirs is None:
            continue
        if ours is None:
            setattr(self, key, theirs)
            continue
        if isinstance(ours, list):
            ours += theirs          # concatenate lists
        elif isinstance(ours, set):
            ours |= theirs          # union sets
        elif isinstance(ours, dict):
            result = merge_dict(ours, copy.deepcopy(theirs))  # deep merge dicts
            setattr(self, key, result)
        elif MetaBase in get_all_bases(field.type_):
            ours.merge(theirs)      # recursive merge for nested MetaBase
        else:
            setattr(self, key, theirs)  # overwrite scalars
```

Used in `generate_mojang.py` when merging LWJGL component data across versions.

### `Versioned`

Adds format versioning:

```python
META_FORMAT_VERSION = 1

class Versioned(MetaBase):
    @validator("format_version")
    def format_version_must_be_supported(cls, v: int):
        assert v <= META_FORMAT_VERSION
        return v

    format_version: int = Field(META_FORMAT_VERSION, alias="formatVersion")
```

All output files include `"formatVersion": 1`.

---

## `GradleSpecifier`

A Maven coordinate parser, not a Pydantic model but integrated with Pydantic via `__get_validators__`:

```python
class GradleSpecifier:
    def __init__(self, group, artifact, version, classifier=None, extension=None):
        if extension is None:
            extension = "jar"
        self.group = group
        self.artifact = artifact
        self.version = version
        self.classifier = classifier
        self.extension = extension
```

### Parsing

```python
@classmethod
def from_string(cls, v: str):
    ext_split = v.split("@")
    components = ext_split[0].split(":")
    group = components[0]
    artifact = components[1]
    version = components[2]
    extension = ext_split[1] if len(ext_split) == 2 else None
    classifier = components[3] if len(components) == 4 else None
    return cls(group, artifact, version, classifier, extension)
```

Examples:
- `"org.lwjgl.lwjgl:lwjgl:2.9.0"` → `group="org.lwjgl.lwjgl"`, `artifact="lwjgl"`, `version="2.9.0"`
- `"net.minecraft:client:1.20.4:slim@jar"` → includes classifier `"slim"` and explicit extension `"jar"`

### Path Generation

```python
def base(self):
    return "%s/%s/%s/" % (self.group.replace(".", "/"), self.artifact, self.version)

def filename(self):
    if self.classifier:
        return "%s-%s-%s.%s" % (self.artifact, self.version, self.classifier, self.extension)
    else:
        return "%s-%s.%s" % (self.artifact, self.version, self.extension)

def path(self):
    return self.base() + self.filename()
```

`path()` produces Maven-standard paths like `org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar`.

### Classification Helpers

```python
def is_lwjgl(self):
    return self.group in (
        "org.lwjgl", "org.lwjgl.lwjgl",
        "net.java.jinput", "net.java.jutils",
    )

def is_log4j(self):
    return self.group == "org.apache.logging.log4j"
```

Used by `generate_mojang.py` to separate LWJGL into its own component and to apply Log4j security patches.

---

## Output Models

### `MetaVersion`

The primary output format for every component version:

```python
class MetaVersion(Versioned):
    name: str                                           # Human-readable name
    version: str                                        # Version string
    uid: str                                            # Component UID
    type: Optional[str]                                 # "release", "snapshot", "old_alpha", "old_beta", "experiment"
    order: Optional[int]                                # Sort priority (lower = applied first)
    volatile: Optional[bool]                            # May change between runs
    requires: Optional[List[Dependency]]               # Component dependencies
    conflicts: Optional[List[Dependency]]              # Incompatible components
    libraries: Optional[List[Library]]                  # Java libraries
    asset_index: Optional[MojangAssets]                 # Asset manifest (alias: assetIndex)
    maven_files: Optional[List[Library]]                # Extra Maven artifacts (alias: mavenFiles)
    main_jar: Optional[Library]                         # Primary game JAR (alias: mainJar)
    jar_mods: Optional[List[Library]]                   # Legacy jar mods (alias: jarMods)
    main_class: Optional[str]                           # JVM entry point (alias: mainClass)
    applet_class: Optional[str]                         # Legacy applet class (alias: appletClass)
    minecraft_arguments: Optional[str]                  # Legacy MC arguments (alias: minecraftArguments)
    release_time: Optional[datetime]                    # Release timestamp (alias: releaseTime)
    compatible_java_majors: Optional[List[int]]        # e.g., [17, 21] (alias: compatibleJavaMajors)
    compatible_java_name: Optional[str]                # Mojang Java component name (alias: compatibleJavaName)
    java_agents: Optional[List[JavaAgent]]              # e.g., Log4j patcher (alias: +agents)
    additional_traits: Optional[List[str]]              # e.g., "noapplet" (alias: +traits)
    additional_tweakers: Optional[List[str]]            # Legacy FML tweakers (alias: +tweakers)
    additional_jvm_args: Optional[List[str]]            # Extra JVM flags (alias: +jvmArgs)
    logging: Optional[MojangLogging]                    # Log4j configuration
```

### `MetaPackage`

Package-level metadata:

```python
class MetaPackage(Versioned):
    name: str
    uid: str
    recommended: Optional[List[str]]
    authors: Optional[List[str]]
    description: Optional[str]
    project_url: Optional[str] = Field(alias="projectUrl")
```

### `Dependency`

Component dependency specification:

```python
class Dependency(MetaBase):
    uid: str                   # Required component UID
    equals: Optional[str]      # Exact version match
    suggests: Optional[str]    # Suggested version (not enforced)
```

Examples:
- Fabric Intermediary requires exact MC version: `Dependency(uid="net.minecraft", equals="1.21.5")`
- Fabric Loader requires any intermediary: `Dependency(uid="net.fabricmc.intermediary")`
- Forge requires a specific MC version: `Dependency(uid="net.minecraft", equals="1.20.4")`

### `Library`

A Java library dependency:

```python
class Library(MetaBase):
    extract: Optional[MojangLibraryExtractRules]
    name: Optional[GradleSpecifier]
    downloads: Optional[MojangLibraryDownloads]
    natives: Optional[Dict[str, str]]
    rules: Optional[MojangRules]
    url: Optional[str]                    # Maven repository URL
    mmcHint: Optional[str] = Field(None, alias="MMC-hint")
```

### `JavaAgent`

Extends `Library` with a JVM argument:

```python
class JavaAgent(Library):
    argument: Optional[str]
```

Used for the Log4j patcher agent.

---

## Mojang-Specific Models

### `MojangArtifactBase` / `MojangArtifact`

```python
class MojangArtifactBase(MetaBase):
    sha1: Optional[str]
    sha256: Optional[str]
    size: Optional[int]
    url: str

class MojangArtifact(MojangArtifactBase):
    path: Optional[str]
```

### `MojangLibraryDownloads`

```python
class MojangLibraryDownloads(MetaBase):
    artifact: Optional[MojangArtifact]
    classifiers: Optional[Dict[Any, MojangArtifact]]
```

### `MojangRules`

OS-specific allow/disallow rules for libraries:

```python
class MojangRule(MetaBase):
    action: str       # "allow" or "disallow"
    os: Optional[OSRule]

class OSRule(MetaBase):
    name: str         # "osx", "linux", "windows", "windows-arm64", etc.
    version: Optional[str]

class MojangRules(MetaBase):
    __root__: List[MojangRule]
```

### `MojangVersion` (in `model/mojang.py`)

The full Mojang version manifest, containing the raw version JSON. Key fields:

```python
class MojangVersion(MetaBase):
    id: str
    type: str
    main_class: str = Field(alias="mainClass")
    minecraft_arguments: Optional[str] = Field(alias="minecraftArguments")
    arguments: Optional[MojangArguments]
    release_time: datetime = Field(alias="releaseTime")
    libraries: List[Library]
    downloads: MojangVersionDownloads
    asset_index: Optional[MojangAssets] = Field(alias="assetIndex")
    java_version: Optional[MojangJavaVersion] = Field(alias="javaVersion")
    logging: Optional[Dict[str, MojangLogging]]
    compliance_level: Optional[int] = Field(alias="complianceLevel")
```

### `MojangIndex`

The version manifest index:

```python
class MojangIndexEntry(MetaBase):
    id: str
    type: str
    url: str
    time: datetime
    release_time: datetime = Field(alias="releaseTime")
    sha1: str

class MojangIndex(MetaBase):
    latest: Dict[str, str]
    versions: List[MojangIndexEntry]

class MojangIndexWrap(MetaBase):
    versions: MojangIndex
```

### `MojangJavaComponent` (StrEnum)

```python
class MojangJavaComponent(StrEnum):
    Alpha = "java-runtime-alpha"
    Beta = "java-runtime-beta"
    Gamma = "java-runtime-gamma"
    GammaSnapshot = "java-runtime-gamma-snapshot"
    Delta = "java-runtime-delta"
    JreLegacy = "jre-legacy"
    Exe = "minecraft-java-exe"
```

Uses a `_missing_()` override to accept unknown future component names.

---

## Forge Models (in `model/forge.py`)

### `ForgeEntry` / `DerivedForgeIndex`

```python
class ForgeFile(MetaBase):
    classifier: str      # "installer", "client", "universal", etc.
    hash: Optional[str]
    extension: str       # "jar", "zip"

class ForgeEntry(MetaBase):
    long_version: str = Field(alias="longversion")
    mc_version: str = Field(alias="mcversion")
    version: str
    build: int
    branch: Optional[str]
    latest: Optional[bool]
    recommended: Optional[bool]
    files: Optional[List[ForgeFile]]

class DerivedForgeIndex(MetaBase):
    versions: Dict[str, ForgeEntry]
    by_mc_version: Dict[str, List[str]] = Field(alias="by_mcversion")
```

### `ForgeVersion`

Post-processed Forge version with installer data:

```python
class ForgeVersion(MetaBase):
    long_version: str = Field(alias="longversion")
    mc_version: str = Field(alias="mcversion")
    version: str
    build: int
    branch: Optional[str]
    installer_filename: Optional[str]
    installer_url: Optional[str]
    
    def uses_installer(self) -> bool:
        # True for MC >= 1.6
        
    def is_supported(self) -> bool:
        # Checks if version can be processed
```

### `ForgeInstallerProfileV2`

Modern Forge installer profiles (MC 1.13+):

```python
class ForgeInstallerProfileV2(MetaBase):
    spec: Optional[int]
    profile: Optional[str]
    version: Optional[str]
    icon: Optional[str]
    json: Optional[str]
    minecraft: str
    data: Optional[Dict[str, ForgeProfileData]]
    processors: Optional[List[ForgeProcessor]]
    libraries: List[Library]
```

---

## NeoForge Models (in `model/neoforge.py`)

Similar to Forge but with additions:

```python
class NeoForgeFile(MetaBase):
    classifier: str
    hash: Optional[str]
    extension: str
    artifact: Optional[str]     # Additional field for NeoForge

class NeoForgeInstallerProfileV2(MetaBase):
    # Same structure as ForgeInstallerProfileV2
    minecraft: str
    libraries: List[Library]
```

---

## Fabric Models (in `model/fabric.py`)

```python
class FabricMainClasses(MetaBase):
    client: Optional[str]
    common: Optional[str]
    server: Optional[str]

class FabricInstallerArguments(MetaBase):
    client: Optional[List[str]]
    common: Optional[List[str]]
    server: Optional[List[str]]

class FabricInstallerLibraries(MetaBase):
    client: Optional[List[Library]]
    common: Optional[List[Library]]
    server: Optional[List[Library]]

class FabricInstallerDataV1(MetaBase):
    version: int
    libraries: FabricInstallerLibraries
    main_class: Optional[Union[str, FabricMainClasses]]
    arguments: Optional[FabricInstallerArguments]
    launchwrapper: Optional[FabricInstallerLaunchwrapper]

class FabricJarInfo(MetaBase):
    release_time: Optional[datetime] = Field(alias="releaseTime")
```

---

## Index Models (in `model/index.py`)

### `MetaVersionIndexEntry`

Per-version index entry with hash:

```python
class MetaVersionIndexEntry(MetaBase):
    version: str
    type: Optional[str]
    release_time: Optional[datetime] = Field(alias="releaseTime")
    sha256: str
    requires: Optional[List[Dependency]]
    recommended: Optional[bool]
    volatile: Optional[bool]

    @classmethod
    def from_meta_version(cls, meta_version: MetaVersion, sha256: str):
        # Factory method to create from MetaVersion + computed SHA-256
```

### `MetaVersionIndex`

Per-package version list:

```python
class MetaVersionIndex(Versioned):
    name: Optional[str]
    uid: str
    description: Optional[str]
    project_url: Optional[str] = Field(alias="projectUrl")
    authors: Optional[List[str]]
    versions: List[MetaVersionIndexEntry]
```

### `MetaPackageIndex`

Master index of all packages:

```python
class MetaPackageIndexEntry(MetaBase):
    name: Optional[str]
    uid: str
    sha256: str

class MetaPackageIndex(Versioned):
    packages: List[MetaPackageIndexEntry]
```

---

## Helper Functions

### `make_launcher_library()`

Creates a `Library` with a pre-computed artifact URL pointing at the launcher Maven:

```python
LAUNCHER_MAVEN = "https://files.projecttick.org/maven/%s"

def make_launcher_library(name: GradleSpecifier, hash: str, size: int, maven=LAUNCHER_MAVEN):
    artifact = MojangArtifact(url=maven % name.path(), sha1=hash, size=size)
    return Library(name=name, downloads=MojangLibraryDownloads(artifact=artifact))
```

### `serialize_datetime()`

Custom datetime serializer used by Pydantic's `json_encoders`:

```python
def serialize_datetime(dt: datetime) -> str:
    return dt.strftime("%Y-%m-%dT%H:%M:%S+00:00")
```

All timestamps use UTC with a fixed `+00:00` offset.

---

## Model Hierarchy

```
pydantic.BaseModel
└── MetaBase
    ├── Versioned
    │   ├── MetaVersion
    │   │   └── JavaRuntimeVersion
    │   ├── MetaPackage
    │   └── MetaVersionIndex
    ├── GradleSpecifier (not a BaseModel, but Pydantic-integrated)
    ├── Library
    │   └── JavaAgent
    ├── Dependency
    ├── MojangArtifactBase
    │   ├── MojangArtifact
    │   ├── MojangAssets
    │   └── MojangLoggingArtifact
    ├── MojangVersion
    ├── ForgeEntry / NeoForgeEntry
    ├── ForgeVersion / NeoForgeVersion
    ├── FabricInstallerDataV1
    ├── JavaRuntimeMeta
    ├── JavaVersionMeta
    ├── AdoptxRelease / AdoptxBinary
    ├── ZuluPackageDetail
    └── APIQuery
        ├── AdoptxAPIFeatureReleasesQuery
        └── AzulApiPackagesQuery
```
