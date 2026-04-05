# Meta — Mojang Metadata

## Overview

Mojang metadata processing is the foundation of the entire Meta pipeline. Every other component (Forge, Fabric, etc.) depends on the Minecraft version data produced here. The pipeline handles:

1. The Mojang version manifest (`version_manifest_v2.json`)
2. Individual version JSONs for every Minecraft release and snapshot
3. Experimental snapshots (zip-packaged)
4. Pre-launcher old snapshots
5. Mojang's Java runtime manifest
6. LWJGL library extraction and deduplication
7. Log4j vulnerability patching
8. Legacy version overrides

---

## Phase 1: Update — `update_mojang.py`

### Version Manifest Fetching

The updater starts by fetching Mojang's version manifest:

```python
r = sess.get("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json")
r.raise_for_status()
remote_versions = MojangIndexWrap(MojangIndex(**r.json()))
```

The `MojangIndex` model maps the manifest structure:

```python
class MojangIndex(MetaBase):
    latest: MojangLatestVersion    # {"release": "1.21.5", "snapshot": "25w14a"}
    versions: List[MojangIndexEntry]

class MojangIndexEntry(MetaBase):
    id: Optional[str]              # "1.21.5"
    release_time: Optional[datetime]
    time: Optional[datetime]
    type: Optional[str]            # "release", "snapshot"
    url: Optional[str]             # URL to full version JSON
    sha1: Optional[str]
    compliance_level: Optional[int]
```

The `MojangIndexWrap` class provides a dict-based lookup:

```python
class MojangIndexWrap:
    def __init__(self, index: MojangIndex):
        self.index = index
        self.latest = index.latest
        self.versions = dict((x.id, x) for x in index.versions)
```

### Incremental Updates

Only new or modified versions are fetched:

```python
if os.path.exists(version_manifest_path):
    current_versions = MojangIndexWrap(MojangIndex.parse_file(version_manifest_path))
    local_ids = set(current_versions.versions.keys())
    pending_ids = remote_ids.difference(local_ids)

    for x in local_ids:
        remote_version = remote_versions.versions[x]
        local_version = current_versions.versions[x]
        if remote_version.time > local_version.time:
            pending_ids.add(x)
else:
    pending_ids = remote_ids
```

A version is re-fetched if its `time` field changed (Mojang updates this when they modify a version).

### Concurrent Version Downloads

Individual version JSONs are downloaded in parallel:

```python
with concurrent.futures.ThreadPoolExecutor() as executor:
    futures = [
        executor.submit(fetch_version_concurrent, remote_versions, x)
        for x in pending_ids
    ]
    for f in futures:
        f.result()
```

Each version is saved to `upstream/mojang/versions/<id>.json`.

### Experimental Snapshots

Experimental snapshots (like combat test snapshots) are distributed as zip files with embedded JSON. Meta handles these via a static registry:

```python
if os.path.exists(STATIC_EXPERIMENTS_FILE):
    experiments = ExperimentIndexWrap(
        ExperimentIndex.parse_file(STATIC_EXPERIMENTS_FILE)
    )
    for x in experiment_ids:
        version = experiments.versions[x]
        if not os.path.isfile(experiment_path):
            fetch_zipped_version(experiment_path, version.url)
```

The `fetch_zipped_version()` function downloads a zip, extracts the JSON, and marks the type as `"experiment"`:

```python
def fetch_zipped_version(path, url):
    zip_path = f"{path}.zip"
    download_binary_file(sess, zip_path, url)
    with zipfile.ZipFile(zip_path) as z:
        for info in z.infolist():
            if info.filename.endswith(".json"):
                version_json = json.load(z.open(info))
                break
    version_json["type"] = "experiment"
    with open(path, "w", encoding="utf-8") as f:
        json.dump(version_json, f, sort_keys=True, indent=4)
```

### Old Snapshots

