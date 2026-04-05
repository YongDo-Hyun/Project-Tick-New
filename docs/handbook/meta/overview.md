# Meta — Overview

## What is Meta?

Meta is a Python-based metadata generation pipeline that produces the JSON files consumed by the ProjT Launcher (a fork of Prism Launcher). It fetches, processes, and transforms version information from multiple upstream sources — Mojang, Forge, NeoForge, Fabric, Quilt, LiteLoader, Adoptium, Azul, and others — into a unified, normalized format that the launcher can understand.

The launcher does **not** talk to Mojang or mod-loader APIs directly at runtime. Instead, it reads pre-generated metadata hosted as static JSON files in a Git repository. Meta is the tool that keeps those files up to date.

---

## Why Does Meta Exist?

Minecraft's ecosystem has a fragmented metadata landscape:

| Source | API / Format | What it provides |
|---|---|---|
| Mojang | `piston-meta.mojang.com` | Vanilla version manifests, libraries, assets, Java runtimes |
| Forge | `files.minecraftforge.net` | Installer JARs, promotions, maven metadata |
| NeoForge | `maven.neoforged.net` | Installer JARs, version lists |
| Fabric | `meta.fabricmc.net` | Loader versions, intermediary mappings |
| Quilt | `meta.quiltmc.org` | Loader versions, hashed mappings |
| LiteLoader | `dl.liteloader.com` | Artefact metadata |
| Adoptium | `api.adoptium.net` | Eclipse Temurin JRE/JDK binaries |
| IBM Semeru | `api.adoptopenjdk.net` | OpenJ9-based JRE/JDK binaries |
| Azul | `api.azul.com` | Zulu JRE/JDK packages |

Every source uses a different schema, different versioning conventions, and different distribution mechanisms. Meta normalizes all of these into a single `MetaVersion` / `MetaPackage` JSON schema that the launcher consumes through a flat-file index.

---

## High-Level Pipeline

The pipeline has two major phases executed in sequence by `update.sh`:

### Phase 1 — Update (Fetch + Store Upstream)

Each upstream source has a dedicated `update_*` script in `meta/run/`. These scripts:

1. Fetch the latest metadata from the upstream API.
2. Download installer JARs when needed (Forge, NeoForge) and extract install profiles.
3. Write raw upstream data into the `upstream/` Git repository.

Scripts executed in Phase 1:

```
python -m meta.run.update_mojang
python -m meta.run.update_forge
python -m meta.run.update_neoforge
python -m meta.run.update_fabric
python -m meta.run.update_quilt
python -m meta.run.update_liteloader
python -m meta.run.update_java
python -m meta.run.update_risugami
python -m meta.run.update_stationloader
python -m meta.run.update_optifine
python -m meta.run.update_modloadermp
```

After all updaters finish, the upstream repo is committed and pushed (if `DEPLOY_TO_GIT=true`).

### Phase 2 — Generate (Transform + Publish Launcher Metadata)

Each `generate_*` script reads from the `upstream/` directory and writes normalized `MetaVersion` JSON into the `launcher/` (aka `metalauncher/`) directory:

```
python -m meta.run.generate_mojang
python -m meta.run.generate_forge
python -m meta.run.generate_neoforge
python -m meta.run.generate_fabric
python -m meta.run.generate_quilt
python -m meta.run.generate_liteloader
python -m meta.run.generate_java
python -m meta.run.generate_risugami
python -m meta.run.generate_stationloader
python -m meta.run.generate_optifine
python -m meta.run.generate_modloadermp
python -m meta.run.index
```

The final `index` step walks all generated component directories and produces a master `index.json` listing every package and version with SHA-256 checksums.

---

## Project Identity

| Field | Value |
|---|---|
| **Package name** | `meta` |
| **Version** | `0.0.5-1` |
| **License** | MS-PL |
| **Python** | `>=3.10, <4.0` |
| **Build system** | Poetry (`poetry-core`) |
| **Repository** | `https://github.com/Project-Tick/meta` |
| **User-Agent** | `ProjectTickMeta/1.0` |

### Key Dependencies

| Package | Version | Purpose |
|---|---|---|
| `pydantic` | `^1.10.13` | Data model validation and serialization |
| `requests` | `^2.31.0` | HTTP client for upstream APIs |
| `cachecontrol` | `^0.14.0` | HTTP response caching (disk-backed via `FileCache`) |
| `filelock` | `^3.20.3` | File locking for concurrent operations |
| `packaging` | `^25.0` | PEP 440 version parsing (used in Forge generation) |

