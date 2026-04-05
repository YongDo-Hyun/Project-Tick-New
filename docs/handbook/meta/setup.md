# Meta — Setup Guide

## Prerequisites

| Requirement | Minimum Version | Notes |
|---|---|---|
| Python | 3.10+ | Required by type hint syntax (`list[int]`, `dict[str, Any]`, match statements) |
| Poetry | 1.x+ | For dependency management (or use pip with `requirements.txt`) |
| Git | 2.x | For upstream/launcher repo management |
| Nix (optional) | 2.x with flakes | For reproducible NixOS deployment |

---

## Installation Methods

### Method 1: Poetry (Recommended for Development)

```bash
cd meta/

# Install Poetry if not present
pip install poetry

# Install all dependencies
poetry install

# Verify installation
poetry run python -c "import meta; print('OK')"
```

Poetry reads `pyproject.toml` and installs:

```toml
[tool.poetry.dependencies]
python = ">=3.10,<4.0"
cachecontrol = "^0.14.0"
requests = "^2.31.0"
filelock = "^3.20.3"
packaging = "^25.0"
pydantic = "^1.10.13"
```

### Method 2: pip with requirements.txt

```bash
cd meta/

# Create and activate a virtual environment
python3 -m venv venv
source venv/bin/activate

# Install pinned dependencies
pip install -r requirements.txt
```

The `requirements.txt` contains exact pinned versions:

```
beautifulsoup4==4.14.3
CacheControl==0.14.2
certifi==2025.11.12
charset-normalizer==3.4.4
filelock==3.20.1
idna==3.11
msgpack==1.1.2
packaging==25.0
pydantic==1.10.24
requests==2.32.4
soupsieve==2.8.1
typing_extensions==4.15.0
urllib3==2.6.3
```

### Method 3: Nix Flake (Reproducible)

```bash
cd meta/

# Enter the development shell
nix develop

# Or build the package
nix build
```

The `flake.nix` defines a complete reproducible environment with all dependencies pinned via `flake.lock`. Supported systems: `x86_64-linux` and `aarch64-linux`.

---

## Initial Repository Setup

Meta requires two Git repositories for operation:

1. **Upstream repository** — stores raw fetched data from upstream APIs
2. **Launcher repository** — stores the generated launcher metadata

### Using `init.sh`

The `init.sh` script clones both repositories:

```bash
#!/usr/bin/env bash
set -ex

if [ -f config.sh ]; then
    source config.sh
fi

export META_CACHE_DIR=${CACHE_DIRECTORY:-./caches}
export META_UPSTREAM_DIR=${META_UPSTREAM_DIR:-${STATE_DIRECTORY:-.}/upstream}
export META_LAUNCHER_DIR=${META_LAUNCHER_DIR:-${STATE_DIRECTORY:-.}/metalauncher}

function init_repo {
    if [ -d "$1" ]; then
        return 0  # Already exists, skip
    fi
    if [ -z "$2" ]; then
        echo "Can't initialize missing $1 directory. Please specify $3" >&2
        return 1
    fi
    git clone "$2" "$1"
}

init_repo "$META_UPSTREAM_DIR" "$META_UPSTREAM_URL" "META_UPSTREAM_URL"
init_repo "$META_LAUNCHER_DIR" "$META_LAUNCHER_URL" "META_LAUNCHER_URL"
```

For this to work, you need `META_UPSTREAM_URL` and `META_LAUNCHER_URL` set in your `config.sh`.

### Manual Setup (Without Remote Repos)

For local development/testing without pushing to remote:

```bash
cd meta/

# Create local upstream directory
mkdir -p upstream

# Create local launcher directory
mkdir -p launcher

# Initialize as Git repos (optional, only needed if DEPLOY_TO_GIT=true)
cd upstream && git init && cd ..
cd launcher && git init && cd ..
```

---

## Configuration

### Creating `config.sh`

Copy the example and customize:

```bash
cp config.example.sh config.sh
```

The example configuration:

```bash
export META_UPSTREAM_DIR=upstream
export META_LAUNCHER_DIR=launcher
export DEPLOY_TO_FOLDER=false
export DEPLOY_FOLDER=/app/public/v1
export DEPLOY_FOLDER_USER=http
export DEPLOY_FOLDER_GROUP=http

export DEPLOY_TO_GIT=true
export GIT_AUTHOR_NAME="Herpington Derpson"
export GIT_AUTHOR_EMAIL="herpderp@derpmail.com"
export GIT_COMMITTER_NAME="$GIT_AUTHOR_NAME"
export GIT_COMMITTER_EMAIL="$GIT_AUTHOR_EMAIL"
export GIT_SSH_COMMAND="ssh -i ${BASEDIR}/config/deploy.key"
```

### Configuration Variables Reference

| Variable | Required | Default | Description |
|---|---|---|---|
| `META_UPSTREAM_DIR` | Yes | `upstream` | Path to the upstream data repository |
| `META_LAUNCHER_DIR` | Yes | `launcher` | Path to the launcher metadata repository |
| `META_CACHE_DIR` | No | `./caches` | HTTP and Forge cache directory |
| `DEPLOY_TO_GIT` | No | `false` | Commit and push changes after generation |
| `DEPLOY_TO_FOLDER` | No | `false` | Copy output to a static folder |
| `DEPLOY_FOLDER` | No | `/app/public/v1` | Target folder for deployment |
| `DEPLOY_FOLDER_USER` | No | `http` | User ownership for deployed files |
| `DEPLOY_FOLDER_GROUP` | No | `http` | Group ownership for deployed files |
| `GIT_AUTHOR_NAME` | If deploying | — | Git commit author name |
| `GIT_AUTHOR_EMAIL` | If deploying | — | Git commit author email |
| `GIT_COMMITTER_NAME` | If deploying | — | Git committer name |
| `GIT_COMMITTER_EMAIL` | If deploying | — | Git committer email |
| `GIT_SSH_COMMAND` | If using SSH | — | Custom SSH command for push authentication |

### For Local Development

Minimal `config.sh` for local testing without remote deployment:

```bash
export META_UPSTREAM_DIR=upstream
export META_LAUNCHER_DIR=launcher
export DEPLOY_TO_GIT=false
```

---

## Running the Pipeline

### Full Pipeline

```bash
cd meta/

# Source config
source config.sh

# Initialize repos (first time only)
bash init.sh

# Run the full pipeline
bash update.sh
```

### Individual Steps

#### Update (fetch upstream data)

```bash
# All updaters
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

#### Generate (transform to launcher format)

```bash
# All generators
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
```

#### Build the index

```bash
python -m meta.run.index
```

### Using Poetry Entrypoints

If installed via Poetry, use the registered script names:

```bash
poetry run updateMojang
poetry run updateForge
poetry run generateMojang
poetry run generateForge
poetry run index
```

---

## Directory Structure After First Run

After a successful pipeline execution, the workspace looks like:

```
meta/
├── upstream/                    # Populated by update_* scripts
│   ├── mojang/
│   │   ├── version_manifest_v2.json
│   │   ├── java_all.json
│   │   └── versions/
│   │       ├── 1.21.5.json
│   │       ├── 1.20.4.json
│   │       └── ...
│   ├── forge/
│   │   ├── derived_index.json
│   │   ├── legacyinfo.json
│   │   ├── maven-metadata.json
│   │   ├── promotions_slim.json
│   │   ├── jars/               # Downloaded installer JARs
│   │   ├── installer_info/     # Installer SHA/size info
│   │   ├── installer_manifests/# Extracted install_profile.json
│   │   ├── version_manifests/  # Extracted version.json
│   │   └── files_manifests/    # Maven classifier metadata
│   ├── neoforge/               # Same structure as forge
│   ├── fabric/
│   │   ├── meta-v2/
│   │   │   ├── loader.json
│   │   │   └── intermediary.json
│   │   ├── loader-installer-json/
│   │   └── jars/
│   ├── quilt/                  # Same structure as fabric
│   ├── java_runtime/
│   │   ├── adoptium/
│   │   │   ├── available_releases.json
│   │   │   └── versions/
│   │   ├── ibm/
│   │   └── azul/
│   └── ...
│
├── launcher/                   # Populated by generate_* and index
│   ├── index.json              # Master package index
│   ├── net.minecraft/
│   │   ├── package.json
│   │   ├── index.json
│   │   ├── 1.21.5.json
│   │   └── ...
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
└── caches/
    ├── forge_cache/
    └── http_cache/             # CacheControl disk cache
