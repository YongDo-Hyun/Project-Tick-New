# Meta — Update Pipeline

## Overview

The Meta update pipeline follows a strict two-phase architecture orchestrated by `update.sh`:

1. **Update Phase** — fetch and cache upstream vendor data into the `upstream/` git repository
2. **Generate Phase** — transform cached data into launcher-compatible JSON in the `launcher/` git repository
3. **Index Phase** — build version indices with SHA-256 hashes
4. **Deploy Phase** — commit and push both repositories (or rsync to a folder)

---

## Orchestration: `update.sh`

### Environment Configuration

```bash
export META_CACHE_DIR=${CACHE_DIRECTORY:-./caches}
export META_UPSTREAM_DIR=${META_UPSTREAM_DIR:-${STATE_DIRECTORY:-.}/upstream}
export META_LAUNCHER_DIR=${META_LAUNCHER_DIR:-${STATE_DIRECTORY:-.}/metalauncher}
```

These can be overridden via `config.sh` (sourced if present) or systemd environment variables (`CACHE_DIRECTORY`, `STATE_DIRECTORY`).

### Execution Order

**Update scripts** (populate `upstream/`):

```bash
upstream_git reset --hard HEAD || exit 1

python -m meta.run.update_mojang    || fail_in
python -m meta.run.update_forge     || fail_in
python -m meta.run.update_neoforge  || fail_in
python -m meta.run.update_fabric    || fail_in
python -m meta.run.update_quilt     || fail_in
python -m meta.run.update_liteloader || fail_in
python -m meta.run.update_java      || fail_in
python -m meta.run.update_risugami  || fail_in
python -m meta.run.update_stationloader || fail_in
python -m meta.run.update_optifine  || fail_in
python -m meta.run.update_modloadermp || fail_in
```

**Generate scripts** (produce `launcher/`):

```bash
launcher_git reset --hard HEAD || exit 1

python -m meta.run.generate_mojang    || fail_out
python -m meta.run.generate_forge     || fail_out
python -m meta.run.generate_neoforge  || fail_out
python -m meta.run.generate_fabric    || fail_out
python -m meta.run.generate_quilt     || fail_out
python -m meta.run.generate_liteloader || fail_out
python -m meta.run.generate_java      || fail_out
python -m meta.run.generate_risugami  || fail_in
python -m meta.run.generate_stationloader || fail_in
python -m meta.run.generate_optifine  || fail_in
python -m meta.run.generate_modloadermp || fail_in
python -m meta.run.index              || fail_out
```

### Error Handling

Two failure functions ensure clean recovery:

```bash
function fail_in() {
    upstream_git reset --hard HEAD
    exit 1
}

function fail_out() {
    launcher_git reset --hard HEAD
    exit 1
}
```

On any script failure, the corresponding git repo is reset to HEAD, discarding partial changes.

---

## Update Scripts

### `update_mojang.py`

**Sources**: Mojang's `piston-meta.mojang.com`

**Steps**:
1. Fetch `version_manifest_v2.json` — the master list of all Minecraft versions
2. For each version entry, download the individual version JSON (concurrent)
3. Fetch experimental snapshots from a bundled ZIP resource
4. Load old snapshot metadata from bundled JSON
5. Fetch Mojang's Java runtime manifest (`java_all.json`)

**Concurrency**: `ThreadPoolExecutor` for version JSON downloads

**Output**: `upstream/mojang/`

### `update_forge.py`

**Sources**: Forge Maven (`files.minecraftforge.net`)

**Steps**:
1. Fetch `maven-metadata.json` and `promotions_slim.json`
2. Build `DerivedForgeIndex` from version metadata
3. For each supported version, download installer JAR files
4. Extract `install_profile.json` and `version.json` from installer JARs
5. Cache `InstallerInfo` (SHA-1, size) for the launcher Maven
6. Handle legacy Forge info (FML libs, pre-1.6 versions)

**Key complexity**: Three generation eras (legacy jar mods, profile-based, build system)

**Output**: `upstream/forge/`

### `update_neoforge.py`

**Sources**: NeoForge Maven (`maven.neoforged.net`)

**Steps**:
1. Fetch maven-metadata.xml from two artifact paths:
   - `net/neoforged/forge/` (early NeoForge, branched from Forge)  
   - `net/neoforged/neoforge/` (independent NeoForge)
2. Parse versions with two regex patterns
3. Download installer JARs and extract profiles (same as Forge)

**Output**: `upstream/neoforge/`

### `update_fabric.py`

**Sources**: Fabric Meta API v2 (`meta.fabricmc.net`)

**Steps**:
1. Fetch loader and intermediary version lists
2. For each loader version, download installer JSON from Fabric Maven
3. Extract JAR timestamps (HEAD requests, download fallback)

**Concurrency**: `multiprocessing.Pool`

**Output**: `upstream/fabric/`

### `update_quilt.py`

**Sources**: Quilt Meta API v3 (`meta.quiltmc.org`)

**Steps**:
1. Fetch quilt-loader version list
2. Download installer JSONs from Quilt Maven
3. Download full JARs for timestamp extraction (no HEAD optimization)

**Concurrency**: `ThreadPoolExecutor`

**Output**: `upstream/quilt/`

### `update_java.py`

**Sources**: Adoptium API (`api.adoptium.net`), OpenJ9 API (`api.adoptopenjdk.net`), Azul API (`api.azul.com`)