Pre-launcher snapshots that Mojang never published with proper manifests:

```python
def fetch_modified_version(path, version):
    r = sess.get(version.url)
    version_json = r.json()
    version_json["releaseTime"] = version_json["releaseTime"] + "T00:00:00+02:00"
    version_json["time"] = version_json["releaseTime"]
    downloads = {
        "client": {"url": version.jar, "sha1": version.sha1, "size": version.size}
    }
    version_json["downloads"] = downloads
    version_json["type"] = "old_snapshot"
```

These versions have manually curated JAR URLs, SHA-1 hashes, and sizes stored in `mojang-minecraft-old-snapshots.json`.

### Java Runtime Manifest

Mojang provides a manifest of Java runtimes for different platforms at a fixed URL:

```python
MOJANG_JAVA_URL = "https://piston-meta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json"

def update_javas():
    r = sess.get(MOJANG_JAVA_URL)
    remote_javas = JavaIndex(__root__=r.json())
    remote_javas.write(java_manifest_path)
```

---

## Phase 2: Generate — `generate_mojang.py`

This is the most complex generator in the pipeline. It produces:

- `net.minecraft/<version>.json` — for every Minecraft version
- `org.lwjgl/<version>.json` — for LWJGL 2 versions
- `org.lwjgl3/<version>.json` — for LWJGL 3 versions
- `net.minecraft/package.json`, `org.lwjgl/package.json`, `org.lwjgl3/package.json`

### Loading Static Data

```python
override_index = LegacyOverrideIndex.parse_file(STATIC_OVERRIDES_FILE)
legacy_services = LegacyServices.parse_file(STATIC_LEGACY_SERVICES_FILE)
library_patches = LibraryPatches.parse_file(LIBRARY_PATCHES_FILE)
```

### MojangVersion to MetaVersion Conversion

Each raw Mojang version JSON is parsed into a `MojangVersion` and then converted:

```python
mojang_version = MojangVersion.parse_file(input_file)
v = mojang_version.to_meta_version("Minecraft", MINECRAFT_COMPONENT, mojang_version.id)
```

The `to_meta_version()` method handles:

1. **Client JAR**: Extracts the client download into a `main_jar` `Library`:
   ```python
   main_jar = Library(
       name=GradleSpecifier("com.mojang", "minecraft", self.id, "client"),
       downloads=MojangLibraryDownloads(artifact=artifact),
   )
   ```

2. **Compliance level**: Maps `compliance_level=1` to the `XR:Initial` trait.

3. **Java version**: Extracts `javaVersion.majorVersion` and `javaVersion.component`, populating `compatible_java_majors` and `compatible_java_name`.

4. **Type mapping**: Converts `"pending"` to `"experiment"`.

### LWJGL Extraction

Libraries are classified as LWJGL if their `GradleSpecifier.is_lwjgl()` returns `True`:

```python
def is_lwjgl(self):
    return self.group in (
        "org.lwjgl",
        "org.lwjgl.lwjgl",
        "net.java.jinput",
        "net.java.jutils",
    )
```

LWJGL libraries are extracted into "buckets" keyed by their OS rules:

```python
def add_or_get_bucket(buckets, rules: Optional[MojangRules]) -> MetaVersion:
    rule_hash = None
    if rules:
        rule_hash = hash(rules.json())
    if rule_hash in buckets:
        bucket = buckets[rule_hash]
    else:
        bucket = MetaVersion(name="LWJGL", version="undetermined", uid=LWJGL_COMPONENT)
        bucket.type = "release"
        buckets[rule_hash] = bucket
    return bucket
```

### LWJGL Variant Deduplication

Multiple Minecraft versions may ship identical LWJGL libraries. Each unique set is identified by SHA-1:

```python
def hash_lwjgl_version(lwjgl: MetaVersion):
    lwjgl_copy = copy.deepcopy(lwjgl)
    lwjgl_copy.release_time = None
    return hashlib.sha1(lwjgl_copy.json().encode("utf-8", "strict")).hexdigest()
```