```

---

## Troubleshooting

### Common Issues

#### `ModuleNotFoundError: No module named 'meta'`

The `meta` package isn't installed or not on `PYTHONPATH`. Solutions:
- Install with `poetry install` or `pip install -e .`
- Run from the project root: `cd meta/ && python -m meta.run.update_mojang`

#### `FileNotFoundError` for upstream directories

The upstream/launcher directories don't exist. Run `bash init.sh` first, or create them manually.

#### `AssertionError` in Pydantic validation

An upstream API changed its schema. Check:
- Which version caused the error (look at stderr output)
- Whether the upstream format changed (compare with `upstream/` cached data)
- Whether `BAD_VERSIONS` needs updating

#### `requests.exceptions.HTTPError: 429 Too Many Requests`

API rate limiting. The HTTP cache (`CacheControl`) helps, but first runs hit APIs heavily. Wait and retry, or run individual updaters incrementally.

#### Stale SHA-1 mismatches

If an installer JAR was re-uploaded upstream with the same version but different content:
```bash
# Clear the cached JAR and its SHA file
rm upstream/forge/jars/<version>.*
rm upstream/forge/installer_info/<version>.json
rm upstream/forge/installer_manifests/<version>.json
```

Then re-run the updater.

### Logging

- Standard output: progress messages and version processing.
- Standard error (`eprint()`): warnings, skipped versions, download failures.
- The pipeline does not use Python's `logging` module — all output goes to stdout/stderr directly.

---

## NixOS Deployment

### Adding to a NixOS Configuration

```nix
{
  inputs.prism-meta.url = "github:PrismLauncher/meta";
}
```

```nix
{inputs, ...}: {
  imports = [inputs.prism-meta.nixosModules.default];
  services.blockgame-meta = {
    enable = true;
    settings = {
      DEPLOY_TO_GIT = "true";
      GIT_AUTHOR_NAME = "Bot Name";
      GIT_AUTHOR_EMAIL = "bot@example.com";
      GIT_COMMITTER_NAME = "Bot Name";
      GIT_COMMITTER_EMAIL = "bot@example.com";
    };
  };
}
```

### Managing the Service

```bash
# Trigger a manual run
systemctl start blockgame-meta.service

# Monitor logs
journalctl -fu blockgame-meta.service

# Check status
systemctl status blockgame-meta.service
```

---

## CI/CD with Garnix

The `garnix.yaml` configuration:

```yaml
builds:
  include:
    - "checks.x86_64-linux.*"
    - "devShells.*.*"
    - "packages.*.*"
```

This builds all checks, development shells, and packages on Garnix CI for every push.

---

## Development Workflow

### Making Changes

1. Edit source files in `meta/`.
2. Test individual steps: `python -m meta.run.update_mojang` or `python -m meta.run.generate_mojang`.
3. Inspect output in `launcher/` directory.
4. Verify the index: `python -m meta.run.index`.

### Testing with Local Data

To test generation without fetching from upstream (if you already have `upstream/` populated):

```bash
# Skip update phase, just regenerate
python -m meta.run.generate_mojang
python -m meta.run.generate_forge
# ... etc.
python -m meta.run.index
```

### Adding a New Mod Loader

To add support for a new mod loader:

1. Create `meta/common/<loader>.py` with path constants and component UID.
2. Create model classes in `meta/model/<loader>.py` inheriting from `MetaBase`.
3. Create `meta/run/update_<loader>.py` to fetch upstream data.
4. Create `meta/run/generate_<loader>.py` to produce `MetaVersion` files.
5. Add entries to `update.sh` for both update and generate phases.
6. Add entrypoints to `pyproject.toml` under `[tool.poetry.scripts]`.
7. Add Git add patterns to `update.sh` for the new component directories.
