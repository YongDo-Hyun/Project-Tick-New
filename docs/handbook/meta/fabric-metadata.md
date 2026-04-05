# Meta — Fabric Metadata

## Overview

Fabric is a lightweight modding toolchain with a simpler metadata structure than Forge. Fabric has two components that Meta tracks:

1. **Fabric Loader** — the mod loading framework itself
2. **Intermediary Mappings** — obfuscation mappings that allow mods to work across Minecraft versions

The processing is straightforward compared to Forge because Fabric publishes structured JSON metadata directly, with no need to download and extract installer JARs.

---

## Phase 1: Update — `update_fabric.py`

### Fetching Component Metadata

Fabric Meta v2 exposes version lists at:

```python
for component in ["intermediary", "loader"]:
    index = get_json_file(
        os.path.join(UPSTREAM_DIR, META_DIR, f"{component}.json"),
        "https://meta.fabricmc.net/v2/versions/" + component,
    )
```

This fetches two files:
- `upstream/fabric/meta-v2/loader.json` — list of loader versions
- `upstream/fabric/meta-v2/intermediary.json` — list of intermediary mappings

Each entry contains:
```json
{
    "separator": ".",
    "build": 1,
    "maven": "net.fabricmc:fabric-loader:0.16.9",
    "version": "0.16.9",
    "stable": true
}
```

### JAR Timestamp Extraction

For each loader and intermediary version, the updater determines the release timestamp. It tries an efficient HTTP HEAD first, falling back to downloading the JAR:

```python
def compute_jar_file(path, url):
    try:
        headers = head_file(url)
        tstamp = datetime.strptime(headers["Last-Modified"], DATETIME_FORMAT_HTTP)
    except requests.HTTPError:
        print(f"Falling back to downloading jar for {url}")
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

The `DATETIME_FORMAT_HTTP` is `"%a, %d %b %Y %H:%M:%S %Z"`.

The result is saved as a `FabricJarInfo` JSON:

```python
class FabricJarInfo(MetaBase):
    release_time: Optional[datetime] = Field(alias="releaseTime")
```

### Loader Installer JSON

For each loader version, the updater downloads the installer JSON from Fabric's Maven:

```python
def get_json_file_concurrent(it):
    maven_url = get_maven_url(it["maven"], "https://maven.fabricmc.net/", ".json")
    get_json_file(
        os.path.join(UPSTREAM_DIR, INSTALLER_INFO_DIR, f"{it['version']}.json"),
        maven_url,
    )
```

The `get_maven_url()` function constructs Maven repository URLs:

```python
def get_maven_url(maven_key, server, ext):
    parts = maven_key.split(":", 3)
    maven_ver_url = (
        server + parts[0].replace(".", "/") + "/" + parts[1] + "/" + parts[2] + "/"
    )
    maven_url = maven_ver_url + parts[1] + "-" + parts[2] + ext
    return maven_url
```

For `net.fabricmc:fabric-loader:0.16.9`, this produces:
`https://maven.fabricmc.net/net/fabricmc/fabric-loader/0.16.9/fabric-loader-0.16.9.json`

### Concurrency

The Fabric updater uniquely uses `multiprocessing.Pool` (not `ThreadPoolExecutor`):

```python
with Pool(None) as pool:
    deque(pool.imap_unordered(compute_jar_file_concurrent, index, 32), 0)
```

The `deque(..., 0)` pattern consumes the iterator without storing results, purely for side effects.

---

## Phase 2: Generate — `generate_fabric.py`

### Processing Loader Versions

```python
def process_loader_version(entry) -> MetaVersion:
    jar_info = load_jar_info(transform_maven_key(entry["maven"]))
    installer_info = load_installer_info(entry["version"])

    v = MetaVersion(
        name="Fabric Loader",
        uid="net.fabricmc.fabric-loader",
        version=entry["version"]
    )
    v.release_time = jar_info.release_time
    v.requires = [Dependency(uid="net.fabricmc.intermediary")]
    v.order = 10
    v.type = "release"
```

#### Main Class Resolution

The loader installer info may specify the main class as either a string or a `FabricMainClasses` object:

```python
if isinstance(installer_info.main_class, FabricMainClasses):
    v.main_class = installer_info.main_class.client
else:
    v.main_class = installer_info.main_class
```

The `FabricMainClasses` model:
```python
class FabricMainClasses(MetaBase):
    client: Optional[str]
    common: Optional[str]
    server: Optional[str]
```

#### Library Assembly

Loader libraries come from the installer's `common` and `client` sections, plus the loader itself:

```python
v.libraries = []
v.libraries.extend(installer_info.libraries.common)
v.libraries.extend(installer_info.libraries.client)
loader_lib = Library(
    name=GradleSpecifier.from_string(entry["maven"]),
    url="https://maven.fabricmc.net",
)
v.libraries.append(loader_lib)
```