The hash excludes `release_time` so that identical library sets from different Minecraft versions produce the same hash.

### Log4j Patching

To mitigate CVE-2021-44228 (Log4Shell), all Log4j libraries are forcibly upgraded:

```python
def map_log4j_artifact(version):
    x = pversion.parse(version)
    if x <= pversion.parse("2.0"):
        return "2.0-beta9-fixed", "https://files.projecttick.org/maven/%s"
    if x <= pversion.parse("2.17.1"):
        return "2.17.1", "https://repo1.maven.org/maven2/%s"
    return None, None
```

Version `2.0-beta9-fixed` is a custom patched build hosted on the ProjT Maven. Version `2.17.1` is the official fixed release from Apache.

Pre-computed hashes are stored for each artifact:

```python
LOG4J_HASHES = {
    "2.0-beta9-fixed": {
        "log4j-api": {"sha1": "b61eaf2e64d8b0277e188262a8b771bbfa1502b3", "size": 107347},
        "log4j-core": {"sha1": "677991ea2d7426f76309a73739cecf609679492c", "size": 677588},
    },
    "2.17.1": {
        "log4j-api": {"sha1": "d771af8e336e372fb5399c99edabe0919aeaf5b2", "size": 301872},
        "log4j-core": {"sha1": "779f60f3844dadc3ef597976fcb1e5127b1f343d", "size": 1790452},
        "log4j-slf4j18-impl": {"sha1": "ca499d751f4ddd8afb016ef698c30be0da1d09f7", "size": 21268},
    },
}
```

### Library Rules and Platform Filtering

Mojang uses a rules system to specify platform-specific libraries:

```python
class OSRule(MetaBase):
    name: str   # "osx", "linux", "windows", "windows-arm64", etc.
    version: Optional[str]

class MojangRule(MetaBase):
    action: str   # "allow" or "disallow"
    os: Optional[OSRule]
```

The generator filters macOS-only libraries (like older LWJGL builds that were OS-specific):

```python
def is_macos_only(rules: Optional[MojangRules]):
    allows_osx = False
    allows_all = False
    if rules:
        for rule in rules:
            if rule.action == "allow" and rule.os and rule.os.name == "osx":
                allows_osx = True
            if rule.action == "allow" and not rule.os:
                allows_all = True
        if allows_osx and not allows_all:
            return True
    return False
```

### Argument Processing

Modern Minecraft (1.13+) uses a structured argument format instead of a flat string. The generator flattens these back:

```python
def adapt_new_style_arguments(arguments):
    foo = []
    for arg in arguments.game:
        if isinstance(arg, str):
            if arg == "--clientId":
                continue
            if arg == "${clientid}":
                continue
            if arg == "--xuid":
                continue
            if arg == "${auth_xuid}":
                continue
            foo.append(arg)
        else:
            print("!!! Unrecognized structure in Minecraft game arguments:")
            pprint(arg)
    return " ".join(foo)
```

