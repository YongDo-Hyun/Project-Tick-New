# Meta — Forge Metadata

## Overview

Forge is the oldest and most complex mod loader supported by Meta. The processing pipeline handles:

- Multiple Forge distribution formats across different Minecraft versions (legacy JARs, installer profiles v1, installer profiles v2 with build system)
- ForgeWrapper integration for modern Forge versions
- FML library injection for legacy versions (1.3.2–1.5.2)
- Installer JAR downloading, extraction, and caching
- Library deduplication against Minecraft's own libraries

---

## Phase 1: Update — `update_forge.py`

### Fetching the Version Index

Forge publishes two key metadata files:

```python
# Maven metadata — maps MC versions to Forge version lists
r = sess.get(
    "https://files.minecraftforge.net/net/minecraftforge/forge/maven-metadata.json"
)
main_json = r.json()  # dict: {"1.20.4": ["1.20.4-49.0.31", ...], ...}

# Promotions — marks latest/recommended versions
r = sess.get(
    "https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json"
)
promotions_json = r.json()  # {"promos": {"1.20.4-latest": "49.0.31", ...}}
```

### Building the Derived Index

The updater reconstructs a comprehensive index from these fragments:

```python
new_index = DerivedForgeIndex()

version_expression = re.compile(
    r"^(?P<mc>[0-9a-zA-Z_\.]+)-(?P<ver>[0-9\.]+\.(?P<build>[0-9]+))(-(?P<branch>[a-zA-Z0-9\.]+))?$"
)

for mc_version, value in main_json.items():
    for long_version in value:
        match = version_expression.match(long_version)
        files = get_single_forge_files_manifest(long_version)

        entry = ForgeEntry(
            long_version=long_version,
            mc_version=mc_version,
            version=match.group("ver"),
            build=int(match.group("build")),
            branch=match.group("branch"),
            latest=False,
            recommended=version in recommended_set,
            files=files,
        )
        new_index.versions[long_version] = entry
```

### File Manifest Fetching

For each version, the updater fetches a file manifest from Forge's Maven:

```python
def get_single_forge_files_manifest(longversion):
    file_url = (
        "https://files.minecraftforge.net/net/minecraftforge/forge/%s/meta.json"
        % longversion
    )
    r = sess.get(file_url)
    files_json = r.json()

    for classifier, extensionObj in files_json.get("classifiers").items():
        # Parse each file: installer, universal, changelog, etc.
        file_obj = ForgeFile(
            classifier=classifier, hash=processed_hash, extension=extension
        )
        ret_dict[classifier] = file_obj

    return ret_dict
```

Each `ForgeFile` represents a downloadable artifact:

```python
class ForgeFile(MetaBase):
    classifier: str    # "installer", "universal", "client", "changelog"
    hash: str          # MD5 hash
    extension: str     # "jar", "zip", "txt"

    def filename(self, long_version):
        return "%s-%s-%s.%s" % ("forge", long_version, self.classifier, self.extension)

    def url(self, long_version):
        return "https://maven.minecraftforge.net/net/minecraftforge/forge/%s/%s" % (
            long_version, self.filename(long_version),
        )
```

### Installer JAR Processing

For versions that use installers, the updater downloads the JAR and extracts two files:

```python
def process_forge_version(version, jar_path):
    # Download if not cached
    if not os.path.isfile(jar_path):
        download_binary_file(sess, jar_path, version.url())

    # Extract from ZIP
    with zipfile.ZipFile(jar_path) as jar:
        # Extract version.json (Minecraft version overlay)
        with jar.open("version.json") as profile_zip_entry:
            version_data = profile_zip_entry.read()
            MojangVersion.parse_raw(version_data)  # Validate
            with open(version_file_path, "wb") as f:
                f.write(version_data)

        # Extract install_profile.json
        with jar.open("install_profile.json") as profile_zip_entry:
            install_profile_data = profile_zip_entry.read()
            # Try both v1 and v2 formats
            try:
                ForgeInstallerProfile.parse_raw(install_profile_data)
            except ValidationError:
                ForgeInstallerProfileV2.parse_raw(install_profile_data)
```

