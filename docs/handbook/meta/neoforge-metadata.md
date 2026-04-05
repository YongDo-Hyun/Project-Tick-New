# Meta — NeoForge Metadata

## Overview

NeoForge is a fork of Forge that emerged in 2023. Its metadata pipeline closely mirrors Forge's, but with key differences in version numbering and Maven repository structure. NeoForge exclusively uses the build-system installer format (equivalent to Forge's v2 installer profile), so there are no legacy paths to handle.

---

## Phase 1: Update — `update_neoforge.py`

### Fetching Version Lists

NeoForge publishes versions under two Maven artifacts:

```python
# Legacy artifact (1.20.1 era, when NeoForge still used Forge's naming)
r = sess.get(
    "https://maven.neoforged.net/api/maven/versions/releases/net%2Fneoforged%2Fforge"
)
main_json = r.json()["versions"]

# New artifact (post-1.20.1, NeoForge's own naming)
r = sess.get(
    "https://maven.neoforged.net/api/maven/versions/releases/net%2Fneoforged%2Fneoforge"
)
new_main_json = r.json()["versions"]

main_json += new_main_json  # Merge both lists
```

### Version Parsing

Two regex patterns handle the two naming schemes:

```python
# Legacy format: "1.20.1-47.1.100"
version_expression = re.compile(
    r"^(?P<mc>[0-9a-zA-Z_\.]+)-(?P<ver>[0-9\.]+\.(?P<build>[0-9]+))(-(?P<branch>[a-zA-Z0-9\.]+))?$"
)

# New NeoForge format: "20.4.237" or "21.0.0-beta"
neoforge_version_re = re.compile(
    r"^(?P<mcminor>\d+)\.(?:(?P<mcpatch>\d+)|(?P<snapshot>[0-9a-z]+))\.(?P<number>\d+)(?:\.(?P<build>\d+))?(?:-(?P<tag>[0-9A-Za-z][0-9A-Za-z.+-]*))?$"
)
```

For the new format, the Minecraft version is reconstructed from the NeoForge version number:

```python
if match_nf:
    mc_version = match_nf.group("snapshot")
    if not mc_version:
        mc_version = f"1.{match_nf.group('mcminor')}"
        if match_nf.group("mcpatch") != "0":
            mc_version += f".{match_nf.group('mcpatch')}"
    artifact = "neoforge"
```

### File Manifest from Maven API

Unlike Forge which uses its own `meta.json`, NeoForge file manifests come from the Maven API:

```python
def get_single_forge_files_manifest(longversion, artifact: str):
    file_url = (
        f"https://maven.neoforged.net/api/maven/details/releases/net%2Fneoforged%2F{artifact}%2F"
        + urllib.parse.quote(longversion)
    )
    r = sess.get(file_url)
    files_json = r.json()

    for file in files_json.get("files"):
        name = file["name"]
        prefix = f"{artifact}-{longversion}"
        file_name = name[len(prefix):]
        if file_name.startswith("."):
            continue  # Skip top-level extension files
        classifier, ext = os.path.splitext(file_name)
        if ext in [".md5", ".sha1", ".sha256", ".sha512"]:
            continue  # Skip checksum files

        file_obj = NeoForgeFile(
            artifact=artifact, classifier=classifier, extension=ext[1:]
        )
        ret_dict[classifier] = file_obj
```

### NeoForgeFile Model

```python
class NeoForgeFile(MetaBase):
    artifact: str       # "forge" or "neoforge"
    classifier: str     # "installer", "universal"
    extension: str      # "jar"

    def filename(self, long_version):
        return "%s-%s-%s.%s" % (
            self.artifact, long_version, self.classifier, self.extension,
        )

    def url(self, long_version):
        return "https://maven.neoforged.net/releases/net/neoforged/%s/%s/%s" % (
            self.artifact, long_version, self.filename(long_version),
        )
```

### Installer Processing

The processing is virtually identical to Forge:

```python
def process_neoforge_version(key, entry):
    version = NeoForgeVersion(entry)
    if version.url() is None or not version.uses_installer():
        return

    jar_path = os.path.join(UPSTREAM_DIR, JARS_DIR, version.filename())

    # SHA-1 verification, download, extract version.json and install_profile.json
    with zipfile.ZipFile(jar_path) as jar:
        with jar.open("version.json") as profile_zip_entry:
            MojangVersion.parse_raw(version_data)
        with jar.open("install_profile.json") as profile_zip_entry:
            NeoForgeInstallerProfileV2.parse_raw(install_profile_data)

    # Cache installer info
    installer_info = InstallerInfo()
    installer_info.sha1hash = file_hash(jar_path, hashlib.sha1)
    installer_info.sha256hash = file_hash(jar_path, hashlib.sha256)
    installer_info.size = os.path.getsize(jar_path)
    installer_info.write(installer_info_path)
```

---

## Phase 2: Generate — `generate_neoforge.py`

### Single Generation Path

Unlike Forge (which has three paths), NeoForge only uses the build-system installer path:

```python
def version_from_build_system_installer(
    installer: MojangVersion,
    profile: NeoForgeInstallerProfileV2,
    version: NeoForgeVersion,
) -> MetaVersion:
    v = MetaVersion(name="NeoForge", version=version.rawVersion, uid=NEOFORGE_COMPONENT)
    v.main_class = "io.github.zekerzhayard.forgewrapper.installer.Main"
```

### Library Handling

Profile libraries go into `maven_files` (install-time downloads), and installer libraries plus ForgeWrapper go into `libraries` (runtime classpath):