---

## Entrypoints

`pyproject.toml` registers CLI entrypoints via `[tool.poetry.scripts]`:

```toml
[tool.poetry.scripts]
generateFabric = "meta.run.generate_fabric:main"
generateForge = "meta.run.generate_forge:main"
generateLiteloader = "meta.run.generate_liteloader:main"
generateMojang = "meta.run.generate_mojang:main"
generateNeoForge = "meta.run.generate_neoforge:main"
generateQuilt = "meta.run.generate_quilt:main"
generateJava = "meta.run.generate_java:main"
updateFabric = "meta.run.update_fabric:main"
updateForge = "meta.run.update_forge:main"
updateLiteloader = "meta.run.update_liteloader:main"
updateMojang = "meta.run.update_mojang:main"
updateNeoForge = "meta.run.update_neoforge:main"
updateQuilt = "meta.run.update_quilt:main"
updateJava = "meta.run.update_java:main"
index = "meta.run.index:main"
```

Each entrypoint invokes the `main()` function of its respective module. They can also be executed as Python modules (`python -m meta.run.update_mojang`), which is the approach `update.sh` uses.

---

## Directory Layout

```
meta/                          # Project root
├── pyproject.toml             # Poetry project definition
├── requirements.txt           # Pinned pip dependencies
├── flake.nix                  # Nix flake for reproducible builds
├── garnix.yaml                # CI build config (Garnix)
├── renovate.json              # Dependency update bot config
├── config.example.sh          # Example environment config
├── config.sh                  # Active environment config (git-ignored)
├── init.sh                    # Clone upstream/launcher repos
├── update.sh                  # Main pipeline orchestrator
│
├── meta/                      # Python package
│   ├── __init__.py
│   ├── common/                # Shared utilities & constants
│   │   ├── __init__.py        # Core helpers, session factory, path utils
│   │   ├── http.py            # download_binary_file()
│   │   ├── mojang.py          # Mojang path constants
│   │   ├── forge.py           # Forge path constants, ForgeWrapper lib
│   │   ├── neoforge.py        # NeoForge path constants
│   │   ├── fabric.py          # Fabric path constants
│   │   ├── quilt.py           # Quilt path constants, beacon disabling
│   │   ├── java.py            # Java runtime path constants
│   │   ├── liteloader.py      # LiteLoader constants
│   │   ├── risugami.py        # Risugami ModLoader constants
│   │   ├── stationloader.py   # Station Loader constants
│   │   ├── optifine.py        # OptiFine constants
│   │   ├── modloadermp.py     # ModLoaderMP constants
│   │   └── mojang-*.json      # Static override/patch data files
│   │
│   ├── model/                 # Pydantic data models
│   │   ├── __init__.py        # GradleSpecifier, MetaBase, MetaVersion, Library, etc.
│   │   ├── enum.py            # StrEnum backport
│   │   ├── mojang.py          # MojangVersion, MojangIndex, LibraryPatches, etc.
│   │   ├── forge.py           # ForgeEntry, ForgeVersion, ForgeInstallerProfile, etc.
│   │   ├── neoforge.py        # NeoForgeEntry, NeoForgeVersion, etc.
│   │   ├── fabric.py          # FabricInstallerDataV1, FabricJarInfo
│   │   ├── java.py            # JavaRuntimeMeta, AdoptxRelease, ZuluPackage, etc.
│   │   ├── liteloader.py      # LiteloaderIndex, LiteloaderEntry
│   │   └── index.py           # MetaVersionIndex, MetaPackageIndex
│   │
│   └── run/                   # Executable pipeline scripts
│       ├── __init__.py
│       ├── update_mojang.py   # Fetch Mojang version manifests
│       ├── update_forge.py    # Fetch Forge installer JARs
│       ├── update_neoforge.py # Fetch NeoForge installer JARs
│       ├── update_fabric.py   # Fetch Fabric loader & intermediary
│       ├── update_quilt.py    # Fetch Quilt loader
│       ├── update_java.py     # Fetch Adoptium/OpenJ9/Azul releases
│       ├── update_liteloader.py
│       ├── update_risugami.py
│       ├── update_stationloader.py
│       ├── update_optifine.py
│       ├── update_modloadermp.py
│       ├── generate_mojang.py  # Transform Mojang → MetaVersion + LWJGL
│       ├── generate_forge.py   # Transform Forge → MetaVersion
│       ├── generate_neoforge.py# Transform NeoForge → MetaVersion
│       ├── generate_fabric.py  # Transform Fabric → MetaVersion
│       ├── generate_quilt.py   # Transform Quilt → MetaVersion
│       ├── generate_java.py    # Transform Java runtimes → MetaVersion
│       ├── generate_liteloader.py
│       ├── generate_risugami.py
│       ├── generate_stationloader.py
│       ├── generate_optifine.py
│       ├── generate_modloadermp.py
│       └── index.py            # Build master index.json
│
├── upstream/                  # Git repo: raw upstream data (submodule)
│   ├── mojang/
│   ├── forge/
│   ├── neoforge/
│   ├── fabric/
│   ├── quilt/
│   ├── liteloader/
│   ├── java_runtime/
│   └── ...
│
├── launcher/                  # Git repo: generated launcher metadata
│   ├── index.json
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
│   ├── com.azul.java/
│   ├── com.ibm.java/
│   └── ...
│
├── cache/                     # HTTP response cache (CacheControl)
│   └── http_cache/
│
├── caches/                    # Additional caches
│   ├── forge_cache/
│   └── http_cache/
│
└── public/                    # Static deploy target (optional)
```