Some arguments like `--clientId` and `--xuid` are filtered out (they're Microsoft-account-specific and not used by the launcher).

Feature flags in structured arguments are converted to traits:

```python
def adapt_new_style_arguments_to_traits(arguments):
    foo = []
    for arg in arguments.game:
        if isinstance(arg, dict):
            for rule in arg["rules"]:
                for k, v in rule["features"].items():
                    if rule["action"] == "allow" and v and k in SUPPORTED_FEATURES:
                        foo.append(f"feature:{k}")
    return foo
```

### Legacy Override System

The `LegacyOverrideIndex` provides manual corrections for old Minecraft versions:

```python
class LegacyOverrideEntry(MetaBase):
    main_class: Optional[str]
    applet_class: Optional[str]
    release_time: Optional[datetime]
    additional_traits: Optional[List[str]]
    additional_jvm_args: Optional[List[str]]

    def apply_onto_meta_version(self, meta_version: MetaVersion, legacy: bool = True):
        meta_version.main_class = self.main_class
        meta_version.applet_class = self.applet_class
        if self.release_time:
            meta_version.release_time = self.release_time
        if self.additional_traits:
            if not meta_version.additional_traits:
                meta_version.additional_traits = []
            meta_version.additional_traits += self.additional_traits
        if legacy:
            meta_version.libraries = None  # Remove all libraries for legacy
```

### Dependency Wiring

Each Minecraft version declares a dependency on either LWJGL 2 or LWJGL 3:

```python
if is_lwjgl_3:
    lwjgl_dependency = Dependency(uid=LWJGL3_COMPONENT)
else:
    lwjgl_dependency = Dependency(uid=LWJGL_COMPONENT)

lwjgl_dependency.suggests = suggested_version
v.requires = [lwjgl_dependency]
```

LWJGL 3 versions also get the `FirstThreadOnMacOS` trait:

```python
if is_lwjgl_3:
    if not v.additional_traits:
        v.additional_traits = []
    v.additional_traits.append("FirstThreadOnMacOS")
```

### Package Metadata

Finally, the generator produces `package.json` files:

```python
minecraft_package = MetaPackage(uid=MINECRAFT_COMPONENT, name="Minecraft")
minecraft_package.recommended = [mojang_index.latest.release]
minecraft_package.write(os.path.join(LAUNCHER_DIR, MINECRAFT_COMPONENT, "package.json"))
```

---

## Library Patches

The `mojang-library-patches.json` file contains patches applied to Minecraft libraries during generation:

```python
class LibraryPatch(MetaBase):
    match: List[GradleSpecifier]
    override: Optional[Library]
    additionalLibraries: Optional[List[Library]]
    patchAdditionalLibraries: bool = False

    def applies(self, target: Library) -> bool:
        return target.name in self.match
```

Use cases:
- Adding ARM64 native libraries for platforms Mojang doesn't officially support
- Replacing broken library URLs
- Adding missing download metadata

---

## Supported OS Values

The `OSRule.name` validator accepts:

```python
["osx", "linux", "windows", "windows-arm64", "osx-arm64",
 "linux-arm64", "linux-arm32", "linux-riscv64"]
```

---

## Output Structure

After generation:

```
launcher/
├── net.minecraft/
│   ├── package.json           # recommended: [latest release]
│   ├── 1.21.5.json
│   ├── 1.20.4.json
│   ├── 24w14a.json            # snapshot
│   ├── 1.0.json               # legacy
│   └── ...
├── org.lwjgl/
│   ├── package.json
│   ├── 2.9.0.json
│   ├── 2.9.1.json
│   └── 2.9.4-nightly-20150209.json
└── org.lwjgl3/
    ├── package.json
    ├── 3.1.2.json
    ├── 3.2.2.json
    ├── 3.3.1.json
    ├── 3.3.3.json
    └── 3.4.1.json
```

---

## Key Constants

| Constant | Value | Location |
|---|---|---|
| `MINECRAFT_COMPONENT` | `"net.minecraft"` | `common/mojang.py` |
| `LWJGL_COMPONENT` | `"org.lwjgl"` | `common/mojang.py` |
| `LWJGL3_COMPONENT` | `"org.lwjgl3"` | `common/mojang.py` |
| `SUPPORTED_LAUNCHER_VERSION` | `21` | `model/mojang.py` |
| `SUPPORTED_COMPLIANCE_LEVEL` | `1` | `model/mojang.py` |
| `DEFAULT_JAVA_MAJOR` | `8` | `model/mojang.py` |
| `DEFAULT_JAVA_NAME` | `"jre-legacy"` | `model/mojang.py` |
| `META_FORMAT_VERSION` | `1` | `model/__init__.py` |