### Installer Info Caching

SHA-1, SHA-256, and size of each installer JAR are cached:

```python
installer_info = InstallerInfo()
installer_info.sha1hash = file_hash(jar_path, hashlib.sha1)
installer_info.sha256hash = file_hash(jar_path, hashlib.sha256)
installer_info.size = os.path.getsize(jar_path)
installer_info.write(installer_info_path)
```

### SHA-1 Verification

Before processing, the updater checks if the local JAR matches the remote checksum:

```python
fileSha1 = get_file_sha1_from_file(jar_path, sha1_file)
rfile = sess.get(version.url() + ".sha1")
new_sha1 = rfile.text.strip()
if fileSha1 != new_sha1:
    remove_files([jar_path, profile_path, installer_info_path, sha1_file])
```

If the SHA-1 mismatch is detected, all cached artifacts are deleted and re-downloaded.

### Legacy Version Info

For pre-installer Forge versions (MC 1.1–1.5.2), the updater extracts release times from JAR file timestamps:

```python
tstamp = datetime.fromtimestamp(0)
with zipfile.ZipFile(jar_path) as jar:
    for info in jar.infolist():
        tstamp_new = datetime(*info.date_time)
        if tstamp_new > tstamp:
            tstamp = tstamp_new
legacy_info = ForgeLegacyInfo()
legacy_info.release_time = tstamp
legacy_info.sha1 = file_hash(jar_path, hashlib.sha1)
```

### Bad Versions

Certain versions are blacklisted:

```python
BAD_VERSIONS = ["1.12.2-14.23.5.2851"]
```

---

## Phase 2: Generate — `generate_forge.py`

### ForgeVersion Post-Processing

The raw `ForgeEntry` is converted into a `ForgeVersion` object that resolves download URLs:

```python
class ForgeVersion:
    def __init__(self, entry: ForgeEntry):
        self.build = entry.build
        self.rawVersion = entry.version
        self.mc_version = entry.mc_version
        self.mc_version_sane = self.mc_version.replace("_pre", "-pre", 1)
        self.long_version = "%s-%s" % (self.mc_version, self.rawVersion)
        if self.branch is not None:
            self.long_version += "-%s" % self.branch

        for classifier, file in entry.files.items():
            if classifier == "installer" and extension == "jar":
                self.installer_filename = filename
                self.installer_url = url
            if (classifier == "universal" or classifier == "client") and ...:
                self.universal_filename = filename
                self.universal_url = url

    def uses_installer(self):
        if self.installer_url is None:
            return False
        if self.mc_version == "1.5.2":
            return False
        return True
```

### Library Deduplication

Libraries already provided by Minecraft are filtered out:

```python
def should_ignore_artifact(libs: Collection[GradleSpecifier], match: GradleSpecifier):
    for ver in libs:
        if ver.group == match.group and ver.artifact == match.artifact and ver.classifier == match.classifier:
            if ver.version == match.version:
                return True  # Exact match, ignore
            elif pversion.parse(ver.version) > pversion.parse(match.version):
                return True  # Minecraft has newer version
            else:
                return False  # Forge has newer, keep it
    return False  # Not in Minecraft, keep it
```

### Three Generation Paths

The generator handles three distinct Forge eras:

#### Path 1: Legacy (MC 1.1–1.5.2) — `version_from_legacy()`

Pre-installer versions that inject a JAR mod:

```python
def version_from_legacy(info: ForgeLegacyInfo, version: ForgeVersion) -> MetaVersion:
    v = MetaVersion(name="Forge", version=version.rawVersion, uid=FORGE_COMPONENT)
    v.requires = [Dependency(uid=MINECRAFT_COMPONENT, equals=mc_version)]
    v.release_time = info.release_time
    v.order = 5

    if fml_libs_for_version(mc_version):
        v.additional_traits = ["legacyFML"]

    main_mod = Library(
        name=GradleSpecifier("net.minecraftforge", "forge", version.long_version, classifier)
    )
    main_mod.downloads = MojangLibraryDownloads()
    main_mod.downloads.artifact = MojangArtifact(url=version.url(), sha1=info.sha1, size=info.size)
    v.jar_mods = [main_mod]
    return v
```