**Steps**:
1. Adoptium: paginate feature releases, save per-major-version
2. OpenJ9: same API structure as Adoptium, different vendor/JVM-impl
3. Azul: paginate package list, fetch individual package details

**Retry logic**: 3 attempts with linear backoff for 5xx errors

**Output**: `upstream/java_runtime/`

---

## Generate Scripts

### Processing Pattern

All generate scripts follow the same pattern:

```python
def main():
    # 1. Load upstream data
    index = DerivedIndex.parse_file(upstream_path(...))
    
    # 2. Transform to MetaVersion
    for entry in index:
        version = process_version(entry)
        version.write(launcher_path(COMPONENT, f"{version.version}.json"))
    
    # 3. Write package metadata
    package = MetaPackage(uid=COMPONENT, name="...", recommended=[...])
    package.write(launcher_path(COMPONENT, "package.json"))
```

### `generate_mojang.py` — Most Complex

Handles:
- LWJGL extraction into `org.lwjgl` and `org.lwjgl3` components (variant hashing)
- Log4j patching (CVE-2021-44228) via `+agents` injection
- Split natives workaround for pre-1.19 versions
- Library patching from static JSON overrides
- Legacy argument processing
- Compatible Java version detection

### `generate_forge.py` — Three Eras

| Era | MC Versions | Method | Key Class |
|---|---|---|---|
| Legacy | 1.1 – 1.5.2 | Jar mods + FML libs | `version_from_legacy()` |
| Profile | 1.6 – 1.12.2 | Installer JSON | `version_from_profile()` / `version_from_modernized_installer()` |
| Build System | 1.13+ | ForgeWrapper shim | `version_from_build_system_installer()` |

### `generate_java.py` — Multi-Vendor

Processes four vendors sequentially. Each is written as a separate component:
1. Adoptium → `net.adoptium.java`
2. OpenJ9 → `com.ibm.java`
3. Azul → `com.azul.java`
4. Mojang → `net.minecraft.java` (augmented with third-party runtimes)

---

## Index Generation: `index.py`

```python
def main():
    for package_dir in sorted(os.listdir(LAUNCHER_DIR)):
        package_path = os.path.join(LAUNCHER_DIR, package_dir, "package.json")
        if not os.path.isfile(package_path):
            continue

        # Read package metadata
        package = MetaPackage.parse_file(package_path)

        # Build version index with SHA-256 hashes
        version_entries = []
        for version_file in version_files:
            sha256 = file_hash(version_file, hashlib.sha256)
            meta_version = MetaVersion.parse_file(version_file)
            entry = MetaVersionIndexEntry.from_meta_version(meta_version, sha256)
            version_entries.append(entry)

        # Sort by release_time descending
        version_entries.sort(key=lambda e: e.release_time, reverse=True)

        # Write per-package index
        version_index = MetaVersionIndex(uid=package.uid, versions=version_entries)
        version_index.write(os.path.join(LAUNCHER_DIR, package_dir, "index.json"))

    # Write master index
    master_index = MetaPackageIndex(packages=package_entries)
    master_index.write(os.path.join(LAUNCHER_DIR, "index.json"))
```

---

## HTTP Caching

All HTTP requests use `CacheControl` with disk-backed `FileCache`:

```python
def default_session():
    cache = FileCache(os.path.join(cache_path(), "http_cache"))
    sess = CacheControl(requests.Session(), cache)
    sess.headers.update({"User-Agent": "ProjectTickMeta/1.0"})
    return sess
```

This respects HTTP cache headers (`ETag`, `Last-Modified`, `Cache-Control`), reducing bandwidth on subsequent runs.

---

## Directory Layout

```
meta/
├── upstream/           # Git repo — raw vendor data (Phase 1 output)
│   ├── mojang/
│   ├── forge/
│   ├── neoforge/
│   ├── fabric/
│   ├── quilt/
│   ├── java_runtime/
│   └── ...
├── metalauncher/       # Git repo — launcher-ready JSON (Phase 2 output)
│   ├── index.json      # Master package index
│   ├── net.minecraft/
│   ├── org.lwjgl/
│   ├── org.lwjgl3/
│   ├── net.minecraftforge/
│   ├── net.neoforged/
│   ├── net.fabricmc.fabric-loader/
│   ├── net.fabricmc.intermediary/
│   ├── org.quiltmc.quilt-loader/
│   ├── net.minecraft.java/
│   ├── net.adoptium.java/
│   ├── com.ibm.java/
│   ├── com.azul.java/
│   └── ...
└── caches/             # HTTP cache directory
    └── http_cache/
```

---

## Pipeline Flow Diagram

```
┌────────────────────────────────────────────────────┐
│                    update.sh                        │
│                                                    │
│  ┌──────────────────────────────────────────────┐  │
│  │           Phase 1: UPDATE                     │  │
│  │  update_mojang → update_forge → update_neo →  │  │
│  │  update_fabric → update_quilt → update_java   │  │
│  │          ↓ (writes to upstream/)               │  │
│  └──────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────┐  │
│  │  Git commit + push upstream/ (if changed)     │  │
│  └──────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────┐  │
│  │           Phase 2: GENERATE                   │  │
│  │  gen_mojang → gen_forge → gen_neoforge →      │  │
│  │  gen_fabric → gen_quilt → gen_java → index    │  │
│  │          ↓ (writes to launcher/)              │  │
│  └──────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────┐  │
│  │  Git commit + push launcher/ (if changed)     │  │
│  │  — OR — rsync to DEPLOY_FOLDER               │  │
│  └──────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────┘
```