```python
    v.maven_files = []

    # Installer JAR as Maven file
    installer_lib = Library(
        name=GradleSpecifier("net.neoforged", version.artifact, version.long_version, "installer")
    )
    installer_lib.downloads = MojangLibraryDownloads()
    installer_lib.downloads.artifact = MojangArtifact(
        url="https://maven.neoforged.net/releases/%s" % installer_lib.name.path(),
        sha1=info.sha1hash, size=info.size,
    )
    v.maven_files.append(installer_lib)

    # Profile libraries (processor dependencies)
    for forge_lib in profile.libraries:
        if forge_lib.name.is_log4j():
            continue
        update_library_info(forge_lib)
        v.maven_files.append(forge_lib)

    # Runtime libraries
    v.libraries = [FORGEWRAPPER_LIBRARY]
    for forge_lib in installer.libraries:
        if forge_lib.name.is_log4j():
            continue
        v.libraries.append(forge_lib)
```

### Library Info Fetching

Same approach as Forge — fills in missing SHA-1 and size from Maven:

```python
def update_library_info(lib: Library):
    if not lib.downloads:
        lib.downloads = MojangLibraryDownloads()
    if not lib.downloads.artifact:
        url = lib.url or f"https://maven.neoforged.net/releases/{lib.name.path()}"
        lib.downloads.artifact = MojangArtifact(url=url, sha1=None, size=None)

    art = lib.downloads.artifact
    if art and art.url:
        if not art.sha1:
            r = sess.get(art.url + ".sha1")
            if r.status_code == 200:
                art.sha1 = r.text.strip()
        if not art.size:
            r = sess.head(art.url)
            if r.status_code == 200 and 'Content-Length' in r.headers:
                art.size = int(r.headers['Content-Length'])
```

### Minecraft Version Dependency

NeoForge extracts the Minecraft version from the installer profile's `minecraft` field:

```python
v.requires = [Dependency(uid=MINECRAFT_COMPONENT, equals=profile.minecraft)]

# Skip if we don't have the corresponding Minecraft version
if not os.path.isfile(
    os.path.join(LAUNCHER_DIR, MINECRAFT_COMPONENT, f"{profile.minecraft}.json")
):
    eprint("Skipping %s with no corresponding Minecraft version %s" % (key, profile.minecraft))
    continue
```

### Argument Construction

```python
mc_args = (
    "--username ${auth_player_name} --version ${version_name} --gameDir ${game_directory} "
    "--assetsDir ${assets_root} --assetIndex ${assets_index_name} --uuid ${auth_uuid} "
    "--accessToken ${auth_access_token} --userType ${user_type} --versionType ${version_type}"
)
for arg in installer.arguments.game:
    mc_args += f" {arg}"
v.minecraft_arguments = mc_args
```

---

## Data Models

### `NeoForgeEntry`

```python
class NeoForgeEntry(MetaBase):
    artifact: str           # "forge" or "neoforge"
    long_version: str       # "1.20.1-47.1.100" or "20.4.237"
    version: str            # Short version: "47.1.100" or "237"
    latest: Optional[bool]
    recommended: Optional[bool]
    files: Optional[Dict[str, NeoForgeFile]]
```

### `DerivedNeoForgeIndex`

```python
class DerivedNeoForgeIndex(MetaBase):
    versions: Dict[str, NeoForgeEntry]
```

Note: Unlike Forge's `DerivedForgeIndex`, this does not have a `by_mc_version` mapping.

### `NeoForgeVersion`

Post-processed version with resolved download URLs:

```python
class NeoForgeVersion:
    def __init__(self, entry: NeoForgeEntry):
        self.artifact = entry.artifact
        self.rawVersion = entry.version
        if self.artifact == "neoforge":
            self.rawVersion = entry.long_version

        self.long_version = entry.long_version
        for classifier, file in entry.files.items():
            if classifier == "installer" and extension == "jar":
                self.installer_filename = filename
                self.installer_url = url
```

### `NeoForgeInstallerProfileV2`

Same structure as Forge's v2 profile:

```python
class NeoForgeInstallerProfileV2(MetaBase):
    spec: Optional[int]
    profile: Optional[str]
    version: Optional[str]
    path: Optional[GradleSpecifier]
    minecraft: Optional[str]
    data: Optional[Dict[str, DataSpec]]
    processors: Optional[List[ProcessorSpec]]
    libraries: Optional[List[Library]]
```

---

## Key Differences from Forge

| Aspect | Forge | NeoForge |
|---|---|---|
| Maven URL | `maven.minecraftforge.net` | `maven.neoforged.net/releases` |
| File manifest API | `meta.json` per version | Maven details API |
| Artifacts | Always `forge` | `forge` (1.20.1) or `neoforge` (1.20.2+) |
| Version format | `<mc>-<forge_ver>` | `<mc>-<forge_ver>` or `<mcminor>.<mcpatch>.<build>` |
| Legacy support | Yes (MC 1.1–1.12.2) | No (MC 1.20.1+ only) |
| Component UID | `net.minecraftforge` | `net.neoforged` |
| Bad versions list | Yes | No |
| ForgeWrapper | Yes | Yes (same library) |
| Promotions/recommended | Yes | Not currently (`is_recommended = False`) |

---

## Output Structure

```
launcher/net.neoforged/
├── package.json
├── 21.4.38.json         # New NeoForge format
├── 20.4.237.json        # New NeoForge format
├── 47.1.100.json        # Legacy Forge-style format (1.20.1)
└── ...
```

---

## Constants

| Constant | Value | Location |
|---|---|---|
| `NEOFORGE_COMPONENT` | `"net.neoforged"` | `common/neoforge.py` |
| `BASE_DIR` | `"neoforge"` | `common/neoforge.py` |
| `FORGEWRAPPER_LIBRARY` | (shared with Forge) | `common/forge.py` |