---

## Component UIDs

Every "component" the launcher manages has a unique identifier. The following UIDs are produced by Meta:

| UID | Component | Generator |
|---|---|---|
| `net.minecraft` | Minecraft vanilla | `generate_mojang` |
| `org.lwjgl` | LWJGL 2 | `generate_mojang` |
| `org.lwjgl3` | LWJGL 3 | `generate_mojang` |
| `net.minecraftforge` | Forge | `generate_forge` |
| `net.neoforged` | NeoForge | `generate_neoforge` |
| `net.fabricmc.fabric-loader` | Fabric Loader | `generate_fabric` |
| `net.fabricmc.intermediary` | Intermediary Mappings | `generate_fabric` |
| `org.quiltmc.quilt-loader` | Quilt Loader | `generate_quilt` |
| `org.quiltmc.hashed` | Quilt Hashed Mappings | `generate_quilt` (if enabled) |
| `com.mumfrey.liteloader` | LiteLoader | `generate_liteloader` |
| `net.minecraft.java` | Mojang Java Runtimes | `generate_java` |
| `net.adoptium.java` | Eclipse Temurin JREs | `generate_java` |
| `com.ibm.java` | IBM Semeru Open JREs | `generate_java` |
| `com.azul.java` | Azul Zulu JREs | `generate_java` |
| `net.optifine` | OptiFine | `generate_optifine` |
| `risugami` | Risugami ModLoader | `generate_risugami` |
| `station-loader` | Station Loader | `generate_stationloader` |

---

## Environment Variables

The pipeline is configured through shell environment variables, typically set in `config.sh`:

| Variable | Default | Description |
|---|---|---|
| `META_CACHE_DIR` | `$CACHE_DIRECTORY` or `./caches` | HTTP and Forge cache directory |
| `META_UPSTREAM_DIR` | `$STATE_DIRECTORY/upstream` or `./upstream` | Path to upstream data Git repo |
| `META_LAUNCHER_DIR` | `$STATE_DIRECTORY/metalauncher` or `./launcher` | Path to launcher metadata Git repo |
| `DEPLOY_TO_GIT` | `false` | Whether to commit and push changes |
| `DEPLOY_TO_FOLDER` | `false` | Whether to copy output to a folder |
| `DEPLOY_FOLDER` | `/app/public/v1` | Target folder for folder deployment |
| `GIT_AUTHOR_NAME` | — | Git commit author name |
| `GIT_AUTHOR_EMAIL` | — | Git commit author email |
| `GIT_COMMITTER_NAME` | — | Git commit committer name |
| `GIT_COMMITTER_EMAIL` | — | Git commit committer email |
| `GIT_SSH_COMMAND` | — | Custom SSH command for Git push |

---

## The Meta Format

All generated version files conform to `META_FORMAT_VERSION = 1`, defined in `meta/model/__init__.py`. The format is a superset of Mojang's own version JSON, extended with:

- **Component dependencies** (`requires`, `conflicts`) — e.g., Forge requires a specific Minecraft version.
- **Maven files** (`mavenFiles`) — additional JARs needed at install time (ForgeWrapper, installer JARs).
- **Jar mods** (`jarMods`) — legacy mod injection mechanism.
- **Traits** (`+traits`) — launcher behavior hints like `FirstThreadOnMacOS`, `legacyFML`, `legacyServices`.
- **Tweakers** (`+tweakers`) — legacy Forge/LiteLoader tweaker classes.
- **JVM args** (`+jvmArgs`) — additional JVM arguments (e.g., Quilt beacon disabling).
- **Java agents** (`+agents`) — Java agent libraries for instrumentation.
- **Java compatibility** (`compatibleJavaMajors`, `compatibleJavaName`) — which Java versions work.
- **Ordering** (`order`) — controls component load order (Minecraft = -2, LWJGL = -1, Forge = 5, Fabric = 10).

---

## HTTP Caching

All HTTP requests are routed through a `CacheControl`-wrapped `requests.Session` created by `default_session()`:

```python
def default_session():
    cache = FileCache(os.path.join(cache_path(), "http_cache"))
    sess = CacheControl(requests.Session(), cache)
    sess.headers.update({"User-Agent": "ProjectTickMeta/1.0"})
    return sess
```

This transparently caches responses to disk, respecting HTTP cache headers. The cache directory is controlled by `META_CACHE_DIR`.

---

## Concurrency

Several update and generate scripts use `concurrent.futures.ThreadPoolExecutor` (or `multiprocessing.Pool` in the Fabric updater) for parallel downloads. For example, `update_mojang.py` fetches individual version JSONs concurrently, and `update_forge.py` processes installer JARs in parallel.

---

## Static Data Files

The `meta/common/` directory contains several JSON files with manual overrides and patches:

| File | Purpose |
|---|---|
| `mojang-minecraft-experiments.json` | Experimental snapshot URLs (zip-packaged versions) |
| `mojang-minecraft-old-snapshots.json` | Pre-launcher old snapshot metadata |
| `mojang-minecraft-legacy-override.json` | Main class, applet class, and trait overrides for legacy versions |
| `mojang-minecraft-legacy-services.json` | List of versions needing the `legacyServices` trait |
| `mojang-library-patches.json` | Library replacement/addition patches (e.g., adding ARM natives) |

---

## Relationship to the Launcher

The launcher fetches `index.json` from the hosted metadata repository. This index contains SHA-256 hashes for every package's version index. The launcher then fetches individual version files as needed, verifying integrity via SHA-256.

```
index.json
├── net.minecraft/index.json
│   ├── 1.21.5.json
│   ├── 1.20.4.json
│   └── ...
├── net.minecraftforge/index.json
│   ├── 49.0.31.json
│   └── ...
├── net.adoptium.java/index.json
│   ├── java21.json
│   └── ...
└── ...
```

Each version JSON is a self-contained `MetaVersion` document that the launcher uses to construct a launch configuration: libraries to download, main class to invoke, arguments to pass, Java version to require, etc.

---

## Security Considerations

- **SHA-1 and SHA-256 verification**: Installer JARs are verified against remote SHA-1 checksums. Version index entries include SHA-256 hashes.
- **URL normalization**: The `replace_old_launchermeta_url()` function rewrites deprecated `launchermeta.mojang.com` URLs to `piston-meta.mojang.com`.
- **Log4j patching**: `generate_mojang.py` forcibly upgrades Log4j libraries to patched versions (2.0-beta9-fixed or 2.17.1) to mitigate CVE-2021-44228 (Log4Shell).
- **Validation**: All parsed data passes through Pydantic model validation with strict type checking and custom validators.

---

## Nix Integration

The project includes a `flake.nix` that provides:

- A NixOS module (`services.blockgame-meta`) for scheduled execution as a systemd service.
- Development shells with all Python dependencies.
- Reproducible builds via Garnix CI.

Supported systems: `x86_64-linux`, `aarch64-linux`.

---

## Summary

Meta is the backbone of the ProjT Launcher's version management. It bridges the gap between dozens of upstream metadata sources and the launcher's unified component model. The two-phase update/generate pipeline ensures that raw upstream data is preserved (for auditability and incremental updates) while the launcher receives clean, normalized, integrity-verified metadata.
