# Meta — Java Runtime Metadata

## Overview

Meta aggregates Java runtime information from **four vendors** and produces unified metadata for the launcher. This allows the launcher to automatically download and manage Java installations across platforms and architectures.

### Vendors and Components

| Vendor | Component UID | API Source | JVM Implementation |
|---|---|---|---|
| Mojang | `net.minecraft.java` | `launchermeta.mojang.com` | HotSpot |
| Adoptium (Eclipse Temurin) | `net.adoptium.java` | `api.adoptium.net` | HotSpot |
| OpenJ9 (IBM Semeru) | `com.ibm.java` | `api.adoptopenjdk.net` | OpenJ9 |
| Azul (Zulu) | `com.azul.java` | `api.azul.com` | HotSpot |

Each vendor gets its own component UID and separate output files. Mojang's component (`net.minecraft.java`) is special: it is the primary component used by the launcher and is augmented with data from Adoptium and Azul for platforms that Mojang itself doesn't cover.

---

## Phase 1: Update — `update_java.py`

### Adoptium (Eclipse Temurin)

Fetches from the Adoptium v3 API:

```python
ADOPTIUM_API_BASE = "https://api.adoptium.net"
ADOPTX_API_AVAILABLE_RELEASES = f"{{base_url}}/v3/info/available_releases"
ADOPTX_API_FEATURE_RELEASES = (
    f"{{base_url}}/v3/assets/feature_releases/{{feature_version}}/{{release_type}}"
)
```

**Step 1**: Get available feature versions:
```python
r = sess.get(ADOPTX_API_AVAILABLE_RELEASES.format(base_url=ADOPTIUM_API_BASE))
available = AdoptxAvailableReleases(**r.json())
```

**Step 2**: For each feature version, paginate through releases:
```python
for feature in available.available_releases:
    page = 0
    while True:
        query = AdoptxAPIFeatureReleasesQuery(
            image_type=AdoptxImageType.Jre,
            page_size=page_size,
            page=page,
            jvm_impl=AdoptxJvmImpl.Hotspot,
            vendor=AdoptxVendor.Eclipse,
        )
        api_call = adoptiumAPIFeatureReleasesUrl(feature, query=query)
        r_rls = sess.get(api_call)
        # ...
        if len(r_rls.json()) < page_size:
            break
        page += 1
```

**Step 3**: Save as `AdoptxReleases` per feature version:
```python
releases = AdoptxReleases(__root__=releases_for_feature)
releases.write(feature_file)
```

**Step 4**: Write filtered available releases:
```python
filtered_available_releases(available, present_adoptium_features).write(
    available_releases_file
)
```

The `filtered_available_releases()` function removes features that had no actual releases:

```python
def filtered_available_releases(
    available: AdoptxAvailableReleases, present_features: list[int]
) -> AdoptxAvailableReleases:
    filtered_features = sorted(set(present_features))
    filtered_lts = [
        feature for feature in available.available_lts_releases
        if feature in filtered_features
    ]
    # ...
```

### Retry Logic

All vendor APIs use a 3-attempt retry pattern with linear backoff for server errors (5xx):

```python
for attempt in range(3):
    r = sess.get(api_call)
    if r.status_code >= 500:
        if attempt < 2:
            time.sleep(1 * (attempt + 1))
            continue
        else:
            r.raise_for_status()
    else:
        r.raise_for_status()
        break
```

### OpenJ9 (IBM Semeru)

Uses the same API structure as Adoptium but with a different base URL and vendor:

```python
OPENJ9_API_BASE = "https://api.adoptopenjdk.net"
```

```python
query = AdoptxAPIFeatureReleasesQuery(
    image_type=AdoptxImageType.Jre,
    jvm_impl=AdoptxJvmImpl.OpenJ9,
    vendor=AdoptxVendor.Ibm,
)
api_call = openj9APIFeatureReleasesUrl(feature, query=query)
```

The model classes `AdoptxRelease`, `AdoptxBinary`, `AdoptxVersion` are shared between Adoptium and OpenJ9 (the "adoptx" prefix covers both).

### Azul (Zulu)

Uses a completely different API structure:

```python
AZUL_API_BASE = "https://api.azul.com/metadata/v1"
AZUL_API_PACKAGES = f"{AZUL_API_BASE}/zulu/packages/"
AZUL_API_PACKAGE_DETAIL = f"{AZUL_API_BASE}/zulu/packages/{{package_uuid}}"
```

