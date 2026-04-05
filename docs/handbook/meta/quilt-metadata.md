# Meta — Quilt Metadata

## Overview

Quilt is a fork of Fabric that reuses Fabric's Intermediary mappings but has its own loader and mod system. Meta tracks Quilt as two components:

1. **Quilt Loader** — `org.quiltmc.quilt-loader`
2. **Intermediary Mappings** — reuses Fabric's `net.fabricmc.intermediary`

Quilt's metadata pipeline closely mirrors Fabric's, using nearly identical data models (`FabricInstallerDataV1`, `FabricJarInfo`), but with distinct quirks around beacon telemetry disabling and version recommendation logic.

---

## Phase 1: Update — `update_quilt.py`

### Fetching Component Metadata

Quilt Meta v3 provides version lists:

```python
for component in ["quilt-loader"]:
    index = get_json_file(
        os.path.join(UPSTREAM_DIR, META_DIR, f"{component}.json"),
        f"https://meta.quiltmc.org/v3/versions/{component}",
    )
```

Unlike Fabric which tracks two components, Quilt only fetches loader versions from Quilt Meta. Intermediary is shared with Fabric.

### Hashed Mappings Decision

```python
USE_QUILT_MAPPINGS = False
```

When `USE_QUILT_MAPPINGS` is `False` (the current default), Quilt uses Fabric's Intermediary mappings rather than Quilt's own hashed mappings. This ensures broader mod compatibility.

If `USE_QUILT_MAPPINGS` were `True`, the updater would also fetch:
```python
if USE_QUILT_MAPPINGS:
    components.append("quilt-mappings")
```

And use `https://maven.quiltmc.org/repository/release/` as the Maven source.

### JAR Timestamp Extraction

Quilt **always downloads the full JAR** to extract timestamps, unlike Fabric which tries HEAD requests first:

```python
def compute_jar_file(path, url):
    jar_path = path + ".jar"
    get_binary_file(jar_path, url)
    tstamp = datetime.fromtimestamp(0)
    with zipfile.ZipFile(jar_path) as jar:
        allinfo = jar.infolist()
        for info in allinfo:
            tstamp_new = datetime(*info.date_time)
            if tstamp_new > tstamp:
                tstamp = tstamp_new
    data = FabricJarInfo(release_time=tstamp)
    data.write(path + ".json")
```

This is because Quilt's Maven HEAD responses were historically unreliable for `Last-Modified` headers.

### Loader Installer JSON

```python
def get_json_file_concurrent(it):
    maven_url = get_maven_url(
        it["maven"], "https://maven.quiltmc.org/repository/release/", ".json"
    )
    get_json_file(
        os.path.join(UPSTREAM_DIR, INSTALLER_INFO_DIR, f"{it['version']}.json"),
        maven_url,
    )
```

### Concurrency Model

Quilt uses `ThreadPoolExecutor` (not `multiprocessing.Pool` like Fabric):

```python
with ThreadPoolExecutor() as pool:
    deque(pool.map(compute_jar_file_concurrent, index, chunksize=32), 0)
```

---

## Phase 2: Generate — `generate_quilt.py`

### Processing Loader Versions

```python
def process_loader_version(entry) -> MetaVersion:
    jar_info = load_jar_info(transform_maven_key(entry["maven"]))
    installer_info = load_installer_info(entry["version"])

    v = MetaVersion(
        name="Quilt Loader",
        uid=LOADER_COMPONENT,
        version=entry["version"],
    )
    v.release_time = jar_info.release_time
    v.requires = [Dependency(uid=INTERMEDIARY_COMPONENT)]
    v.order = 10
    v.type = "release"
```

#### Main Class Handling

Identical to Fabric — the main class can be a string or a `FabricMainClasses` object:

```python
if isinstance(installer_info.main_class, FabricMainClasses):
    v.main_class = installer_info.main_class.client
else:
    v.main_class = installer_info.main_class
```

#### Library Assembly

```python
v.libraries = []
v.libraries.extend(installer_info.libraries.common)
v.libraries.extend(installer_info.libraries.client)
loader_lib = Library(
    name=GradleSpecifier.from_string(entry["maven"]),
    url="https://maven.quiltmc.org/repository/release/",
)
v.libraries.append(loader_lib)
```

#### Beacon Telemetry Disabling

A unique Quilt feature: certain loader versions have opt-out telemetry. Meta disables it:

```python
DISABLE_BEACON_VERSIONS = {
    "0.17.0",
    "0.17.1",
    # ... etc
}
DISABLE_BEACON_ARG = "-Dloader.disable_beacon=true"
```

Applied during generation:

```python
if entry["version"] in DISABLE_BEACON_VERSIONS:
    if v.additional_jvm_args is None:
        v.additional_jvm_args = []
    v.additional_jvm_args.append(DISABLE_BEACON_ARG)
```

### Processing Intermediary / Hashed Mappings

When using Fabric's intermediary (default):

```python
INTERMEDIARY_COMPONENT = "net.fabricmc.intermediary"
```

This means Quilt Loader's `requires` field references `net.fabricmc.intermediary`, and the intermediary versions are handled entirely by Fabric's generate pipeline.

If `USE_QUILT_MAPPINGS` were `True`, Quilt would generate its own hashed mappings component:

```python
if USE_QUILT_MAPPINGS:
    INTERMEDIARY_COMPONENT = "org.quiltmc.hashed"
```

### Version Recommendation Logic

Quilt uses a SemVer-based heuristic instead of Fabric's `stable` boolean:

```python
for entry in loader_version_index:
    version = entry["version"]
    # ...
    if not recommended_loader_versions and "-" not in version:
        recommended_loader_versions.append(version)
```

The `"-" not in version` check follows SemVer conventions: a version containing a hyphen (e.g., `0.18.0-beta.1`) is a pre-release. The first version **without** a hyphen becomes the recommended version.

### Package Metadata

```python
package = MetaPackage(uid=LOADER_COMPONENT, name="Quilt Loader")
package.recommended = recommended_loader_versions
package.description = "Quilt Loader is a tool to load Quilt-compatible mods in game environments."
package.project_url = "https://quiltmc.org"
package.authors = ["Quilt Developers"]
```

---

## Constants

| Constant | Value | Location |
|---|---|---|
| `LOADER_COMPONENT` | `"org.quiltmc.quilt-loader"` | `common/quilt.py` |
| `INTERMEDIARY_COMPONENT` | `"net.fabricmc.intermediary"` (default) | `common/quilt.py` |
| `USE_QUILT_MAPPINGS` | `False` | `common/quilt.py` |
| `BASE_DIR` | `"quilt"` | `common/quilt.py` |
| `META_DIR` | `"quilt/meta-v3"` | `common/quilt.py` |
| `INSTALLER_INFO_DIR` | `"quilt/loader-installer-json"` | `common/quilt.py` |
| `JARS_DIR` | `"quilt/jars"` | `common/quilt.py` |
| `DISABLE_BEACON_ARG` | `"-Dloader.disable_beacon=true"` | `common/quilt.py` |

---

## Differences from Fabric

| Aspect | Fabric | Quilt |
|---|---|---|
| **Loader UID** | `net.fabricmc.fabric-loader` | `org.quiltmc.quilt-loader` |
| **Meta API** | v2 (`meta.fabricmc.net/v2/`) | v3 (`meta.quiltmc.org/v3/`) |
| **Maven** | `maven.fabricmc.net` | `maven.quiltmc.org/repository/release/` |
| **JAR timestamps** | HEAD first, download fallback | Always download |
| **Concurrency** | `multiprocessing.Pool` | `ThreadPoolExecutor` |
| **Recommendation** | `stable` boolean field | SemVer heuristic (`-` = pre-release) |
| **Beacon disable** | N/A | Injected JVM arg for specific versions |
| **Mappings** | Own intermediary | Uses Fabric intermediary (configurable) |

---

## Component Dependency Chain

```
org.quiltmc.quilt-loader ──► net.fabricmc.intermediary ──► net.minecraft
```

Quilt Loader depends on Fabric Intermediary, which depends on Minecraft.

---

## Output Structure

```
launcher/
└── org.quiltmc.quilt-loader/
    ├── package.json
    ├── 0.27.1.json
    ├── 0.27.0.json
    └── ...
```

Quilt does not generate its own intermediary package (it reuses Fabric's).

---

## Upstream Data Structure

```
upstream/quilt/
├── meta-v3/
│   └── quilt-loader.json      # Full loader version index
├── loader-installer-json/
│   ├── 0.27.1.json            # Installer JSON per loader version
│   └── ...
└── jars/
    ├── org.quiltmc.quilt-loader.0.27.1.json  # JAR timestamp info
    └── ...
```