#### Path 2: Old Installer (MC 1.6–1.12.2) — `version_from_profile()` / `version_from_modernized_installer()`

These versions have installer profiles with embedded version JSONs:

```python
def version_from_profile(profile: ForgeInstallerProfile, version: ForgeVersion) -> MetaVersion:
    v = MetaVersion(name="Forge", version=version.rawVersion, uid=FORGE_COMPONENT)
    v.main_class = profile.version_info.main_class
    v.release_time = profile.version_info.time

    # Extract tweaker classes from arguments
    args = profile.version_info.minecraft_arguments
    tweakers = []
    expression = re.compile(r"--tweakClass ([a-zA-Z0-9.]+)")
    match = expression.search(args)
    while match is not None:
        tweakers.append(match.group(1))
        # ...
    v.additional_tweakers = tweakers

    # Filter libraries against Minecraft's library set
    mc_filter = load_mc_version_filter(mc_version)
    for forge_lib in profile.version_info.libraries:
        if forge_lib.name.is_lwjgl() or should_ignore_artifact(mc_filter, forge_lib.name):
            continue
        # Rename minecraftforge → forge with universal classifier
        # ...
        v.libraries.append(overridden_lib)
```

#### Path 3: Modern Build System (MC 1.13+) — `version_from_build_system_installer()`

Modern Forge uses a two-file installer system (`install_profile.json` + `version.json`). The launcher cannot run the installer's processors directly, so ForgeWrapper is injected:

```python
def version_from_build_system_installer(
    installer: MojangVersion, profile: ForgeInstallerProfileV2, version: ForgeVersion
) -> MetaVersion:
    v = MetaVersion(name="Forge", version=version.rawVersion, uid=FORGE_COMPONENT)
    v.main_class = "io.github.zekerzhayard.forgewrapper.installer.Main"

    v.maven_files = []

    # Add installer JAR as a Maven file
    info = InstallerInfo.parse_file(...)
    installer_lib = Library(
        name=GradleSpecifier("net.minecraftforge", "forge", version.long_version, "installer")
    )
    installer_lib.downloads = MojangLibraryDownloads()
    installer_lib.downloads.artifact = MojangArtifact(
        url="https://maven.minecraftforge.net/%s" % installer_lib.name.path(),
        sha1=info.sha1hash, size=info.size,
    )
    v.maven_files.append(installer_lib)

    # Add profile libraries as Maven files
    for forge_lib in profile.libraries:
        if forge_lib.name.is_log4j():
            continue
        update_library_info(forge_lib)
        v.maven_files.append(forge_lib)

    # Add ForgeWrapper as runtime library
    v.libraries = [FORGEWRAPPER_LIBRARY]

    # Add installer's runtime libraries
    for forge_lib in installer.libraries:
        if forge_lib.name.is_log4j():
            continue
        v.libraries.append(forge_lib)

    # Build Minecraft arguments
    mc_args = "--username ${auth_player_name} --version ${version_name} ..."
    for arg in installer.arguments.game:
        mc_args += f" {arg}"
    if "--fml.forgeGroup" not in installer.arguments.game:
        mc_args += f" --fml.forgeGroup net.minecraftforge"
    if "--fml.forgeVersion" not in installer.arguments.game:
        mc_args += f" --fml.forgeVersion {version.rawVersion}"
    if "--fml.mcVersion" not in installer.arguments.game:
        mc_args += f" --fml.mcVersion {version.mc_version}"
    v.minecraft_arguments = mc_args
    return v
```

### Library Info Population

For libraries that lack complete download metadata, the generator fetches it:

```python
def update_library_info(lib: Library):
    if not lib.downloads:
        lib.downloads = MojangLibraryDownloads()
    if not lib.downloads.artifact:
        url = lib.url or f"https://maven.minecraftforge.net/{lib.name.path()}"
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

### FML Libraries

Legacy Forge versions (MC 1.3.2–1.5.2) require FML libraries. The `fml_libs_for_version()` function returns the exact set for each MC version:

```python
def fml_libs_for_version(mc_version: str) -> List[FMLLib]:
    if mc_version == "1.3.2":
        return [argo_2_25, guava_12_0_1, asm_all_4_0]
    elif mc_version in ["1.4", "1.4.1", ..., "1.4.7"]:
        return [argo_2_25, guava_12_0_1, asm_all_4_0, bcprov_jdk15on_147]
    elif mc_version == "1.5":
        return [argo_small_3_2, guava_14_0_rc3, asm_all_4_1,
                bcprov_jdk15on_148, deobfuscation_data_1_5, scala_library]
    # ...
```

### Recommended Versions

Versions marked as `recommended` by Forge promotions are tracked:

```python
recommended_versions = []
for key, entry in remote_versions.versions.items():
    if entry.recommended:
        recommended_versions.append(version.rawVersion)

package = MetaPackage(uid=FORGE_COMPONENT, name="Forge",
                      project_url="https://www.minecraftforge.net/forum/")
package.recommended = recommended_versions
```

---

## Data Models

### `ForgeEntry`

```python
class ForgeEntry(MetaBase):
    long_version: str    # "1.20.4-49.0.31"
    mc_version: str      # "1.20.4"
    version: str         # "49.0.31"
    build: int           # 31
    branch: Optional[str]
    latest: Optional[bool]
    recommended: Optional[bool]
    files: Optional[Dict[str, ForgeFile]]
```

### `DerivedForgeIndex`

```python
class DerivedForgeIndex(MetaBase):
    versions: Dict[str, ForgeEntry]
    by_mc_version: Dict[str, ForgeMCVersionInfo]
```

### `ForgeInstallerProfile` (v1)

```python
class ForgeInstallerProfile(MetaBase):
    install: ForgeInstallerProfileInstallSection
    version_info: ForgeVersionFile
    optionals: Optional[List[ForgeOptional]]
```

### `ForgeInstallerProfileV2`

```python
class ForgeInstallerProfileV2(MetaBase):
    spec: Optional[int]
    profile: Optional[str]
    version: Optional[str]
    path: Optional[GradleSpecifier]
    minecraft: Optional[str]
    data: Optional[Dict[str, DataSpec]]
    processors: Optional[List[ProcessorSpec]]
    libraries: Optional[List[Library]]
```

### `InstallerInfo`

```python
class InstallerInfo(MetaBase):
    sha1hash: Optional[str]
    sha256hash: Optional[str]
    size: Optional[int]
```

---

## ForgeWrapper

The `FORGEWRAPPER_LIBRARY` is defined in `common/forge.py`:

```python
FORGEWRAPPER_LIBRARY = make_launcher_library(
    GradleSpecifier("io.github.zekerzhayard", "ForgeWrapper", "projt-2026-04-04"),
    "4c4653d80409e7e968d3e3209196ffae778b7b4e",
    29731,
)
```

This creates a `Library` object with:
- Download URL: `https://files.projecttick.org/maven/io/github/zekerzhayard/ForgeWrapper/projt-2026-04-04/ForgeWrapper-projt-2026-04-04.jar`
- SHA-1: `4c4653d80409e7e968d3e3209196ffae778b7b4e`
- Size: 29731 bytes

---

## Output Structure

```
launcher/net.minecraftforge/
├── package.json        # name, recommended versions, project URL
├── 49.0.31.json        # Modern (build system installer)
├── 36.2.39.json        # Modern (build system installer)
├── 14.23.5.2860.json   # Old installer (profile v1)
├── 10.13.4.1614.json   # Modernized installer for legacy MC
├── 7.8.1.740.json      # Legacy JAR mod
└── ...
```
