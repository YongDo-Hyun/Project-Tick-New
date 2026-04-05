# ProjT Modpack

## Overview

ProjT Modpack was a curated Minecraft modpack created and distributed by Project Tick.
The project served as the official modpack offering alongside the ProjT Launcher, providing
a pre-configured set of mods for the Project Tick community.

**Status**: Archived — no longer maintained or distributed.

---

## Project Identity

| Property          | Value                                               |
|-------------------|-----------------------------------------------------|
| **Name**          | ProjT Modpack                                       |
| **Location**      | `archived/projt-modpack/`                           |
| **Type**          | Minecraft Modpack                                   |
| **License**       | GPL-3.0-or-later                                    |
| **Copyright**     | 2025–2026 Project Tick                              |

---

## Repository Contents

The modpack repository contained modpack configuration files and promotional assets:

```
archived/projt-modpack/
├── .DS_Store                    # macOS filesystem metadata (artifact)
├── .gitattributes               # Git line ending and diff configuration
├── COPYING.md                   # GPL-3.0 license summary with copyright notice
├── LICENSE                      # Full GPL-3.0 license text
├── README.md                    # Minimal project README
├── ProjT1.png                   # Promotional image 1
├── ProjT2.png                   # Promotional image 2
├── ProjT3.png                   # Promotional image 3
├── affiliate-banner-bg.webp     # Affiliate banner background
├── affiliate-banner-fg.webp     # Affiliate banner foreground
└── bisect-icon.webp             # Bisect hosting icon
```

---

## License

The modpack was licensed under GPL-3.0-or-later:

```
ProjT Modpack - Minecraft Modpack by Project Tick
Copyright (C) 2025-2026 Project Tick

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
```

Note: This is GPL-3.0-**or-later** (with the "or any later version" clause), unlike
the ProjT Launcher which was GPL-3.0-**only**.

---

## Promotional Assets

The repository included several promotional images used for marketing and distribution:

### Modpack Screenshots

| File       | Description                              |
|-----------|------------------------------------------|
| `ProjT1.png` | Promotional screenshot/image 1        |
| `ProjT2.png` | Promotional screenshot/image 2        |
| `ProjT3.png` | Promotional screenshot/image 3        |

These were likely used on mod platform listing pages (Modrinth, CurseForge) and
the Project Tick website.

### Affiliate Assets

| File                       | Format | Description                          |
|---------------------------|--------|--------------------------------------|
| `affiliate-banner-bg.webp` | WebP   | Affiliate banner background image    |
| `affiliate-banner-fg.webp` | WebP   | Affiliate banner foreground overlay  |
| `bisect-icon.webp`         | WebP   | Bisect Hosting affiliate icon        |

The affiliate assets suggest the modpack had hosting partnership integrations,
specifically with [Bisect Hosting](https://www.bisecthosting.com/), a popular
Minecraft server hosting provider.

---

## Distribution

### Primary Distribution Channels

The modpack was distributed through:

1. **ProjT Launcher** — Native integration via the launcher's modpack download dialog
2. **Mod platforms** — Listed on Modrinth and/or CurseForge for wider discoverability
3. **Project Tick website** — https://projecttick.org/ (no longer active for this modpack)

### Installation Flow

Users could install the modpack through the ProjT Launcher:

1. Open ProjT Launcher
2. Navigate to the modpack browser
3. Search for "ProjT Modpack" or browse curated packs
4. Click Install — the launcher handles mod downloading and configuration
5. Launch the game with the modpack pre-configured

---

## Relationship to ProjT Launcher

The ProjT Modpack was tightly coupled with the ProjT Launcher:

- The launcher's modpack platform integrations (Modrinth, CurseForge, ATLauncher,
  Technic, FTB) enabled direct modpack installation
- The modpack was the launcher's "showcase" offering — a reference configuration
  demonstrating what the launcher could manage
- Promotional assets were shared between the modpack and launcher marketing

When the ProjT Launcher was archived, the modpack lost its primary distribution
channel and was archived alongside it.

---

## Relationship to MiniCraft Modpack

The ProjT Modpack was a separate project from the MiniCraft Modpack
(`archived/projt-minicraft-modpack/`):

| Aspect          | ProjT Modpack                    | MiniCraft Modpack                    |
|----------------|----------------------------------|--------------------------------------|
| **License**     | GPL-3.0                         | MIT                                  |
| **Content**     | Curated mod configuration       | Pre-built modpack ZIPs               |
| **Format**      | Platform-distributed configs    | Self-contained ZIP archives          |
| **Versioning**  | Standard semver                 | Season-based (S1–S4)                 |
| **Distribution**| Mod platforms + launcher        | Direct download                      |
| **Period**      | 2025–2026                       | 2024–2026                            |

---

## Why It Was Archived

The ProjT Modpack was archived because:

1. **Distribution channel archived** — The ProjT Launcher, which was the primary
   distribution mechanism, was itself archived
2. **Community consolidation** — Project Tick's focus shifted to other projects
   (MeshMC, corebinutils, cgit, etc.)
3. **No standalone value** — The modpack configuration files without a corresponding
   launcher integration had limited utility

---

## Historical Significance

The ProjT Modpack was significant in Project Tick's history because:

- **Community engagement** — It was one of the first user-facing products, giving
  the community something to interact with directly
- **Platform integration testing** — It served as a test bed for the launcher's
  modpack download and installation workflows
- **Branding** — The promotional assets established Project Tick's visual identity
  in the Minecraft modding community
- **Ecosystem validation** — It validated the end-to-end flow from mod curation
  → platform listing → launcher installation → gameplay

---

## File Details

### .gitattributes

The repository included Git attributes for handling binary files and line endings:

```
# Binary files should not be diffed
*.png binary
*.webp binary
```

### README.md

The README was minimal:

```markdown
# ProjT Modpack
```

This suggests the modpack's detailed description was maintained on the mod platform
listing pages rather than in the repository.

---

## Ownership

Maintained by `@YongDo-Hyun` as defined in `ci/OWNERS`:

```
/archived/projt-modpack/                             @YongDo-Hyun
```

---

## Assets Inventory

### Image Assets

| Asset                      | Format | Size Category | Purpose          |
|---------------------------|--------|---------------|------------------|
| `ProjT1.png`              | PNG    | Full-size     | Promotional      |
| `ProjT2.png`              | PNG    | Full-size     | Promotional      |
| `ProjT3.png`              | PNG    | Full-size     | Promotional      |
| `affiliate-banner-bg.webp`| WebP   | Banner-size   | Affiliate        |
| `affiliate-banner-fg.webp`| WebP   | Banner-size   | Affiliate        |
| `bisect-icon.webp`        | WebP   | Icon-size     | Affiliate        |

The use of WebP for affiliate/banner assets and PNG for screenshots reflects
the different quality requirements:
- PNG for screenshots — lossless quality for game imagery
- WebP for banners — smaller file size for web distribution

---

## Mod Content

The repository does not contain the mod files themselves (`.jar` files) — these
were downloaded dynamically through the launcher's mod platform integrations.
The modpack definition (which mods, versions, and configurations to include)
was stored in the platform-specific manifest format (e.g., Modrinth's
`modrinth.index.json` or CurseForge's `manifest.json`), which is not present
in the archived copy.

This is typical for modpack distribution: the repository contains metadata and
marketing assets, while the actual mod binaries are served by the platform CDNs.
