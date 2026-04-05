# Meta — Deployment

## Overview

Meta supports three deployment strategies, configured via environment variables:

1. **Git deployment** (`DEPLOY_TO_GIT=true`) — commit and push both `upstream/` and `launcher/` repositories
2. **Folder deployment** (`DEPLOY_TO_FOLDER=true`) — rsync launcher output to a local directory
3. **NixOS service** — systemd timer-based deployment via a Nix flake module

---

## Git Deployment

### Configuration

Set in `config.sh`:

```bash
export DEPLOY_TO_GIT=true
export GIT_AUTHOR_NAME="Herpington Derpson"
export GIT_AUTHOR_EMAIL="herpderp@derpmail.com"
export GIT_COMMITTER_NAME="$GIT_AUTHOR_NAME"
export GIT_COMMITTER_EMAIL="$GIT_AUTHOR_EMAIL"
export GIT_SSH_COMMAND="ssh -i ${BASEDIR}/config/deploy.key"
```

### Flow

After update scripts complete, `update.sh` stages and commits to the upstream repo:

```bash
upstream_git add mojang/version_manifest_v2.json mojang/java_all.json mojang/versions/*
upstream_git add forge/*.json forge/version_manifests/*.json ...
# ... more components ...

if ! upstream_git diff --cached --exit-code; then
    upstream_git commit -a -m "Update Date ${currentDate} Time ${currentHour}:${currentMinute}:${currentSecond}"
    upstream_git push
fi
```

After generate scripts complete, the same pattern applies to the launcher repo:

```bash
launcher_git add index.json org.lwjgl/* org.lwjgl3/* net.minecraft/*
launcher_git add net.minecraftforge/*
# ... more components ...

if ! launcher_git diff --cached --exit-code; then
    launcher_git commit -a -m "Update Date ${currentDate} Time ${currentHour}:${currentMinute}:${currentSecond}"
    launcher_git push
fi
```

The `diff --cached --exit-code` check ensures no empty commits — if nothing changed, nothing is pushed.

### Repository Initialization

Run `init.sh` before the first update:

```bash
export META_UPSTREAM_URL="git@example.com:org/meta-upstream.git"
export META_LAUNCHER_URL="git@example.com:org/meta-launcher.git"
./init.sh
```

This clones both repositories:

```bash
function init_repo {
    if [ -d "$1" ]; then
        return 0
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

---

## Folder Deployment

For serving the launcher metadata via a static file server:

```bash
export DEPLOY_TO_FOLDER=true
export DEPLOY_FOLDER=/app/public/v1
```

In `update.sh`:

```bash
if [ "${DEPLOY_TO_FOLDER}" = true ]; then
    echo "Deploying to ${DEPLOY_FOLDER}"
    mkdir -p "${DEPLOY_FOLDER}"
    rsync -av --exclude=.git "${META_LAUNCHER_DIR}/" "${DEPLOY_FOLDER}"