**Step 1**: Paginate through Zulu package listings:
```python
query = AzulApiPackagesQuery(
    archive_type=AzulArchiveType.Zip,
    release_status=AzulReleaseStatus.Ga,
    availability_types=[AzulAvailabilityType.CA],
    java_package_type=AzulJavaPackageType.Jre,
    javafx_bundled=False,
    latest=True,
    page=page,
    page_size=page_size,
)
```

**Step 2**: For each package, fetch detailed info (or use cached):
```python
pkg_file = os.path.join(UPSTREAM_DIR, AZUL_VERSIONS_DIR, f"{pkg.package_uuid}.json")
if os.path.exists(pkg_file):
    pkg_detail = ZuluPackageDetail.parse_file(pkg_file)
else:
    api_call = azulApiPackageDetailUrl(pkg.package_uuid)
    r_pkg = sess.get(api_call)
    pkg_detail = ZuluPackageDetail(**r_pkg.json())
    pkg_detail.write(pkg_file)
```

---

## Phase 2: Generate — `generate_java.py`

### Architecture and OS Translation

The generator normalizes vendor-specific OS and architecture names to a unified `JavaRuntimeOS` enum:

```python
MOJANG_OS_ARCHITECTURE_TRANSLATIONS = {
    64: "x64",
    32: "x86",
    "x32": "x86",
    "i386": "x86",
    "aarch64": "arm64",
    "x86_64": "x64",
    "arm": "arm32",
    "riscv64": "riscv64",
}

MOJANG_OS_TRANSLATIONS = {
    "osx": "mac-os",
    "mac": "mac-os",
    "macos": "mac-os",
}
```

`JavaRuntimeOS` combines OS and architecture into a single enum value like `linux-x64`, `windows-arm64`, `mac-os-arm64`.

### Vendor-Specific Converters

Each vendor has a dedicated conversion function that maps vendor data to the unified `JavaRuntimeMeta` model:

#### Mojang Converter

```python
def mojang_runtime_to_java_runtime(
    mojang_runtime: MojangJavaRuntime,
    mojang_component: MojangJavaComponent,
    runtime_os: JavaRuntimeOS,
) -> JavaRuntimeMeta:
    # Parse version strings like "8u422" or "17.0.12"
    major, _, trail = mojang_runtime.version.name.partition("u")
    # ...
    return JavaRuntimeMeta(
        name=mojang_component,
        vendor="mojang",
        url=mojang_runtime.manifest.url,
        downloadType=JavaRuntimeDownloadType.Manifest,
        packageType=JavaPackageType.Jre,
        # ...
    )
```

Key: Mojang uses `downloadType="manifest"` (a JSON manifest of individual files), while other vendors use `downloadType="archive"` (a ZIP/tar.gz download).

#### Adoptium/OpenJ9 Converter

```python
def adoptx_release_binary_to_java_runtime(
    rls: AdoptxRelease,
    binary: AdoptxBinary,
    runtime_os: JavaRuntimeOS,
) -> JavaRuntimeMeta:
    # ...
    if rls.vendor == "eclipse":
        rls_distribution = "temurin"
    elif rls.vendor == "ibm":
        rls_distribution = "semeru-open"

    rls_name = f"{rls.vendor}_{rls_distribution}_{binary.image_type}{version}"
    return JavaRuntimeMeta(
        vendor=rls.vendor,
        url=binary.package.link,
        downloadType=JavaRuntimeDownloadType.Archive,
        # ...
    )
```

#### Azul Converter

```python
def azul_package_to_java_runtime(
    pkg: ZuluPackageDetail, runtime_os: JavaRuntimeOS
) -> JavaRuntimeMeta:
    # ...
    rls_name = f"azul_{pkg.product}_{pkg.java_package_type}{version}"
    return JavaRuntimeMeta(
        vendor="azul",
        url=pkg.download_url,
        downloadType=JavaRuntimeDownloadType.Archive,
        # ...
    )
```

### Mojang Java Component Mapping

Mojang names Java runtime groups by Greek letters. The generator maps these to major versions:

```python
def mojang_component_to_major(mojang_component: MojangJavaComponent) -> int:
    match mojang_component:
        case MojangJavaComponent.JreLegacy: return 8
        case MojangJavaComponent.Alpha:     return 17
        case MojangJavaComponent.Beta:      return 17
        case MojangJavaComponent.Gamma:     return 17
        case MojangJavaComponent.GammaSnapshot: return 17
        case MojangJavaComponent.Exe:       return 0
        case MojangJavaComponent.Delta:     return 21
```

### Extra Mojang Java (Platform Gap Filling)

Mojang's Java manifest doesn't cover all platforms. The generator fills gaps using Adoptium and Azul data:

```python
def add_java_runtime(runtime: JavaRuntimeMeta, major: int):
    javas[major].append(runtime)

    # Track runtimes for platforms Mojang doesn't cover
    if (
        (runtime.runtime_os in [JavaRuntimeOS.MacOsArm64, JavaRuntimeOS.WindowsArm64]
            and major == 8)
        or (runtime.runtime_os in [
                JavaRuntimeOS.WindowsArm32, JavaRuntimeOS.LinuxArm32,
                JavaRuntimeOS.LinuxArm64,
            ] and major in [8, 17, 21])
        or (runtime.runtime_os in [JavaRuntimeOS.LinuxX86, JavaRuntimeOS.LinuxRiscv64]
            and major in [17, 21, 25])
    ):
        extra_mojang_javas[major].append(runtime)
```

After processing all vendors, these extras are injected into `net.minecraft.java`:

```python
for java_os in [
    JavaRuntimeOS.WindowsArm32,
    JavaRuntimeOS.LinuxArm32,
    JavaRuntimeOS.LinuxArm64,
    JavaRuntimeOS.LinuxRiscv64,
]:
    for comp in [
        MojangJavaComponent.JreLegacy,
        MojangJavaComponent.Alpha,
        MojangJavaComponent.Beta,
        MojangJavaComponent.Gamma,
        MojangJavaComponent.GammaSnapshot,
        MojangJavaComponent.Delta,
    ]:
        runtime = get_mojang_extra_java(comp, java_os)
        if runtime != None:
            add_java_runtime(runtime, mojang_component_to_major(comp))
```

The `get_mojang_extra_java()` function prefers Adoptium over Azul, selects the latest version:

```python
def get_mojang_extra_java(
    mojang_component: MojangJavaComponent, java_os: JavaRuntimeOS
) -> JavaRuntimeMeta | None:
    posible_javas = list(
        filter(lambda x: x.runtime_os == java_os, extra_mojang_javas[java_major])
    )
    prefered_vendor = list(filter(lambda x: x.vendor != "azul", posible_javas))
    if len(prefered_vendor) == 0:
        prefered_vendor = posible_javas
    prefered_vendor.sort(key=lambda x: x.version, reverse=True)
    runtime = prefered_vendor[0]
    runtime.name = mojang_component
    return runtime
```

### Writing Output

```python
def writeJavas(javas: dict[int, list[JavaRuntimeMeta]], uid: str):
    javas = dict(sorted(javas.items(), key=lambda item: item[0]))

    for major, runtimes in javas.items():
        version_file = os.path.join(LAUNCHER_DIR, uid, f"java{major}.json")
        java_version = JavaRuntimeVersion(
            name=f"Java {major}",
            uid=uid,
            version=f"java{major}",
            releaseTime=timestamps.get(major),
            runtimes=runtimes,
        )
        java_version.write(version_file)

    package = MetaPackage(uid=uid, name="Java Runtimes", recommended=[])
    package.write(os.path.join(LAUNCHER_DIR, uid, "package.json"))
```

`JavaRuntimeVersion` extends `MetaVersion` with a `runtimes: list[JavaRuntimeMeta]` field.

Timestamps are derived from the **oldest** runtime in each major version to ensure monotonically increasing release times:

```python
releaseTime = reduce(
    oldest_timestamp,
    (runtime.release_time for runtime in runtimes),
    None,
)
if prevDate is not None and releaseTime < prevDate:
    releaseTime = prevDate + datetime.timedelta(seconds=1)
```

---

## Key Data Models

### `JavaRuntimeOS` (StrEnum)

Platform identifiers combining OS and architecture:

| Value | Platform |
|---|---|
| `mac-os-x64` | macOS Intel |
| `mac-os-arm64` | macOS Apple Silicon |
| `linux-x64` | Linux x86_64 |
| `linux-x86` | Linux 32-bit |
| `linux-arm64` | Linux AArch64 |
| `linux-arm32` | Linux ARM 32-bit |
| `linux-riscv64` | Linux RISC-V 64 |
| `windows-x64` | Windows 64-bit |
| `windows-x86` | Windows 32-bit |
| `windows-arm64` | Windows ARM 64-bit |
| `windows-arm32` | Windows ARM 32-bit |

### `JavaVersionMeta`

Structured Java version (supports comparison with `@total_ordering`):

```python
class JavaVersionMeta(MetaBase):
    major: int
    minor: int
    security: int
    build: Optional[int] = None
    buildstr: Optional[str] = None
    name: Optional[str] = None
```

Example: Java `17.0.12+7` → `major=17, minor=0, security=12, build=7`.

### `JavaRuntimeMeta`

The core runtime descriptor:

```python
class JavaRuntimeMeta(MetaBase):
    name: str                       # e.g., "eclipse_temurin_jre17.0.12"
    vendor: str                     # "mojang", "eclipse", "ibm", "azul"
    url: str                        # Download URL
    release_time: datetime
    checksum: Optional[JavaChecksumMeta]
    download_type: JavaRuntimeDownloadType  # "manifest" or "archive"
    package_type: JavaPackageType           # "jre" or "jdk"
    version: JavaVersionMeta
    runtime_os: JavaRuntimeOS
```

### `JavaRuntimeVersion`

Extends `MetaVersion` with a runtime list:

```python
class JavaRuntimeVersion(MetaVersion):
    runtimes: list[JavaRuntimeMeta]
```

### `AdoptxRelease` / `AdoptxBinary`

Shared models for Adoptium and OpenJ9:

```python
class AdoptxBinary(MetaBase):
    os: str
    architecture: AdoptxArchitecture
    image_type: AdoptxImageType
    package: Optional[AdoptxPackage]
    jvm_impl: AdoptxJvmImpl
    heap_size: AdoptxHeapSize

class AdoptxRelease(MetaBase):
    release_id: str = Field(alias="id")
    timestamp: datetime
    binaries: list[AdoptxBinary]
    vendor: AdoptxVendor
    version_data: AdoptxVersion
```

### `ZuluPackageDetail`

Azul's detailed package info:

```python
class ZuluPackageDetail(MetaBase):
    package_uuid: str
    sha256_hash: Optional[str]
    download_url: str
    java_version: list[int]
    java_package_type: AzulJavaPackageType
    os: AzulOs
    arch: AzulArch
    hw_bitness: AzulHwBitness
    archive_type: AzulArchiveType
```

---

## Output Structure

```
launcher/
├── net.minecraft.java/
│   ├── package.json
│   ├── java8.json
│   ├── java17.json
│   └── java21.json
├── net.adoptium.java/
│   ├── package.json
│   ├── java8.json
│   ├── java11.json
│   ├── java17.json
│   └── java21.json
├── com.ibm.java/
│   ├── package.json
│   ├── java8.json
│   └── java11.json
└── com.azul.java/
    ├── package.json
    ├── java8.json
    ├── java11.json
    ├── java17.json
    └── java21.json
```

---

## Upstream Data Structure

```
upstream/java_runtime/
├── adoptium/
│   ├── available_releases.json
│   └── versions/
│       ├── java8.json
│       ├── java11.json
│       └── ...
├── ibm/
│   ├── available_releases.json
│   └── versions/
│       ├── java8.json
│       └── ...
└── azul/
    ├── packages.json
    └── versions/
        ├── <package-uuid>.json
        ├── java8.json
        └── ...
```

---

## Processing Pipeline Summary

```
Vendor APIs ──► update_java.py ──► upstream/java_runtime/
                                         │
                                         ▼
                               generate_java.py
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
          net.adoptium.java  com.ibm.java    com.azul.java
                    │               │               │
                    └───── extra_mojang_javas ──────┘
                                    │
                                    ▼
                          net.minecraft.java
                    (augmented with third-party
                     runtimes for ARM & RISC-V)
```

The Mojang component is always processed **last** so it can pull in third-party runtimes for platforms Mojang doesn't natively support.