### Processing Intermediary Versions

```python
def process_intermediary_version(entry) -> MetaVersion:
    jar_info = load_jar_info(transform_maven_key(entry["maven"]))

    v = MetaVersion(
        name="Intermediary Mappings",
        uid="net.fabricmc.intermediary",
        version=entry["version"],
    )
    v.release_time = jar_info.release_time
    v.requires = [Dependency(uid="net.minecraft", equals=entry["version"])]
    v.order = 11
    v.type = "release"
    v.volatile = True
    intermediary_lib = Library(
        name=GradleSpecifier.from_string(entry["maven"]),
        url="https://maven.fabricmc.net",
    )
    v.libraries = [intermediary_lib]
    return v
```

Key points:
- Intermediary mappings are `volatile=True` — they may change between runs.
- The `version` matches the Minecraft version (e.g., `1.21.5`).
- The `requires` field pins the intermediary to an exact Minecraft version via `equals`.

### Recommended Versions

Fabric Meta has a `stable` field in its loader index. The **first** stable loader version is recommended:

```python
for entry in loader_version_index:
    v = process_loader_version(entry)
    if not recommended_loader_versions and entry["stable"]:
        recommended_loader_versions.append(version)
```

All intermediary versions are recommended (since each maps to exactly one Minecraft version).

### Package Metadata

```python
package = MetaPackage(uid=LOADER_COMPONENT, name="Fabric Loader")
package.recommended = recommended_loader_versions
package.description = "Fabric Loader is a tool to load Fabric-compatible mods in game environments."
package.project_url = "https://fabricmc.net"
package.authors = ["Fabric Developers"]

package = MetaPackage(uid=INTERMEDIARY_COMPONENT, name="Intermediary Mappings")
package.recommended = recommended_intermediary_versions
package.description = "Intermediary mappings allow using Fabric Loader with mods for Minecraft in a more compatible manner."
package.project_url = "https://fabricmc.net"
package.authors = ["Fabric Developers"]
```

---

## Data Models

### `FabricInstallerDataV1`

The installer JSON from Fabric's Maven:

```python
class FabricInstallerDataV1(MetaBase):
    version: int
    libraries: FabricInstallerLibraries
    main_class: Optional[Union[str, FabricMainClasses]]
    arguments: Optional[FabricInstallerArguments]
    launchwrapper: Optional[FabricInstallerLaunchwrapper]

class FabricInstallerLibraries(MetaBase):
    client: Optional[List[Library]]
    common: Optional[List[Library]]
    server: Optional[List[Library]]

class FabricInstallerArguments(MetaBase):
    client: Optional[List[str]]
    common: Optional[List[str]]
    server: Optional[List[str]]
```

### `FabricJarInfo`

```python
class FabricJarInfo(MetaBase):
    release_time: Optional[datetime] = Field(alias="releaseTime")
```

---

## Constants

| Constant | Value | Location |
|---|---|---|
| `LOADER_COMPONENT` | `"net.fabricmc.fabric-loader"` | `common/fabric.py` |
| `INTERMEDIARY_COMPONENT` | `"net.fabricmc.intermediary"` | `common/fabric.py` |
| `BASE_DIR` | `"fabric"` | `common/fabric.py` |
| `META_DIR` | `"fabric/meta-v2"` | `common/fabric.py` |
| `INSTALLER_INFO_DIR` | `"fabric/loader-installer-json"` | `common/fabric.py` |
| `JARS_DIR` | `"fabric/jars"` | `common/fabric.py` |
| `DATETIME_FORMAT_HTTP` | `"%a, %d %b %Y %H:%M:%S %Z"` | `common/fabric.py` |

---

## Component Dependency Chain

```
org.quiltmc.quilt-loader ──► net.fabricmc.intermediary ──► net.minecraft
net.fabricmc.fabric-loader ──► net.fabricmc.intermediary ──► net.minecraft
```

Fabric Loader requires Intermediary Mappings, which require a specific Minecraft version.

---

## Output Structure

```
launcher/
├── net.fabricmc.fabric-loader/
│   ├── package.json
│   ├── 0.16.9.json
│   ├── 0.16.8.json
│   └── ...
└── net.fabricmc.intermediary/
    ├── package.json
    ├── 1.21.5.json
    ├── 1.20.4.json
    └── ...
```

---

## Upstream Data Structure

```
upstream/fabric/
├── meta-v2/
│   ├── loader.json          # Full loader version index
│   └── intermediary.json    # Full intermediary version index
├── loader-installer-json/
│   ├── 0.16.9.json          # Installer JSON per loader version
│   └── ...
└── jars/
    ├── net.fabricmc.fabric-loader.0.16.9.json  # JAR timestamp info
    ├── net.fabricmc.intermediary.1.21.5.json
    └── ...
```