fi
```

This rsyncs the entire launcher directory (excluding `.git`) to the target folder. Can be combined with Git deployment.

---

## NixOS Deployment

### Nix Package

The Nix package (`blockgame-meta`) is built with `buildPythonApplication`:

```nix
buildPythonApplication {
  pname = "blockgame-meta";
  version = "unstable";
  pyproject = true;

  propagatedBuildInputs = [
    cachecontrol requests filelock packaging pydantic_1
  ];

  postInstall = ''
    install -Dm755 $src/update.sh $out/bin/update
    install -Dm755 $src/init.sh $out/bin/init

    wrapProgram $out/bin/update \
      --prefix PYTHONPATH : "$PYTHONPATH" \
      --prefix PATH : ${lib.makeBinPath [git openssh python rsync]}

    wrapProgram $out/bin/init \
      --prefix PATH : ${lib.makeBinPath [git openssh]}
  '';

  mainProgram = "update";
}
```

The package:
- Includes `update.sh` as the main executable (`update`)
- Includes `init.sh` as `init`
- Wraps both with correct `PYTHONPATH` and `PATH` (git, openssh, python, rsync)

### NixOS Module

Enable the service with:

```nix
{
  services.blockgame-meta = {
    enable = true;
    package = inputs.meta.packages.${system}.blockgame-meta;
    settings = {
      DEPLOY_TO_GIT = "true";
      # ... other config vars
    };
  };
}
```

The module creates:

**System user and group**:
```nix
users.users."blockgame-meta" = {
  isSystemUser = true;
  group = "blockgame-meta";
};
```

**systemd service**:
```nix
systemd.services."blockgame-meta" = {
  description = "blockgame metadata generator";
  after = ["network-online.target"];
  wants = ["network-online.target"];
  serviceConfig = {
    EnvironmentFile = [(settingsFormat.generate "blockgame-meta.env" cfg.settings)];
    ExecStartPre = getExe' cfg.package "init";
    ExecStart = getExe cfg.package;
    StateDirectory = "blockgame-meta";
    CacheDirectory = "blockgame-meta";
    User = "blockgame-meta";
  };
};
```

- `ExecStartPre` runs `init.sh` to clone repos if needed
- `ExecStart` runs `update.sh` (the main pipeline)
- `StateDirectory` maps to `$STATE_DIRECTORY` → `/var/lib/blockgame-meta/` (where upstream/ and launcher/ repos live)
- `CacheDirectory` maps to `$CACHE_DIRECTORY` → `/var/cache/blockgame-meta/` (HTTP cache)

**systemd timer**:
```nix
systemd.timers."blockgame-meta" = {
  timerConfig = {
    OnCalendar = "hourly";
    RandomizedDelaySec = "5m";
  };
  wantedBy = ["timers.target"];
};
```

The pipeline runs **hourly** with up to 5 minutes of randomized delay to avoid thundering herd effects.

### Service Options

| Option | Type | Default | Description |
|---|---|---|---|
| `enable` | bool | `false` | Enable the blockgame-meta service |
| `package` | package | `pkgs.blockgame-meta` | Package to use |
| `settings.DEPLOY_TO_GIT` | string | `"false"` | Enable Git deployment |
| `settings.DEPLOY_TO_FOLDER` | string | `"false"` | Enable folder deployment |
| `settings.DEPLOY_TO_S3` | string | `"false"` | Enable S3 deployment |

Additional settings can be added as freeform key-value pairs.

---

## CI: Garnix

CI is configured in `garnix.yaml`:

```yaml
builds:
  include:
    - "checks.x86_64-linux.*"
    - "devShells.*.*"
    - "packages.*.*"
```

This builds and checks all packages and dev shells on every commit via [Garnix](https://garnix.io).

---

## Manual Execution

For development or one-off updates:

```bash
# 1. Set up environment
cp config.example.sh config.sh
# Edit config.sh with your settings

# 2. Initialize repositories
./init.sh

# 3. Run the pipeline
poetry run ./update.sh
```

Or run individual steps:

```bash
# Update only Mojang
poetry run python -m meta.run.update_mojang

# Generate only Forge
poetry run python -m meta.run.generate_forge

# Rebuild indices
poetry run python -m meta.run.index
```

---

## Environment Variables Reference

| Variable | Default | Description |
|---|---|---|
| `META_CACHE_DIR` | `$CACHE_DIRECTORY` or `./caches` | HTTP cache directory |
| `META_UPSTREAM_DIR` | `$STATE_DIRECTORY/upstream` or `./upstream` | Upstream git repo path |
| `META_LAUNCHER_DIR` | `$STATE_DIRECTORY/metalauncher` or `./metalauncher` | Launcher git repo path |
| `META_UPSTREAM_URL` | (none) | Git clone URL for upstream repo |
| `META_LAUNCHER_URL` | (none) | Git clone URL for launcher repo |
| `DEPLOY_TO_GIT` | `false` | Commit/push git repos after update |
| `DEPLOY_TO_FOLDER` | `false` | rsync launcher to `DEPLOY_FOLDER` |
| `DEPLOY_FOLDER` | `/app/public/v1` | Target directory for folder deployment |
| `GIT_AUTHOR_NAME` | (none) | Git author name for commits |
| `GIT_AUTHOR_EMAIL` | (none) | Git author email for commits |
| `GIT_SSH_COMMAND` | (none) | SSH command for git push (deploy key) |
